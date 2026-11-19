// Sonic-style platformer + watch for RP2040-Zero
// 4 zones × 2 acts, Egg Wrecker bosses, special-stage emeralds
// Board: Waveshare RP2040-Zero | Core: Earle Philhower | Libs: U8g2, WatchCore
//
// Clock, alarm, set screens and buttons live in libraries/WatchCore.
// This sketch supplies the Sonic-themed clock face and the game (see WatchApp at the bottom).

#include <Arduino.h>
#include <math.h>
#include <WatchCore.h>
#include "title_bitmap.h"
#include "sonic_sprites.h"
#include "sonic_levels.h"

// Fixed-point type must be visible before Arduino's auto-prototypes
typedef int32_t fx;

void drawHog(int16_t x, int16_t y, bool roll, bool faceRight, uint8_t frame);
void drawHogAt(int16_t x, int16_t y, bool roll, bool faceRight, uint8_t frame, bool moving);
void drawSunflower(int16_t x, int16_t baseY);
void drawRingGlyph(int16_t x, int16_t y);

// Game-side modes only; clock face / set / alarm are owned by WatchCore
enum : uint8_t { SPLASH, GAME, MENU, OVER, WIN, SPECIAL, CLEAR, DEAD, SELECT };
uint8_t mode = SPLASH;

// ---- Screen transitions: dither fade out → switch screens → fade back in ----
// Like the Genesis palette fades: nothing changes screens with a hard cut. The game freezes
// while fading out; the new screen runs while it fades in (title cards slide in under it).
enum : uint8_t { TR_NONE, TR_OUT, TR_IN };
const uint8_t TR_STEPS = 14;          // frames each way (~0.47 s)
uint8_t trState, trLevel;             // trLevel 0 = clear … TR_STEPS = fully faded
bool trWhite;                         // fade to white (special stage) instead of black
void (*trAction)();
void transitionTo(void (*action)(), bool white = false) {
  if (trState == TR_OUT) return;      // already on the way out
  trState = TR_OUT; trWhite = white; trAction = action;
}
uint8_t splashFrame = 0;
// Title-screen secret: Left Left Right Right Left Right, then Jump → level select.
// Last six Left/Right presses, two bits each (1 = Left, 2 = Right).
uint16_t codeHist = 0;
const uint16_t LEVEL_SELECT_CODE = (1 << 10) | (1 << 8) | (2 << 6) | (2 << 4) | (1 << 2) | 2;
uint8_t selIdx = 0;          // level-select cursor

// Watch-face Sonic vignette (walk + spin-jump for a ring)
int16_t wfX = -14;
int16_t wfY = 50;
int8_t  wfVy = 0;
uint8_t wfAnim = 0;
uint8_t wfRingCD = 0;     // frames until ring respawns
uint8_t wfPause = 0;      // brief hold before looping from the left
bool    wfRingLive = true;
bool    wfAir = false;

void drawSplash() {
  // Title screen shown when GAME button is pressed from watch mode
  u8g2.drawXBMP(0, 0, TITLE_W, TITLE_H, titleBitmap);

  if (splashFrame < 16) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(splashFrame * 8, 0, 128 - splashFrame * 8, 64);
    u8g2.setDrawColor(1);
  }

  if (splashFrame > 18 && ((splashFrame / 8) & 1)) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(10, 54, 108, 10);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(10, 54, 108, 10);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(16, 62);
    u8g2.print(F("PUSH JUMP TO START"));
  }
}

void stepWatchFace() {
  const int16_t ground = 50;   // Sonic centre: feet land on the dashed line at y 56
  const int16_t ringX = 118;
  const int16_t ringY = 20;
  wfAnim++;

  if (wfRingCD) {
    wfRingCD--;
    if (!wfRingCD) wfRingLive = true;
  }

  // Pause off the left before the next lap
  if (wfPause) { wfPause--; return; }

  if (!wfAir) {
    wfX += 2;
    if (wfRingLive && wfX >= 100 && wfX <= 106) {
      wfAir = true;
      wfVy = -8;  // high enough to reach the clock-side ring
    }
  } else {
    // Keep sprite fully clear of the right edge (12px wide, center=wfX)
    if (wfX < 114) wfX += 2;
    wfY += wfVy;
    wfVy += 1;
    int16_t by = wfY - 6;
    if (wfRingLive && abs(wfX - ringX) < 14 && abs(by - ringY) < 12) {
      wfRingLive = false;
      wfRingCD = 100;
      beep(1400, 40);
    }
    if (wfY >= ground) {
      wfY = ground;
      wfVy = 0;
      wfAir = false;
    }
  }

  // After the jump, leave before the sprite can kiss column 128 (wraps to col 0-2)
  if (!wfAir && wfX > 118) {
    wfX = -16;
    wfY = ground;
    wfVy = 0;
    wfPause = 90;
  }
}

// ---- Theme: Sonic clock face (WatchApp::faceDraw) ----
void drawView() {
  using namespace watch;
  u8g2.setDrawColor(1);
  for (uint8_t i = 0; i < 128; i += 8) hLineSafe(i, 56, 4);

  // Left deco / center time / right ring — sized so they don't collide
  drawSunflower(10, 54);

  u8g2.setFont(u8g2_font_logisoso24_tn);
  u8g2.setCursor(30, 36); print2(hours()); u8g2.print(':'); print2(minutes());

  if (wfRingLive) drawRingGlyph(118, 20);
  else if ((wfRingCD > 70) && ((wfAnim >> 1) & 1)) {
    plot(118, 18); plot(116, 20); plot(120, 20); plot(118, 22);
  }

  drawHogAt(wfX, wfY, wfAir, true, wfAnim, !wfAir);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(2, 63); printMonth(month()); u8g2.print(' '); print2(day());
  u8g2.setCursor(100, 63); print2(seconds());
  if (alarmOn()) {
    u8g2.setCursor(40, 9); u8g2.print(F("ALM "));
    print2(alarmHours()); u8g2.print(':'); print2(alarmMinutes());
  }
}

// ===================== GAME =====================
// kinds: 0 spike 1 ring 2 crab 3 spring 4 bee
//        5 deco (state: 0 sun 1 palm 2 pipe 3 neon 4 gear)
//        6 goal 7 wall 8 flame/chem trap 9 bumper 10 saw
struct Obj { int16_t x, y, home; uint8_t kind, state, timer; int8_t vx; bool live; };
struct Plat { int16_t x, y, w; bool live; };
struct Shot { int16_t x, y; int16_t vx, vy; int16_t qx, qy; bool live; };   // q/v in 1/16 px
struct Scatter { int16_t x, y; int8_t vx, vy; uint8_t life; bool live; };

// Whole act stays resident — no scroll-despawn (draw/AI cull only)
Obj objs[256];   // a hand-built act is ~150–230 objects (each ring is one)
Plat plats[64];
// Green Hill log bridges: a walkway that sags toward wherever Sonic stands
struct Bridge { int16_t x0, x1, y; bool live; };
Bridge bridges[6];
int8_t curBridge = -1;      // bridge Sonic is standing on (-1 none)
int16_t camLead = 0;        // camera look-ahead (px, + = looking right)
bool onLedge;               // standing on a platform/bridge (terrain slope below doesn't apply)
uint8_t deadT;              // death animation frame counter
Shot shots[6];
Scatter scat[3];
const uint8_t SCATTER_LIFE = 90;
const uint8_t SCATTER_GRACE = 30;
int16_t ballWX, ballWY;


// Deterministic layout hash (no random() in level gen)
uint8_t h8(uint16_t n) {
  n = (uint16_t)(n ^ (n << 7));
  n = (uint16_t)(n ^ (n >> 5));
  n = (uint16_t)(n ^ (n << 3));
  return (uint8_t)n;
}

// Fixed-point physics (1/256 px), 60Hz steps.
// ZOOM: the whole world is drawn 5/4 bigger than the level data (sonic_levels.h stays in
// design units). Level positions, sizes, hitboxes, speeds and accelerations all scale by it,
// so timing and feel are unchanged — only the pixels are bigger. Z() scales a design length.
#define Z(v) ((int16_t)(((int32_t)(v) * 5) / 4))
// Watch-tuned: snappy accel, firmer coast (less slippery), hard skid ≈11× accel.
// Design values (pre-zoom) in comments; every speed/accel below is design × 5/4.
static const fx FX_ONE      = 256;
static const fx ACCEL       = 37;      // 30  — snappy launch
static const fx FRICTION    = 70;      // 56  — firmer coast than accel (watch grip)
static const fx BRAKE       = 412;     // 330 — ~11× accel, deliberate skid
static const fx TOP_SPD     = 1920;    // 1536 — 7.5 px/step
static const fx START_KICK  = 160;     // 128 — immediate bite from standstill
static const fx AIR_ACCEL   = 22;      // 18  — air steer so jumps feel controllable
static const fx JUMP_SPD    = 700;     // 560 — ~48 px: 2.2 Sonic heights, the Genesis ratio
static const fx SPRING_SPD  = 890;     // 712 — yellow spring ≈ 77 px (levels are spaced for this)
static const fx GRAVITY     = 20;      // 16
static const fx ROLL_FRIC   = 10;      // 8
static const fx MAX_FALL    = 1462;    // 1170
// Host tests define this to log every place Sonic gets stopped (no-op on the watch)
#ifndef STOP_HOOK
#define STOP_HOOK(reason, x0, x1)
#endif
static const int16_t CLIFF_STEP = 7;
// In the air the Genesis wall sensors sit at Sonic's middle, so a ledge whose top is level
// with his body still catches him (floor sensors pop him up onto it). Same leniency here.
static const int16_t CLIFF_STEP_AIR = 12;   // ground rising more than this within Sonic's half-width = a wall
static const int16_t RAD_STAND = 6, RAD_ROLL = 5;   // feet below centre (5 / 4 before the zoom)
// Slope pull per px of height change over 10 px (slopeAt). Genesis proportions to our ACCEL:
// the max slope you can walk up from rest is ~21° (as in Sonic 1); rolling 2.5× down, 0.625× up.
static const fx SLOPE_RUN     = 10;
static const fx SLOPE_ROLL    = 25;
static const fx SLOPE_ROLL_UP = 6;
static const fx UNROLL_SPD    = ZV(1) / 2;   // roll slower than this → stand up
// Vertical launches, set against the jump (gravity here is scaled for the 64-px screen, so
// Genesis ysp values can't be copied directly). Heights: jump 46, hurt ~20, stomp = the fall.
static const fx HURT_VY       = 470;         // Genesis hurt ysp −4 vs jump −6.5 → ~0.45× jump height
static const fx BOSS_BOUNCE   = 560;         // off the top of the Egg Mobile (~30 px)
static const fx BUMPER_VY     = 780;         // Casino bumper launch up (~60 px)
// Stomp: reverse the fall like the Genesis (ysp = −ysp), kept between ½ and 1 jump
inline fx stompVY(fx fall) { if (fall > JUMP_SPD) fall = JUMP_SPD; if (fall < JUMP_SPD / 2) fall = JUMP_SPD / 2; return -fall; }
// Loop / steep-track rules from Sonic 1 (Sonic_SlopeResist / Sonic_SlopeRepel).
// LOOP_SLP is the slope factor: Genesis 0.125 px/frame² on a much bigger loop; scaled up for
// our ~40 px-radius track so it takes the same share of top speed to get round.
static const fx LOOP_SLP    = 100;                // 80 — px/step² · sin(angle): ~1 screen run-up
static const fx LOOP_PUSH   = (LOOP_SLP * 3) / 8; // holding forward (Genesis accel : slope = 3:8)
static const fx LOOP_BRAKE  = LOOP_SLP * 4;       // holding back (Genesis decel : slope = 4:1)
static const fx LOOP_FALL   = 800;                // 640 — slower than this on a wall → fall
static const uint8_t LOOP_LOCK = 30;              // L/R input lock after falling off (frames)

fx fpx, fpy, fvx, fvy;
int16_t px, py, pvx, pvy, cam, camY;   // camY unlocks tall loops
int16_t actEnd;
uint16_t rings;
uint32_t score;
const ActDef *lvl = &ACTS[0][0];   // current act layout
uint16_t actFrames;                // act timer (display frames, ~33 ms each)
uint8_t goalT;                     // >0: passed the signpost, counting the spin
uint8_t lastRank;                  // 0 A+, 1 B, 2 D (for the tally)
uint32_t tallyRingBonus, tallyTimeBonus;

const uint8_t GOAL_SPIN_FRAMES = 60;  // signpost spin (~2 s), slowing to a stop
const uint8_t GOAL_MIN_FRAMES = 45;   // earliest the tally can follow

// Player input — past the signpost Sonic runs off to the right on his own
uint8_t heldIn() { return goalT ? B_RIGHT : watch::held(); }

// Act time in whole seconds (frames are ~33 ms)
uint16_t actSeconds() { return (uint16_t)(((uint32_t)actFrames * 33) / 1000); }
void printTime(uint16_t secs) {
  u8g2.print(secs / 60); u8g2.print(':'); watch::print2(secs % 60);
}

// Original act-clear fanfare (~3.3 s). Plays on the buzzer pin until a speaker is fitted.
static const watch::Note ACT_CLEAR_TUNE[] = {
  {784, 110}, {1047, 110}, {1319, 110}, {1568, 220}, {0, 40},
  {1319, 110}, {1568, 330}, {0, 80},
  {1397, 110}, {1760, 110}, {2093, 220}, {0, 40},
  {1760, 110}, {2093, 440}, {0, 60},
  {1568, 110}, {1760, 110}, {1976, 110}, {2093, 520},
};
uint8_t lives, invul, intro, animFrame, menuSel;
uint8_t zone, act;          // zone 0..3, act 0=Act1 1=Act2
uint8_t emeralds;
uint16_t clearT;            // tally / special-stage frame counter
uint16_t tallyDoneT;        // frame the bonus count finished (0 = still counting)
uint8_t lastTapDir;         // 1=L 2=R
unsigned long lastTapMs;
bool onGround, rolling, faceRight, groundRoll, jumpCut;
bool bossOn;
bool restartPending;        // death → rebuild act at end of gameStep()
int16_t midPX, midPY;       // player pos after the 1st of 2 physics steps (sweep sample)

