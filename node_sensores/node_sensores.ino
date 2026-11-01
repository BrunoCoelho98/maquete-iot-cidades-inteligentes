/* ============================================================
   MINI SMART CITY - PLACA 1B: SENSORES DO CRUZAMENTO
   ------------------------------------------------------------
   Le todas as entradas do cruzamento e publica. So isso.

     4 detectores de aproximacao (um por via)
     3 sensores de vaga do estacionamento
     4 botoeiras de pedestre

   NAO ACIONA NENHUM FOCO, DE PROPOSITO. Os focos e a maquina de
   fases moram inteiros na node_sinaleiras, para que o invariante
   de seguranca - nunca dois verdes conflitantes - nao dependa da
   rede. Aqui nao existe estado de semaforo nenhum: se esta placa
   travar, ficar sem WiFi ou for desligada, o cruzamento continua
   funcionando no ciclo de tempo fixo. Perde-se eficiencia
   (retencao de verde, prioridade de pedestre), nunca seguranca.

   DETECTORES: modulo optico reflexivo TCRT5000
   O firmware antigo usava ultrassonico e convertia a distancia
   num booleano na linha seguinte - a informacao util sempre foi
   "tem carro ou nao". O modulo entrega esse booleano direto,
   custa uma fracao do preco e ainda elimina o pulseIn com timeout
   de 30 ms, que era o que obrigava a ler um sensor por volta.
   Agora os sete sao lidos na mesma passada.

   O TCRT5000 e reflexivo e de alcance curto - MILIMETROS, nao
   centimetros -, entao ele nao olha a via de lado: vai embutido
   NO PISO, virado para cima, e o carrinho passa ou para em cima
   dele. Isso e melhor do que parece, e nao e improviso: e o
   analogo em escala do laco indutivo, que na rua fica sob o
   asfalto na linha de retencao e e o detector padrao do controle
   atuado. A geometria da maquete ficou mais parecida com a real,
   nao menos.
   O preco disso e a REFLETANCIA: carrinho de fundo preto fosco
   devolve pouco infravermelho e pode nao ser visto. Adesivo
   branco ou fita clara sob o chassi resolve, e cada modulo tem
   trimpot para acertar o ponto.

   Alimente os modulos em 3,3 V: a saida deles acompanha a
   alimentacao, e em 5 V ela levaria 5 V ao GPIO. O comparador
   LM393 tem coletor aberto com pull-up embutido no modulo, entao
   os quatro detectores podem ficar em GPIO 34-39, que nao tem
   pull-up interno. As botoeiras nao podem: essas usam o pull-up
   do proprio ESP32 e precisam de GPIO comum.
   O pino AO de cada modulo fica sem ligacao - a leitura util e o
   booleano, e sete entradas analogicas nao caberiam no ADC1.

   Par:      node_sinaleiras (focos e maquina de fases)
   Core:     Arduino-ESP32 3.x
   Libs:     PubSubClient, ArduinoJson (v7)
   ============================================================ */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

const char* NODE_ID = "esp32-sensores";

// ---------- TOPICOS ----------
const char* TOP_SENSORES       = TOPIC_BASE "cidade/cruzamento/sensores";
const char* TOP_BOTOEIRA       = TOPIC_BASE "cidade/cruzamento/botoeira";
const char* TOP_ESTACIONAMENTO = TOPIC_BASE "cidade/estacionamento/estado";
const char* TOP_STATUS         = TOPIC_BASE "cidade/sensores/status";
const char* TOP_HB             = TOPIC_BASE "cidade/sensores/heartbeat";

