# Esquemáticos elétricos

Esquemáticos das **cinco placas**, em KiCad, para ler e conferir ligação por ligação. Os
PDFs servem para imprimir e levar para a bancada.

| Documento | Responde |
|---|---|
| estes esquemáticos | o que liga em quê, eletricamente |
| [layout/](../layout/) | em que furo da protoboard cada peça entra |
| [README](../README.md#mapa-de-gpio-ajuste-ao-seu-devkit) | pinagem em tabela e as regras elétricas do ESP32 |
| [PASSO_A_PASSO.md](../PASSO_A_PASSO.md) | ordem de montagem, com um teste por etapa |

## Arquivos

```
esquematicos/
  sinaleiras.kicad_sch   placa 1A (todos os focos do cruzamento)
  sensores.kicad_sch     placa 1B (todas as entradas do cruzamento e do estacionamento)
  iluminacao.kicad_sch   placa 2
  ambiental.kicad_sch    placa 3
  seguranca.kicad_sch    placa 4
  pdf/                   os mesmos desenhos, para imprimir
```

Para abrir no KiCad, use **File → Open Schematic**: são folhas avulsas de documentação,
sem projeto de placa de circuito impresso associado.

Os esquemáticos foram gerados a partir da pinagem do próprio firmware. Se um pino mudar no
código, confira também o desenho.
