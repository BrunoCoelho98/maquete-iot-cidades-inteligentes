/* ============================================================
   MINI SMART CITY  -  Placa 4: SEGURANCA PUBLICA
   ------------------------------------------------------------
   Detector de gas / fumaca (prevencao de incendio e vazamento)
     - Sensor MQ-2 (analogico)  -> nivel de gas/fumaca
     - 3 LEDs (verde/amarelo/vermelho) + buzzer/sirene
     - Estados: AQUECENDO -> NORMAL -> ALERTA -> PERIGO

   Comandos (cidade/seguranca/cmd):
     "SILENCIAR" | "REARMAR" | {"limiarAlerta":1500,"limiarPerigo":2500}

   RECURSOS DE ROBUSTEZ
     - secrets.h        : rede, broker e prefixo de topicos num lugar so
     - LWT              : publica "offline" sozinho se a placa cair
     - Preferences(NVS) : limiares calibrados sobrevivem ao reset
     - Heartbeat        : uptime e RSSI para diagnostico

   Core exigido: Arduino-ESP32 3.x
   Bibliotecas:  PubSubClient, ArduinoJson (v7)
   ============================================================ */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "secrets.h"

const char* NODE_ID = "esp32-seguranca";

// ---------- TOPICOS ----------
const char* TOP_ESTADO = TOPIC_BASE "cidade/seguranca/estado";
const char* TOP_CMD    = TOPIC_BASE "cidade/seguranca/cmd";
const char* TOP_STATUS = TOPIC_BASE "cidade/seguranca/status";
const char* TOP_HB     = TOPIC_BASE "cidade/seguranca/heartbeat";

// ---------- PINOS ----------
const int PIN_MQ2      = 34;   // AO do MQ-2 (ADC1) — ver divisor no README
const int LED_VERDE    = 25;
const int LED_AMARELO  = 26;
const int LED_VERMELHO = 27;
const int BUZZER       = 13;

// ---------- PARAMETROS (persistidos na NVS) ----------
int limiarAlerta = 1500;
int limiarPerigo = 2500;
const unsigned long TEMPO_AQUECIMENTO = 20000;   // MQ-2 precisa pre-aquecer

// ---------- ESTADO ----------
enum Est { AQUECENDO, NORMAL, ALERTA, PERIGO };
Est  estado     = AQUECENDO;
int  gas        = 0;
bool silenciado = false;
unsigned long fimAquecimento = 0;
unsigned long ultimoPisca = 0;  bool ledOn  = false;
unsigned long ultimoBeep  = 0;  bool beepOn = false;

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
void carregarConfig() {
  prefs.begin("seguranca", false);
  limiarAlerta = prefs.getInt("la", limiarAlerta);
  limiarPerigo = prefs.getInt("lp", limiarPerigo);
  Serial.print("Config carregada -> alerta="); Serial.print(limiarAlerta);
  Serial.print(" perigo="); Serial.println(limiarPerigo);
}
void salvarConfig() {
  prefs.putInt("la", limiarAlerta);
  prefs.putInt("lp", limiarPerigo);
}

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
  msg.trim();
  Serial.print("CMD: "); Serial.println(msg);

  if (msg == "SILENCIAR")      silenciado = true;
  else if (msg == "REARMAR")   silenciado = false;
  else {
    JsonDocument doc;
    if (!deserializeJson(doc, msg)) {
      bool mudou = false;
      if (doc["limiarAlerta"].is<int>()) { limiarAlerta = doc["limiarAlerta"]; mudou = true; }
      if (doc["limiarPerigo"].is<int>()) { limiarPerigo = doc["limiarPerigo"]; mudou = true; }
      if (mudou) salvarConfig();          // calibracao sobrevive ao reset
    }
  }
  // Confirma o comando na telemetria no proximo laco (ver node_iluminacao).
  ultimaTelemetria = 0;
}

void tentarReconectarMqtt() {
  if (client.connected()) return;
  unsigned long agora = millis();
  if (agora - ultimaTentativaMqtt < INTERVALO_RECONEXAO) return;
  ultimaTentativaMqtt = agora;

  Serial.print("Conectando MQTT... ");
  if (client.connect(NODE_ID, TOP_STATUS, 1, true, "offline")) {
    Serial.println("ok");
    client.publish(TOP_STATUS, "online", true);
    client.subscribe(TOP_CMD);
  } else {
    Serial.print("falhou rc="); Serial.println(client.state());
  }
}

