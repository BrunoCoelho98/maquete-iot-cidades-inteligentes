#pragma once
/* ============================================================
   LEITURA DO DHT22 (AM2302) SEM BIBLIOTECA
   ------------------------------------------------------------
   POR QUE ISTO EXISTE
   A "DHT sensor library" (Adafruit) desliga as interrupcoes antes
   de ler e so as devolve quando o sensor responde. Com o sensor
   MUDO ela fica girando no laco de espera com tudo desligado por
   tempo suficiente para o watchdog de interrupcao derrubar o
   nucleo. Isso nao e teoria: na bancada, com um fio de alimentacao
   em mau contato (o modulo recebia ~1,6 V em vez de 3,3 V), esta
   placa entrou em ciclo de reboot -

     Guru Meditation Error: Core 1 panic'ed
     (Interrupt wdt timeout on CPU1)

   - e a placa da enchente reiniciava a cada poucos segundos por
   causa do termometro. Isso contraria o principio do projeto
   inteiro: sensor ruim degrada a INFORMACAO e nunca derruba o no.
   O nivel de agua, a comporta, a sirene e o MQTT nao tem nada a
   ver com o DHT e nao podem cair junto com ele.

   O QUE MUDA AQUI
   A janela critica e FIXA: 6 ms, com sensor bom ou sem sensor
   nenhum. Nao ha espera indefinida, entao nao ha como estourar
   watchdog. Sensor ausente custa 6 ms e devolve erro.

   O PROTOCOLO
   O mestre segura a linha em baixo por ~1 ms e solta; o sensor
   responde com 80 us baixo + 80 us alto e emite 40 bits, cada um
   como 50 us baixo seguido de um pulso alto CURTO (~26 us = 0) ou
   LONGO (~70 us = 1). Os 5 bytes sao:

     [0][1]  umidade em decimos de %      (613 = 61,3 %)
     [2][3]  temperatura em decimos de C  (bit 15 = sinal negativo)
     [4]     checksum = soma dos quatro anteriores

   UMA ARMADILHA QUE O CHECKSUM NAO PEGA
   Na bancada, o mesmo DHT22 lido com o protocolo do DHT11 fechou o
   checksum e devolveu -9,3 C e 20,6 % numa sala a 22,1 C. Os dois
   sensores somam os bytes do mesmo jeito, entao formato errado passa
   pelo checksum. Por isso existe o teste de faixa no fim: e ele, e
   nao o checksum, que separa leitura boa de leitura plausivel.
   ============================================================ */

struct LeituraDht {
  bool  ok;
  float temperatura;
  float umidade;
  const char* erro;      // por que falhou, para o serial
};

// Buffers estaticos: 1 kB nao cabe com folga na pilha do loopTask.
static const int DHT_MAX_BORDAS = 200;
static uint32_t dht_marca[DHT_MAX_BORDAS];
static uint8_t  dht_nivel[DHT_MAX_BORDAS];

inline LeituraDht lerDht22(int pino) {
  LeituraDht r = { false, 0.0f, 0.0f, "" };

  // Linha em repouso: tem de estar ALTA, segurada pelo pull-up do modulo.
  // INPUT puro de proposito - com INPUT_PULLUP o teste daria alto ate sem
  // sensor nenhum ligado, e nao provaria coisa alguma.
  pinMode(pino, INPUT);
  delayMicroseconds(50);
  if (digitalRead(pino) == LOW) {
    r.erro = "linha em baixo (sem alimentacao, sem pull-up ou fio solto)";
    return r;
  }

  // Sinal de partida: o DHT22 aceita de 0,8 a 20 ms
  pinMode(pino, OUTPUT);
  digitalWrite(pino, LOW);
  delayMicroseconds(1200);
  pinMode(pino, INPUT);

  int n = 0;
  noInterrupts();                  // 6 ms, e sempre 6 ms
  uint8_t ant = digitalRead(pino);
  uint32_t t0 = micros();
  while ((micros() - t0) < 6000 && n < DHT_MAX_BORDAS) {
    uint8_t v = digitalRead(pino);
    if (v != ant) { dht_marca[n] = micros(); dht_nivel[n] = v; n++; ant = v; }
  }
  interrupts();

  if (n == 0) { r.erro = "sensor mudo"; return r; }

  // duracao de cada pulso alto plausivel
  uint32_t alto[DHT_MAX_BORDAS];
  int na = 0;
  for (int i = 0; i + 1 < n; i++) {
    if (dht_nivel[i] != HIGH) continue;
    uint32_t d = dht_marca[i + 1] - dht_marca[i];
    if (d > 200) continue;         // handshake longo, ruido e o repouso final
    alto[na++] = d;
  }
  if (na < 40) { r.erro = "transmissao incompleta"; return r; }

  // Os 40 ULTIMOS sao os bits: contar de tras para a frente descarta o alto de
  // 80 us do handshake sem depender de qual transicao foi a primeira capturada
  // - e isso varia, conforme a linha ja estivesse baixa ou nao quando
  // comecamos a olhar.
  uint8_t dados[5] = { 0, 0, 0, 0, 0 };
  for (int k = na - 40, bit = 0; k < na; k++, bit++) {
    dados[bit / 8] <<= 1;
    if (alto[k] > 45) dados[bit / 8] |= 1;     // ~26 us = 0, ~70 us = 1
  }

  uint8_t soma = dados[0] + dados[1] + dados[2] + dados[3];
  if (soma != dados[4]) { r.erro = "checksum"; return r; }

  r.umidade = (((uint16_t)dados[0] << 8) | dados[1]) * 0.1f;
  r.temperatura = ((((uint16_t)dados[2] & 0x7F) << 8) | dados[3]) * 0.1f;
  if (dados[2] & 0x80) r.temperatura = -r.temperatura;

  // Faixa do datasheet. E a defesa contra leitura plausivel e errada que o
  // checksum deixa passar (ver o cabecalho).
  if (r.umidade < 0.0f || r.umidade > 100.0f ||
      r.temperatura < -40.0f || r.temperatura > 80.0f) {
    r.erro = "valores fora da faixa do DHT22";
    return r;
  }

  r.ok = true;
  return r;
}
