#include "WatchCore.h"
#include <Wire.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

namespace {

// ---------------------------------------------------------------- config
const uint16_t FRAME_MS      = 33;       // ~30 FPS
const uint16_t DEBOUNCE_MS   = 20;
const uint32_t DIM_AFTER_MS  = 10000UL;  // face only
const uint32_t OFF_AFTER_MS  = 20000UL;  // face only
const uint32_t RING_MAX_MS   = 60000UL;  // alarm gives up after a minute

const uint8_t MDAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const char MONS[] = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";

enum Sys : uint8_t { S_FACE, S_SET, S_RING, S_APP };

watch::WatchApp app;
watch::Pins pins;
uint8_t btnPin[5];

Sys sys = S_FACE;
Sys ringReturn = S_FACE;
unsigned long ringStartMs = 0;
bool ringToneOn = false;

// buttons
uint8_t heldMask = 0, lastRaw = 0, pending = 0;
unsigned long changedAt[5];

// clock
uint32_t tod = 12UL * 3600;       // seconds since midnight
uint8_t mon = 1, dday = 1;
uint32_t alarmTod = 7UL * 3600;
bool alarmEnabled = false;
unsigned long lastSec = 0;

// set screen: 0 time, 1 date, 2 alarm, 3 sound
uint8_t setPg = 0, setFld = 0;
const uint8_t SET_PAGES = 4;

unsigned long lastFrame = 0;
uint32_t frames = 0;
uint16_t workUs = 0;              // how long the last frame's update + draw + send took (µs)
uint16_t workMaxUs = 0;           // the worst since the last "F" query
// USB test hooks: "P <buttons> <ms>" holds buttons, "F" reports frame times
uint8_t injMask = 0; unsigned long injUntil = 0;

// sound: off silences beeps and tunes (the alarm still rings)
bool soundOn = true;

// time from the USB serial port: "T <seconds since midnight> <month> <day>\n" (scripts/settime.sh)
char serBuf[32]; uint8_t serLen = 0;
void serialStep() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      serBuf[serLen] = 0;
      unsigned long t; unsigned m, d;
      unsigned mask, ms;
      if (serLen == 1 && serBuf[0] == 'F') {
        Serial.print("work_us="); Serial.print(workUs); Serial.print(" max_us="); Serial.print(workMaxUs);
        Serial.print(" frames="); Serial.println(frames);
        workMaxUs = 0;
      } else if (serLen && serBuf[0] == 'P' && sscanf(serBuf + 1, "%u %u", &mask, &ms) == 2) {
        injMask = mask & 31; injUntil = millis() + ms;
        Serial.println("OK");
      } else if (serLen && serBuf[0] == 'T' && sscanf(serBuf + 1, "%lu %u %u", &t, &m, &d) == 3 && t < 86400UL && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
        tod = t; mon = m; dday = d; lastSec = millis();
        Serial.println("OK");
      }
      serLen = 0;
    } else if (serLen < sizeof(serBuf) - 1) serBuf[serLen++] = c;
  }
}

// power save (face only): 0 full, 1 dim, 2 off
uint8_t idleStage = 0;
unsigned long lastInputMs = 0;


// tune player
const watch::Note *tune = nullptr;
uint8_t tuneLen = 0, tuneIdx = 0;
unsigned long noteEndMs = 0;

void tuneStep() {
  if (!tune) return;
  unsigned long now = millis();
  if (tuneIdx > 0 && (long)(now - noteEndMs) < 0) return;
  if (tuneIdx >= tuneLen) { tune = nullptr; noTone(pins.buzzer); return; }
  const watch::Note &n = tune[tuneIdx++];
  if (n.freq) {
    // a few ms of silence at the end of each note keeps repeated notes distinct
    uint16_t on = n.ms > 30 ? n.ms - 12 : n.ms;
    tone(pins.buzzer, n.freq, on);
  } else {
    noTone(pins.buzzer);
  }
  noteEndMs = now + n.ms;
}