struct LoopTrack { int16_t cx, cy, r; bool live; };
LoopTrack loopTrack;
bool inLoop;
float loopAng;       // radians; 0=right, π/2=bottom, π=left, 3π/2=top (y-down)
fx loopGspd;         // signed speed along +angle tangent (carry / gravity / input)
float loopSense;     // −1 entered from the left (up the right wall first), +1 from the right
float loopProg;      // radians travelled forward from the bottom; ≥ 2π = full lap
bool inHollow;       // fell off the track: free fall inside the loop
int8_t loopSide;     // side he came in from while under the loop (−1 left, +1 right, 0 none)
bool loopPassed;     // completed the lap — don't re-grab the track until he's clear
bool loopRolled;     // entered the loop rolling (uses rolling slope factors, no forward push)
uint8_t loopLock;    // Genesis control lock after slipping off a wall (counts down on ground)
uint8_t bossHP, bossCool, bossPhase;
int16_t eggX, eggY;              // Egg Wrecker ship centre (world)
int8_t eggDir;
// Egg Wrecker motion: the ship glides between the arena ends, pausing to turn; the ball hangs
// on a chain from its belly and swings as a pendulum (rises at the ends, fastest at the bottom)
enum { EGG_ENTER, EGG_LOWER, EGG_FIGHT, EGG_BLOWUP, EGG_FLEE };
float eggFX;                     // sub-pixel ship x
float pendPhase, pendAmp;        // swing phase (radians) and amplitude
float chainLen;                  // grows from 0 when the ball is let down
float ballAng;                   // current chain angle (0 = straight down, + = to the right)
uint8_t eggT, eggPause, eggLaugh;
int16_t arenaOrigin;             // world X where boss arena starts

// Special stage
uint8_t spRings, spGot;
uint16_t spTimer;
int16_t spEX, spEY;

