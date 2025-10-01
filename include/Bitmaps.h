#pragma once
#include <stdint.h>

// Bitmap 64x64 (1bpp)
extern const uint8_t FP_64x64[];

constexpr int FP64_W = 64;
constexpr int FP64_H = 64;

// Alias de compatibilidad con código viejo:
#define FP64 FP_64x64

// Si tu bitmap fuente está en XBM/MSB-first, poné 1.
// Si está en LSB-first (típico "drawBitmap"), poné 0.
#ifndef FP_BITMAP_IS_XBM
#define FP_BITMAP_IS_XBM 0
#endif
