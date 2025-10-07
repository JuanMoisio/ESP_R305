# HuellaDactilar — Firmware ESP32 + R305 + SH1106

Descripción
- Firmware para ESP32 que integra lector de huellas R305, pantalla SH1106 y servidor web SSE.
- Modo principal: espera comandos (standby). Recibe órdenes por Serial (CLI) o por HTTP API y emite eventos vía Server‑Sent Events (/fp/events).
- UI local muestra estado, animación de escaneo y una barra de “láser” sobre la imagen de la huella.

Hardware
- ESP32
- Lector R305 por UART (UART2: RX=25, TX=26)
- OLED SH1106 por I2C (SDA=21, SCL=22, addr=0x3C)

Compilación y subida
- Repositorio basado en PlatformIO.
- Dependencias (platformio.ini): AsyncTCP, ESPAsyncWebServer, Adafruit SH1106 libs, Adafruit Fingerprint.
- Desde VS Code: PlatformIO → Build / Upload.
- Desde terminal:
  - pio run
  - pio run -t upload
  - pio device monitor -b 115200

Comportamiento al arrancar
- El dispositivo arranca en modo standby y muestra “Waiting command”.
- No inicia escaneo hasta recibir comando por Serial o API.

Serial CLI (consola serie, comandos útiles)
- e <id>         — Enrolar huella en ID (0..999)
- s              — Solicitar match 1:N (lanza petición de escaneo)
- d <id>         — Borrar plantilla ID
- c              — Contar plantillas
- x              — Vaciar base de datos
- i              — Info del sensor (ReadSysPara)
- n <id> <nombre>— Setear nombre para ID
- ok / err / panel— Pruebas UI (muestran pantallas de OK / Error / Panel)
- anim           — Ejecutar animación de huellas 5s

HTTP API
- Landing:
  - GET /fp
- Comando (simple, por querystring):
  - GET /fp/command?action=scan
    - Pide escaneo (emite evento prompt y encola petición para AutoMode)
  - GET /fp/command?action=enrollStart
  - GET /fp/command?action=enrollAbort
  - GET /fp/command?action=erase&id=<id>
  - GET /fp/command?action=status
    - Devuelve JSON con estado básico
- SSE (eventos en tiempo real):
  - /fp/events
  - Eventos emitidos:
    - event "prompt"  — {"event":"prompt","msg":"Ponga su huella"}
    - event "result"  — {"event":"result","ok":true|false,"id":N,"score":S}
    - event "enroll"  — etapas: start/abort/result
    - event "erase"   — request/result

Ejemplos (reemplazar <IP> por la IP del dispositivo)
- Landing (navegador): http://<IP>/fp
- Encolar scan:
  - curl "http://<IP>/fp/command?action=scan"
- Escuchar eventos (terminal):
  - curl -sN "http://<IP>/fp/events"
- Escuchar eventos (navegador consola):
  ```javascript
  const es = new EventSource('http://<IP>/fp/events');
  es.addEventListener('prompt', e => console.log('PROMPT', e.data));
  es.addEventListener('result', e => console.log('RESULT', e.data));
  ```

Integración con AutoMode
- El flujo de escaneo fue cambiado para que AutoMode solo entre en MATCHING cuando se consume una petición (serial o API) — evita que el dispositivo pida huella automáticamente al detectar el dedo.
- Para solicitar un scan desde otra parte del firmware llamar a `requestScan()` (implementado en ScanRequest).

Notas de depuración
- Ver logs por puerto serie 115200.
- Si no aparecen eventos SSE, confirmar:
  - Wi‑Fi conectado
  - servidor HTTP inicializado (mensaje "HTTP server iniciado" en serie)
  - fpApiLoop() ejecutado periódicamente en loop() (envía eventos encolados)

Archivos principales
- src/main.cpp
- include/AutoMode.h
- src/FingerprintApi.cpp, include/FingerprintApi.h
- include/ScanRequest.h, src/ScanRequest.cpp
- include/Config.h (credenciales WIFI_SSID / WIFI_PASS)
- DisplayModel.*, FingerprintModel.*, NamesModel.*

Contacto rápido
- Para pruebas rápidas usa:
  - curl "http://192.168.xxx.xxx/fp/command?action=scan"
  - curl -sN "http://192.168.xxx.xxx/fp/events"

Licencia
- Código de ejemplo / proyecto personal. Ajustar según corresponda.
