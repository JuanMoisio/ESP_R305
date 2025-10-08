#include "DisplayModel.h"
#include "Bitmaps.h"

// ===== helper con (w, h) — debe ir ANTES de usarlo =====
static inline void drawBitmapAny(Adafruit_SH1106G& disp, int x, int y, const uint8_t* img, int w, int h) {
#if FP_BITMAP_IS_XBM
  disp.drawXBitmap(x, y, img, w, h, 1);                 // XBM no tiene bg
#else
  disp.drawBitmap (x, y, img, w, h, SH110X_WHITE, SH110X_BLACK);  // <-- color y fondo
#endif
}

Adafruit_SH1106G& DisplayModel::raw() { return _display; }

// ---------- init ----------
bool DisplayModel::begin(uint8_t addr, bool reset) {
  if (!_display.begin(addr, reset)) return false;
  _display.clearDisplay();
  _display.display();
  return true;
}


// ---------- dibujos ----------
void DisplayModel::drawFp64Right() {
  const int x = 64 + _xoff;
  const int y = 0;
  drawBitmapAny(_display, x, y, FP_64x64, FP64_W, FP64_H);
}

void DisplayModel::drawFpPhase(uint8_t phase) {
  const int x = 64 + _xoff, y = 0;

  // Secuencia de “respiración”: 25% → 1 → 50% → 75% → 100%
  static const uint8_t* const FRAMES[] = {
    FP_64x64_25,
    FP_64x64_1,     // <- NUEVO frame intermedio
    FP_64x64_50,
    FP_64x64_75,
    FP_64x64        // 100%
  };
  constexpr uint8_t FRAME_COUNT = sizeof(FRAMES)/sizeof(FRAMES[0]);

  if (phase >= FRAME_COUNT) phase = FRAME_COUNT - 1;
  const uint8_t* img = FRAMES[phase];

  
#if FP_BITMAP_IS_XBM
  _display.drawXBitmap(x, y, img, FP64_W, FP64_H, 1);
#else
  drawBitmapAny(_display, x, y, img, FP64_W, FP64_H);
#endif
  _display.display();
}


void DisplayModel::drawOkRight() {
  const int paneX = 64, paneY = 0;
  _display.fillRect(paneX, paneY, 64, 64, SH110X_BLACK);
#if FP_BITMAP_IS_XBM
  _display.drawXBitmap(paneX, paneY, ICON_OK_64, ICON_W, ICON_H, 1);
#else
  _display.drawBitmap (paneX, paneY, ICON_OK_64, ICON_W, ICON_H, 1);
#endif
  _display.display();
}

void DisplayModel::drawErrRight() {
  const int paneX = 64, paneY = 0;
  _display.fillRect(paneX, paneY, 64, 64, SH110X_BLACK);
#if FP_BITMAP_IS_XBM
  _display.drawXBitmap(paneX, paneY, ICON_ERR_64, ICON_W, ICON_H, 1);
#else
  _display.drawBitmap (paneX, paneY, ICON_ERR_64, ICON_W, ICON_H, 1);
#endif
  _display.display();
}


void DisplayModel::scanBlinkTick(bool on) {
  // ON=100%, OFF=limpio (compat con código viejo)
  const int x = 64 + _xoff, y = 0;
  _display.fillRect(x, y, FP64_W, FP64_H, SH110X_BLACK);
  if (on) drawBitmapAny(_display, x, y, FP_64x64, FP64_W, FP64_H);
  _display.display();
}

// ---------- pantallas ----------
void DisplayModel::idle() {
  // nuevo: dibujar logo en idle (ICON_PERMAQUIM_64 debe estar definido en Bitmaps.cpp)
  auto& d = raw();
  d.clearDisplay();
  #if FP_BITMAP_IS_XBM
    d.drawXBitmap(32, 0, ICON_PERMAQUIM_64, ICON_W, ICON_H, SH110X_WHITE);
  #else
    d.drawBitmap(32, 0, ICON_PERMAQUIM_64, ICON_W, ICON_H, SH110X_WHITE);
  #endif
  d.display();
}

void DisplayModel::scanning() {
  _display.clearDisplay();
  _display.setTextColor(SH110X_WHITE);
  _display.setTextSize(1);
  _display.setCursor(0, 8);  _display.println("Escaneando...");
  _display.setCursor(0, 20); _display.println("mantener");
  _display.display();
}

void DisplayModel::welcome(const String& nombre, uint16_t id, int score) {
  _display.clearDisplay();
  _display.setTextColor(SH110X_WHITE);
  _display.setTextSize(1);
  _display.setCursor(0, 0);  _display.println("Acceso concedido");
  _display.setTextSize(2);
  _display.setCursor(0, 16);
  if (nombre.length()) _display.println("Bienvenido");
  else                 _display.println("ID OK");
  _display.setTextSize(1);
  _display.setCursor(0, 36);
  if (nombre.length()) {
    _display.print(nombre);
    _display.print(" (ID "); _display.print(id); _display.println(")");
  } else {
    _display.print("ID "); _display.println(id);
  }
  _display.setCursor(0, 48);
  _display.print("Score: "); _display.println(score);

  // LIMPIEZA EXPLÍCITA DEL PANEL DERECHO + TILDE
  _display.fillRect(64, 0, 64, 64, SH110X_BLACK);
  drawOkRight();
  _display.display();
}

void DisplayModel::errorMsg(const String& msg) {
  _display.clearDisplay();
  _display.setTextColor(SH110X_WHITE);
  _display.setTextSize(2);
  _display.setCursor(6, 4); _display.println("ERROR");
  _display.setTextSize(1);
  _display.setCursor(0, 24); _display.println(msg);

  // LIMPIEZA EXPLÍCITA DEL PANEL DERECHO + CRUZ
  _display.fillRect(64, 0, 64, 64, SH110X_BLACK);
  drawErrRight();
  _display.display();
}

void DisplayModel::okMsg(const String& l2) {
  _display.clearDisplay();
  _display.setTextColor(SH110X_WHITE);
  _display.setTextSize(2);
  _display.setCursor(26, 4); _display.println("OK");
  _display.setTextSize(1);
  _display.setCursor(0, 24); _display.println(l2);
  drawFp64Right();
  _display.display();
}

void DisplayModel::drawFpPhaseLabeled(uint8_t phase, uint8_t label) {
  const int x = 64 + _xoff, y = 0;
  const uint8_t* img =
      (phase >= 3) ? FP_64x64
    : (phase == 2) ? FP_64x64_75
    : (phase == 1) ? FP_64x64_50
                   : FP_64x64_25;

  // panel derecho
  _display.fillRect(x, y, FP64_W, FP64_H, SH110X_BLACK);

  // huella del frame
  #if FP_BITMAP_IS_XBM
    _display.drawXBitmap(x, y, img, FP64_W, FP64_H, 1);
  #else
    _display.drawBitmap (x, y, img, FP64_W, FP64_H, 1);
  #endif

  // badge con número (esquina sup-izquierda del panel)
  _display.fillRect(x+2, y+2, 12, 10, SH110X_WHITE);
  _display.setTextColor(SH110X_BLACK);
  _display.setTextSize(1);
  _display.setCursor(x+4, y+3);
  _display.print(label);  // 1..4

  _display.display();
}