// Ground height from the act's terrain profile (linear between points)
// Ground height at world x. The profile is in design units: find the segment in data space
// (world x × 4/5) and interpolate, then scale the height by 5/4 — all in one division.
static inline int32_t floorDiv(int32_t a, int32_t b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
int16_t terrainAt(int32_t wx) {
  const TerrainPt *t = lvl->ter;
  uint8_t n = lvl->nTer;
  int32_t q = wx * 4;                    // world x in units of 1/5 design px
  if (q <= (int32_t)t[0].x * 5) return Z(t[0].y);
  if (q >= (int32_t)t[n - 1].x * 5) return Z(t[n - 1].y);
  uint8_t lo = 0, hi = n - 1;            // binary search: t[lo].x <= x < t[hi].x
  while (hi - lo > 1) {
    uint8_t mid = (lo + hi) / 2;
    if ((int32_t)t[mid].x * 5 <= q) lo = mid; else hi = mid;
  }
  int32_t den = ((int32_t)t[hi].x - t[lo].x) * 5;
  int32_t num = q - (int32_t)t[lo].x * 5;
  int32_t y5 = ((int32_t)t[lo].y * den + (int32_t)(t[hi].y - t[lo].y) * num) * 5;
  return (int16_t)floorDiv(y5, den * 4);
}

int16_t groundAt(int32_t wx) {
  if (wx < 0) wx = 0;
  // Boss arena: flat ground
  if (bossOn && wx >= arenaOrigin) return 52;
  // Flatten a long runway into / under the loop (solid floor — no pits)
  if (loopTrack.live) {
    int16_t floorY = loopTrack.cy + loopTrack.r;
    int16_t x0 = loopTrack.cx - loopTrack.r - Z(100);
    int16_t x1 = loopTrack.cx + loopTrack.r + Z(48);
    if (wx >= x0 && wx <= x1) return floorY;
  }
  return terrainAt(wx);
}

int16_t sx(int16_t wx) { return wx - cam; }
int16_t sy(int16_t wy) { return wy - camY; }

bool onScreen(int16_t wx, int16_t margin = 16) {
  int16_t s = wx - cam;
  return s >= -margin && s <= 128 + margin;
}

static inline fx I2F(int16_t v) { return (fx)v * FX_ONE; }
// A design-units speed (px/step before the zoom) in world fixed-point
static inline fx ZV(int16_t v) { return (fx)v * FX_ONE * 5 / 4; }
static inline int16_t F2I(fx v) {
  return (int16_t)((v >= 0) ? ((v + 128) >> 8) : -(((-v) + 128) >> 8));
}
void syncPlayerInts() {
  px = F2I(fpx); py = F2I(fpy);
  pvx = F2I(fvx); pvy = F2I(fvy);
}
void setPlayerPos(int16_t x, int16_t y) {
  fpx = I2F(x); fpy = I2F(y); syncPlayerInts();
}
void setPlayerVel(fx vx, fx vy) {
  fvx = vx; fvy = vy; syncPlayerInts();
}

// Approx slope: + = downhill to the right (ground Y grows downward)
// Height change over 10 px around wx (8 design px). Each half is measured separately so a cliff edge
// (a step bigger than any slope) doesn't count as a hill and shove Sonic off it.
int16_t slopeAt(int16_t wx) {
  int16_t g = groundAt(wx);
  int16_t a = g - groundAt(wx - 5), b = groundAt(wx + 5) - g;
  if (abs(a) > CLIFF_STEP) a = b;
  if (abs(b) > CLIFF_STEP) b = a;
  if (abs(a) > CLIFF_STEP) return 0;
  return a + b;
}

void placeGreenHillLoop(int16_t x) {
  // Diameter > screen height — camera must scroll vertically through it
  loopTrack.r = Z(38);
  int16_t g = terrainAt(x);
  loopTrack.cx = x + loopTrack.r;
  loopTrack.cy = g - loopTrack.r;   // bottom of track on the floor
  loopTrack.live = true;
}

// Strip hazards from the loop approach + ≥100px after the exit
void clearLoopRunway() {
  if (!loopTrack.live) return;
  int16_t L = loopTrack.cx - loopTrack.r - Z(110);
  int16_t R = loopTrack.cx + loopTrack.r + Z(100);
  for (auto &o : objs) {
    if (!o.live) continue;
    if (o.x < L || o.x > R) continue;
    if (o.kind == 1 || o.kind == 5 || o.kind == 6) continue;  // rings / deco / goal
    // Keep the run-up spring if it's near the start of the clear zone
    if (o.kind == 3 && o.x < L + Z(40)) continue;
    o.live = false;
  }
  for (auto &p : plats) {
    if (!p.live) continue;
    if (p.x + p.w < L || p.x > R) continue;
    p.live = false;
  }
}

// Chemical Plant signature: two wavy crossing tube runs
void placeChemWavyCross(int16_t x0) {
  static const int8_t A[7] = { 0, -5, -10, -14, -10, -5, 0 };
  static const int8_t B[7] = { -14, -10, -5, 0, -5, -10, -14 };
  int16_t g = groundAt(x0 + Z(40));
  for (uint8_t i = 0; i < 7; i++) {
    int16_t x = x0 + Z(i * 16);
    addPlat(x, g + Z(A[i]), Z(18));
    addPlat(x + Z(6), g + Z(B[i]), Z(18));
    if ((i & 1) == 0) addDeco(x + Z(2), 2);
  }
  addObj(x0 + Z(48), g + Z(A[3] - 6), 1);
  addObj(x0 + Z(64), g + Z(B[4] - 6), 1);
  addObj(x0 + Z(30), 0, 8);
}


const __FlashStringHelper* zoneName() {
  switch (zone) {
    case 0: return F("GREEN HILL");
    case 1: return F("CHEM PLANT");
    case 2: return F("CASINO NIGHT");
    default: return F("SCRAP BRAIN");
  }
}

bool addObj(int16_t x, int16_t y, uint8_t kind) {
  for (auto &o : objs) {
    if (o.live) continue;
    o.live = true;
    o.x = x; o.y = y; o.home = x;
    o.kind = kind;
    o.state = 0; o.timer = 0;
    o.vx = (kind == 4 || kind == 2) ? -1 : 0;
    if (kind == 4) o.timer = 30 + (h8((uint16_t)x) & 31);
    // Ground-anchored kinds keep their real height: the draw pass culls by o.y, so spikes left
    // at y 0 vanished on high ground while staying solid (the "invisible wall" stops)
    if (kind == 0 || kind == 5 || kind == 8) o.y = groundAt(x);
    if (kind == 8) { o.state = 0; o.timer = 20 + (h8((uint16_t)(x + 3)) & 31); }
    if (kind == 9) { o.y = y; o.timer = 0; }
    if (kind == 10) o.timer = 0;
    if (kind == 11) {                       // fish: rests in the pool at `home`, leaps on a timer
      o.home = y; o.state = 0; o.vx = 0;
      o.timer = 20 + (h8((uint16_t)(x * 3)) & 31);
    }
    return true;
  }
  return false;
}

void addDeco(int16_t x, uint8_t decoType) {
  for (auto &o : objs) {
    if (o.live) continue;
    o.live = true;
    o.x = x; o.home = x; o.y = groundAt(x);
    o.kind = 5; o.state = decoType; o.timer = 0; o.vx = 0;
    return;
  }
}

void addWall(int16_t x, uint8_t h) {
  int16_t g = groundAt(x);
  for (auto &o : objs) {
    if (o.live) continue;
    o.live = true; o.x = x; o.y = g; o.kind = 7;
    o.state = h; o.timer = 0; o.vx = 0;
    return;
  }
}
bool addPlat(int16_t x, int16_t y, int16_t w) {
  for (auto &p : plats) {
    if (p.live) continue;
    p.live = true; p.x = x; p.y = y; p.w = w;
    return true;
  }
  return false;
}
void addBridge(int16_t x0, int16_t x1, int16_t y) {
  for (auto &b : bridges) {
    if (b.live) continue;
    b.live = true; b.x0 = x0; b.x1 = x1; b.y = y;
    return;
  }
}

// How far bridge i dips at x: deepest under Sonic (up to 6 px mid-span), 0 at the posts
int16_t bridgeSag(uint8_t i, int16_t u) {
  if (curBridge != (int8_t)i) return 0;
  const Bridge &b = bridges[i];
  int16_t sp = px;
  if (sp <= b.x0) sp = b.x0 + 1;
  if (sp >= b.x1) sp = b.x1 - 1;
  int16_t half = (b.x1 - b.x0) / 2;
  int16_t edge = (sp - b.x0 < b.x1 - sp) ? sp - b.x0 : b.x1 - sp;
  int16_t S = (int16_t)((6 * (int32_t)edge) / (half > 0 ? half : 1));
  if (u <= sp) return (int16_t)((int32_t)S * (u - b.x0) / (sp - b.x0));
  return (int16_t)((int32_t)S * (b.x1 - u) / (b.x1 - sp));
}

// Standing height for placing things: a bridge deck if one spans x, else the ground
int16_t surfaceAt(int16_t x) {
  for (auto &b : bridges)
    if (b.live && x >= b.x0 && x <= b.x1) return b.y;
  return groundAt(x);
}

// vx/vy in 1/16 px per frame
void addShot(int16_t x, int16_t y, int16_t vx, int16_t vy) {
  for (auto &s : shots) {
    if (s.live) continue;
    s.live = true; s.x = x; s.y = y; s.vx = vx; s.vy = vy; s.qx = 0; s.qy = 0;
    return;
  }
}

void clearWorld() {
  for (auto &o : objs) o.live = false;
  for (auto &p : plats) p.live = false;
  for (auto &s : shots) s.live = false;
  for (auto &r : scat) r.live = false;
  for (auto &b : bridges) b.live = false;
  curBridge = -1;
  onLedge = false;
  loopTrack.live = false;
  inLoop = false;
  loopAng = 0;
  loopGspd = 0;
  loopSense = 0;
  loopProg = 0;
  inHollow = false;
  loopSide = 0;
  loopPassed = false;
  loopRolled = false;
  loopLock = 0;
}

void spillRings() {
  // At most 3 rings fly out; flung hard so you actually lose them
  static const int8_t XV[3] = { -6, 6, -4 };
  static const int8_t YV[3] = { -9, -9, -11 };
  for (uint8_t i = 0; i < 3; i++) {
    scat[i].live = true;
    scat[i].x = px + (int16_t)(i - 1) * 15;
    scat[i].y = py - 4;
    scat[i].vx = XV[i];
    scat[i].vy = YV[i];
    scat[i].life = SCATTER_LIFE;
  }
  beep(400, 100);
}

void updateScatter() {
  for (auto &r : scat) {
    if (!r.live) continue;
    if (r.life) r.life--;
    else { r.live = false; continue; }

    r.vy++;
    if (r.vy > 7) r.vy = 7;
    r.x += r.vx;
    r.y += r.vy;

    int16_t g = groundAt(r.x) - 4;
    if (r.y >= g && r.vy > 0) {
      r.y = g;
      r.vy = (int8_t)(-(r.vy * 2) / 3);
      if (r.vy > -2) r.vy = -2;
      if (r.vx > 1) r.vx--;
      else if (r.vx < -1) r.vx++;
    }
    for (auto &p : plats) {
      if (!p.live) continue;
      if (r.x < p.x || r.x > p.x + p.w) continue;
      if (r.vy > 0 && r.y >= p.y - 4 && r.y <= p.y + 5) {
        r.y = p.y - 4;
        r.vy = (int8_t)(-(r.vy * 2) / 3);
        if (r.vy > -2) r.vy = -2;
      }
    }

    // Grace period: spilled rings are not grab-able until they've flown out
    if (r.life > SCATTER_LIFE - SCATTER_GRACE) continue;
    if (abs(px - r.x) < 8 && abs(py - r.y) < 8) {
      r.live = false;
      rings++;
      score += 10;
      beep(1800, 22);
    }
  }
}

// Solid ground reads as solid: a 2 px crust over a zone-specific fill, anchored to the world
// so it scrolls with the level. Each zone's pattern doubles as its identity in 1-bit.
static inline bool groundPattern(int32_t wx, int32_t wy) {
  switch (zone) {
    case 0:  return ((wx >> 2) ^ (wy >> 2)) & 1;                                  // Green Hill: 4×4 checkerboard
    case 1:  return ((wy & 3) == 0) || ((wx & 15) == 0 && (wy & 3) == 2);         // Chemical: stripes + rivets
    case 2:  return ((wx + wy) & 7) == 0 || ((wx - wy) & 7) == 0;                 // Casino: diamond lattice
    default: return (((wx + wy) & 7) == 0) && ((wy >> 2) & 1);                    // Scrap Brain: sparse ///
  }
}

void drawTerrain() {
  uint8_t *buf = u8g2.getBufferPtr();   // SH1106 full buffer: 8 pages × 128 bytes, LSB = top row
  for (int16_t i = 0; i < 128; i++) {
    int32_t wx = (int32_t)cam + i;
    int16_t g = sy(groundAt(wx));
    if (g > 63) continue;
    for (int16_t y = (g < 0 ? 0 : g); y < 64; y++) {
      int16_t depth = y - g;
      if (depth < 2 || (depth > 2 && groundPattern(wx, (int32_t)y + camY)))
        buf[(y >> 3) * 128 + i] |= (uint8_t)(1 << (y & 7));
    }
  }
}

void hurt() {
  if (invul || restartPending) return;
  if (rings) {
    // Ring hit: spill rings, keep going
    beep(200, 140);
    rings = 0;
    spillRings();
    invul = 55;
    setPlayerVel(faceRight ? ZV(-2) : ZV(2), -HURT_VY);
    jumpCut = true;            // knock-back isn't a jump: releasing the button can't shorten it
    onGround = false;
    rolling = true;
    groundRoll = false;
    inLoop = false;
    inHollow = false;
    return;
  }
  // No rings = death: lose a life; the death animation runs after gameStep() finishes
  // (hurt() is called mid-iteration over objs), then the act restarts or it's game over
  if (lives) lives--;
  restartPending = true;
}

// Original short "dun da dun dun daaa" (~1.9 s)
static const watch::Note DEATH_TUNE[] = {
  {523, 150}, {0, 30}, {494, 110}, {466, 150}, {0, 30}, {440, 150}, {0, 50}, {349, 900},
};

void startDeath() {
  mode = DEAD;
  deadT = 0;
  inLoop = inHollow = false;
  rolling = groundRoll = false;
  onGround = false;
  curBridge = -1;
  invul = 0;
  setPlayerVel(0, -JUMP_SPD);     // pop straight up…
  watch::playTune(DEATH_TUNE, sizeof(DEATH_TUNE) / sizeof(DEATH_TUNE[0]));
}

void deathStep(uint8_t e) {
  if (e & B_MODE) { watch::stopTune(); watch::exitToClock(); return; }
  deadT++;
  // …then fall through everything and off the bottom of the screen (camera stays put)
  for (uint8_t i = 0; i < 2; i++) {
    fvy += GRAVITY;
    fpy += fvy;
  }
  syncPlayerInts();
  if (deadT >= 60 && sy(py) > 80 && !watch::tunePlaying()) {
    if (!lives) transitionTo([] { mode = OVER; });
    else transitionTo([] { startAct(zone, act); invul = 45; });   // lives/score/emeralds kept
  }
}

void placeGoal(int16_t x) { addObj(x, groundAt(x) - 25, 6); }

// Title card: slides in from the right, holds ~0.8 s, then slides off to the left
const uint8_t INTRO_LEN = 46, INTRO_IN = 6, INTRO_OUT = 10;
void announce() {
  intro = INTRO_LEN;
  beep(600 + zone * 80 + act * 40, 70);
}

void startEggWrecker() {
  bossOn = true;
  bossHP = 8;
  bossCool = 0;
  bossPhase = 0;
  arenaOrigin = px + 20;
  setPlayerPos(arenaOrigin + 30, groundAt(arenaOrigin + 30) - RAD_STAND);
  setPlayerVel(0, 0);
  inLoop = false;
  cam = arenaOrigin;
  camY = 0;
  clearWorld();
  // Flat arena, like the original: the ship flies in from the top right
  eggFX = arenaOrigin + 160;
  eggX = (int16_t)eggFX;
  eggY = -24;
  eggDir = -1;
  bossPhase = EGG_ENTER;
  eggT = 0; eggPause = 0; eggLaugh = 0;
  chainLen = 0; pendPhase = 0; pendAmp = 0; ballAng = 0;
  intro = INTRO_LEN;
  beep(280, 250);
}

void buildAct() {
  // Whole act is built once from its layout table and stays loaded. The table is in design
  // units; positions and sizes are zoomed (Z) here, and spacing is worked out before zooming.
  clearWorld();
  // Bridges first, so rings/springs above them sit on the deck, not in the pool
  for (uint8_t i = 0; i < lvl->nObj; i++) {
    const LvlObj &d = lvl->obj[i];
    if (d.kind == L_BRIDGE) addBridge(Z(d.x), Z(d.x + d.arg * 4), groundAt(Z(d.x)));
  }
  for (uint8_t i = 0; i < lvl->nObj; i++) {
    const LvlObj &d = lvl->obj[i];
    int16_t x = Z(d.x), h = Z(d.h);
    int16_t g = surfaceAt(x);
    switch (d.kind) {
      case L_RING:   addObj(x, g - h, 1); break;
      case L_CRAB:   addObj(x, g - 6, 2); break;
      case L_SPRING: addObj(x, g - h - 5, 3); break;
      case L_BEE:    addObj(x, g - h, 4); break;
      case L_DECO:   addDeco(x, d.arg); break;
      case L_WALL:   addWall(x, Z(d.arg)); break;
      case L_SPIKE:  addObj(x, 0, 0); break;
      case L_FLAME:  addObj(x, 0, 8); break;
      case L_BUMPER: addObj(x, g - h, 9); break;
      case L_SAW:    addObj(x, g - h, 10); break;
      case L_PLAT:   addPlat(x, g - h, Z(d.arg)); break;
      case L_RINGS:
        for (uint8_t k = 0; k < d.arg; k++) {
          int16_t rx = Z(d.x + k * 12);
          addObj(rx, surfaceAt(rx) - h, 1);
        }
        break;
      case L_ARC:
        for (uint8_t k = 0; k < d.arg; k++) {
          int16_t rx = Z(d.x + k * 12);
          // parabola: 8 px at the ends, h in the middle
          int16_t m = d.arg - 1;
          int16_t u = 2 * k - m;                       // -m..m
          int16_t lift = m ? 8 + ((int32_t)(d.h - 8) * (m * m - u * u)) / (m * m) : d.h;
          addObj(rx, surfaceAt(rx) - Z(lift), 1);
        }
        break;
      case L_SPIKES:
        for (uint8_t k = 0; k < d.arg; k++) addObj(Z(d.x + k * 12), 0, 0);
        break;
      case L_LOOP:   placeGreenHillLoop(x); break;
      case L_CHEMX:  placeChemWavyCross(x); break;
      case L_FISH:   addObj(x, groundAt(x) - 4, 11); break;   // in the pool, under the deck
    }
  }
  clearLoopRunway();                 // no hazards right before / after a loop
  if (act == 0) placeGoal(actEnd);   // Act 2 ends in the boss arena instead
}

void startAct(uint8_t z, uint8_t a) {
  zone = z; act = a;
  bossOn = false;
  lvl = &ACTS[z][a];
  actEnd = Z(lvl->len);
  rings = 0;                 // rings are per act (and lost on a restart), as in Sonic
  actFrames = 0;
  camLead = 0;
  goalT = 0;
  cam = 0; camY = 0;
  setPlayerPos(30, groundAt(30) - RAD_STAND);
  setPlayerVel(0, 0);
  onGround = true; rolling = false; groundRoll = false; faceRight = true;
  jumpCut = false; inLoop = false;
  loopAng = 0; loopGspd = 0; loopSense = 0; loopProg = 0;
  inHollow = false; loopSide = 0; loopPassed = false; loopRolled = false; loopLock = 0;
  invul = 0; animFrame = 0;
  lastTapDir = 0; lastTapMs = 0;
  buildAct();
  mode = GAME;
  announce();
}

void startGame() {
  randomSeed(micros());
  rings = 0; score = 0; lives = 3; emeralds = 0;
  startAct(0, 0);
}

const uint8_t SPECIAL_RINGS = 20;

void nextStage() {
  if (act == 0) startAct(zone, 1);
  else if (zone < 3) startAct(zone + 1, 0);
  else { mode = WIN; beep(1000, 500); }
}

void startSpecial();
void advanceAfterClear() {
  // Finish an act holding 20+ rings → Special Stage (flashes white on the way in)
  if (emeralds < 7 && rings >= SPECIAL_RINGS) transitionTo(startSpecial, true);
  else transitionTo(nextStage);
}

void startSpecial() {
  mode = SPECIAL;
  spRings = 0;
  spTimer = 450; // ~15s at 30 FPS
  spGot = 0;
  // Must set the fixed-point state too, or syncPlayerInts() restores the act position
  setPlayerPos(64, 40);
  setPlayerVel(0, 0);
  spEX = 64; spEY = 28;
  clearWorld();
  for (uint8_t i = 0; i < 8; i++)
    addObj(20 + i * 12, 18 + (i & 1) * 14, 1);
  intro = 35;
  beep(1200, 80);
}

void beginClear() {
  bool tuneAlready = goalT != 0;      // Act 1: the fanfare started at the signpost
  mode = CLEAR;
  clearT = 0;
  tallyDoneT = 0;
  goalT = 0;
  uint16_t secs = actSeconds();
  uint16_t par = lvl->parSec;
  // A+ within 5 s of par (the fastest realistic clear), B within 12 s, otherwise D
  lastRank = (secs <= par + 5) ? 0 : (secs <= par + 12) ? 1 : 2;
  static const uint16_t TIME_BONUS[3] = {5000, 2000, 500};
  tallyTimeBonus = TIME_BONUS[lastRank];
  tallyRingBonus = (uint32_t)rings * 100;
  if (!tuneAlready) watch::playTune(ACT_CLEAR_TUNE, sizeof(ACT_CLEAR_TUNE) / sizeof(ACT_CLEAR_TUNE[0]));
}

// ---- physics helpers ----
void resolveLanding(int16_t rad) {
  syncPlayerInts();
  int16_t g = groundAt(px);
  int16_t feet = py + rad;
  // Use actual fixed-point delta so fast falls can't tunnel past the surface
  int16_t prevFeet = F2I(fpy - fvy) + rad;
  bool landed = false;

  // Solid world floor: if falling/standing and feet meet or penetrate ground, snap on top.
  // (Old "must cross within 3px" test let high-speed steps fall straight through.)
  if (fvy >= 0 && feet >= g) {
    fpy = I2F(g - rad);
    fvy = 0;
    landed = true;
  }

  for (auto &p : plats) {
    if (!p.live) continue;
    if (px + (rad - 1) < p.x || px - (rad - 1) > p.x + p.w) continue;
    // One-way platforms still require crossing from above
    if (fvy >= 0 && prevFeet <= p.y + 2 && feet >= p.y) {
      int16_t top = p.y - rad;
      if (!landed || top < F2I(fpy)) {
        fpy = I2F(top);
        fvy = 0;
        landed = true;
      }
    }
  }

  // Log bridges: one-way from above; while standing, follow the sag down
  int8_t landedBridge = -1;
  bool ledge = landed && fpy != I2F(g - rad);
  for (uint8_t i = 0; i < 6; i++) {
    const Bridge &b = bridges[i];
    if (!b.live || px < b.x0 || px > b.x1) continue;
    int16_t top = b.y + bridgeSag(i, px);
    bool standing = (i == curBridge);
    if (fvy >= 0 && prevFeet <= top + 4 && (feet >= top || (standing && feet >= top - 8))) {
      int16_t t2 = top - rad;
      if (!landed || t2 < F2I(fpy) || standing) {
        fpy = I2F(t2);
        fvy = 0;
        landed = true;
        landedBridge = i;
        ledge = true;
      }
    }
  }
  curBridge = landedBridge;

  syncPlayerInts();
  // Safety: never leave Sonic buried under terrain
  g = groundAt(px);
  if (py + rad > g) {
    fpy = I2F(g - rad);
    fvy = 0;
    landed = true;
    ledge = false;
    curBridge = -1;
    syncPlayerInts();
  }
  onLedge = landed && ledge;

  if (landed) {
    onGround = true;
    jumpCut = false;
    if (!groundRoll) rolling = false;
  } else {
    onGround = false;
  }
}

void stopRoll() {
  groundRoll = false;
  if (onGround) rolling = false;
}

// Push Sonic out of a solid AABB and stop his inward speed (side bump)
int16_t stepPrevX;   // Sonic's x before the current physics step (which side he came from)

void bumpFromSolid(int16_t left, int16_t right, int16_t top, int16_t bot, int16_t rad) {
  if (py + rad <= top || py - rad >= bot) return;
  if (px + rad <= left || px - rad >= right) return;
  // Push back out the side he came in from — at speed he can be past the middle already
  int16_t mid = (left + right) / 2;
  if (stepPrevX + rad <= left) mid = 32767;
  else if (stepPrevX - rad >= right) mid = -32768;
  // Genesis: touching a solid's side just stops you (no knock-back), so you can jump
  // straight up beside a wall or spikes and steer over the top.
  STOP_HOOK("solid", left, right);
  if (px < mid) {
    fpx = I2F(left - rad);
    if (fvx > 0) fvx = 0;
  } else {
    fpx = I2F(right + rad);
    if (fvx < 0) fvx = 0;
  }
  if (groundRoll) stopRoll();
  syncPlayerInts();
}

const int16_t SPIKE_H = 18, SPIKE_W = 12;

// Walls and spike sides are solid every 60 Hz step, so even top speed can't sink into them
void applyWalls(int16_t rad) {
  syncPlayerInts();
  for (auto &o : objs) {
    if (!o.live || abs(o.x - px) > 40) continue;
    if (o.kind == 7) {
      int16_t h = o.state ? o.state : 18;
      bumpFromSolid(o.x, o.x + 8, o.y - h, o.y, rad);
    } else if (o.kind == 0) {
      int16_t gy = groundAt(o.x);
      bumpFromSolid(o.x, o.x + SPIKE_W, gy - SPIKE_H + 2, gy, rad);
    }
  }
  syncPlayerInts();
}

// True if spin-rolling or stomping down onto a target
bool canBreakEnemy(int16_t ey) {
  if (rolling || groundRoll) return true;
  return (fvy > 0 && py < ey);
}

void handleDoubleTapRoll(uint8_t e) {
  unsigned long now = millis();
  bool startedRoll = false;

  if (e & B_RIGHT) {
    if (!groundRoll && lastTapDir == 2 && now - lastTapMs < 280 && onGround &&
        (fvx > 0 || (heldIn() & B_RIGHT))) {
      groundRoll = true;
      rolling = true;
      startedRoll = true;
      if (fvx < ZV(3)) fvx = ZV(3);
      syncPlayerInts();
      beep(700, 25);
    }
    lastTapDir = 2;
    lastTapMs = now;
  }
  if (e & B_LEFT) {
    if (!groundRoll && lastTapDir == 1 && now - lastTapMs < 280 && onGround &&
        (fvx < 0 || (heldIn() & B_LEFT))) {
      groundRoll = true;
      rolling = true;
      startedRoll = true;
      if (fvx > ZV(-3)) fvx = ZV(-3);
      syncPlayerInts();
      beep(700, 25);
    }
    lastTapDir = 1;
    lastTapMs = now;
  }

  // Stay in spin until a later Left/Right tap (not the one that started it) or a wall
  if (groundRoll && !startedRoll && (e & (B_LEFT | B_RIGHT))) stopRoll();

  // Keep ball form while spin-rolling (through jumps / ledges too)
  if (groundRoll) rolling = true;
}

// ---- Egg Wrecker ----
static const int16_t EGG_HOVER = 10;          // ship centre height while fighting
static const int16_t EGG_LEFT = 26, EGG_RIGHT = 102;   // patrol ends (from arenaOrigin)
static const float CHAIN_LEN = 24.0f;          // pivot → ball centre
static const int16_t BALL_R = 6;

int16_t eggPivotY() { return eggY + 11; }

void eggPlaceBall() {
  ballAng = pendAmp * sinf(pendPhase);
  ballWX = eggX + (int16_t)lroundf(chainLen * sinf(ballAng));
  ballWY = eggPivotY() + (int16_t)lroundf(chainLen * cosf(ballAng));
}

void eggStep(int16_t rad) {
  eggT++;
  if (bossCool) bossCool--;
  if (eggLaugh) eggLaugh--;
  float swingRate = 2.0f * (float)PI / (84 - zone * 6);   // ~2.8 s per full swing, quicker later
  float shipSpd = 0.6f + zone * 0.15f;

  switch (bossPhase) {
    case EGG_ENTER: {                             // glide in diagonally from off-screen
      float tx = arenaOrigin + EGG_RIGHT;
      eggFX += (tx - eggFX) * 0.06f - 0.4f;
      if (eggY < EGG_HOVER) eggY++;
      if (eggFX <= tx + 1 && eggY >= EGG_HOVER) { eggFX = tx; bossPhase = EGG_LOWER; eggT = 0; beep(300, 60); }
      break;
    }
    case EGG_LOWER:                               // let the ball down, then start it swinging
      eggY = EGG_HOVER;
      if (chainLen < CHAIN_LEN) chainLen += 0.6f;
      else if (eggT > 50) { bossPhase = EGG_FIGHT; eggPause = 20; }
      break;
    case EGG_FIGHT:
      eggY = EGG_HOVER + ((eggT >> 4) & 1);       // gentle bob
      if (pendAmp < 1.15f) pendAmp += 0.02f;      // swing builds to ~65°
      pendPhase += swingRate;
      if (pendPhase > 2 * (float)PI) pendPhase -= 2 * (float)PI;
      if (eggPause) eggPause--;
      else {
        eggFX += eggDir * shipSpd;
        if (eggFX <= arenaOrigin + EGG_LEFT)  { eggFX = arenaOrigin + EGG_LEFT;  eggDir = 1;  eggPause = 40; }
        if (eggFX >= arenaOrigin + EGG_RIGHT) { eggFX = arenaOrigin + EGG_RIGHT; eggDir = -1; eggPause = 40; }
      }
      break;
    case EGG_BLOWUP:                              // shudders and smokes, the ball drops away
      if ((eggT & 3) == 0) beep(150 + (h8(eggT) & 127), 30);
      if (ballWY < 70) chainLen += 1.5f;
      pendAmp *= 0.9f;
      if (eggT > 60) { bossPhase = EGG_FLEE; eggT = 0; beep(700, 80); }
      break;
    case EGG_FLEE:                                // limps off to the right, climbing
      eggFX += 2.0f;
      if (eggT & 1) eggY--;
      if (eggX > arenaOrigin + 150) { transitionTo([] { bossOn = false; beginClear(); }); return; }
      break;
  }
  eggX = (int16_t)eggFX;
  eggPlaceBall();
  if (bossPhase >= EGG_BLOWUP) return;            // can't hurt or be hurt any more

  // Touch the ship: always bounce off; spinning into it scores a hit
  if (abs(px - eggX) < 14 && abs(py - eggY) < 13) {
    bool attack = rolling && bossCool == 0;
    setPlayerVel((px < eggX) ? ZV(-2) : ZV(2),
                 (py <= eggY + 2) ? -BOSS_BOUNCE : BOSS_BOUNCE / 2);
    jumpCut = true;
    onGround = false;
    rolling = true;
    faceRight = fvx > 0;
    if (attack) {
      bossHP--;
      bossCool = 32;                              // flashes; can't be hit again yet
      score += 100;
      beep(1500, 100);
      if (!bossHP) { bossPhase = EGG_BLOWUP; eggT = 0; score += 1000; }
    } else if (bossCool == 0 && !invul) {
      hurt();
      eggLaugh = 40;
    }
  }

  // Wrecking ball
  if (!invul && chainLen > 4) {
    int16_t dx = px - ballWX, dy = py - ballWY;
    int16_t reach = BALL_R + rad - 1;
    if ((int32_t)dx * dx + (int32_t)dy * dy < (int32_t)reach * reach) { hurt(); eggLaugh = 40; }
  }
}

void applyGroundControl() {
  bool pinball = (zone == 2 && (rolling || abs(fvx) >= ZV(2)));
  // Rolling keeps whatever speed you had (Genesis caps rolls at 16, i.e. never in practice)
  fx top = pinball ? ZV(5) : TOP_SPD;
  // On a platform or bridge the terrain below is irrelevant — it's flat underfoot
  int16_t slope = (onGround && !onLedge) ? slopeAt(px) : 0;

  if (groundRoll && onGround) {
    // Holding forward adds no roll speed; slope does (2.5×)
    bool downhill = (fvx == 0) || ((slope > 0) == (fvx > 0));
    fvx += (fx)slope * (downhill ? SLOPE_ROLL : SLOPE_ROLL_UP);
    if (fvx > 0) { fvx -= ROLL_FRIC; if (fvx < 0) fvx = 0; }
    else if (fvx < 0) { fvx += ROLL_FRIC; if (fvx > 0) fvx = 0; }
    // Genesis: a roll that slows below 0.5 px/frame stands back up, so holding a direction
    // runs again (otherwise an accidental double-tap leaves him parked in a ball)
    if (abs(fvx) < UNROLL_SPD) stopRoll();
  } else if (onGround) {
    fvx += (fx)slope * SLOPE_RUN;
    bool left = heldIn() & B_LEFT, right = heldIn() & B_RIGHT;
    if (loopLock) { left = right = false; }
    if (right && !left) {
      if (fvx < 0) fvx += BRAKE;          // skid
      else {
        if (fvx == 0) fvx = START_KICK;  // leave standstill immediately
        fvx += ACCEL;
      }
      faceRight = true;
    } else if (left && !right) {
      if (fvx > 0) fvx -= BRAKE;
      else {
        if (fvx == 0) fvx = -START_KICK;
        fvx -= ACCEL;
      }
      faceRight = false;
    } else {
      // coast — same rate as accel
      if (fvx > 0) { fvx -= FRICTION; if (fvx < 0) fvx = 0; }
      else if (fvx < 0) { fvx += FRICTION; if (fvx > 0) fvx = 0; }
    }
  } else if (pinball) {
    if (heldIn() & B_LEFT)  { fvx -= ACCEL; faceRight = false; }
    if (heldIn() & B_RIGHT) { fvx += ACCEL; faceRight = true; }
  } else {
    // Air steer — classic Sonic still answers the D-pad off the ground
    if (heldIn() & B_LEFT)  { fvx -= AIR_ACCEL; faceRight = false; }
    if (heldIn() & B_RIGHT) { fvx += AIR_ACCEL; faceRight = true; }
  }

  if (fvx > top) fvx = top;
  if (fvx < -top) fvx = -top;
}

// ---- Green Hill loop: physics, not a speed gate ----
// The road runs flat under the loop. Crossing the loop's centre line on the floor (in the
// direction you came in from) puts Sonic on the circular track at the bottom — at ANY speed.
// From there it's physics only:
//   * the slope factor along the track slows the climb and speeds the descent;
//   * like Sonic 1: more than 45° from flat and slower than LOOP_FALL → he lets go and
//     falls (L/R locked 30 frames); slower climbs just slide back down and roll out;
//   * the through-exit only counts after a full 2π in the entry direction.
// Angles are y-down: 0 = right, π/2 = bottom, π = left, 3π/2 = top.

static float loopTrackR(int16_t rad) {
  float R = (float)loopTrack.r - (float)rad - 4.0f;   // feet against the inner face
  return (R < 15.0f) ? 15.0f : R;
}

// Unit tangent for +angle (θ increasing): (-sin, cos) in y-down space
static void loopTangent(float th, float *tx, float *ty) {
  *tx = -sinf(th);
  *ty =  cosf(th);
}

static int16_t loopFloorY() { return loopTrack.cy + loopTrack.r; }

// Surface angle away from flat: 0 at the bottom, ±π/2 on the walls, ±π on the ceiling
static float loopRelBottom() {
  float rel = loopAng - (float)HALF_PI;
  while (rel > (float)PI) rel -= (float)TWO_PI;
  while (rel <= -(float)PI) rel += (float)TWO_PI;
  return rel;
}

static void loopPlaceOnTrack(int16_t rad) {
  float R = loopTrackR(rad);
  fpx = I2F(loopTrack.cx + (int16_t)lroundf(R * cosf(loopAng)));
  fpy = I2F(loopTrack.cy + (int16_t)lroundf(R * sinf(loopAng)));
  syncPlayerInts();
}

static void loopAttach(float th, fx gspd, float sense, float prog, int16_t rad) {
  inLoop = true;
  inHollow = false;
  loopAng = th;
  loopGspd = gspd;
  loopSense = sense;
  loopProg = prog;
  onGround = false;
  rolling = true;
  groundRoll = true;
  loopPlaceOnTrack(rad);
}

// Back onto the flat floor at the bottom of the loop, keeping horizontal speed
static void loopToGround(int16_t rad) {
  float tx, ty;
  loopTangent(loopAng, &tx, &ty);
  fx vx = (fx)((float)loopGspd * tx);
  float R = loopTrackR(rad);
  setPlayerPos(loopTrack.cx + (int16_t)lroundf(R * cosf(loopAng)), loopFloorY() - rad);
  setPlayerVel(vx, 0);
  inLoop = false;
  inHollow = false;
  onGround = true;
  if (vx > 0) faceRight = true;
  else if (vx < 0) faceRight = false;
}

// Off the track into the hollow, carrying the velocity he had at the moment he let go
static void loopFallOff(int16_t rad) {
  float tx, ty;
  loopTangent(loopAng, &tx, &ty);
  fvx = (fx)((float)loopGspd * tx);
  fvy = (fx)((float)loopGspd * ty);
  float R = loopTrackR(rad) - 1.0f;   // just inside, so he doesn't instantly re-stick
  fpx = I2F(loopTrack.cx + (int16_t)(R * cosf(loopAng)));
  fpy = I2F(loopTrack.cy + (int16_t)(R * sinf(loopAng)));
  syncPlayerInts();
  inLoop = false;
  inHollow = true;
  onGround = false;
  rolling = true;
  groundRoll = false;
}

// Fell off the track. A loop is a road that climbs, flips and comes down NEXT TO where it
// started (in 3D), so falling off drops Sonic straight down to the road underneath — the
// side walls don't catch him like a ring would. Landing:
//   * on the entry half (behind the centre line) → he's on the entry road: roll back out
//     or turn round and try again;
//   * on the climbing half → he lands low on the climb and slides back down and out.
void stepHollow(int16_t rad) {
  if (heldIn() & B_LEFT)  { fvx -= AIR_ACCEL; faceRight = false; }
  if (heldIn() & B_RIGHT) { fvx += AIR_ACCEL; faceRight = true; }
  fvy += GRAVITY;
  if (fvy > MAX_FALL) fvy = MAX_FALL;
  fpx += fvx;
  fpy += fvy;

  float R = loopTrackR(rad);
  float fwdDir = -loopSense;                         // +1: forward is +x (entered from the left)
  // Can't fall through the climbing wall to the far side
  float fwd = ((float)fpx / FX_ONE - loopTrack.cx) * fwdDir;
  if (fwd > R) {
    fpx = I2F(loopTrack.cx + (int16_t)(R * fwdDir));
    if ((float)fvx * fwdDir > 0) { STOP_HOOK("hollow", loopTrack.cx, 0); fvx = 0; }
  }
  syncPlayerInts();

  int16_t floorY = loopFloorY();
  if (py + rad < floorY) return;

  fwd = (float)(px - loopTrack.cx) * fwdDir;
  if (fwd > 3.0f) {
    // On the climb: put him on the track low down (≤ 40°, below the slip zone)
    float rel = asinf(fwd / R > 1.0f ? 1.0f : fwd / R);
    if (rel > 0.70f) rel = 0.70f;
    float th = (float)HALF_PI + loopSense * rel;
    float tx, ty;
    loopTangent(th, &tx, &ty);
    fx gspd = (fx)((float)fvx * tx + (float)fvy * ty);
    loopAttach(th, gspd, loopSense, rel, rad);
  } else {
    // Behind the centre line: that's just the entry road
    fpy = I2F(floorY - rad);
    fvy = 0;
    onGround = true;
    inHollow = false;
    syncPlayerInts();
  }
}

void stepLoop(int16_t rad) {
  float R = loopTrackR(rad);
  float tx, ty;
  loopTangent(loopAng, &tx, &ty);

  // Sonic_SlopeResist: slope factor · sin(surface angle) along the track (pulls +Y).
  // Sonic 1 skips it within ±45° of upside-down, so he coasts across the ceiling.
  // Rolling: 2.5× downhill, 0.625× uphill (S1's #$50, and #$50>>2 when climbing).
  if (fabsf(loopRelBottom()) < 2.35619f /* 135° */) {
    float slope = (float)LOOP_SLP * ty;           // + pushes toward +θ
    if (loopRolled) {
      bool downhill = (slope > 0) == (loopGspd > 0);
      slope *= downhill ? 2.5f : 0.625f;
    }
    loopGspd += (fx)slope;
  }

  // Forward = the direction you ENTERED (holding right on a left→right run keeps loop
  // momentum even while moving left across the ceiling). Opposite only brakes through zero.
  // Push/brake keep Genesis's ratios to the slope factor (accel 3/8, decel 4×).
  // Rolling: holding forward adds nothing (as on flat ground). Locked after a fall-off.
  if (!loopLock) {
    fx push  = (fx)(loopSense * (float)LOOP_PUSH);
    fx brake = (fx)(loopSense * (float)LOOP_BRAKE);
    bool fwd = (loopSense < 0 && (heldIn() & B_RIGHT)) || (loopSense > 0 && (heldIn() & B_LEFT));
    bool back = (loopSense < 0 && (heldIn() & B_LEFT)) || (loopSense > 0 && (heldIn() & B_RIGHT));
    if (fwd && !loopRolled) loopGspd += push;
    if (back) loopGspd -= brake;
  }
  if (loopGspd > TOP_SPD) loopGspd = TOP_SPD;
  if (loopGspd < -TOP_SPD) loopGspd = -TOP_SPD;

  float ds = (float)loopGspd / (float)FX_ONE;    // px per step along the track
  float dAng = ds / R;
  float prevProg = loopProg;
  loopAng += dAng;
  loopProg += dAng * loopSense;                  // + = forward in the entry direction
  while (loopAng < 0) loopAng += (float)TWO_PI;
  while (loopAng >= (float)TWO_PI) loopAng -= (float)TWO_PI;

  loopPlaceOnTrack(rad);
  loopTangent(loopAng, &tx, &ty);
  fvx = (fx)(ds * tx * (float)FX_ONE);           // for sprite facing / anim only
  fvy = 0;

  // Crossing the bottom of the track (progress passes a multiple of 2π)
  if (floorf(loopProg / (float)TWO_PI) != floorf(prevProg / (float)TWO_PI)) {
    if (loopProg < prevProg) {                   // sliding back → roll out the way he came
      loopToGround(rad);
      return;
    }
    if (loopProg >= (float)TWO_PI) {             // full lap → out the far side
      loopToGround(rad);
      loopPassed = true;
      beep(1300, 40);
      return;
    }
  }

  // Sonic_SlopeRepel: on a wall or ceiling (> 45° from flat) and slower than the fall
  // speed → ground speed cleared, airborne with current velocity, L/R locked 30 frames
  fx absG = loopGspd >= 0 ? loopGspd : -loopGspd;
  if (fabsf(loopRelBottom()) > 0.785398f /* 45° */ && absG < LOOP_FALL) {
    loopFallOff(rad);
    loopLock = LOOP_LOCK;
  }
}

// Loop entry + solid outer shell. Runs after normal movement/landing each step.
void loopGate(int16_t prevX, int16_t rad) {
  if (!loopTrack.live || inLoop || inHollow || bossOn) return;
  int16_t r = loopTrack.r, cx = loopTrack.cx, cy = loopTrack.cy;

  // Which side did he come in from? (remembered while under the loop)
  if (abs(px - cx) > r + 2) { loopSide = 0; loopPassed = false; }
  else if (loopSide == 0) loopSide = (prevX < cx) ? -1 : 1;

  float R = loopTrackR(rad);
  float dx = (float)fpx / FX_ONE - cx, dy = (float)fpy / FX_ONE - cy;
  float d2 = dx * dx + dy * dy;

  // Jumped (or got bounced) up into the hollow: the track is solid from the inside
  if (!onGround && d2 < R * R) { inHollow = true; return; }

  // On the floor under the loop, crossing the centre line → onto the track, any speed
  bool nearFloor = py + rad >= loopFloorY() - 8;
  if (nearFloor && !loopPassed) {
    if (loopSide < 0 && fvx > 0 && px >= cx) {          // from the left: up the right wall
      loopRolled = groundRoll;
      loopAttach((float)HALF_PI, -fvx, -1.0f, 0.0f, rad);
      beep(950, 25);
      return;
    }
    if (loopSide > 0 && fvx < 0 && px <= cx) {          // from the right: up the left wall
      loopRolled = groundRoll;
      loopAttach((float)HALF_PI, -fvx, 1.0f, 0.0f, rad);
      beep(950, 25);
      return;
    }
  }

  // Outer shell: can't jump into the ring from outside (bottom band is the road itself)
  int16_t idx = px - cx, idy = py - cy;
  int32_t id2 = (int32_t)idx * idx + (int32_t)idy * idy;
  int32_t rIn = (int32_t)(r - 9) * (r - 9);
  int32_t rOut = (int32_t)(r + 8) * (r + 8);
  if (idy < r - 18 && id2 >= rIn && id2 <= rOut) {
    float len = sqrtf((float)id2);
    if (len >= 1.0f) {
      float scale = (float)(r + 9) / len;
      fpx = I2F(cx + (int16_t)(idx * scale));
      fpy = I2F(cy + (int16_t)(idy * scale));
      STOP_HOOK("loopshell", cx, idy);
      fvx = (fx)(-fvx / 4);
      if (fvy < 0) fvy = 0;
      syncPlayerInts();
    }
  }
}

void physicsStep(uint8_t e) {
  int16_t rad = rolling ? RAD_ROLL : RAD_STAND;

  if ((e & B_MID) && onGround && !inLoop) {
    fvy = -JUMP_SPD;
    onGround = false;
    rolling = true;          // jump keeps horizontal momentum
    jumpCut = false;
    beep(900, 30);
  }
  // Variable jump: release early cuts the rise
  if (!onGround && !jumpCut && !(heldIn() & B_MID) && fvy < 0) {
    fvy >>= 1;
    jumpCut = true;
  }
  if (!onGround || groundRoll) rolling = true;

  if (loopLock && (onGround || inLoop)) loopLock--;
  if (inLoop) { stepLoop(rad); return; }
  if (inHollow) { stepHollow(rad); return; }

  int16_t prevX = px;
  stepPrevX = px;
  bool wasGrounded = onGround && !onLedge && fvy >= 0;
  applyGroundControl();

  if (!onGround) {
    fvy += GRAVITY;
    if (fvy > MAX_FALL) fvy = MAX_FALL;
  }

  fpx += fvx;
  fpy += fvy;
  syncPlayerInts();

  int16_t minX = bossOn ? arenaOrigin : 8;
  if (px < minX) {
    fpx = I2F(minX); fvx = 0;
    if (groundRoll) stopRoll();
    syncPlayerInts();
  }
  if (bossOn && px > arenaOrigin + 122) {       // arena's right wall (screen edge)
    fpx = I2F(arenaOrigin + 122); fvx = 0;
    if (groundRoll) stopRoll();
    syncPlayerInts();
  }
  // Terrain cliffs: a rise steeper than any slope is a wall face — stop at it (no snap-up).
  // Downhill drops are just ledges to fall off.
  if (px != prevX) {
    int8_t dir = px > prevX ? 1 : -1;
    int16_t step = wasGrounded ? CLIFF_STEP : CLIFF_STEP_AIR;
    for (uint8_t k = 0; k < 8; k++) {
      int16_t lead = px + dir * (rad - 1);
      if (py + rad - groundAt(lead) <= step) break;
      if (k == 0) STOP_HOOK("cliff", lead, py + rad - groundAt(lead));
      fpx -= dir * FX_ONE;
      fvx = 0;
      if (groundRoll) stopRoll();
      syncPlayerInts();
    }
  }

  // Genesis ground stick: running over a crest or down a slope keeps him on the ground
  // instead of skipping through the air (he only leaves at a drop bigger than this)
  if (wasGrounded) {
    int16_t gap = groundAt(px) - (py + rad);
    int16_t stick = (int16_t)(abs(fvx) >> 8) + 5;
    if (stick > 12) stick = 12;
    if (gap > 0 && gap <= stick) { fpy = I2F(groundAt(px) - rad); fvy = 0; syncPlayerInts(); }
  }

  applyWalls(rad);
  resolveLanding(rad);
  // Final floor clamp after wall pushes
  {
    int16_t g = groundAt(px);
    if (py + rad > g) {
      fpy = I2F(g - rad);
      fvy = 0;
      onGround = true;
      syncPlayerInts();
    }
  }
  loopGate(prevX, rad);
}

// True if (x,y) is within (rx,ry) of the player at either physics sub-step.
// At top speed Sonic moves 12 px per display frame — more than a ring's pickup window.
bool nearPlayerSwept(int16_t x, int16_t y, int16_t rx, int16_t ry) {
  if (abs(px - x) < rx && abs(py - y) < ry) return true;
  return abs(midPX - x) < rx && abs(midPY - y) < ry;
}

void gameStepInner(uint8_t e);

void gameStep(uint8_t e) {
  gameStepInner(e);
  if (restartPending) {
    restartPending = false;
    startDeath();
  }
}

void gameStepInner(uint8_t e) {
  if (e & B_GAME) { menuSel = 0; mode = MENU; return; }
  if (e & B_MODE) { watch::exitToClock(); return; }

  if (goalT) e = 0;                              // past the signpost: no control
  else if (actFrames < 0xFFFF) actFrames++;      // act timer (stops at the signpost)

  if (invul) invul--;
  if (intro) intro--;
  animFrame++;

  handleDoubleTapRoll(e);

  // Two 60Hz physics steps per ~30 FPS display frame
  physicsStep(e);
  midPX = px; midPY = py;
  physicsStep(0);

  syncPlayerInts();
  int16_t rad = rolling ? RAD_ROLL : RAD_STAND;

  if (bossOn) {
    cam = arenaOrigin;
    // Floor stays at the bottom; the view rises to keep Sonic in sight above the Egg Mobile
    int16_t want = py - 16;
    if (want > 0) want = 0;
    if (camY > want) camY -= (camY - want + 1) / 2;
    else if (camY < want) camY += (want - camY + 3) / 4;
  } else {
    // Look-ahead: the faster you run, the further the camera leads — at top speed Sonic sits
    // 24 px from the trailing edge with 104 px of sightline. Eased 1 px/frame so it never snaps;
    // held steady through the loop (where fvx swings around).
    if (!inLoop && !inHollow && !goalT) {
      int16_t want = (int16_t)((fvx * 40) / TOP_SPD);
      if (want > 40) want = 40;
      if (want < -40) want = -40;
      if (camLead < want) camLead++;
      else if (camLead > want) camLead--;
    }
    if (goalT) {
      // Act over: the camera settles with the signpost in the middle and lets Sonic run off
      int16_t want = actEnd - 64;
      cam += (want - cam) / 4 + ((want > cam) ? 1 : (want < cam) ? -1 : 0);
    } else {
      cam = px - 64 + camLead;
      if (cam < 0) cam = 0;
      // Vertical follow — keeps Sonic framed through tall loops
      int16_t psy = py - camY;
      if (psy < 20) camY = py - 20;
      if (psy > 44) camY = py - 44;
    }
  }

  if (!bossOn && !goalT && px >= actEnd - 8) {
    if (act == 0) {
      goalT = 1;                 // passed the signpost: timer stops, sign spins, fanfare plays
      groundRoll = false;        // stand up and run off screen
      watch::playTune(ACT_CLEAR_TUNE, sizeof(ACT_CLEAR_TUNE) / sizeof(ACT_CLEAR_TUNE[0]));
    } else {
      transitionTo(startEggWrecker);
      return;
    }
  }
  if (goalT) {
    if (goalT < 255) goalT++;
    // Tally once Sonic is off screen and the fanfare is over (or after ~8 s regardless)
    bool gone = sx(px) > 140;
    if ((goalT >= GOAL_MIN_FRAMES && gone && !watch::tunePlaying()) || goalT == 255)
      transitionTo(beginClear);
  }

  // Update only nearby objects (level stays fully loaded)
  for (auto &o : objs) {
    if (!o.live) continue;
    if (!onScreen(o.x, 40)) continue;

    if (o.kind == 0 || o.kind == 5 || o.kind == 8) o.y = groundAt(o.x);

    if (o.kind == 2) { // crab leashed to home
      o.x += o.vx;
      if (o.x < o.home - 22 || o.x > o.home + 22) o.vx = -o.vx;
      o.y = groundAt(o.x) - 6;
    }
    if (o.kind == 4) { // bee patrol near home
      if (o.state == 0) {
        o.x += o.vx;
        if (o.x < o.home - 37 || o.x > o.home + 37) o.vx = -o.vx;
        if (o.timer) o.timer--; else { o.state = 1; o.timer = 26; }
      } else if (o.state == 1) {
        if (o.timer) o.timer--;
        else {
          // Buzz-bomber shot: a slow ball aimed at Sonic (~1.6 px/frame, 1-2 s to cross the
          // screen). Only fires while on screen so it never comes out of nowhere.
          int16_t bsy = sy(o.y);
          if (onScreen(o.x, -6) && bsy >= 8 && bsy <= 56 && abs(px - o.x) < 120) {
            int16_t dx = px - o.x, dy = py - (o.y + 4);
            if (dy < 8) dy = 8;                         // always at least a little downward
            float len = sqrtf((float)dx * dx + (float)dy * dy);
            const float SPD = 26.0f;                    // 1/16 px per frame
            addShot(o.x + (dx < 0 ? -4 : 4), o.y + 4, (int16_t)(dx * SPD / len), (int16_t)(dy * SPD / len));
            beep(480, 16);
          }
          o.state = 2; o.timer = 14;
        }
      } else {
        if (o.timer) o.timer--; else { o.state = 0; o.timer = 40; }
      }
    }
    if (o.kind == 8) {
      if (o.timer) o.timer--;
      else {
        o.state ^= 1;
        o.timer = o.state ? (18 + zone * 2) : (30 + (h8((uint16_t)o.x) & 15));
      }
    }
    if (o.kind == 9 && o.timer) o.timer--;
    if (o.kind == 10) o.timer++;
    if (o.kind == 11) {                        // fish: wait in the pool, leap, splash back down
      if (o.state == 0) {
        if (o.timer) o.timer--;
        else { o.state = 1; o.vx = -11; }      // vx doubles as vertical speed
      } else {
        o.y += o.vx;
        o.vx++;
        if (o.vx > 0 && o.y >= o.home) {
          o.y = o.home; o.state = 0;
          o.timer = 30 + (h8((uint16_t)(o.x + animFrame)) & 31);
        }
      }
    }
  }

  for (auto &s : shots) {
    if (!s.live || s.vy == 99) continue; // skip ball marker
    s.qx += s.vx; s.qy += s.vy;
    s.x += s.qx / 16; s.qx %= 16;
    s.y += s.qy / 16; s.qy %= 16;
    if (s.x < cam - 6 || s.x > cam + 134 || s.y < camY - 6 || s.y > camY + 70) { s.live = false; continue; }
    if (s.y > groundAt(s.x)) { s.live = false; continue; }     // pops on the ground
    if (!invul && abs(s.x - px) < 6 && abs(s.y - py) < 8) { s.live = false; hurt(); }
  }

  updateScatter();

  if (bossOn) { eggStep(rad); return; }

  // collisions — nearby only
  for (auto &o : objs) {
    if (restartPending || mode != GAME) break;  // died this frame — stop colliding
    if (!o.live) continue;
    if (!onScreen(o.x, 20)) continue;
    if (o.kind == 1) {
      if (nearPlayerSwept(o.x, o.y, 8, 9)) {
        o.live = false; rings++; score += 10; beep(1800, 22);
      }
    } else if (o.kind == 0) {
      // Spikes: solid from the sides; only the tips hurt when landing on them
      // (the sides are solid in applyWalls, every physics step)
      int16_t gy = groundAt(o.x);
      int16_t tipY = gy - SPIKE_H;
      int16_t feet = py + rad;
      bool hitX = (px + rad - 1 > o.x) && (px - rad + 1 < o.x + SPIKE_W);
      bool onTips = hitX && (feet >= tipY - 1) && (feet <= tipY + 4) && (py <= tipY + rad);
      if (onTips && fvy >= 0) hurt();
    } else if (o.kind == 2) {
      // Crab: spin/stomp destroys; otherwise solid bump
      if (abs(px - o.x) < 10 && abs(py - o.y) < 9) {
        if (canBreakEnemy(o.y)) {
          o.live = false; score += 50; beep(1400, 50);
          if (fvy > 0) { fvy = stompVY(fvy); rolling = true; syncPlayerInts(); }
        } else if (!invul) {
          hurt();                       // ran into it upright: lose rings, or die with none
        }
      }
    } else if (o.kind == 3) {
      // Check both physics sub-steps: at top speed he moves 12 px a frame, wider than the spring
      bool overX = abs(px - o.x) < 9 || abs(midPX - o.x) < 9 || (midPX < o.x) != (px < o.x);
      if (overX && py + rad >= o.y - 2 && py + rad <= o.y + 9 && fvy >= 0) {
        if (abs(px - o.x) >= 9) { fpx = I2F(o.x); syncPlayerInts(); }
        fvy = -SPRING_SPD; onGround = false; rolling = true;
        jumpCut = true;            // springs ignore the jump button (only a real jump can be cut short)
        syncPlayerInts(); beep(1100, 65);
      }
    } else if (o.kind == 4) {
      // Bee: spin/stomp destroys; otherwise solid bump
      if (abs(px - o.x) < 9 && abs(py - o.y) < 8) {
        if (canBreakEnemy(o.y)) {
          o.live = false; score += 75; beep(1400, 50);
          if (fvy > 0) { fvy = stompVY(fvy); syncPlayerInts(); }
        } else {
          bumpFromSolid(o.x - 5, o.x + 6, o.y - 5, o.y + 5, rad);
        }
      }
    } else if (o.kind == 8) {
      // flame / chem jet — only hurts while firing
      if (o.state && abs(px - o.x) < 9 && py + rad > o.y - 18 && py < o.y + 2) hurt();
    } else if (o.kind == 9) {
      // pinball bumper (cooldown avoids sticky re-hits / lag spikes)
      if (o.timer == 0 && abs(px - o.x) < 11 && abs(py - o.y) < 11) {
        int8_t dx = (px >= o.x) ? 1 : -1;
        int8_t dy = (py >= o.y) ? 1 : -1;
        setPlayerVel(ZV(dx * 4), (dy > 0) ? BUMPER_VY / 3 : -BUMPER_VY);
        faceRight = dx > 0;
        onGround = false;
        rolling = true;
        groundRoll = true;
        o.timer = 12;
        score += 5;
        beep(1200, 30);
      }
    } else if (o.kind == 10) {
      // Saw: spin/stomp destroys; otherwise solid bump (same as enemies)
      if (abs(px - o.x) < 10 && abs(py - o.y) < 10) {
        if (canBreakEnemy(o.y)) {
          o.live = false; score += 80; beep(1400, 50);
          if (fvy > 0) { fvy = stompVY(fvy); syncPlayerInts(); }
        } else {
          bumpFromSolid(o.x - 6, o.x + 6, o.y - 6, o.y + 6, rad);
        }
      }
    } else if (o.kind == 11) {
      // Fish: only dangerous while it's out of the water
      if (o.y < o.home - 5 && abs(px - o.x) < 8 && abs(py - o.y) < 10) {
        if (canBreakEnemy(o.y)) {
          o.live = false; score += 60; beep(1400, 50);
          if (fvy > 0) { fvy = stompVY(fvy); syncPlayerInts(); }
        } else if (!invul) {
          hurt();
        }
      }
    }
    // kind 5 deco: no collision
  }
}

void specialStep(uint8_t e) {
  if (e & B_MODE) { watch::exitToClock(); return; }
  if (intro) intro--;
  animFrame++;
  if (spTimer) spTimer--;
  else {
    // failed — continue game without emerald
    beep(200, 200);
    transitionTo(nextStage, true);
    return;
  }

  if (heldIn() & B_LEFT) fpx -= I2F(2);
  if (heldIn() & B_RIGHT) fpx += I2F(2);
  if (e & B_MID) fvy = I2F(-5);
  if (fvy < I2F(4)) fvy += GRAVITY * 2;
  fpy += fvy;
  syncPlayerInts();
  if (px < 10) setPlayerPos(10, py);
  if (px > 118) setPlayerPos(118, py);
  if (py > 50) { setPlayerPos(px, 50); fvy = 0; }
  if (py < 12) { setPlayerPos(px, 12); fvy = 0; }

  // collect rings
  for (auto &o : objs) {
    if (!o.live || o.kind != 1) continue;
    // drift
    o.y += ((animFrame + o.x) & 8) ? 0 : (((animFrame >> 2) & 1) ? 1 : -1);
    if (o.y < 14) o.y = 14;
    if (o.y > 48) o.y = 48;
    if (abs(px - o.x) < 6 && abs(py - o.y) < 6) {
      o.live = false; spRings++; score += 20; beep(1800, 20);
    }
  }

  // emerald appears after 6 rings
  if (spRings >= 6 && !spGot) {
    if (abs(px - spEX) < 8 && abs(py - spEY) < 8) {
      spGot = 1;
      emeralds++;
      score += 500;
      beep(1600, 200);
      clearT = 40;
      // brief then advance
    }
  }
  // bob emerald
  spEY = 26 + ((animFrame >> 2) & 3);

  if (spGot) {
    if (clearT) clearT--;
    else transitionTo(nextStage, true);
  }
}

void clearStep(uint8_t e) {
  clearT++;
  if (e & B_MID) {
    // Mid skips ahead: finish the jingle / count, or move on once it's done
    if (tallyDoneT && clearT > tallyDoneT + 10) { advanceAfterClear(); return; }
    watch::stopTune();
    score += tallyRingBonus + tallyTimeBonus;
    tallyRingBonus = tallyTimeBonus = 0;
  }
  if (!tallyDoneT) {
    // Count up once the fanfare has finished (and the screen has been up ~1.5 s)
    if (clearT < 45 || watch::tunePlaying()) return;
    uint32_t r = tallyRingBonus < 100 ? tallyRingBonus : 100;
    uint32_t t = tallyTimeBonus < 100 ? tallyTimeBonus : 100;
    tallyRingBonus -= r; tallyTimeBonus -= t;
    score += r + t;
    if ((clearT & 1) && (r || t)) beep(1800, 12);
    if (!tallyRingBonus && !tallyTimeBonus) { tallyDoneT = clearT; beep(1400, 120); }
    return;
  }
  if (clearT >= tallyDoneT + 60) advanceAfterClear();   // ~2 s on the totals
}

// ---------- draw ----------
uint16_t runAnim;   // run-cycle phase (8.8 fixed): advances faster the faster he runs, like Sonic 1

// Blit one sprite frame (sonic_sprites.h); mirrored when facing left. A dark 1-px halo on
// the sides and top keeps him readable over ground patterns and scenery (none below, so the
// shoes still sit on the ground line).
void blitSonic(int16_t left, int16_t top, uint8_t frameId, bool faceRight) {
  const uint8_t *src = sonicFrames[frameId];
  for (uint8_t pass = 0; pass < 2; pass++) {
    u8g2.setDrawColor(pass);
    for (uint8_t row = 0; row < SONIC_H; row++) {
      uint32_t bits = 0;
      for (uint8_t k = 0; k < SONIC_ROWBYTES; k++)
        bits |= (uint32_t)pgm_read_byte(&src[row * SONIC_ROWBYTES + k]) << (8 * k);
      if (!bits) continue;
      int16_t y = top + row;
      for (uint8_t col = 0; col < SONIC_W; col++) {
        if (!(bits & (1ul << col))) continue;
        int16_t x = left + (faceRight ? col : (SONIC_W - 1 - col));
        if (pass) plot(x, y);
        else { plot(x - 1, y); plot(x + 1, y); plot(x, y - 1); }
      }
    }
  }
  u8g2.setDrawColor(1);
}

// Centre at (x, y): feet on the row y + RAD_STAND (the spin-ball's bottom is one row higher)
void drawSonicFrame(int16_t x, int16_t y, uint8_t id, bool faceRight) {
  blitSonic(x - (int16_t)SONIC_W / 2, y - (int16_t)SONIC_H + 1 + RAD_STAND, id, faceRight);
}

// Simple version for the watch face: run cycle / spin-ball / idle blink by frame count
void drawHogAt(int16_t x, int16_t y, bool roll, bool faceRight, uint8_t frame, bool moving) {
  uint8_t id;
  if (roll) id = SPR_ROLL0 + ((frame >> 1) & 3);
  else if (moving) id = SPR_RUN0 + ((frame >> 1) & 3);
  else id = ((frame & 63) < 3) ? SPR_BLINK : SPR_STAND;
  drawSonicFrame(x, y, id, faceRight);
}

// In-game Sonic: animation follows his speed (walk → run → figure-8 legs at top speed),
// skids when you push against a fast run, spins faster when rolling faster.
void drawHog(int16_t x, int16_t y, bool roll, bool faceRight, uint8_t frame) {
  fx spd = inLoop ? abs(loopGspd) : abs(fvx);
  uint8_t id;
  bool face = faceRight;
  if (roll || groundRoll) {
    if (spd > ZV(3)) runAnim += 256; else runAnim += 128;
    id = SPR_ROLL0 + ((runAnim >> 8) & 3);
  } else if (spd == 0 && !inLoop) {
    id = ((frame & 63) < 3) ? SPR_BLINK : SPR_STAND;
    runAnim = 0;
  } else if (onGround && !inLoop && spd > ZV(1) &&
             (((heldIn() & B_LEFT) && fvx > 0) || ((heldIn() & B_RIGHT) && fvx < 0))) {
    id = SPR_SKID;           // braking: lean back, still facing the way he's sliding
    face = fvx > 0;
  } else if (spd >= 1750) {
    id = SPR_FAST0 + (frame & 1);
  } else {
    runAnim += 24 + spd / 15;  // ~4 steps/s at a walk up to ~15/s near top speed
    id = SPR_RUN0 + ((runAnim >> 8) & 3);
  }
  drawSonicFrame(x, y, id, face);
}

void drawLoopTrack() {
  if (!loopTrack.live || !onScreen(loopTrack.cx, loopTrack.r + 10)) return;
  int16_t x = sx(loopTrack.cx);
  int16_t y = sy(loopTrack.cy);
  int16_t r = loopTrack.r;
  // Thick track like GH checker loop (outer + mid + inner)
  circleSafe(x, y, r);
  circleSafe(x, y, r - 1);
  circleSafe(x, y, r - 4);
  circleSafe(x, y, r - 6);
  // Mouth openings (clear bottom-left / bottom-right notches)
  int16_t floorY = sy(loopTrack.cy + r);
  u8g2.setDrawColor(0);
  fillBoxClipped(x - r - 1, floorY - 8, 12, 8);
  fillBoxClipped(x + r - 10, floorY - 8, 12, 8);
  u8g2.setDrawColor(1);
  // Checker ticks around the ring
  for (uint8_t i = 0; i < 20; i++) {
    float th = i * (6.2831853f / 20.0f);
    int16_t tx = x + (int16_t)((r - 2.5f) * cosf(th));
    int16_t ty = y + (int16_t)((r - 2.5f) * sinf(th));
    if ((i & 1) == 0) { plot(tx, ty); plot(tx + 1, ty); }
  }
}

// Sonic GHZ sunflower — solid center disc, 8 sharp petals, tall stem + leaf pairs
void drawSunflower(int16_t x, int16_t baseY) {
  int16_t cy = baseY - 30;

  // stem
  vLineSafe(x, cy + 6, baseY - (cy + 6));

  // three leaf pairs (pointed, angled up & out)
  fillTriSafe(x, baseY - 8,  x - 6, baseY - 13, x - 3, baseY - 6);
  fillTriSafe(x, baseY - 8,  x + 6, baseY - 13, x + 3, baseY - 6);
  fillTriSafe(x, baseY - 16, x - 5, baseY - 20, x - 2, baseY - 14);
  fillTriSafe(x, baseY - 16, x + 5, baseY - 20, x + 2, baseY - 14);
  fillTriSafe(x, baseY - 23, x - 4, baseY - 26, x - 2, baseY - 21);
  fillTriSafe(x, baseY - 23, x + 4, baseY - 26, x + 2, baseY - 21);

  // 8 filled triangular petals (N/NE/E/SE/S/SW/W/NW)
  fillTriSafe(x,     cy - 9, x - 3, cy - 3, x + 3, cy - 3);  // N
  fillTriSafe(x + 7, cy - 7, x + 1, cy - 4, x + 4, cy - 1);  // NE
  fillTriSafe(x + 9, cy,     x + 3, cy - 3, x + 3, cy + 3);  // E
  fillTriSafe(x + 7, cy + 7, x + 1, cy + 4, x + 4, cy + 1);  // SE
  fillTriSafe(x,     cy + 9, x - 3, cy + 3, x + 3, cy + 3);  // S
  fillTriSafe(x - 7, cy + 7, x - 1, cy + 4, x - 4, cy + 1);  // SW
  fillTriSafe(x - 9, cy,     x - 3, cy - 3, x - 3, cy + 3);  // W
  fillTriSafe(x - 7, cy - 7, x - 1, cy - 4, x - 4, cy - 1);  // NW

  // solid round face on the petal bases (dark center like the reference)
  discSafe(x, cy, 4);
  u8g2.setDrawColor(0);
  discSafe(x, cy, 2);
  u8g2.setDrawColor(1);
}

void drawRingGlyph(int16_t x, int16_t y) {
  // Ring: 13 px across, 2 px band
  circleSafe(x, y, 6);
  circleSafe(x, y, 5);
  u8g2.setDrawColor(0); circleSafe(x, y, 3); u8g2.setDrawColor(1);
  circleSafe(x, y, 4);
}

// Classic Sonic spikes: 3 spears ~Sonic-tall, 4 px bases, group ~his width
void drawSpikes(int16_t x, int16_t baseY) {
  const int16_t h = 18;
  for (uint8_t i = 0; i < 3; i++) {
    int16_t bx = x + (int16_t)i * 4;
    fillTriSafe(bx, baseY, bx + 4, baseY, bx + 2, baseY - h);
    triSafe(bx, baseY, bx + 4, baseY, bx + 2, baseY - h);
  }
  hLineSafe(x, baseY - 1, 12);
}

void drawPalm(int16_t x, int16_t baseY) {
  // ~60 px tall (sunflower ~50): trunk leans right with bark bands, five drooping
  // fronds drawn two pixels thick so they survive the OLED, and a pair of coconuts.
  const int16_t H = 50;
  for (int16_t i = 0; i <= H; i++) {
    int16_t cx = x + (i * i) / 400;          // gentle lean, ~6 px at the crown
    int16_t yy = baseY - i;
    plot(cx, yy); plot(cx + 1, yy); plot(cx + 2, yy);
    if ((i & 3) == 0) plot(cx + 3, yy);      // bark bands
  }
  int16_t tx = x + (H * H) / 400 + 1, ty = baseY - H;
  static const int8_t FR[5][4] = {
    {-9, -5, -19,  4},    // long left, drooping
    {-5, -9, -12, -7},    // upper left
    { 1,-10,   6,-11},    // top
    { 6, -8,  14, -6},    // upper right
    {10, -4,  19,  5},    // long right, drooping
  };
  for (uint8_t f = 0; f < 5; f++) {
    for (int8_t t = 0; t <= 1; t++) {
      lineSafe(tx, ty + t, tx + FR[f][0], ty + FR[f][1] + t);
      lineSafe(tx + FR[f][0], ty + FR[f][1] + t, tx + FR[f][2], ty + FR[f][3] + t);
    }
  }
  discSafe(tx - 1, ty + 4, 2);
  discSafe(tx + 3, ty + 4, 2);
}

void drawPipe(int16_t x, int16_t baseY) {
  frameSafe(x - 5, baseY - 22, 12, 22);
  hLineSafe(x - 7, baseY - 22, 16);
  hLineSafe(x - 7, baseY - 12, 16);
  fillBoxClipped(x - 2, baseY - 5, 7, 5);
}

// Casino "neon": a card-suit outline on a pole that blinks every ~0.5 s (no text/logos —
// they turn into blobs at 128×64). Suit picked from the position.
void drawNeon(int16_t x, int16_t baseY) {
  vLineSafe(x, baseY - 12, 12);
  if ((animFrame / 15) & 1) return;          // off phase
  int16_t cy = baseY - 21;
  switch ((x >> 5) & 3) {
    case 0:   // diamond
      lineSafe(x, cy - 8, x + 6, cy); lineSafe(x + 6, cy, x, cy + 8);
      lineSafe(x, cy + 8, x - 6, cy); lineSafe(x - 6, cy, x, cy - 8);
      break;
    case 1:   // heart
      circleSafe(x - 3, cy - 2, 3); circleSafe(x + 3, cy - 2, 3);
      lineSafe(x - 6, cy - 1, x, cy + 7); lineSafe(x + 6, cy - 1, x, cy + 7);
      break;
    case 2:   // spade
      lineSafe(x - 6, cy + 1, x, cy - 7); lineSafe(x + 6, cy + 1, x, cy - 7);
      circleSafe(x - 3, cy + 2, 3); circleSafe(x + 3, cy + 2, 3);
      vLineSafe(x, cy + 4, 5);
      break;
    default:  // club
      circleSafe(x, cy - 4, 3); circleSafe(x - 4, cy + 1, 3); circleSafe(x + 4, cy + 1, 3);
      vLineSafe(x, cy + 3, 6);
      break;
  }
}

// Green Hill totem pole: three stacked faces with wings on top, wireframe (background)
void drawTotem(int16_t x, int16_t baseY) {
  for (uint8_t k = 0; k < 3; k++) {
    int16_t top = baseY - 11 * (k + 1);
    frameSafe(x - 5, top, 11, 11);
    plot(x - 2, top + 4); plot(x + 2, top + 4);
    hLineSafe(x - 2, top + 7, 5);
  }
  int16_t t = baseY - 33;
  lineSafe(x - 5, t + 2, x - 11, t - 2); lineSafe(x - 11, t - 2, x - 5, t + 6);
  lineSafe(x + 5, t + 2, x + 11, t - 2); lineSafe(x + 11, t - 2, x + 5, t + 6);
}

// In-game sunflower as an outline, so it reads as scenery and never as a spike.
// (The filled version stays on the watch face.)
void drawSunflowerOutline(int16_t x, int16_t baseY) {
  int16_t cy = baseY - 38;
  vLineSafe(x, cy + 7, baseY - (cy + 7));
  triSafe(x, baseY - 10, x - 8, baseY - 16, x - 4, baseY - 7);
  triSafe(x, baseY - 10, x + 8, baseY - 16, x + 4, baseY - 7);
  triSafe(x, baseY - 20, x - 6, baseY - 25, x - 2, baseY - 17);
  triSafe(x, baseY - 20, x + 6, baseY - 25, x + 2, baseY - 17);
  triSafe(x,      cy - 11, x - 4, cy - 4, x + 4, cy - 4);
  triSafe(x + 9,  cy - 9,  x + 1, cy - 5, x + 5, cy - 1);
  triSafe(x + 11, cy,      x + 4, cy - 4, x + 4, cy + 4);
  triSafe(x + 9,  cy + 9,  x + 1, cy + 5, x + 5, cy + 1);
  triSafe(x,      cy + 11, x - 4, cy + 4, x + 4, cy + 4);
  triSafe(x - 9,  cy + 9,  x - 1, cy + 5, x - 5, cy + 1);
  triSafe(x - 11, cy,      x - 4, cy - 4, x - 4, cy + 4);
  triSafe(x - 9,  cy - 9,  x - 1, cy - 5, x - 5, cy - 1);
  circleSafe(x, cy, 5);
  circleSafe(x, cy, 2);
}

void drawGear(int16_t x, int16_t baseY) {
  int16_t cy = baseY - 12;
  circleSafe(x, cy, 6);
  circleSafe(x, cy, 5);
  discSafe(x, cy, 2);
  uint8_t rot = (animFrame >> 3) & 3;
  static const int8_t DX[4] = {8, 0, -8, 0};
  static const int8_t DY[4] = {0, 8, 0, -8};
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t a = (rot + i) & 3;
    discSafe(x + DX[a], cy + DY[a], 1);
  }
}

