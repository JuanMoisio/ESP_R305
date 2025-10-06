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

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println("\n[ESP32 + R305 + SH1106] – inicio");

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

  autoMode.begin();
  printHelp();
}

// ===== Loop =====
void loop() {
  autoMode.tick();  // corre la máquina de estados (no bloquea)

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n'); line.trim();
    if (line.length()) {
      handleSerialCommand(line, displayModel, fpModel, names, autoMode);
    }
  }
}
