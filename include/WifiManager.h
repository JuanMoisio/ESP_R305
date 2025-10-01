#pragma once
#include <WiFi.h>
class WifiManager {
public:
  bool connect(const char* ssid, const char* pass, uint16_t retries=40, uint16_t stepMs=250);
};
