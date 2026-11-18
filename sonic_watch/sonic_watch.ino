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
enum : uint8_t { SPLASH, GAME, MENU, OVER, WIN, SPECIAL, CLEAR };
uint8_t mode = SPLASH;
uint8_t splashFrame = 0;

// Watch-face Sonic vignette (walk + spin-jump for a ring)
int16_t wfX = -14;
int16_t wfY = 54;
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
  const int16_t ground = 54;
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
struct Shot { int16_t x, y; int8_t vx, vy; bool live; };
struct Scatter { int16_t x, y; int8_t vx, vy; uint8_t life; bool live; };

// Whole act stays resident — no scroll-despawn (draw/AI cull only)
Obj objs[256];   // a hand-built act is ~150–230 objects (each ring is one)
Plat plats[64];
Shot shots[6];
Scatter scat[3];
const uint8_t SCATTER_LIFE = 90;
const uint8_t SCATTER_GRACE = 30;
int16_t ballWX, ballWY;
uint8_t gStrip[128];


// Deterministic layout hash (no random() in level gen)
uint8_t h8(uint16_t n) {
  n = (uint16_t)(n ^ (n << 7));
  n = (uint16_t)(n ^ (n >> 5));
  n = (uint16_t)(n ^ (n << 3));
  return (uint8_t)n;
}

// Fixed-point physics (1/256 px), 60Hz steps.
// Watch-tuned: snappy accel, firmer coast (less slippery), hard skid ≈11× accel.
static const fx FX_ONE      = 256;
static const fx ACCEL       = 30;      // ~0.117 — snappy launch
static const fx FRICTION    = 56;      // firmer coast than accel (watch grip)
static const fx BRAKE       = 330;     // ~11× accel — deliberate skid
static const fx TOP_SPD     = 1536;    // 6 px/frame
static const fx START_KICK  = 128;     // 0.5 — immediate bite from standstill
static const fx AIR_ACCEL   = 18;      // air steer so jumps feel controllable
static const fx JUMP_SPD    = 475;     // 6.5 × (64/224) ≈ 1.86
static const fx GRAVITY     = 16;      // 0.21875 × (64/224) = 0.0625
static const fx ROLL_FRIC   = 8;
static const fx MAX_FALL    = 1170;
static const fx SLOPE_RUN   = 24;
static const fx SLOPE_ROLL  = 60;      // ~2.5× run
// Loop / steep-track rules from Sonic 1 (Sonic_SlopeResist / Sonic_SlopeRepel).
// LOOP_SLP is the slope factor: Genesis 0.125 px/frame² on a much bigger loop; scaled up for
// our ~30 px-radius track so it takes the same share of top speed to get round.
static const fx LOOP_SLP    = 80;                 // 0.3125 px/step² · sin(angle): ~1 screen run-up
static const fx LOOP_PUSH   = (LOOP_SLP * 3) / 8; // holding forward (Genesis accel : slope = 3:8)
static const fx LOOP_BRAKE  = LOOP_SLP * 4;       // holding back (Genesis decel : slope = 4:1)
static const fx LOOP_FALL   = 640;                // 2.5 px/step: slower than this on a wall → fall
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

const uint8_t GOAL_SPIN_FRAMES = 75;  // ~2.5 s of spinning signpost before the tally

// Player input — none while the signpost spins (Sonic just coasts to a stop)
uint8_t heldIn() { return goalT ? 0 : watch::held(); }

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
int16_t eggX, eggY, ballAngle;   // Egg Wrecker (screen/arena space)
int8_t eggDir;
int16_t arenaOrigin;             // world X where boss arena starts

// Special stage
uint8_t spRings, spGot;
uint16_t spTimer;
int16_t spEX, spEY;

// Ground height from the act's terrain profile (linear between points)
int16_t terrainAt(int32_t wx) {
  const TerrainPt *t = lvl->ter;
  uint8_t n = lvl->nTer;
  if (wx <= t[0].x) return t[0].y;
  if (wx >= t[n - 1].x) return t[n - 1].y;
  uint8_t lo = 0, hi = n - 1;            // binary search: t[lo].x <= wx < t[hi].x
  while (hi - lo > 1) {
    uint8_t mid = (lo + hi) / 2;
    if (t[mid].x <= wx) lo = mid; else hi = mid;
  }
  int32_t dx = t[hi].x - t[lo].x;
  return t[lo].y + (int16_t)(((int32_t)(t[hi].y - t[lo].y) * (wx - t[lo].x)) / dx);
}

