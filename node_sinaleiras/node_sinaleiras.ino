/* ============================================================
   MINI SMART CITY - PLACA 1A: SINALEIRAS DO CRUZAMENTO
   ------------------------------------------------------------
   Aciona TODOS os focos do cruzamento - 4 cabecas de veiculo e
   4 de pedestre - e e a dona exclusiva da maquina de fases. Os focos sao
   modulos de mini sinaleira, nao LEDs soltos.

   A TRAVESSIA TEM AVISO SONORO, E ISSO NAO E ENFEITE
   O buzzer apita durante a fase de pedestre e acelera no intermitente.
   E a unica indicacao da travessia para quem nao ve o foco - tirar o
   buzzer tira a acessibilidade do cruzamento, nao um detalhe sonoro.

   POR QUE ESTA PLACA NAO TEM SENSOR NENHUM
   O cruzamento nao cabia em um ESP32 (14 canais de foco + 11
   entradas = 25 pinos, contra ~20 utilizaveis) e a cadeia de
   74HC595 que resolvia isso ficou indisponivel. A saida foi
   dividir em duas placas, e ONDE cortar importa mais que o
   corte em si:

     o invariante de seguranca - nunca dois verdes conflitantes -
     tem que caber inteiro dentro de um microcontrolador.

   Dividir por via (A aqui, B na outra) devolveria o problema:
   duas placas decidindo verde em paralelo dependem da rede para
   nao se contradizerem. Dividido por funcao, nao. Todos os focos
   estao aqui, sob uma unica maquina de fases; a outra placa so
   le entradas. Entrada atrasada ou perdida degrada a EFICIENCIA
   do cruzamento - o verde encerra na hora errada, o pedestre
   espera mais - e nunca a seguranca. E, sem comando por
   T_SEM_COMANDO, esta placa assume o ciclo de tempo fixo
   sozinha, exatamente como fazia quando era uma placa so.

   AS DUAS CABECAS DE UMA VIA COMPARTILHAM UM CANAL
   Em toda fase - inclusive amarelo e piscante - a liberacao
   devolve o mesmo valor para Norte e Sul, e o mesmo para Leste e
   Oeste. As duas cabecas de uma via sempre mostraram o mesmo
   aspecto; ligar os dois LEDs no mesmo pino nao muda um unico
   comportamento, so economiza 6 pinos. Ja as 4 travessias de
   pedestre sao independentes de verdade (servindo[j] abre so a
   faixa pedida), entao cada uma tem seus dois canais.

   Par:      node_sensores  (aproximacao, vagas e botoeiras)
   Core:     Arduino-ESP32 3.x
   Libs:     PubSubClient, ArduinoJson (v7)
   ============================================================ */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "fases.h"      // enum Fase (ver o comentario de la sobre o porque)

const char* NODE_ID = "esp32-sinaleiras";

// ---------- TOPICOS ----------
const char* TOP_ESTADO = TOPIC_BASE "cidade/cruzamento/estado";
const char* TOP_CMD    = TOPIC_BASE "cidade/cruzamento/cmd";
const char* TOP_STATUS = TOPIC_BASE "cidade/cruzamento/status";
const char* TOP_HB     = TOPIC_BASE "cidade/cruzamento/heartbeat";

// ---------- PINOS ----------
// via 0 = Norte+Sul (via A) · via 1 = Leste+Oeste (via B)
// cada linha: vermelho, amarelo, verde. Dois focos em paralelo por pino - com
// os modulos de mini sinaleira, dois modulos no mesmo canal (~6 mA cada com 220
// ohms em 3,3 V, 12 mA no pino, dentro dos 20 mA; se o modulo trouxer resistor
// proprio a corrente cai e sobra folga, mas o verde pode ficar fraco: ver nota).
const int PIN_CARRO[2][3] = {
  { 23, 22, 21 },
  { 19, 18, 17 }
};
// travessias N, S, L, O - cada uma independente: { vermelho, verde }
const int PIN_PED[4][2] = {
  { 16,  4 },
  {  5, 13 },
  { 14, 27 },
  { 26, 25 }
};
const int PIN_BUZZER = 33;                // travessia sonora (acessibilidade)
// GPIO 32 fica livre. Evitados: 0/2/12/15 (strapping), 1/3 (UART0), 6-11 (flash),
// 34-39 (so entrada, e esta placa e toda saida).