// ---------------------------------------------------------------- buttons
uint8_t readButtons() {
  unsigned long now = millis();
  uint8_t edges = 0;
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t bit = (uint8_t)(1 << i);
    bool raw = !digitalRead(btnPin[i]) || ((injMask & bit) && (long)(injUntil - now) > 0);
    if (raw != ((lastRaw & bit) != 0)) { lastRaw ^= bit; changedAt[i] = now; }
    if (now - changedAt[i] >= DEBOUNCE_MS) {
      bool isHeld = (heldMask & bit) != 0;
      if (raw != isHeld) {
        heldMask ^= bit;
        if (raw) edges |= bit;
      }
    }
  }
  return edges;
}

// ---------------------------------------------------------------- display power
void displayWake() {
  idleStage = 0;
  lastInputMs = millis();
  u8g2.setPowerSave(0);
}

// This SH1106 ignores setContrast, so "dim" punches out half the lit pixels.
void applySoftwareDim() {
  u8g2.setDrawColor(0);
  for (uint8_t y = 0; y < 64; y++)
    for (uint8_t x = (y & 1); x < 128; x += 2)
      u8g2.drawPixel(x, y);
  u8g2.setDrawColor(1);
}

void updateIdle() {
  if (sys != S_FACE) {
    if (idleStage) displayWake();
    return;
  }
  unsigned long idle = millis() - lastInputMs;
  if (idle >= OFF_AFTER_MS) {
    if (idleStage != 2) {
      idleStage = 2;
      u8g2.clearBuffer();
      u8g2.sendBuffer();
      u8g2.setPowerSave(1);
    }
  } else if (idle >= DIM_AFTER_MS) {
    if (idleStage != 1) { idleStage = 1; u8g2.setPowerSave(0); }
  } else if (idleStage) {
    displayWake();
  }
}

// ---------------------------------------------------------------- alarm
void startRing() {
  if (sys == S_RING) return;
  tune = nullptr;  // the alarm wins over any jingle
  // Set screens are abandoned; the game is frozen and resumed after.
  ringReturn = (sys == S_APP) ? S_APP : S_FACE;
  if (sys == S_APP && app.appInterrupted) app.appInterrupted();
  sys = S_RING;
  ringStartMs = millis();
  ringToneOn = false;
  displayWake();
}

void stopRing() {
  noTone(pins.buzzer);
  ringToneOn = false;
  sys = ringReturn;
  displayWake();
}

void ringTone() {
  bool want = ((millis() - ringStartMs) % 1000) < 300;
  if (want != ringToneOn) {
    ringToneOn = want;
    if (want) tone(pins.buzzer, 1200);
    else noTone(pins.buzzer);
  }
}

void drawRingDefault() {
  if ((millis() / 400) & 1) {
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.setCursor(18, 30);
    u8g2.print(F("WAKE UP!"));
  }
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(18, 50);
  u8g2.print(F("PRESS ANY BUTTON"));
}

// ---------------------------------------------------------------- clock
void tick() {
  if (++tod >= 86400UL) {
    tod = 0;
    if (++dday > MDAYS[mon - 1]) { dday = 1; if (++mon > 12) mon = 1; }
  }
  // Fires from ANY mode (menus, set screens, end screens…), not just face/game
  if (alarmEnabled && tod == alarmTod) startRing();
}

