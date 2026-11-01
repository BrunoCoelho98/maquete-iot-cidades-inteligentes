/* ============================================================
   MINI SMART CITY  -  Módulo da atividade: PONTO DE ÔNIBUS INTELIGENTE
   ------------------------------------------------------------
   O que este programa faz, em palavras simples:

     1. Quando alguém aperta o BOTÃO do ponto, a placa manda uma
        mensagem para a central da cidade: "AGUARDANDO".
     2. A central (o painel no computador) pode mandar dois
        comandos de volta para o ponto:
          VINDO  -> a LUZ do ponto pisca: o ônibus está a caminho.
          CHEGOU -> a LUZ fica acesa um pouco e apaga: o passageiro
                    embarcou, e o ponto avisa a central "VAZIO".

   Sensor (entrada) : o botão       -> percebe o passageiro
   Atuador (saída)  : a luz (LED)   -> avisa o passageiro
   ============================================================ */


// =================== ÁREA DO ESTUDANTE ===================
// Estas são as únicas linhas que você vai mudar.

// 1) O nome do seu ponto de ônibus. Escreva entre as aspas.
//    Ele vai aparecer no painel da cidade, no computador.
const char* NOME_DO_PONTO = "Ponto do Ollie";

// 2) A velocidade do pisca-pisca quando o ônibus está vindo,
//    em milissegundos (1000 = 1 segundo). Número menor = pisca mais rápido.
const int VELOCIDADE_PISCA = 350;

// ======================================================
//  Daqui para baixo é a parte técnica. Você pode ler à
//  vontade, mas não precisa mudar nada.
// ======================================================


/* NOTAS PARA O MEDIADOR (e para quem integrar o módulo depois)

   Segue o mesmo formato de nó das outras cinco placas - telemetria em
   <base>/estado, comandos em <base>/cmd, LWT retido em <base>/status e
   heartbeat em <base>/heartbeat -, que é o que permite acrescentá-lo sem
   mexer em nenhuma placa existente.

   Duas diferenças deliberadas, ambas por causa do público da atividade:
   - A telemetria é TEXTO SIMPLES ("AGUARDANDO"/"VAZIO"), não JSON. O estudante lê
     a mensagem crua no painel de depuração do Node-RED e liga o "mqtt in"
     direto num "ui_text", sem nó de função no meio.
   - A telemetria sai SÓ QUANDO ALGO ACONTECE (e ao conectar), não em período
     fixo. Um aperto de botão = uma mensagem na tela. Com publicação a cada
     1,5 s, como nas outras placas, a primeira coisa que ela veria seria uma
     enxurrada de mensagens iguais, e a relação causa-efeito se perderia.

   O heartbeat leva o NOME_DO_PONTO; o painel "Estado da Cidade" usa esse
   campo para mostrar o módulo com o nome que o estudante escolheu.

   Core: Arduino-ESP32 3.x   Bibliotecas: PubSubClient, ArduinoJson (v7) */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

const char* NODE_ID = "esp32-ponto";

// ---------- TÓPICOS (os "endereços" das mensagens) ----------
const char* TOP_ESTADO = TOPIC_BASE "cidade/ponto/estado";     // o ponto fala
const char* TOP_CMD    = TOPIC_BASE "cidade/ponto/cmd";        // o ponto escuta
const char* TOP_STATUS = TOPIC_BASE "cidade/ponto/status";     // online / offline
const char* TOP_HB     = TOPIC_BASE "cidade/ponto/heartbeat";  // "estou vivo"

// ---------- PINOS (onde cada fio entra na placa) ----------
// Na placa, estes pinos estão escritos como D18 e D19.
const int PIN_LUZ   = 18;   // LED, através do resistor de 220 ohms
const int PIN_BOTAO = 19;   // botão, com a outra perna no GND
// Escolhidos pensando no estudante, nao no circuito: tem o numero impresso na placa (o
// 16 e o 17 aparecem como RX2/TX2 e ela nao os acharia), ficam lado a lado e
// na fileira voltada para a protoboard. Nenhum e pino de strapping.

// ---------- TEMPOS ----------
const unsigned long TEMPO_NO_PONTO = 5000;   // luz acesa enquanto o ônibus está parado
const unsigned long DEBOUNCE_MS    = 40;     // botão mecânico "treme" ao ser apertado

// ---------- ESTADO ----------
enum Onibus { LONGE, VINDO, NO_PONTO };
Onibus onibus = LONGE;
bool passageiroEsperando = false;
unsigned long chegadaEm = 0;
unsigned long ultimoPisca = 0;
bool luzAcesa = false;

bool botaoLido = false, botaoEstavel = false;
unsigned long botaoMudouEm = 0;

// ---------- INFRA ----------
WiFiClient   espClient;
PubSubClient client(espClient);

unsigned long ultimaTentativaMqtt = 0;
const unsigned long INTERVALO_RECONEXAO = 5000;
unsigned long ultimoHeartbeat = 0;
const unsigned long INTERVALO_HEARTBEAT = 10000;

// ============================================================
//  1. CONEXÕES
// ============================================================
void conectarWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando ao Wi-Fi \"");
  Serial.print(WIFI_SSID);
  Serial.print("\"");
  unsigned long inicio = millis();
  bool avisou = false;
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    // O ESP32 so enxerga rede de 2,4 GHz; com rede so de 5 GHz ele fica aqui
    // para sempre, sem erro nenhum. Melhor dizer isso do que so imprimir pontos.
    if (!avisou && millis() - inicio > 20000) {
      avisou = true;
      Serial.println();
      Serial.println("  Está demorando. Confira se a rede está ligada e se ela é de 2,4 GHz.");
    }
  }
  Serial.println();
  Serial.print("Wi-Fi conectado! Endereço da placa na rede: ");
  Serial.println(WiFi.localIP());
}