// ---------- POLARIDADE DOS FOCOS ----------
// Os focos sao modulos prontos de mini sinaleira, nao LEDs soltos: cada GPIO
// entra em uma das entradas de cor, atraves do seu resistor, e um unico pino
// COMUM do modulo fecha o circuito.
//    comum no GND  = catodo comum = acende com HIGH
//    comum no 3V3  = anodo comum  = acende com LOW
// Trocar de modulo e trocar estas duas linhas: aplicarSaidas() escreve por
// foco() e nao sabe a diferenca. Como saber qual e o seu: desligue a placa,
// multimetro em teste de diodo, ponta vermelha no pino comum - se as tres cores
// acendem fraco, o comum e o ANODO; se nada acende e acende invertendo as
// pontas, o comum e o CATODO.
const int ACESO   = HIGH;    // catodo comum (pino comum no GND)
const int APAGADO = LOW;

inline void foco(int pino, bool ligado) {
  digitalWrite(pino, ligado ? ACESO : APAGADO);
}

// ---------- TEMPOS (ms) ----------
const unsigned long T_VERDE_MIN     =  4000;   // ninguem corta o verde antes disso
const unsigned long T_AMARELO       =  2000;
const unsigned long T_ENTREVERDES   =  1500;   // tudo vermelho entre fases
const unsigned long T_TRAVESSIA     =  6000;   // verde de pedestre (fixo)
const unsigned long T_TRAV_INTERM   =  3000;   // vermelho de pedestre piscando
const unsigned long T_SEM_COMANDO   = 10000;   // acima disso, assume o ciclo local
const unsigned long T_VERDE_FIXO    =  8000;   // verde do ciclo autonomo

// ---------- FASES ----------  (enum em fases.h)
Fase fase        = F_ENTREVERDES;
Fase faseSaindo  = F_VIA_A;      // de qual verde veio o amarelo
Fase faseDestino = F_VIA_A;      // para onde vamos depois do entreverdes
unsigned long faseInicio = 0;

bool  temPedidoFase = false;     // comando chegou antes do verde minimo
Fase  pedidoFase    = F_VIA_A;

// ---------- DEMANDA ----------
// Chega por comando: a botoeira fisica esta na placa de sensores, e o Node-RED
// traduz a borda dela em {"pedestre":j}. E o mesmo caminho da botoeira virtual
// do painel, que ja existia - nenhum mecanismo novo foi inventado para isso.
bool pedidoTravessia[4] = { false, false, false, false };
bool servindo[4]        = { false, false, false, false };  // travessias abertas nesta fase

bool forcarPiscante = false;
unsigned long ultimoComando = 0;
bool supervisionado = false;     // o Node-RED esta mandando?

// ---------- INFRA ----------
WiFiClient   espClient;
PubSubClient client(espClient);
unsigned long ultimaTentativaMqtt = 0;
const unsigned long INTERVALO_RECONEXAO = 5000;
unsigned long ultimoHeartbeat = 0;
const unsigned long INTERVALO_HEARTBEAT = 10000;
unsigned long ultimaPublicacao = 0;
const unsigned long INTERVALO_PUBLICACAO = 500;
unsigned long ultimoBlink = 0;
bool blinkOn = false;

// prototipos declarados a mao: nao depender do pre-processador do Arduino
// deixa este arquivo compilavel em qualquer toolchain.
void publicarEstado();
bool publicar(const char* topico, const char* buf, size_t n);
void irPara(Fase nova);

