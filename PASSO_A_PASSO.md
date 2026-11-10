# Passo a passo de montagem

Roteiro para montar as **cinco placas** por etapas, provando cada uma antes de
seguir. Descobrir um erro com 3 fios no lugar é trivial; descobrir com 60 é uma
noite perdida.

> **Gerado a partir do firmware**, junto com os sketches de bancada: os pinos citados
> aqui são os mesmos do código.

| Documento | Responde |
|---|---|
| **este** | *em que ordem montar e como provar cada etapa* |
| [MATERIAIS.md](MATERIAIS.md) | o material de cada parte, e o **desenho** de como fica |
| [esquematicos/](esquematicos/README.md) | o que liga em quê, eletricamente |
| [README.md](README.md) | pinagem completa, contrato MQTT, broker e Node-RED |

**Premissas desta montagem:** protoboard de **400 pontos** (30 colunas), **ESP32
fora** da protoboard, e módulos de sensor ligados por **jumper macho–fêmea** direto
nos pinos do devkit.

---

## Antes de ligar a primeira coisa

Cinco regras. As três primeiras são as que custam dinheiro quando ignoradas.

1. **Desligue o USB antes de mexer na protoboard.** Sempre. Enfiar um jumper com a
   placa energizada é como a maioria dos ESP32 morre — um encosto de 0,2 s entre
   3,3 V e um GPIO basta. Montar → conferir → ligar → testar → **desligar** → seguir.
2. **Nunca 5 V num GPIO.** O ESP32 é 3,3 V e não tolera 5 V. Nesta maquete só três
   coisas usam 5 V: o servo e o MQ-2 — e as duas entregam sinal seguro (o servo
   sai em 3,3 V, o servo só recebe, o MQ-2 passa por divisor).
3. **Módulo IR alimentado em 3,3 V, nunca em 5 V.** A saída dele acompanha a
   alimentação: em 5 V, ela leva 5 V ao GPIO. São sete módulos — sete chances.
4. **Um bloco por vez.** A tentação de montar tudo e ligar no fim é forte e é sempre
   um erro: com 20 LEDs montados de uma vez, um que não acende tem 20 suspeitos.
5. **Terra comum.** Tudo que entra na placa compartilha o GND do ESP32. Sensor
   alimentado por fora sem terra comum lê lixo — e lê lixo de forma intermitente,
   que é o pior tipo de defeito para caçar.

**Na bancada:** multímetro (o item que mais economiza tempo aqui), alicate de corte,
jumpers macho–macho e macho–fêmea, e a lista da [MATERIAIS.md](MATERIAIS.md)
separada por parte.

---

## Os sketches de bancada

O firmware de verdade só funciona com a placa **inteira**: ele conecta no Wi-Fi, fala
com o broker e roda a máquina de estados. Ótimo no fim, inútil no meio da montagem —
com metade dos LEDs no lugar, um problema pode ser o circuito, a rede ou o código, e
você não sabe qual.

Por isso existe um sketch de bancada por placa, em [`bancada/`](bancada/):
**sem Wi-Fi e sem MQTT**, com um menu no monitor serial que exercita uma parte de
cada vez. Se uma etapa falha com ele, o problema é o circuito. Ponto.

| Placa | Sketch |
|---|---|
| 1A — sinaleiras | `bancada/bancada_sinaleiras/` |
| 1B — sensores | `bancada/bancada_sensores/` |
| 2 — iluminação | `bancada/bancada_iluminacao/` |
| 3 — ambiental  | `bancada/bancada_ambiental/`  |
| 4 — segurança  | `bancada/bancada_seguranca/`  |

Grave pela Arduino IDE (placa **ESP32 Dev Module**), abra o **Monitor Serial em
115200** com final de linha **"Nova linha"**, e digite `?` para ver o menu. Os
números deste roteiro ("comando `2`") são as opções desse menu.

Não precisam de `secrets.h` e não têm credencial nenhuma dentro. O da placa 3 usa a
biblioteca **ESP32Servo**; os outros quatro, nenhuma. A leitura do DHT22 não usa
biblioteca: vai em `dht_simples.h`, copiado do firmware pelo gerador.

---

## Placa 1A — Sinaleiras

Vinte focos em **duas protoboards de 400 pontos**: veículos numa, pedestres na outra.
A placa não tem sensor nenhum — ela só aciona foco e roda a máquina de fases.

