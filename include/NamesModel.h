#pragma once
#include <Arduino.h>
#include <Preferences.h>

class NamesModel {
public:
  bool begin() { return _prefs.begin("users", false); }
  String get(uint16_t id) {
    char key[8]; snprintf(key, sizeof(key), "id%03u", id);
    return _prefs.getString(key, "");
  }
  void set(uint16_t id, const String& name) {
    char key[8]; snprintf(key, sizeof(key), "id%03u", id);
    _prefs.putString(key, name);
  }
private:
  Preferences _prefs;
};
