// ESP32 + R305 + SH1106 OLED – Serie + Nombres persistentes (versión MODELOS + 64x64)

#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <Adafruit_Fingerprint.h>

#include "DisplayModel.h"      // UI SH1106 (usa 64x64 a la derecha)
#include "FingerprintModel.h"  // R305 (baud autodetect, enroll, match)
#include "NamesModel.h"        // NVS nombres por ID

// ====== Config pines ======
static const int PIN_RX = 25;   // UART2 RX (desde TX R305)
static const int PIN_TX = 26;   // UART2 TX (hacia RX R305)

// ====== Instancias de modelos ======
HardwareSerial   FingerSerial(2);
DisplayModel     displayModel(&Wire, /*xoffset=*/2);
FingerprintModel fpModel(FingerSerial, PIN_RX, PIN_TX);
NamesModel       names;

// ====== Utilidades CLI ======
static void printHelp() {
  Serial.println();
  Serial.println(F("Comandos:"));
  Serial.println(F("  e <id>           Enrolar en ID (0..999)"));
  Serial.println(F("  s                Match 1:N manual"));
  Serial.println(F("  d <id>           Borrar ID"));
  Serial.println(F("  c                Contar plantillas"));
  Serial.println(F("  x                Vaciar base"));
  Serial.println(F("  i                Info (ReadSysPara)"));
  Serial.println(F("  n <id> <nombre>  Setear nombre para ID"));
  Serial.println();
}

static String readLineWithTimeout(uint32_t ms) {
  String s; unsigned long t0=millis();
  while (millis()-t0 < ms) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c=='\r') continue;
      if (c=='\n') break;
      s += c;
    }
    delay(1);
  }
  s.trim();
  return s;
}

// ====== Loop de espera con auto-match ======
unsigned long lastAutoMatch = 0;
const unsigned long autoMatchEveryMs = 800; // ~0.8s

// ====== Loop de espera con auto-match (FSM no bloqueante con blink) ======
// ====== Estado de automatch sin throttling ======
// ====== Automatch con blink + retención mínima ======
// ===== Blink asimétrico (más lento) =====
static bool blinkOn = false;
static unsigned long nextBlinkAt = 0;
static const unsigned long BLINK_ON_MS  = 700;  // encendido más tiempo
static const unsigned long BLINK_OFF_MS = 500;  // apagado más corto

// Timers/estado (si no los tenés ya)
enum class AutoState { WAIT_FINGER, MATCHING, COOLDOWN };
static AutoState autoState = AutoState::WAIT_FINGER;
static unsigned long cooldownUntil = 0;
static unsigned long scanStart = 0;
static const unsigned long MIN_SCAN_MS = 4000;    // tu valor preferido
static unsigned long matchingDeadline = 0;

static bool resultReady = false;
static bool resultOk = false;
static int  resultId = -1;
static int  resultScore = 0;
static unsigned long showResultAt = 0;

void idleWithAnimationAndAutoMatch() {
  unsigned long now = millis();

  switch (autoState) {
    case AutoState::WAIT_FINGER: {
      displayModel.idle();

      if (fpModel.chip().getImage() != FINGERPRINT_NOFINGER) {
        displayModel.scanning();
        blinkOn = true;
        displayModel.scanBlinkTick(true);
        nextBlinkAt = now + BLINK_ON_MS;   // ⬅️ primer vencimiento (ON)

        scanStart = now;
        matchingDeadline = now + 15000;
        resultReady = false;

        autoState = AutoState::MATCHING;
      }
      break;
    }

    case AutoState::MATCHING: {
      // Blink asimétrico por tiempo
      if ((long)(now - nextBlinkAt) >= 0) {
        blinkOn = !blinkOn;
        displayModel.scanBlinkTick(blinkOn);
        nextBlinkAt = now + (blinkOn ? BLINK_ON_MS : BLINK_OFF_MS);
      }

      // ¿ya hay resultado y pasó el mínimo de animación?
      if (resultReady) {
        if ((long)(now - showResultAt) >= 0) {
          blinkOn = false;
          if (resultOk) {
            String name = names.get(resultId);
            displayModel.welcome(name, resultId, resultScore);
          } else {
            displayModel.errorMsg("Sin coincidencia");
          }
          cooldownUntil = now + 1500;
          autoState = AutoState::COOLDOWN;
        }
        break;
      }

      // Timeout de seguridad
      if ((long)(now - matchingDeadline) >= 0) {
        blinkOn = false;
        displayModel.errorMsg("Tiempo agotado");
        cooldownUntil = now + 1200;
        autoState = AutoState::COOLDOWN;
        break;
      }

      // Captura NO bloqueante
      uint8_t gi = fpModel.chip().getImage();
      if (gi != FINGERPRINT_OK) break;

      // Convertir y buscar (una vez)
      if (fpModel.chip().image2Tz(1) != FINGERPRINT_OK) {
        resultOk = false; resultId = -1; resultScore = 0;
        resultReady = true;
        showResultAt = max(now, scanStart + MIN_SCAN_MS);
        break;
      }

      bool ok = (fpModel.chip().fingerFastSearch() == FINGERPRINT_OK);
      resultOk    = ok;
      resultId    = ok ? fpModel.chip().fingerID    : -1;
      resultScore = ok ? fpModel.chip().confidence : 0;
      resultReady = true;
      showResultAt = max(now, scanStart + MIN_SCAN_MS);
      break;
    }

    case AutoState::COOLDOWN: {
      if ((long)(now - cooldownUntil) >= 0) {
        autoState = AutoState::WAIT_FINGER;
      }
      break;
    }
  }
}


