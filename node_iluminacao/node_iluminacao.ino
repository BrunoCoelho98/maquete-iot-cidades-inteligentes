/* ============================================================
   MINI SMART CITY  -  Placa 2: ILUMINACAO PUBLICA
   ------------------------------------------------------------
   Poste Inteligente = luminosidade (LDR) + presenca (TCRT5000)
     - LDR  (ADC1)  -> decide se esta escuro
     - TCRT5000     -> decide se ha alguem passando sob o poste
     - LED  (PWM)   -> acende so com as DUAS condicoes: escuro E presenca

   POR QUE UM SENSOR REFLEXIVO NO LUGAR DO PIR
   O PIR enxerga corpo quente a metros; o TCRT5000 e optico e reflexivo, e
   enxerga MILIMETROS - o objeto tem de passar por cima dele. Na maquete isso
   muda o que a placa demonstra: em vez de "alguem se aproximando do poste",
   ela detecta "o veiculo passando naquele ponto da via". O que encolheu foi o
   alcance, nao a ideia: continua sendo iluminacao sob demanda. O sensor vai
   embutido no piso, virado para cima, ao lado do poste.

   Duas consequencias que nao sao so de nome:
     - A saida e ATIVA EM BAIXO (LOW com objeto), ao contrario do PIR. Trocar
       so o modulo, sem trocar a comparacao, inverteria o poste inteiro.
     - O TCRT5000 nao tem retencao propria: a saida volta ao repouso no
       instante em que o objeto sai. Quem segura a luz acesa agora e apenas o
       TEMPO_PRESENCA daqui - no PIR, o proprio modulo ja segurava por segundos.

   Controle pelo dashboard Node-RED (MQTT):
     modo   = AUTO | ON | OFF
     limiar = valor do LDR abaixo do qual considera "escuro"

   RECURSOS DE ROBUSTEZ
     - secrets.h        : rede, broker e prefixo de topicos num lugar so
     - LWT              : publica "offline" sozinho se a placa cair
     - Preferences(NVS) : limiar e modo sobrevivem ao reset
     - Heartbeat        : uptime e RSSI para diagnostico

   Core exigido: Arduino-ESP32 3.x
   Bibliotecas:  PubSubClient, ArduinoJson (v7)
   ============================================================ */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "secrets.h"

const char* NODE_ID = "esp32-iluminacao";

// ---------- TOPICOS ----------
const char* TOP_ESTADO = TOPIC_BASE "cidade/iluminacao/estado";
const char* TOP_CMD    = TOPIC_BASE "cidade/iluminacao/cmd";
const char* TOP_STATUS = TOPIC_BASE "cidade/iluminacao/status";
const char* TOP_HB     = TOPIC_BASE "cidade/iluminacao/heartbeat";

// ---------- PINOS ----------
const int PIN_LDR = 32;   // ADC1 (obrigatorio com WiFi ligado)
const int PIN_PRESENCA = 33;   // DO do TCRT5000; o AO fica sem ligacao
const int PIN_LED = 25;   // saida PWM

// Nivel que o modulo apresenta COM objeto na frente. O TCRT5000 vai a LOW; se o
// seu acender o LED de bordo e o pino subir, esta e a unica linha a trocar.
const int NIVEL_DETECTADO = LOW;

// ---------- PARAMETROS (persistidos na NVS) ----------
int    limiar = 1000;      // abaixo disso = escuro
String modo   = "AUTO";    // AUTO | ON | OFF
// Brilho com escuro e SEM ninguem. Zero significa apagado: o poste exige as
// duas condicoes. Vale saber o que se abre mao aqui - iluminacao publica de
// verdade nao apaga entre um pedestre e outro, ela cai para uma base (80 = ~30%)
// e sobe quando alguem passa, porque rua as escuras e problema de seguranca
// publica. Trocar este 0 por 80 devolve o poste dimerizavel sem mexer em mais
// nada: a logica abaixo ja e a mesma nos dois casos.
const int  brilhoBase  = 0;
const int  brilhoAlto  = 255;
const unsigned long TEMPO_PRESENCA = 5000;

// ---------- ESTADO ----------
int           valorLDR    = 0;
bool          presenca    = false;
int           brilhoAtual = 0;
unsigned long fimPresenca = 0;

// ---------- INFRA ----------
WiFiClient    espClient;
PubSubClient  client(espClient);
Preferences   prefs;

unsigned long ultimaTentativaMqtt = 0;
const unsigned long INTERVALO_RECONEXAO = 5000;
unsigned long ultimaTelemetria = 0;
const unsigned long INTERVALO_TELEMETRIA = 1500;
unsigned long ultimoHeartbeat = 0;
const unsigned long INTERVALO_HEARTBEAT = 10000;

// ============================================================
//  PERSISTENCIA (NVS)
// ============================================================
void carregarConfig() {
  prefs.begin("iluminacao", false);
  limiar = prefs.getInt("limiar", limiar);
  modo   = prefs.getString("modo", modo);
  Serial.print("Config carregada -> limiar="); Serial.print(limiar);
  Serial.print(" modo="); Serial.println(modo);
}
void salvarConfig() {
  prefs.putInt("limiar", limiar);
  prefs.putString("modo", modo);
}