// ============================================================
//  SAIDAS
// ============================================================
// via 0 = A (Norte+Sul), via 1 = B (Leste+Oeste)
bool viaLiberada(Fase f, int via) {
  return (f == F_VIA_A && via == 0) || (f == F_VIA_B && via == 1);
}

void aplicarSaidas() {
  for (int via = 0; via < 2; via++) {
    bool verde = false, amarelo = false, vermelho = true;

    switch (fase) {
      case F_VIA_A:
      case F_VIA_B:
        verde = viaLiberada(fase, via);
        vermelho = !verde;
        break;
      case F_AMARELO:
        amarelo = viaLiberada(faseSaindo, via);
        vermelho = !amarelo;
        break;
      case F_PISCANTE:
        amarelo = blinkOn;
        vermelho = false;
        break;
      default:                       // entreverdes e fase de pedestre
        vermelho = true;
        break;
    }
    foco(PIN_CARRO[via][0], vermelho);
    foco(PIN_CARRO[via][1], amarelo);
    foco(PIN_CARRO[via][2], verde);
  }

  for (int j = 0; j < 4; j++) {
    bool verde = false, vermelho = true;
    if (fase == F_PEDESTRES && servindo[j]) {
      if (millis() - faseInicio < T_TRAVESSIA) { verde = true; vermelho = false; }
      else                                     { vermelho = blinkOn; }   // despeje a faixa
    } else if (fase == F_PISCANTE) {
      // Manual Brasileiro de Sinalizacao de Transito, vol. V, 4.3.4: em amarelo
      // intermitente os focos veiculares piscam em amarelo e "todos os grupos
      // focais de pedestres devem permanecer apagados". Vermelho de pedestre
      // piscando daria ao pedestre uma indicacao que o modo nao sustenta.
      verde = false;
      vermelho = false;
    }
    foco(PIN_PED[j][0], vermelho);
    foco(PIN_PED[j][1], verde);
  }
}

// ============================================================
//  BUZZER (travessia sonora)
// ============================================================
unsigned long ultimoToggleBuzzer = 0;
bool buzzerOn = false;

void atualizarBuzzer(unsigned long agora) {
  if (fase != F_PEDESTRES) { if (buzzerOn) { noTone(PIN_BUZZER); buzzerOn = false; } return; }
  unsigned long t = agora - faseInicio;
  unsigned long intervalo = (t < T_TRAVESSIA) ? 400 : 150;   // acelera no intermitente
  if (agora - ultimoToggleBuzzer >= intervalo) {
    ultimoToggleBuzzer = agora;
    buzzerOn = !buzzerOn;
    if (buzzerOn) tone(PIN_BUZZER, 1200);
    else          noTone(PIN_BUZZER);
  }
}

// ============================================================
//  MAQUINA DE FASES
// ============================================================
const char* nomeFase(Fase f) {
  switch (f) {
    case F_VIA_A:       return "VIA_A";
    case F_VIA_B:       return "VIA_B";
    case F_PEDESTRES:   return "PEDESTRES";
    case F_AMARELO:     return "AMARELO";
    case F_ENTREVERDES: return "ENTREVERDES";
    default:            return "PISCANTE";
  }
}

bool ehVerde(Fase f) { return f == F_VIA_A || f == F_VIA_B || f == F_PEDESTRES; }

void irPara(Fase nova) {
  if (nova == F_PEDESTRES)                      // congela o conjunto atendido
    for (int j = 0; j < 4; j++) servindo[j] = pedidoTravessia[j];
  fase = nova;
  faseInicio = millis();
  aplicarSaidas();
  publicarEstado();
}

/* Pedido de troca de fase, venha do Node-RED ou do ciclo local.
   Nunca troca verde por verde: passa sempre por amarelo + entreverdes. */
