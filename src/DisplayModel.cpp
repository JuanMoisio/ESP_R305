#include "DisplayModel.h"
#include <Wire.h>
#include "Bitmaps.h"

DisplayModel::DisplayModel(TwoWire *wire, int xoffset)
: _display(SCREEN_W, SCREEN_H, wire, -1), _xoff(xoffset) {}

bool DisplayModel::begin() {
  if (!_display.begin(0x3C, true)) return false;
  _display.clearDisplay(); _display.display();
  idle();
  return true;
}

void DisplayModel::clear() { _display.clearDisplay(); }
void DisplayModel::show()  { _display.display(); }

void DisplayModel::drawFp64Right() {
  const int x = 64 + _xoff;  // mitad derecha
  const int y = 0;
#if FP_BITMAP_IS_XBM
  _display.drawXBitmap(x, y, FP_64x64, FP64_W, FP64_H, 1);
#else
  _display.drawBitmap (x, y, FP_64x64, FP64_W, FP64_H, 1);
#endif
}

void DisplayModel::leftTwoLines(const String& l1, const String& l2) {
  clear();
  _display.setTextColor(SH110X_WHITE);
  _display.setTextSize(1);

  auto printLine = [&](const String& s, int y) {
    int16_t x1,y1; uint16_t w,h;
    _display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    int x = max(0, (64 - (int)w)/2);
    _display.setCursor(x, y);
    _display.println(s);
  };

  printLine(l1, 10);
  if (l2.length()) printLine(l2, 26);

  drawFp64Right();
  show();
}

void DisplayModel::idle()     { leftTwoLines("Ponga su", "huella"); }
void DisplayModel::scanning() { leftTwoLines("Escaneando", "mantener..."); _scanningActive = true;}

void DisplayModel::okMsg(const String& l2) {
  clear();
  _display.setTextColor(SH110X_WHITE);
  _display.setTextSize(2);
  _display.setCursor(6, 4); _display.println("OK");
  _display.setTextSize(1);
  _display.setCursor(0, 28); _display.println(l2);
  drawFp64Right();
  show();
  _scanningActive = false;
}

void DisplayModel::errorMsg(const String& msg) {
  clear();
  _display.setTextColor(SH110X_WHITE);
  _display.setTextSize(2);
  _display.setCursor(0, 4); _display.println("ERROR");
  _display.setTextSize(1);
  _display.setCursor(0, 28); _display.println(msg);
  drawFp64Right();
  show();
  _scanningActive = false;
}

void DisplayModel::welcome(const String& nombre, uint16_t id, int score) {
  clear();
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
    _display.print(nombre); _display.print(" (ID "); _display.print(id); _display.println(")");
  } else {
    _display.print("ID "); _display.println(id);
  }
  _display.setCursor(0, 48);
  _display.print("Score: "); _display.println(score);

  drawFp64Right();
  show();
  _scanningActive = false;
}

void DisplayModel::scanBlinkTick(bool on) {
  const int x = 64 + _xoff;
  const int y = 0;

  // Apagar exactamente el área del ícono
  _display.fillRect(x, y, FP64_W, FP64_H, SH110X_BLACK);

  // Encender si corresponde
#if FP_BITMAP_IS_XBM
  if (on) _display.drawXBitmap(x, y, FP_64x64, FP64_W, FP64_H, 1);
#else
  if (on) _display.drawBitmap (x, y, FP_64x64, FP64_W, FP64_H, 1);
#endif

  _display.display();
}

