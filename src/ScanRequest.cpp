#include "ScanRequest.h"
#include <Arduino.h>

// almacenamos instante hasta el cual la petición es válida (0 = no)
static volatile unsigned long s_scanUntil = 0;

void requestScan(unsigned long timeoutMs) {
  noInterrupts();
  if (timeoutMs == 0) s_scanUntil = (unsigned long)(~0u); // forever
  else s_scanUntil = millis() + timeoutMs;
  interrupts();
  Serial.printf("[scanreq] requestScan timeoutMs=%lu at=%lu until=%lu\n", timeoutMs, (unsigned long)millis(), s_scanUntil);
}

void cancelScan() {
  noInterrupts();
  s_scanUntil = 0;
  interrupts();
  Serial.printf("[scanreq] cancelScan at=%lu\n", (unsigned long)millis());
}

bool isScanRequested() {
  noInterrupts();
  unsigned long t = s_scanUntil;
  interrupts();
  if (t == 0) return false;
  if (t == (unsigned long)(~0u)) return true; // forever
  // si expiró, limpiar y devolver false
  if ((long)((long)millis() - (long)t) > 0) {
    cancelScan();
    return false;
  }
  return true;
}