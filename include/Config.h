// include/Config.h
#pragma once

#ifndef WIFI_SSID
  #define WIFI_SSID "JUANPQ"
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS "12345678"
#endif
#ifndef HTTP_PORT
#define HTTP_PORT 80
#endif

// R305 en UART2 remapeado (cruzado)
static const int FP_PIN_RX = 25;  // TX del R305 -> RX del ESP32
static const int FP_PIN_TX = 26;  // RX del R305 <- TX del ESP32

// I2C OLED
static const int I2C_SDA = 21;
static const int I2C_SCL = 22;
static const uint8_t OLED_ADDR = 0x3C;
