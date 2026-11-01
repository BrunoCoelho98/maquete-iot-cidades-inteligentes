#pragma once
/* Lista FECHADA de fases seguras do cruzamento.
   O Node-RED escolhe entre elas; nunca comanda LED individual. Por isso nenhum
   comando consegue produzir uma combinacao com verde conflitante.

   Fica num header, e nao no .ino, de proposito: o pre-processador do Arduino
   injeta os prototipos automaticos logo apos os #include, entao um tipo usado
   em assinatura de funcao precisa estar visivel antes disso. Declarar o enum
   dentro do .ino funciona ou nao dependendo de onde cai a ultima diretiva de
   pre-processador - fragil demais para deixar no ar. */
enum Fase {
  F_VIA_A,        // carros Norte e Sul (via A) no verde
  F_VIA_B,        // carros Leste e Oeste (via B) no verde
  F_PEDESTRES,    // todos os carros no vermelho, travessias pedidas abertas
  F_AMARELO,      // encerrando a fase de verde que estava em curso
  F_ENTREVERDES,  // tudo vermelho, intervalo de limpeza do cruzamento
  F_PISCANTE      // falha / manutencao
};
