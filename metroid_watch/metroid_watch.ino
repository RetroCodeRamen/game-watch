// Metroid Watch — ENGINE SANDBOX (step back from full game)
// One large room. Horizontal + vertical camera scroll.
// Solid floors + jump-through platforms. Stable Samus movement.
// Full campaign comes later once this feels right.
//
// Logical buttons: Left Right Up Down Menu
// Current 5-button board: Mid→Up, Mode→Down, Game→Menu

#include <Arduino.h>
#include <Wire.h>
#define U8G2_16BIT
#include <U8g2lib.h>
#include "metroid_sprites.h"

// Before Arduino auto-prototypes
typedef int32_t fx;
struct Plat {
  int16_t x, y, w;
  bool solid;  // true = solid slab; false = jump-through from below
};

// -------------------- pins (change here for new hardware) --------------------
const uint8_t P_LEFT = 2, P_UP = 3, P_RIGHT = 6, P_DOWN = 7, P_MENU = 8;
const uint8_t P_BUZ = 15, P_SDA = 4, P_SCL = 5;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

enum : uint8_t { B_LEFT = 1, B_UP = 2, B_RIGHT = 4, B_DOWN = 8, B_MENU = 16 };
const uint8_t BPIN[5] = {P_LEFT, P_UP, P_RIGHT, P_DOWN, P_MENU};
uint8_t held = 0, lastRaw = 0, pending = 0;
unsigned long changedAt[5];

uint8_t readButtons() {
  unsigned long now = millis();
  uint8_t edges = 0;
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t bit = (uint8_t)(1 << i);
    bool raw = !digitalRead(BPIN[i]);
    if (raw != ((lastRaw & bit) != 0)) { lastRaw ^= bit; changedAt[i] = now; }
    if (now - changedAt[i] >= 20) {
      bool isHeld = (held & bit) != 0;
      if (raw != isHeld) {
        held ^= bit;
        if (raw) edges |= bit;
      }
    }
  }
  return edges;
}

void beep(uint16_t f, uint16_t ms) { tone(P_BUZ, f, ms); }

// -------------------- modes --------------------
enum : uint8_t { VIEW, SETM, RING, GAME, PAUSE };
uint8_t mode = VIEW;

unsigned long lastInputMs = 0;
uint8_t idleStage = 0;

void displayWake() {
  idleStage = 0;
  lastInputMs = millis();
  u8g2.setPowerSave(0);
}
void displayEnsureOn() {
  if (idleStage) displayWake();
  else u8g2.setPowerSave(0);
}
void applySoftwareDim() {
  u8g2.setDrawColor(0);
  for (uint8_t y = 0; y < 64; y++)
    for (uint8_t x = (y & 1); x < 128; x += 2)
      u8g2.drawPixel(x, y);
  u8g2.setDrawColor(1);
}
void updateClockIdle() {
  if (mode != VIEW) {
    if (idleStage) displayWake();
    return;
  }
  unsigned long idle = millis() - lastInputMs;
  if (idle >= 20000UL) {
    if (idleStage != 2) {
      idleStage = 2;
      u8g2.clearBuffer();
      u8g2.sendBuffer();
      u8g2.setPowerSave(1);
    }
  } else if (idle >= 10000UL) {
    if (idleStage != 1) { idleStage = 1; u8g2.setPowerSave(0); }
  } else if (idleStage) {
    displayWake();
  }
}

