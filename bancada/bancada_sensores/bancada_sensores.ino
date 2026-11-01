// GERADO por ferramentas/gera_passo_a_passo.py — nao edite a mao.
//
// Sketch de bancada da placa 1B — sensores. Sem WiFi e sem MQTT de proposito: aqui
// se testa o CIRCUITO, e uma falha nao pode ter tres explicacoes possiveis.
// A pinagem vem do firmware (node_sensores.ino) — nao ha o que conferir a mao.
//
// Monitor Serial em 115200, com final de linha "Nova linha".
// Digite ? para ver o menu.

const int PIN_APROX[4]    = { 34, 35, 36, 39 };
const int PIN_VAGA[3]     = { 32, 33, 25 };
const int PIN_BOTOEIRA[4] = { 17, 21, 18, 19 };

// nivel que o modulo IR apresenta COM obstaculo (o mesmo do firmware)
const int NIVEL_DETECTADO = LOW;

const int N_IR = 7;
const char* NOME_IR[N_IR] = { "aprox N", "aprox S", "aprox L", "aprox O", "vaga 1", "vaga 2", "vaga 3" };
const char* NOME_BOT[4]   = { "botoeira N", "botoeira S", "botoeira L", "botoeira O" };

int pinoIR(int i) { return (i < 4) ? PIN_APROX[i] : PIN_VAGA[i - 4]; }
bool detectado(int i) { return digitalRead(pinoIR(i)) == NIVEL_DETECTADO; }

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

bool irAntes[N_IR]  = { false, false, false, false, false, false, false };
bool botAntes[4]    = { false, false, false, false };

void menu() {
  Serial.println();
  Serial.println(F("== bancada — placa 1B, sensores =="));
  Serial.println(F("  0   parado"));
  Serial.println(F("  1   detectores IR: imprime so quando algum muda"));
  Serial.println(F("  2   detectores IR: imprime os 7 continuamente (para ajustar o trimpot)"));
  Serial.println(F("  3   botoeiras: imprime a que voce apertar"));
  Serial.println(F("  4   tudo junto"));
  Serial.println();
}

void entrar(int m) {
  modo = m;
  t0 = millis();
  switch (m) {
    case 0: Serial.println(F("[0] parado")); break;
    case 1: Serial.println(F("[1] passe a mao na frente de um sensor por vez")); break;
    case 2:
      Serial.println(F("[2] gire o trimpot do modulo ate ele detectar na distancia que"));
      Serial.println(F("    voce quer. O LED de bordo do modulo acende junto - se ele nao"));
      Serial.println(F("    acende, o problema e o modulo, nao a fiacao."));
      break;
    case 3: Serial.println(F("[3] aperte cada botoeira")); break;
    case 4: Serial.println(F("[4] sensores e botoeiras juntos")); break;
    default: Serial.println(F("modo desconhecido — ? para o menu")); modo = 0;
  }
}

void executar(String s) {
  s.trim();
  if (s == "?") { menu(); return; }
  if (s.length() == 1 && s[0] >= '0' && s[0] <= '4') { entrar(s[0] - '0'); return; }
  Serial.println(F("comando desconhecido — ? para o menu"));
}

void mudancasIR() {
  for (int i = 0; i < N_IR; i++) {
    bool d = detectado(i);
    if (d != irAntes[i]) {
      Serial.print(NOME_IR[i]);
      Serial.print(F(" (GPIO "));
      Serial.print(pinoIR(i));
      Serial.println(d ? F(")  DETECTOU") : F(")  livre"));
      irAntes[i] = d;
    }
  }
}

void mudancasBotoeiras() {
  for (int j = 0; j < 4; j++) {
    bool ap = (digitalRead(PIN_BOTOEIRA[j]) == LOW);   // pull-up interno
    if (ap != botAntes[j]) {
      Serial.print(NOME_BOT[j]);
      Serial.print(F(" (GPIO "));
      Serial.print(PIN_BOTOEIRA[j]);
      Serial.println(ap ? F(")  APERTADA") : F(")  solta"));
      botAntes[j] = ap;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  for (int i = 0; i < 4; i++) pinMode(PIN_APROX[i], INPUT);   // pull-up e do modulo
  for (int i = 0; i < 3; i++) pinMode(PIN_VAGA[i], INPUT);
  for (int j = 0; j < 4; j++) pinMode(PIN_BOTOEIRA[j], INPUT_PULLUP);
  menu();
  entrar(0);
}

void loop() {
  lerComando();
  unsigned long agora = millis();

  if ((modo == 1 || modo == 4) && agora - t0 >= 50) {
    t0 = agora;
    mudancasIR();
    if (modo == 4) mudancasBotoeiras();
  }
  if (modo == 3 && agora - t0 >= 50) { t0 = agora; mudancasBotoeiras(); }

  if (modo == 2 && agora - t0 >= 400) {
    t0 = agora;
    for (int i = 0; i < N_IR; i++) {
      Serial.print(detectado(i) ? F("[X] ") : F("[ ] "));
      Serial.print(NOME_IR[i]);
      Serial.print(F("   "));
    }
    Serial.println();
  }
}