As quatro cabeças de veículo são **módulos de mini sinaleira**, não LEDs soltos: cada
módulo traz os três LEDs em um corpo só, com três entradas de cor e um **pino comum**.
O módulo usado na maquete já traz um resistor SMD sob cada LED, então as entradas de cor
vão direto ao GPIO; ele fica **fora** da protoboard, ligado por quatro jumpers. Se o seu
módulo não tiver resistor próprio, ponha 220 Ω em cada entrada de cor.

A particularidade a entender antes de montar: **as duas cabeças de uma via
compartilham o mesmo pino**, porque mostram o mesmo aspecto em todas as fases. São
6 canais de veículo servindo 4 cabeças, e 8 canais de pedestre servindo 4 travessias.

> **O pino comum do módulo decide o sinal.** Comum no **GND** (cátodo comum) e o foco
> acende com `HIGH`; comum no **3,3 V** (ânodo comum) e acende com `LOW`. O firmware
> nasce em `ACESO = HIGH`; se o seu módulo for de ânodo comum, troque **essa linha** em
> `node_sinaleiras.ino` e nada mais — a etapa 2 descobre qual é o seu caso.

### Etapa 1 — Alimentação e sinal de vida

**Monte:** o ESP32 **fora da protoboard**, ligado só pelo cabo USB. Da protoboard, por enquanto, só o terra: `GND` do devkit para a trilha **−**, e as duas trilhas **−** unidas na outra ponta.

**Confira antes de ligar:** continuidade entre o pino `GND` do devkit e a trilha **−** dos dois lados — encoste as pontas do multímetro no modo de continuidade. **A trilha + fica sem uso nesta placa**: nada aqui consome 3,3 V, porque os LEDs são acionados pelo GPIO e vão ao terra. Medir entre as duas trilhas daria 0 V, e está certo.

**Prove:** grave `bancada_sinaleiras` e abra o serial em 115200. O menu tem que aparecer; digite `?` se não aparecer.

**Se não passar:** sem porta COM na IDE = falta o driver USB do devkit (CP2102 ou CH340). Menu com caracteres embaralhados = baud diferente de 115200.

> Posições exatas de cada peça: [layout/placa1A_1_veiculos.png](layout/placa1A_1_veiculos.png).

### Etapa 2 — Um módulo só, e a polaridade dele

**Monte:** **um** módulo de mini sinaleira na cabeça Norte: pino **comum** na trilha do terra e a entrada **vermelha** no GPIO **23** (canal 0, via A). Se o seu módulo não tiver resistor próprio, ponha 220 Ω no caminho.

**Confira antes de ligar:** o módulo fica fora da protoboard; o que entra na placa são os jumpers. Nenhuma entrada de cor fica sem resistor: o do próprio módulo ou um externo de 220 Ω.

**Prove:** comando `c0` e, em seguida, comando `4`. O pino alterna HIGH e LOW a cada 1,5 s e o serial diz qual nível está aplicado. **Anote em qual deles o vermelho acendeu**: `HIGH` = cátodo comum, `LOW` = ânodo comum.

**Se não passar:** não acendeu em nenhum dos dois níveis → comum fora do terra (ou do 3,3 V), resistor sem contato, ou a entrada no GPIO errado. Com **um** módulo montado são três suspeitos; com quatro, seriam doze.

> Se acendeu no `LOW`, o seu módulo é de ânodo comum: troque `ACESO` para `LOW` em `node_sinaleiras.ino`, grave o sketch de bancada de novo e repita esta etapa. Fazer isso agora evita descobrir no fim que a cidade inteira está com as cores invertidas.

### Etapa 3 — A segunda cabeça do mesmo canal

**Monte:** o segundo módulo — a cabeça Sul — com a entrada vermelha no mesmo GPIO **23** (e o seu próprio resistor de 220 Ω, se o módulo não tiver resistor embutido).

**Confira antes de ligar:** que são **dois resistores**, um por entrada. Não divida um resistor entre os dois módulos.

**Prove:** comando `c0` de novo. Agora acendem **dois** vermelhos, com o mesmo brilho.

**Se não passar:** um bem mais fraco que o outro = os dois estão dividindo um resistor só. Dois LEDs em paralelo nunca repartem corrente por igual: o de menor queda rouba a maior parte e o outro fica fraco ou apagado.

> Esta etapa é o teste da decisão de projeto inteira. Se os dois acendem iguais, o resto dos focos de veículo é repetição.