void pedirFase(Fase destino) {
  if (destino == fase || destino == faseDestino) return;
  if (destino == F_PEDESTRES) {                 // sem ninguem esperando, ignora
    bool algum = false;
    for (int j = 0; j < 4; j++) if (pedidoTravessia[j]) algum = true;
    if (!algum) return;
  }

  unsigned long t = millis() - faseInicio;

  if (ehVerde(fase)) {
    if (fase == F_PEDESTRES) {                  // travessia tem tempo fixo: fica pendente
      temPedidoFase = true; pedidoFase = destino; return;
    }
    if (t < T_VERDE_MIN) {                      // verde minimo: guarda o pedido
      temPedidoFase = true; pedidoFase = destino; return;
    }
    faseSaindo  = fase;
    faseDestino = destino;
    irPara(F_AMARELO);
    return;
  }
  faseDestino = destino;                        // entre fases: so anota o destino
}

// Ciclo local, usado quando o Node-RED nao esta mandando.
Fase proximaFaseLocal() {
  for (int j = 0; j < 4; j++) if (pedidoTravessia[j]) return F_PEDESTRES;
  return (faseSaindo == F_VIA_A) ? F_VIA_B : F_VIA_A;
}

void maquinaDeFases(unsigned long agora) {
  unsigned long t = agora - faseInicio;

  if (forcarPiscante) { if (fase != F_PISCANTE) irPara(F_PISCANTE); return; }
  if (fase == F_PISCANTE) { faseDestino = F_VIA_A; irPara(F_ENTREVERDES); return; }

  supervisionado = (ultimoComando != 0) && (agora - ultimoComando < T_SEM_COMANDO);

  switch (fase) {
    case F_AMARELO:
      if (t >= T_AMARELO) irPara(F_ENTREVERDES);
      break;

    case F_ENTREVERDES:
      if (t >= T_ENTREVERDES) {
        if (faseDestino == F_AMARELO || faseDestino == F_ENTREVERDES) faseDestino = F_VIA_A;
        irPara(faseDestino);
        if (fase != F_PEDESTRES) faseSaindo = fase;
      }
      break;

    case F_PEDESTRES:
      if (t >= T_TRAVESSIA + T_TRAV_INTERM) {
        for (int j = 0; j < 4; j++) if (servindo[j]) pedidoTravessia[j] = false;
        Fase destino = temPedidoFase ? pedidoFase : proximaFaseLocal();
        if (destino == F_PEDESTRES) destino = (faseSaindo == F_VIA_A) ? F_VIA_B : F_VIA_A;
        temPedidoFase = false;
        faseDestino = destino;
        irPara(F_ENTREVERDES);                  // travessia -> entreverdes -> verde
      }
      break;

    case F_VIA_A:
    case F_VIA_B:
      if (temPedidoFase && t >= T_VERDE_MIN) {  // comando que chegou cedo, agora vale
        temPedidoFase = false;
        pedirFase(pedidoFase);
        break;
      }
      // sem supervisao, o proprio ciclo local encerra o verde
      if (!supervisionado && t >= T_VERDE_FIXO) pedirFase(proximaFaseLocal());
      break;

    default: break;
  }
}

// ============================================================
//  MQTT
// ============================================================
bool publicar(const char* topico, const char* buf, size_t n) {
  // Nao existe publish(const char*, const char*, unsigned) no PubSubClient, e
  // char* nao converte implicitamente para uint8_t*: escrever
  // publish(topico, buf, n) casaria com a sobrecarga de retained e publicaria
  // RETIDO por acidente, sem aviso do compilador. Este wrapper forca false.
  return client.publish(topico, (const uint8_t*)buf, (unsigned int)n, false);
}

