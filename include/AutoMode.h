#pragma once
#include <Arduino.h>
#include "DisplayModel.h"
#include "FingerprintModel.h"
#include "NamesModel.h"

// ===== Configs que ya usabas =====
#ifndef SCORE_MATCH
#define SCORE_MATCH 90
#endif

// ===== Estados =====
enum class AutoState { WAIT_FINGER, MATCHING, COOLDOWN };

// ===== Job FreeRTOS para el match =====
struct MatchJob {
  volatile bool active = false;
  volatile bool done   = false;
  bool ok = false;
  int  id = -1;
  int  score = 0;
};

class AutoMode {
public:
  AutoMode(DisplayModel& dm, FingerprintModel& fm, NamesModel& nm)
  : display(dm), finger(fm), names(nm) {}

  // Llamar en setup()
  void begin() {
    uiDrawn = AutoState::COOLDOWN;   // forzar primer draw de idle
    // inicializa barra de escaneo
    scanBarY = FP_Y;
    scanBarNextAt = millis();
  }

  // Habilitar/deshabilitar la barra de escaneo
  void setScanBarEnabled(bool en) { scanBarEnabled = en; }

  // Nuevo: ajustar tiempo mínimo visual de escaneo (ms)
  void setMinScanMs(unsigned long ms) { minScanMs = ms; }

  // Llamar en loop() muy seguido
  void tick() {
    unsigned long now = millis();

    switch (state) {
      case AutoState::WAIT_FINGER: {
        // Dibujo de idle una sola vez al entrar
        if (uiDrawn != AutoState::WAIT_FINGER) {
          display.idle();
          uiDrawn = AutoState::WAIT_FINGER;
        }

        // Dedo presente -> entrar a MATCHING y arrancar animación YA
        if (finger.chip().getImage() != FINGERPRINT_NOFINGER) {
          display.scanning();
          uiDrawn = AutoState::MATCHING;

          phase = 0; phaseDir = +1;
          nextPhaseAt = now;               // primer frame inmediato
          display.drawFpPhase(phase);      // dibuja el 1er fotograma YA

          scanStart       = now;
          matchingDeadline= now + 15000;
          resultReady     = false;

          // armamos el job para que pueda correr cuando haya imagen OK
          job.done = false;
          job.active = true; // marcar activo porque lanzamos la tarea ahora
          // lanzar tarea en core 0 para no trabar loop/UI
          xTaskCreatePinnedToCore(+[](void* arg){
            AutoMode* self = static_cast<AutoMode*>(arg);
            self->runMatchTask();
            vTaskDelete(nullptr);
          }, "fingerMatch", 4096, this, 1, nullptr, 0);

          // reset scan bar
          scanBarY = FP_Y;
          scanBarDir = +1;
          scanBarNextAt = now;

          state = AutoState::MATCHING;
        }
        break;
      }

      case AutoState::MATCHING: {
        // 1) Animación "breathe" sin bloquear
        if ((long)(now - nextPhaseAt) >= 0) {
          stepAnimation(now);
        }

        // Dibujo extra: barra de escaneo (overlay)
        if (scanBarEnabled) {
          drawScanBarIfNeeded(now);
        }

        // 2) ¿Llegó resultado? (y ya pasó el mínimo)
        if (resultReady) {
          if ((long)(now - showResultAt) >= 0) {
            if (resultOk) {
              String name = names.get(resultId);
              display.welcome(name, resultId, resultScore);
            } else {
              display.errorMsg("Sin coincidencia");
            }
            cooldownUntil = now + RESULT_MS;
            state  = AutoState::COOLDOWN;
            uiDrawn= AutoState::COOLDOWN;
          }
          break;
        }

        // 3) Timeout duro
        if ((long)(now - matchingDeadline) >= 0) {
          display.errorMsg("Tiempo agotado");
          cooldownUntil = now + RESULT_MS;
          state  = AutoState::COOLDOWN;
          uiDrawn= AutoState::COOLDOWN;
          break;
        }

        // Nota: la captura y el matching se hacen en background (runMatchTask).
        // No se llama a getImage() aquí para no bloquear la UI.

        // 5) ¿job listo? Copio resultado y programo el momento de mostrar
        if (job.done && !resultReady) {
          bool ok    = job.ok;
          int  id    = job.id;
          int  score = job.score;

          resultOk     = ok && (score > SCORE_MATCH);
          resultId     = resultOk ? id : -1;
          resultScore  = score;
          resultReady  = true;
          // usar minScanMs configurable
          showResultAt = max(now, scanStart + minScanMs);

          job.done   = false;
          job.active = false;
        }
        break;
      }

      case AutoState::COOLDOWN: {
        // Nada que pintar; sólo esperar
        if ((long)(now - cooldownUntil) >= 0) {
          state = AutoState::WAIT_FINGER;
        }
        break;
      }
    }
  }

