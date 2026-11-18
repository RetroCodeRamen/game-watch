#pragma once
// WatchCore — shared shell for every Game Watch firmware.
//
// Owns: buttons, clock + alarm, set screens, clock-face power save, frame loop.
// Each firmware owns: its themed clock FACE art and its game (via WatchApp hooks).
//
//   const watch::WatchApp APP = { faceStep, faceDraw, gameStart, gameUpdate, gameDraw };
//   void setup() { watch::begin(APP); }
//   void loop()  { watch::run(); }
//
// System flow:  FACE ⇄ SET          FACE --Game--> APP --watch::exitToClock()--> FACE
//               any mode --alarm--> RING --any key--> back where you were
//
// Build: arduino-cli compile --libraries <repo>/libraries ...  (scripts/flash.sh does this)

#include <Arduino.h>
#include <U8g2lib.h>
#include "WatchGfx.h"

// Shared display (defined in WatchCore.cpp)
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// Physical button bits (edge masks from the frame loop, and watch::held()).
// Games may alias these to logical names, e.g. `const uint8_t B_UP = B_MID;`
enum : uint8_t { B_LEFT = 1, B_MID = 2, B_RIGHT = 4, B_MODE = 8, B_GAME = 16 };

// Short non-blocking beep on the shared buzzer
void beep(uint16_t freq, uint16_t ms);

namespace watch {

struct Pins {
  uint8_t left, mid, right, mode, game;  // buttons, active-low with pull-ups
  uint8_t buzzer, sda, scl;
};
// Current board — see docs/HARDWARE.md. Pass a different Pins to begin() for new hardware.
static const Pins DEFAULT_PINS = {2, 3, 6, 7, 8, 15, 4, 5};

struct WatchApp {
  // ---- Theme: clock face (required) ----
  void (*faceStep)();               // advance face animation; called each frame the face is lit
  void (*faceDraw)();               // draw the whole face (time, date, alarm badge, art)

  // ---- Game (required) ----
  void (*appStart)();               // Game pressed on the face
  void (*appUpdate)(uint8_t edges); // one frame of game logic (edges = new presses)
  void (*appDraw)();                // draw one game frame

  // ---- Optional (nullptr = default behaviour) ----
  void (*appInterrupted)();         // alarm took over mid-game: e.g. switch to your pause menu
  void (*setDraw)();                // themed set-time/date/alarm screen (see setPage/setField)
  void (*ringDraw)();               // themed alarm-ringing screen
  const char *nextLabel;            // set-screen hint for the Mid button (default "MID")
};

void begin(const WatchApp &app, const Pins &pins = DEFAULT_PINS);
void run();                         // call from loop(); paces itself to ~30 FPS

// ---- System ----
void exitToClock();                 // leave the game; face is shown on the next frame
bool inApp();                       // true while the game (not face/set/ring) owns the screen
uint8_t held();                     // buttons currently held (debounced)
uint32_t frameCount();              // increments once per rendered frame

// ---- Clock (read by faces) ----
uint8_t hours();
uint8_t minutes();
uint8_t seconds();
uint8_t month();                    // 1..12
uint8_t day();                      // 1..31
bool alarmOn();
uint8_t alarmHours();
uint8_t alarmMinutes();

// ---- Set screen state (for a themed setDraw) ----
uint8_t setPage();                  // 0 time, 1 date, 2 alarm
uint8_t setField();                 // time/alarm: 0 hour 1 minute (alarm 2 = on/off); date: 0 month 1 day

// ---- Text helpers (print at the current u8g2 cursor) ----
void print2(uint8_t v);             // zero-padded two digits
void printMonth(uint8_t m);         // "JAN".."DEC"

}  // namespace watch
