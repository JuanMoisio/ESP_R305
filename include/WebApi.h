#pragma once
#include <ESPAsyncWebServer.h>
#include "FingerprintService.h"

class WebApi {
public:
  WebApi(FingerprintService& fp);
  void begin(uint16_t port=80);
private:
  AsyncWebServer server_;
  FingerprintService& fp_;
  void cors(AsyncWebServerRequest *req);
  void routes();
};