int16_t groundAt(int32_t wx) {
  if (wx < 0) wx = 0;
  // Boss arena: flat ground
  if (bossOn && wx >= arenaOrigin) return 52;
  // Flatten a long runway into / under the loop (solid floor — no pits)
  if (loopTrack.live) {
    int16_t floorY = loopTrack.cy + loopTrack.r;
    int16_t x0 = loopTrack.cx - loopTrack.r - 100;
    int16_t x1 = loopTrack.cx + loopTrack.r + 48;
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
int16_t slopeAt(int16_t wx) {
  return groundAt(wx + 4) - groundAt(wx - 4);
}

void placeGreenHillLoop(int16_t x) {
  // Diameter > screen height — camera must scroll vertically through it
  loopTrack.r = 38;
  int16_t g = terrainAt(x);
  loopTrack.cx = x + loopTrack.r;
  loopTrack.cy = g - loopTrack.r;   // bottom of track on the floor
  loopTrack.live = true;
}

// Strip hazards from the loop approach + ≥100px after the exit
void clearLoopRunway() {
  if (!loopTrack.live) return;
  int16_t L = loopTrack.cx - loopTrack.r - 110;
  int16_t R = loopTrack.cx + loopTrack.r + 100;
  for (auto &o : objs) {
    if (!o.live) continue;
    if (o.x < L || o.x > R) continue;
    if (o.kind == 1 || o.kind == 5 || o.kind == 6) continue;  // rings / deco / goal
    // Keep the run-up spring if it's near the start of the clear zone
    if (o.kind == 3 && o.x < L + 40) continue;
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
  int16_t g = groundAt(x0 + 40);
  for (uint8_t i = 0; i < 7; i++) {
    int16_t x = x0 + (int16_t)i * 16;
    addPlat(x, g + A[i], 18);
    addPlat(x + 6, g + B[i], 18);
    if ((i & 1) == 0) addDeco(x + 2, 2);
  }
  addObj(x0 + 48, g + A[3] - 6, 1);
  addObj(x0 + 64, g + B[4] - 6, 1);
  addObj(x0 + 30, 0, 8);
}

bool beesShoot() { return zone >= 2; }  // Casino Night+

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
    if (kind == 5 || kind == 8) o.y = groundAt(x);
    if (kind == 8) { o.state = 0; o.timer = 20 + (h8((uint16_t)(x + 3)) & 31); }
    if (kind == 9) { o.y = y; o.timer = 0; }
    if (kind == 10) o.timer = 0;
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
void addShot(int16_t x, int16_t y, int8_t vx, int8_t vy) {
  for (auto &s : shots) {
    if (s.live) continue;
    s.live = true; s.x = x; s.y = y; s.vx = vx; s.vy = vy;
    return;
  }
}

void clearWorld() {
  for (auto &o : objs) o.live = false;
  for (auto &p : plats) p.live = false;
  for (auto &s : shots) s.live = false;
  for (auto &r : scat) r.live = false;
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
  static const int8_t XV[3] = { -5, 5, -3 };
  static const int8_t YV[3] = { -7, -7, -9 };
  for (uint8_t i = 0; i < 3; i++) {
    scat[i].live = true;
    scat[i].x = px + (int16_t)(i - 1) * 12;
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
    if (r.vy > 6) r.vy = 6;
    r.x += r.vx;
    r.y += r.vy;

    int16_t g = groundAt(r.x) - 3;
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
      if (r.vy > 0 && r.y >= p.y - 3 && r.y <= p.y + 4) {
        r.y = p.y - 3;
        r.vy = (int8_t)(-(r.vy * 2) / 3);
        if (r.vy > -2) r.vy = -2;
      }
    }

    // Grace period: spilled rings are not grab-able until they've flown out
    if (r.life > SCATTER_LIFE - SCATTER_GRACE) continue;
    if (abs(px - r.x) < 6 && abs(py - r.y) < 6) {
      r.live = false;
      rings++;
      score += 10;
      beep(1800, 22);
    }
  }
}

void cacheGroundStrip() {
  for (uint8_t i = 0; i < 128; i++) {
    int16_t g = sy(groundAt(cam + i));
    // Store screen-Y; 255 = off-screen (drawn only if 0..63)
    if (g < 0 || g > 63) gStrip[i] = 255;
    else gStrip[i] = (uint8_t)g;
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
    setPlayerVel(faceRight ? I2F(-2) : I2F(2), I2F(-5));
    onGround = false;
    rolling = true;
    groundRoll = false;
    inLoop = false;
    inHollow = false;
    return;
  }
  // No rings = death: lose a life and restart this act from the beginning
  if (lives) lives--;
  if (!lives) {
    mode = OVER;
    beep(120, 400);
    return;
  }
  beep(150, 350);
  // Rebuild after gameStep() finishes — hurt() is called mid-iteration over objs
  restartPending = true;
}

void placeGoal(int16_t x) { addObj(x, groundAt(x) - 20, 6); }

void announce() {
  intro = 60;   // ~2 s title card
  beep(600 + zone * 80 + act * 40, 70);
}

void startEggWrecker() {
  bossOn = true;
  bossHP = 8;
  bossCool = 0;
  bossPhase = 0;
  arenaOrigin = px + 20;
  setPlayerPos(arenaOrigin + 30, groundAt(arenaOrigin + 30) - 5);
  setPlayerVel(0, 0);
  inLoop = false;
  cam = arenaOrigin;
  camY = 0;
  clearWorld();
  // two floating platforms (world)
  addPlat(arenaOrigin + 28, 34, 26);
  addPlat(arenaOrigin + 78, 34, 26);
  eggX = arenaOrigin + 100;
  eggY = 16;
  eggDir = -1;
  ballAngle = 0;
  intro = 60;
  beep(280, 250);
}

void buildAct() {
  // Whole act is built once from its layout table and stays loaded
  clearWorld();
  for (uint8_t i = 0; i < lvl->nObj; i++) {
    const LvlObj &d = lvl->obj[i];
    int16_t x = d.x;
    int16_t g = groundAt(x);
    switch (d.kind) {
      case L_RING:   addObj(x, g - d.h, 1); break;
      case L_CRAB:   addObj(x, g - 5, 2); break;
      case L_SPRING: addObj(x, g - d.h - 4, 3); break;
      case L_BEE:    addObj(x, g - d.h, 4); break;
      case L_DECO:   addDeco(x, d.arg); break;
      case L_WALL:   addWall(x, d.arg); break;
      case L_SPIKE:  addObj(x, 0, 0); break;
      case L_FLAME:  addObj(x, 0, 8); break;
      case L_BUMPER: addObj(x, g - d.h, 9); break;
      case L_SAW:    addObj(x, g - d.h, 10); break;
      case L_PLAT:   addPlat(x, g - d.h, d.arg); break;
      case L_RINGS:
        for (uint8_t k = 0; k < d.arg; k++) {
          int16_t rx = x + k * 12;
          addObj(rx, groundAt(rx) - d.h, 1);
        }
        break;
      case L_ARC:
        for (uint8_t k = 0; k < d.arg; k++) {
          int16_t rx = x + k * 12;
          // parabola: 8 px at the ends, h in the middle
          int16_t m = d.arg - 1;
          int16_t u = 2 * k - m;                       // -m..m
          int16_t lift = m ? 8 + ((int32_t)(d.h - 8) * (m * m - u * u)) / (m * m) : d.h;
          addObj(rx, groundAt(rx) - lift, 1);
        }
        break;
      case L_SPIKES:
        for (uint8_t k = 0; k < d.arg; k++) addObj(x + k * 12, 0, 0);
        break;
      case L_LOOP:   placeGreenHillLoop(x); break;
      case L_CHEMX:  placeChemWavyCross(x); break;
    }
  }
  clearLoopRunway();                 // no hazards right before / after a loop
  if (act == 0) placeGoal(actEnd);   // Act 2 ends in the boss arena instead
}

void startAct(uint8_t z, uint8_t a) {
  zone = z; act = a;
  bossOn = false;
  lvl = &ACTS[z][a];
  actEnd = lvl->len;
  rings = 0;                 // rings are per act (and lost on a restart), as in Sonic
  actFrames = 0;
  goalT = 0;
  cam = 0; camY = 0;
  setPlayerPos(24, groundAt(24) - 5);
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

void advanceAfterClear() {
  // Chance for special stage (~28%, better with many rings)
  uint8_t chance = 22 + (rings > 20 ? 10 : 0) + (rings > 40 ? 10 : 0);
  if (emeralds < 7 && random(100) < chance) {
    // enter special
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
    return;
  }
  // next act / zone
  if (act == 0) startAct(zone, 1);
  else if (zone < 3) startAct(zone + 1, 0);
  else { mode = WIN; beep(1000, 500); }
}

void beginClear() {
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
  watch::playTune(ACT_CLEAR_TUNE, sizeof(ACT_CLEAR_TUNE) / sizeof(ACT_CLEAR_TUNE[0]));
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

  syncPlayerInts();
  // Safety: never leave Sonic buried under terrain
  g = groundAt(px);
  if (py + rad > g) {
    fpy = I2F(g - rad);
    fvy = 0;
    landed = true;
    syncPlayerInts();
  }

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

// Push Sonic out of a solid AABB and kill inward speed (side bump)
void bumpFromSolid(int16_t left, int16_t right, int16_t top, int16_t bot, int16_t rad) {
  if (py + rad <= top || py - rad >= bot) return;
  if (px + rad <= left || px - rad >= right) return;
  int16_t mid = (left + right) / 2;
  if (px < mid) {
    fpx = I2F(left - rad);
    if (fvx > 0) fvx = I2F(-2);
  } else {
    fpx = I2F(right + rad);
    if (fvx < 0) fvx = I2F(2);
  }
  faceRight = fvx >= 0;
  if (groundRoll) stopRoll();
  syncPlayerInts();
}

void applyWalls(int16_t rad) {
  syncPlayerInts();
  for (auto &o : objs) {
    if (!o.live || o.kind != 7) continue;
    int16_t h = o.state ? o.state : 14;
    bumpFromSolid(o.x, o.x + 6, o.y - h, o.y, rad);
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
      if (fvx < I2F(3)) fvx = I2F(3);
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
      if (fvx > I2F(-3)) fvx = I2F(-3);
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
void eggStep(int16_t rad) {
  eggX += eggDir;
  if (eggX < arenaOrigin + 30) eggDir = 1;
  if (eggX > arenaOrigin + 110) eggDir = -1;
  eggY = 14 + ((animFrame >> 4) & 1);

  int16_t speed = 4 + zone;
  ballAngle += speed;
  if (ballAngle >= 360) ballAngle -= 360;
  int16_t a = ballAngle;
  int16_t swing = (a < 180) ? (-40 + (a * 80) / 180) : (40 - ((a - 180) * 80) / 180);
  ballWX = eggX + swing;
  ballWY = eggY + 22 + abs(swing) / 8;

  if (bossCool) bossCool--;

  // Touch Egg Mobile (flying craft): always bounce off; damage if spinning into it
  if (abs(px - eggX) < 12 && abs(py - eggY) < 11) {
    bool attack = rolling && bossCool == 0;
    // Bounce AWAY from the craft: above → up (−y), below → down (+y)
    setPlayerVel((px < eggX) ? I2F(-4) : I2F(4),
                 (py <= eggY + 2) ? I2F(-5) : I2F(5));
    onGround = false;
    rolling = true;
    faceRight = fvx > 0;
    if (attack) {
      bossHP--;
      bossCool = 28;
      score += 100;
      beep(1500, 100);
      if (!bossHP) {
        bossOn = false;
        beginClear();
        return;
      }
    } else if (bossCool == 0 && !invul) {
      hurt();
    }
  }

  // Wrecking ball — safe under / on platforms
  bool underPlat = false;
  for (auto &p : plats) {
    if (!p.live) continue;
    if (px + rad < p.x || px - rad > p.x + p.w) continue;
    if (py + rad >= p.y - 2 && py < p.y + 8) underPlat = true;
  }
  if (!underPlat && !invul && abs(px - ballWX) < 7 && abs(py - ballWY) < 7) hurt();
}

void applyGroundControl() {
  bool pinball = (zone == 2 && (rolling || abs(fvx) >= I2F(2)));
  // Rolling keeps whatever speed you had (Genesis caps rolls at 16, i.e. never in practice)
  fx top = pinball ? I2F(5) : TOP_SPD;
  int16_t slope = onGround ? slopeAt(px) : 0;

  if (groundRoll && onGround) {
    // Holding forward adds no roll speed; slope does (2.5×)
    fvx += (fx)slope * SLOPE_ROLL;
    if (fvx > 0) { fvx -= ROLL_FRIC; if (fvx < 0) fvx = 0; }
    else if (fvx < 0) { fvx += ROLL_FRIC; if (fvx > 0) fvx = 0; }
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
  float R = (float)loopTrack.r - (float)rad - 3.0f;   // feet against the inner face
  return (R < 12.0f) ? 12.0f : R;
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

// Free fall inside the loop; the inside face of the track is solid all the way round
void stepHollow(int16_t rad) {
  if (heldIn() & B_LEFT)  { fvx -= AIR_ACCEL; faceRight = false; }
  if (heldIn() & B_RIGHT) { fvx += AIR_ACCEL; faceRight = true; }
  fvy += GRAVITY;
  if (fvy > MAX_FALL) fvy = MAX_FALL;
  fpx += fvx;
  fpy += fvy;
  syncPlayerInts();

  float R = loopTrackR(rad);
  float dx = (float)fpx / FX_ONE - loopTrack.cx;
  float dy = (float)fpy / FX_ONE - loopTrack.cy;
  if (dx * dx + dy * dy < R * R) return;

  // Hit the track: keep only the along-track part of his velocity
  float th = atan2f(dy, dx);
  if (th < 0) th += (float)TWO_PI;
  float tx, ty;
  loopTangent(th, &tx, &ty);
  fx gspd = (fx)((float)fvx * tx + (float)fvy * ty);
  // Progress measured the short way from the bottom — a fall never counts toward a lap
  float rel = th - (float)HALF_PI;
  while (rel > (float)PI) rel -= (float)TWO_PI;
  while (rel <= -(float)PI) rel += (float)TWO_PI;
  loopAttach(th, gspd, loopSense, loopSense * rel, rad);
  // Landed near the bottom: that's just the floor — ordinary running/rolling takes over
  if (fabsf(rel) < 0.35f) loopToGround(rad);
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
  bool nearFloor = py + rad >= loopFloorY() - 6;
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
  int32_t rIn = (int32_t)(r - 7) * (r - 7);
  int32_t rOut = (int32_t)(r + 6) * (r + 6);
  if (idy < r - 14 && id2 >= rIn && id2 <= rOut) {
    float len = sqrtf((float)id2);
    if (len >= 1.0f) {
      float scale = (float)(r + 7) / len;
      fpx = I2F(cx + (int16_t)(idx * scale));
      fpy = I2F(cy + (int16_t)(idy * scale));
      fvx = (fx)(-fvx / 4);
      if (fvy < 0) fvy = 0;
      syncPlayerInts();
    }
  }
}

void physicsStep(uint8_t e) {
  int16_t rad = rolling ? 4 : 5;

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
  // Allow climbing above y=0 for tall loops; only clamp extreme escape
  if (py < -90) {
    fpy = I2F(-90);
    if (fvy < 0) fvy = 0;
    syncPlayerInts();
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
    startAct(zone, act);   // rebuild stage; lives/score/emeralds kept
    invul = 45;
  }
}

void gameStepInner(uint8_t e) {
  if (e & B_GAME) { menuSel = 0; mode = MENU; return; }
  if (e & B_MODE) { watch::exitToClock(); return; }

  if (goalT) e = 0;                              // signpost spinning: no control
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
  int16_t rad = rolling ? 4 : 5;

  if (bossOn) {
    cam = arenaOrigin;
    camY = 0;
  } else {
    int16_t psx = px - cam;
    if (psx < 32) cam = px - 32;
    if (psx > 96) cam = px - 96;
    if (cam < 0) cam = 0;
    // Vertical follow — keeps Sonic framed through tall loops
    int16_t psy = py - camY;
    if (psy < 18) camY = py - 18;
    if (psy > 46) camY = py - 46;
  }

  if (!bossOn && !goalT && px >= actEnd - 8) {
    if (act == 0) {
      goalT = 1;                 // passed the signpost: timer stops, sign starts spinning
      groundRoll = false;        // stand up so normal friction brings him to a stop
      beep(1500, 60);
    } else {
      startEggWrecker();
      return;
    }
  }
  if (goalT) {
    goalT++;
    if (goalT < 50 && (goalT % 3) == 0) beep(900 + (goalT & 4) * 60, 15);   // spin clicks
    if (goalT >= GOAL_SPIN_FRAMES) { beginClear(); return; }
  }

  // Update only nearby objects (level stays fully loaded)
  for (auto &o : objs) {
    if (!o.live) continue;
    if (!onScreen(o.x, 40)) continue;

    if (o.kind == 5 || o.kind == 8) o.y = groundAt(o.x);

    if (o.kind == 2) { // crab leashed to home
      o.x += o.vx;
      if (o.x < o.home - 18 || o.x > o.home + 18) o.vx = -o.vx;
      o.y = groundAt(o.x) - 5;
    }
    if (o.kind == 4) { // bee patrol near home
      if (o.state == 0) {
        o.x += o.vx;
        if (o.x < o.home - 30 || o.x > o.home + 30) o.vx = -o.vx;
        if (o.timer) o.timer--; else { o.state = 1; o.timer = 26; }
      } else if (o.state == 1) {
        if (o.timer) o.timer--;
        else {
          if (beesShoot()) {
            int8_t dir = (px < o.x) ? -1 : 1;
            addShot(o.x + dir * 3, o.y + 3, dir * 2, 2);
            beep(480, 16);
          }
          o.state = 2; o.timer = 10;
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
  }

  for (auto &s : shots) {
    if (!s.live || s.vy == 99) continue; // skip ball marker
    s.x += s.vx; s.y += s.vy;
    if (s.x < cam - 8 || s.x > cam + 140 || s.y > 70) { s.live = false; continue; }
    if (!invul && abs(s.x - px) < 5 && abs(s.y - py) < 6) { s.live = false; hurt(); }
  }

  updateScatter();

  if (bossOn) { eggStep(rad); return; }

  // collisions — nearby only
  for (auto &o : objs) {
    if (restartPending || mode != GAME) break;  // died this frame — stop colliding
    if (!o.live) continue;
    if (!onScreen(o.x, 20)) continue;
    if (o.kind == 1) {
      if (nearPlayerSwept(o.x, o.y, 6, 7)) {
        o.live = false; rings++; score += 10; beep(1800, 22);
      }
    } else if (o.kind == 0) {
      // Spikes: solid from the sides; only the tips hurt when landing on them
      int16_t gy = groundAt(o.x);
      const int16_t spikeH = 14;
      const int16_t spikeW = 9;
      int16_t tipY = gy - spikeH;
      int16_t feet = py + rad;
      bool hitX = (px + rad - 1 > o.x) && (px - rad + 1 < o.x + spikeW);
      bool onTips = hitX && (feet >= tipY - 1) && (feet <= tipY + 4) && (py <= tipY + rad);
      if (onTips && fvy >= 0) {
        hurt();
      } else {
        bumpFromSolid(o.x, o.x + spikeW, tipY + 2, gy, rad);
      }
    } else if (o.kind == 2) {
      // Crab: spin/stomp destroys; otherwise solid bump
      if (abs(px - o.x) < 8 && abs(py - o.y) < 7) {
        if (canBreakEnemy(o.y)) {
          o.live = false; score += 50; beep(1400, 50);
          if (fvy > 0) { fvy = I2F(-4); rolling = true; syncPlayerInts(); }
        } else {
          bumpFromSolid(o.x - 5, o.x + 5, o.y - 5, o.y + 4, rad);
        }
      }
    } else if (o.kind == 3) {
      if (abs(px - o.x) < 7 && py + rad >= o.y - 2 && py + rad <= o.y + 7 && fvy >= 0) {
        fvy = -(JUMP_SPD + JUMP_SPD / 2); onGround = false; rolling = true; jumpCut = false;
        syncPlayerInts(); beep(1100, 65);
      }
    } else if (o.kind == 4) {
      // Bee: spin/stomp destroys; otherwise solid bump
      if (abs(px - o.x) < 7 && abs(py - o.y) < 6) {
        if (canBreakEnemy(o.y)) {
          o.live = false; score += 75; beep(1400, 50);
          if (fvy > 0) { fvy = I2F(-4); syncPlayerInts(); }
        } else {
          bumpFromSolid(o.x - 4, o.x + 5, o.y - 4, o.y + 4, rad);
        }
      }
    } else if (o.kind == 8) {
      // flame / chem jet — only hurts while firing
      if (o.state && abs(px - o.x) < 7 && py + rad > o.y - 14 && py < o.y + 2) hurt();
    } else if (o.kind == 9) {
      // pinball bumper (cooldown avoids sticky re-hits / lag spikes)
      if (o.timer == 0 && abs(px - o.x) < 9 && abs(py - o.y) < 9) {
        int8_t dx = (px >= o.x) ? 1 : -1;
        int8_t dy = (py >= o.y) ? 1 : -1;
        setPlayerVel(I2F(dx * 4), I2F((dy > 0) ? 3 : -6));
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
      if (abs(px - o.x) < 8 && abs(py - o.y) < 8) {
        if (canBreakEnemy(o.y)) {
          o.live = false; score += 80; beep(1400, 50);
          if (fvy > 0) { fvy = I2F(-4); syncPlayerInts(); }
        } else {
          bumpFromSolid(o.x - 5, o.x + 5, o.y - 5, o.y + 5, rad);
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
    if (act == 0) startAct(zone, 1);
    else if (zone < 3) startAct(zone + 1, 0);
    else mode = WIN;
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
    else {
      if (act == 0) startAct(zone, 1);
      else if (zone < 3) startAct(zone + 1, 0);
      else mode = WIN;
    }
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
// Blit a 16x20 XBM frame; mirror when facing left
void blitSonic(int16_t left, int16_t top, uint8_t frameId, bool faceRight) {
  const uint8_t *src = sonicFrames[frameId];
  const uint8_t rowBytes = (SONIC_W + 7) / 8;
  u8g2.setDrawColor(1);
  for (uint8_t row = 0; row < SONIC_H; row++) {
    int16_t py = top + row;
    if (py < 0 || py >= 64) continue;
    for (uint8_t col = 0; col < SONIC_W; col++) {
      uint8_t byte = pgm_read_byte(&src[row * rowBytes + (col >> 3)]);
      if (!(byte & (1 << (col & 7)))) continue;
      int16_t px = left + (faceRight ? col : (SONIC_W - 1 - col));
      if (px < 0 || px >= 128) continue;
      plot(px, py);
    }
  }
}

void drawHogAt(int16_t x, int16_t y, bool roll, bool faceRight, uint8_t frame, bool moving) {
  uint8_t id;
  if (roll) id = SPR_ROLL;
  else if (moving) id = SPR_RUN0 + ((frame >> 1) & 3);
  else id = ((frame >> 3) & 1) ? SPR_STAND1 : SPR_STAND0;
  int16_t top = y - (int16_t)SONIC_H + 6;
  int16_t left = x - (int16_t)SONIC_W / 2;
  blitSonic(left, top, id, faceRight);
}

void drawHog(int16_t x, int16_t y, bool roll, bool faceRight, uint8_t frame) {
  drawHogAt(x, y, roll || groundRoll, faceRight, frame, fvx != 0 || inLoop);
}

void drawLoopTrack() {
  if (!loopTrack.live || !onScreen(loopTrack.cx, loopTrack.r + 8)) return;
  int16_t x = sx(loopTrack.cx);
  int16_t y = sy(loopTrack.cy);
  int16_t r = loopTrack.r;
  // Thick track like GH checker loop (outer + mid + inner)
  circleSafe(x, y, r);
  circleSafe(x, y, r - 1);
  circleSafe(x, y, r - 3);
  circleSafe(x, y, r - 5);
  // Mouth openings (clear bottom-left / bottom-right notches)
  int16_t floorY = sy(loopTrack.cy + r);
  u8g2.setDrawColor(0);
  fillBoxClipped(x - r - 1, floorY - 3, 10, 8);
  fillBoxClipped(x + r - 8, floorY - 3, 10, 8);
  u8g2.setDrawColor(1);
  // Checker ticks around the ring
  for (uint8_t i = 0; i < 16; i++) {
    float th = i * (6.2831853f / 16.0f);
    int16_t tx = x + (int16_t)((r - 2) * cosf(th));
    int16_t ty = y + (int16_t)((r - 2) * sinf(th));
    if ((i & 1) == 0) plot(tx, ty);
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
  // Real ring: outer + inner circle (old r=3 is now the hole edge)
  circleSafe(x, y, 5);
  circleSafe(x, y, 3);
}

// Classic Sonic spikes: 3 vertical spears ~Sonic-tall, 3px bases, group ~his width
void drawSpikes(int16_t x, int16_t baseY) {
  const int16_t h = 14;
  for (uint8_t i = 0; i < 3; i++) {
    int16_t bx = x + (int16_t)i * 3;
    fillTriSafe(bx, baseY, bx + 3, baseY, bx + 1, baseY - h);
    // crisp outline so they read on the OLED
    triSafe(bx, baseY, bx + 3, baseY, bx + 1, baseY - h);
  }
}

void drawPalm(int16_t x, int16_t baseY) {
  vLineSafe(x, baseY - 20, 20);
  lineSafe(x, baseY - 20, x - 6, baseY - 14);
  lineSafe(x, baseY - 20, x + 6, baseY - 14);
  lineSafe(x, baseY - 21, x - 4, baseY - 26);
  lineSafe(x, baseY - 21, x + 4, baseY - 26);
  plot(x - 1, baseY - 20);
  plot(x + 1, baseY - 20);
}

void drawPipe(int16_t x, int16_t baseY) {
  frameSafe(x - 4, baseY - 18, 10, 18);
  hLineSafe(x - 6, baseY - 18, 14);
  hLineSafe(x - 6, baseY - 10, 14);
  fillBoxClipped(x - 2, baseY - 4, 6, 4);
}

void drawNeon(int16_t x, int16_t baseY) {
  frameSafe(x - 6, baseY - 20, 14, 10);
  if ((animFrame >> 2) & 1) {
    plot(x - 3, baseY - 16);
    plot(x, baseY - 15);
    plot(x + 3, baseY - 16);
  }
  vLineSafe(x, baseY - 10, 10);
}

void drawGear(int16_t x, int16_t baseY) {
  int16_t cy = baseY - 10;
  circleSafe(x, cy, 5);
  discSafe(x, cy, 2);
  uint8_t rot = (animFrame >> 3) & 3;
  static const int8_t DX[4] = {6, 0, -6, 0};
  static const int8_t DY[4] = {0, 6, 0, -6};
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t a = (rot + i) & 3;
    plot(x + DX[a], cy + DY[a]);
  }
}

void drawFlame(int16_t x, int16_t baseY, bool on) {
  fillBoxClipped(x - 3, baseY - 3, 7, 3);
  if (on) {
    triSafe(x, baseY - 14, x - 4, baseY - 3, x + 4, baseY - 3);
    if (animFrame & 2) plot(x, baseY - 16);
  }
}

void drawBumper(int16_t x, int16_t y, bool flash) {
  discSafe(x, y, flash ? 7 : 6);
  u8g2.setDrawColor(0);
  discSafe(x, y, 3);
  u8g2.setDrawColor(1);
  circleSafe(x, y, 3);
}

void drawSaw(int16_t x, int16_t y, uint8_t spin) {
  circleSafe(x, y, 5);
  discSafe(x, y, 2);
  uint8_t a = (spin >> 1) & 3;
  static const int8_t DX[4] = {6, 0, -6, 0};
  static const int8_t DY[4] = {0, 6, 0, -6};
  lineSafe(x - DX[a], y - DY[a], x + DX[a], y + DY[a]);
  lineSafe(x - DY[a], y + DX[a], x + DY[a], y - DX[a]);
}

void drawDeco(int16_t x, int16_t baseY, uint8_t typ) {
  if (typ == 0) drawSunflower(x, baseY);
  else if (typ == 1) drawPalm(x, baseY);
  else if (typ == 2) drawPipe(x, baseY);
  else if (typ == 3) drawNeon(x, baseY);
  else drawGear(x, baseY);
}

void drawCrab(int16_t x, int16_t y, uint8_t frame) {
  int8_t claw = (frame & 4) ? 1 : 0;
  fillBoxClipped(x - 5, y - 2, 10, 5);
  vLineSafe(x - 2, y - 5, 3); vLineSafe(x + 2, y - 5, 3);
  discSafe(x - 2, y - 5, 1); discSafe(x + 2, y - 5, 1);
  triSafe(x - 5, y, x - 9 - claw, y - 3, x - 9 - claw, y + 2);
  triSafe(x + 5, y, x + 9 + claw, y - 3, x + 9 + claw, y + 2);
}

void drawBee(int16_t x, int16_t y, uint8_t frame, uint8_t state) {
  discSafe(x, y, 3); fillBoxClipped(x - 1, y - 2, 5, 4);
  u8g2.setDrawColor(0); vLineSafe(x + 1, y - 2, 4); u8g2.setDrawColor(1);
  discSafe(x - 3, y, 2);
  u8g2.setDrawColor(0); plot(x - 4, y - 1); u8g2.setDrawColor(1);
  triSafe(x + 4, y, x + 7, y - 1, x + 7, y + 1);
  hLineSafe(x - 1, y - ((frame & 1) ? 5 : 4), 4);
  if (state == 1) plot(x - 5, y + 2);
}

void drawEggWrecker() {
  int16_t ex = sx(eggX);
  discSafe(ex, eggY + 4, 6);
  fillBoxClipped(ex - 5, eggY + 8, 10, 3);
  u8g2.setDrawColor(0);
  for (int16_t i = -4; i <= 4; i += 3) vLineSafe(ex + i, eggY + 8, 3);
  u8g2.setDrawColor(1);
  discSafe(ex, eggY, 3);
  u8g2.setDrawColor(0);
  plot(ex - 1, eggY - 1);
  plot(ex + 1, eggY - 1);
  u8g2.setDrawColor(1);
  hLineSafe(ex - 2, eggY + 2, 5);

  int16_t bx = sx(ballWX), by = ballWY;
  lineSafe(ex, eggY + 8, bx, by);
  discSafe(bx, by, 4);
  u8g2.setDrawColor(0);
  plot(bx - 1, by - 1);
  u8g2.setDrawColor(1);

  frameSafe(2, 56, 50, 6);
  fillBoxClipped(4, 58, bossHP * 5, 2);
}

void drawBanner() {
  // Compact title card: zone name over "ACT n" / boss name
  u8g2.setFont(u8g2_font_5x7_tr);
  const char *top = (const char *)zoneName();
  const char *bot = bossOn ? "EGG WRECKER" : (act ? "ACT 2" : "ACT 1");
  int16_t w = u8g2.getStrWidth(top);
  int16_t w2 = u8g2.getStrWidth(bot);
  if (w2 > w) w = w2;
  int16_t bw = w + 10, bx = (128 - bw) / 2;
  u8g2.setDrawColor(0); u8g2.drawBox(bx, 16, bw, 20);
  u8g2.setDrawColor(1); u8g2.drawFrame(bx, 16, bw, 20);
  u8g2.setCursor((128 - u8g2.getStrWidth(top)) / 2, 25); u8g2.print(top);
  u8g2.setCursor((128 - u8g2.getStrWidth(bot)) / 2, 33); u8g2.print(bot);
  u8g2.setFont(u8g2_font_6x10_tr);
}

void drawGame() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setDrawColor(1);

  // Terrain in camera space (vertical scroll for tall loops)
  cacheGroundStrip();
  for (uint8_t i = 0; i < 128; i++) {
    if (gStrip[i] > 63) continue;
    u8g2.drawPixel(i, gStrip[i]);
    if ((i & 15) == 0 && gStrip[i] + 2 < 64) u8g2.drawPixel(i, gStrip[i] + 2);
  }

  drawLoopTrack();

  for (auto &p : plats) {
    if (!p.live) continue;
    if (p.w <= 0 || p.w > 48) continue;
    int16_t y = sy(p.y);
    if (y < -4 || y > 66) continue;
    fillBoxClipped(sx(p.x), y, p.w, 3);
  }

  for (auto &o : objs) {
    if (!o.live) continue;
    int16_t x = sx(o.x);
    int16_t y = sy(o.y);
    if (x < -14 || x > 142) continue;
    if (y < -24 || y > 80) continue;
    u8g2.setDrawColor(1);
    if (o.kind == 0) {
      drawSpikes(x, sy(groundAt(o.x)));
    } else if (o.kind == 1) {
      drawRingGlyph(x, y);
    } else if (o.kind == 2) drawCrab(x, y, animFrame);
    else if (o.kind == 3) {
      fillBoxClipped(x - 4, y - 2, 8, 3);
      hLineSafe(x - 3, y - 4, 6);
    } else if (o.kind == 4) drawBee(x, y, animFrame, o.state);
    else if (o.kind == 5) drawDeco(x, y, o.state);
    else if (o.kind == 6) {
      int16_t gh = groundAt(o.x) - o.y;
      if (gh > 0 && gh < 50) vLineSafe(x, y + 6, gh - 6);
      if (goalT && goalT < 55) {
        // spinning: panel width follows |cos|, alternating faces
        float c = cosf(goalT * 0.45f);
        int16_t w = (int16_t)(fabsf(c) * 6.0f) + 1;
        if (c > 0) fillBoxClipped(x - w, y - 2, w * 2, 10);
        else frameSafe(x - w, y - 2, w * 2, 10);
      } else if (goalT) {
        frameSafe(x - 6, y - 2, 12, 10);       // settled: ring emblem
        circleSafe(x, y + 3, 3);
      } else {
        fillBoxClipped(x - 6, y - 2, 12, 10);  // not yet passed
      }
    } else if (o.kind == 7) {
      int16_t h = o.state ? o.state : 14;
      fillBoxClipped(x, y - h, 6, h);
    } else if (o.kind == 8) drawFlame(x, y, o.state);
    else if (o.kind == 9) drawBumper(x, y, o.timer > 0);
    else if (o.kind == 10) drawSaw(x, y, o.timer);
  }

  for (auto &s : shots) {
    if (!s.live || s.vy == 99) continue;
    discSafe(sx(s.x), sy(s.y), 1);
  }

  for (auto &r : scat) {
    if (!r.live) continue;
    if (r.life < 30 && ((r.life >> 1) & 1)) continue;
    drawRingGlyph(sx(r.x), sy(r.y));
  }

  if (bossOn) drawEggWrecker();

  bool blink = invul && ((invul >> 2) & 1);
  if (!blink) drawHog(sx(px), sy(py), rolling, faceRight, animFrame);

  u8g2.setCursor(0, 9); u8g2.print(F("R")); u8g2.print(rings);
  u8g2.setCursor(32, 9); printTime(actSeconds());
  for (uint8_t i = 0; i < emeralds && i < 7; i++) plot(70 + i * 4, 3);
  for (uint8_t i = 0; i < lives; i++) discSafe(122 - i * 9, 5, 3);
  if (intro) drawBanner();
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

void drawClear() {
  // Zone + "ACT n CLEAR!" header
  u8g2.setFont(u8g2_font_5x7_tr);
  const char *zn = (const char *)zoneName();
  u8g2.setCursor((128 - u8g2.getStrWidth(zn)) / 2, 7); u8g2.print(zn);
  u8g2.setFont(u8g2_font_6x10_tr);
  const char *hd = act ? "ACT 2 CLEAR!" : "ACT 1 CLEAR!";
  u8g2.setCursor((128 - u8g2.getStrWidth(hd)) / 2, 18); u8g2.print(hd);
  hLineSafe(0, 21, 128);

  u8g2.setCursor(2, 32); u8g2.print(F("TIME  ")); printTime(actSeconds());
  u8g2.setCursor(2, 43); u8g2.print(F("RINGS ")); u8g2.print(rings);
  u8g2.setCursor(2, 54); u8g2.print(F("BONUS ")); u8g2.print(tallyRingBonus + tallyTimeBonus);
  u8g2.setCursor(2, 64); u8g2.print(F("SCORE ")); u8g2.print(score);

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
    else if (menuSel == 1) startGame();
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

// ===================== WatchCore hooks =====================
void gameStart() {            // Game pressed on the clock face
  splashFrame = 0;
  mode = SPLASH;
}

void gameUpdate(uint8_t e) {
  switch (mode) {
    case SPLASH:
      if (splashFrame < 250) splashFrame++;
      if (e & B_MID) startGame();
      else if (e & (B_MODE | B_GAME | B_LEFT | B_RIGHT)) watch::exitToClock();
      break;
    case GAME: gameStep(e); break;
    case SPECIAL: specialStep(e); break;
    case CLEAR: clearStep(e); break;
    case MENU: menuLoop(e); break;
    case OVER:
      if (e & B_MID) startGame();
      else if (e & (B_GAME | B_MODE)) watch::exitToClock();  // Mode always escapes to clock
      break;
    case WIN: if (e) watch::exitToClock(); break;
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
  }
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