void drawFlame(int16_t x, int16_t baseY, bool on) {
  fillBoxClipped(x - 4, baseY - 4, 9, 4);
  if (on) {
    triSafe(x, baseY - 18, x - 5, baseY - 4, x + 5, baseY - 4);
    triSafe(x, baseY - 12, x - 2, baseY - 4, x + 2, baseY - 4);
    if (animFrame & 2) plot(x, baseY - 20);
  }
}

void drawBumper(int16_t x, int16_t y, bool flash) {
  discSafe(x, y, flash ? 9 : 8);
  u8g2.setDrawColor(0);
  discSafe(x, y, 4);
  u8g2.setDrawColor(1);
  circleSafe(x, y, 4);
  plot(x, y);
}

void drawSaw(int16_t x, int16_t y, uint8_t spin) {
  circleSafe(x, y, 6);
  discSafe(x, y, 2);
  uint8_t a = (spin >> 1) & 3;
  static const int8_t DX[4] = {8, 0, -8, 0};
  static const int8_t DY[4] = {0, 8, 0, -8};
  lineSafe(x - DX[a], y - DY[a], x + DX[a], y + DY[a]);
  lineSafe(x - DY[a], y + DX[a], x + DY[a], y - DX[a]);
}

void drawDeco(int16_t x, int16_t baseY, uint8_t typ) {
  if (typ == 0) drawSunflowerOutline(x, baseY);
  else if (typ == 5) drawTotem(x, baseY);
  else if (typ == 1) drawPalm(x, baseY);
  else if (typ == 2) drawPipe(x, baseY);
  else if (typ == 3) drawNeon(x, baseY);
  else drawGear(x, baseY);
}

