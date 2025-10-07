#include "ScanRequest.h"

// indicador simple protegido por interrupciones (suficiente para este caso)
static volatile uint8_t s_scanFlag = 0;

void requestScan() {
  noInterrupts();
  s_scanFlag = 1;
  interrupts();
}

bool consumeScanRequest() {
  bool got = false;
  noInterrupts();
  if (s_scanFlag) { got = true; s_scanFlag = 0; }
  interrupts();
  return got;
}