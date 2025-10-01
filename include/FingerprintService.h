#pragma once
#include <HardwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include "Types.h"        // <- ahora existe en include/
#include <OledUi.h>       // si OledUi se usa en headers/impl
#include <Config.h>


class FingerprintService {
public:
  FingerprintService(HardwareSerial& serial, int pinRx, int pinTx);
  bool begin();
  uint32_t detectedBaud() const { return detectedBaud_; }

  String infoJson();
  int count();
  bool empty();
  bool remove(uint16_t id);
  bool enroll(uint16_t id, String& err);     // bloqueante simple
  FPMatch match();                           // bloqueante simple

private:
  bool captureTo(uint8_t bufNo);
  bool tryAt(uint32_t baud);
  void autoDetectBaud();

  HardwareSerial& ser_;
  Adafruit_Fingerprint fp_;
  int rx_, tx_;
  uint32_t detectedBaud_ = 0;
};
