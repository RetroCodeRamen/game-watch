#pragma once
#include "Arduino.h"
#include "U8g2lib.h"
#include "WatchGfx.h"
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;
enum : uint8_t { B_LEFT = 1, B_MID = 2, B_RIGHT = 4, B_MODE = 8, B_GAME = 16 };
inline void beep(uint16_t, uint16_t) {}
namespace watch {
struct Note { uint16_t freq; uint16_t ms; };
extern uint8_t g_held; extern int g_tune;
inline void playTune(const Note *, uint8_t c) { g_tune = 30 * c; }
inline bool tunePlaying() { return g_tune > 0; }
inline void stopTune() { g_tune = 0; }
struct WatchApp { void (*faceStep)(); void (*faceDraw)(); void (*appStart)(); void (*appUpdate)(uint8_t); void (*appDraw)(); void (*appInterrupted)(); void (*setDraw)(); void (*ringDraw)(); const char *nextLabel; };
inline void begin(const WatchApp &) {}
inline void run() {}
inline void exitToClock() {}
inline uint16_t frameWorkUs() { return 0; }
inline bool inApp() { return true; }
inline uint8_t held() { return g_held; }
inline uint32_t frameCount() { return 0; }
inline uint8_t hours() { return 10; } inline uint8_t minutes() { return 10; } inline uint8_t seconds() { return 0; }
inline uint8_t month() { return 12; } inline uint8_t day() { return 25; }   // screenshots: Christmas morning
inline bool alarmOn() { return false; } inline uint8_t alarmHours() { return 0; } inline uint8_t alarmMinutes() { return 0; }
inline uint8_t setPage() { return 0; } inline uint8_t setField() { return 0; }
void print2(uint8_t); inline void printMonth(uint8_t) {}
}
