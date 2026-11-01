@echo off
setlocal enabledelayedexpansion
rem ============================================================
rem   MINI SMART CITY - sobe o broker MQTT para a maquete
rem ============================================================
rem   Duplo-clique e pronto. Deixe esta janela ABERTA: o log do
rem   mosquitto mostra cada placa conectando e cada publicacao.
rem   Fechar a janela (ou Ctrl+C) derruba o broker.
rem
rem   Por que este script existe: o mosquitto, iniciado sem -c,
rem   escuta SO no loopback - a ESP32 leva "conexao recusada" e
rem   nada no firmware explica isso. Aqui a configuracao vai
rem   junto, escrita na hora, e por isso nao pode divergir.
rem ============================================================
cd /d "%~dp0"
title Mini Smart City - broker MQTT
echo.
echo ============================================================
echo   BROKER MQTT DA MAQUETE
echo ============================================================
echo.

rem ---------- 1) achar o mosquitto ----------
set "MOSQ=C:\Program Files\mosquitto\mosquitto.exe"
if not exist "!MOSQ!" (
  for /f "delims=" %%i in ('where mosquitto.exe 2^>nul') do set "MOSQ=%%i"
)
if not exist "!MOSQ!" (
  echo   [X] mosquitto.exe nao encontrado.
  echo       Procurei em "C:\Program Files\mosquitto\" e no PATH.
  echo       Instale de https://mosquitto.org/download/ e rode de novo.
  echo.
  pause
  exit /b 1
)
echo   broker  : !MOSQ!

rem ---------- 2) derrubar instancia anterior ----------
rem Sem pipe para "find" ou "findstr": se este .cmd for aberto de um shell
rem cujo PATH tem utilitarios Unix (Git Bash), "find" nao e o do Windows e o
rem teste falha. O taskkill ja informa, pelo errorlevel, se havia algo.
taskkill /f /im mosquitto.exe >nul 2>&1
if not errorlevel 1 (
  echo   aviso   : havia um mosquitto rodando - derrubado, porque
  echo             provavelmente era o que escuta so no loopback.
  rem da tempo de a porta 1883 ser liberada antes de reabrir
  ping -n 2 127.0.0.1 >nul
)

rem ---------- 3) esperar a porta 1883 ficar livre ----------
rem Se um mosquitto anterior sobreviver ao taskkill, ou a porta ficar em
rem TIME_WAIT, o novo morre no bind - e o log do mosquitto para isso e uma
rem linha de erro do Windows no meio de dez linhas de "support available",
rem que nao explica nada a quem esta montando a maquete.
set TENTA=0
:esperaporta
set "OCUPADA="
for /f "delims=" %%i in ('powershell -NoProfile -Command "$c=Get-NetTCPConnection -LocalPort 1883 -State Listen -ErrorAction SilentlyContinue; if($c){ 'sim' }" 2^>nul') do set "OCUPADA=%%i"
if defined OCUPADA (
  set /a TENTA+=1
  if !TENTA! GEQ 6 (
    echo.
    echo   [X] a porta 1883 continua ocupada depois de 5 tentativas.
    echo       Quem esta com ela:
    powershell -NoProfile -Command "Get-NetTCPConnection -LocalPort 1883 -State Listen | ForEach-Object { '          ' + (Get-Process -Id $_.OwningProcess).ProcessName + '  PID ' + $_.OwningProcess }"
    echo       Feche esse processo e rode este script de novo.
    echo.
    pause
    exit /b 1
  )
  echo   porta   : 1883 ocupada, esperando liberar... ^(!TENTA!/5^)
  ping -n 2 127.0.0.1 >nul
  goto esperaporta
)
echo   porta   : 1883 livre

rem ---------- 4) escrever a configuracao ----------
rem Escrita a cada execucao, no TEMP: nao tem como ficar desatualizada
rem nem depender de alguem ter editado o mosquitto.conf de 51 KB que
rem vem na instalacao.
set "CONF=%TEMP%\maquete_mosquitto.conf"
> "!CONF!" echo # gerado por broker.cmd - nao editar, e reescrito a cada execucao
>>"!CONF!" echo listener 1883 0.0.0.0
>>"!CONF!" echo allow_anonymous true
>>"!CONF!" echo # sem persistencia: as retidas de /status sao o par do last will e
>>"!CONF!" echo # as placas republicam ao conectar, entao estado velho em disco so
>>"!CONF!" echo # confundiria o painel depois de um restart do broker.
>>"!CONF!" echo persistence false
>>"!CONF!" echo connection_messages true
>>"!CONF!" echo log_timestamp true
echo   conf    : !CONF!

