// GERADO por ferramentas/gera_passo_a_passo.py — nao edite a mao.
//
// Sketch de bancada da placa 2 — iluminacao. Sem WiFi e sem MQTT de proposito: aqui
// se testa o CIRCUITO, e uma falha nao pode ter tres explicacoes possiveis.
// A pinagem vem do firmware (node_iluminacao.ino) — nao ha o que conferir a mao.
//
// Monitor Serial em 115200, com final de linha "Nova linha".
// Digite ? para ver o menu.

const int PIN_LDR = 32;   // ADC1
const int PIN_PRESENCA = 33;
const int NIVEL_DETECTADO = LOW;   // TCRT5000: LOW com objeto na frente
const int PIN_LED = 25;   // PWM

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

int brilho = 0, passo = 5;
bool presencaAntes = false;
int minLdr = 4095, maxLdr = 0;

void menu() {
  Serial.println();
  Serial.println(F("== bancada — placa 2, iluminacao =="));
  Serial.println(F("  0   apaga o LED"));
  Serial.println(F("  1   rampa de PWM: o LED sobe e desce continuamente"));
  Serial.println(F("  2   LDR: leitura, tensao e os extremos ja vistos"));
  Serial.println(F("  3   presenca (TCRT5000): imprime quando muda"));
  Serial.println(F("  4   conjunto: so acende com escuro E presenca"));
  Serial.println();
}

void entrar(int m) {
  modo = m;
  t0 = millis();
  analogWrite(PIN_LED, 0);
  brilho = 0;
  switch (m) {
    case 0: Serial.println(F("[0] LED apagado")); break;
    case 1: Serial.println(F("[1] rampa — se piscar em degraus, o resistor esta alto demais")); break;
    case 2:
      minLdr = 4095; maxLdr = 0;
      Serial.println(F("[2] tape o LDR e depois ilumine. Anote os dois extremos:"));
      Serial.println(F("    o limiar do firmware fica no meio deles."));
      break;
    case 3: Serial.println(F("[3] passe a mao a ~1 cm do TCRT5000")); break;
    case 4: Serial.println(F("[4] os dois sensores juntos, como no firmware")); break;
    default: Serial.println(F("modo desconhecido — ? para o menu")); modo = 0;
  }
}

void executar(String s) {
  s.trim();
  if (s == "?") { menu(); return; }
  if (s.length() == 1 && s[0] >= '0' && s[0] <= '4') { entrar(s[0] - '0'); return; }
  Serial.println(F("comando desconhecido — ? para o menu"));
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(PIN_PRESENCA, INPUT);
  pinMode(PIN_LED, OUTPUT);
  analogWrite(PIN_LED, 0);
  menu();
  entrar(0);
}

void loop() {
  lerComando();
  unsigned long agora = millis();

  if (modo == 1 && agora - t0 >= 15) {
    t0 = agora;
    brilho += passo;
    if (brilho >= 255) { brilho = 255; passo = -5; }
    if (brilho <= 0)   { brilho = 0;   passo =  5; }
    analogWrite(PIN_LED, brilho);
  }

  if (modo == 2 && agora - t0 >= 300) {
    t0 = agora;
    int v = analogRead(PIN_LDR);
    if (v < minLdr) minLdr = v;
    if (v > maxLdr) maxLdr = v;
    Serial.print(F("LDR = "));
    Serial.print(v);
    Serial.print(F("  ("));
    Serial.print(v * 3.3 / 4095.0, 2);
    Serial.print(F(" V)   min "));
    Serial.print(minLdr);
    Serial.print(F("  max "));
    Serial.println(maxLdr);
  }

  if (modo == 3 && agora - t0 >= 50) {
    t0 = agora;
    bool p = (digitalRead(PIN_PRESENCA) == NIVEL_DETECTADO);
    if (p != presencaAntes) {
      Serial.println(p ? F("presenca: DETECTADO") : F("presenca: livre"));
      presencaAntes = p;
    }
  }

  if (modo == 4 && agora - t0 >= 200) {
    t0 = agora;
    int v = analogRead(PIN_LDR);
    bool p = (digitalRead(PIN_PRESENCA) == NIVEL_DETECTADO);
    int b = (v < 1000 && p) ? 255 : 0;   // as duas condicoes, como no firmware
    analogWrite(PIN_LED, b);
    Serial.print(F("LDR = ")); Serial.print(v);
    Serial.print(F("  PRESENCA = ")); Serial.print(p ? 1 : 0);
    Serial.print(F("  ->  brilho ")); Serial.println(b);
  }
}