const char* nomeEstado() {
  switch (estado) {
    case AQUECENDO: return "AQUECENDO";
    case NORMAL:    return "NORMAL";
    case ALERTA:    return "ALERTA";
    default:        return "PERIGO";
  }
}

/* CUIDADO com as sobrecargas do PubSubClient. Nao existe
     publish(const char*, const char*, unsigned int)
   e char* NAO converte implicitamente para uint8_t*, entao escrever
     client.publish(topico, buf, n)
   casa com  publish(const char*, const char*, boolean retained)  e o
   tamanho n vira retained = true. Ou seja: publica RETIDO por acidente,
   sem aviso do compilador.

   Aqui isso seria pior que no resto da cidade: um PERIGO retido faria o
   painel acusar vazamento de gas no instante em que abre, com o ar ja
   normal. A telemetria e deliberadamente NAO retida - vivacidade vem do
   heartbeat e do LWT. O "online" do TOP_STATUS acima, sim, e retido de
   proposito - faz par com o will message. */
bool publicar(const char* topico, const char* buf, size_t n) {
  return client.publish(topico, (const uint8_t*)buf, (unsigned int)n, false);
}

void publicarTelemetria() {
  JsonDocument doc;
  doc["gas"]        = gas;
  doc["estado"]     = nomeEstado();
  doc["silenciado"] = silenciado;
  char buf[128];
  size_t n = serializeJson(doc, buf);
  publicar(TOP_ESTADO, buf, n);
}

void publicarHeartbeat() {
  JsonDocument doc;
  doc["uptime"] = millis() / 1000;
  doc["rssi"]   = WiFi.RSSI();
  char buf[80];
  size_t n = serializeJson(doc, buf);
  publicar(TOP_HB, buf, n);
}

// LEDs + buzzer conforme o estado (nao bloqueante)
void atualizarAtuadores(unsigned long agora) {
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERMELHO, LOW);

  switch (estado) {
    case AQUECENDO:
      if (agora - ultimoPisca > 500) { ledOn = !ledOn; ultimoPisca = agora; }
      digitalWrite(LED_AMARELO, ledOn);
      noTone(BUZZER);
      break;
    case NORMAL:
      digitalWrite(LED_VERDE, HIGH);
      noTone(BUZZER);
      break;
    case ALERTA:
      digitalWrite(LED_AMARELO, HIGH);
      if (silenciado) { noTone(BUZZER); break; }
      if (agora - ultimoBeep > 700) {
        beepOn = !beepOn; ultimoBeep = agora;
        if (beepOn) tone(BUZZER, 800); else noTone(BUZZER);
      }
      break;
    case PERIGO:
      if (agora - ultimoPisca > 200) { ledOn = !ledOn; ultimoPisca = agora; }
      digitalWrite(LED_VERMELHO, ledOn);
      if (silenciado) { noTone(BUZZER); break; }
      if (agora - ultimoBeep > 150) {
        beepOn = !beepOn; ultimoBeep = agora;
        if (beepOn) tone(BUZZER, 1200); else noTone(BUZZER);
      }
      break;
  }
}

// ============================================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  fimAquecimento = millis() + TEMPO_AQUECIMENTO;
  carregarConfig();
  setupWifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  tentarReconectarMqtt();
  client.loop();

  unsigned long agora = millis();
  gas = analogRead(PIN_MQ2);

  if (agora < fimAquecimento) {
    estado = AQUECENDO;
  } else if (gas >= limiarPerigo) {
    estado = PERIGO;
  } else if (gas >= limiarAlerta) {
    estado = ALERTA;
  } else {
    estado = NORMAL;
    silenciado = false;              // rearma sozinho quando o ar normaliza
  }

  atualizarAtuadores(agora);

  if (agora - ultimaTelemetria >= INTERVALO_TELEMETRIA) {
    ultimaTelemetria = agora;
    if (client.connected()) publicarTelemetria();
  }
  if (agora - ultimoHeartbeat >= INTERVALO_HEARTBEAT) {
    ultimoHeartbeat = agora;
    if (client.connected()) publicarHeartbeat();
  }

  delay(50);
}
