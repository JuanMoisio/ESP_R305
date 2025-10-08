#pragma once
#include <Arduino.h>
#include "AutoMode.h"
#include "DisplayModel.h"
#include "FingerprintModel.h"
#include "NamesModel.h"

#ifndef MAX_ENROLL_ATTEMPTS
  #define MAX_ENROLL_ATTEMPTS 5
#endif


namespace {
  static DisplayModel* gBlinkDisp = nullptr;
  static void BlinkCbThunk(bool on) {
    if (gBlinkDisp) gBlinkDisp->scanBlinkTick(on);
  }

  // Mostrar un icono 64x64 centrado en pantalla (desplazado 32px a la derecha)
  static void showCenteredIcon(DisplayModel& display, const uint8_t* icon) {
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
}



static inline String readLineWithTimeout(uint32_t ms) {
  String s; unsigned long t0=millis();
  while (millis()-t0 < ms) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c=='\r') continue;
      if (c=='\n') break;
      s += c;
    }
    delay(1);
  }
  s.trim(); return s;
}

static inline void printHelp() {
  Serial.println();
  Serial.println(F("Comandos:"));
  Serial.println(F("  e <id>           Enrolar en ID (0..999)"));
  Serial.println(F("    (nuevo) guarda 5 posiciones por ID: center, top, bottom, left, right"));
  Serial.println(F("  s                Match 1:N manual"));
  Serial.println(F("  d <id>           Borrar ID"));
  Serial.println(F("  c                Contar plantillas"));
  Serial.println(F("  x                Vaciar base"));
  Serial.println(F("  i                Info (ReadSysPara)"));
  Serial.println(F("  n <id> <nombre>  Setear nombre para ID"));
  Serial.println(F("  ok / err / panel Pruebas de UI"));
  Serial.println(F("  anim             Animar 5s las 4 huellas"));
  Serial.println();
}

