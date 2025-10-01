#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Adafruit_Fingerprint.h>

struct MatchRes { bool ok; int id; int score; };

class FingerprintModel {
public:
  FingerprintModel(HardwareSerial& ser, int pinRx, int pinTx)
  : _ser(ser), _finger(&ser), _pinRx(pinRx), _pinTx(pinTx) {}

  void begin(uint32_t initialBaud = 57600);
  uint32_t detectedBaud() const { return _detectedBaud; }
  bool ready() const { return _detectedBaud != 0; }

  bool captureToBuffer(uint8_t buf, uint32_t timeoutMs,
                       void (*blinkCb)(bool) = nullptr);

  bool enroll(uint16_t id, void (*blinkCb)(bool) = nullptr);
  MatchRes fastMatch(void (*blinkCb)(bool) = nullptr);

  const char* err(uint8_t code) const;

  Adafruit_Fingerprint& chip() { return _finger; }

private:
  bool tryAt(uint32_t b);
  void autoDetect();

  HardwareSerial& _ser;
  Adafruit_Fingerprint _finger;
  int _pinRx, _pinTx;
  uint32_t _detectedBaud = 0;
};
