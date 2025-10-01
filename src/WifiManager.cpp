#include "WifiManager.h"
bool WifiManager::connect(const char* ssid, const char* pass, uint16_t retries, uint16_t stepMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  for (uint16_t i=0;i<retries && WiFi.status()!=WL_CONNECTED;i++) delay(stepMs);
  return WiFi.isConnected();
}
