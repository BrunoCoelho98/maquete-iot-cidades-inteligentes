# Atividade de aula: um ponto de ônibus inteligente

Nesta atividade cada estudante (ou dupla) constrói um aparelho novo para a cidade: um
**ponto de ônibus inteligente**. Um botão avisa a central que há passageiro esperando, e a
central acende uma luz no ponto para avisar que o ônibus está vindo. Para isso o estudante
monta o circuito, grava o programa no ESP32, cria no Node-RED os fluxos que publicam e
assinam os tópicos do ponto e o vê aparecer no painel da cidade, **sem alterar nenhuma das
cinco placas que já estão funcionando**.

A atividade foi pensada para turmas introdutórias e não exige conhecimento prévio de
eletrônica ou programação. Conceitos trabalhados: sensor, atuador, microcontrolador,
rede sem fio, broker, tópico, publicar e assinar, comando remoto e estado *online*/*offline*.

## Arquivos

| Arquivo | Para quê |
|---|---|
| [`documentos/Roteiro_do_Estudante.docx`](documentos/Roteiro_do_Estudante.docx) | o roteiro que o estudante segue, em sete partes |
| [`documentos/Questionario_antes.docx`](documentos/Questionario_antes.docx) | questionário aplicado antes da atividade (partes A a C) |
| [`documentos/Questionario_depois.docx`](documentos/Questionario_depois.docx) | o mesmo questionário, depois da atividade, com a percepção da atividade (parte D) |
| [`ponto_onibus/ponto_onibus.ino`](ponto_onibus/ponto_onibus.ino) | firmware do ponto; o estudante só altera a área marcada no topo |
| [`fluxo_ponto_onibus.json`](fluxo_ponto_onibus.json) | gabarito do Node-RED: a aba do ponto pronta |
| [`figuras/`](figuras/) | circuito do ponto e o caminho da mensagem (em branco e preenchido) |
| [`ferramentas/duble_ponto.py`](ferramentas/duble_ponto.py) | "dublê" que faz na rede o que a placa do ponto faz, para ensaiar sem hardware |

## Material por estudante ou dupla

Uma ESP32 com cabo USB, uma protoboard, um botão, um LED, um resistor de 220 Ω e jumpers.
Leve reservas: um LED, um resistor, um botão e alguns jumpers.

## Preparação

**Antes da aula**

- Faça a atividade inteira sozinho, seguindo o roteiro, com o cronômetro ligado. Para a
  parte do Node-RED sem hardware, use o dublê:
  `python ferramentas/duble_ponto.py --host <ip-do-broker>` (precisa de `pip install paho-mqtt`).
- Em `ponto_onibus/`, copie `secrets.example.h` para `secrets.h`, preencha a rede e o IP do
  broker e grave o programa uma vez numa ESP32, para conferir que ela conecta e aparece no
  Estado da cidade.
- Teste cada peça: um LED invertido ou um jumper rompido custam dez minutos no dia.
- Imprima o roteiro e os dois questionários.

**No dia, antes de começar**

- Broker rodando com o arquivo de configuração (veja o [README](../README.md#broker)).
- Node-RED com o `flows.json` da maquete e **sem** nenhuma aba do ponto: cada estudante
  cria a sua.
- Limpe o estado retido de um ensaio anterior, senão o ponto aparece no painel antes de
  existir: `mosquitto_pub -h <ip-do-broker> -t cidade/ponto/status -r -n`.
- As cinco placas da maquete ligadas e aparecendo como *ok* no Estado da cidade.
- Arduino IDE aberta com `ponto_onibus.ino`, placa **ESP32 Dev Module** e porta escolhidas.

## Cronograma

| Início | Etapa | Duração |
|---|---|---|
| 0:00 | Apresentação da atividade | 10 min |
| 0:10 | Questionário antes | 15 min |
| 0:25 | Parte 1: conhecendo a cidade | 10 min |
| 0:35 | Parte 2: as peças | 10 min |
| 0:45 | Parte 3: montando o circuito | 25 min |
| 1:10 | Intervalo | 10 min |
| 1:20 | Parte 4: o programa | 15 min |
| 1:35 | Parte 5: ligando à central (Node-RED) | 35 min |
| 2:10 | Parte 6: a missão completa | 10 min |
| 2:20 | Parte 7: o que aconteceu por trás | 10 min |
| 2:30 | Recolher o roteiro e aplicar o questionário depois | 20 min |

São cerca de duas horas de atividade e três no total. Em aulas mais curtas, divida em dois
encontros: o primeiro vai até o fim da Parte 3 (o circuito montado fica guardado) e o
segundo começa na Parte 4.

## Como conduzir

A ajuda vai sempre do nível mais baixo para o mais alto:

| Nível | Ajuda | Exemplo |
|---|---|---|
| 0 | nenhuma | o estudante faz sozinho |
| 1 | incentivo ou pergunta | "O que o passo 5 está pedindo?" · "Confere a imagem." |
| 2 | dica específica | "Olha a coluna do LED." · "O tópico tem uma letra diferente." |
| 3 | demonstração | o professor mostra ou faz junto |

- Não toque nas peças nem no mouse. Se precisar mostrar, aponte.
- Enquanto o estudante estiver tentando algo com sentido, espere até uns dois minutos ou
  até ele pedir ajuda. Tentar e errar faz parte da aprendizagem.
- Não explique além do roteiro. Perguntas de conceito recebem resposta curta, e vale anotar
  quais foram: elas mostram onde o roteiro precisa melhorar.
- **Não corrija nem comente o questionário antes** até o fim da atividade. Assim, o que
  mudar no questionário depois reflete o que foi aprendido, e não o gabarito.

### Parte a parte

| Parte | Conceitos | Problemas mais comuns |
|---|---|---|
| 1 · Conhecendo a cidade | sensor, atuador, microcontrolador, IoT | achar que o LED é sensor ("ele sabe que escureceu"); a Parte 7 retoma |
| 2 · As peças | papel de cada peça; polaridade do LED | achar que o resistor tem lado (não tem) |
| 3 · Montando o circuito | circuito como caminho da corrente | contar a coluna errada; LED invertido; fio na trilha vermelha; botão fora do canal |
| 4 · O programa | programa, gravação, comunicação serial | apagar aspas ou ponto e vírgula; gravação parada em `Connecting...` (segurar BOOT) |
| 5 · Ligando à central | broker, tópico, publicar, assinar, dois sentidos | erro de digitação no tópico; esquecer o *Deploy*; criar outro broker no "+" do *Server*; *inject* com tipo data/hora |
| 6 · A missão completa | painel, comando remoto, *online*/*offline* | impaciência com o OFFLINE: com o cabo tirado, o broker leva cerca de 30 s para perceber |
| 7 · O que aconteceu por trás | a cadeia inteira | trocar broker e central; ao final, mostre `figuras/caminho_mensagem.png` |

