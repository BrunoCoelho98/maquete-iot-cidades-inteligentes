/* ============================================================
   MINI SMART CITY  -  Placa 3: AMBIENTAL / ALERTAS
   ------------------------------------------------------------
   Junta dois sistemas na mesma ESP32:
     1) Alerta de enchente : sensor de nivel + 4 LEDs + buzzer + servo (comporta)
     2) Estacao de temperatura e umidade : DHT22 + 3 LEDs

   DUAS TROCAS DE SENSOR, pela indisponibilidade das pecas originais:

   HC-SR04 -> sensor de nivel de agua analogico (modulo tipo pente).
   Alem de mais barato, mede o fenomeno que a maquete demonstra em vez
   de medir a distancia ate ele. A grandeza INVERTEU: antes era altura
   livre em cm, e mais era melhor; agora e submersao em %, e mais e
   pior. Todos os limites e comparacoes viraram junto.

   TMP36 -> DHT22. Digital, 3,3 V nativo, e ainda entrega umidade -
   que a estacao ambiental publica de brinde. -40 a 80 C com +-0,5 C,
   em decimos. A leitura nao usa biblioteca: esta em dht_simples.h,
   com o porque escrito la.

   Sem LCD (tudo vai para o dashboard Node-RED).
   Botao fisico da enchente foi removido -> simulacao via MQTT:
        cidade/enchente/cmd :  "SIMULAR"  (sobe o nivel da agua)
                               "RESET"    (volta ao normal)

   Core exigido: Arduino-ESP32 3.x  (tone/noTone disponiveis)
   Bibliotecas:  PubSubClient, ArduinoJson, ESP32Servo
   ============================================================ */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include "dht_simples.h"   // leitura propria; ver o porque la dentro
#include "secrets.h"          // rede, broker e prefixo de topicos

const char* NODE_ID = "esp32-ambiental";

// ---------- TOPICOS ----------
const char* TOP_ENCHENTE       = TOPIC_BASE "cidade/enchente/estado";
const char* TOP_ENCHENTE_CMD   = TOPIC_BASE "cidade/enchente/cmd";
const char* TOP_TEMPERATURA    = TOPIC_BASE "cidade/temperatura/estado";
const char* TOP_STATUS         = TOPIC_BASE "cidade/ambiental/status";
const char* TOP_HB             = TOPIC_BASE "cidade/ambiental/heartbeat";

// ---------- PINOS: ENCHENTE ----------
// Sensor de nivel analogico: ADC1 obrigatorio, porque o ADC2 fica
// indisponivel com o WiFi ligado. GPIO 35 e so entrada, o que serve.
const int PIN_NIVEL     = 35;   // ADC1
const int PIN_LED_VERDE    = 19;
const int PIN_LED_AMARELO  = 21;
const int PIN_LED_LARANJA  = 22;
const int PIN_LED_VERMELHO = 23;
const int PIN_BUZZER    = 13;
const int PIN_SERVO     = 25;

// ---------- PINOS: TEMPERATURA ----------
const int PIN_DHT       = 4;    // DHT22: linha bidirecional, exige GPIO comum
const int PIN_T_VERDE   = 26;
const int PIN_T_AMARELO = 27;
const int PIN_T_VERMELHO = 14;

// ---------- LIMITES ENCHENTE (% de submersao do sensor) ----------
// Calibre com o sensor na agua: anote a leitura crua com ele seco e com ele
// submerso ate o topo, e ajuste NIVEL_SECO / NIVEL_CHEIO.
const int NIVEL_SECO   = 100;    // ADC cru com o sensor fora da agua
const int NIVEL_CHEIO  = 2200;   // ADC cru com o sensor submerso ate o topo
const int LIMITE_ATENCAO    = 30;
const int LIMITE_ALERTA     = 55;
const int LIMITE_EMERGENCIA = 80;