### Etapa 4 — As outras entradas de cor dos módulos de veículo

**Monte:** amarelo e verde da via A, e as três cores das duas cabeças da via B, cada entrada com o seu resistor. Ordem das colunas no desenho: [layout/placa1A_1_veiculos.png](layout/placa1A_1_veiculos.png).

**Prove:** comando `2` (varredura). Cada canal acende sozinho por 0,9 s e o serial escreve o nome **e quantos focos esperar**: `canal  4  GPIO 18  ->  via B (L+O) amarelo   (2 focos)`. Confira as duas coisas.

**Se não passar:** acendeu **um** foco num canal que anuncia dois = falta a segunda cabeça, ou o fio dela caiu na coluna vizinha. Acendeu a cor errada = duas entradas trocadas entre si, e o serial diz qual deveria ser. Verde ou amarelo visivelmente mais fracos que o vermelho com o mesmo resistor indicam que o módulo já traz resistor próprio: ver a nota no fim desta placa.

> Esta varredura é o teste que o firmware **não** consegue fazer: ele acende os focos por estado do semáforo, nunca um canal por vez em ordem conhecida.

### Etapa 5 — Os 8 LEDs de pedestre, na segunda protoboard

**Monte:** vermelho e verde de cada travessia, um resistor por LED. Desenho: [layout/placa1A_2_pedestres.png](layout/placa1A_2_pedestres.png).

**Confira antes de ligar:** as duas protoboards compartilham o **mesmo** ESP32 e o **mesmo** terra: una as trilhas − das duas.

**Prove:** comando `2` outra vez. Os canais 6 a 13 anunciam **1 LED** cada — se acender dois, há um LED na travessia errada.

**Se não passar:** use `c6`, `c7`, … para fixar um canal e isolar o LED suspeito.

### Etapa 6 — O buzzer da travessia

**Monte:** buzzer **passivo** no GPIO **33**, com o outro terminal no terra.

**Prove:** comando `5` — três bips.

**Se não passar:** um clique só, sem tom = buzzer **ativo** em vez de passivo. O ativo tem frequência fixa própria e ignora o `tone()`; troque a peça.

> Esta etapa parece a mais dispensável da placa e é a que sustenta a acessibilidade do cruzamento: o apito é a única indicação da travessia para quem não vê o foco. Ele apita na fase de pedestre e **acelera** no intermitente — a mudança de ritmo é o que avisa que o tempo está acabando.

### Etapa 7 — O firmware de verdade, ainda sem Node-RED

**Monte:** nada. Grave `node_sinaleiras` com o `secrets.h` preenchido.

**Prove:** ao ligar, **todos os vermelhos acendem e o resto fica apagado**. Depois de ~10 s sem comando, a placa assume o **ciclo de tempo fixo** e alterna sozinha. Toda troca passa por amarelo e por um instante de tudo vermelho — **nunca verde direto para verde**.

**Se não passar:** se acender qualquer coisa fora do padrão inicial, volte ao comando `2` do sketch de bancada: é fiação, não firmware.

> A placa 1B ainda nem existe, e o cruzamento já funciona. É isso que o corte por função garante: sem os sensores, perde-se eficiência, nunca segurança.


> **Se o verde ou o amarelo ficarem fracos.** Parte dos módulos de mini sinaleira já
> traz resistor embutido. Somado ao de 220 Ω, o total deixa pouca tensão para o LED:
> em 3,3 V, verde e amarelo (queda de ~2,6 a 3,0 V) apagam muito antes do vermelho
> (~1,8 V). O teste é direto — tire o resistor externo de **uma** entrada de verde e
> compare. Se o brilho normalizar e o módulo não esquentar, ele tem resistor próprio e
> o externo sai de todas as entradas; se não acender igual, o resistor externo fica e
> troque **só** o das cores fracas por 100 Ω.

> **O apito da travessia não é enfeite.** Ele é a indicação da travessia para quem
> não vê o foco, e por isso ficou na placa mesmo depois de a montagem ter passado um
> tempo sem ele. Ritmo lento no verde de pedestre, rápido no intermitente.
---

## Placa 1B — Sensores

Só entradas: 7 sensores TCRT5000 e 4 botoeiras. Não aciona foco nenhum.

Os sete módulos **não entram na protoboard** — vão por jumper macho–fêmea direto aos
pinos do devkit. A protoboard leva as botoeiras e distribui 3,3 V e terra.

### Etapa 1 — Alimentação e sinal de vida