Respostas da Parte 7 do roteiro: publica · Wi-Fi, broker · assina · tópico · comando.

### Quando algo não funciona

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| Gravação para em `Connecting......` | a placa não entrou em modo de gravação | segurar BOOT até aparecerem porcentagens |
| Monitor Serial só imprime pontinhos depois de "Conectando ao Wi-Fi" | rede errada, de 5 GHz ou senha errada | conferir o `secrets.h` e regravar |
| "não deu (código -2)" ao conectar à central | broker fora do ar, IP errado ou firewall | conferir o broker e o `MQTT_HOST` |
| Aperta o botão e o Monitor Serial não mostra nada | fio no furo errado; botão fora do canal | conferir os passos 6 a 8 da Parte 3: sinal em C14, terra em J16 |
| Monitor mostra "Botão apertado" mas a depuração do Node-RED não | tópico digitado diferente; faltou o *Deploy* | comparar letra por letra com `cidade/ponto/estado` |
| O comando chega, mas a luz não pisca | LED invertido ou fora da coluna | perna comprida em I7, curta em I8; resistor em H5–H7 |
| Monitor mostra "Não conheço esse comando" | texto do comando digitado errado | os comandos são `VINDO` e `CHEGOU` |
| O grupo do ponto não aparece no painel | grupo sem aba, ou faltou o *Deploy* | abrir o nó de texto, conferir o grupo, fazer *Deploy* e recarregar o painel |

**Plano B.** Se um problema de hardware travar a atividade, o dublê
(`python ferramentas/duble_ponto.py --host <ip-do-broker>`) faz na rede tudo o que a placa
faz: Enter aperta o botão e `k` simula tirar o cabo. A Parte 5, onde estão publicar, assinar
e tópico, acontece do mesmo jeito. Se o tempo acabar na Parte 5, importe o gabarito
`fluxo_ponto_onibus.json` (menu ☰ → *Import*), depois de apagar a aba que o estudante tiver
começado.