rem ---------- 5) IP desta maquina x o que a placa procura ----------
echo.
echo   ----- enderecos -----
set "IPLAN="
for /f "delims=" %%i in ('powershell -NoProfile -Command "$t=Get-NetIPAddress -AddressFamily IPv4; foreach($x in $t){ if($x.IPAddress -notlike '127.*' -and $x.IPAddress -notlike '169.254.*'){ $x.IPAddress; break } }" 2^>nul') do set "IPLAN=%%i"
if defined IPLAN (
  echo   este PC : !IPLAN!   ^<- e o que tem de estar no secrets.h das placas
) else (
  echo   este PC : nao consegui descobrir o IPv4 ^(sem rede?^)
)

set "IPSEC="
for /f "delims=" %%i in ('powershell -NoProfile -Command "$m=Select-String -Path 'node_sinaleiras\secrets.h' -Pattern 'MQTT_HOST\s+.([0-9.]+)'; if($m){ $m[0].Matches[0].Groups[1].Value }" 2^>nul') do set "IPSEC=%%i"
if defined IPSEC (
  echo   secrets : !IPSEC!   ^(node_sinaleiras/secrets.h^)
  if defined IPLAN (
    if /i "!IPSEC!"=="!IPLAN!" (
      echo   batem   : OK
    ) else (
      echo.
      echo   [ATENCAO] OS DOIS NAO BATEM. A placa vai procurar !IPSEC! e o broker
      echo       esta em !IPLAN! - e o endereco muda sozinho, porque e DHCP.
      echo       Corrija o MQTT_HOST no secrets.h de cada placa
      echo       e regrave as placas.
      echo       Uma reserva de DHCP para este PC
      echo       no roteador resolve de vez.
    )
  )
)
rem ---------- 5b) a REDE do PC x a rede do secrets.h ----------
rem IP igual nao basta: se o PC e a placa estiverem em redes diferentes, o
rem endereco nem chega a importar. Comparar o SSID separa os dois casos.
set "SSIDPC="
for /f "delims=" %%i in ('powershell -NoProfile -Command "$w=netsh wlan show interfaces; foreach($l in $w){ if($l -match '^\s*SSID\s*:\s*(.+)$'){ $matches[1].Trim(); break } }" 2^>nul') do set "SSIDPC=%%i"
set "SSIDSEC="
for /f "delims=" %%i in ('powershell -NoProfile -Command "$q=[char]34; $m=(Select-String -Path 'node_sinaleiras\secrets.h' -Pattern 'WIFI_SSID' ^| Select-Object -First 1).Line; if($m){ ($m -split $q)[1] }" 2^>nul') do set "SSIDSEC=%%i"
if defined SSIDPC if defined SSIDSEC (
  echo   rede PC : !SSIDPC!
  echo   rede sec: !SSIDSEC!
  if /i "!SSIDPC!"=="!SSIDSEC!" (
    echo   rede    : OK, a mesma
  ) else (
    echo.
    echo   [ATENCAO] O PC ESTA EM OUTRA REDE. A placa procura a rede "!SSIDSEC!"
    echo       e este computador esta em "!SSIDPC!". Em redes diferentes nao ha
    echo       endereco que resolva - nem o broker nem o painel vao ver a placa.
    echo       Ponha os dois na mesma rede e, se o IP mudar, corrija
    echo       o MQTT_HOST no secrets.h de cada placa.
    echo.
    echo       Rede de faculdade ^(eduroam^) nao serve: ela e WPA2-Enterprise, e o
    echo       firmware usa WiFi.begin^(ssid, senha^), que so faz WPA2-Personal.
    echo       Alem disso essas redes isolam os aparelhos entre si. Para a
    echo       aula, use o hotspot do celular ou um roteador proprio.
  )
)

echo   Node-RED: use "localhost" no broker1, nunca o IP - ele roda aqui
echo             do lado e assim nao quebra quando o IP mudar.

rem ---------- 6) subir ----------
echo.
echo   ----- log do mosquitto ^(Ctrl+C para parar^) -----
echo.
"!MOSQ!" -c "!CONF!" -v

rem ---------- 7) so chega aqui se o broker morreu ----------
echo.
echo ============================================================
echo   O broker PAROU.
echo ============================================================
echo   Se ele nem subiu, o motivo mais comum e a porta 1883 ja
echo   estar em uso por outro mosquitto. Veja com:
echo       netstat -ano ^| findstr :1883
echo.
echo   Se as placas continuam sem conectar mesmo com o broker no ar,
echo   falta liberar a porta no firewall. Em um Prompt como
echo   ADMINISTRADOR, uma vez so:
echo       netsh advfirewall firewall add rule name="Mosquitto MQTT 1883" dir=in action=allow protocol=TCP localport=1883
echo.
pause