void publicarEstado() {
  const char* msg = passageiroEsperando ? "AGUARDANDO" : "VAZIO";
  // publish(topico, texto) de dois argumentos: NAO retido. Ver o aviso sobre
  // as sobrecargas do PubSubClient em node_seguranca.ino.
  client.publish(TOP_ESTADO, msg);
  Serial.print("  Mensagem enviada para a central: ");
  Serial.println(msg);
}

void publicarHeartbeat() {
  JsonDocument doc;
  doc["uptime"] = millis() / 1000;
  doc["rssi"]   = WiFi.RSSI();
  doc["nome"]   = NOME_DO_PONTO;
  char buf[160];
  size_t n = serializeJson(doc, buf);
  client.publish(TOP_HB, (const uint8_t*)buf, (unsigned int)n, false);
}

void tentarConectarCentral() {
  if (client.connected()) return;
  unsigned long agora = millis();
  if (agora - ultimaTentativaMqtt < INTERVALO_RECONEXAO) return;
  ultimaTentativaMqtt = agora;

  Serial.print("Conectando à central da cidade (broker MQTT)... ");
  // Se a placa cair, o proprio broker publica "offline" por ela (LWT).
  if (client.connect(NODE_ID, TOP_STATUS, 1, true, "offline")) {
    Serial.println("conectado!");
    client.publish(TOP_STATUS, "online", true);
    client.subscribe(TOP_CMD);
    publicarEstado();        // a central fica sabendo como o ponto esta agora
    publicarHeartbeat();     // e o nome aparece no painel sem esperar 10 s
    ultimoHeartbeat = agora;
  } else {
    Serial.print("não deu (código ");
    Serial.print(client.state());
    Serial.println("). Tentando de novo em 5 segundos.");
  }
}

// ============================================================
//  2. QUANDO CHEGA UMA MENSAGEM DA CENTRAL
// ============================================================
void quandoChegaMensagem(char* topico, byte* conteudo, unsigned int tamanho) {
  String msg;
  for (unsigned int i = 0; i < tamanho; i++) msg += (char)conteudo[i];
  // Tolerante de proposito: quem digita o comando no painel e iniciante.
  // " vindo ", "Vindo" e "\"VINDO\"" (payload JSON por engano) valem o mesmo.
  msg.replace("\"", "");
  msg.trim();
  msg.toUpperCase();

  Serial.print("Chegou uma mensagem da central: ");
  Serial.println(msg);

  if (msg == "VINDO") {
    onibus = VINDO;
    Serial.println("  -> O ônibus está vindo. A luz vai piscar.");
  } else if (msg == "CHEGOU") {
    onibus = NO_PONTO;
    chegadaEm = millis();
    Serial.println("  -> O ônibus chegou. A luz fica acesa enquanto o passageiro embarca.");
  } else {
    Serial.println("  -> Não conheço esse comando. Os comandos são VINDO e CHEGOU.");
  }
}

// ============================================================
//  3. O SENSOR: o botão
// ============================================================
void lerBotao(unsigned long agora) {
  bool apertado = digitalRead(PIN_BOTAO) == LOW;   // com INPUT_PULLUP, apertado = LOW
  if (apertado != botaoLido) {
    botaoLido = apertado;
    botaoMudouEm = agora;
  }
  if (agora - botaoMudouEm < DEBOUNCE_MS || apertado == botaoEstavel) return;
  botaoEstavel = apertado;
  if (!botaoEstavel) return;                        // so interessa o apertar, nao o soltar

  Serial.println("Botão apertado! Tem passageiro esperando.");
  passageiroEsperando = true;
  // Publica a cada aperto, mesmo se ja estava AGUARDANDO: na atividade,
  // cada aperto precisa virar uma mensagem visivel na tela.
  if (client.connected()) publicarEstado();
  else Serial.println("  Sem conexão com a central: o aviso NÃO foi enviado.");
}

// ============================================================
//  4. O ATUADOR: a luz
// ============================================================
void atualizarLuz(unsigned long agora) {
  switch (onibus) {
    case LONGE:
      digitalWrite(PIN_LUZ, LOW);
      break;

    case VINDO:
      if (agora - ultimoPisca >= (unsigned long)VELOCIDADE_PISCA) {
        luzAcesa = !luzAcesa;
        ultimoPisca = agora;
      }
      digitalWrite(PIN_LUZ, luzAcesa ? HIGH : LOW);
      break;

    case NO_PONTO:
      digitalWrite(PIN_LUZ, HIGH);
      if (agora - chegadaEm >= TEMPO_NO_PONTO) {
        onibus = LONGE;
        if (passageiroEsperando) {
          passageiroEsperando = false;
          Serial.println("O passageiro embarcou. O ponto está vazio.");
          if (client.connected()) publicarEstado();
        }
      }
      break;
  }
}

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== Ponto de Ônibus Inteligente ===");
  Serial.print("Nome do ponto: ");
  Serial.println(NOME_DO_PONTO);

  pinMode(PIN_LUZ, OUTPUT);
  pinMode(PIN_BOTAO, INPUT_PULLUP);   // resistor interno: o botao nao precisa de outro
  digitalWrite(PIN_LUZ, LOW);

  conectarWifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(quandoChegaMensagem);
}

void loop() {
  tentarConectarCentral();
  client.loop();

  unsigned long agora = millis();
  lerBotao(agora);
  atualizarLuz(agora);

  if (agora - ultimoHeartbeat >= INTERVALO_HEARTBEAT) {
    ultimoHeartbeat = agora;
    if (client.connected()) publicarHeartbeat();
  }

  delay(10);
}
