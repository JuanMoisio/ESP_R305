#include <Arduino.h>
#include "OledUi.h"
#include "Bitmaps.h"

static constexpr int XOFF   = 2;   // corrimiento horizontal si querés
static constexpr int ICON_Y = 0;   // corrimiento vertical del ícono

static const uint8_t PROGMEM kBitRev[256] = {
  0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
  0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
  0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
  0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
  0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
  0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
  0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
  0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
  0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
  0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
  0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
  0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
  0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
  0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
  0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
  0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF
};

// Convierte XBM (LSB-first) a MSB-first en un buffer temporal y dibuja con drawBitmap().
static void drawXbmAny(Adafruit_GFX& dsp, int16_t x, int16_t y,
                       const uint8_t* xbmPROGMEM, int w, int h, uint16_t color)
{
  const size_t n = (size_t)w*h/8;
  static uint8_t tmp[(64*64)/8];           // suficiente para 64x64
  for (size_t i = 0; i < n; ++i) {
    uint8_t b = pgm_read_byte(xbmPROGMEM + i);
    tmp[i] = pgm_read_byte(&kBitRev[b]);   // invierte bits por byte
  }
  dsp.drawBitmap(x, y, tmp, w, h, color);
}

OledUi::OledUi(TwoWire& wire, int w, int h, uint8_t addr)
: wire_(wire),
  disp_(w, h, &wire_, -1),
  width_(w),
  height_(h),
  addr_(addr)
{}

bool OledUi::begin(int sda, int scl) {
  wire_.begin(sda, scl);
  // para SH1106G:
  if (!disp_.begin(addr_)) return false;  // <-- CAMBIO: sin SSD1306_SWITCHCAPVCC
  disp_.clearDisplay();
  disp_.display();
  return true;
}

void OledUi::centerText(const String& a, const String& b) {
  disp_.clearDisplay();
  disp_.setTextSize(1);
  disp_.setTextColor(SH110X_WHITE);

  int16_t x1, y1; uint16_t w, h;

  disp_.getTextBounds(a, 0, 0, &x1, &y1, &w, &h);
  int x = (width_ - (int)w) / 2;
  disp_.setCursor(x, 2);
  disp_.println(a);

  if (b.length()) {
    disp_.getTextBounds(b, 0, 0, &x1, &y1, &w, &h);
    x = (width_ - (int)w) / 2;
    disp_.setCursor(x, 14);
    disp_.println(b);
  }

  // icono
int bx = (width_  - FP64_W) / 2 + XOFF;
int by = (height_ - FP64_H) / 2;
drawXbmAny(disp_, bx, by, FP64, FP64_W, FP64_H, SH110X_WHITE);

  disp_.display();
}

void OledUi::idle() {
  centerText("Ponga su", "huella");
}

void OledUi::scanning() {
  disp_.clearDisplay();
  disp_.setTextSize(1);
  disp_.setTextColor(SH110X_WHITE);

  // Títulos centrados
  int16_t x1, y1; uint16_t w, h;
  String a = "Ponga su", b = "huella";
  disp_.getTextBounds(a, 0,0, &x1,&y1,&w,&h);
  int x = (width_ - (int)w) / 2;
  disp_.setCursor(x, 2);
  disp_.println(a);

  disp_.getTextBounds(b, 0,0, &x1,&y1,&w,&h);
  x = (width_ - (int)w) / 2;
  disp_.setCursor(x, 14);
  disp_.println(b);

  // Ícono de huella 64x64 centrado
  int bx = (width_  - FP64_W) / 2 + 2;      // XOFF = 2
  int by = (height_ - FP64_H) / 2;
  drawXbmAny(disp_, bx, by, FP64, FP64_W, FP64_H, SH110X_WHITE);

  disp_.display();
}


void OledUi::ok(const String& msg) {
  disp_.clearDisplay();
  disp_.setTextSize(2);
  disp_.setTextColor(SH110X_WHITE);
  disp_.setCursor( (width_ - 2*6*2) / 2, 0 ); // “OK” centrado (aprox)
  disp_.println("OK");

  disp_.setTextSize(1);
  disp_.setCursor(0, 18);
  disp_.println(msg.length() ? msg : "Lectura correcta");
int bx = (width_  - FP64_W) / 2 + XOFF;
int by = (height_ - FP64_H) / 2;
drawXbmAny(disp_, bx, by, FP64, FP64_W, FP64_H, SH110X_WHITE);

  disp_.display();
}

void OledUi::error(const String& msg) {
  disp_.clearDisplay();
  disp_.setTextSize(2);
  disp_.setTextColor(SH110X_WHITE);
  disp_.setCursor( (width_ - 6*5) / 2, 0 ); // “ERROR” aprox al centro
  disp_.println("ERROR");

  disp_.setTextSize(1);
  disp_.setCursor(0, 18);
  disp_.println(msg);

int bx = (width_  - FP64_W) / 2 + XOFF;
int by = (height_ - FP64_H) / 2;
drawXbmAny(disp_, bx, by, FP64, FP64_W, FP64_H, SH110X_WHITE);
  disp_.display();
}

void OledUi::enrollStep(uint8_t n) {
  disp_.clearDisplay();
  disp_.setTextSize(1);
  disp_.setTextColor(SH110X_WHITE);
  disp_.setCursor(0, 0);
  disp_.println("Enrolamiento");
  disp_.setTextSize(2);
  disp_.setCursor( (width_ - 6*6) / 2, 12); // “Toma N”
  disp_.print("Toma ");
  disp_.println(n);
  disp_.setTextSize(1);
  disp_.setCursor(0, 32);
  disp_.println(n==1 ? "Apoye y mantenga" : "Mismo dedo, leve giro");

int bx = (width_  - FP64_W) / 2 + XOFF;
int by = (height_ - FP64_H) / 2;
drawXbmAny(disp_, bx, by, FP64, FP64_W, FP64_H, SH110X_WHITE);

  disp_.display();
}