const uint8_t MDAYS[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
const char MONS[] = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
uint32_t tod = 43200;
uint8_t mon = 1, dday = 1;
uint32_t alarmTod = 25200;
bool alarmOn = false;
unsigned long lastSec = 0;

void tick() {
  if (++tod >= 86400UL) {
    tod = 0;
    if (++dday > MDAYS[mon - 1]) { dday = 1; if (++mon > 12) mon = 1; }
  }
  if (alarmOn && tod == alarmTod && (mode == VIEW || mode == GAME))
    mode = RING;
}

void two(uint8_t v) { if (v < 10) u8g2.print('0'); u8g2.print(v); }
void monthName(uint8_t m) {
  for (uint8_t i = 0; i < 3; i++) u8g2.print(MONS[(m - 1) * 3 + i]);
}

void plot(int16_t x, int16_t y) {
  if ((uint16_t)x < 128 && (uint16_t)y < 64) u8g2.drawPixel(x, y);
}
void hLineSafe(int16_t x, int16_t y, int16_t w) {
  if ((uint16_t)y >= 64) return;
  if (x < 0) { w += x; x = 0; }
  if (x + w > 128) w = 128 - x;
  if (w > 0) u8g2.drawHLine(x, y, w);
}
void vLineSafe(int16_t x, int16_t y, int16_t h) {
  if ((uint16_t)x >= 128) return;
  if (y < 0) { h += y; y = 0; }
  if (y + h > 64) h = 64 - y;
  if (h > 0) u8g2.drawVLine(x, y, h);
}
void fillBox(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 128) w = 128 - x;
  if (y + h > 64) h = 64 - y;
  if (w > 0 && h > 0) u8g2.drawBox(x, y, w, h);
}
void frameBox(int16_t x, int16_t y, int16_t w, int16_t h) {
  hLineSafe(x, y, w); hLineSafe(x, y + h - 1, w);
  vLineSafe(x, y, h); vLineSafe(x + w - 1, y, h);
}

// =====================================================================
// ENGINE SANDBOX — one room, 384×224 playfield (3×4 viewports of 128×56)
// =====================================================================
const int16_t WORLD_W = 384;
const int16_t WORLD_H = 224;
const uint8_t HUD_H = 8;
const uint8_t VIEW_H = 56;

// Layout: ground + ledges to climb + a few one-way shelves
static const Plat PLATS[] = {
  // Main floor stretches (solid)
  {0,   200, 384, true},
  // Left climb tower
  {16,  168, 48,  true},
  {40,  136, 48,  true},
  {16,  104, 48,  true},
  {40,   72, 48,  true},
  {16,   40, 64,  true},
  // Mid floating jump-through shelves (land on top, jump up through)
  {120, 160, 64,  false},
  {160, 120, 56,  false},
  {200,  80, 64,  false},
  {240,  48, 48,  false},
  // Right solid staircase down from high ledge
  {300, 168, 60,  true},
  {320, 136, 48,  true},
  {340, 104, 44,  true},
  {300,  72, 56,  true},
  // Ceiling chunks / overhangs (solid)
  {180,  24, 80,  true},
  {280,  24, 70,  true},
  // Small mid solid block to hop onto
  {200, 176, 32,  true},
};
static const uint8_t PLAT_N = sizeof(PLATS) / sizeof(PLATS[0]);

// -------------------- Super Metroid–scaled physics --------------------
// Reference: standing sprite height H, 60 FPS SNES constants → our H & FPS.
// H = 22 (Samus sprite). Loop ≈ 30 FPS. Fixed-point 1/256 px.
//
// Per-frame from V(H/s), A(H/s²):
//   v_fx = V * H / FPS * 256
//   a_fx = A * H / FPS² * 256

static const fx FX_ONE = 256;

static const uint8_t SM_H   = 22;   // standing sprite / hitbox height
static const uint8_t SM_FPS = 30;

// Hitbox (SM proportions)
static const int16_t BODY_H   = 22;                          // 1.000 H
static const int16_t BODY_HALF = 4;                          // ~0.381 H / 2 → 8px wide
static const int16_t SLAB_H   = 4;                           // solid platform thickness

// Vertical (screen Y+ down → jump velocity is negative)
// g=9.429 H/s², v_y0=6.500 H/s, v_fall=7.143 H/s
static const fx GRAV_FX      = (fx)(9.429f * SM_H / (SM_FPS * SM_FPS) * FX_ONE);  // ~59
static const fx JUMP_FX      = (fx)(6.500f * SM_H / SM_FPS * FX_ONE);             // ~1220
static const fx FALL_MAX_FX  = (fx)(7.143f * SM_H / SM_FPS * FX_ONE);             // ~1341
// Horizontal boost on launch: +0.083*|vx| in H/s → ≈ |vx_fx|*21/256 in fx
static const fx JUMP_XV_NUM  = 21;
static const fx JUMP_XV_DEN  = 256;

