# -*- coding: utf-8 -*-
"""Fonte unica da atividade: tudo que o roteiro, o gabarito do Node-RED e os
desenhos precisam dizer sobre o modulo sai daqui, e daqui sai do FIRMWARE.

O roteiro manda o estudante digitar nomes de topico e comandos. Se o texto do
roteiro e o firmware divergirem em uma letra, a atividade para no meio sem
erro visivel - a mensagem simplesmente nao chega. Por isso nenhum desses
valores e escrito a mao em outro lugar: e o mesmo principio do gera_layout.py,
em que lista de material e desenho nao tem como divergir.
"""
import os
import re

AQUI = os.path.dirname(os.path.abspath(__file__))
ATIV = os.path.dirname(AQUI)
RAIZ = os.path.dirname(ATIV)
INO = os.path.join(ATIV, "ponto_onibus", "ponto_onibus.ino")
SRC = open(INO, encoding="utf-8").read()


def _topico(nome):
    return re.search(nome + r'\s*=\s*TOPIC_BASE\s*"([^"]+)"', SRC).group(1)


def _int(nome):
    return int(re.search(r"const\s+int\s+" + nome + r"\s*=\s*(\d+)", SRC).group(1))


TOPICO_ESTADO = _topico("TOP_ESTADO")
TOPICO_CMD = _topico("TOP_CMD")
TOPICO_STATUS = _topico("TOP_STATUS")
TOPICO_HB = _topico("TOP_HB")
PIN_LUZ = _int("PIN_LUZ")
PIN_BOTAO = _int("PIN_BOTAO")
VELOCIDADE_PISCA = _int("VELOCIDADE_PISCA")
NOME_PADRAO = re.search(r'NOME_DO_PONTO\s*=\s*"([^"]*)"', SRC).group(1)

# comandos que o firmware reconhece, na ordem em que aparecem no codigo
COMANDOS = re.findall(r'msg == "([A-Z_]+)"', SRC)
# as duas mensagens de telemetria:  passageiroEsperando ? "AGUARDANDO" : "VAZIO"
_m = re.search(r'passageiroEsperando\s*\?\s*"(\w+)"\s*:\s*"(\w+)"', SRC)
MSG_ESPERANDO, MSG_VAZIO = _m.group(1), _m.group(2)

TEMPO_NO_PONTO_S = int(re.search(r"TEMPO_NO_PONTO\s*=\s*(\d+)", SRC).group(1)) // 1000

# o segmento do topico que o painel "Estado da Cidade" usa como chave
PLACA = TOPICO_STATUS.split("/")[-2]

assert COMANDOS == ["VINDO", "CHEGOU"], COMANDOS
assert TOPICO_ESTADO.startswith("cidade/") and TOPICO_CMD.startswith("cidade/")
assert PIN_LUZ != PIN_BOTAO


# O que NAO vem do firmware mas aparece em mais de um lugar (roteiro e gabarito):
# nomes que o estudante ve e digita no Node-RED. Definidos uma vez so.
GRUPO_PAINEL = "Ponto de Ônibus"      # grupo no painel (dashboard)
ABA_EDITOR = "Ponto de Ônibus"        # aba no editor do Node-RED
ROTULO_SITUACAO = "Situação do ponto"
BOTOES = [("Ônibus vindo", "VINDO"), ("Ônibus chegou", "CHEGOU")]
assert [c for _, c in BOTOES] == COMANDOS


def rotulo_pino(g):
    """Como o pino aparece impresso no DevKit V1 de 30 pinos."""
    return {1: "TX0", 3: "RX0", 16: "RX2", 17: "TX2"}.get(g, "D%d" % g)


if __name__ == "__main__":
    import sys
    sys.stdout.reconfigure(encoding="utf-8")
    for k in ("TOPICO_ESTADO", "TOPICO_CMD", "TOPICO_STATUS", "TOPICO_HB", "PIN_LUZ",
              "PIN_BOTAO", "COMANDOS", "MSG_ESPERANDO", "MSG_VAZIO", "NOME_PADRAO",
              "VELOCIDADE_PISCA", "TEMPO_NO_PONTO_S", "PLACA"):
        print("%-17s %s" % (k, globals()[k]))