// ====== Comandos por Serial ======
static void handleSerialCommand(const String& line) {
  if (line=="s") {
  Serial.println(F("Apoyá el dedo..."));

  // 👇 NUEVO
  displayModel.scanning();

  auto blinkCb = [](bool on){ displayModel.scanBlinkTick(on); };
  MatchRes m = fpModel.fastMatch(blinkCb);
  if (m.ok) {
    String name = names.get(m.id);
    displayModel.welcome(name, m.id, m.score);
    Serial.print(F("Match ID=")); Serial.print(m.id);
    Serial.print(F(" score=")); Serial.println(m.score);
    delay(1500);
    displayModel.idle();
  } else {
    displayModel.errorMsg("Sin coincidencia");
    Serial.println(F("No se encontró match"));
    delay(900);
    displayModel.idle();
  }
  return;
}

  if (line=="c") {
    if (fpModel.chip().getTemplateCount()==FINGERPRINT_OK)
      Serial.println(fpModel.chip().templateCount);
    else Serial.println("ERR");
    return;
  }

  if (line=="x") {
    Serial.println(fpModel.chip().emptyDatabase()==FINGERPRINT_OK ? "OK":"ERR");
    return;
  }

  if (line=="i") {
    if (fpModel.chip().getParameters()==FINGERPRINT_OK) {
      Serial.print("capacity="); Serial.println(fpModel.chip().capacity);
      Serial.print("security="); Serial.println(fpModel.chip().security_level);
      Serial.print("system_id=0x"); Serial.println(fpModel.chip().system_id, HEX);
      Serial.print("baud="); Serial.println(fpModel.chip().baud_rate);
      Serial.print("packet_len="); Serial.println(fpModel.chip().packet_len);
    } else Serial.println("getParameters FAIL");
    return;
  }

  if (line.startsWith("d ")) {
    uint16_t id = line.substring(2).toInt();
    Serial.println(fpModel.chip().deleteModel(id)==FINGERPRINT_OK ? "OK":"ERR");
    return;
  }

  if (line.startsWith("n ")) {
    int sp = line.indexOf(' ', 2);
    if (sp<0) { Serial.println("Uso: n <id> <nombre>"); return; }
    uint16_t id = line.substring(2, sp).toInt();
    String name = line.substring(sp+1); name.trim();
    names.set(id, name);
    Serial.println("Nombre guardado");
    return;
  }

  if (line.startsWith("e ")) {
  uint16_t id = line.substring(2).toInt();
  Serial.print("Enrolando ID "); Serial.println(id);

  // 👇 NUEVO: leyenda antes de la 1ª captura
  displayModel.scanning();
  auto blinkCb = [](bool on){ displayModel.scanBlinkTick(on); };

  if (fpModel.enroll(id, blinkCb)) {
    Serial.print("Ingresá nombre para ID "); Serial.print(id); Serial.println(": ");
    String name = readLineWithTimeout(30000);
    if (name.length()) { names.set(id, name); Serial.println("Nombre guardado."); }
    else Serial.println("Sin nombre (timeout).");
    displayModel.okMsg(String("ID ")+id);
  } else {
    displayModel.errorMsg("Enrolamiento falló");
    Serial.println("Enrolamiento falló.");
  }
  displayModel.idle();
  return;
}
  Serial.println("Comando no reconocido.");
  printHelp();
}

// ====== setup/loop ======
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[ESP32+R305 SH1106 Serie – MODELOS+64x64]");

  // I2C + OLED
  Wire.begin(21,22);
  if (!displayModel.begin()) {
    Serial.println("OLED no encontrado (0x3C?)");
  }

  // NVS nombres
  names.begin();

  // UART sensor + autodetección
  fpModel.begin(57600);
  if (!fpModel.ready()) {
    Serial.println("ERROR: sin handshake R305. Revisá cableado/5V/GND.");
    displayModel.errorMsg("Sin handshake");
  } else {
    Serial.print("R305 baud: "); Serial.println(fpModel.detectedBaud());
    Serial.println(fpModel.chip().getParameters()==FINGERPRINT_OK
                   ? "getParameters OK" : "getParameters FAIL (no crítico)");
  }

  printHelp();
}

void loop() {
  idleWithAnimationAndAutoMatch();  // cada frame, sin delays largos

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n'); line.trim();
    if (line.length()) handleSerialCommand(line);
  }
}