enum EstadoNivel { NORMAL = 0, ATENCAO = 1, ALERTA = 2, EMERGENCIA = 3 };
EstadoNivel estadoAtual   = NORMAL;
EstadoNivel estadoAnterior = NORMAL;
int nivelAgua = 0;               // % de submersao: 0 = seco, 100 = cheio
// Simulacao pelo painel. Antes ela se apoiava no sensor devolver leitura
// invalida, entao so funcionava com o ultrassonico DESLIGADO; o sensor
// analogico sempre devolve algo, entao agora e estado explicito.
int  nivelSimulado = 0;
bool simulando     = false;

unsigned long ultimoTempoPisca = 0;
bool ledPiscando = false;

// ---------- TEMPERATURA ----------
float temperatura = 0;
float umidade     = 0;

// ---------- INFRA ----------
Servo         comporta;
// (o DHT nao tem objeto: dht_simples.h e uma funcao so)
WiFiClient    espClient;
PubSubClient  client(espClient);

unsigned long ultimaTentativaMqtt = 0;
const unsigned long INTERVALO_RECONEXAO = 5000;
unsigned long ultimaTelemetria = 0;
const unsigned long INTERVALO_TELEMETRIA = 1500;
unsigned long ultimoHeartbeat = 0;
const unsigned long INTERVALO_HEARTBEAT = 10000;

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
  msg.trim();
  Serial.print("CMD ["); Serial.print(topic); Serial.print("]: "); Serial.println(msg);

  if (String(topic) == TOP_ENCHENTE_CMD) {
    if (msg == "SIMULAR") {              // simula agua subindo (submersao aumenta)
      simulando = true;
      nivelSimulado += 12;
      if (nivelSimulado > 95) nivelSimulado = 95;
    } else if (msg == "RESET") {
      simulando = false;
      nivelSimulado = 0;
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
  // LWT: se a placa cair, o broker publica "offline" (retido) sozinho
  if (client.connect(NODE_ID, TOP_STATUS, 1, true, "offline")) {
    Serial.println("ok");
    client.publish(TOP_STATUS, "online", true);
    client.subscribe(TOP_ENCHENTE_CMD);
  } else {
    Serial.print("falhou rc="); Serial.println(client.state());
  }
}

// ============================================================
//  ENCHENTE
// ============================================================
int medirNivel() {
  if (simulando) return nivelSimulado;
  int cru = analogRead(PIN_NIVEL);                     // 0..4095
  long pct = (long)(cru - NIVEL_SECO) * 100 / (NIVEL_CHEIO - NIVEL_SECO);
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return (int)pct;
}

void apagarTodos() {
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARELO, LOW);
  digitalWrite(PIN_LED_LARANJA, LOW);
  digitalWrite(PIN_LED_VERMELHO, LOW);
  noTone(PIN_BUZZER);          // buzzer passivo: quem cala o tom e o noTone
}

// Buzzer PASSIVO, como nas outras tres placas: o mesmo componente em todas
// elas, acionado por tone()/noTone().
void tocarBuzzer(int bips, int duracao) {
  for (int i = 0; i < bips; i++) {
    tone(PIN_BUZZER, 2000);
    delay(duracao);
    noTone(PIN_BUZZER);
    delay(100);
  }
}

void atualizarEstado() {
  if (nivelAgua >= LIMITE_EMERGENCIA)   estadoAtual = EMERGENCIA;
  else if (nivelAgua >= LIMITE_ALERTA)  estadoAtual = ALERTA;
  else if (nivelAgua >= LIMITE_ATENCAO) estadoAtual = ATENCAO;
  else                                  estadoAtual = NORMAL;
}

void executarTransicao() {
  apagarTodos();
  switch (estadoAtual) {
    case NORMAL:
      digitalWrite(PIN_LED_VERDE, HIGH);
      comporta.write(0);
      break;
    case ATENCAO:
      digitalWrite(PIN_LED_AMARELO, HIGH);
      tocarBuzzer(1, 200);
      comporta.write(45);
      break;
    case ALERTA:
      digitalWrite(PIN_LED_LARANJA, HIGH);
      tocarBuzzer(3, 150);
      comporta.write(90);
      break;
    case EMERGENCIA:
      comporta.write(180);
      break;
  }
}

