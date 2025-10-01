#include "FingerprintModel.h"

void FingerprintModel::begin(uint32_t initialBaud) {
  _ser.begin(initialBaud, SERIAL_8N1, _pinRx, _pinTx);
  delay(60);
  autoDetect();
  if (_detectedBaud) {
    _finger.setPacketSize(FINGERPRINT_PACKET_SIZE_32);
    _finger.getParameters(); // best-effort
  }
}

bool FingerprintModel::tryAt(uint32_t b) {
  _ser.updateBaudRate(b); delay(40);
  _finger.begin(b);       delay(40);
  return _finger.verifyPassword();
}

void FingerprintModel::autoDetect() {
  uint32_t bauds[] = {57600,115200,38400,19200};
  for (uint8_t i=0;i<sizeof(bauds)/sizeof(bauds[0]);++i) {
    if (tryAt(bauds[i])) { _detectedBaud=bauds[i]; return; }
  }
  _detectedBaud=0;
}

const char* FingerprintModel::err(uint8_t code) const {
  switch (code) {
    case FINGERPRINT_OK:               return "OK";
    case FINGERPRINT_PACKETRECIEVEERR: return "packet";
    case FINGERPRINT_NOFINGER:         return "no_finger";
    case FINGERPRINT_IMAGEFAIL:        return "image_fail";
    case FINGERPRINT_IMAGEMESS:        return "image_mess";
    case FINGERPRINT_FEATUREFAIL:      return "feature_fail";
    case FINGERPRINT_INVALIDIMAGE:     return "invalid_image";
    case FINGERPRINT_ENROLLMISMATCH:   return "enroll_mismatch";
    case FINGERPRINT_BADLOCATION:      return "bad_location";
    case FINGERPRINT_DBCLEARFAIL:      return "db_clear_fail";
    case FINGERPRINT_UPLOADFEATUREFAIL:return "upload_feature_fail";
    case FINGERPRINT_PACKETRESPONSEFAIL:return "packet_resp_fail";
    case FINGERPRINT_TEMPLATECOUNT:    return "templatecount_err";
    default:                           return "unknown";
  }
}

bool FingerprintModel::captureToBuffer(uint8_t buf, uint32_t timeoutMs,
                                       void (*blinkCb)(bool)) {
  unsigned long t0=millis();
  while (_finger.getImage()!=FINGERPRINT_NOFINGER && millis()-t0<1500) delay(40);

  unsigned long start = millis();
  bool blink=false; unsigned long lastToggle=0;

  while (true) {
    unsigned long now = millis();
    if (blinkCb && now-lastToggle>180) { lastToggle=now; blink=!blink; blinkCb(blink); }

    uint8_t rc = _finger.getImage();
    if (rc == FINGERPRINT_OK) { if (blinkCb) blinkCb(true); break; }
    if (rc != FINGERPRINT_NOFINGER) { /* ruido; continuar */ }

    if (millis()-start > timeoutMs) return false;
    delay(20);
  }

  uint8_t rc = _finger.image2Tz(buf);
  if (rc != FINGERPRINT_OK) return false;

  unsigned long t2=millis();
  while (_finger.getImage()!=FINGERPRINT_NOFINGER && millis()-t2<1500) delay(40);
  return true;
}

bool FingerprintModel::enroll(uint16_t id, void (*blinkCb)(bool)) {
  if (id>999) return false;
  if (!captureToBuffer(1, 15000, blinkCb)) return false;
  delay(500);
  if (!captureToBuffer(2, 15000, blinkCb)) return false;

  uint8_t rc = _finger.createModel();
  if (rc != FINGERPRINT_OK) return false;
  rc = _finger.storeModel(id);
  return rc == FINGERPRINT_OK;
}

MatchRes FingerprintModel::fastMatch(void (*blinkCb)(bool)) {
  MatchRes r{false,-1,0};
  if (!captureToBuffer(1, 15000, blinkCb)) return r;
  if (_finger.fingerFastSearch() == FINGERPRINT_OK) {
    r.ok = true; r.id = _finger.fingerID; r.score = _finger.confidence;
  }
  return r;
}
