// GERADO por ferramentas/gera_passo_a_passo.py — nao edite a mao.
//
// Sketch de bancada da placa 1A — sinaleiras. Sem WiFi e sem MQTT de proposito: aqui
// se testa o CIRCUITO, e uma falha nao pode ter tres explicacoes possiveis.
// A pinagem vem do firmware (node_sinaleiras.ino) — nao ha o que conferir a mao.
//
// Monitor Serial em 115200, com final de linha "Nova linha".
// Digite ? para ver o menu.

const int PIN_CARRO[2][3] = { { 23, 22, 21 }, { 19, 18, 17 } };
const int PIN_PED[4][2]   = { { 16, 4 }, { 5, 13 }, { 14, 27 }, { 26, 25 } };
const int PIN_BUZZER      = 33;

// Polaridade copiada do firmware: os focos de veiculo sao MODULOS de mini
// sinaleira e o pino comum do modulo decide se acender e HIGH ou LOW.
// Nao sabe qual e o seu? Comando 4 descobre em 3 segundos, sem multimetro.
const int ACESO   = HIGH;
const int APAGADO = LOW;

// 14 canais: 6 de veiculo (cada um serve DUAS cabecas) e 8 de pedestre.
const int N_CANAIS = 14;
const char* NOME[N_CANAIS]  = { "via A (N+S) vermelho", "via A (N+S) amarelo", "via A (N+S) verde", "via B (L+O) vermelho", "via B (L+O) amarelo", "via B (L+O) verde", "pedestre N vermelho", "pedestre N verde", "pedestre S vermelho", "pedestre S verde", "pedestre L vermelho", "pedestre L verde", "pedestre O vermelho", "pedestre O verde" };
// quantos focos tem de acender em cada canal - e o que prova o paralelo
const int QUANTOS[N_CANAIS] = { 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1 };

int pinoDoCanal(int c) {
  if (c < 6) return PIN_CARRO[c / 3][c % 3];
  return PIN_PED[(c - 6) / 2][(c - 6) % 2];
}

void foco(int pino, bool ligado) {
  digitalWrite(pino, ligado ? ACESO : APAGADO);
}

void apagarTudo() {
  for (int c = 0; c < N_CANAIS; c++) foco(pinoDoCanal(c), false);
}

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

int canalFixo = 0, varre = 0;
bool polaridade = false;      // modo 4: nivel atual no pino sob teste

void menu() {
  Serial.println();
  Serial.println(F("== bancada — placa 1A, sinaleiras =="));
  Serial.println(F("  0   apaga tudo"));
  Serial.println(F("  1   acende so o canal fixo (padrao 0)"));
  Serial.println(F("  2   varredura: um canal por vez, com o nome e quantos LEDs esperar"));
  Serial.println(F("  3   acende os 14 canais ao mesmo tempo"));
  Serial.println(F("  4   polaridade: alterna HIGH e LOW no canal fixo"));
  Serial.println(F("  5   buzzer da travessia: tres bips"));
  Serial.println(F("  cN  fixa o canal N (0..13) e entra no modo 1"));
  Serial.println();
  Serial.println(F("  canais 0-5  = veiculo, DOIS focos cada (as duas cabecas da via)"));
  Serial.println(F("  canais 6-13 = pedestre, UM foco cada"));
  Serial.println();
}

void anuncia(int c) {
  Serial.print(F("canal "));
  if (c < 10) Serial.print(' ');
  Serial.print(c);
  Serial.print(F("  GPIO "));
  Serial.print(pinoDoCanal(c));
  Serial.print(F("  ->  "));
  Serial.print(NOME[c]);
  Serial.print(F("   ("));
  Serial.print(QUANTOS[c]);
  Serial.println(QUANTOS[c] == 2 ? F(" focos)") : F(" foco)"));
}

void entrar(int m) {
  modo = m;
  t0 = millis();
  varre = 0;
  apagarTudo();
  switch (m) {
    case 0: Serial.println(F("[0] tudo apagado")); break;
    case 1:
      foco(pinoDoCanal(canalFixo), true);
      Serial.print(F("[1] "));
      anuncia(canalFixo);
      Serial.println(F("    esses focos ficam acesos, parados, ate voce mudar de modo"));
      break;
    case 2:
      Serial.println(F("[2] varredura — confira o nome E a contagem de focos"));
      Serial.println(F("    canal de veiculo que acende UM foco so tem uma cabeca ligada"));
      break;
    case 3:
      for (int c = 0; c < N_CANAIS; c++) foco(pinoDoCanal(c), true);
      Serial.println(F("[3] 20 focos acesos — procure o que ficou apagado ou fraco"));
      break;
    case 4:
      Serial.print(F("[4] polaridade do canal "));
      Serial.println(canalFixo);
      Serial.println(F("    o pino vai alternar entre HIGH e LOW a cada 1,5 s."));
      Serial.println(F("    acendeu no HIGH  -> comum no GND  -> ACESO = HIGH"));
      Serial.println(F("    acendeu no LOW   -> comum no 3V3  -> ACESO = LOW"));
      Serial.print(F("    o firmware esta configurado para ACESO = "));
      Serial.println(ACESO == HIGH ? F("HIGH") : F("LOW"));
      break;
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
  if (s.length() > 1 && (s[0] == 'c' || s[0] == 'C')) {
    int c = s.substring(1).toInt();
    if (c < 0 || c >= N_CANAIS) { Serial.println(F("canal fora de 0..13")); return; }
    canalFixo = c;
    entrar(1);
    return;
  }
  if (s.length() == 1 && s[0] >= '0' && s[0] <= '5') { entrar(s[0] - '0'); return; }
  Serial.println(F("comando desconhecido — ? para o menu"));
}

void setup() {
  Serial.begin(115200);
  delay(300);
  for (int c = 0; c < N_CANAIS; c++) pinMode(pinoDoCanal(c), OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  apagarTudo();
  menu();
  entrar(0);
}

void loop() {
  lerComando();
  unsigned long agora = millis();
  if (modo == 2 && agora - t0 >= 900) {
    t0 = agora;
    apagarTudo();
    foco(pinoDoCanal(varre), true);
    anuncia(varre);
    varre = (varre + 1) % N_CANAIS;
  }
  if (modo == 4 && agora - t0 >= 1500) {
    t0 = agora;
    polaridade = !polaridade;
    digitalWrite(pinoDoCanal(canalFixo), polaridade ? HIGH : LOW);
    Serial.println(polaridade ? F("    agora HIGH") : F("    agora LOW"));
  }
}