void executarAcoesEstado() {
  unsigned long agora = millis();
  switch (estadoAtual) {
    case NORMAL:
      digitalWrite(PIN_LED_VERDE, HIGH);
      break;
    case ATENCAO:
      digitalWrite(PIN_LED_AMARELO, HIGH);
      break;
    case ALERTA:
      if (agora - ultimoTempoPisca > 500) {
        ledPiscando = !ledPiscando;
        digitalWrite(PIN_LED_LARANJA, ledPiscando);
        ultimoTempoPisca = agora;
      }
      break;
    case EMERGENCIA:
      if (agora - ultimoTempoPisca > 250) {
        ledPiscando = !ledPiscando;
        digitalWrite(PIN_LED_VERDE, ledPiscando);
        digitalWrite(PIN_LED_AMARELO, ledPiscando);
        digitalWrite(PIN_LED_LARANJA, ledPiscando);
        digitalWrite(PIN_LED_VERMELHO, ledPiscando);
        // Buzzer PASSIVO: nivel alto nao gera som, so um estalo na borda. Esta
        // linha era digitalWrite e sobrou da epoca do buzzer ativo - deixava o
        // estado mais grave da placa como o mais silencioso dela.
        if (ledPiscando) tone(PIN_BUZZER, 2000); else noTone(PIN_BUZZER);
        ultimoTempoPisca = agora;
      }
      break;
  }
}

const char* nomeEstado() {
  switch (estadoAtual) {
    case NORMAL:     return "NORMAL";
    case ATENCAO:    return "ATENCAO";
    case ALERTA:     return "ALERTA";
    default:         return "EMERGENCIA";
  }
}

// ============================================================
//  TEMPERATURA
// ============================================================
// O DHT22 nao aceita mais de uma leitura a cada 2 s: insistindo, so devolve erro.
// Em falha, mantem o ultimo valor bom em vez de publicar lixo no painel - e
// conta as falhas seguidas, para o serial poder dizer QUAL falha e nao so que
// o valor parou de andar. Um sensor mudo que deixa um numero plausivel na tela
// e pior do que um sensor que avisa.
unsigned long ultimaLeituraDht = 0;
int falhasDht = 0;
// Contadores ACUMULADOS, publicados na telemetria. O falhasDht acima zera a cada
// leitura boa - serve para o serial dizer "falhou 3x seguidas", nao para medir
// taxa de falha. Estes dois e que respondem "de cada 100 tentativas, quantas o
// sensor perdeu?".
unsigned long dhtTentativas = 0, dhtFalhas = 0;
void lerTemperatura() {
  unsigned long agora = millis();
  // 2,5 s e nao 2: no limite exato do datasheet o sensor fica sem margem.
  if (agora - ultimaLeituraDht < 2500) return;
  ultimaLeituraDht = agora;
  dhtTentativas++;

  LeituraDht d = lerDht22(PIN_DHT);
  if (d.ok) {
    if (falhasDht >= 3) {
      Serial.print("DHT voltou depois de "); Serial.print(falhasDht);
      Serial.println(" falhas");
    }
    falhasDht  = 0;
    temperatura = d.temperatura;
    umidade     = d.umidade;
    return;
  }
  falhasDht++;
  dhtFalhas++;
  // Uma linha a cada ~1 min: barulho no serial esconde o resto do diagnostico.
  if (falhasDht == 3 || falhasDht % 30 == 0) {
    Serial.print("DHT falhou "); Serial.print(falhasDht);
    Serial.print("x seguidas - "); Serial.println(d.erro);
  }
}

const char* statusTemp() {
  if (temperatura > 35) return "ALTA";
  if (temperatura < 15) return "BAIXA";
  return "OK";
}