inline void handleSerialCommand(
  const String& line,
  DisplayModel& display,        // <- se usa adentro
  FingerprintModel& fpModel,
  NamesModel& names,
  AutoMode& autoMode
) {
  if (line == "s") {
    // solicitar scan (misma acción que API) -> comportamiento idéntico
    Serial.println("Solicitud scan -> esperando dedo...");
    requestScan(15000); // mismo comportamiento que el API
    return;
  }

  if (line == "c") {
    if (fpModel.chip().getTemplateCount() == FINGERPRINT_OK)
      Serial.println(fpModel.chip().templateCount);
    else
      Serial.println("ERR");
    return;
  }

  if (line == "x") {
    Serial.println(fpModel.chip().emptyDatabase() == FINGERPRINT_OK ? "OK" : "ERR");
    return;
  }

  if (line == "i") {
    if (fpModel.chip().getParameters() == FINGERPRINT_OK) {
      Serial.print("capacity=");    Serial.println(fpModel.chip().capacity);
      Serial.print("security=");    Serial.println(fpModel.chip().security_level);
      Serial.print("system_id=0x"); Serial.println(fpModel.chip().system_id, HEX);
      Serial.print("baud=");        Serial.println(fpModel.chip().baud_rate);
      Serial.print("packet_len=");  Serial.println(fpModel.chip().packet_len);
    } else Serial.println("getParameters FAIL");
    return;
  }

  if (line.startsWith("d ")) {
    uint16_t id = line.substring(2).toInt();
    Serial.println(fpModel.chip().deleteModel(id) == FINGERPRINT_OK ? "OK" : "ERR");
    return;
  }

  if (line.startsWith("n ")) {
    int sp = line.indexOf(' ', 2);
    if (sp < 0) { Serial.println("Uso: n <id> <nombre>"); return; }
    uint16_t id = line.substring(2, sp).toInt();
    String name = line.substring(sp + 1); name.trim();
    names.set(id, name);
    Serial.println("Nombre guardado");
    return;
  }

  if (line.startsWith("e ")) {
    uint16_t id = line.substring(2).toInt();
    // Nuevo enrol multi-posiciones (5 imágenes): center, top, bottom, left, right
    Serial.print("Enrolando ID "); Serial.println(id);

    // comprobar capacidad
    if (fpModel.chip().getParameters() != FINGERPRINT_OK) {
      Serial.println("Error leyendo parámetros del sensor");
      return;
    }
    const int capacity = fpModel.chip().capacity;
    const int neededSlots = 5;
    const long baseSlot = (long)id * neededSlots;
    if (baseSlot + (neededSlots - 1) >= capacity) {
      Serial.printf("No hay espacio: capacity=%d, id*%d+4=%ld\n", capacity, neededSlots, baseSlot + 4);
      Serial.println("El ID supera la capacidad disponible para 5-templates por usuario.");
      return;
    }

    const char* posNames[5] = { "CENTER", "TOP", "BOTTOM", "LEFT", "RIGHT" };
   bool allOk = true;
   gBlinkDisp = &display;
   const int maxAttempts = MAX_ENROLL_ATTEMPTS;
   for (int p = 0; p < neededSlots; ++p) {
     uint16_t slot = baseSlot + p;
     Serial.printf("Coloque el dedo en posición %s -> guardando slot %u\n", posNames[p], slot);
     int attempt = 0;
     bool ok = false;
     while (attempt < maxAttempts && !ok) {
       ++attempt;
       // mostrar en pantalla instrucción específica
       display.scanning();
       // opcional: mostrar texto de posición (sin mostrar intentos)
       {
         auto& d = display.raw();
         d.setTextSize(1);
         d.setTextColor(SH110X_WHITE);
         d.setCursor(0, 0);
         d.print(posNames[p]);
         d.display();
       }
       // intentar enroll en este slot
       ok = fpModel.enroll(slot, &BlinkCbThunk);
       if (!ok) {
         Serial.printf("Intento %d falló en slot %u (pos %s)\n", attempt, slot, posNames[p]);
         // mostrar sólo icono de error centrado
         showCenteredIcon(display, ICON_ERR_64);
         delay(700);
       } else {
         Serial.printf("Slot %u guardado correctamente (pos %s)\n", slot, posNames[p]);
       }
       delay(200);
     }
     if (!ok) {
       Serial.printf("Enrolamiento falló en slot %u tras %d intentos (pos %s)\n", slot, maxAttempts, posNames[p]);
       allOk = false;
       break; // no reiniciamos posiciones previas, sólo abortamos el flujo
     }
     delay(300); // pequeño reposo entre posiciones exitosas
   }
    gBlinkDisp = nullptr;

    if (allOk) {
      // pedir nombre
      Serial.print("Ingresá nombre para ID "); Serial.print(id); Serial.println(": ");
      String name = ""; unsigned long t0 = millis();
      while (millis() - t0 < 30000) {
        if (Serial.available()) {
          char c = Serial.read();
          if (c=='\r') continue;
          if (c=='\n') break;
          name += c;
        }
        delay(1);
      }
      name.trim();
      if (name.length()) { names.set(id, name); Serial.println("Nombre guardado."); }
      else Serial.println("Sin nombre (timeout).");
      showCenteredIcon(display, ICON_OK_64);
      delay(1200);
    } else {
      showCenteredIcon(display, ICON_ERR_64);
      delay(1200);
      Serial.println("Enrolamiento falló. Puedes reintentar el enrolamiento para este ID.");
    }
    // volver a idle (AutoMode gestionará el redraw si corresponde)
    display.idle();
    return;
  }

  // Tests UI opcionales (si los usás)
  if (line == "ok")   { showCenteredIcon(display, ICON_OK_64); delay(1500); display.idle(); return; }
  if (line == "err")  { showCenteredIcon(display, ICON_ERR_64); delay(1500); display.idle(); return; }
  if (line == "panel"){ 
    Serial.println("Panel test - showing icons:");
    showCenteredIcon(display, ICON_OK_64); 
    delay(1500);
    showCenteredIcon(display, ICON_ERR_64);
    delay(1500);
    display.idle();
    return; 
  }

  Serial.println("Comando no reconocido.");
  Serial.println(F("  e <id> | s | d <id> | c | x | i | n <id> <nombre>"));
}
