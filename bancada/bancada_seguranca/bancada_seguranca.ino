// GERADO por ferramentas/gera_passo_a_passo.py — nao edite a mao.
//
// Sketch de bancada da placa 4 — seguranca. Sem WiFi e sem MQTT de proposito: aqui
// se testa o CIRCUITO, e uma falha nao pode ter tres explicacoes possiveis.
// A pinagem vem do firmware (node_seguranca.ino) — nao ha o que conferir a mao.
//
// Monitor Serial em 115200, com final de linha "Nova linha".
// Digite ? para ver o menu.

const int PIN_MQ2    = 34;   // ADC1, entrada apenas — via divisor 1:2
const int PIN_BUZZER = 13;

const int N_LED = 3;
const int PIN_LED[N_LED] = { 25, 26, 27 };
const char* NOME[N_LED]  = { "verde", "amarelo", "vermelho" };

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

int varre = 0, minimo = 4095, maximo = 0;

void apagarLeds() { for (int i = 0; i < N_LED; i++) digitalWrite(PIN_LED[i], LOW); }

void menu() {
  Serial.println();
  Serial.println(F("== bancada — placa 4, seguranca =="));
  Serial.println(F("  0   apaga tudo"));
  Serial.println(F("  1   varredura dos 3 LEDs"));
  Serial.println(F("  2   MQ-2: leitura bruta, tensao e os extremos ja vistos"));
  Serial.println(F("  3   buzzer: tres bips"));
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
      minimo = 4095; maximo = 0;
      Serial.println(F("[2] deixe uns 20 s em ar limpo e anote o valor estavel."));
      Serial.println(F("    depois aproxime a fonte de gas: a diferenca define os limiares."));
      Serial.println(F("    a tensao NAO pode passar de 3,3 V — se passar, o divisor esta errado."));
      break;
    case 3:
      Serial.println(F("[3] tres bips"));
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
  if (s.length() == 1 && s[0] >= '0' && s[0] <= '3') { entrar(s[0] - '0'); return; }
  Serial.println(F("comando desconhecido — ? para o menu"));
}

void setup() {
  Serial.begin(115200);
  delay(300);
  for (int i = 0; i < N_LED; i++) pinMode(PIN_LED[i], OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
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

  if (modo == 2 && agora - t0 >= 500) {
    t0 = agora;
    int v = analogRead(PIN_MQ2);
    if (v < minimo) minimo = v;
    if (v > maximo) maximo = v;
    Serial.print(F("MQ-2 = "));
    Serial.print(v);
    Serial.print(F("  ("));
    Serial.print(v * 3.3 / 4095.0, 2);
    Serial.print(F(" V)   min "));
    Serial.print(minimo);
    Serial.print(F("  max "));
    Serial.println(maximo);
  }
}