void controlarLedsTemp() {
  digitalWrite(PIN_T_VERDE, LOW);
  digitalWrite(PIN_T_AMARELO, LOW);
  digitalWrite(PIN_T_VERMELHO, LOW);
  if (temperatura > 35)      digitalWrite(PIN_T_AMARELO, HIGH);
  else if (temperatura < 15) digitalWrite(PIN_T_VERMELHO, HIGH);
  else                       digitalWrite(PIN_T_VERDE, HIGH);
}

// ============================================================
//  TELEMETRIA
// ============================================================
/* CUIDADO com as sobrecargas do PubSubClient. Nao existe
     publish(const char*, const char*, unsigned int)
   e char* NAO converte implicitamente para uint8_t*, entao escrever
     client.publish(topico, buf, n)
   casa com  publish(const char*, const char*, boolean retained)  e o
   tamanho n vira retained = true. Ou seja: publica RETIDO por acidente,
   sem aviso do compilador.

   Aqui um EMERGENCIA retido faria o painel abrir acusando enchente com o
   nivel ja normal. A telemetria e deliberadamente NAO retida - vivacidade
   vem do heartbeat e do LWT. O "online" do TOP_STATUS acima, sim, e retido
   de proposito - faz par com o will message. */
bool publicar(const char* topico, const char* buf, size_t n) {
  return client.publish(topico, (const uint8_t*)buf, (unsigned int)n, false);
}

void publicarTelemetria() {
  // Enchente
  {
    JsonDocument doc;
    doc["nivel"]  = nivelAgua;
    doc["estado"] = nomeEstado();
    char buf[128];
    size_t n = serializeJson(doc, buf);
    publicar(TOP_ENCHENTE, buf, n);
  }
  // Temperatura
  {
    JsonDocument doc;
    doc["temp"]    = round(temperatura * 10) / 10.0;
    doc["umidade"] = round(umidade * 10) / 10.0;   // o DHT22 entrega decimos
    doc["status"]  = statusTemp();
    doc["tentativas"] = dhtTentativas;             // para a taxa de falha de leitura
    doc["falhas"]     = dhtFalhas;
    char buf[128];
    size_t n = serializeJson(doc, buf);
    publicar(TOP_TEMPERATURA, buf, n);
  }
}

void publicarHeartbeat() {
  JsonDocument doc;
  doc["uptime"] = millis() / 1000;
  doc["rssi"]   = WiFi.RSSI();
  char buf[80];
  size_t n = serializeJson(doc, buf);
  publicar(TOP_HB, buf, n);
}

// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_AMARELO, OUTPUT);
  pinMode(PIN_LED_LARANJA, OUTPUT);
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  pinMode(PIN_T_VERDE, OUTPUT);
  pinMode(PIN_T_AMARELO, OUTPUT);
  pinMode(PIN_T_VERMELHO, OUTPUT);

  // o DHT nao precisa de begin(): a primeira leitura ja faz o handshake
  comporta.attach(PIN_SERVO);
  comporta.write(0);

  setupWifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  tentarReconectarMqtt();
  client.loop();

  // --- ENCHENTE ---
  nivelAgua = medirNivel();
  atualizarEstado();
  if (estadoAtual != estadoAnterior) {
    executarTransicao();
    estadoAnterior = estadoAtual;
  }
  executarAcoesEstado();

  // --- TEMPERATURA ---
  lerTemperatura();
  controlarLedsTemp();

  // --- TELEMETRIA ---
  unsigned long agora = millis();
  if (agora - ultimaTelemetria >= INTERVALO_TELEMETRIA) {
    ultimaTelemetria = agora;
    if (client.connected()) publicarTelemetria();
  }
  if (agora - ultimoHeartbeat >= INTERVALO_HEARTBEAT) {
    ultimoHeartbeat = agora;
    if (client.connected()) publicarHeartbeat();
  }

  delay(100);
}
