#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>

class OledUi {
public:
  OledUi(TwoWire& wire, int w=128, int h=64, uint8_t addr=0x3C);

  bool begin(int sda=21, int scl=22);

  void showIdle();                    // "Ponga su" / "huella" + icono fijo
  void startScanning();               // entra a pantalla "Escaneando"
  void tick();                        // llamalo en loop(): parpadeo de huella
  void showOk(const String& msg="Lectura correcta");
  void showError(const String& msg);

private:
  void drawXbmAny(Adafruit_GFX& dsp,int16_t x,int16_t y,
                  const uint8_t* xbm,int w,int h,uint16_t color);
  void drawFingerprint(bool on);
  void printCenter(const String& l1, const String& l2, int y1=2, int y2=14);

  TwoWire&         wire_;
  Adafruit_SH1106G disp_;
  int              width_, height_;
  uint8_t          addr_;

  // anim
  bool             blinkOn_ = true;
  unsigned long    lastBlink_ = 0;
  int              fpX_=0, fpY_=0;
};
