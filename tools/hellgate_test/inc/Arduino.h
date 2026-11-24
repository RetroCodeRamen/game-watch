#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
class __FlashStringHelper;
#define F(s) ((const __FlashStringHelper*)(s))
extern unsigned long g_ms;
inline unsigned long millis() { return g_ms; }
inline unsigned long micros() { return g_ms * 1000; }
inline long random(long n) { return n ? rand() % n : 0; }
inline long random(long a, long b) { return a + (b > a ? rand() % (b - a) : 0); }
inline void randomSeed(unsigned long s) { srand(s); }
using std::min; using std::max;
template<class T> T constrain(T v, T a, T b) { return v < a ? a : v > b ? b : v; }
#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
