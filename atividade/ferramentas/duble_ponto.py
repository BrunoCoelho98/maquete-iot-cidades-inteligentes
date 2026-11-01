# -*- coding: utf-8 -*-
"""Dublê do ponto de onibus: faz na rede o que o ESP32 da atividade faz.

Mesmos topicos, mesmo LWT, mesmo heartbeat com o nome, mesmos comandos - lidos
do firmware pelo fonte.py, entao nao tem como divergir dele.

Para que serve
  - ENSAIAR a atividade sem hardware: o mediador roda a parte do Node-RED
    inteira, do jeito que o estudante vai fazer, antes do dia.
  - PLANO B no dia: se a placa do estudante nao funcionar e o tempo apertar, o
    dublê assume o lugar dela e a parte do Node-RED - que e onde estao os
    conceitos de publicar, assinar e topico - acontece mesmo assim. Registre
    na ficha de observacao que o dublê foi usado.

Uso
  python atividade/ferramentas/duble_ponto.py                  # broker do secrets.h
  python atividade/ferramentas/duble_ponto.py --host 127.0.0.1 --nome "Ponto Teste"

  Enter  = apertar o botao        q = sair normalmente
  k      = "arrancar o cabo" (sai sem se despedir: o broker publica o LWT offline)
"""
import argparse
import json
import os
import re
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fonte as F  # noqa: E402

import paho.mqtt.client as mqtt  # noqa: E402


def broker_do_secrets():
    """So MQTT_HOST, MQTT_PORT e TOPIC_BASE. A senha do Wi-Fi nao e lida."""
    pasta = os.path.join(F.ATIV, "ponto_onibus")
    arq = os.path.join(pasta, "secrets.h")
    if not os.path.exists(arq):  # sem secrets.h: usa o modelo (passe --host)
        arq = os.path.join(pasta, "secrets.example.h")
    txt = open(arq, encoding="utf-8").read()
    host = re.search(r'#define\s+MQTT_HOST\s+"([^"]*)"', txt).group(1)
    porta = int(re.search(r"#define\s+MQTT_PORT\s+(\d+)", txt).group(1))
    base = re.search(r'#define\s+TOPIC_BASE\s+"([^"]*)"', txt).group(1)
    return host, porta, base


class Ponto:
    def __init__(self, host, porta, base="", nome=F.NOME_PADRAO, log=print):
        self.base, self.nome, self.log = base, nome, log
        self.t_estado = base + F.TOPICO_ESTADO
        self.t_cmd = base + F.TOPICO_CMD
        self.t_status = base + F.TOPICO_STATUS
        self.t_hb = base + F.TOPICO_HB
        self.esperando = False
        self.onibus = "LONGE"
        self.chegada = 0.0
        self.recebidos = []
        self.inicio = time.time()
        self.c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                             client_id="duble-ponto-%d" % (int(time.time() * 1000) % 100000))
        self.c.will_set(self.t_status, "offline", qos=1, retain=True)
        self.c.on_connect = self._conectou
        self.c.on_message = self._mensagem
        self.c.connect(host, porta, keepalive=15)
        self.c.loop_start()
        self._vivo = True
        threading.Thread(target=self._relogio, daemon=True).start()

    def _conectou(self, c, u, f, rc, props=None):
        self.log("conectado à central (%s)" % rc)
        c.publish(self.t_status, "online", qos=1, retain=True)
        c.subscribe(self.t_cmd)
        self.publicar_estado()
        self.heartbeat()

    def _mensagem(self, c, u, m):
        msg = m.payload.decode("utf-8", "replace").replace('"', "").strip().upper()
        self.recebidos.append(msg)
        self.log("chegou da central: %s" % msg)
        if msg == F.COMANDOS[0]:
            self.onibus = "VINDO"
            self.log("  LUZ: piscando (ônibus vindo)")
        elif msg == F.COMANDOS[1]:
            self.onibus = "NO_PONTO"
            self.chegada = time.time()
            self.log("  LUZ: acesa (ônibus no ponto)")
        else:
            self.log("  comando desconhecido")

    def publicar_estado(self):
        msg = F.MSG_ESPERANDO if self.esperando else F.MSG_VAZIO
        self.c.publish(self.t_estado, msg)
        self.log("  enviado: %s" % msg)

    def heartbeat(self):
        self.c.publish(self.t_hb, json.dumps({"uptime": int(time.time() - self.inicio),
                                              "rssi": -55, "nome": self.nome},
                                             ensure_ascii=False))

    def apertar(self):
        self.log("botão apertado")
        self.esperando = True
        self.publicar_estado()

    def _relogio(self):
        ultimo_hb = time.time()
        while self._vivo:
            time.sleep(0.2)
            if self.onibus == "NO_PONTO" and time.time() - self.chegada >= F.TEMPO_NO_PONTO_S:
                self.onibus = "LONGE"
                self.log("  LUZ: apagada")
                if self.esperando:
                    self.esperando = False
                    self.log("passageiro embarcou")
                    self.publicar_estado()
            if time.time() - ultimo_hb >= 10:
                ultimo_hb = time.time()
                self.heartbeat()

    def sair(self):
        self._vivo = False
        self.c.publish(self.t_status, "offline", qos=1, retain=True).wait_for_publish(2)
        self.c.disconnect()
        self.c.loop_stop()


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    host, porta, base = broker_do_secrets()
    a = argparse.ArgumentParser(description="Dublê do ponto de ônibus da atividade")
    a.add_argument("--host", default=host)
    a.add_argument("--porta", type=int, default=porta)
    a.add_argument("--base", default=base, help="prefixo de tópico (vazio na produção)")
    a.add_argument("--nome", default=F.NOME_PADRAO)
    a = a.parse_args()
    print("dublê do ponto -> %s:%d  (tópicos %s...)" % (a.host, a.porta, a.base + "cidade/ponto/"))
    print("Enter = apertar o botão   q = sair   k = arrancar o cabo (dispara o LWT)\n")
    p = Ponto(a.host, a.porta, a.base, a.nome,
              log=lambda s: print(time.strftime("%H:%M:%S"), s, flush=True))
    for linha in sys.stdin:
        cmd = linha.strip().lower()
        if cmd == "q":
            p.sair()
            return
        if cmd == "k":
            print("cabo arrancado: saindo sem se despedir", flush=True)
            os._exit(0)
        p.apertar()


if __name__ == "__main__":
    main()
