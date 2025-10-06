#pragma once
#include <Arduino.h>
#include <Adafruit_SH110X.h>

class DisplayModel {
public:
  explicit DisplayModel(Adafruit_SH1106G& d, int xoff = 2) : _display(d), _xoff(xoff) {}
  Adafruit_SH1106G& raw();  // acceso al objeto OLED

  // Inicialización del OLED (wrapper conveniente)
  bool begin(uint8_t addr = 0x3C, bool reset = true);

  // Pantallas básicas (podés ajustar los textos a gusto)
  void idle();
  void scanning();
  void welcome(const String& nombre, uint16_t id, int score);
  void errorMsg(const String& msg);
  void okMsg(const String& l2 = "");

  // Dibujo del ícono 64x64
  void drawFp64Right();

  // Animación por fases (0=25%, 1=50%, 2=75%, 3=100)
  void drawFpPhase(uint8_t phase);
  void drawFpPhaseLabeled(uint8_t phase, uint8_t label);

  // Compatibilidad: si alguien aún llama a ON/OFF
  void scanBlinkTick(bool on);

  // Offset horizontal típico del SH1106
  void setXOffset(int xo) { _xoff = xo; }
  int  xoffset() const { return _xoff; }
  void drawOkRight();
  void drawErrRight();

private:
  Adafruit_SH1106G& _display;
  int _xoff;
};

  