**Depois da aula**, se o ponto não for ficar na maquete, apague a aba dele no Node-RED e
limpe o estado retido (comando da preparação). Sem isso, o Estado da cidade mostra o ponto
OFFLINE para sempre.

## Questionário e correção

O mesmo questionário é aplicado antes e depois. A versão do depois acrescenta a parte D,
sobre a percepção da atividade. A pontuação vai de 0 a 21 e segue critérios fechados, para
que qualquer professor chegue à mesma nota.

**Parte A — múltipla escolha (1 ponto cada).** "Não sei" vale zero, como o erro.

| Item | Conceito | Resposta |
|---|---|---|
| 1 | O que é Internet das Coisas | c |
| 2 | Sensor | d |
| 3 | Atuador | a |
| 4 | Microcontrolador | b |
| 5 | Programa (firmware) | d |
| 6 | Comunicação sem fio | a |
| 7 | Broker MQTT | c |
| 8 | Publicar e assinar | b |
| 9 | Tópico | d |
| 10 | Comunicação nos dois sentidos | c |
| 11 | Estado *online*/*offline* | a |
| 12 | Modularidade | b |

**Questão 13 — ordenar o caminho do aviso (até 2 pontos).** Ordem correta: (1) o passageiro
aperta o botão; (2) o microcontrolador percebe o botão e publica uma mensagem pelo Wi-Fi;
(3) o broker recebe a mensagem e a entrega a quem assinou o tópico; (4) o programa da
central recebe a mensagem; (5) a tela da central mostra "passageiro aguardando".
Pontos = número de passos na posição certa × 0,4.

**Questão 14 — lixeira inteligente (1 ponto por campo).**

| Campo | Aceitar |
|---|---|
| a) sensor | algo que percebe a lixeira cheia (nível, peso, distância, presença) |
| b) atuador | a luz da lixeira |
| c) nome do tópico | qualquer nome plausível e hierárquico que identifique a lixeira e o assunto (ex.: `cidade/lixeira/estado`); não vale um nome sem relação |
| d) mensagem da central | um comando curto da central para a lixeira (ex.: `CAMINHAO_VINDO`, `COLETA`); não vale uma mensagem no sentido contrário (`CHEIA`) |

**Questão 15 — explicação do ponto de ônibus (rubrica de 0 a 3 pontos).**

| Pontos | Critério |
|---|---|
| 0 | não responde, ou a explicação não tem relação com a pergunta |
| 1 | diz que a informação "vai para algum lugar" e "volta", sem nomear as partes do caminho |
| 2 | nomeia pelo menos três elementos da cadeia (botão/sensor, microcontrolador, rede/Wi-Fi, broker, central/painel, comando, luz/atuador) em ordem coerente, mas só em um sentido ou com uma ligação trocada |
| 3 | descreve a ida (o ponto publica e a central recebe) e a volta (a central manda um comando que o ponto recebe e executa), com o broker no meio; a decisão de mandar o comando pode ser de uma pessoa ou automática |

Sempre que possível, peça a um segundo professor que corrija a questão 15 sem ver a sua
nota, e compare as duas.

**Parte C — autoconfiança (1 a 5)** e **parte D — percepção da atividade (1 a 5)** não
entram na nota. No item D2 ("Em algum momento não soube o que fazer em seguida"),
concordar é ruim: para comparar com os demais, use 6 − resposta.

### Como ler os resultados

- **Ganho normalizado:** g = (depois − antes) ÷ (21 − antes), a fração do que faltava
  aprender que foi aprendida. Referência usual (Hake, 1998): abaixo de 0,3, baixo; de 0,3 a
  0,7, médio; acima de 0,7, alto.
- **Item a item:** marque cada item da parte A como acerto, erro ou "não sei" antes e
  depois. "Não sei" → acerto e erro → acerto são aprendizagem; acerto → erro indica uma
  confusão criada na atividade e merece atenção.
- **Confiança × acerto:** confiança alta com resposta errada também é um achado, e aponta o
  que reforçar no roteiro.

HAKE, R. R. Interactive-engagement versus traditional methods: a six-thousand-student
survey of mechanics test data for introductory physics courses. **American Journal of
Physics**, v. 66, n. 1, p. 64-74, 1998.
