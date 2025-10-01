
#include <FingerprintService.h>
#include <WebApi.h>
#include <OledUi.h>
#include "Log.h"

FingerprintService::FingerprintService(HardwareSerial& s, int rx, int tx)
: ser_(s), fp_(&ser_), rx_(rx), tx_(tx) {}

bool FingerprintService::begin() {
  ser_.begin(57600, SERIAL_8N1, rx_, tx_);
  delay(80);
  autoDetectBaud();
  if (!detectedBaud_) return false;
  fp_.setPacketSize(FINGERPRINT_PACKET_SIZE_32); // no falla si no aplica
  return true;
}

bool FingerprintService::tryAt(uint32_t b) {
  ser_.updateBaudRate(b);
  delay(40);
  fp_.begin(b);
  delay(40);
  return fp_.verifyPassword();
}

void FingerprintService::autoDetectBaud() {
  uint32_t bauds[] = {57600, 115200, 38400, 19200};
  for (auto b : bauds) if (tryAt(b)) { detectedBaud_ = b; return; }
  detectedBaud_ = 0;
}

String FingerprintService::infoJson() {
  if (fp_.getParameters() != FINGERPRINT_OK) return "{\"ok\":false}";
  String j = "{";
  j += "\"ok\":true,";
  j += "\"capacity\":" + String(fp_.capacity) + ",";
  j += "\"security_level\":" + String(fp_.security_level) + ",";
  j += "\"baud\":" + String(detectedBaud_);
  j += "}";
  return j;
}

int FingerprintService::count() {
  return (fp_.getTemplateCount()==FINGERPRINT_OK) ? fp_.templateCount : -1;
}

bool FingerprintService::empty() { return fp_.emptyDatabase()==FINGERPRINT_OK; }
bool FingerprintService::remove(uint16_t id) { return fp_.deleteModel(id)==FINGERPRINT_OK; }

bool FingerprintService::captureTo(uint8_t buf) {
  uint32_t t0 = millis();
  while (fp_.getImage() != FINGERPRINT_OK) {
    if (millis()-t0 > 15000) return false;
    delay(60);
  }
  return fp_.image2Tz(buf) == FINGERPRINT_OK;
}

bool FingerprintService::enroll(uint16_t id, String& err) {
  if (!captureTo(1)) { err="cap1"; return false; }
  delay(600);
  if (!captureTo(2)) { err="cap2"; return false; }
  if (fp_.createModel()!=FINGERPRINT_OK) { err="mismatch"; return false; }
  if (fp_.storeModel(id)!=FINGERPRINT_OK) { err="store"; return false; }
  return true;
}

FPMatch FingerprintService::match() {
  FPMatch r{false,-1,0,""};
  if (!captureTo(1)) { r.err="capture"; return r; }
  if (fp_.fingerFastSearch()==FINGERPRINT_OK) {
    r.ok=true; r.id=fp_.fingerID; r.score=fp_.confidence; return r;
  }
  r.err="notfound"; return r;
}