**Monte:** o ESP32 **fora da protoboard**, ligado só pelo cabo USB. Da protoboard, por enquanto, apenas as trilhas: `GND` do devkit para a trilha **−** e `3V3` para a trilha **+**, e as duas trilhas de mesmo sinal unidas na outra ponta.

**Confira antes de ligar:** com o multímetro entre as trilhas **+** e **−**: **3,3 V**. Se der 0 V, o jumper caiu numa fileira errada — as trilhas de cima e de baixo **não** são ligadas de fábrica.

**Prove:** grave `bancada_sensores` e abra o serial em 115200. O menu tem que aparecer; digite `?` se não aparecer.

**Se não passar:** sem porta COM na IDE = falta o driver USB do devkit (CP2102 ou CH340). Menu com caracteres embaralhados = baud diferente de 115200.

> Posições exatas de cada peça: [layout/placa1B_sensores.png](layout/placa1B_sensores.png).

### Etapa 2 — O primeiro TCRT5000, e o ajuste do trimpot

**Monte:** **um** módulo: `VCC` na trilha **+** (3,3 V), `GND` na **−**, `DO` no GPIO **34** (aproximação Norte). O pino `AO` fica **sem ligação**.

**Confira antes de ligar:** a alimentação é **3,3 V**, não 5 V. A saída acompanha a alimentação.

**Prove:** comando `2`. O serial imprime os sete detectores continuamente; o primeiro deve alternar entre `[ ]` e `[X]` quando você aproxima a mão **a um centímetro** do par de LEDs. Gire o trimpot até detectar na distância que você quer.

**Se não passar:** nunca detecta = trimpot no extremo, `DO` no pino errado, ou a mão longe demais — este sensor enxerga **milímetros**, não os 30 cm de um ultrassônico. Detecta **sempre** = trimpot no outro extremo, ou há uma superfície clara parada na frente dele. O LED de detecção do módulo acende junto — se ele não acende, o problema é o módulo, não a fiação.

> O TCRT5000 mede **reflexão**: superfície clara devolve muito infravermelho, preto fosco quase nada. Calibre com o carrinho que você vai usar de verdade, e não com a mão — pele reflete bem mais que plástico escuro. Se um carrinho de fundo preto não for visto, um adesivo branco sob o chassi resolve.

### Etapa 3 — Os outros 6 sensores

**Monte:** três de aproximação (GPIO 35, 36, 39) e três de vaga (GPIO 32, 33, 25), um de cada vez.

**Confira antes de ligar:** os quatro de aproximação ficam em **34, 35, 36 e 39**, que são de entrada apenas e **sem pull-up interno** — podem, porque o módulo traz o próprio pull-up. Os de vaga vão no **piso da vaga**, virados para cima; os de aproximação, no **piso da via**, na linha de retenção.

**Prove:** comando `2`. Os sete aparecem na mesma linha; passe a mão em cada um e veja só ele mudar.

**Se não passar:** um sensor que muda junto com o vizinho = os dois estão em curto na trilha, ou dois `DO` no mesmo pino. Dois vizinhos que disparam juntos sem encostar em nada = estão perto demais e um vê o infravermelho do outro; afaste-os ou desencontre-os.

### Etapa 4 — As quatro botoeiras

**Monte:** cada botão a cavalo no canal central da protoboard: um terminal no GPIO, o outro no terra. GPIO 17, 21, 18, 19.

**Confira antes de ligar:** o tactile tem 4 pernas ligadas 2 a 2. Montado a cavalo no canal, os dois terminais são a **mesma coluna** nas duas metades. Se pegar o par errado, ele fica permanentemente apertado — gire-o 90°.

**Prove:** comando `3` e aperte uma de cada vez. O serial imprime `botoeira N (GPIO 17) APERTADA` e depois `solta`.

**Se não passar:** sempre "APERTADA" = par de pernas errado. Nenhuma responde = o fio de terra não chegou.

> Sem resistor, de propósito: o firmware usa o pull-up **interno** do ESP32, então nível baixo significa pressionada. Por isso as botoeiras não podem ficar em 34–39, que não têm pull-up.


