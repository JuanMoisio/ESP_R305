#pragma once
#include <Arduino.h>
#include "DisplayModel.h"
#include "FingerprintModel.h"
#include "NamesModel.h"
#include "FingerprintApi.h"
#include "ScanRequest.h"
#include "Bitmaps.h"

enum class AutoState { WAIT_FINGER, MATCHING, COOLDOWN };

struct MatchJob {
  bool active = false;
  bool done   = false;
  bool ok     = false;
  int  id     = -1;
  int  score  = 0;
};

class AutoMode {
public:
  AutoMode(DisplayModel& d, FingerprintModel& f, NamesModel& n) 
    : display(d), finger(f), names(n) {}

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
      drawWaitingCommand();
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
            Serial.printf("[AutoMode] requestScan -> waitingForFinger at %lu\n", millis());
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
            Serial.printf("[AutoMode] request cleared -> idle at %lu\n", millis());
          }
        }
        break;
      }

      case AutoState::MATCHING: {
        // 1) Avanzar animación si es momento
        if ((long)(now - nextPhaseAt) >= 0) stepAnimation(now);

        // 1b) Dibujar scan-bar si está habilitada
        if (scanBarEnabled) drawScanBarIfNeeded(now);

        // 2) ¿Llegó resultado? (y ya pasó el mínimo)
        if (resultReady) {
          if ((long)(now - showResultAt) >= 0) {
            if (resultOk) {
              // mostrar sólo icono de OK centrado (sin nombre/texto)
              showCenteredIcon(ICON_OK_64);
              // opcional: imprimir info por serial para debug
              const int slotsPerUser = 5;
              int userId = (resultId >= 0) ? (resultId / slotsPerUser) : -1;
              String name = names.get(userId);
              Serial.printf("Match OK: user=%d name='%s' score=%d\n", userId, name.c_str(), resultScore);
            } else {
              // mostrar sólo icono de ERROR centrado
              showCenteredIcon(ICON_ERR_64);
              Serial.println("Match FAIL: sin coincidencia");
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

        // 3) ¿Job en background ya terminó? Setear resultado y marcar tiempo de show
        if (job.active && job.done) {
          job.active = false;
          resultOk    = job.ok;
          resultId    = job.id;
          resultScore = job.score;
          resultReady = true;
          // asegurar que ha pasado el mínimo de tiempo de escaneo visual
          unsigned long elapsed = now - scanStart;
          if (elapsed >= minScanMs) {
            showResultAt = now; // mostrar inmediatamente
          } else {
            showResultAt = scanStart + minScanMs; // esperar al mínimo
          }
          break;
        }

        // 4) ¿Timeout global?
        if ((long)(now - matchingDeadline) >= 0) {
          display.errorMsg("Tiempo agotado");
          Serial.printf("[AutoMode] matching timeout -> enter cooldown at %lu\n", millis());
          cooldownUntil = now + RESULT_MS;
          state = AutoState::COOLDOWN;
          uiDrawn = AutoState::COOLDOWN;
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

private:
  // ===== modelos
  DisplayModel&     display;
  FingerprintModel& finger;
  NamesModel&       names;

  // helper: dibuja logo en idle
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

  // helper: mostrar icono centrado sin texto
  void showCenteredIcon(const uint8_t* icon) {
    auto& d = display.raw();
    d.clearDisplay();
    const int xOffset = 32; // desplazar 32 píxeles a la derecha  
    const int yOffset = 0;
  #if FP_BITMAP_IS_XBM
    d.drawXBitmap(xOffset, yOffset, icon, ICON_W, ICON_H, SH110X_WHITE);
  #else
    d.drawBitmap(xOffset, yOffset, icon, ICON_W, ICON_H, SH110X_WHITE);
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
  unsigned long minScanMs = 3900;  // "mínimo escaneo" visual (ms)

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
};