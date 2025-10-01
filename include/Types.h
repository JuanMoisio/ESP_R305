#pragma once
#include <Arduino.h>

enum class FPResult : uint8_t { Ok, Timeout, ImageFail, NoMatch, Error };
struct FPMatch { bool ok; int id; int score; String err; };

enum class OledState : uint8_t {
  Idle,
  Scanning,
  Ok,
  Error,
  Enroll1,
  Enroll2
};