// Horizontal
// walk=2.143, run=4.286 H/s; a_g=14.286, d_g=17.143, a_air=6.429 H/s²
static const fx WALK_FX      = (fx)(2.143f * SM_H / SM_FPS * FX_ONE);             // ~402
static const fx RUN_FX       = (fx)(4.286f * SM_H / SM_FPS * FX_ONE);             // ~805
static const fx ACCEL_G_FX   = (fx)(14.286f * SM_H / (SM_FPS * SM_FPS) * FX_ONE); // ~89
static const fx DECEL_G_FX   = (fx)(17.143f * SM_H / (SM_FPS * SM_FPS) * FX_ONE); // ~107
static const fx ACCEL_A_FX   = (fx)(6.429f * SM_H / (SM_FPS * SM_FPS) * FX_ONE);  // ~40

// Hold L/R this many frames before run cap (no dash button on watch yet)
static const uint8_t RUN_HOLD_FRAMES = 12;  // 0.4 s ≈ ease into run

static inline int16_t F2I(fx v) {
  return (int16_t)((v >= 0) ? ((v + 128) >> 8) : -(((-v) + 128) >> 8));
}
static inline fx I2F(int16_t v) { return (fx)v * FX_ONE; }
static inline fx fxAbs(fx v) { return v >= 0 ? v : -v; }

// Player (fixed-point position / velocity; px/py are derived ints for collision/draw)
fx fpx, fpy, fvx, fvy;
int16_t px = 40, py = 180;
int16_t camX = 0, camY = 0;
bool faceRight = true;
bool onGround = false;
bool jumpHeld = false;       // Up currently held
bool jumpCutDone = false;    // already applied release cut this jump
uint8_t coyote = 0;
uint8_t jumpBuf = 0;
uint8_t dirHold = 0;         // frames holding move → unlock run
uint8_t invul = 0;
uint8_t runPhase = 0;
uint8_t animTimer = 0;
uint8_t pauseSel = 0;
uint8_t inputLock = 0;
unsigned long menuDownMs = 0;
bool menuHeld = false;

void syncPlayerInts() {
  px = F2I(fpx);
  py = F2I(fpy);
}

int16_t sx(int16_t wx) { return wx - camX; }
int16_t sy(int16_t wy) { return wy - camY + HUD_H; }

bool platOverlapsX(const Plat &p, int16_t x) {
  return x >= p.x && x < p.x + p.w;
}

// Land on highest platform top crossed this frame
int16_t findFloor(int16_t x, int16_t feetY, int16_t prevFeetY, fx vy, bool *outSolid) {
  int16_t best = 32767;
  bool bestSolid = true;
  int16_t hw = BODY_HALF;
  for (uint8_t i = 0; i < PLAT_N; i++) {
    const Plat &p = PLATS[i];
    if (!platOverlapsX(p, x) && !platOverlapsX(p, x - hw) && !platOverlapsX(p, x + hw))
      continue;
    int16_t top = p.y;
    if (p.solid) {
      // allow a couple px of forgiveness for subpixel landing
      if (feetY >= top && prevFeetY <= top + 3) {
        if (top < best) { best = top; bestSolid = true; }
      }
    } else {
      // jump-through: only when falling/resting and crossing from above
      if (vy >= 0 && prevFeetY <= top && feetY >= top) {
        if (top < best) { best = top; bestSolid = false; }
      }
    }
  }
  if (outSolid) *outSolid = bestSolid;
  return best;
}

bool solidBlockAt(int16_t x, int16_t y) {
  for (uint8_t i = 0; i < PLAT_N; i++) {
    const Plat &p = PLATS[i];
    if (!p.solid) continue;
    if (x >= p.x && x < p.x + p.w && y >= p.y && y < p.y + SLAB_H)
      return true;
  }
  return false;
}

void updateCamera() {
  // Look ahead in facing direction; follow height without jump snap
  int16_t wantX = px - (faceRight ? 48 : 72);
  int16_t wantY = py - 36;
  int16_t maxX = WORLD_W - 128;
  int16_t maxY = WORLD_H - VIEW_H;
  if (maxX < 0) maxX = 0;
  if (maxY < 0) maxY = 0;
  if (wantX < 0) wantX = 0;
  if (wantY < 0) wantY = 0;
  if (wantX > maxX) wantX = maxX;
  if (wantY > maxY) wantY = maxY;

  // Soft follow (2px/frame) — stable, no jitter
  if (camX < wantX - 1) camX += 2;
  else if (camX > wantX + 1) camX -= 2;
  else camX = wantX;
  if (camY < wantY - 1) camY += 2;
  else if (camY > wantY + 1) camY -= 2;
  else camY = wantY;

  if (camX < 0) camX = 0;
  if (camY < 0) camY = 0;
  if (camX > maxX) camX = maxX;
  if (camY > maxY) camY = maxY;
}

