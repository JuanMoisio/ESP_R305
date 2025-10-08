// main.cpp — ESP32 + R305 + SH1106 (máquina de estados + CLI modular)

#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <Adafruit_Fingerprint.h>

#include "DisplayModel.h"
#include "FingerprintModel.h"
#include "NamesModel.h"

#include "AutoMode.h"   // máquina de estados (UI + match en background)
#include "SerialCli.h"  // comandos por Serial
#include "FingerprintApi.h"
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include "Config.h"

// ===== Pines / OLED =====
static const int PIN_RX = 25;    // UART2 RX (desde TX del R305)
static const int PIN_TX = 26;    // UART2 TX (hacia RX del R305)

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C

// ===== Instancias globales =====
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
HardwareSerial   FingerSerial(2);

DisplayModel     displayModel(display, /*xoffset=*/2);
FingerprintModel fpModel(FingerSerial, PIN_RX, PIN_TX);
NamesModel       names;
AutoMode         autoMode(displayModel, fpModel, names);

// server deferred until WiFi connected
static AsyncWebServer* serverPtr = nullptr;
static AsyncEventSource* fpEventsPtr = nullptr;
static bool serverStarted = false;

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println("\n[ESP32 + R305 + SH1106] – inicio");

  // Wi-Fi: modo estación y comienzo conexión (usa WIFI_SSID / WIFI_PASS de Config.h)
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  Serial.printf("Conectando a WiFi SSID='%s'...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Escaneo rápido para diagnosticar: lista SSIDs disponibles (no bloquear mucho tiempo)
  int n = WiFi.scanNetworks();
  Serial.printf("ScanNetworks: %d redes encontradas\n", n);
  for (int i = 0; i < n; ++i) {
    Serial.printf("  %d: %s (RSSI %d) ch=%d %s\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "");
  }
  WiFi.scanDelete();

  // I2C + OLED
  Wire.begin(21, 22);
  Wire.setClock(400000);                 // I2C fast
  if (!displayModel.begin(OLED_ADDR)) {
    Serial.println("OLED no encontrado (0x3C?)");
  }

  // Nombres en NVS
  names.begin();

  // UART del sensor + autodetección
  fpModel.begin(57600);
  if (!fpModel.ready()) {
    Serial.println("ERROR: sin handshake R305. Revisá cableado/5V/GND.");
    displayModel.errorMsg("Sin handshake");
  } else {
    Serial.print("R305 baud: "); Serial.println(fpModel.detectedBaud());
    if (fpModel.chip().getParameters()==FINGERPRINT_OK) {
      Serial.println("getParameters OK");
    } else {
      Serial.println("getParameters FAIL (no crítico)");
    }
  }

  // Esperar conexión Wi‑Fi (timeout 10 s) antes de intentar arrancar server
  Serial.println("Esperando WiFi...");
  unsigned long waitUntil = millis() + 10000;
  while (millis() < waitUntil && WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: "); Serial.println(WiFi.localIP());
    // start server now that WiFi is up
    serverPtr = new AsyncWebServer(80);
    fpEventsPtr = new AsyncEventSource("/fp/events");
    initFingerprintApi(*serverPtr, *fpEventsPtr);
    serverPtr->addHandler(fpEventsPtr);
    serverPtr->begin();
    serverStarted = true;
    Serial.println("HTTP server iniciado");
  } else {
    Serial.println("WiFi no conectado: servidor diferido hasta conexión");
    // servidor se iniciará desde loop() cuando haya conexión
  }

  autoMode.begin();
  printHelp();
}

// ===== Loop =====
void loop() {
  autoMode.tick();  // corre la máquina de estados (no bloquea)

  // si el servidor aún no arrancó, intentar iniciar cuando haya Wi‑Fi
  if (!serverStarted && WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado: arrancando HTTP server ahora");
    serverPtr = new AsyncWebServer(80);
    fpEventsPtr = new AsyncEventSource("/fp/events");
    initFingerprintApi(*serverPtr, *fpEventsPtr);
    serverPtr->addHandler(fpEventsPtr);
    serverPtr->begin();
    serverStarted = true;
    Serial.println("HTTP server iniciado (deferred)");
  }

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n'); line.trim();
    if (line.length()) {
      handleSerialCommand(line, displayModel, fpModel, names, autoMode);
    }
  }
  fpApiLoop(); // procesar y enviar eventos pendientes

  // NO dibujar nada aquí: AutoMode gestiona la UI (idle/scanning/matching)
}
