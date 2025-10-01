#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

class DisplayModel {
public:
  static constexpr int SCREEN_W = 128;
  static constexpr int SCREEN_H = 64;

  explicit DisplayModel(TwoWire *wire, int xoffset = 2);

  bool begin();                    // inicia SH1106 (0x3C)
  void idle();                     // "Ponga su" / "huella"
  void scanning();                 // "Escaneando" / "mantener..."
  void okMsg(const String& l2="Lectura correcta");
  void errorMsg(const String& msg);
  void welcome(const String& nombre, uint16_t id, int score);

  void scanBlinkTick(bool on);     // parpadeo icono durante captura

private:
  void clear();
  void show();
  void leftTwoLines(const String& l1, const String& l2);
  void drawFp64Right();

  Adafruit_SH1106G _display;
  int _xoff;
};