void startSandbox() {
  fpx = I2F(48);
  fpy = I2F(180);
  fvx = fvy = 0;
  syncPlayerInts();
  camX = 0;
  camY = WORLD_H - VIEW_H;
  faceRight = true;
  onGround = true;
  jumpHeld = false;
  jumpCutDone = false;
  coyote = 0;
  jumpBuf = 0;
  dirHold = 0;
  runPhase = 0;
  animTimer = 0;
  inputLock = 8;
  menuHeld = false;
  menuDownMs = 0;
  mode = GAME;
  beep(700, 30);
}

void blitBits(int16_t left, int16_t top, const uint8_t *bits, uint8_t rows, uint8_t rowBytes, bool flip) {
  u8g2.setDrawColor(1);
  for (uint8_t row = 0; row < rows; row++) {
    for (uint8_t bi = 0; bi < rowBytes; bi++) {
      uint8_t b = bits[row * rowBytes + bi];
      for (uint8_t col = 0; col < 8; col++) {
        if (!(b & (0x80 >> col))) continue;
        int16_t dx = bi * 8 + col;
        if (flip) dx = (rowBytes * 8 - 1) - dx;
        plot(left + dx, top + row);
      }
    }
  }
}

void drawSamus() {
  int16_t x = sx(px);
  int16_t y = sy(py);
  if (invul && ((invul >> 2) & 1)) return;

  const uint8_t *spr = SAMUS_IDLE;
  if (!onGround) {
    spr = SAMUS_JUMP;
  } else if (fxAbs(fvx) > FX_ONE / 4) {
    // Pose rate scales with speed: walk slower anim, run faster
    spr = (runPhase & 1) ? SAMUS_RUN1 : SAMUS_RUN0;
  }
  blitBits(x - 8, y - (SAMUS_H - 1), spr, SAMUS_H, 2, !faceRight);
}

void drawWorld() {
  u8g2.setDrawColor(1);

  // Sparse stars / dust (stable, based on camera)
  for (uint8_t i = 0; i < 20; i++) {
    int16_t wx = (int16_t)((i * 97 + 13) % WORLD_W);
    int16_t wy = (int16_t)((i * 53 + 7) % WORLD_H);
    int16_t x = sx(wx), y = sy(wy);
    if (y >= HUD_H && y < 64) plot(x, y);
  }

  // Platforms
  for (uint8_t i = 0; i < PLAT_N; i++) {
    const Plat &p = PLATS[i];
    int16_t x = sx(p.x);
    int16_t y = sy(p.y);
    if (y < HUD_H - 4 || y > 66) continue;
    if (x > 128 || x + p.w < 0) continue;

    if (p.solid) {
      fillBox(x, y, p.w, 4);
      // rock edge ticks
      for (int16_t t = 0; t < p.w; t += 8)
        plot(x + t, y - 1);
    } else {
      // jump-through: dashed top edge
      for (int16_t t = 0; t < p.w; t += 4)
        hLineSafe(x + t, y, 2);
      hLineSafe(x, y + 1, p.w);
    }
  }

  // Room bounds hint
  if (camX <= 0) vLineSafe(sx(0), HUD_H, VIEW_H);
  if (camX >= WORLD_W - 128) vLineSafe(sx(WORLD_W - 1), HUD_H, VIEW_H);

  drawSamus();
}

void drawHud() {
  fillBox(0, 0, 128, HUD_H);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(2, 7);
  u8g2.print(F("ENGINE"));
  u8g2.setCursor(48, 7);
  u8g2.print(F("X"));
  u8g2.print(px);
  u8g2.print(F(" Y"));
  u8g2.print(py);
  u8g2.setDrawColor(1);
}

void drawGame() {
  drawWorld();
  drawHud();
}

