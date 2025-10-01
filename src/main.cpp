// ESP32 + R305 + SH1106 OLED (I2C) – Serie + Nombres persistentes
// ---------------------------------------------------------------
// Pines sensor: TX R305 -> GPIO25 (RX ESP32)
//               RX R305 <- GPIO26 (TX ESP32)
// OLED I2C: SDA=21, SCL=22, addr 0x3C (SH1106 1.3")
// Serie: 115200

#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Fingerprint.h>

// ====== Config pines ======
static const int PIN_RX = 25;   // UART2 RX (desde TX R305)
static const int PIN_TX = 26;   // UART2 TX (hacia RX R305)

// ====== Seriales y sensor ======
HardwareSerial FingerSerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FingerSerial);

// ====== OLED SH1106 128x64 ======
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ====== NVS (nombres por ID) ======
Preferences prefs; // namespace: "users", key: "id%03u"

// --- Offset típico en SH1106 (ajustalo si ves el dibujo corrido) ---
static const int XOFF = 2;   // probá 0, 2 o 4 según tu módulo

// Huella 32x32 (1bpp, MSB-first) — ideal cuando también mostramos texto


// ====== Utils ======
const char* fp_err(uint8_t code) {
  switch (code) {
    case FINGERPRINT_OK:               return "OK";
    case FINGERPRINT_PACKETRECIEVEERR: return "packet";
    case FINGERPRINT_NOFINGER:         return "no_finger";
    case FINGERPRINT_IMAGEFAIL:        return "image_fail";
    case FINGERPRINT_IMAGEMESS:        return "image_mess";
    case FINGERPRINT_FEATUREFAIL:      return "feature_fail";
    case FINGERPRINT_INVALIDIMAGE:     return "invalid_image";
    case FINGERPRINT_ENROLLMISMATCH:   return "enroll_mismatch";
    case FINGERPRINT_BADLOCATION:      return "bad_location";
    case FINGERPRINT_DBCLEARFAIL:      return "db_clear_fail";
    case FINGERPRINT_UPLOADFEATUREFAIL:return "upload_feature_fail";
    case FINGERPRINT_PACKETRESPONSEFAIL:return "packet_resp_fail";
    case FINGERPRINT_TEMPLATECOUNT:    return "templatecount_err";
    default:                           return "unknown";
  }
}

void printHelp() {
  Serial.println();
  Serial.println(F("Comandos:"));
  Serial.println(F("  e <id>         Enrolar en ID (0..999)"));
  Serial.println(F("  s              Match 1:N manual"));
  Serial.println(F("  d <id>         Borrar ID"));
  Serial.println(F("  c              Contar plantillas"));
  Serial.println(F("  x              Vaciar base"));
  Serial.println(F("  i              Info (ReadSysPara)"));
  Serial.println(F("  n <id> <nombre>  Setear nombre para ID"));
  Serial.println();
}

// ====== Nombres en NVS ======
String getName(uint16_t id) {
  char key[8]; snprintf(key, sizeof(key), "id%03u", id);
  String val = prefs.getString(key, "");
  return val;
}
void setName(uint16_t id, const String& name) {
  char key[8]; snprintf(key, sizeof(key), "id%03u", id);
  prefs.putString(key, name);
}

// ====== OLED: helpers ======
inline void oled_clear() { display.clearDisplay(); }
inline void oled_show()  { display.display(); }

// Dibuja huella 32x32 centrada abajo del texto
static inline void draw_fp32_centered(int y = 30) {
  int x = ((SCREEN_WIDTH - 32) / 2) + XOFF;
  display.drawBitmap(x, y, FP_32x32, 32, 32, 1);
}

void oled_center(const String& l1, const String& l2="") {
  oled_clear();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);

  int16_t x1,y1; uint16_t w,h;
  display.getTextBounds(l1, 0,0,&x1,&y1,&w,&h);
  int x = (SCREEN_WIDTH - w)/2;
  display.setCursor(x, 6);
  display.println(l1);

  if (l2.length()) {
    display.getTextBounds(l2, 0,0,&x1,&y1,&w,&h);
    x = (SCREEN_WIDTH - w)/2;
    display.setCursor(x, 6+14);
    display.println(l2);
  }

  draw_fp32_centered(30);
  oled_show();
}

// Blink simple de la huella mientras captura (sin usar spinner)
static void oled_scanBlinkTick(bool on) {
  // Borro zona del icono y redibujo según 'on'
  display.fillRect(0, 28, SCREEN_WIDTH, 36, SH110X_BLACK);
  if (on) draw_fp32_centered(30);
  oled_show();
}

