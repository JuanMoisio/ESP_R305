#pragma once
#include <ESPAsyncWebServer.h>

void initFingerprintApi(AsyncWebServer& server, AsyncEventSource& events);

// Encolan eventos (no envían inmediatamente)
void fpApiEmitPrompt();
void fpApiEmitResult(bool ok, int id, int score);
void fpApiEmitEnrollStart();
void fpApiEmitEnrollAbort();
void fpApiEmitEnrollResult(bool ok, int id);
void fpApiEmitEraseRequest(int id);
void fpApiEmitEraseResult(bool ok, int id);

// Llamar periódicamente desde loop() para intentar enviar eventos pendientes
void fpApiLoop();