void drawPause() {
  // Big readable pause — not the full Start tabs yet
  u8g2.setDrawColor(1);
  fillBox(14, 12, 100, 44);
  u8g2.setDrawColor(0);
  frameBox(14, 12, 100, 44);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(40, 24);
  u8g2.print(F("PAUSED"));
  u8g2.setCursor(28, 36);
  u8g2.print(pauseSel == 0 ? F("> RESUME") : F("  RESUME"));
  u8g2.setCursor(28, 48);
  u8g2.print(pauseSel == 1 ? F("> EXIT") : F("  EXIT"));
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(18, 58);
  u8g2.print(F("L/R  MENU=ok"));
}

void gamePhysics(uint8_t e) {
  if (inputLock) { inputLock--; return; }

  // Menu: tap or hold opens pause
  if (e & B_MENU) {
    menuHeld = true;
    menuDownMs = millis();
  }
  if (menuHeld && (held & B_MENU) && millis() - menuDownMs >= 350UL) {
    menuHeld = false;
    pauseSel = 0;
    mode = PAUSE;
    beep(400, 30);
    return;
  }
  if (menuHeld && !(held & B_MENU)) {
    menuHeld = false;
    pauseSel = 0;
    mode = PAUSE;
    beep(400, 20);
    return;
  }

  // ---- Horizontal accel (semi-implicit Euler) ----
  int8_t dir = 0;
  if (held & B_LEFT)  { dir = -1; faceRight = false; }
  if (held & B_RIGHT) { dir =  1; faceRight = true; }

  if (dir != 0) {
    if (dirHold < 255) dirHold++;
  } else {
    dirHold = 0;
  }

  // No dash button yet: walk cap first, then run after short hold
  fx speedCap = (dirHold >= RUN_HOLD_FRAMES) ? RUN_FX : WALK_FX;
  fx accel = onGround ? ACCEL_G_FX : ACCEL_A_FX;
  fx decel = DECEL_G_FX;

  if (dir != 0) {
    fvx += (fx)dir * accel;
    if (fvx > speedCap) fvx = speedCap;
    if (fvx < -speedCap) fvx = -speedCap;
    // Turning around on ground: stronger bite toward new direction
    if (onGround && ((dir > 0 && fvx < 0) || (dir < 0 && fvx > 0))) {
      fvx += (fx)dir * decel;
    }
  } else if (onGround) {
    // Ground friction; air has zero drag when neutral (SM)
    if (fvx > 0) {
      fvx -= decel;
      if (fvx < 0) fvx = 0;
    } else if (fvx < 0) {
      fvx += decel;
      if (fvx > 0) fvx = 0;
    }
  }

  // ---- Jump ----
  if (jumpBuf) jumpBuf--;
  if (e & B_UP) {
    jumpBuf = 8;       // ~130ms buffer
    jumpHeld = true;
  }
  if (!(held & B_UP)) jumpHeld = false;

  if (onGround) coyote = 6;     // ~100ms
  else if (coyote) coyote--;

  if (jumpBuf && coyote) {
    // v_y,launch = v_y0 + 0.083*|v_x|  (up = negative here)
    fx boost = fxAbs(fvx) * JUMP_XV_NUM / JUMP_XV_DEN;
    fvy = -(JUMP_FX + boost);
    onGround = false;
    coyote = 0;
    jumpBuf = 0;
    jumpHeld = true;
    jumpCutDone = false;
    beep(750, 12);
  }

  // Variable jump: on release while rising, cut vy once by half (SM)
  if (!jumpHeld && !jumpCutDone && fvy < 0) {
    fvy = fvy / 2;
    jumpCutDone = true;
  }

  // Gravity + terminal velocity
  fvy += GRAV_FX;
  if (fvy > FALL_MAX_FX) fvy = FALL_MAX_FX;

  // ---- Integrate X ----
  int16_t prevX = px;
  fpx += fvx;
  syncPlayerInts();
  if (px < BODY_HALF + 1) { px = BODY_HALF + 1; fpx = I2F(px); fvx = 0; }
  if (px > WORLD_W - BODY_HALF - 1) {
    px = WORLD_W - BODY_HALF - 1; fpx = I2F(px); fvx = 0;
  }

  if (fvx != 0) {
    int16_t probeX = px + (fvx > 0 ? BODY_HALF : -BODY_HALF);
    for (int16_t probeY = py - 2; probeY > py - BODY_H + 2; probeY -= 5) {
      if (solidBlockAt(probeX, probeY)) {
        px = prevX;
        fpx = I2F(px);
        fvx = 0;
        break;
      }
    }
  }

  // ---- Integrate Y ----
  int16_t prevFeet = py;
  fpy += fvy;
  syncPlayerInts();

  // Ceiling vs solid slabs
  if (fvy < 0) {
    int16_t head = py - BODY_H + 1;
    if (solidBlockAt(px, head) ||
        solidBlockAt(px - BODY_HALF, head) ||
        solidBlockAt(px + BODY_HALF, head)) {
      for (uint8_t i = 0; i < PLAT_N; i++) {
        const Plat &p = PLATS[i];
        if (!p.solid) continue;
        if (px >= p.x - BODY_HALF && px < p.x + p.w + BODY_HALF &&
            head < p.y + SLAB_H && head >= p.y) {
          py = p.y + SLAB_H + BODY_H - 1;
          fpy = I2F(py);
          fvy = 0;
          break;
        }
      }
    }
  }

  // Floor
  bool landedSolid = true;
  int16_t floorY = findFloor(px, py, prevFeet, fvy, &landedSolid);
  onGround = false;
  if (floorY != 32767 && fvy >= 0 && py >= floorY) {
    py = floorY;
    fpy = I2F(py);
    fvy = 0;
    onGround = true;
  }

  if (py > WORLD_H + 20) {
    fpx = I2F(48); fpy = I2F(180); fvx = fvy = 0; syncPlayerInts();
  }

  // Run anim: period from speed (walk ~8f, run ~5f)
  if (onGround && fxAbs(fvx) > FX_ONE / 4) {
    uint8_t period = (fxAbs(fvx) >= (WALK_FX + RUN_FX) / 2) ? 5 : 8;
    if (++animTimer >= period) { animTimer = 0; runPhase++; }
  } else {
    animTimer = 0;
    if (onGround) runPhase = 0;
  }

  updateCamera();
}