// Pantallas de estado
void oled_idle()       { oled_center("Ponga su", "huella"); }
void oled_scanning()   { oled_center("Escaneando", "mantener..."); }
void oled_ok_msg(const String& l2="Lectura correcta") {
  oled_clear();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(26, 4); display.println("OK");
  display.setTextSize(1);
  display.setCursor(0, 24); display.println(l2);
  draw_fp32_centered(30);
  oled_show();
}
void oled_error(const String& msg) {
  oled_clear();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(6, 4); display.println("ERROR");
  display.setTextSize(1);
  display.setCursor(0, 24); display.println(msg);
  draw_fp32_centered(30);
  oled_show();
}
void oled_bienvenido(const String& nombre, uint16_t id, int score) {
  oled_clear();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);  display.println("Acceso concedido");
  display.setTextSize(2);
  display.setCursor(0, 16);
  if (nombre.length()) display.println("Bienvenido");
  else                 display.println("ID OK");
  display.setTextSize(1);
  display.setCursor(0, 36);
  if (nombre.length()) {
    display.print(nombre);
    display.print(" (ID "); display.print(id); display.println(")");
  } else {
    display.print("ID "); display.println(id);
  }
  display.setCursor(0, 48);
  display.print("Score: "); display.println(score);
  display.drawBitmap(96, 28, FP_32x32, 32, 32, 1);
  oled_show();
}

// ====== Sensor helpers ======
// Captura a buffer con BLINK de huella durante el loop de espera
bool captureToBuffer(uint8_t buf) {
  // esperar dedo fuera
  unsigned long t0=millis();
  while (finger.getImage()!=FINGERPRINT_NOFINGER && millis()-t0<1500) delay(40);

  // pedir dedo con blink
  unsigned long t1 = millis();
  bool blink = false, lastBlink = false;
  unsigned long lastToggle = 0;

  while (true) {
    // toggle ~cada 180 ms
    unsigned long now = millis();
    if (now - lastToggle > 180) {
      lastToggle = now;
      blink = !blink;
      oled_scanBlinkTick(blink);
    }

    uint8_t rc = finger.getImage();
    if (rc == FINGERPRINT_OK) {
      // dejar la huella encendida antes de seguir
      if (!lastBlink) { oled_scanBlinkTick(true); lastBlink=true; }
      break;
    }
    if (rc != FINGERPRINT_NOFINGER) {
      // ruido: seguimos intentando
    }
    if (millis() - t1 > 15000) { oled_error("Tiempo agotado"); return false; }
    delay(20);
  }

  uint8_t rc = finger.image2Tz(buf);
  if (rc != FINGERPRINT_OK) {
    oled_error(String("image2Tz: ")+fp_err(rc));
    return false;
  }

  // retirar dedo (sin blink)
  unsigned long t2=millis();
  while (finger.getImage()!=FINGERPRINT_NOFINGER && millis()-t2<1500) delay(40);
  return true;
}

bool enroll(uint16_t id) {
  if (id>999) { Serial.println("ID fuera de rango"); return false; }
  Serial.println(F("[ENROLL] Toma 1/2"));
  oled_scanning();
  if (!captureToBuffer(1)) { Serial.println("Falló captura(1)"); return false; }

  delay(500);
  Serial.println(F("[ENROLL] Toma 2/2"));
  oled_scanning();
  if (!captureToBuffer(2)) { Serial.println("Falló captura(2)"); return false; }

  uint8_t rc = finger.createModel();
  if (rc != FINGERPRINT_OK) { oled_error("No coinciden"); Serial.print("createModel: "); Serial.println(fp_err(rc)); return false; }

  rc = finger.storeModel(id);
  if (rc != FINGERPRINT_OK) { oled_error("No guarda"); Serial.print("storeModel: "); Serial.println(fp_err(rc)); return false; }

  oled_ok_msg(String("ID ")+id);
  Serial.println(F("Enrolamiento OK"));
  return true;
}

struct MatchRes { bool ok; int id; int score; };
MatchRes doMatch() {
  MatchRes r{false, -1, 0};
  oled_scanning();
  if (!captureToBuffer(1)) return r;
  if (finger.fingerFastSearch() == FINGERPRINT_OK) {
    r.ok = true; r.id = finger.fingerID; r.score = finger.confidence;
  }
  return r;
}

// ====== Autodetección de baud ======
uint32_t detectedBaud=0;
bool tryAt(uint32_t b) {
  FingerSerial.updateBaudRate(b);
  delay(40);
  finger.begin(b);
  delay(40);
  return finger.verifyPassword();
}
void autoDetectBaud() {
  uint32_t bauds[] = {57600,115200,38400,19200};
  for (uint8_t i=0;i<sizeof(bauds)/sizeof(bauds[0]);++i) {
    if (tryAt(bauds[i])) { detectedBaud=bauds[i]; return; }
  }
  detectedBaud=0;
}

// ====== Loop de espera con auto-match ======
unsigned long lastAutoMatch = 0;
const unsigned long autoMatchEveryMs = 800; // cada ~0.8s intenta

