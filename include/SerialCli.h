#pragma once
#include <Arduino.h>
#include "AutoMode.h"
#include "DisplayModel.h"
#include "FingerprintModel.h"
#include "NamesModel.h"


namespace {
  static DisplayModel* gBlinkDisp = nullptr;
  static void BlinkCbThunk(bool on) {
    if (gBlinkDisp) gBlinkDisp->scanBlinkTick(on);
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
    Serial.println(F("Apoyá el dedo..."));
    display.scanning();

    gBlinkDisp = &display;
    MatchRes m = fpModel.fastMatch(&BlinkCbThunk);
    gBlinkDisp = nullptr;

    if (m.ok) {
      String name = names.get(m.id);
      display.welcome(name, m.id, m.score);
      Serial.print(F("Match ID=")); Serial.print(m.id);
      Serial.print(F(" score=")); Serial.println(m.score);
      delay(1500);
      display.idle();
    } else {
      display.errorMsg("Sin coincidencia");
      Serial.println(F("No se encontró match"));
      delay(900);
      display.idle();
    }
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
    Serial.print("Enrolando ID "); Serial.println(id);

    display.scanning();
    gBlinkDisp = &display;
    bool okEnroll = fpModel.enroll(id, &BlinkCbThunk);
    gBlinkDisp = nullptr;

    if (okEnroll) {
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
      display.okMsg(String("ID ") + id);
    } else {
      display.errorMsg("Enrolamiento falló");
      Serial.println("Enrolamiento falló.");
    }
    display.idle();
    return;
  }

  // Tests UI opcionales (si los usás)
  if (line == "ok")   { display.welcome("TEST", 123, 99); delay(1500); display.idle(); return; }
  if (line == "err")  { display.errorMsg("Prueba error"); delay(1500); display.idle(); return; }
  if (line == "panel"){ autoMode.showPanelTest(); return; }

  Serial.println("Comando no reconocido.");
  Serial.println(F("  e <id> | s | d <id> | c | x | i | n <id> <nombre>"));
}
