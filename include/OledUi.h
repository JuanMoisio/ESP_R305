#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include "Bitmaps.h"

class OledUi {
public:
  OledUi(TwoWire& wire, int w = 128, int h = 64, uint8_t addr = 0x3C);
  bool begin(int sda = 21, int scl = 22);

  void centerText(const String& a, const String& b = "");
  void idle();
  void scanning();
  void ok(const String& msg = "");
  void error(const String& msg);
  void enrollStep(uint8_t n);

private:
  TwoWire&          wire_;
  Adafruit_SH1106G  disp_;
  int               width_;
  int               height_;
  uint8_t           addr_;
};