// ---------- PINOS ----------
// aproximacoes: 0=Norte 1=Sul 2=Leste 3=Oeste
// em 34/35/36/39 (so entrada, sem pull-up interno) porque o modulo IR traz o
// proprio pull-up - nao ha pino flutuante aqui.
const int PIN_APROX[4] = { 34, 35, 36, 39 };
const int PIN_VAGA[3]  = { 32, 33, 25 };
// Botoeiras na mesma ordem das travessias, em GPIO comuns de proposito: usam
// INPUT_PULLUP e dispensam resistor externo. Em 34-39 elas flutuariam - na
// simulacao isso deu botoeira apertada sozinha para sempre.
//
// Ficam de proposito do LADO OPOSTO do devkit em relacao aos sensores: os sete
// modulos caem todos na fileira 34..25, e se as botoeiras ficassem la
// tambem, cada fio delas teria de contornar a placa para chegar a protoboard.
// Nenhum destes quatro e pino de strapping - 5 seria, porque nasce em nivel
// alto e uma botoeira apertada no boot o derrubaria.
const int PIN_BOTOEIRA[4] = { 17, 21, 18, 19 };
// Sobram 4, 13, 14, 16, 22, 23, 26, 27 para expansao.

// Nivel que o modulo apresenta COM objeto na frente. O TCRT5000 vai a LOW; se o
// seu modulo for o contrario, esta e a unica linha a trocar.
const int NIVEL_DETECTADO = LOW;

// Botoeira: confirmacao por PERMANENCIA, nao por borda unica.
//
// A versao anterior publicava no PRIMEIRO digitalRead em LOW, e os 40 ms de
// debounce so bloqueavam o evento seguinte - nunca exigiam que o nivel se
// mantivesse. Um pulso acoplado de microssegundos bastava para pedir travessia.
// Medido na bancada, com os focos ja em modulos de mini sinaleira: 19 pedidos
// em 50 s sem ninguem encostar na botoeira, treze deles a menos de 0,85 s de
// uma troca de fase - o chaveamento dos focos entrando pelo fio da botoeira.
//
// Na MESMA placa, os quatro TCRT5000 nao mudaram de estado uma vez em 100
// amostras. A diferenca nao e sorte: o modulo do TCRT5000 tem pull-up proprio
// (comparador de coletor aberto, impedancia baixa), e a botoeira depende do
// pull-up interno do ESP32, de ~45 kOhm, que deixa o fio em alta impedancia -
// ou seja, uma antena.
//
// Um aperto humano dura mais de 100 ms; ruido acoplado, bem menos. Exigir
// T_FIRME de leitura estavel separa os dois sem hardware nenhum. E o mesmo
// esquema de atividade/ponto_onibus, que ja nascia assim.
const unsigned long T_FIRME = 60;     // ms de leitura estavel para valer aperto

// ---------- ESTADO ----------
bool carroPresente[4] = { false, false, false, false };
bool presenteAntes[4] = { false, false, false, false };
int  fluxo[4]         = { 0, 0, 0, 0 };

int vagaLivre[3] = { 1, 1, 1 };
int vagasLivres  = 3;

bool botaoLido[4]               = { false, false, false, false };  // leitura crua
bool botaoFirme[4]              = { false, false, false, false };  // ja confirmada
unsigned long botaoMudouEm[4]   = { 0, 0, 0, 0 };

// ---------- INFRA ----------
WiFiClient   espClient;
PubSubClient client(espClient);
unsigned long ultimaTentativaMqtt = 0;
const unsigned long INTERVALO_RECONEXAO = 5000;
unsigned long ultimoHeartbeat = 0;
const unsigned long INTERVALO_HEARTBEAT = 10000;
unsigned long ultimaPublicacao = 0;
const unsigned long INTERVALO_PUBLICACAO = 500;
unsigned long ultimoScanVagas = 0;
const unsigned long INTERVALO_VAGAS = 1000;

bool publicar(const char* topico, const char* buf, size_t n) {
  // Mesma armadilha do PubSubClient de sempre: publish(topico, buf, n) casaria
  // com a sobrecarga de retained e publicaria RETIDO sem aviso do compilador,
  // porque char* nao converte implicitamente para uint8_t*. Este wrapper forca
  // false. Telemetria retida faz o painel mostrar carro parado que ja foi
  // embora, e o coordenador decidir fase com dado velho.
  return client.publish(topico, (const uint8_t*)buf, (unsigned int)n, false);
}