> **Travessia pedida sozinha?** O pull-up interno do ESP32 é de ~45 kΩ — fraco o
> bastante para o fio da botoeira funcionar como antena. Na bancada, com os focos já
> em módulos, deram **19 pedidos em 50 s sem ninguém encostar**, concentrados nas
> trocas de fase: o chaveamento dos focos entrando pelo fio. Os TCRT5000 da mesma
> placa não tremeram nenhuma vez, porque o módulo deles tem pull-up próprio.
>
> O firmware já exige **60 ms de leitura estável** para contar um aperto, o que
> descarta pulso acoplado sem descartar aperto humano (que dura mais de 100 ms). Se
> ainda escapar alguma, por esta ordem: confira que **os quatro botões estão de fato
> montados** — pino com fio solto e sem botão é antena pura; afaste o fio da botoeira
> da fiação dos focos; e, se persistir, **10 kΩ do pino para o 3,3 V** (em paralelo
> com o interno, baixa a impedância do nó umas cinco vezes) mais **100 nF do pino
> para o terra**.
### Etapa 5 — O firmware, e o cruzamento inteiro

**Monte:** nada. Grave `node_sensores` e ligue as duas placas do cruzamento juntas.

**Prove:** com o broker de pé, `mosquitto_sub -h <IP> -t 'cidade/#' -v` mostra a 1B publicando em `cidade/cruzamento/sensores` e `cidade/estacionamento/estado`, e um `cidade/cruzamento/botoeira` a cada aperto.

> Daqui em diante, siga a seção *Verificação ponta a ponta* do [README.md](README.md).

---

## Placa 2 — Iluminação

A mais simples das cinco. Boa para montar primeiro se você quiser pegar o ritmo antes
de encarar o cruzamento.

### Etapa 1 — Alimentação e sinal de vida

**Monte:** o ESP32 **fora da protoboard**, ligado só pelo cabo USB. Da protoboard, por enquanto, apenas as trilhas: `GND` do devkit para a trilha **−** e `3V3` para a trilha **+**, e as duas trilhas de mesmo sinal unidas na outra ponta.

**Confira antes de ligar:** com o multímetro entre as trilhas **+** e **−**: **3,3 V**. Se der 0 V, o jumper caiu numa fileira errada — as trilhas de cima e de baixo **não** são ligadas de fábrica.

**Prove:** grave `bancada_iluminacao` e abra o serial em 115200. O menu tem que aparecer; digite `?` se não aparecer.

**Se não passar:** sem porta COM na IDE = falta o driver USB do devkit (CP2102 ou CH340). Menu com caracteres embaralhados = baud diferente de 115200.

> Posições exatas de cada peça: [layout/placa2_iluminacao.png](layout/placa2_iluminacao.png).

### Etapa 2 — O LED do poste

**Monte:** LED **branco** com resistor de **100 Ω** (não 220 Ω) no GPIO **25**.

**Prove:** comando `1` — o LED sobe e desce de brilho continuamente, sem piscar em degraus visíveis.

> 100 Ω e não 220 Ω porque o LED branco tem queda de tensão alta por natureza; com 220 Ω em 3,3 V ele fica fraco demais para se ver na maquete. Se não achar 100 Ω, dois de 220 Ω **em paralelo** dão 110 Ω.

### Etapa 3 — O divisor do LDR

**Monte:** LDR entre a trilha **+** e o nó de leitura, resistor de **10 kΩ** entre o nó e a trilha **−**, e o nó no GPIO **32**.

**Confira antes de ligar:** a ordem importa: LDR em cima, 10 kΩ embaixo. Invertido, a leitura anda ao contrário — mais luz derrubaria o valor. E confira que o resistor e o fio de terra saem da **mesma coluna**: colunas vizinhas são nós diferentes, e o divisor ficaria aberto.

**Prove:** comando `2`. Tape o LDR com a mão e depois ilumine. **Anote os dois extremos** — o sketch mostra `min` e `max` — porque o limiar do firmware fica no meio deles.

**Se não passar:** valor colado em 0 ou em 4095 = um dos dois lados do divisor está solto. Valor que não muda com a luz = o nó de leitura pegou a coluna errada.

### Etapa 4 — O sensor de presença (TCRT5000)

**Monte:** módulo alimentado em **3,3 V**, `DO` no GPIO **33**, terra comum. O pino `AO` fica **sem ligação**. Ele é reflexivo e de alcance curto: enxerga por volta de **1 cm**, e o objeto tem de passar **por cima** dele — na maquete o sensor vai embutido no piso, virado para cima, ao lado do poste.

**Prove:** comando `3` e passe a mão a **um dedo de distância** do sensor. O serial alterna entre `DETECTADO` e `livre`, e o LED de bordo do módulo acende junto.