// ---------------------------------------------------------------- set screens
void adjust(int8_t d) {
  if (setPg == 0) {
    uint8_t h = tod / 3600, m = (tod / 60) % 60, s = tod % 60;
    if (setFld == 0) h = (h + d + 24) % 24; else m = (m + d + 60) % 60;
    tod = h * 3600UL + m * 60UL + s;
  } else if (setPg == 1) {
    if (setFld == 0) mon = (mon - 1 + d + 12) % 12 + 1;
    uint8_t dim = MDAYS[mon - 1];
    if (dday > dim) dday = dim;
    if (setFld == 1) dday = (dday - 1 + d + dim) % dim + 1;
  } else if (setPg == 3) {
    soundOn = !soundOn;
    if (soundOn) tone(pins.buzzer, 1200, 60);
  } else {
    if (setFld == 2) { alarmEnabled = !alarmEnabled; return; }
    uint8_t h = alarmTod / 3600, m = (alarmTod / 60) % 60;
    if (setFld == 0) h = (h + d + 24) % 24; else m = (m + d + 60) % 60;
    alarmTod = h * 3600UL + m * 60UL;
  }
}

void setStep(uint8_t e) {
  if (e & B_MODE) { setPg++; setFld = 0; if (setPg >= SET_PAGES) sys = S_FACE; return; }
  if (e & B_LEFT) adjust(-1);
  if (e & B_RIGHT) adjust(1);
  if (e & B_MID) {
    setFld++;
    if (setFld >= ((setPg == 2) ? 3 : setPg == 3 ? 1 : 2)) { setFld = 0; sys = S_FACE; }
  }
}

void drawSetDefault() {
  using namespace watch;
  u8g2.setFont(u8g2_font_6x10_tr);
  if (setPg == 3) {                         // sound on/off
    u8g2.setCursor(46, 12); u8g2.print(F("SOUND"));
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.setCursor(soundOn ? 50 : 42, 40); u8g2.print(soundOn ? F("ON") : F("OFF"));
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 54); u8g2.print(F("L/R change"));
    u8g2.setCursor(0, 63); u8g2.print(F("ALARM RINGS"));
    u8g2.setCursor(72, 63); u8g2.print(app.nextLabel ? app.nextLabel : "MID"); u8g2.print(F(" done"));
    return;
  }
  u8g2.setCursor(setPg == 2 ? 40 : 30, 12);
  u8g2.print(setPg == 0 ? F("SET TIME") : setPg == 1 ? F("SET DATE") : F("ALARM"));
  u8g2.setFont(u8g2_font_logisoso20_tn);
  u8g2.setCursor(20, 40);
  if (setPg == 0) { print2(hours()); u8g2.print(':'); print2(minutes()); }
  else if (setPg == 1) { printMonth(mon); u8g2.print(' '); print2(dday); }
  else { print2(alarmHours()); u8g2.print(':'); print2(alarmMinutes()); }
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 54);
  u8g2.print(F("L/R change"));
  u8g2.setCursor(0, 63);
  if (setPg == 2 && setFld == 2) { u8g2.print(F("ALARM ")); u8g2.print(alarmEnabled ? F("ON") : F("OFF")); }
  else if (setPg == 1) u8g2.print(setFld ? F("DAY") : F("MONTH"));
  else u8g2.print(setFld ? F("MINUTE") : F("HOUR"));
  u8g2.setCursor(72, 63);
  u8g2.print(app.nextLabel ? app.nextLabel : "MID");
  u8g2.print(F(" next"));
}

// ---------------------------------------------------------------- frame
void update(uint8_t e) {
  switch (sys) {
    case S_FACE:
      if (idleStage < 2 && app.faceStep) app.faceStep();
      if (e & B_MODE) { sys = S_SET; setPg = 0; setFld = 0; }
      else if (e & B_GAME) { sys = S_APP; app.appStart(); }
      break;
    case S_SET:
      setStep(e);
      break;
    case S_RING:
      if (e || millis() - ringStartMs >= RING_MAX_MS) stopRing();
      else ringTone();
      break;
    case S_APP:
      app.appUpdate(e);
      break;
  }
}

void render() {
  u8g2.setDrawColor(1);
  switch (sys) {
    case S_FACE: app.faceDraw(); break;
    case S_SET:  if (app.setDraw) app.setDraw(); else drawSetDefault(); break;
    case S_RING: if (app.ringDraw) app.ringDraw(); else drawRingDefault(); break;
    case S_APP:  app.appDraw(); break;
  }
  u8g2.setDrawColor(1);
}

}  // namespace

