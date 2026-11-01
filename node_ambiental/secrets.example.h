// Modelo de configuracao. Copie este arquivo para "secrets.h", na mesma
// pasta, e preencha com os dados da SUA rede. O secrets.h nao vai para o
// repositorio (esta no .gitignore), entao a senha nunca fica publica.
//
// Todas as placas usam os mesmos valores.
#pragma once

#define WIFI_SSID   "NOME_DA_REDE"     // rede de 2,4 GHz (o ESP32 nao enxerga 5 GHz)
#define WIFI_PASS   "SENHA_DA_REDE"    // WPA2-Personal; redes como a eduroam nao servem
#define MQTT_HOST   "192.168.0.10"     // IP do computador onde o broker esta rodando
#define MQTT_PORT   1883               // porta padrao do MQTT
#define TOPIC_BASE  ""                 // prefixo opcional dos topicos; deixe vazio
