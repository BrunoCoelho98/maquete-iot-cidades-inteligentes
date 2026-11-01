// GERADO por ferramentas/gera_passo_a_passo.py — nao edite a mao.
//
// Sketch de bancada da placa 3 — ambiental. Sem WiFi e sem MQTT de proposito: aqui
// se testa o CIRCUITO, e uma falha nao pode ter tres explicacoes possiveis.
// A pinagem vem do firmware (node_ambiental.ino) — nao ha o que conferir a mao.
//
// Monitor Serial em 115200, com final de linha "Nova linha".
// Digite ? para ver o menu.

#include <ESP32Servo.h>
#include "dht_simples.h"   // copiado de node_ambiental/ por este gerador

const int PIN_NIVEL  = 35;   // ADC1
const int PIN_DHT    = 4;
const int PIN_SERVO  = 25;
const int PIN_BUZZER = 13;

// os mesmos pontos de calibracao do firmware
const int NIVEL_SECO  = 100;
const int NIVEL_CHEIO = 2200;

const int N_LED = 7;
const int PIN_LED[N_LED] = { 19, 21, 22, 23, 26, 27, 14 };
const char* NOME[N_LED]  = { "nivel verde", "nivel amarelo", "nivel laranja", "nivel vermelho", "temp verde", "temp amarelo", "temp vermelho" };

Servo comporta;

// ---------------------------------------------------------------- comandos
String linha;
int modo = 0;
unsigned long t0 = 0;

void lerComando() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (linha.length()) { executar(linha); linha = ""; }
    } else {
      linha += c;
    }
  }
}

int varre = 0, ang = 0, passo = 3;
int minNivel = 4095, maxNivel = 0;

void apagarLeds() { for (int i = 0; i < N_LED; i++) digitalWrite(PIN_LED[i], LOW); }

void menu() {
  Serial.println();
  Serial.println(F("== bancada — placa 3, ambiental =="));
  Serial.println(F("  0   apaga tudo"));
  Serial.println(F("  1   varredura dos 7 LEDs, com o nome no serial"));
  Serial.println(F("  2   sensor de nivel: leitura crua, % e os extremos (para calibrar)"));
  Serial.println(F("  3   DHT22: temperatura e umidade"));
  Serial.println(F("  4   servo: varre de 0 a 90 e volta"));
  Serial.println(F("  5   buzzer: tres bips"));
  Serial.println();
}

void entrar(int m) {
  modo = m;
  t0 = millis();
  varre = 0;
  apagarLeds();
  switch (m) {
    case 0: Serial.println(F("[0] tudo apagado")); break;
    case 1: Serial.println(F("[1] confira se o LED que acende e o que o serial diz")); break;
    case 2:
      minNivel = 4095; maxNivel = 0;
      Serial.println(F("[2] anote a leitura CRUA com o sensor seco e com ele submerso ate"));
      Serial.println(F("    o topo. Esses dois numeros viram NIVEL_SECO e NIVEL_CHEIO no"));
      Serial.println(F("    firmware. Mais submersao = leitura maior = pior."));
      break;
    case 3: Serial.println(F("[3] segure o sensor entre os dedos: a temperatura sobe")); break;
    case 4: Serial.println(F("[4] o servo tem que ir e voltar sem tremer nem travar")); break;
    case 5:
      Serial.println(F("[5] tres bips"));
      for (int i = 0; i < 3; i++) { tone(PIN_BUZZER, 2000, 120); delay(260); }
      noTone(PIN_BUZZER);
      modo = 0;
      break;
    default: Serial.println(F("modo desconhecido — ? para o menu")); modo = 0;
  }
}

void executar(String s) {
  s.trim();
  if (s == "?") { menu(); return; }
  if (s.length() == 1 && s[0] >= '0' && s[0] <= '5') { entrar(s[0] - '0'); return; }
  Serial.println(F("comando desconhecido — ? para o menu"));
}

void setup() {
  Serial.begin(115200);
  delay(300);
  for (int i = 0; i < N_LED; i++) pinMode(PIN_LED[i], OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  comporta.attach(PIN_SERVO);
  comporta.write(0);
  apagarLeds();
  menu();
  entrar(0);
}

void loop() {
  lerComando();
  unsigned long agora = millis();

  if (modo == 1 && agora - t0 >= 800) {
    t0 = agora;
    apagarLeds();
    digitalWrite(PIN_LED[varre], HIGH);
    Serial.print(F("GPIO "));
    Serial.print(PIN_LED[varre]);
    Serial.print(F("  ->  "));
    Serial.println(NOME[varre]);
    varre = (varre + 1) % N_LED;
  }

  if (modo == 2 && agora - t0 >= 400) {
    t0 = agora;
    int cru = analogRead(PIN_NIVEL);
    if (cru < minNivel) minNivel = cru;
    if (cru > maxNivel) maxNivel = cru;
    long pct = (long)(cru - NIVEL_SECO) * 100 / (NIVEL_CHEIO - NIVEL_SECO);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    Serial.print(F("cru "));
    Serial.print(cru);
    Serial.print(F("  ->  "));
    Serial.print(pct);
    Serial.print(F(" %   min "));
    Serial.print(minNivel);
    Serial.print(F("  max "));
    Serial.println(maxNivel);
  }

  // 2,5 s: o DHT22 nao aceita mais de uma leitura a cada 2 s
  if (modo == 3 && agora - t0 >= 2500) {
    t0 = agora;
    LeituraDht d = lerDht22(PIN_DHT);
    if (d.ok) {
      Serial.print(d.temperatura, 1); Serial.print(F(" C   "));
      Serial.print(d.umidade, 1);     Serial.println(F(" % de umidade"));
    } else {
      Serial.print(F("DHT22 nao respondeu - ")); Serial.println(d.erro);
    }
  }

  if (modo == 4 && agora - t0 >= 25) {
    t0 = agora;
    ang += passo;
    if (ang >= 90) { ang = 90; passo = -3; }
    if (ang <= 0)  { ang = 0;  passo =  3; }
    comporta.write(ang);
  }
}
