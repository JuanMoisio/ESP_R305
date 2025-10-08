#pragma once
#include <Arduino.h>
#include "DisplayModel.h"
#include "FingerprintModel.h"
#include "NamesModel.h"
#include "FingerprintApi.h"
#include "ScanRequest.h"
#include "Bitmaps.h"

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
    // asegurar estado inicial: no esperar huella y mostrar idle
    cancelScan();                     // limpiar cualquier petición previa
    waitingForFinger = false;
    state = AutoState::WAIT_FINGER;
    uiDrawn = AutoState::WAIT_FINGER;

    // nuevo: dibujar explícito "Waiting command"
    drawWaitingCommand();

    // debug: confirmar estado inicial de scan
    Serial.printf("[AutoMode] begin() isScanRequested=%d at=%lu\n", isScanRequested() ? 1 : 0, millis());

    // inicializa barra de escaneo
    scanBarY = FP_Y;
    scanBarNextAt = millis();
  }

  // Habilitar/deshabilitar la barra de escaneo
  void setScanBarEnabled(bool en) { scanBarEnabled = en; }

  // Nuevo: ajustar tiempo mínimo visual de escaneo (ms)
  void setMinScanMs(unsigned long ms) { minScanMs = ms; }
  // Nuevo: ajustar velocidad de la barra (ms entre pasos, pixeles por paso)
  void setScanBarSpeed(unsigned long stepMs, int pixels) {
    scanBarStepMs = stepMs;
    scanBarStepPixels = max(1, pixels);
  }

  // Llamar en loop() muy seguido
  void tick() {
    unsigned long now = millis();

    // debug: detectar cambios de estado
    if (state != prevState) {
      Serial.printf("[AutoMode] state %d -> %d at %lu\n", (int)prevState, (int)state, now);
      prevState = state;
    }

    // Si programamos un retorno forzado a idle, cumplirlo (por seguridad)
    if (forcedReturnAt != 0 && (long)(now - forcedReturnAt) >= 0) {
      Serial.printf("[AutoMode] forced return to idle at %lu\n", now);
      forcedReturnAt = 0;
      cancelScan();
      waitingForFinger = false;
      state = AutoState::WAIT_FINGER;
      uiDrawn = AutoState::WAIT_FINGER;
      display.idle();
    }

    switch (state) {
      case AutoState::WAIT_FINGER: {
        // Dibujar idle SOLO si NO hay petición de scan activa
        if (!isScanRequested()) {
          if (uiDrawn != AutoState::WAIT_FINGER) {
            drawWaitingCommand();
             uiDrawn = AutoState::WAIT_FINGER;
             Serial.printf("[AutoMode] UI -> idle at %lu\n", millis());
          }
        }

        // Si hubo petición externa (API/serial) entramos en "esperando dedo"
        // y mantenemos esa pantalla hasta detectar el dedo o hasta cancelar/expirar.
        if (isScanRequested()) {
          // marcar que estamos en modo "esperando dedo" y mostrar instrucción sólo al entrar
          if (!waitingForFinger) {
            waitingForFinger = true;
            display.scanning();    // pantalla que indica "Ponga su huella"
            Serial.printf("[AutoMode] requestScan received -> waitingForFinger at %lu\n", millis());
            uiDrawn = AutoState::MATCHING; // usamos MATCHING UI mientras esperamos el dedo
          }

          // Si detecta dedo => arrancar MATCHING (tarea background)
          if (finger.chip().getImage() != FINGERPRINT_NOFINGER) {
            // salir del modo "esperando dedo" porque ya apoyó el dedo
            waitingForFinger = false;
            Serial.printf("[AutoMode] dedo detectado -> start MATCHING at %lu\n", millis());

            // arrancamos animación y matching como antes
            phase = 0; phaseDir = +1;
            nextPhaseAt = now;
            display.drawFpPhase(phase);

            scanStart       = now;
            matchingDeadline= now + 15000;
            resultReady     = false;

            job.done = false;
            job.active = true;
            xTaskCreatePinnedToCore(+[](void* arg){
              AutoMode* self = static_cast<AutoMode*>(arg);
              self->runMatchTask();
              vTaskDelete(nullptr);
            }, "fingerMatch", 8192, this, 1, nullptr, 0);

            // reset scan bar
            scanBarY = FP_Y;
            scanBarDir = +1;
            scanBarNextAt = now;

            state = AutoState::MATCHING;
          }
        } else {
          // no hay petición: si veníamos en "esperando dedo" la limpiamos y volvemos a idle
          if (waitingForFinger) {
            waitingForFinger = false;
            drawWaitingCommand();
            uiDrawn = AutoState::WAIT_FINGER;
          }
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
            Serial.printf("[AutoMode] result shown ok=%d id=%d score=%d at %lu\n", resultOk, resultId, resultScore, millis());

            // Asegurar que cancelamos cualquier petición de escaneo y salimos del modo "esperando dedo"
            cancelScan();
            waitingForFinger = false;
           // programar un retorno forzado a idle tras RESULT_MS por si algo re-habilita el modo
           forcedReturnAt = millis() + RESULT_MS;

            cooldownUntil = now + RESULT_MS;
            state  = AutoState::COOLDOWN;
            uiDrawn= AutoState::COOLDOWN;
          }
          break;
        }

        // 3) Timeout duro
        if ((long)(now - matchingDeadline) >= 0) {
          display.errorMsg("Tiempo agotado");
          Serial.printf("[AutoMode] matching timeout -> enter cooldown at %lu\n", millis());
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
          showResultAt = max(now, scanStart + minScanMs);

          job.done   = false;
          job.active = false;

         // Al finalizar el intento limpiamos la petición de scan (si estaba activa)
         cancelScan();

        }
        break;
      }

      case AutoState::COOLDOWN: {
        // Nada que pintar; sólo esperar
        if ((long)(now - cooldownUntil) >= 0) {
          state = AutoState::WAIT_FINGER;
          // asegurar que la UI vuelva a idle inmediatamente
          uiDrawn = AutoState::WAIT_FINGER;
          waitingForFinger = false;
          // también cancelar petición por si quedó en cola desde otro lado
          cancelScan();
          forcedReturnAt = 0;
          drawWaitingCommand();
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

  // helper: dibuja "Waiting command" limpiando todo el buffer
  void drawWaitingCommand() {
    auto& d = display.raw();
    d.clearDisplay();

    // offset horizontal (desplazar 32 píxeles a la derecha)
    const int xOffset = 32;
    const int yOffset = 0;

    // debug: leer primeros bytes del bitmap para comprobar que existe en PROGMEM
    uint8_t first = pgm_read_byte_near(ICON_PERMAQUIM_64);
    Serial.printf("[AutoMode] ICON_PERMAQUIM_64 first=0x%02x\n", first);

    // contar píxeles para detectar formato
    auto countPixels = [&](bool msbFirst)->int {
      int count = 0;
      for (int y = 0; y < ICON_H; ++y) {
        for (int x = 0; x < ICON_W; ++x) {
          int bitIndex = y * ICON_W + x;
          int byteIndex = bitIndex >> 3;
          int bitPos = bitIndex & 7;
          uint8_t b = pgm_read_byte_near(ICON_PERMAQUIM_64 + byteIndex);
          bool on = msbFirst ? ((b >> (7 - bitPos)) & 1) : ((b >> bitPos) & 1);
          if (on) ++count;
        }
      }
      return count;
    };

    int msbCount = countPixels(true);
    int lsbCount = countPixels(false);
    Serial.printf("[AutoMode] pixelCounts msb=%d lsb=%d\n", msbCount, lsbCount);

    if (msbCount == 0 && lsbCount == 0) {
      // fallback texto centrado
      d.setTextSize(1);
      d.setTextColor(SH110X_WHITE);
      d.setCursor(0, 28);
      d.print("Waiting command");
      d.display();
      return;
    }

    bool useMsb = msbCount >= lsbCount;

  #if FP_BITMAP_IS_XBM
    if (useMsb) {
      d.drawXBitmap(xOffset, yOffset, ICON_PERMAQUIM_64, ICON_W, ICON_H, SH110X_WHITE);
    } else {
      // dibujado manual LSB
      for (int y = 0; y < ICON_H; ++y) {
        for (int x = 0; x < ICON_W; ++x) {
          int bitIndex = y * ICON_W + x;
          int byteIndex = bitIndex >> 3;
          int bitPos = bitIndex & 7;
          uint8_t b = pgm_read_byte_near(ICON_PERMAQUIM_64 + byteIndex);
          bool on = ((b >> bitPos) & 1);
          if (on) d.drawPixel(x + xOffset, y + yOffset, SH110X_WHITE);
        }
      }
    }
  #else
    if (!useMsb) {
      d.drawBitmap(xOffset, yOffset, ICON_PERMAQUIM_64, ICON_W, ICON_H, SH110X_WHITE);
    } else {
      // dibujado manual MSB
      for (int y = 0; y < ICON_H; ++y) {
        for (int x = 0; x < ICON_W; ++x) {
          int bitIndex = y * ICON_W + x;
          int byteIndex = bitIndex >> 3;
          int bitPos = bitIndex & 7;
          uint8_t b = pgm_read_byte_near(ICON_PERMAQUIM_64 + byteIndex);
          bool on = ((b >> (7 - bitPos)) & 1);
          if (on) d.drawPixel(x + xOffset, y + yOffset, SH110X_WHITE);
        }
      }
    }
  #endif

    d.display();
  }

  // ===== estado UI
  AutoState state   = AutoState::WAIT_FINGER;
  AutoState uiDrawn = AutoState::COOLDOWN;
  bool waitingForFinger = false; // true mientras se espera que el usuario apoye el dedo tras requestScan()

  // debug / forzar retorno a idle
  AutoState prevState = AutoState::WAIT_FINGER;
  unsigned long forcedReturnAt = 0;
  
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
  // velocidad configurable: ms entre pasos y pixeles por paso
  unsigned long scanBarStepMs = 24; // menor = más rápido
  int scanBarStepPixels = 1;        // píxeles que avanza cada paso
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
     scanBarNextAt = now + scanBarStepMs;

     // avanzar posición (posible salto >1px para acelerar)
     scanBarY += scanBarDir * scanBarStepPixels;
     if (scanBarY > FP_Y + FP_H - SCANBAR_THICK) { scanBarY = FP_Y + FP_H - SCANBAR_THICK; scanBarDir = -1; }
     if (scanBarY < FP_Y) { scanBarY = FP_Y; scanBarDir = +1; }

     // redraw: limpiamos la región de la huella, pintamos la huella y luego la barra (overlay)
     // así evitamos restos dejados por la barra anterior
     auto& d = display.raw();
     d.fillRect(FP_X, FP_Y, FP_W, FP_H, SH110X_BLACK); // limpia área FP
     display.drawFpPhase(phase);                        // pinta huella original
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