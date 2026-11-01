# Sketches de bancada

Um sketch por placa, **sem Wi-Fi e sem MQTT**, para montar o circuito por etapas e
provar cada uma antes de seguir. O roteiro que os usa está no
[PASSO_A_PASSO.md](../PASSO_A_PASSO.md).

| Pasta | Placa | O que exercita |
|---|---|---|
| `bancada_sinaleiras/` | 1A | 14 canais de foco um a um, com a contagem de LEDs esperada, e o buzzer |
| `bancada_sensores/`   | 1B | 7 módulos IR (com modo de ajuste de trimpot) e 4 botoeiras |
| `bancada_iluminacao/` | 2 | rampa de PWM, LDR com mín/máx para calibrar, presença |
| `bancada_ambiental/`  | 3 | varredura dos 7 LEDs, sensor de nível cru + %, DHT22, servo, buzzer |
| `bancada_seguranca/`  | 4 | LEDs, MQ-2 cru com mín/máx para calibrar, buzzer |

Grave pela Arduino IDE (**ESP32 Dev Module**), abra o Monitor Serial em **115200**
com final de linha **"Nova linha"** e digite `?`.

**Por que existem.** O firmware só funciona com a placa inteira — ele conecta no
Wi-Fi, fala com o broker e roda a máquina de estados. No meio da montagem isso
transforma toda falha num mistério de três suspeitos. Aqui não há rede nem lógica:
se falha, é o circuito.

Alguns testes são **impossíveis** com o firmware. A varredura da placa 1A acende um
canal por vez, em ordem conhecida, e anuncia **quantos LEDs devem acender** — dois
para os canais de veículo, que servem as duas cabeças da via, e um para os de
pedestre. É isso que revela uma cabeça não ligada ou dois GPIOs trocados. O firmware
nunca acende os focos assim: ele os acende por estado do semáforo.

Os modos de calibração mostram valor cru e extremos observados — são eles que
produzem os números que vão para o firmware: limiar do LDR, `NIVEL_SECO` e
`NIVEL_CHEIO`, limiares do MQ-2 e o ajuste do trimpot dos módulos IR.

**São gerados** por `ferramentas/gera_passo_a_passo.py`, a partir da pinagem dos
próprios `.ino`. Não edite à mão: um teste que aponta para o pino errado é pior que
teste nenhum. Não precisam de `secrets.h` e não contêm credencial alguma. Só o da
placa 3 usa uma biblioteca (**ESP32Servo**) e leva junto o `dht_simples.h` do firmware.