// Fish leaping straight up: body, side fins, tail fin below, teeth + eye cut out
void drawFish(int16_t x, int16_t y) {
  fillBoxClipped(x - 3, y - 5, 7, 10);
  vLineSafe(x - 4, y - 2, 3); vLineSafe(x + 4, y - 2, 3);
  fillTriSafe(x - 4, y + 9, x + 4, y + 9, x, y + 4);
  u8g2.setDrawColor(0);
  plot(x - 2, y - 5); plot(x, y - 5); plot(x + 2, y - 5);   // teeth
  plot(x + 1, y - 2); plot(x + 2, y - 2);                   // eye
  u8g2.setDrawColor(1);
}

// Crab: shell with eye stalks, legs, and big claws that snap open and shut
void drawCrab(int16_t x, int16_t y, uint8_t frame) {
  int8_t claw = (frame & 4) ? 1 : 0;
  fillBoxClipped(x - 6, y - 3, 13, 6);
  u8g2.setDrawColor(0); hLineSafe(x - 4, y - 1, 9); u8g2.setDrawColor(1);
  vLineSafe(x - 3, y - 7, 4); vLineSafe(x + 3, y - 7, 4);
  discSafe(x - 3, y - 7, 1); discSafe(x + 3, y - 7, 1);
  for (int8_t k = -1; k <= 1; k += 2) {
    lineSafe(x + k * 3, y + 3, x + k * 6, y + 6);            // legs
    lineSafe(x + k * 6, y,     x + k * 9, y - 2);            // arm
    fillTriSafe(x + k * 9, y - 2, x + k * (12 + claw), y - 6 - claw, x + k * (12 + claw), y - 1);
    fillTriSafe(x + k * 9, y - 2, x + k * (12 + claw), y + 2 + claw, x + k * (12 + claw), y - 1 + claw);
  }
}