**Se não passar:** **Detecta sempre?** Há uma superfície clara parada na frente dele (a bancada, se o módulo estiver deitado) ou o trimpot está no extremo. **Nunca detecta?** Aproxime mais: o alcance é de milímetros. Se ainda assim nada, meça `DO` contra o terra — **3,3 V livre, ~0 V com objeto**; valor parado no meio da escala é fio solto. O LED de detecção do módulo é o juiz: se ele acende e o serial não muda, o problema está entre o `DO` e o GPIO; se ele não acende, o problema é anterior ao `DO`.

> Este é o mesmo módulo dos sete da placa 1B — o oitavo. Ele substituiu o PIR HC-SR501 e a diferença não é só de alcance: a saída é **ativa em baixo**, e o módulo **não segura** a detecção. Quem mantém o poste aceso depois que o objeto sai são os 5 s de `TEMPO_PRESENCA` do firmware.

### Etapa 5 — Os dois juntos, e depois o firmware

**Monte:** nada.

**Prove:** comando `4`: o LED só acende com as **duas** condições — escuro **e** presença. Batendo, grave `node_iluminacao` e confira no painel.

---

## Placa 3 — Ambiental

Sete LEDs, dois sensores e um servo. O servo é a única peça da maquete que puxa
corrente de pico alta — trate a alimentação dele com respeito.

### Etapa 1 — Alimentação e sinal de vida

**Monte:** o ESP32 **fora da protoboard**, ligado só pelo cabo USB. Da protoboard, por enquanto, apenas as trilhas: `GND` do devkit para a trilha **−** e `3V3` para a trilha **+**, e as duas trilhas de mesmo sinal unidas na outra ponta.

**Confira antes de ligar:** com o multímetro entre as trilhas **+** e **−**: **3,3 V**. Se der 0 V, o jumper caiu numa fileira errada — as trilhas de cima e de baixo **não** são ligadas de fábrica.

**Prove:** grave `bancada_ambiental` e abra o serial em 115200. O menu tem que aparecer; digite `?` se não aparecer.

**Se não passar:** sem porta COM na IDE = falta o driver USB do devkit (CP2102 ou CH340). Menu com caracteres embaralhados = baud diferente de 115200.

> Posições exatas de cada peça: [layout/placa3_ambiental.png](layout/placa3_ambiental.png).

### Etapa 2 — Os sete LEDs

**Monte:** quatro da régua de nível e três das faixas de temperatura, cada um com 220 Ω: nivel verde = GPIO **19**, nivel amarelo = GPIO **21**, nivel laranja = GPIO **22**, nivel vermelho = GPIO **23**, temp verde = GPIO **26**, temp amarelo = GPIO **27**, temp vermelho = GPIO **14**.

**Prove:** comando `1` (varredura). Cada LED acende por 0,8 s e o serial diz o nome — confira que batem.

**Se não passar:** LED trocado de lugar é o erro mais comum aqui, porque são sete cores parecidas em sequência. O serial resolve em 10 s.

### Etapa 3 — O sensor de nível, e a calibração dele

**Monte:** sensor de nível analógico alimentado em **3,3 V**, saída no GPIO **35**.

**Confira antes de ligar:** GPIO 35 é do **ADC1**. Analógico fora do ADC1 lê valor travado com o Wi-Fi ligado.

**Prove:** comando `2`. Anote a leitura **crua** com o sensor **seco** e com ele **submerso até o topo**. Esses dois números viram `NIVEL_SECO` e `NIVEL_CHEIO` no `node_ambiental.ino`.

**Se não passar:** leitura que não muda ao molhar = sensor sem alimentação, ou saída no pino errado. Leitura que satura em 4095 seco = a saída está ligada ao 3,3 V em vez do pino de sinal.

> A grandeza **inverteu** em relação ao ultrassônico: agora é submersão, e **mais é pior**. Sem calibrar, os limiares de alerta caem no lugar errado.

### Etapa 4 — O DHT22

**Monte:** módulo DHT22 (AM2302) de 3 pinos alimentado em **3,3 V**, dados no GPIO **4**.

**Confira antes de ligar:** **a ordem dos pinos pela serigrafia do seu módulo, não pela posição no desenho.** Ela muda de fabricante para fabricante, e em boa parte dos módulos de 3 pinos o **VCC é o do meio**, com o dado na borda (`S`, `OUT` ou `DATA`) — o contrário do desenho. Prefira a versão em **módulo**, que já traz o resistor de pull-up: o sensor nu de 4 pinos precisa de um 10 kΩ entre dados e 3,3 V.

