# Material por parte

Lista separada **por placa**, na ordem em que dá para montar e testar cada uma
isoladamente. Sai do mesmo mapa que gera os desenhos de
[layout/](layout/) — lista e desenho não têm como divergir.

> Premissas desta montagem: protoboard de **400 pontos** (30 colunas), **ESP32 fora**
> da protoboard, e os módulos de sensor ligados por **jumper macho–fêmea** direto
> nos pinos, sem passar pela protoboard.

| Parte | Desenho |
|---|---|
| 1A — Sinaleiras, veículos | [layout/placa1A_1_veiculos.png](layout/placa1A_1_veiculos.png) |
| 1A — Sinaleiras, pedestres | [layout/placa1A_2_pedestres.png](layout/placa1A_2_pedestres.png) |
| 1B — Sensores | [layout/placa1B_sensores.png](layout/placa1B_sensores.png) |
| 2 — Iluminação | [layout/placa2_iluminacao.png](layout/placa2_iluminacao.png) |
| 3 — Ambiental | [layout/placa3_ambiental.png](layout/placa3_ambiental.png) |
| 4 — Segurança | [layout/placa4_seguranca.png](layout/placa4_seguranca.png) |

---

## 1A · protoboard 1 (veículos)

| Item | Qtd | Observação |
|---|---|---|
| Protoboard 400 pontos | **1** |  |
| Módulo mini sinaleira (3 cores) | **4** | uma cabeça cada; 2 cabeças por via no mesmo canal |
| Jumper macho–fêmea | **16** | 3 sinais + comum por módulo; fêmea no pino do módulo |
| Jumper macho–macho | **8** | trilhas e terra da protoboard |

## 1A · protoboard 2 (pedestres)

| Item | Qtd | Observação |
|---|---|---|
| Protoboard 400 pontos | **1** |  |
| LED 5 mm vermelho | **4** | um por travessia |
| LED 5 mm verde | **4** | um por travessia |
| Resistor 220 Ω | **8** | um por LED |
| Buzzer passivo (piezo) | **1** | travessia sonora · acessibilidade |
| Jumper macho–macho | **16** |  |

## 1A · comum

| Item | Qtd | Observação |
|---|---|---|
| ESP32 DevKit (WROOM-32) | **1** | fora da protoboard; as duas protoboards são da MESMA ESP32 |
| Cabo USB | **1** |  |

## 1B · sensores

| Item | Qtd | Observação |
|---|---|---|
| ESP32 DevKit (WROOM-32) | **1** | fora da protoboard |
| Cabo USB | **1** |  |
| Protoboard 400 pontos | **1** | botoeiras e distribuição |
| Módulo óptico reflexivo TCRT5000 | **7** | 4 aproximação + 3 vaga; alimentar em 3,3 V, embutidos no piso |
| Botão de pressão 6 mm | **4** | pull-up interno, sem resistor |
| Jumper macho–fêmea | **21** | 3 por módulo (o AO não é ligado) |
| Jumper macho–macho | **12** | botoeiras e trilhas |

## 2 · iluminação

| Item | Qtd | Observação |
|---|---|---|
| ESP32 DevKit (WROOM-32) | **1** | fora da protoboard |
| Cabo USB | **1** |  |
| Protoboard 400 pontos | **1** |  |
| LDR 5 mm | **1** | com resistor de 10 kΩ para o terra |
| Resistor 10 kΩ | **1** | parte de baixo do divisor |
| LED 5 mm branco | **1** | poste |
| Resistor 100 Ω | **1** | LED branco tem queda alta; dois de 220 Ω em paralelo também servem |
| Módulo óptico reflexivo TCRT5000 | **1** | o oitavo; alimentar em 3,3 V, embutido no piso ao lado do poste |
| Jumper macho–fêmea | **3** | TCRT5000 (o AO não é ligado) |
| Jumper macho–macho | **10** |  |

## 3 · ambiental

| Item | Qtd | Observação |
|---|---|---|
| ESP32 DevKit (WROOM-32) | **1** | fora da protoboard |
| Cabo USB | **1** |  |
| Protoboard 400 pontos | **1** |  |
| LED 5 mm verde | **2** | nível + temperatura |
| LED 5 mm amarelo | **2** | nível + temperatura |
| LED 5 mm laranja | **1** | régua de nível |
| LED 5 mm vermelho | **2** | nível + temperatura |
| Resistor 220 Ω | **7** | um por LED |
| Sensor de nível de água analógico | **1** | tipo pente, em 3,3 V |
| Módulo DHT22 (AM2302) | **1** | versão de 3 pinos, com pull-up embutido |
| Micro servo SG90 | **1** | comporta; alimentar pelo VIN |
| Buzzer passivo (piezo) | **1** | alerta de enchente |
| Jumper macho–fêmea | **9** | 3 por módulo |
| Jumper macho–macho | **14** |  |

## 4 · segurança

| Item | Qtd | Observação |
|---|---|---|
| ESP32 DevKit (WROOM-32) | **1** | fora da protoboard |
| Cabo USB | **1** |  |
| Protoboard 400 pontos | **1** |  |
| LED 5 mm verde | **1** |  |
| LED 5 mm amarelo | **1** |  |
| LED 5 mm vermelho | **1** |  |
| Resistor 220 Ω | **3** | um por LED |
| Resistor 1 kΩ | **1** | divisor do AO — parte de cima |
| Resistor 2 kΩ | **1** | divisor do AO — parte de baixo; dois de 1 kΩ em série também servem |
| Módulo MQ-2 (com saída AO) | **1** | alimentar em 5 V (VIN) |
| Buzzer passivo (piezo) | **1** | sirene |
| Jumper macho–fêmea | **4** | MQ-2 |
| Jumper macho–macho | **12** |  |

---

## Total consolidado

Soma de todas as partes, com a folga que vale comprar junto.

| Item | Em uso | Comprar |
|---|---|---|
| Botão de pressão 6 mm | 4 | **6** |
| Buzzer passivo (piezo) | 3 | **4** |
| Cabo USB | 5 | **5** |
| ESP32 DevKit (WROOM-32) | 5 | **6** |
| Jumper macho–fêmea | 53 | **73** |
| Jumper macho–macho | 72 | **102** |
| LDR 5 mm | 1 | **1** |
| LED 5 mm amarelo | 3 | **7** |
| LED 5 mm branco | 1 | **4** |
| LED 5 mm laranja | 1 | **4** |
| LED 5 mm verde | 7 | **11** |
| LED 5 mm vermelho | 7 | **11** |
| Micro servo SG90 | 1 | **1** |
| Módulo DHT22 (AM2302) | 1 | **2** |
| Módulo MQ-2 (com saída AO) | 1 | **1** |
| Módulo mini sinaleira (3 cores) | 4 | **5** |
| Módulo óptico reflexivo TCRT5000 | 8 | **10** |
| Protoboard 400 pontos | 6 | **6** |
| Resistor 1 kΩ | 1 | **10** |
| Resistor 10 kΩ | 1 | **10** |
| Resistor 100 Ω | 1 | **10** |
| Resistor 2 kΩ | 1 | **10** |
| Resistor 220 Ω | 18 | **38** |
| Sensor de nível de água analógico | 1 | **2** |

> A folga não é exagero: LED e jumper são o que mais se perde e mais queima em
> montagem de protoboard, e uma ESP32 morta trava uma placa inteira.