void drawBee(int16_t x, int16_t y, uint8_t frame, uint8_t state) {
  discSafe(x, y, 4); fillBoxClipped(x - 1, y - 3, 6, 6);
  u8g2.setDrawColor(0); vLineSafe(x + 1, y - 3, 6); vLineSafe(x + 4, y - 2, 4); u8g2.setDrawColor(1);
  discSafe(x - 4, y, 3);
  u8g2.setDrawColor(0); plot(x - 5, y - 1); u8g2.setDrawColor(1);
  triSafe(x + 5, y, x + 9, y - 1, x + 9, y + 1);
  int16_t wy = y - ((frame & 1) ? 7 : 5);
  hLineSafe(x - 1, wy, 5); hLineSafe(x, wy - 1, 3);
  if (state == 1) discSafe(x - 7, y + 3, 1);
}

void drawEggWrecker() {
  int16_t ex = sx(eggX), ey = sy(eggY);
  bool flash = bossCool && (bossCool & 2);
  if (bossPhase == EGG_BLOWUP) { ex += (eggT & 2) ? 1 : -1; }   // shudder

  // Chain: a link every 4 px from the belly to the ball, then the checkered ball
  if (chainLen > 1) {
    int16_t pxv = ex, pyv = sy(eggPivotY());
    int16_t bx = sx(ballWX), by = sy(ballWY);
    float sa = sinf(ballAng), ca = cosf(ballAng);
    for (float d = 2; d < chainLen - BALL_R + 1; d += 4) {
      int16_t lx = pxv + (int16_t)lroundf(d * sa), ly = pyv + (int16_t)lroundf(d * ca);
      frameSafe(lx - 1, ly - 1, 3, 3);
    }
    // checker quarters turn with the chain
    for (int16_t yy = -BALL_R; yy <= BALL_R; yy++)
      for (int16_t xx = -BALL_R; xx <= BALL_R; xx++) {
        int16_t r2 = xx * xx + yy * yy;
        if (r2 > BALL_R * BALL_R + 2) continue;
        float u = xx * ca - yy * sa, v = xx * sa + yy * ca;
        bool lit = r2 >= (BALL_R - 1) * (BALL_R - 1) || ((u >= 0) == (v >= 0));
        if (lit) plot(bx + xx, by + yy);
      }
  }

  // Ship: round cockpit hull, skirt with vents, pilot dome
  if (!flash) {
    discSafe(ex, ey + 5, 8);
    fillBoxClipped(ex - 7, ey + 10, 14, 4);
    u8g2.setDrawColor(0);
    for (int16_t i = -5; i <= 5; i += 4) vLineSafe(ex + i, ey + 10, 4);
    u8g2.setDrawColor(1);
  } else {
    circleSafe(ex, ey + 5, 8);
    frameSafe(ex - 7, ey + 10, 14, 4);
  }
  discSafe(ex, ey, 4);
  u8g2.setDrawColor(0);
  if (bossCool || bossPhase == EGG_BLOWUP) {      // ouch: squeezed-shut eyes
    hLineSafe(ex - 3, ey - 1, 2); hLineSafe(ex + 2, ey - 1, 2);
  } else {
    plot(ex - 2, ey - 1); plot(ex + 2, ey - 1);
  }
  u8g2.setDrawColor(1);
  if (eggLaugh && (eggLaugh & 4)) frameSafe(ex - 2, ey + 2, 5, 3);   // laughing
  else hLineSafe(ex - 3, ey + 3, 7);

  // Smoke puffs while it breaks up / escapes
  if (bossPhase >= EGG_BLOWUP) {
    for (uint8_t i = 0; i < 3; i++) {
      uint8_t h = h8((uint16_t)(eggT / 3 + i * 37));
      int16_t ox = (h & 15) - 8, oy = ((h >> 4) & 7) - 6;
      circleSafe(ex + ox - (bossPhase == EGG_FLEE ? 6 : 0), ey + oy, 2 + (eggT + i) % 3);
    }
  }

  frameSafe(2, 57, 50, 6);
  fillBoxClipped(4, 59, bossHP * 5, 2);
}

