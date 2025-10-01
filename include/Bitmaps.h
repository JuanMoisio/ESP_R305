#pragma once
#include <Arduino.h>

// Bitmap de huella 64x64 (MSB-first, apto para Adafruit_GFX::drawBitmap)
extern const uint8_t FP64[] PROGMEM;

inline constexpr int FP64_W = 64;
inline constexpr int FP64_H = 64;