  // Helpers para pruebas desde serie
  void showOkTest()  { display.welcome("TEST", 1, 99);   enterCooldown(); }
  void showErrTest() { display.errorMsg("Prueba error"); enterCooldown(); }
  void showPanelTest() {
    auto& d = display.raw();
    d.clearDisplay();
    d.display();
    d.fillRect(64, 0, 64, 64, SH110X_BLACK);
    d.drawRect(64, 0, 64, 64, SH110X_WHITE);
    d.display();
    cooldownUntil = millis() + RESULT_MS;
  }

private:
  // ===== modelos
  DisplayModel&     display;
  FingerprintModel& finger;
  NamesModel&       names;

  // ===== estado UI
  AutoState state   = AutoState::WAIT_FINGER;
  AutoState uiDrawn = AutoState::COOLDOWN;

  // ===== animación
  uint8_t  phase    = 0;   // 0..3 (25/50/75/100)
  int8_t   phaseDir = +1;
  unsigned long nextPhaseAt = 0;
  static constexpr unsigned long PHASE_MS     = 150;   // más chico = más rápido
  static constexpr unsigned long RESULT_MS    = 1500; // tiempo que se ve el resultado
  // MIN_SCAN_MS ahora configurable en runtime:
  unsigned long minScanMs = 3900;  // “mínimo escaneo” visual (ms)

  // ===== timers
  unsigned long scanStart        = 0;
  unsigned long matchingDeadline = 0;
  unsigned long cooldownUntil    = 0;

  // ===== resultado
  bool resultReady = false;
  bool resultOk    = false;
  int  resultId    = -1;
  int  resultScore = 0;
  unsigned long showResultAt     = 0;

  // ===== job
  MatchJob job;

  // ===== Scan-bar overlay config (no modifica la animación existente)
  // Huella en la mitad derecha: pantalla 128x64 -> x=64..127, alto 0..63
  static constexpr int FP_X = 64; // moved to right half
  static constexpr int FP_Y = 0;
  static constexpr int FP_W = 64;
  static constexpr int FP_H = 64;

  int scanBarY = FP_Y;
  int scanBarDir = +1;
  unsigned long scanBarNextAt = 0;
  static constexpr unsigned long SCANBAR_STEP_MS = 24; // velocidad de la barra
  static constexpr int SCANBAR_THICK = 4; // grosor en pixeles
  bool scanBarEnabled = true;

  // --- avanzo un frame de animación
  inline void stepAnimation(unsigned long now) {
    nextPhaseAt = now + PHASE_MS;
    int p = (int)phase + phaseDir;
    if (p >= 3) { p = 3; phaseDir = -1; }
    if (p <= 0) { p = 0; phaseDir = +1; }
    phase = (uint8_t)p;
    display.drawFpPhase(phase);
  }

  // --- dibuja la barra de escaneo (se llama desde tick)
  void drawScanBarIfNeeded(unsigned long now) {
    if ((long)(now - scanBarNextAt) < 0) return;
    scanBarNextAt = now + SCANBAR_STEP_MS;

    // avanzar posición
    scanBarY += scanBarDir;
    if (scanBarY > FP_Y + FP_H - SCANBAR_THICK) { scanBarY = FP_Y + FP_H - SCANBAR_THICK; scanBarDir = -1; }
    if (scanBarY < FP_Y) { scanBarY = FP_Y; scanBarDir = +1; }

    // redraw: limpiamos la región de la huella, pintamos la huella y luego la barra (overlay)
    // así evitamos restos dejados por la barra anterior
    auto& d = display.raw();
    d.fillRect(FP_X, FP_Y, FP_W, FP_H, SH110X_BLACK); // <-- limpia área FP
    display.drawFpPhase(phase);                         // pinta huella original
    // barra horizontal blanca que cruza la imagen de la huella (en la mitad derecha)
    d.fillRect(FP_X, scanBarY, FP_W, SCANBAR_THICK, SH110X_WHITE);
    d.display();
  }

  // --- tarea en background (bloqueante sin afectar UI)
  void runMatchTask() {
    auto& chip = finger.chip();
    bool ok = false; int id = -1; int score = 0;

    // Hacer la captura en el task (bloqueante aquí, pero no afecta UI)
    if (chip.getImage() == FINGERPRINT_OK) {
      if (chip.image2Tz(1) == FINGERPRINT_OK) {
        ok = (chip.fingerFastSearch() == FINGERPRINT_OK);
        if (ok) { id = chip.fingerID; score = chip.confidence; }
      }
    }

    job.ok   = ok;
    job.id   = id;
    job.score= ok ? score : 0;
    job.done = true;
    // job.active se limpia en el thread que lanzó la tarea cuando procese el resultado
  }

  void enterCooldown() {
    cooldownUntil = millis() + RESULT_MS;
    state   = AutoState::COOLDOWN;
    uiDrawn = AutoState::COOLDOWN;
  }
};