void idleWithAnimationAndAutoMatch() {
  static bool first = true;
  if (first) { oled_idle(); first=false; }

  // (spinner desactivado)

  unsigned long now = millis();
  if (now - lastAutoMatch > autoMatchEveryMs) {
    lastAutoMatch = now;
    if (finger.getImage() == FINGERPRINT_NOFINGER) return; // nada que hacer
    MatchRes m = doMatch();
    if (m.ok) {
      String name = getName(m.id);
      oled_bienvenido(name, m.id, m.score);
      Serial.print(F("Match OK -> ID=")); Serial.print(m.id);
      Serial.print(F(" score=")); Serial.println(m.score);
      delay(1700);
      oled_idle();
    } else {
      oled_error("Sin coincidencia");
      delay(900);
      oled_idle();
    }
  }
}

// ====== Serial parsing ======
String readLineWithTimeout(uint32_t ms) {
  String s;
  unsigned long t0=millis();
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

void handleSerialCommand(const String& line) {
  if (line=="s") {
    Serial.println(F("Apoyá el dedo..."));
    MatchRes m = doMatch();
    if (m.ok) {
      String name = getName(m.id);
      oled_bienvenido(name, m.id, m.score);
      Serial.print(F("Match ID=")); Serial.print(m.id);
      Serial.print(F(" score=")); Serial.println(m.score);
      delay(1500);
      oled_idle();
    } else {
      oled_error("Sin coincidencia");
      Serial.println(F("No se encontró match"));
      delay(900);
      oled_idle();
    }
    return;
  }
  if (line=="c") { if (finger.getTemplateCount()==FINGERPRINT_OK) Serial.println(finger.templateCount); else Serial.println("ERR"); return; }
  if (line=="x") { Serial.println(finger.emptyDatabase()==FINGERPRINT_OK ? "OK":"ERR"); return; }
  if (line=="i") {
    if (finger.getParameters()==FINGERPRINT_OK) {
      Serial.print("capacity="); Serial.println(finger.capacity);
      Serial.print("security="); Serial.println(finger.security_level);
      Serial.print("system_id=0x"); Serial.println(finger.system_id, HEX);
      Serial.print("baud="); Serial.println(finger.baud_rate);
      Serial.print("packet_len="); Serial.println(finger.packet_len);
    } else Serial.println("getParameters FAIL");
    return;
  }
  if (line.startsWith("d ")) {
    uint16_t id = line.substring(2).toInt();
    Serial.println(finger.deleteModel(id)==FINGERPRINT_OK ? "OK":"ERR");
    return;
  }
  if (line.startsWith("n ")) {
    int sp = line.indexOf(' ', 2);
    if (sp<0) { Serial.println("Uso: n <id> <nombre>"); return; }
    uint16_t id = line.substring(2, sp).toInt();
    String name = line.substring(sp+1); name.trim();
    setName(id, name);
    Serial.println("Nombre guardado");
    return;
  }
  if (line.startsWith("e ")) {
    uint16_t id = line.substring(2).toInt();
    Serial.print("Enrolando ID "); Serial.println(id);
    if (enroll(id)) {
      Serial.print("Ingresá nombre para ID "); Serial.print(id); Serial.println(": ");
      String name = readLineWithTimeout(30000);
      if (name.length()) { setName(id, name); Serial.println("Nombre guardado."); }
      else Serial.println("Sin nombre (timeout).");
    } else {
      Serial.println("Enrolamiento falló.");
    }
    oled_idle();
    return;
  }
  Serial.println("Comando no reconocido.");
  printHelp();
}

// ====== setup/loop ======
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[ESP32+R305 SH1106 Serie]");

  // OLED
  Wire.begin(21,22);
  if (!display.begin(OLED_ADDR, true)) {
    Serial.println("OLED no encontrado (0x3C?)");
  } else {
    display.clearDisplay(); display.display();
    oled_idle();
  }

  // NVS
  prefs.begin("users", false);

  // UART sensor + autodetección
  FingerSerial.begin(57600, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(60);
  autoDetectBaud();
  if (!detectedBaud) {
    Serial.println("ERROR: sin handshake R305. Revisá cableado/5V/GND.");
    oled_error("Sin handshake");
  } else {
    Serial.print("R305 baud: "); Serial.println(detectedBaud);
    finger.setPacketSize(FINGERPRINT_PACKET_SIZE_32); // si no aplica, se ignora
    if (finger.getParameters()==FINGERPRINT_OK) Serial.println("getParameters OK");
    else Serial.println("getParameters FAIL (no crítico)");
  }

  printHelp();
}

void loop() {
  // Espera + auto-match
  idleWithAnimationAndAutoMatch();

  // CLI por Serial (no bloqueante)
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n'); line.trim();
    if (line.length()) handleSerialCommand(line);
  }
}