void pauseStep(uint8_t e) {
  // L/R or Up moves; Menu confirms (same button that opened pause)
  if (e & (B_LEFT | B_RIGHT | B_UP))
    pauseSel ^= 1;
  if (e & B_MENU) {
    if (pauseSel == 0) {
      mode = GAME;
      inputLock = 8;
      menuHeld = false;
      menuDownMs = 0;
      beep(600, 15);
    } else {
      mode = VIEW;
      beep(300, 40);
    }
  }
}

// -------------------- clock UI --------------------
uint8_t setPage = 0, setField = 0;
int16_t wfX = -8;
uint8_t wfAnim = 0;

void stepWatchFace() {
  wfAnim++;
  wfX += 1;
  if (wfX > 140) wfX = -10;
}

void drawView() {
  u8g2.setDrawColor(1);
  for (uint8_t i = 0; i < 16; i++)
    plot((i * 41 + wfAnim) & 127, 8 + (i * 7) % 18);
  u8g2.setFont(u8g2_font_logisoso24_tn);
  u8g2.setCursor(28, 36); two(tod / 3600); u8g2.print(':'); two((tod / 60) % 60);
  // tiny ball on ground line
  blitBits(wfX - 3, 49, SAMUS_BALL, 7, 1, false);
  hLineSafe(0, 56, 128);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(2, 63); monthName(mon); u8g2.print(' '); two(dday);
  u8g2.setCursor(100, 63); two(tod % 60);
  if (alarmOn) {
    u8g2.setCursor(40, 9); u8g2.print(F("ALM "));
    two(alarmTod / 3600); u8g2.print(':'); two((alarmTod / 60) % 60);
  }
}