// ============================================================
//  WiFi / MQTT
// ============================================================
void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println();
  Serial.print("IP: "); Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.print("CMD: "); Serial.println(msg);

  bool mudou = false;
  JsonDocument doc;
  if (!deserializeJson(doc, msg)) {                 // aceita JSON
    if (doc["modo"].is<const char*>())  { modo   = String((const char*)doc["modo"]); mudou = true; }
    if (doc["limiar"].is<int>())        { limiar = doc["limiar"];                    mudou = true; }
  } else {                                          // ou string simples
    msg.trim();
    if (msg == "ON" || msg == "OFF" || msg == "AUTO") { modo = msg; mudou = true; }
  }
  if (mudou) {
    salvarConfig();                                 // calibracao sobrevive ao reset
    // Confirma o comando na telemetria JA no proximo laco, em vez de esperar o
    // periodo de 1,5 s. Alem de deixar o painel responsivo, e o que torna
    // mensuravel a latencia de comando: sem isto, o que se mede e o periodo de
    // publicacao, nao o tempo de resposta.
    ultimaTelemetria = 0;
  }
}

void tentarReconectarMqtt() {
  if (client.connected()) return;
  unsigned long agora = millis();
  if (agora - ultimaTentativaMqtt < INTERVALO_RECONEXAO) return;
  ultimaTentativaMqtt = agora;

  Serial.print("Conectando MQTT... ");
  // LWT: se a placa cair, o broker publica "offline" (retido) sozinho
  if (client.connect(NODE_ID, TOP_STATUS, 1, true, "offline")) {
    Serial.println("ok");
    client.publish(TOP_STATUS, "online", true);
    client.subscribe(TOP_CMD);
  } else {
    Serial.print("falhou rc="); Serial.println(client.state());
  }
}

/* CUIDADO com as sobrecargas do PubSubClient. Nao existe
     publish(const char*, const char*, unsigned int)
   e char* NAO converte implicitamente para uint8_t*, entao escrever
     client.publish(topico, buf, n)
   casa com  publish(const char*, const char*, boolean retained)  e o
   tamanho n vira retained = true. Ou seja: publica RETIDO por acidente,
   sem aviso do compilador.

   A telemetria daqui e deliberadamente NAO retida: a vivacidade vem do
   heartbeat e do LWT, e uma leitura de horas atras nao deve reaparecer
   no painel como se fosse atual. O "online" do TOP_STATUS acima, sim, e
   retido de proposito - faz par com o will message. */
bool publicar(const char* topico, const char* buf, size_t n) {
  return client.publish(topico, (const uint8_t*)buf, (unsigned int)n, false);
}

void publicarTelemetria() {
  JsonDocument doc;
  doc["ldr"]      = valorLDR;
  doc["presenca"] = presenca;
  doc["brilho"]   = brilhoAtual;
  doc["modo"]     = modo;
  doc["limiar"]   = limiar;
  char buffer[160];
  size_t n = serializeJson(doc, buffer);
  publicar(TOP_ESTADO, buffer, n);
}

void publicarHeartbeat() {
  JsonDocument doc;
  doc["uptime"] = millis() / 1000;
  doc["rssi"]   = WiFi.RSSI();
  char buffer[80];
  size_t n = serializeJson(doc, buffer);
  publicar(TOP_HB, buffer, n);
}

// ============================================================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_PRESENCA, INPUT);   // o pull-up e do modulo, nao do ESP32
  pinMode(PIN_LED, OUTPUT);
  analogWrite(PIN_LED, 0);

  carregarConfig();
  setupWifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  tentarReconectarMqtt();
  client.loop();

  // ---- sensores ----
  valorLDR = analogRead(PIN_LDR);            // 0..4095
  presenca = digitalRead(PIN_PRESENCA) == NIVEL_DETECTADO;
  if (presenca) fimPresenca = millis() + TEMPO_PRESENCA;
  bool emPresenca = millis() < fimPresenca;

  // ---- brilho alvo ----
  int alvo;
  if (modo == "OFF")      alvo = 0;
  else if (modo == "ON")  alvo = brilhoAlto;
  else {                                     // AUTO
    // Duas condicoes, e as duas precisam valer: escuro E presenca. A presenca
    // vale por TEMPO_PRESENCA depois do ultimo disparo - sem essa retencao o
    // poste apagaria no instante em que o objeto saisse de cima do sensor.
    bool escuro = valorLDR < limiar;
    if (!escuro)          alvo = 0;
    else if (emPresenca)  alvo = brilhoAlto;
    else                  alvo = brilhoBase;
  }

  // ---- fade nao bloqueante ----
  if (brilhoAtual < alvo)      brilhoAtual = min(alvo, brilhoAtual + 5);
  else if (brilhoAtual > alvo) brilhoAtual = max(alvo, brilhoAtual - 5);
  analogWrite(PIN_LED, brilhoAtual);

  unsigned long agora = millis();
  if (agora - ultimaTelemetria >= INTERVALO_TELEMETRIA) {
    ultimaTelemetria = agora;
    if (client.connected()) publicarTelemetria();
  }
  if (agora - ultimoHeartbeat >= INTERVALO_HEARTBEAT) {
    ultimoHeartbeat = agora;
    if (client.connected()) publicarHeartbeat();
  }

  delay(15);   // passo do fade
}