// ================================================================ public
void beep(uint16_t freq, uint16_t ms) {
  if (sys == S_RING || tune || !soundOn) return;  // don't stomp on the alarm or a tune; muted: silent
  tone(pins.buzzer, freq, ms);
}

namespace watch {

void begin(const WatchApp &a, const Pins &p) {
  app = a;
  pins = p;
  btnPin[0] = p.left; btnPin[1] = p.mid; btnPin[2] = p.right;
  btnPin[3] = p.mode; btnPin[4] = p.game;
  for (uint8_t i = 0; i < 5; i++) pinMode(btnPin[i], INPUT_PULLUP);
  pinMode(p.buzzer, OUTPUT);
  Wire.setSDA(p.sda);
  Wire.setSCL(p.scl);
  Wire.setClock(400000);
  Serial.begin(115200);   // USB: accepts the time from scripts/settime.sh
  u8g2.begin();
  u8g2.setBusClock(800000);   // 400 kHz spent ~23 ms of the 33 ms frame sending the screen; 800 kHz: ~19 ms frames
  u8g2.setBitmapMode(1);  // bitmap 0-bits are transparent
  lastSec = millis();
  displayWake();
}

void run() {
  serialStep();
  pending |= readButtons();
  tuneStep();
  unsigned long now = millis();
  while (now - lastSec >= 1000) { lastSec += 1000; tick(); }

  if (pending) {
    lastInputMs = now;
    if (sys == S_FACE && idleStage == 2) {
      // Screen was off: this press only wakes the watch, it doesn't act
      displayWake();
      pending = 0;
    } else if (sys == S_FACE && idleStage) {
      displayWake();
    }
  }

  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  uint8_t ev = pending;
  pending = 0;

  unsigned long t0 = micros();
  update(ev);
  updateIdle();
  if (sys == S_FACE && idleStage == 2) return;  // screen off: no I2C traffic

  frames++;
  u8g2.clearBuffer();
  render();
  if (sys == S_FACE && idleStage == 1) applySoftwareDim();
  u8g2.sendBuffer();
  unsigned long dt = micros() - t0; workUs = dt > 65535UL ? 65535 : (uint16_t)dt;
  if (workUs > workMaxUs) workMaxUs = workUs;
}

void exitToClock() {
  sys = S_FACE;
  displayWake();
}

bool inApp() { return sys == S_APP; }

void playTune(const Note *notes, uint8_t count) {
  if (sys == S_RING || !soundOn) return;
  tune = notes;
  tuneLen = count;
  tuneIdx = 0;
  tuneStep();
}
bool tunePlaying() { return tune != nullptr; }
void stopTune() { tune = nullptr; noTone(pins.buzzer); }
uint8_t held() { return heldMask; }
uint32_t frameCount() { return frames; }
uint16_t frameWorkUs() { return workUs; }

uint8_t hours() { return tod / 3600; }
uint8_t minutes() { return (tod / 60) % 60; }
uint8_t seconds() { return tod % 60; }
uint8_t month() { return mon; }
uint8_t day() { return dday; }
bool alarmOn() { return alarmEnabled; }
bool soundEnabled() { return soundOn; }
uint8_t alarmHours() { return alarmTod / 3600; }
uint8_t alarmMinutes() { return (alarmTod / 60) % 60; }

uint8_t setPage() { return setPg; }
uint8_t setField() { return setFld; }

void print2(uint8_t v) {
  if (v < 10) u8g2.print('0');
  u8g2.print(v);
}

void printMonth(uint8_t m) {
  if (m < 1 || m > 12) m = 1;
  for (uint8_t i = 0; i < 3; i++) u8g2.print(MONS[(m - 1) * 3 + i]);
}

}  // namespace watch