void adjust(int8_t d) {
  if (setPage == 0) {
    uint8_t h = tod / 3600, m = (tod / 60) % 60, s = tod % 60;
    if (setField == 0) h = (h + d + 24) % 24; else m = (m + d + 60) % 60;
    tod = h * 3600UL + m * 60UL + s;
  } else if (setPage == 1) {
    if (setField == 0) mon = (mon - 1 + d + 12) % 12 + 1;
    uint8_t dim = MDAYS[mon - 1];
    if (dday > dim) dday = dim;
    if (setField == 1) dday = (dday - 1 + d + dim) % dim + 1;
  } else {
    if (setField == 2) { alarmOn = !alarmOn; return; }
    uint8_t h = alarmTod / 3600, m = (alarmTod / 60) % 60;
    if (setField == 0) h = (h + d + 24) % 24; else m = (m + d + 60) % 60;
    alarmTod = h * 3600UL + m * 60UL;
  }
}
void setLoop(uint8_t e) {
  if (e & B_DOWN) { setPage++; setField = 0; if (setPage > 2) mode = VIEW; return; }
  if (e & B_LEFT) adjust(-1);
  if (e & B_RIGHT) adjust(1);
  if (e & B_UP) {
    setField++;
    if (setField >= ((setPage == 2) ? 3 : 2)) { setField = 0; mode = VIEW; }
  }
}
void drawSet() {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(setPage == 2 ? 40 : 30, 12);
  u8g2.print(setPage == 0 ? F("SET TIME") : setPage == 1 ? F("SET DATE") : F("ALARM"));
  u8g2.setFont(u8g2_font_logisoso20_tn);
  u8g2.setCursor(20, 40);
  if (setPage == 0) { two(tod / 3600); u8g2.print(':'); two((tod / 60) % 60); }
  else if (setPage == 1) { monthName(mon); u8g2.print(' '); two(dday); }
  else { two(alarmTod / 3600); u8g2.print(':'); two((alarmTod / 60) % 60); }
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 54); u8g2.print(F("L/R change"));
  u8g2.setCursor(0, 63);
  if (setPage == 2 && setField == 2) { u8g2.print(F("ALARM ")); u8g2.print(alarmOn ? F("ON") : F("OFF")); }
  else if (setPage == 1) u8g2.print(setField ? F("DAY") : F("MONTH"));
  else u8g2.print(setField ? F("MINUTE") : F("HOUR"));
  u8g2.setCursor(72, 63); u8g2.print(F("UP next"));
}

void ringLoop() {
  static bool on = false;
  bool want = (millis() % 1000) < 300;
  if (want != on) { on = want; if (on) tone(P_BUZ, 1200); else noTone(P_BUZ); }
}
void drawRing() {
  if ((millis() / 400) & 1) {
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.setCursor(18, 30); u8g2.print(F("WAKE UP!"));
  }
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(18, 50); u8g2.print(F("PRESS ANY BUTTON"));
}

void update(uint8_t e) {
  switch (mode) {
    case VIEW:
      if (idleStage < 2) stepWatchFace();
      if (e & B_DOWN) { displayEnsureOn(); mode = SETM; setPage = 0; setField = 0; }
      else if (e & B_MENU) { displayEnsureOn(); startSandbox(); }
      break;
    case SETM: setLoop(e); break;
    case RING: if (e) { mode = VIEW; noTone(P_BUZ); } else ringLoop(); break;
    case GAME: gamePhysics(e); break;
    case PAUSE: pauseStep(e); break;
  }
}

void render() {
  switch (mode) {
    case VIEW: drawView(); break;
    case SETM: drawSet(); break;
    case RING: drawRing(); break;
    case GAME: drawGame(); break;
    case PAUSE: drawPause(); break;
  }
}

void setup() {
  for (uint8_t i = 0; i < 5; i++) pinMode(BPIN[i], INPUT_PULLUP);
  pinMode(P_BUZ, OUTPUT);
  Wire.setSDA(P_SDA); Wire.setSCL(P_SCL); Wire.setClock(400000);
  u8g2.begin();
  u8g2.setBitmapMode(1);
  lastSec = millis();
  displayWake();
}

void loop() {
  pending |= readButtons();
  unsigned long now = millis();
  if (now - lastSec >= 1000) { lastSec += 1000; tick(); }
  if (pending) {
    lastInputMs = now;
    if (mode == VIEW && idleStage) displayWake();
  }
  static unsigned long lastFrame = 0;
  if (now - lastFrame < 33) return;
  lastFrame = now;
  uint8_t ev = pending; pending = 0;
  update(ev);
  updateClockIdle();
  if (mode == VIEW && idleStage == 2) return;
  u8g2.clearBuffer();
  render();
  if (mode == VIEW && idleStage == 1) applySoftwareDim();
  u8g2.sendBuffer();
}