**Prove:** comando `3`. Deve mostrar temperatura ambiente e umidade, com uma casa decimal. Segure o sensor entre os dedos: a temperatura sobe alguns décimos em segundos.

**Se não passar:** o serial diz o motivo, e cada um aponta para um lugar. **`linha em baixo`**: meça a tensão **entre o `+` do módulo e o GND do DevKit** — tem de dar 3,3 V. Na primeira bancada deu ~1,6 V: um jumper de alimentação com mau contato, e o sensor passou uma noite parecendo morto. Meça contra o GND do DevKit, e não contra o `−` do módulo: assim você descobre de qual lado está o fio ruim. **`sensor mudo`**: a linha está boa e nada volta — ordem dos pinos trocada ou sensor queimado. **`checksum`** ou **`transmissao incompleta`**: o sensor fala e se atrapalha — fio comprido ou contato ruim no dado. Ritmo não é causa: o sketch já espera 2,5 s entre leituras.

### Etapa 5 — O servo da comporta

**Monte:** SG90 com sinal no GPIO **25**, alimentado pelo **5 V** (VIN ou fonte), com o **terra unido** ao do ESP32.

**Confira antes de ligar:** o terra comum. Sem ele o pulso de comando não tem referência e o servo treme ou ignora o comando.

**Prove:** comando `4` — vai de 0° a 90° e volta, sem tremer e sem travar.

**Se não passar:** tremor constante ou reset do ESP32 no meio do movimento = o servo está puxando do regulador de 3,3 V do devkit. Passe para o 5 V.

### Etapa 6 — O buzzer, e depois o firmware

**Monte:** buzzer **passivo** no GPIO **13**.

**Prove:** comando `5` — três bips. Depois grave `node_ambiental` com os valores de calibração que você anotou na etapa 3.

> Esta placa usava buzzer **ativo** até a arquitetura mudar. Agora as quatro usam passivo — se você comprou um ativo, ele vai apitar contínuo.

---

## Placa 4 — Segurança

Poucas peças, mas com o único trecho da maquete em que um erro de montagem **queima**
o microcontrolador de imediato. A etapa 3 tem uma medição obrigatória.

### Etapa 1 — Alimentação e sinal de vida

**Monte:** o ESP32 **fora da protoboard**, ligado só pelo cabo USB. Da protoboard, por enquanto, apenas as trilhas: `GND` do devkit para a trilha **−** e `3V3` para a trilha **+**, e as duas trilhas de mesmo sinal unidas na outra ponta.

**Confira antes de ligar:** com o multímetro entre as trilhas **+** e **−**: **3,3 V**. Se der 0 V, o jumper caiu numa fileira errada — as trilhas de cima e de baixo **não** são ligadas de fábrica.

**Prove:** grave `bancada_seguranca` e abra o serial em 115200. O menu tem que aparecer; digite `?` se não aparecer.

**Se não passar:** sem porta COM na IDE = falta o driver USB do devkit (CP2102 ou CH340). Menu com caracteres embaralhados = baud diferente de 115200.

> Posições exatas de cada peça: [layout/placa4_seguranca.png](layout/placa4_seguranca.png).

### Etapa 2 — Os três LEDs de estado

**Monte:** verde = GPIO **25**, amarelo = GPIO **26**, vermelho = GPIO **27**, cada um com 220 Ω.

**Prove:** comando `1` (varredura), conferindo os nomes no serial.

### Etapa 3 — O divisor do MQ-2: meça antes de plugar

**Monte:** o **divisor primeiro, sozinho**: resistor de **1 kΩ** entre a saída **AO** do módulo e o nó de leitura, e **2 kΩ** entre o nó e a trilha **−**. O MQ-2 alimentado em **5 V**. **Ainda não ligue o nó no GPIO 34.**

**Confira antes de ligar:** que o resistor de baixo e o fio de terra saem da **mesma coluna**. Colunas vizinhas são nós diferentes — o divisor ficaria aberto embaixo e o nó iria a 5 V. A saída DO do módulo fica sem ligação.

**Prove:** com o MQ-2 energizado e o nó **ainda solto**, meça **duas** tensões contra o terra: a do AO direto no módulo e a do nó. O nó tem que dar **⅔ da do AO** (AO 4,5 V → nó ≈ 3,0 V). É a razão que prova o divisor, não um número absoluto — o AO varia com o gás no ar. Confirmada a razão, ligue o nó ao GPIO 34.