void publicarEstado() {
  if (!client.connected()) return;
  ultimaPublicacao = millis();

  JsonDocument doc;
  doc["fase"]           = nomeFase(fase);
  doc["destino"]        = nomeFase(faseDestino);
  doc["t_fase"]         = millis() - faseInicio;
  doc["supervisionado"] = supervisionado;
  // carros[] e fluxo[] agora vem da placa de sensores, em topico proprio; o
  // coordenador do Node-RED junta os dois. Esta placa so publica o que e dela.
  JsonArray p = doc["pedidos"].to<JsonArray>();
  for (int j = 0; j < 4; j++) p.add(pedidoTravessia[j] ? 1 : 0);
  JsonArray s = doc["servindo"].to<JsonArray>();
  for (int j = 0; j < 4; j++) s.add(servindo[j] ? 1 : 0);

  char buf[320];
  publicar(TOP_ESTADO, buf, serializeJson(doc, buf));
}

void tratarComando(const String& msg) {
  ultimoComando = millis();

  if (msg == "PISCANTE") { forcarPiscante = true;  return; }
  if (msg == "NORMAL")   { forcarPiscante = false; return; }

  JsonDocument doc;
  if (deserializeJson(doc, msg)) return;

  if (doc["pedestre"].is<int>()) {              // botoeira: fisica via Node-RED, ou do painel
    int j = doc["pedestre"];
    if (j >= 0 && j < 4) pedidoTravessia[j] = true;
  }
  if (doc["fase"].is<const char*>()) {
    const char* f = doc["fase"];
    if      (!strcmp(f, "VIA_A"))     pedirFase(F_VIA_A);
    else if (!strcmp(f, "VIA_B"))     pedirFase(F_VIA_B);
    else if (!strcmp(f, "PEDESTRES")) pedirFase(F_PEDESTRES);
    else if (!strcmp(f, "PISCANTE"))  forcarPiscante = true;
    else if (!strcmp(f, "NORMAL"))    forcarPiscante = false;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  (void)topic;
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  Serial.print("CMD: "); Serial.println(msg);
  tratarComando(msg);
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) { delay(300); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) { Serial.print("IP: "); Serial.println(WiFi.localIP()); }
  else Serial.println("WiFi falhou - sigo no ciclo local");
}

void tentarReconectarMqtt() {
  if (client.connected()) return;
  unsigned long agora = millis();
  if (agora - ultimaTentativaMqtt < INTERVALO_RECONEXAO) return;
  ultimaTentativaMqtt = agora;

  Serial.print("Conectando MQTT... ");
  if (client.connect(NODE_ID, TOP_STATUS, 1, true, "offline")) {
    Serial.println("ok");
    client.publish(TOP_STATUS, "online", true);   // retido de proposito (LWT)
    client.subscribe(TOP_CMD, 1);
  } else {
    Serial.print("falhou rc="); Serial.println(client.state());
  }
}

void publicarHeartbeat() {
  JsonDocument doc;
  doc["uptime"] = millis() / 1000;
  doc["rssi"]   = WiFi.RSSI();
  char buf[80];
  publicar(TOP_HB, buf, serializeJson(doc, buf));
}

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== SINALEIRAS DO CRUZAMENTO (2 vias + 4 travessias) ===");

  for (int via = 0; via < 2; via++)
    for (int c = 0; c < 3; c++) pinMode(PIN_CARRO[via][c], OUTPUT);
  for (int j = 0; j < 4; j++)
    for (int c = 0; c < 2; c++) pinMode(PIN_PED[j][c], OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  faseDestino = F_VIA_A;
  irPara(F_ENTREVERDES);            // nasce tudo vermelho

  setupWifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);
  client.setBufferSize(512);
}

void loop() {
  tentarReconectarMqtt();
  client.loop();

  unsigned long agora = millis();
  if (agora - ultimoBlink >= 350) { ultimoBlink = agora; blinkOn = !blinkOn; }

  maquinaDeFases(agora);
  aplicarSaidas();
  atualizarBuzzer(agora);

  if (agora - ultimaPublicacao >= INTERVALO_PUBLICACAO) publicarEstado();
  if (agora - ultimoHeartbeat >= INTERVALO_HEARTBEAT) {
    ultimoHeartbeat = agora;
    if (client.connected()) publicarHeartbeat();
  }
}
