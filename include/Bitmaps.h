#pragma once
#include <Arduino.h>

#define FP64_W 64
#define FP64_H 64

// 0 = drawBitmap (LSB-first), 1 = drawXBitmap (XBM/MSB-first)
#ifndef FP_BITMAP_IS_XBM
#define FP_BITMAP_IS_XBM 0
#endif

#define ICON_W 64
#define ICON_H 64

// Bitmaps (definidos en src/Bitmaps.cpp)
extern const uint8_t FP_64x64[512]       PROGMEM;
extern const uint8_t FP_64x64_75[512]    PROGMEM;
extern const uint8_t FP_64x64_50[512]    PROGMEM;
extern const uint8_t FP_64x64_25[512]    PROGMEM;
extern const uint8_t FP_64x64_1[512]     PROGMEM;

// ===== Íconos de resultado 64x64 =====


extern const uint8_t ICON_OK_64[512]  PROGMEM;  // tilde ✔  (64x64, 1bpp)
extern const uint8_t ICON_ERR_64[512] PROGMEM;  // cruz ✖  (64x64, 1bpp)


// Compatibilidad con código viejo (OledUi.cpp, etc.)
#define FP64 FP_64x64
