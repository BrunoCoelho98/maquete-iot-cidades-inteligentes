# Instalação

Três programas fazem a maquete funcionar: a **Arduino IDE**, para gravar as placas; o
**Mosquitto**, o broker MQTT; e o **Node-RED**, o painel. O broker e o Node-RED podem
ficar no mesmo computador ou em computadores diferentes da mesma rede.

## 1. Arduino IDE e as placas

1. Instale a [Arduino IDE 2](https://www.arduino.cc/en/software).
2. Em *Ferramentas → Placa → Gerenciador de placas*, instale **esp32 by Espressif
   Systems**, versão **3.x**. Se ele não aparecer na busca, acrescente em
   *Arquivo → Preferências → URLs adicionais* o endereço
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json` e procure de novo.
3. Em *Ferramentas → Gerenciar bibliotecas*, instale **PubSubClient**, **ArduinoJson**
   (versão 7) e **ESP32Servo**. O sensor DHT22 não precisa de biblioteca: a leitura está
   em `node_ambiental/dht_simples.h`.
4. Em cada pasta `node_*`, copie `secrets.example.h` para `secrets.h` e preencha:
   - `WIFI_SSID` e `WIFI_PASS`: rede de **2,4 GHz** com senha compartilhada;
   - `MQTT_HOST`: IP do computador onde o broker vai rodar (passo 2);
   - `MQTT_PORT` e `TOPIC_BASE`: deixe como estão (`1883` e vazio).
5. Abra o `.ino` de cada placa, escolha a placa **ESP32 Dev Module** e a porta, e grave.
   Se a gravação parar em `Connecting......`, segure o botão **BOOT** da placa até
   aparecerem as porcentagens.
6. Abra o Monitor Serial em **115200**: a placa deve mostrar o Wi-Fi conectado e
   `Conectando MQTT... ok`.

Antes de gravar o firmware completo numa placa recém-montada, vale testar o circuito com os
sketches de [`bancada/`](bancada/README.md), que não usam rede.

## 2. Broker MQTT (Mosquitto)

O broker pode rodar em qualquer computador da rede. Depois de escolher qual, descubra o IP
dele (no Windows, `ipconfig`; no Linux e no macOS, `ip addr` ou `ifconfig`) e coloque esse
IP no `MQTT_HOST` do `secrets.h` de todas as placas.

### Windows

1. Instale o Mosquitto a partir de <https://mosquitto.org/download/> (o instalador usa
   `C:\Program Files\mosquitto\`).
2. Dê duplo clique em [`broker.cmd`](broker.cmd), na raiz deste repositório. Deixe a janela
   aberta durante a aula.
3. Se as placas não conectarem com o broker no ar, libere a porta no firewall, uma única
   vez, num Prompt de Comando aberto como administrador:

   ```
   netsh advfirewall firewall add rule name="Mosquitto MQTT 1883" dir=in action=allow protocol=TCP localport=1883
   ```

### Linux ou macOS

```bash
# Debian, Ubuntu, Raspberry Pi OS
sudo apt install mosquitto mosquitto-clients
# macOS (Homebrew)
brew install mosquitto

# na pasta deste repositório
mosquitto -c mosquitto.conf -v
```

### Broker fixo numa Raspberry Pi

Para que o broker fique sempre ligado e não dependa do notebook do professor:

```bash
sudo apt install mosquitto mosquitto-clients
printf 'listener 1883 0.0.0.0\nallow_anonymous true\n' | sudo tee /etc/mosquitto/conf.d/maquete.conf
sudo systemctl enable mosquitto
sudo systemctl restart mosquitto
```

Reserve um IP fixo para a Raspberry Pi no roteador (reserva de DHCP). Assim o `MQTT_HOST`
das placas não precisa mudar de uma aula para outra. O Node-RED pode rodar na mesma
Raspberry Pi (veja abaixo), e a maquete fica independente de qualquer outro computador.

### Conferir se o broker está recebendo

Em qualquer computador da rede com o Mosquitto instalado:

```bash
mosquitto_sub -h <ip-do-broker> -t 'cidade/#' -v
```

Com as placas ligadas, a telemetria aparece na tela. Para mandar um comando sem o painel:

```bash
mosquitto_pub -h <ip-do-broker> -t 'cidade/iluminacao/cmd' -m '{"modo":"ON"}'
```

## 3. Node-RED

### Windows, Linux ou macOS

1. Instale o [Node.js](https://nodejs.org/) na versão LTS.
2. Num terminal: `npm install -g --unsafe-perm node-red`.
3. Rode `node-red` e abra `http://localhost:1880`.

### Raspberry Pi

```bash
bash <(curl -sL https://github.com/node-red/linux-installers/releases/latest/download/install-update-nodered-deb)
sudo systemctl enable nodered.service
node-red-start
```

### Recriar o painel

1. No editor do Node-RED: menu ☰ → *Manage palette* → *Install* → instale
   **node-red-dashboard**.
2. Menu ☰ → *Import* → selecione o [`flows.json`](flows.json) deste repositório →
   *Import* → *Deploy*.
3. Abra qualquer nó MQTT e clique no lápis ao lado de *Server* (nó **broker1**). Deixe
   `localhost` se o broker roda no mesmo computador do Node-RED; se não, coloque o IP do
   broker. Clique em *Update* e em *Deploy*. Os nós MQTT devem mostrar **connected**.
4. Abra o painel em `http://<ip-do-node-red>:1880/ui`. Ele pode ser aberto em qualquer
   computador, tablet ou celular da mesma rede.

## Problemas comuns

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| Monitor Serial só imprime pontinhos depois de "Conectando WiFi" | rede de 5 GHz, nome ou senha errados | usar rede 2,4 GHz e conferir o `secrets.h` |
| A placa conecta no Wi-Fi mas não no MQTT | broker fora do ar, sem o arquivo de configuração, IP errado ou firewall | conferir o broker e o `MQTT_HOST`; liberar a porta 1883 |
| Funcionava ontem e hoje não conecta | o IP do computador do broker mudou (DHCP) | atualizar o `MQTT_HOST` ou reservar IP fixo no roteador |
| Painel abre mas não mostra dados | nó broker1 desconectado | conferir o endereço do broker1 no Node-RED |
| Placa aparece OFFLINE alguns segundos depois de desligada | comportamento normal | o broker espera o tempo de *keepalive* antes de publicar o *last will* |
| Rede da universidade não funciona | eduroam e redes semelhantes exigem login individual (WPA2-Enterprise) | usar o hotspot do celular ou um roteador próprio |