**Se não passar:** nó **igual** ao AO = um dos resistores sem contato, e o GPIO receberia os 5 V inteiros. Nó em **⅓** em vez de ⅔ = os dois resistores trocados de posição. Nó em 0 V = o de 2 kΩ está em curto com o terra dos dois lados.

> O divisor é 1:2, então o nó fica em **2/3** do AO — no pior caso, 5 V viram **3,33 V**. É o teto por projeto, dentro do máximo absoluto do ESP32 (3,6 V), mas encosta no fim da escala do ADC. Se faltar o de 2 kΩ, dois de 1 kΩ **em série** dão o mesmo. É o único divisor que restou no projeto.

### Etapa 4 — Calibração do MQ-2

**Monte:** nada — o sensor precisa de **~20 s de pré-aquecimento** toda vez que liga.

**Prove:** comando `2`. Deixe uns 20 s em ar limpo e **anote o valor estável**: é o seu piso. Aproxime a fonte de gás (um isqueiro **sem acender**, só o gás) e anote o pico. Os limiares de alerta e de perigo saem desses dois números — o sketch mostra `min` e `max`.

> Os valores de fábrica no firmware são um chute razoável, não uma medição. Calibrar aqui é o que faz o alerta disparar na hora certa na apresentação.

### Etapa 5 — A sirene, e depois o firmware

**Monte:** buzzer passivo no GPIO **13**.

**Prove:** comando `3` — três bips. Depois grave `node_seguranca` com os limiares que você anotou.

---

## Depois das cinco

Com as placas provadas individualmente, o resto é rede, não solda:

1. Broker Mosquitto de pé, IP anotado, e o **mesmo IP** nos cinco `secrets.h` e no
   nó `broker1` do `flows.json` — seis lugares.
2. `mosquitto_sub -h <IP> -t 'cidade/#' -v` com as cinco ligadas: as cinco têm que
   aparecer publicando.
3. Node-RED com o `flows.json` importado e o painel em `http://<ip>:1880/ui`.
4. A seção *Verificação ponta a ponta* do [README.md](README.md).

**Alimentação de todas juntas:** hub USB com fonte própria. Cinco ESP32 mais o servo
passam do que uma porta de notebook entrega, e o sintoma de subalimentação é a placa
reiniciando sozinha no meio da demonstração — não um erro claro.

---

## Quando algo não funciona

| Sintoma | Causa mais provável |
|---|---|
| Um LED bem mais fraco que o par dele | os dois dividem um resistor só — cada LED leva o seu |
| Canal de veículo acende **um** LED em vez de dois | a segunda cabeça não foi ligada, ou caiu na coluna vizinha |
| Acende o LED errado na varredura | dois GPIOs trocados entre si |
| Botoeira sempre "apertada" | par de pernas errado do botão — gire 90° |
| Botoeira que nunca responde | está em 34–39, que não têm pull-up interno |
| Módulo IR detecta sempre, ou nunca | trimpot no extremo; confira o LED de detecção do módulo |
| Módulo IR queimou o GPIO | foi alimentado em 5 V; a saída acompanha a alimentação |
| Leitura analógica travada | pino fora do **ADC1** (só GPIO 32–39 servem com Wi-Fi) |
| Divisor com o nó igual à entrada | resistor sem contato, ou terra na coluna vizinha |
| ESP32 reinicia ao mover o servo | servo puxando do regulador de 3,3 V do devkit |
| Buzzer só dá um clique | é buzzer **ativo**; o projeto usa **passivo** |
| `DHT22 nao respondeu - linha em baixo` | módulo sem alimentação: meça 3,3 V entre o `+` do módulo e o GND do DevKit |
| `DHT22 nao respondeu - sensor mudo` | a linha está boa e nada volta: pinos trocados, ou sensor queimado |
| `DHT22 nao respondeu - checksum` | o sensor fala e se atrapalha: fio comprido ou contato ruim |
| Sensor lê lixo de forma intermitente | terra não comum |
| Placa some do painel e volta | Wi-Fi fraco, ou alimentação USB insuficiente |

Em qualquer caso: **volte ao sketch de bancada**. Se a etapa passa nele e falha com o
firmware, o problema é rede ou configuração; se falha nos dois, é o circuito.