void drawBanner() {
  // Compact title card: zone name over "ACT n" / boss name
  u8g2.setFont(u8g2_font_5x7_tr);
  const char *top = (const char *)zoneName();
  const char *bot = bossOn ? "EGG WRECKER" : (act ? "ACT 2" : "ACT 1");
  int16_t w = u8g2.getStrWidth(top);
  int16_t w2 = u8g2.getStrWidth(bot);
  if (w2 > w) w = w2;
  int16_t bw = w + 10;
  // Slide: in from the right (eases to a stop), off to the left (accelerating)
  uint8_t age = INTRO_LEN - intro;
  int16_t off = 0;
  if (age < INTRO_IN) { int16_t k = INTRO_IN - age; off = k * k * 128 / (INTRO_IN * INTRO_IN); }
  else if (intro < INTRO_OUT) { int16_t k = INTRO_OUT - intro; off = -(k * k * (64 + bw / 2 + 4)) / (INTRO_OUT * INTRO_OUT); }
  int16_t bx = (128 - bw) / 2 + off;
  u8g2.setDrawColor(0); u8g2.drawBox(bx, 16, bw, 20);
  u8g2.setDrawColor(1); u8g2.drawFrame(bx, 16, bw, 20);
  u8g2.setCursor(bx + (bw - u8g2.getStrWidth(top)) / 2, 25); u8g2.print(top);
  u8g2.setCursor(bx + (bw - u8g2.getStrWidth(bot)) / 2, 33); u8g2.print(bot);
  u8g2.setFont(u8g2_font_6x10_tr);
}

