#pragma once
#include <string.h>
#include <stdint.h>
struct EEPROMClass {
  uint8_t d[4096];
  void begin(int) {}
  template<class T> void put(int a, const T &v) { memcpy(d + a, &v, sizeof v); }
  template<class T> void get(int a, T &v) { memcpy(&v, d + a, sizeof v); }
  bool commit() { return true; }
};
inline EEPROMClass EEPROM;   // host stub (one definition for every harness)