// ============================================================
//  ENTRADAS
// ============================================================
void lerAproximacoes() {
  for (int i = 0; i < 4; i++) {
    carroPresente[i] = (digitalRead(PIN_APROX[i]) == NIVEL_DETECTADO);
    if (carroPresente[i] && !presenteAntes[i]) fluxo[i]++;   // borda: veiculo novo
    presenteAntes[i] = carroPresente[i];
  }
}

// Cada aperto vira um evento unico. O Node-RED repassa como {"pedestre":j} para
// a placa das sinaleiras - o mesmo caminho da botoeira virtual do painel, que ja
// existia. Evento em vez de estado porque esta placa nao tem como saber quando a
// travessia foi atendida; quem zera o pedido e a maquina de fases, do outro lado.
void lerBotoeiras(unsigned long agora) {
  for (int j = 0; j < 4; j++) {
    bool apertada = (digitalRead(PIN_BOTOEIRA[j]) == LOW);   // pull-up interno

    if (apertada != botaoLido[j]) {        // mudou agora: recomeca a contagem
      botaoLido[j] = apertada;
      botaoMudouEm[j] = agora;
    }
    if (agora - botaoMudouEm[j] < T_FIRME) continue;   // ainda nao firmou
    if (apertada == botaoFirme[j])          continue;   // nada novo
    botaoFirme[j] = apertada;
    if (!apertada)                          continue;   // so o apertar, nao o soltar

    Serial.print("botoeira "); Serial.println(j);
    if (client.connected()) {
      JsonDocument doc;
      doc["j"] = j;
      char buf[32];
      publicar(TOP_BOTOEIRA, buf, serializeJson(doc, buf));
    }
  }
}

void scanVagas() {
  vagasLivres = 0;
  for (int i = 0; i < 3; i++) {
    vagaLivre[i] = (digitalRead(PIN_VAGA[i]) == NIVEL_DETECTADO) ? 0 : 1;
    vagasLivres += vagaLivre[i];
  }
  if (!client.connected()) return;
  JsonDocument doc;
  doc["livres"] = vagasLivres;
  JsonArray v = doc["vagas"].to<JsonArray>();
  for (int i = 0; i < 3; i++) v.add(vagaLivre[i]);
  char buf[96];
  publicar(TOP_ESTACIONAMENTO, buf, serializeJson(doc, buf));
}

// ============================================================
//  MQTT
// ============================================================
void publicarSensores() {
  if (!client.connected()) return;
  ultimaPublicacao = millis();

  JsonDocument doc;
  JsonArray c = doc["carros"].to<JsonArray>();
  for (int i = 0; i < 4; i++) c.add(carroPresente[i] ? 1 : 0);
  JsonArray f = doc["fluxo"].to<JsonArray>();
  for (int i = 0; i < 4; i++) f.add(fluxo[i]);
  doc["livres"] = vagasLivres;

  char buf[200];
  publicar(TOP_SENSORES, buf, serializeJson(doc, buf));
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) { delay(300); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) { Serial.print("IP: "); Serial.println(WiFi.localIP()); }
  else Serial.println("WiFi falhou - o cruzamento segue no ciclo local");
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
  Serial.println("\n=== SENSORES DO CRUZAMENTO (4 aproximacoes + 3 vagas + 4 botoeiras) ===");

  for (int i = 0; i < 4; i++) pinMode(PIN_APROX[i], INPUT);       // pull-up e do modulo
  for (int i = 0; i < 3; i++) pinMode(PIN_VAGA[i], INPUT);
  for (int j = 0; j < 4; j++) pinMode(PIN_BOTOEIRA[j], INPUT_PULLUP);

  setupWifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setBufferSize(512);
}

void loop() {
  tentarReconectarMqtt();
  client.loop();

  unsigned long agora = millis();

  lerAproximacoes();
  lerBotoeiras(agora);

  if (agora - ultimoScanVagas >= INTERVALO_VAGAS) { ultimoScanVagas = agora; scanVagas(); }
  if (agora - ultimaPublicacao >= INTERVALO_PUBLICACAO) publicarSensores();
  if (agora - ultimoHeartbeat >= INTERVALO_HEARTBEAT) {
    ultimoHeartbeat = agora;
    if (client.connected()) publicarHeartbeat();
  }
}