void drawGame() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setDrawColor(1);

  drawTerrain();

  drawLoopTrack();

  // Pools under the bridges: rippling surface a few px below the deck
  for (auto &b : bridges) {
    if (!b.live) continue;
    int16_t wy = sy(b.y + 10);
    for (int16_t u = b.x0 + 2; u < b.x1 - 1; u += 2) {
      int16_t xs = sx(u);
      if (xs < 0 || xs > 127) continue;
      plot(xs, wy + (((u >> 3) + (animFrame >> 2)) & 1));
      if ((u & 7) == 0) plot(xs, wy + 5);
    }
  }

  for (auto &p : plats) {
    if (!p.live) continue;
    if (p.w <= 0 || p.w > 64) continue;
    int16_t y = sy(p.y);
    if (y < -5 || y > 66) continue;
    fillBoxClipped(sx(p.x), y, p.w, 4);
    u8g2.setDrawColor(0);
    for (int16_t u = 4; u < p.w - 2; u += 8) plot(sx(p.x) + u, y + 2);   // rivets
    u8g2.setDrawColor(1);
  }

  for (auto &o : objs) {
    if (!o.live) continue;
    int16_t x = sx(o.x);
    int16_t y = sy(o.y);
    if (x < -24 || x > 152) continue;
    if (y < -30 || y > 100) continue;
    u8g2.setDrawColor(1);
    if (o.kind == 0) {
      drawSpikes(x, sy(groundAt(o.x)));
    } else if (o.kind == 1) {
      drawRingGlyph(x, y);
    } else if (o.kind == 2) drawCrab(x, y, animFrame);
    else if (o.kind == 3) {
      fillBoxClipped(x - 6, y - 2, 12, 4);                 // spring: base, coil, pad
      plot(x - 3, y - 3); plot(x + 3, y - 3); plot(x - 2, y - 4); plot(x + 2, y - 4);
      fillBoxClipped(x - 5, y - 6, 10, 2);
    } else if (o.kind == 4) drawBee(x, y, animFrame, o.state);
    else if (o.kind == 5) drawDeco(x, y, o.state);
    else if (o.kind == 6) {
      int16_t gh = groundAt(o.x) - o.y;
      if (gh > 0 && gh < 60) { vLineSafe(x, y + 8, gh - 8); vLineSafe(x + 1, y + 8, gh - 8); }
      if (goalT && goalT < GOAL_SPIN_FRAMES) {
        // spinning, slowing down: panel width follows |cos|, alternating faces
        float t = goalT;
        float c = cosf(0.6f * t - 0.0042f * t * t);
        int16_t w = (int16_t)(fabsf(c) * 8.0f) + 1;
        if (c > 0) fillBoxClipped(x - w + 1, y - 3, w * 2, 12);
        else frameSafe(x - w + 1, y - 3, w * 2, 12);
      } else if (goalT) {
        frameSafe(x - 7, y - 3, 16, 12);       // settled: ring emblem
        circleSafe(x + 1, y + 3, 4);
      } else {
        fillBoxClipped(x - 7, y - 3, 16, 12);  // not yet passed
      }
    } else if (o.kind == 7) {
      int16_t h = o.state ? o.state : 18;
      fillBoxClipped(x, y - h, 8, h);
      u8g2.setDrawColor(0);
      for (int16_t k = 4; k < h - 1; k += 6) hLineSafe(x + 2, y - h + k, 4);   // block seams
      u8g2.setDrawColor(1);
    } else if (o.kind == 8) drawFlame(x, y, o.state);
    else if (o.kind == 9) drawBumper(x, y, o.timer > 0);
    else if (o.kind == 10) drawSaw(x, y, o.timer);
    else if (o.kind == 11 && o.y < o.home - 5) drawFish(x, y);   // hidden while underwater
  }

  // Bridge decks: logs with a gap, following the sag; posts at each end
  for (uint8_t i = 0; i < 6; i++) {
    const Bridge &b = bridges[i];
    if (!b.live || b.x1 < cam - 8 || b.x0 > cam + 136) continue;
    for (int16_t u = b.x0; u < b.x1; u += 8) {
      int16_t top = b.y + bridgeSag(i, u + 3);
      fillBoxClipped(sx(u), sy(top), 7, 4);
    }
    fillBoxClipped(sx(b.x0) - 1, sy(b.y - 6), 2, 10);
    fillBoxClipped(sx(b.x1) - 1, sy(b.y - 6), 2, 10);
  }

  for (auto &s : shots) {
    if (!s.live || s.vy == 99) continue;
    // dark rim so the ball reads against grass / pipes, lit core
    u8g2.setDrawColor(0); discSafe(sx(s.x), sy(s.y), 3);
    u8g2.setDrawColor(1); discSafe(sx(s.x), sy(s.y), 2);
  }

  for (auto &r : scat) {
    if (!r.live) continue;
    if (r.life < 30 && ((r.life >> 1) & 1)) continue;
    drawRingGlyph(sx(r.x), sy(r.y));
  }

  if (bossOn) drawEggWrecker();

  bool blink = invul && ((invul >> 2) & 1);
  if (!blink) drawHog(sx(px), sy(py), rolling, faceRight, animFrame);

  // Compact HUD: one 7 px strip, backed in black so it reads over terrain
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 52, 7);
  u8g2.drawBox(100, 0, 28, 7);
  u8g2.setDrawColor(1);
  u8g2.setCursor(0, 6); u8g2.print(F("R")); u8g2.print(rings);
  u8g2.setCursor(26, 6); printTime(actSeconds());
  for (uint8_t i = 0; i < emeralds && i < 7; i++) plot(70 + i * 4, 3);
  for (uint8_t i = 0; i < lives; i++) discSafe(124 - i * 6, 3, 2);
  u8g2.setFont(u8g2_font_6x10_tr);
  if (intro) drawBanner();
  if (goalT > 8) {
    // "STAGE CLEAR" slides in from the right and sits at the top while the fanfare plays
    u8g2.setFont(u8g2_font_ncenB10_tr);
    const char *txt = "STAGE CLEAR";
    int16_t w = u8g2.getStrWidth(txt);
    int16_t k = 16 - (int16_t)goalT;               // 8 frames of slide-in
    int16_t off = k > 0 ? k * k * 2 : 0;
    int16_t x = (128 - w) / 2 + off;
    u8g2.setDrawColor(0); u8g2.drawBox(x - 3, 9, w + 6, 14);
    u8g2.setDrawColor(1); u8g2.drawFrame(x - 3, 9, w + 6, 14);
    u8g2.setCursor(x, 20); u8g2.print(txt);
    u8g2.setFont(u8g2_font_6x10_tr);
  }
}

void drawSpecial() {
  u8g2.setFont(u8g2_font_6x10_tr);
  // star field
  for (uint8_t i = 0; i < 18; i++) {
    uint8_t sx0 = (i * 37 + animFrame) & 127;
    uint8_t sy0 = (i * 19) & 63;
    u8g2.drawPixel(sx0, sy0);
  }
  u8g2.setCursor(28, 10); u8g2.print(F("SPECIAL STAGE"));
  for (auto &o : objs) if (o.live && o.kind == 1) {
    drawRingGlyph(o.x, o.y);
  }
  if (spRings >= 6 && !spGot) {
    // emerald diamond
    u8g2.drawTriangle(spEX, spEY - 5, spEX - 4, spEY, spEX + 4, spEY);
    u8g2.drawTriangle(spEX, spEY + 5, spEX - 4, spEY, spEX + 4, spEY);
  }
  if (spGot) { u8g2.setCursor(34, 36); u8g2.print(F("EMERALD!")); }
  drawHog(px, py, false, true, animFrame);
  u8g2.setCursor(0, 63); u8g2.print(F("GET ")); u8g2.print(spRings); u8g2.print(F("/6  T"));
  u8g2.print(spTimer / 30);
}

// Tally rows slide in from the right one after another (row r starts 2 frames after r−1)
int16_t tallyOff(uint8_t row) {
  int16_t k = (int16_t)(row * 2 + 7) - (int16_t)clearT;
  if (k <= 0) return 0;
  return k > 7 ? 128 : k * k * 128 / 49;
}

void drawClear() {
  // Zone + "ACT n CLEAR!" header
  u8g2.setFont(u8g2_font_5x7_tr);
  const char *zn = (const char *)zoneName();
  u8g2.setCursor((128 - u8g2.getStrWidth(zn)) / 2 + tallyOff(0), 7); u8g2.print(zn);
  u8g2.setFont(u8g2_font_6x10_tr);
  const char *hd = act ? "ACT 2 CLEAR!" : "ACT 1 CLEAR!";
  u8g2.setCursor((128 - u8g2.getStrWidth(hd)) / 2 + tallyOff(1), 18); u8g2.print(hd);
  if (tallyOff(1) < 128) hLineSafe(tallyOff(1), 21, 128);

  u8g2.setCursor(2 + tallyOff(2), 32); u8g2.print(F("TIME  ")); printTime(actSeconds());
  u8g2.setCursor(2 + tallyOff(3), 43); u8g2.print(F("RINGS ")); u8g2.print(rings);
  u8g2.setCursor(2 + tallyOff(4), 54); u8g2.print(F("BONUS ")); u8g2.print(tallyRingBonus + tallyTimeBonus);
  u8g2.setCursor(2 + tallyOff(5), 64); u8g2.print(F("SCORE ")); u8g2.print(score);

  // Speed rank, revealed after a beat
  if (clearT > 20) {
    static const char *const RANKS[3] = {"A+", "B", "D"};
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.setCursor(96, 30); u8g2.print(F("RANK"));
    u8g2.setFont(u8g2_font_ncenB18_tr);
    const char *rk = RANKS[lastRank];
    u8g2.setCursor(106 - u8g2.getStrWidth(rk) / 2, 52); u8g2.print(rk);
    u8g2.setFont(u8g2_font_6x10_tr);
  }
}

void drawOver() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(34, 16); u8g2.print(F("GAME OVER"));
  u8g2.setCursor(34, 30); u8g2.print(F("SCORE ")); u8g2.print(score);
  u8g2.setCursor(4, 46); u8g2.print(F("MID=retry GAME=quit"));
}
void drawWin() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(22, 16); u8g2.print(F("ZONE COMPLETE"));
  u8g2.setCursor(28, 30); u8g2.print(F("EMERALDS ")); u8g2.print(emeralds);
  u8g2.setCursor(34, 44); u8g2.print(F("SCORE ")); u8g2.print(score);
  u8g2.setCursor(30, 58); u8g2.print(F("PRESS ANY"));
}

void menuLoop(uint8_t e) {
  if (e & B_GAME) { mode = GAME; return; }
  if (e & (B_LEFT | B_RIGHT)) menuSel = (menuSel + ((e & B_LEFT) ? 2 : 1)) % 3;
  if (e & B_MID) {
    if (menuSel == 0) mode = GAME;
    else if (menuSel == 1) transitionTo(startGame);
    else watch::exitToClock();
  }
}
void drawMenu() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(46, 12); u8g2.print(F("PAUSED"));
  u8g2.setCursor(30, 28); u8g2.print(menuSel == 0 ? F("> CONTINUE") : F("  CONTINUE"));
  u8g2.setCursor(30, 40); u8g2.print(menuSel == 1 ? F("> RESTART") : F("  RESTART"));
  u8g2.setCursor(30, 52); u8g2.print(menuSel == 2 ? F("> QUIT") : F("  QUIT"));
  u8g2.setCursor(0, 63); u8g2.print(F("L/R pick  MID ok"));
}

// ===================== Level select (debug) =====================
// Entries 0..7 = zone*2 + act, 8 = special stage.
const uint8_t SELECT_COUNT = 9;

void selectStep(uint8_t e) {
  if (e & B_LEFT)  { selIdx = (selIdx + SELECT_COUNT - 1) % SELECT_COUNT; beep(900, 10); }
  if (e & B_RIGHT) { selIdx = (selIdx + 1) % SELECT_COUNT; beep(900, 10); }
  if (e & B_MODE)  { splashFrame = 20; codeHist = 0; mode = SPLASH; return; }
  if (e & B_MID) {
    randomSeed(micros());
    rings = 0; score = 0; lives = 3; emeralds = 0;
    if (selIdx < 8) transitionTo([] { startAct(selIdx / 2, selIdx % 2); });
    else transitionTo([] { zone = 0; act = 0; lvl = &ACTS[0][0]; startSpecial(); }, true);
  }
}

void drawSelect() {
  static const char *const ZN[4] = {"GREEN HILL", "CHEM PLANT", "CASINO NIGHT", "SCRAP BRAIN"};
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(34, 7); u8g2.print(F("LEVEL SELECT"));
  hLineSafe(0, 9, 128);
  // 6 visible rows, scrolled so the cursor stays on screen
  int8_t first = (int8_t)selIdx - 2;
  if (first < 0) first = 0;
  if (first > SELECT_COUNT - 6) first = SELECT_COUNT - 6;
  for (uint8_t r = 0; r < 6; r++) {
    uint8_t i = first + r;
    int16_t y = 18 + r * 8;
    if (i == selIdx) { fillBoxClipped(0, y - 7, 128, 8); u8g2.setDrawColor(0); }
    u8g2.setCursor(4, y);
    if (i < 8) { u8g2.print(ZN[i / 2]); u8g2.print(F("  ACT ")); u8g2.print(i % 2 + 1); }
    else u8g2.print(F("SPECIAL STAGE"));
    u8g2.setDrawColor(1);
  }
  u8g2.setCursor(2, 64); u8g2.print(F("L/R  JUMP=go  MODE=back"));
}

// ===================== WatchCore hooks =====================
void gameStart() {            // Game pressed on the clock face
  trState = TR_NONE; trLevel = 0; trAction = nullptr;
  splashFrame = 0;
  codeHist = 0;
  mode = SPLASH;
}

void gameUpdate(uint8_t e) {
  if (trState == TR_OUT) {                       // frozen while the screen fades out
    if (++trLevel >= TR_STEPS) {
      trLevel = TR_STEPS;
      trState = TR_IN;
      void (*a)() = trAction; trAction = nullptr;
      if (a) a();
    }
    return;
  }
  if (trState == TR_IN) {
    if (trLevel) trLevel--;
    if (!trLevel) trState = TR_NONE;
    e = 0;                                       // no input until the fade-in is under way
  }
  switch (mode) {
    case SPLASH:
      // Only Jump (play) and Mode (back to the clock) act here; Left/Right feed the code
      if (splashFrame < 250) splashFrame++;
      if (e & B_LEFT)  codeHist = (uint16_t)((codeHist << 2) | 1) & 0x0FFF;
      if (e & B_RIGHT) codeHist = (uint16_t)((codeHist << 2) | 2) & 0x0FFF;
      if (e & B_MID) {
        if (codeHist == LEVEL_SELECT_CODE) { selIdx = 0; mode = SELECT; beep(1800, 80); }
        else transitionTo(startGame);
      } else if (e & B_MODE) watch::exitToClock();
      break;
    case SELECT: selectStep(e); break;
    case GAME: gameStep(e); break;
    case SPECIAL: specialStep(e); break;
    case CLEAR: clearStep(e); break;
    case MENU: menuLoop(e); break;
    case OVER:
      if (e & B_MID) transitionTo(startGame);
      else if (e & (B_GAME | B_MODE)) watch::exitToClock();  // Mode always escapes to clock
      break;
    case WIN: if (e) watch::exitToClock(); break;
    case DEAD: deathStep(e); break;
  }
}

// Ordered-dither fade: level 0 leaves the frame alone, TR_STEPS blanks it (or whites it out)
void applyFade(uint8_t level, bool white) {
  static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  uint8_t thr = (uint8_t)(level * 16 / TR_STEPS);
  uint8_t mask[4];
  for (uint8_t xm = 0; xm < 4; xm++) {
    uint8_t m = 0;
    for (uint8_t b = 0; b < 8; b++) if (BAYER[b & 3][xm] < thr) m |= (uint8_t)(1 << b);
    mask[xm] = m;
  }
  uint8_t *buf = u8g2.getBufferPtr();
  for (uint16_t i = 0; i < 8 * 128; i++) {
    if (white) buf[i] |= mask[i & 3];
    else buf[i] &= (uint8_t)~mask[i & 3];
  }
}

void gameDraw() {
  switch (mode) {
    case SPLASH: drawSplash(); break;
    case GAME: drawGame(); break;
    case SPECIAL: drawSpecial(); break;
    case CLEAR: drawClear(); break;
    case MENU: drawMenu(); break;
    case OVER: drawOver(); break;
    case WIN: drawWin(); break;
    case DEAD: drawGame(); break;
    case SELECT: drawSelect(); break;
  }
  if (trLevel) applyFade(trLevel, trWhite);
}

// Alarm interrupted a run: come back to the pause menu, not straight into play
void gameInterrupted() {
  if (mode == GAME) { menuSel = 0; mode = MENU; }
}

const watch::WatchApp APP = {
  stepWatchFace,    // faceStep
  drawView,         // faceDraw
  gameStart,        // appStart
  gameUpdate,       // appUpdate
  gameDraw,         // appDraw
  gameInterrupted,  // appInterrupted
  nullptr,          // setDraw  (default set screens)
  nullptr,          // ringDraw (default alarm screen)
  "MID",            // nextLabel
};

void setup() { watch::begin(APP); }
void loop() { watch::run(); }
