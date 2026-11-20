// Metroid Watch — Brinstar slice (rooms B01–B06 of the Rev 4 design document)
//
// Ship → Surface Path → Morph Ball → Low Duct → Missile Launcher → Junction Shaft.
// One continuous world of tile rooms (metroid_world.h); doors, a scrolling camera, beam and
// missiles, Morph Ball, Zoomers / Skrees / Rippers, energy, pickups, a save on the ship.
//
// Clock, alarm, set screens and buttons live in libraries/WatchCore.
// Physics runs at 60 Hz (two steps per ~30 FPS frame); positions are 1/256 px.

#include <Arduino.h>
#include <WatchCore.h>
#include <EEPROM.h>
#include "metroid_gb_sprites.h"   // Samus, from the Metroid II sheet (scripts/gen_metroid_sprites.py)
#include "metroid_world.h"

typedef int32_t fx;

// ---- all structs up here, before Arduino's generated prototypes ----
struct Door { int16_t tx, ty; uint8_t h; char kind; uint8_t timer; };
struct Item { int16_t tx, ty; char kind; int8_t face; };   // face: statue side (+1 left of the orb, -1 right)
struct Enemy {
  bool live; uint8_t kind, st, hp, t, flash, lost;
  int16_t x, y;            // top-left, px
  int8_t nx, ny, dx, dy;   // Zoomer: surface normal and crawl direction
  uint8_t w, h;
  fx acc, vy;
  uint16_t frozen;          // Ice Missiles: physics steps left frozen (solid, harmless)
};
struct Shot { bool live; bool missile, ice; fx x, y, vx; uint8_t life; };
struct Bullet { bool live; fx x, y, vx, vy; uint8_t life, kind; };   // kind: 0 shard, 1 turret shot, 2 pirate bolt, 3 Kraid spike (falls, then skids)
struct Drop { bool live; uint8_t kind; int16_t x, y; uint16_t life; };
struct Spark { int16_t x, y; int8_t vx, vy; uint8_t life; };
struct Bomb { bool live; int16_t x, y; uint8_t t; };   // t counts up: fuse, then the blast
struct SaveData {
  uint32_t magic; uint8_t ver, gear, maxEn, maxMs, gearOff;
  uint64_t items, visited; uint32_t redDoors, frames;
  uint16_t sum;
};

// =====================================================================
// Buttons. The prototype has five: Mode, Left, Action, Right, Select/OK.
//   Left/Right move · Action = UP (jump; stand up from the ball) · Mode = DOWN (morph / save)
//   Select/OK = FIRE (bomb in the ball) · DOWN + FIRE together = "mode": tap switches
//   beam/missiles, hold opens the pause menu.
// When the 6th button is fitted it becomes DOWN and Mode goes back to cancel/exit:
// change B_DOWN here (and add the pin to WatchCore) — nothing else needs to move.
// =====================================================================
const uint8_t B_UP = B_MID;     // Action
const uint8_t B_DOWN = B_MODE;  // Mode (for now)
const uint8_t B_FIRE = B_GAME;  // Select/OK
const uint8_t CHORD_HOLD = 12;  // frames of DOWN+FIRE that open the pause menu (~0.4 s)

// ---- modes ----
enum : uint8_t { M_TITLE, M_STORY, M_PLAY, M_ITEM, M_PAUSE, M_DEAD };
uint8_t mode = M_TITLE;

// ---- screen ----
const uint8_t HUD_H = 8;
const uint8_t VIEW_H = 56;

// ---- physics (per 60 Hz step, 1/256 px) ----
const fx RUN_MAX = 384;     // 1.5 px/step  (~90 px/s)
const fx ACC_G = 40, DEC_G = 56, TURN_G = 80;
const fx ACC_A = 30, DRAG_A = 8;
const fx GRAV = 32;          // jump peak ≈ 46 px (2 × Samus): a 4-tile ledge with ~14 px to spare
const fx JUMP_V = 880;
const fx JUMP2_V = 700;      // double jump in mid-air: ≈ 28 px more (≈ 7 tiles with both)
const uint8_t AIR_JUMPS = 1;
const fx SPACE_READY = -200;  // Space Jump: the next jump comes once the last one has nearly topped out
const uint8_t SPIN_DMG = 4;   // Space Jump somersault vs enemies
const int8_t ORB_R = 7, ORB_LIFT = 4;   // Chozo orb radius; drawn this far above the item tile centre
// Kraid: 6 hits in the roaring mouth (beam 1, missile 2); phases in physics steps
const uint8_t KRAID_HP = 6, KRAID_IDLE = 70, KRAID_BRACE_T = 30, KRAID_VOLLEY = 48, KRAID_ROAR_T = 80, KRAID_FLINCH = 34, KRAID_DIE = 110;
// Ridley: 6 missile hits while he's landed; phases in physics steps
const uint8_t RIDLEY_HP = 6, RID_HOVER = 70, RID_TELL = 26, RID_LAND = 60, RID_RISE = 34, RID_DIE = 120;
// Mother Brain: 6 missiles crack the glass, then 8 beam hits (missiles 2) on the brain
const uint8_t MB_GLASS = 6, MB_HP = 8, MB_CANNON = 140, MB_CANNON_TELL = 26, MB_EYE = 150, MB_RING = 46, MB_DIE = 140;
// Metroid timings (physics steps): pulse before a dive, dive length, energy drain while latched, stun
const uint8_t MET_TELL = 24, MET_DIVE = 44, MET_DRAIN = 50, MET_STUN = 70;
const uint16_t FREEZE_T = 300;  // Ice Missiles: an enemy stays frozen ~5 s (physics steps)
const uint16_t THAW_WARN = 90;  // ...and shivers for the last 1.5 s
const fx JUMP_CUT = 256;     // release while rising → at most 1 px/step up
const fx FALL_MAX = 900;
const fx SPIN_MIN = 160;     // running this fast when jumping → somersault
const uint8_t COYOTE = 6, JUMP_BUF = 8;
const int8_t BODY_HW = 4, BODY_H = 21;   // standing hitbox 8 × 21 (fits a 3-tile corridor)
const int8_t BALL_HW = 3, BALL_H = 7;    // ball 6 × 7 (fits a 1-tile duct)
const int8_t CROUCH_H = 15;              // crouching: low shots; no crouch-walking
// bombs (frames are ~30 Hz game frames)
const uint8_t MAX_BOMB = 3;         // live at once, like Super Metroid
const uint8_t BOMB_FUSE = 18;       // ~0.6 s
const uint8_t BOMB_BLAST_T = 7;     // blast drawn this many frames
const int8_t BOMB_R = 11;           // blast reach (px) for blocks, enemies and the bomb jump
const fx BOMB_JUMP_V = 640;         // pop when the ball is in the blast: ~25 px (3 tiles)
const uint8_t MAX_BROKEN = 32;

// ---- weapons ----
const fx BEAM_V = 1024;      // 4 px/step
const uint8_t BEAM_LIFE = 16;   // ~64 px: Samus' short Power Beam
const fx MISSILE_V0 = 512, MISSILE_ACC = 48, MISSILE_VMAX = 1280;
const uint8_t MISSILE_LIFE = 70;
const uint8_t MAX_SHOTS = 3;

// ---- gear bits ----
enum : uint8_t { G_MORPH = 1, G_MISSILE = 2, G_BOMB = 4, G_SPACE = 8, G_ICE = 16 };

// ---- enemies ----
enum : uint8_t { EN_NONE, EN_ZOOMER, EN_SKREE, EN_RIPPER, EN_TURRET, EN_SWIPE, EN_BOLT, EN_METROID, EN_KRAID, EN_RIDLEY, EN_MBRAIN };
inline bool isBoss(uint8_t k) { return k >= EN_KRAID; }
// weapon that hit an enemy
enum : uint8_t { HIT_BEAM, HIT_MISSILE, HIT_BOMB, HIT_SPIN };
const uint8_t MAX_EN = 12, MAX_BUL = 8, MAX_DROP = 6, MAX_SPARK = 32, MAX_DOOR = 24, MAX_ITEM = 16;

inline int16_t toPx(fx v) { return (int16_t)(v >> 8); }
inline fx toFx(int16_t p) { return (fx)p << 8; }
inline int8_t sgn(fx v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }

// =====================================================================
// State
// =====================================================================
// Samus — x is body centre, y is the feet (bottom edge, exclusive)
fx sX, sY, sVX, sVY;
bool ground, ball, crouch, spin, faceR = true, jumpHeld;
uint8_t coyote, jumpBuf, airJumps; bool jumpPress, bombPop;   // bombPop: rising from a bomb (no jump cut)
uint8_t fireCD, invul, hurtLock, runPhase, animT, spinPhase;
uint8_t energy, maxEnergy, missiles, maxMissiles, weapon, gear;
uint8_t gearOff;                  // gear switched OFF in the pause menu's Items tab
inline uint8_t eg() { return gear & ~gearOff & ~((gearOff & G_MORPH) ? G_BOMB : 0) & ~((gearOff & G_MISSILE) ? G_ICE : 0); }
uint32_t playFrames;

// world
Door doors[MAX_DOOR]; uint8_t doorN;
Item items[MAX_ITEM]; uint8_t itemN;
uint64_t itemsGot, visited;
uint32_t redOpen;
uint16_t orbOpen;                 // major items: the Chozo orb has been shot open (not saved)
int8_t curRoom = 0;
int16_t padTX = 0, padTY = 0;     // the ship's save pad
int16_t sbTX = 0, sbTY = 0;       // test-room start
uint8_t titleHoldT;               // Left + Right held on the title -> test room
int16_t camX, camY;

Enemy en[MAX_EN];
Shot shots[MAX_SHOTS];
Bullet bul[MAX_BUL];
Drop drops[MAX_DROP];
Spark sparks[MAX_SPARK];
Bomb bombs[MAX_BOMB];
int16_t brokenX[MAX_BROKEN], brokenY[MAX_BROKEN]; uint8_t brokenN;   // bombed blocks (re-form on room entry)
bool sandbox;                     // the hidden test room: everything unlocked, nothing saved

// input
uint8_t prevHeld, inputLock;
bool chord;
uint8_t downPrev, downHeldT;   // posture before the last DOWN press (0 stand 1 crouch 2 ball), frames held
uint8_t chordT;

// UI
uint8_t titleSel, storyPage, storyT, pauseSel, itemT, deadT, cardT;
uint8_t pauseTab, gearSel, pauseMsgT;   // pause: 0 MAP / 1 ITEMS, Items cursor, "NEED ROOM" timer
char itemKind;
uint8_t modeLabelT, msgT;
const char *msgText;
uint16_t frameN;

// saves
SaveData lastSave;
bool haveSave;
const uint32_t SAVE_MAGIC = 0x4D575331;  // "MWS1"

// transitions (ordered-dither fade; the game is frozen while it fades out)
enum : uint8_t { TR_NONE, TR_OUT, TR_IN };
uint8_t trState, trLevel, trSteps = 10;
void (*trAction)();
int8_t pendingRoom = -1;

void transitionTo(void (*action)(), uint8_t steps = 10) {
  if (trState == TR_OUT) return;
  trState = TR_OUT; trAction = action; trSteps = steps;
  if (trLevel > trSteps) trLevel = trSteps;
}

// =====================================================================
// World lookup
// =====================================================================
inline int16_t rTX(uint8_t r) { return ROOMS[r].mx * SCR_TW; }
inline int16_t rTY(uint8_t r) { return ROOMS[r].my * SCR_TH; }
inline int16_t rTW(uint8_t r) { return ROOMS[r].sw * SCR_TW; }
inline int16_t rTH(uint8_t r) { return ROOMS[r].sh * SCR_TH; }

int8_t roomAtTile(int16_t tx, int16_t ty) {
  if (curRoom >= 0) {
    uint8_t r = curRoom;
    if (tx >= rTX(r) && tx < rTX(r) + rTW(r) && ty >= rTY(r) && ty < rTY(r) + rTH(r)) return r;
  }
  for (uint8_t r = 0; r < ROOM_N; r++)
    if (tx >= rTX(r) && tx < rTX(r) + rTW(r) && ty >= rTY(r) && ty < rTY(r) + rTH(r)) return r;
  return -1;
}
int8_t roomAtPx(int16_t x, int16_t y) { return roomAtTile(x >> 3, y >> 3); }

char rawTile(int16_t tx, int16_t ty) {
  int8_t r = roomAtTile(tx, ty);
  if (r < 0) return '#';   // outside every room: solid rock
  return ROOMS[r].tiles[(ty - rTY(r)) * rTW(r) + (tx - rTX(r))];
}

int8_t doorAt(int16_t tx, int16_t ty) {
  for (uint8_t i = 0; i < doorN; i++)
    if (doors[i].tx == tx && ty >= doors[i].ty && ty < doors[i].ty + doors[i].h) return i;
  return -1;
}
bool doorIsOpen(uint8_t i) {
  return doors[i].timer > 0 || (doors[i].kind == 'R' && (redOpen >> i) & 1);
}

bool isBroken(int16_t tx, int16_t ty) {
  for (uint8_t i = 0; i < brokenN; i++) if (brokenX[i] == tx && brokenY[i] == ty) return true;
  return false;
}
bool solidTile(int16_t tx, int16_t ty) {
  char c = rawTile(tx, ty);
  switch (c) {
    case 'B': case 'X': return !isBroken(tx, ty);
    case '#': case '=': case 'G': case 'h': case 'S': return true;
    case 'D': case 'R': { int8_t d = doorAt(tx, ty); return d < 0 || !doorIsOpen(d); }
    default: return false;
  }
}
// Solid, for drawing rock outlines (doors count as wall so rock doesn't outline against them)
bool rockish(int16_t tx, int16_t ty) {
  char c = rawTile(tx, ty);
  if (c == 'B' || c == 'X') return !isBroken(tx, ty);
  return c == '#' || c == '=' || c == 'G' || c == 'D' || c == 'R';
}

// [x0,x1) × [y0,y1) in px
bool boxSolid(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  for (int16_t ty = y0 >> 3; ty <= (y1 - 1) >> 3; ty++)
    for (int16_t tx = x0 >> 3; tx <= (x1 - 1) >> 3; tx++)
      if (solidTile(tx, ty)) return true;
  return false;
}

inline int8_t bodyHW() { return ball ? BALL_HW : BODY_HW; }
inline int8_t bodyH() { return ball ? BALL_H : crouch ? CROUCH_H : BODY_H; }
bool samusFitsAt(int16_t x, int16_t y, int8_t hw, int8_t h) {
  return !samusBlocked(x - hw, y - h, x + hw, y);
}
bool canStand() { return samusFitsAt(toPx(sX), toPx(sY), BODY_HW, BODY_H); }

void buildWorld() {
  doorN = 0; itemN = 0;
  for (uint8_t r = 0; r < ROOM_N; r++) {
    int16_t w = rTW(r), h = rTH(r);
    for (int16_t ty = 0; ty < h; ty++)
      for (int16_t tx = 0; tx < w; tx++) {
        char c = ROOMS[r].tiles[ty * w + tx];
        int16_t gx = rTX(r) + tx, gy = rTY(r) + ty;
        if ((c == 'D' || c == 'R') && (ty == 0 || ROOMS[r].tiles[(ty - 1) * w + tx] != c) && doorN < MAX_DOOR) {
          uint8_t n = 0;
          while (ty + n < h && ROOMS[r].tiles[(ty + n) * w + tx] == c) n++;
          doors[doorN++] = {gx, gy, n, c, 0};
        }
        if (strchr("mlteojiMLOJI", c) && itemN < MAX_ITEM)
          items[itemN++] = {gx, gy, (char)tolower(c), (int8_t)(isupper(c) ? -1 : 1)};
        if (c == 'S') { padTX = gx; padTY = gy; }
        if (c == '@') { sbTX = gx; sbTY = gy; }
      }
  }
}

// Open a door (and the one facing it across the room boundary)
void openDoor(uint8_t i, bool permanent) {
  if (permanent) redOpen |= 1UL << i;
  doors[i].timer = 200;
  for (uint8_t j = 0; j < doorN; j++)
    if (j != i && (doors[j].tx == doors[i].tx + 1 || doors[j].tx == doors[i].tx - 1) &&
        doors[j].ty < doors[i].ty + doors[i].h && doors[i].ty < doors[j].ty + doors[j].h) {
      if (permanent && doors[j].kind == 'R') redOpen |= 1UL << j;
      doors[j].timer = 200;
    }
}

// =====================================================================
// Effects
// =====================================================================
void spark(int16_t x, int16_t y, int8_t vx, int8_t vy, uint8_t life) {
  for (uint8_t i = 0; i < MAX_SPARK; i++)
    if (!sparks[i].life) { sparks[i] = {x, y, vx, vy, life}; return; }
}
void burst(int16_t x, int16_t y, uint8_t n, uint8_t spd) {
  for (uint8_t i = 0; i < n; i++) {
    float a = i * 6.2832f / n;
    spark(x, y, (int8_t)(cosf(a) * spd), (int8_t)(sinf(a) * spd), 8 + (i & 3));
  }
}
void showMsg(const char *t, uint8_t frames) { msgText = t; msgT = frames; }

static const watch::Note TUNE_ITEM[] = {
  {523, 120}, {659, 120}, {784, 120}, {1047, 260}, {0, 60}, {988, 120}, {1047, 360}};
static const watch::Note TUNE_MAJOR[] = {     // the big one: slow rise, pause, resolve
  {392, 160}, {523, 160}, {659, 160}, {784, 320}, {0, 90}, {659, 130}, {784, 130}, {988, 130}, {1047, 600}};
static const watch::Note TUNE_TANK[] = {{1047, 60}, {1319, 60}, {1568, 140}};
static const watch::Note TUNE_ORB[] = {{1568, 40}, {1175, 40}, {784, 90}};
static const watch::Note TUNE_SAVE[] = {{784, 90}, {988, 90}, {1175, 90}, {1568, 240}};
static const watch::Note TUNE_TITLE[] = {
  {330, 300}, {0, 60}, {294, 300}, {0, 60}, {262, 300}, {0, 60}, {247, 600}};
static const watch::Note TUNE_DEAD[] = {{392, 120}, {330, 120}, {262, 120}, {196, 400}};

// =====================================================================
// Saving
// =====================================================================
uint16_t saveSum(const SaveData &s) {
  const uint8_t *p = (const uint8_t *)&s; uint16_t a = 0x5A5A;
  for (size_t i = 0; i < offsetof(SaveData, sum); i++) a = (uint16_t)(a * 31 + p[i]);
  return a;
}
void captureSave(SaveData &s) {
  memset(&s, 0, sizeof s);
  s.magic = SAVE_MAGIC; s.ver = 3; s.gear = gear; s.gearOff = gearOff; s.maxEn = maxEnergy; s.maxMs = maxMissiles;
  s.items = itemsGot; s.visited = visited; s.redDoors = redOpen; s.frames = playFrames;
  s.sum = saveSum(s);
}
void writeSave() {
  captureSave(lastSave); haveSave = true;
  EEPROM.put(0, lastSave);
  EEPROM.commit();
}
bool readSave() {
  SaveData s; EEPROM.get(0, s);
  if (s.magic != SAVE_MAGIC || s.ver != 3 || s.sum != saveSum(s)) return false;
  lastSave = s; return true;
}

// =====================================================================
// Rooms and spawning
// =====================================================================
void spawnEnemies(uint8_t r) {
  for (uint8_t i = 0; i < MAX_EN; i++) en[i].live = false;
  uint8_t n = 0; int16_t w = rTW(r), h = rTH(r);
  for (int16_t ty = 0; ty < h; ty++)
    for (int16_t tx = 0; tx < w; tx++) {
      char c = ROOMS[r].tiles[ty * w + tx];
      if (n >= MAX_EN || !strchr("zkrupqnKYW", c)) continue;
      Enemy &e = en[n++];
      memset(&e, 0, sizeof e);
      e.live = true;
      int16_t x = (rTX(r) + tx) * TILE, y = (rTY(r) + ty) * TILE;
      if (c == 'z') { e.kind = EN_ZOOMER; e.w = 8; e.h = 8; e.hp = 2; e.x = x; e.y = y;
                      e.nx = 0; e.ny = -1; e.dx = (tx & 1) ? -1 : 1; e.dy = 0; }
      if (c == 'k') { e.kind = EN_SKREE; e.w = 8; e.h = 10; e.hp = 2; e.x = x; e.y = y; }
      if (c == 'r') { e.kind = EN_RIPPER; e.w = 12; e.h = 6; e.hp = 1; e.x = x - 2; e.y = y + 1; e.dx = 1; }
      if (c == 'u') {             // turret: bolted to the wall beside it, firing away from it
        bool wallR = solidTile(rTX(r) + tx + 1, rTY(r) + ty);
        e.kind = EN_TURRET; e.w = 8; e.h = 6; e.hp = 3; e.x = x; e.y = y + 1; e.dx = wallR ? -1 : 1;
      }
      if (c == 'p' || c == 'q') { // Space Pirates stand on the floor below the tile, facing into the room
        e.kind = c == 'p' ? EN_SWIPE : EN_BOLT; e.w = 10; e.h = 23; e.hp = 6;
        e.x = x - 1; e.y = y + 8 - 23; e.dx = (tx < w / 2) ? 1 : -1;
      }
      if (c == 'K') { e.kind = EN_KRAID; e.w = 24; e.h = 42; e.hp = KRAID_HP; e.x = x - 4; e.y = y + 8 - 42; e.dx = -1; }
      if (c == 'Y') {             // Ridley: starts hovering; e.vy holds his arena's floor (px)
        e.kind = EN_RIDLEY; e.w = 28; e.h = 26; e.hp = RIDLEY_HP; e.vy = y + 8;
        e.y = (int16_t)e.vy - 54; e.x = x - 14; e.dx = -1;
      }
      if (c == 'W') {             // Mother Brain's tank stands on the floor, glass facing left
        e.kind = EN_MBRAIN; e.w = 34; e.h = 40; e.hp = MB_HP; e.lost = 0; e.x = x - 8; e.y = y + 8 - 40; e.dx = -1;
      }
      if (c == 'n') { e.kind = EN_METROID; e.w = 12; e.h = 10; e.hp = 1; e.x = x - 2; e.y = y; }
    }
}

int16_t roomPx0(uint8_t r) { return rTX(r) * TILE; }
int16_t roomPy0(uint8_t r) { return rTY(r) * TILE; }
int16_t roomPx1(uint8_t r) { return (rTX(r) + rTW(r)) * TILE; }
int16_t roomPy1(uint8_t r) { return (rTY(r) + rTH(r)) * TILE; }

bool bossFloorView(int16_t &ty) {      // a boss in the room: hold the view on its floor
  for (const auto &e : en) {
    if (!e.live || !isBoss(e.kind)) continue;
    if (e.kind == EN_RIDLEY) ty = (int16_t)e.vy + 2 - VIEW_H;   // floor at the bottom: room above for him
    else ty = e.y + e.h + 8 - VIEW_H;
    return true;
  }
  return false;
}
void cameraTarget(int16_t &tx, int16_t &ty) {
  int16_t x = toPx(sX), feet = toPx(sY);
  tx = x - 64 + (faceR ? 14 : -14);
  ty = feet - 42;
  bossFloorView(ty);
}
void clampCam() {
  int16_t x0 = roomPx0(curRoom), x1 = roomPx1(curRoom) - 128;
  int16_t y0 = roomPy0(curRoom), y1 = roomPy1(curRoom) - VIEW_H;
  if (camX < x0) camX = x0; if (camX > x1) camX = x1;
  if (camY < y0) camY = y0; if (camY > y1) camY = y1;
}
void snapCamera() { cameraTarget(camX, camY); clampCam(); }

void updateCamera() {
  int16_t tx, ty; cameraTarget(tx, ty);
  int16_t dx = tx - camX;
  camX += dx / 5 + sgn(dx);
  // vertical: follow with a dead zone so ordinary jumps don't drag the view
  int16_t feet = toPx(sY), head = feet - bodyH();
  int16_t fy = feet - camY;
  int16_t by;
  if (bossFloorView(by) && head - by > -12) { int16_t d = by - camY; camY += d / 4 + sgn(d); clampCam(); return; }
  if (fy > 50) camY += min<int16_t>(fy - 50, 6);
  else if (head - camY < 6) camY -= min<int16_t>(6 - (head - camY), 6);
  else if (ground) { int16_t dy = ty - camY; if (dy) camY += dy / 6 + sgn(dy); }
  clampCam();
}

void enterRoomNow() {
  if (pendingRoom < 0) return;
  curRoom = pendingRoom; pendingRoom = -1;
  visited |= 1ULL << curRoom;
  for (auto &s : shots) s.live = false;
  for (auto &b : bul) b.live = false;
  for (auto &d : drops) d.live = false;
  for (auto &s : sparks) s.life = 0;
  for (auto &b : bombs) b.live = false;
  brokenN = 0;
  spawnEnemies(curRoom);
  snapCamera();
}

void placeAtPad() {
  sX = toFx(padTX * TILE + 4) + 128; sY = toFx(padTY * TILE);
  sVX = sVY = 0; ground = true; ball = false; crouch = false; spin = false; faceR = true;
  curRoom = roomAtTile(padTX, padTY);
  pendingRoom = curRoom; enterRoomNow();
}

void loadFrom(const SaveData &s) {
  gear = s.gear; gearOff = s.gearOff; maxEnergy = s.maxEn; maxMissiles = s.maxMs;
  itemsGot = s.items; visited = s.visited; redOpen = s.redDoors; playFrames = s.frames;
  energy = maxEnergy; missiles = maxMissiles; weapon = 0;
  for (uint8_t i = 0; i < doorN; i++) doors[i].timer = 0;
  orbOpen = 0;
  invul = 0; hurtLock = 0; fireCD = 0; jumpBuf = 0; coyote = 0;
  placeAtPad();
}

void newGame() {
  sandbox = false;
  gear = 0; gearOff = 0; maxEnergy = 6; maxMissiles = 0; itemsGot = 0; visited = 0; redOpen = 0; playFrames = 0;
  captureSave(lastSave);
  loadFrom(lastSave);
  writeSave();          // automatic ship checkpoint
  mode = M_PLAY; cardT = 70; inputLock = 6;
}
void continueGame() { sandbox = false; loadFrom(lastSave); mode = M_PLAY; cardT = 70; inputLock = 6; }

// The hidden test room: every item, nothing saved; dying puts Samus back at its start.
void startSandbox() {
  sandbox = true;
  gear = G_MORPH | G_MISSILE | G_BOMB | G_SPACE | G_ICE; gearOff = 0;
  maxEnergy = energy = 10; maxMissiles = missiles = 30; weapon = 0;
  itemsGot = 0; orbOpen = 0; redOpen = 0;
  for (uint8_t i = 0; i < doorN; i++) doors[i].timer = 0;
  invul = 0; hurtLock = 0; fireCD = 0; jumpBuf = 0; coyote = 0;
  sX = toFx(sbTX * TILE + 4) + 128; sY = toFx((sbTY + 1) * TILE);
  sVX = sVY = 0; ground = true; ball = false; crouch = false; spin = false; faceR = true;
  curRoom = roomAtTile(sbTX, sbTY); pendingRoom = curRoom; enterRoomNow();
  mode = M_PLAY; cardT = 70; inputLock = 6;
}
void respawn() {
  if (sandbox) { startSandbox(); cardT = 0; return; }
  loadFrom(lastSave); mode = M_PLAY; inputLock = 6;
}

// =====================================================================
// Samus
// =====================================================================
bool onPad() {
  if (!ground || ball) return false;
  return rawTile(toPx(sX) >> 3, toPx(sY) >> 3) == 'S';
}

void hurt(int8_t fromDir, uint8_t dmg) {
  if (invul || mode != M_PLAY) return;
  energy = energy > dmg ? energy - dmg : 0;
  beep(180, 60);
  if (energy == 0) { mode = M_DEAD; deadT = 0; cardT = 0; msgT = 0; watch::playTune(TUNE_DEAD, 4); return; }
  invul = 90; hurtLock = 14; spin = false;
  sVX = fromDir * 256; sVY = -360; ground = false;
}

// A latched Metroid sucks energy: no knock-back, no invulnerability
void drainEnergy() {
  if (mode != M_PLAY) return;
  beep(90, 40);
  if (energy > 1) { energy--; return; }
  energy = 0; mode = M_DEAD; deadT = 0; cardT = 0; msgT = 0; watch::playTune(TUNE_DEAD, 4);
}

// Leave the ball (Action): all the way up if there's room, else to a crouch (nudging a couple of px)
void tryStand() {
  int16_t x = toPx(sX), y = toPx(sY);
  static const int8_t OFFS[5] = {0, -2, 2, -4, 4};
  for (uint8_t pass = 0; pass < 2; pass++)
    for (int8_t off : OFFS) {
      if (samusFitsAt(x + off, y, BODY_HW, pass ? CROUCH_H : BODY_H)) {
        sX += toFx(off); ball = false; crouch = pass == 1; beep(900, 10); return;
      }
    }
  beep(120, 25);   // no headroom: stay rolled up
  showMsg("NEED ROOM", 30);
}

// DOWN acts the moment it's pressed: stand -> crouch -> ball (and saves on the ship's pad).
// If FIRE joins it for the "mode" chord, handleInput puts the posture back.
void doDown() {
  downPrev = ball ? 2 : crouch ? 1 : 0;
  if (onPad() && !crouch) {
    energy = maxEnergy; missiles = maxMissiles;
    writeSave();
    watch::playTune(TUNE_SAVE, 4);
    showMsg("ENERGY RESTORED  SAVED", 70);
    return;
  }
  if (ball) return;
  if (ground && !crouch) { crouch = true; spin = false; beep(400, 6); return; }
  if (!(eg() & G_MORPH)) { beep(140, 20); return; }
  ball = true; crouch = false; spin = false; beep(500, 12);
}

void undoDown() {
  if (downPrev == 0) { if (canStand()) { ball = false; crouch = false; } }
  else if (downPrev == 1) { if (ball && samusFitsAt(toPx(sX), toPx(sY), BODY_HW, CROUCH_H)) { ball = false; crouch = true; } }
}

void switchWeapon() {
  if (!(eg() & G_MISSILE)) { beep(140, 20); return; }
  weapon ^= 1; modeLabelT = 30; beep(weapon ? 1200 : 800, 15);
}

void fire() {
  if (ball) { dropBomb(); return; }
  if (fireCD) return;
  uint8_t slot = 255;
  for (uint8_t i = 0; i < MAX_SHOTS; i++) if (!shots[i].live) { slot = i; break; }
  if (slot == 255) return;
  bool mis = weapon == 1;
  if (mis && missiles == 0) { beep(160, 20); showMsg("NO MISSILES", 30); return; }
  if (mis) missiles--;
  spin = false;   // firing ends the somersault
  int8_t d = faceR ? 1 : -1;
  Shot &s = shots[slot];
  s.live = true; s.missile = mis; s.ice = mis && (eg() & G_ICE);
  s.x = sX + toFx(d * 8); s.y = sY - toFx(crouch ? 8 : 15);
  s.vx = d * (mis ? MISSILE_V0 : BEAM_V);
  s.life = mis ? MISSILE_LIFE : BEAM_LIFE;
  fireCD = mis ? 14 : 8;
  beep(mis ? (s.ice ? 900 : 520) : 1400, mis ? 30 : 12);
}

// Frozen enemies are solid to Samus, except one she's already overlapping (so freezing an enemy
// against her never traps her)
bool frozenBlocks(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  int16_t cx = toPx(sX), cy = toPx(sY), hw = bodyHW(), h = bodyH();
  for (const auto &e : en) {
    if (!e.live || !e.frozen) continue;
    if (!overlap(x0, y0, x1, y1, e.x, e.y, e.x + e.w, e.y + e.h)) continue;
    if (overlap(cx - hw, cy - h, cx + hw, cy, e.x, e.y, e.x + e.w, e.y + e.h)) continue;
    return true;
  }
  return false;
}
bool samusBlocked(int16_t x0, int16_t y0, int16_t x1, int16_t y1) { return boxSolid(x0, y0, x1, y1) || frozenBlocks(x0, y0, x1, y1); }

// Move along one axis a pixel at a time; returns true if blocked
bool moveAxis(bool horiz, fx dv) {
  fx &p = horiz ? sX : sY;
  int16_t a = toPx(p), b = toPx(p + dv);
  int8_t st = b > a ? 1 : -1;
  int8_t hw = bodyHW(), h = bodyH();
  for (int16_t v = a; v != b; v += st) {
    int16_t n = v + st;
    int16_t x = horiz ? n : toPx(sX), y = horiz ? toPx(sY) : n;
    if (samusBlocked(x - hw, y - h, x + hw, y)) {
      p = toFx(v) + (st > 0 ? 255 : 0);
      return true;
    }
  }
  p += dv;
  return false;
}

void samusStep(int8_t dir, bool firstStep) {
  (void)firstStep;
  if (hurtLock) { hurtLock--; dir = 0; }
  if (crouch) {
    if (!ground && canStand()) crouch = false;          // knocked or dropped off a ledge: stand
    else if (dir) {                                     // a direction stands her up and runs
      if (canStand()) crouch = false;
      else { faceR = dir > 0; dir = 0; }
    }
  }
  if (invul) invul--;
  if (fireCD) fireCD--;

  // horizontal
  if (ground) {
    if (dir) {
      if (sgn(sVX) == -dir) sVX += dir * TURN_G;
      else sVX += dir * ACC_G;
      if (sVX > RUN_MAX) sVX = RUN_MAX; if (sVX < -RUN_MAX) sVX = -RUN_MAX;
    } else if (!hurtLock) {
      if (sVX > 0) { sVX -= DEC_G; if (sVX < 0) sVX = 0; }
      else if (sVX < 0) { sVX += DEC_G; if (sVX > 0) sVX = 0; }
    }
  } else {
    if (dir) {
      sVX += dir * ACC_A;
      if (sVX > RUN_MAX) sVX = RUN_MAX; if (sVX < -RUN_MAX) sVX = -RUN_MAX;
    } else if (!hurtLock) {
      if (sVX > 0) { sVX -= DRAG_A; if (sVX < 0) sVX = 0; }
      else if (sVX < 0) { sVX += DRAG_A; if (sVX > 0) sVX = 0; }
    }
  }
  if (dir) faceR = dir > 0;

  // jump (not in the ball — no Spring Ball yet)
  if (jumpBuf) jumpBuf--;
  if (ground) coyote = COYOTE; else if (coyote) coyote--;
  if (jumpBuf && coyote && !ball && crouch) { if (canStand()) crouch = false; else jumpBuf = 0; }
  if (jumpBuf && coyote && !ball) {
    sVY = -JUMP_V; ground = false; coyote = 0; jumpBuf = 0;
    spin = (sVX > SPIN_MIN || sVX < -SPIN_MIN);
    beep(spin ? 660 : 760, 14);
  }
  // double jump: a fresh press in mid-air (after the ledge grace) always somersaults
  bool spaceOK = (eg() & G_SPACE) && spin && sVY > SPACE_READY;
  if (jumpPress && jumpBuf && !ground && !coyote && !ball && !crouch && (airJumps || spaceOK)) {
    if (!spaceOK) airJumps--;
    sVY = -JUMP2_V; jumpBuf = 0;
    spin = true; spinPhase = 0;
    beep(880, 14);
  }
  jumpPress = false;
  if (sVY >= 0) bombPop = false;
  if (!jumpHeld && !bombPop && sVY < -JUMP_CUT) sVY = -JUMP_CUT;
  sVY += GRAV; if (sVY > FALL_MAX) sVY = FALL_MAX;

  if (moveAxis(true, sVX)) sVX = 0;
  bool wasGround = ground;
  if (moveAxis(false, sVY)) {
    if (sVY > 0) { ground = true; if (!wasGround && !ball) beep(220, 8); spin = false; }
    sVY = 0;
  } else {
    int16_t x = toPx(sX), y = toPx(sY);
    ground = samusBlocked(x - bodyHW(), y, x + bodyHW(), y + 1);
    if (ground && sVY > 0) sVY = 0;
  }
  if (ground) { spin = false; airJumps = AIR_JUMPS; }
  if (spin) spinPhase++;
  if (spin && (eg() & G_SPACE)) {           // the Space Jump somersault cuts through slashed rock
    int16_t x = toPx(sX), y = toPx(sY);
    for (int16_t ty = (y - BODY_H - 2) >> 3; ty <= (y + 1) >> 3; ty++)
      for (int16_t tx = (x - BODY_HW - 2) >> 3; tx <= (x + BODY_HW + 1) >> 3; tx++) breakBlock(tx, ty, 'X');
  }

  // animation
  if (ground && !ball && (sVX > 64 || sVX < -64)) { if (++animT >= 7) { animT = 0; runPhase++; } }
  else if (ball && (sVX > 32 || sVX < -32)) { if (++animT >= 4) { animT = 0; runPhase++; } }
  else animT = 0;
}

// =====================================================================
// Enemies
// =====================================================================
bool enemySolidAt(const Enemy &e, int16_t x, int16_t y) { return boxSolid(x, y, x + e.w, y + e.h); }

void zoomerCrawl(Enemy &e) {
  if (enemySolidAt(e, e.x + e.dx, e.y + e.dy)) {            // inner corner: climb the wall ahead
    int8_t ndx = e.nx, ndy = e.ny;
    e.nx = -e.dx; e.ny = -e.dy; e.dx = ndx; e.dy = ndy;
    return;
  }
  e.x += e.dx; e.y += e.dy;
  if (!enemySolidAt(e, e.x - e.nx, e.y - e.ny)) {           // outer corner: wrap round the edge
    int8_t ndx = -e.nx, ndy = -e.ny;
    e.nx = e.dx; e.ny = e.dy; e.dx = ndx; e.dy = ndy;
    e.x += e.dx; e.y += e.dy;
    if (!enemySolidAt(e, e.x - e.nx, e.y - e.ny)) { if (++e.lost > 2) { e.st = 1; e.vy = 0; } }
    else e.lost = 0;
  } else e.lost = 0;
}

int8_t enemyBullet(int16_t x, int16_t y, fx vx, fx vy, uint8_t life = 26, uint8_t kind = 0) {
  for (uint8_t i = 0; i < MAX_BUL; i++)
    if (!bul[i].live) { bul[i] = {true, toFx(x), toFx(y), vx, vy, life, kind}; return i; }
  return -1;
}

void killEnemy(Enemy &e);
// Every weapon hit goes through here: Metroids only care about ice (then a shattering missile) and
// bombs (which knock a latched one off); Rippers shrug off the beam.
void damageEnemy(Enemy &e, uint8_t dmg, uint8_t how, int8_t fromDir) {
  if (isBoss(e.kind)) { beep(2000, 6); return; }   // bosses take hits only where they're weak
  if (e.kind == EN_METROID) {
    if (how == HIT_BOMB) { e.st = 4; e.t = MET_STUN; e.y -= 10; e.x += fromDir * 8; beep(500, 30); }
    else if (how == HIT_MISSILE) { if (e.st != 3) { e.st = 4; e.t = 20; e.x += fromDir * 6; } beep(1600, 8); }
    else beep(2000, 6);
    return;
  }
  if (e.kind == EN_RIPPER && how == HIT_BEAM) { beep(2000, 8); return; }
  e.flash = 6;
  if (e.hp <= dmg) killEnemy(e);
  else { e.hp -= dmg; beep(600, 10); }
}

void killEnemy(Enemy &e) {
  e.live = false;
  int16_t cx = e.x + e.w / 2, cy = e.y + e.h / 2;
  burst(cx, cy, 8, 6);
  beep(300, 30);
  // drops: energy when hurt, missiles once Samus has the launcher
  uint8_t roll = random(100);
  uint8_t kind = 0;
  if (energy < maxEnergy && roll < 45) kind = 1;
  else if ((gear & G_MISSILE) && missiles < maxMissiles && roll < 75) kind = 2;
  if (kind) for (auto &d : drops) if (!d.live) { d = {true, kind, cx, cy, 300}; break; }
}

// ---- Turret: waits, flashes its barrel (the tell), fires a shot along the floor of its lane
const uint8_t TURRET_WAIT = 90, TURRET_TELL = 26;
void turretStep(Enemy &e, int16_t samX, int16_t samY) {
  (void)samY;
  int16_t dx = samX - (e.x + 4);
  bool ahead = (e.dx > 0 ? dx > 0 : dx < 0) && abs(dx) < 140;
  if (e.st == 0) { if (ahead || e.t) e.t++; if (e.t >= TURRET_WAIT) { e.st = 1; e.t = 0; } }
  else if (++e.t >= TURRET_TELL) {
    enemyBullet(e.dx > 0 ? e.x + 9 : e.x - 2, e.y + 2, e.dx * 400, 0, 220, 1);
    beep(1500, 10); e.st = 0; e.t = 0;
  }
}

// ---- Space Pirates (23 px). Both walk on floors and turn at walls and ledges.
bool pirateCanStep(const Enemy &e, int8_t d) {
  int16_t nx = e.x + d;
  if (enemySolidAt(e, nx, e.y)) return false;
  int16_t fx = d > 0 ? nx + e.w - 1 : nx;
  return solidTile(fx >> 3, (e.y + e.h) >> 3);
}
bool samusNear(const Enemy &e, int16_t samX, int16_t samY, int16_t rx, int16_t ry) {
  int16_t cx = e.x + e.w / 2, feet = e.y + e.h;
  return abs(samX - cx) < rx && abs(samY - feet) < ry;
}
// Swipe Pirate: patrols; when Samus is close it turns, raises its hooked claw (the tell), swipes in
// front of itself, then needs a moment to recover.
const uint8_t SWIPE_TELL = 22, SWIPE_HIT = 10, SWIPE_REST = 36;
void swipeStep(Enemy &e, int16_t samX, int16_t samY) {
  switch (e.st) {
    case 0:
      if (samusNear(e, samX, samY, 32, 22)) { e.dx = samX > e.x + e.w / 2 ? 1 : -1; e.st = 1; e.t = SWIPE_TELL; beep(420, 15); break; }
      if (++e.acc >= 2) { e.acc = 0; if (pirateCanStep(e, e.dx)) e.x += e.dx; else e.dx = -e.dx; }
      e.lost++;                                   // walk cycle
      break;
    case 1: if (!--e.t) { e.st = 2; e.t = SWIPE_HIT; beep(300, 25); } break;
    case 2: if (!--e.t) { e.st = 3; e.t = SWIPE_REST; } break;
    default: if (!--e.t) e.st = 0; break;
  }
}
// the swipe's reach: a box in front of the pirate at chest height
void swipeBox(const Enemy &e, int16_t &x0, int16_t &y0, int16_t &x1, int16_t &y1) {
  y0 = e.y + 7; y1 = e.y + 15;
  if (e.dx > 0) { x0 = e.x + e.w; x1 = x0 + 12; } else { x1 = e.x; x0 = x1 - 12; }
}
// Bolt Pirate: guards its spot facing Samus; reaches out (the tell, with a blinking muzzle) and fires
// one slow bolt at a time, high enough to crouch under and low enough to jump over.
const uint8_t BOLT_TELL = 26, BOLT_REST = 50;
void boltStep(Enemy &e, int16_t samX, int16_t samY) {
  bool seen = samusNear(e, samX, samY, 150, 40);
  if (seen && e.st == 0) e.dx = samX > e.x + e.w / 2 ? 1 : -1;
  switch (e.st) {
    case 0: {
      bool boltOut = e.lost && bul[e.lost - 1].live && bul[e.lost - 1].kind == 2;
      if (seen && !boltOut) { e.st = 1; e.t = BOLT_TELL; }
      break;
    }
    case 1:
      if (!--e.t) {
        int8_t i = enemyBullet(e.dx > 0 ? e.x + e.w + 7 : e.x - 8, e.y + 5, e.dx * 300, 0, 255, 2);
        e.lost = i >= 0 ? i + 1 : 0;
        beep(700, 20); e.st = 2; e.t = BOLT_REST;
      }
      break;
    default: if (!--e.t) e.st = 0; break;
  }
}

// ---- Metroid: drifts toward Samus, pulses (the tell), dives; on contact it latches onto her head and
// drains energy until a bomb blast knocks it off. Ice freezes it, then any missile shatters it.
void metroidStep(Enemy &e, int16_t samX, int16_t samY) {
  int16_t cx = e.x + 6, cy = e.y + 5;
  int16_t tx = samX, ty = samY - bodyH() / 2 - 2;
  switch (e.st) {
    case 0: {                                   // drift toward her, bobbing
      int16_t dx = tx - cx, dy = ty - cy;
      if (abs(dx) < 120 && abs(dy) < 80) {
        if (++e.acc >= 3) { e.acc = 0; e.x += sgn(dx); if (abs(dy) > 4) e.y += sgn(dy); }
        if (abs(dx) < 40 && abs(dy) < 30) { e.st = 1; e.t = MET_TELL; beep(200, 30); }
      }
      if ((frameN & 15) == 0) e.y += (frameN & 16) ? 1 : -1;
      break;
    }
    case 1:                                     // pulse, then dive at where she is now
      if (!--e.t) {
        int16_t dx = tx - cx, dy = ty - cy; int16_t m = max(abs(dx), abs(dy)); if (m < 1) m = 1;
        e.nx = (int8_t)(dx * 2 / m); e.ny = (int8_t)(dy * 2 / m);
        if (!e.nx && !e.ny) e.ny = 1;
        e.st = 2; e.t = MET_DIVE;
      }
      break;
    case 2:                                     // diving
      if (!enemySolidAt(e, e.x + e.nx, e.y)) e.x += e.nx;
      if (!enemySolidAt(e, e.x, e.y + e.ny)) e.y += e.ny;
      if (!--e.t) { e.st = 4; e.t = MET_STUN / 2; }
      break;
    case 3:                                     // latched on: ride her head and drain
      e.x = samX - 6; e.y = samY - bodyH() - 7;
      if (++e.t >= MET_DRAIN) { e.t = 0; drainEnergy(); }
      break;
    default:                                    // knocked away / recovering: float up, then hunt again
      if ((e.t & 3) == 0 && !enemySolidAt(e, e.x, e.y - 1)) e.y--;
      if (!--e.t) e.st = 0;
      break;
  }
}

// ---- Kraid: waits, braces, fires three belly spikes that arc to the floor and skid across it (jump
// them), then roars with his mouth open at jump-shot height: one hit there makes him flinch and close
// up. Six hits and he comes apart.
void kraidStep(Enemy &e) {
  e.t++;
  switch (e.st) {
    case 0: if (e.t >= KRAID_IDLE) { e.st = 1; e.t = 0; beep(120, 40); } break;
    case 1: if (e.t >= KRAID_BRACE_T) { e.st = 2; e.t = 0; } break;
    case 2:
      // three spikes: low, a high lob (it comes down on the far side), low
      if (e.t == 4 || e.t == 32) { enemyBullet(e.x + 2, e.y + 28, -480, 160, 255, 3); beep(260, 15); }
      if (e.t == 18) { enemyBullet(e.x + 4, e.y + 14, -400, -700, 255, 3); beep(200, 20); }
      if (e.t >= KRAID_VOLLEY) { e.st = 3; e.t = 0; beep(90, 80); }
      break;
    case 3: if (e.t >= KRAID_ROAR_T) { e.st = 0; e.t = 0; } break;
    case 4: if (e.t >= KRAID_FLINCH) { e.st = 0; e.t = KRAID_IDLE / 2; } break;
    default:                                    // coming apart
      if ((e.t & 5) == 0) burst(e.x + random(e.w), e.y + random(e.h), 6, 4), beep(80 + random(200), 20);
      if (e.t >= KRAID_DIE) { e.live = false; burst(e.x + 15, e.y + 20, 16, 7); bossDefeated(EN_KRAID); }
      break;
  }
}
// Kraid's weak point while he roars, in world px
void kraidMouth(const Enemy &e, int16_t &x0, int16_t &y0, int16_t &x1, int16_t &y1) { x0 = e.x - 1; y0 = e.y + 2; x1 = e.x + 16; y1 = e.y + 20; }   // the roaring head: a plain shot from the step, or a hop from the floor

// ---- Ridley: hovers high, screeches and flaps (the tell), then swoops across the arena - a high pass
// (stay low, or run under it) or a low pass (jump it), alternating - lands on the far side and stays
// exposed for a moment (only missiles hurt him, only then), and takes off again.
int16_t ridleyPerchX(int8_t side) {      // body x for a landing on the left (-1) or right (+1)
  int16_t x0 = roomPx0(curRoom), x1 = roomPx1(curRoom);
  return side < 0 ? x0 + 16 : x1 - 44;
}
void ridleyStep(Enemy &e) {
  int16_t floorY = (int16_t)e.vy, hoverY = floorY - 54;
  e.t++;
  switch (e.st) {
    case 0: {                                   // hover over the side he'll start from
      int16_t tx = ridleyPerchX(-e.dx);
      if (e.x != tx && (e.t & 1)) e.x += sgn(tx - e.x);
      if (e.y > hoverY) e.y--; else if ((e.t & 15) == 0) e.y += (e.t & 16) ? 1 : -1;
      if (e.t >= RID_HOVER && abs(e.x - tx) < 3) { e.st = 1; e.t = 0; beep(1600, 60); }
      break;
    }
    case 1: if (e.t >= RID_TELL) { e.st = 2; e.t = 0; e.acc = 0; } break;
    case 2: {                                   // the swoop
      bool low = e.lost & 1;
      int16_t passY = low ? floorY - 28 : floorY - 50;
      if (e.y < passY) e.y += min<int16_t>(2, passY - e.y);
      e.acc += 640;                             // 2.5 px/step
      while (e.acc >= 256) { e.acc -= 256; e.x += e.dx; }
      int16_t goal = ridleyPerchX(e.dx);
      if ((e.dx < 0 && e.x <= goal) || (e.dx > 0 && e.x >= goal)) {
        e.x = goal; e.st = 3; e.t = 0; e.h = 40; e.y = floorY - 40; beep(70, 60);
      }
      break;
    }
    case 3: if (e.t >= RID_LAND) { e.st = 4; e.t = 0; e.h = 26; e.y = floorY - 36; e.dx = -e.dx; e.lost ^= 1; } break;
    case 4: if (e.y > hoverY) e.y -= (e.t & 1) ? 2 : 1; if (e.t >= RID_RISE) { e.st = 0; e.t = 0; } break;
    default:
      if ((e.t & 5) == 0) burst(e.x + random(e.w), e.y + random(e.h), 6, 4), beep(80 + random(200), 20);
      if (e.t >= RID_DIE) { e.live = false; burst(e.x + 14, e.y + 20, 16, 7); bossDefeated(EN_RIDLEY); }
      break;
  }
}
// facing: flying the way he goes; landed, back toward the middle
bool ridleyFlip(const Enemy &e) { return (e.st == 3 || e.st == 5) ? e.dx < 0 : e.dx > 0; }
int16_t ridleyArtX(const Enemy &e) { return e.x - 6; }
// landed, his body (not the tail) is open to missiles - at standing-shot height
void ridleyHead(const Enemy &e, int16_t &x0, int16_t &y0, int16_t &x1, int16_t &y1) {
  int16_t ax = ridleyArtX(e);
  y0 = e.y + 4; y1 = e.y + 36;
  if (ridleyFlip(e)) { x0 = ax + 10; x1 = ax + 40; } else { x0 = ax; x1 = ax + 30; }
}

// ---- Mother Brain: sits in a glass tank. The cannon in the tank's base flashes (tell) and fires along
// the floor - jump it. Missiles crack the glass (beam glances off); when it shatters she's exposed: her
// eye opens and closes, and while it's open she fires slow rings at Samus. Beam shots finish her.
void mbrainStep(Enemy &e, int16_t samX, int16_t samY) {
  e.t++;
  if (e.st == 5) {
    if ((e.t & 3) == 0) burst(e.x + random(e.w), e.y + random(e.h), 6, 4), beep(80 + random(300), 20);
    if (e.t >= MB_DIE) { e.live = false; burst(e.x + 17, e.y + 20, 20, 8); bossDefeated(EN_MBRAIN); }
    return;
  }
  // the floor cannon
  e.acc++;
  if (e.acc == MB_CANNON) beep(1100, 30);
  if (e.acc >= MB_CANNON + MB_CANNON_TELL) { e.acc = 0; enemyBullet(e.x - 3, e.y + e.h - 5, -420, 0, 255, 1); beep(1500, 12); }
  // exposed: the eye cycles; open, it fires rings
  if (e.st == 1) {
    uint16_t ph = e.t % MB_EYE;
    bool open = ph < MB_EYE * 3 / 5;
    if (open && ph % MB_RING == MB_RING / 2) {
      int16_t cx = e.x + 8, cy = e.y + 18, dx = samX - cx, dy = (samY - 12) - cy;
      int16_t m = max(abs(dx), abs(dy)); if (m < 1) m = 1;
      enemyBullet(cx, cy, (fx)dx * 320 / m, (fx)dy * 320 / m, 255, 4); beep(600, 20);
    }
  }
}
bool mbrainEyeOpen(const Enemy &e) { return e.st == 1 && (e.t % MB_EYE) < MB_EYE * 3 / 5; }

// A boss is beaten: its reward, with the full pickup ceremony
void bossDefeated(uint8_t kind) {
  if (kind == EN_KRAID) { gear |= G_SPACE; itemKind = 'j'; }
  if (kind == EN_RIDLEY) { gear |= G_ICE; itemKind = 'i'; }
  if (kind == EN_MBRAIN) {                  // the escape comes with Tourian; for now, the news
    for (auto &b : bul) b.live = false;
    showMsg("MOTHER BRAIN DESTROYED", 150); watch::playTune(TUNE_MAJOR, 9); return;
  }
  for (auto &b : bul) b.live = false;
  itemT = 0; mode = M_ITEM; watch::playTune(TUNE_MAJOR, 9);
}

void enemyStep(Enemy &e) {
  if (e.flash) e.flash--;
  if (e.frozen) { e.frozen--; return; }
  int16_t samX = toPx(sX), samY = toPx(sY);
  switch (e.kind) {
    case EN_ZOOMER:
      if (e.st == 1) {          // knocked loose: fall until it lands
        e.vy += GRAV; if (e.vy > FALL_MAX) e.vy = FALL_MAX;
        e.acc += e.vy;
        while (e.acc >= 256) {
          e.acc -= 256;
          if (enemySolidAt(e, e.x, e.y + 1)) { e.st = 0; e.nx = 0; e.ny = -1; e.dy = 0; if (!e.dx) e.dx = 1; e.lost = 0; e.acc = 0; break; }
          e.y++;
        }
      } else {
        e.acc += 96;            // 0.375 px/step
        while (e.acc >= 256) { e.acc -= 256; zoomerCrawl(e); if (e.st) break; }
      }
      break;
    case EN_SKREE:
      if (e.st == 0) {          // hanging
        int16_t dx = samX - (e.x + 4);
        if (samY > e.y && samY - e.y < 70 && dx > -30 && dx < 30) { e.st = 1; e.vy = 0; beep(900, 20); }
      } else if (e.st == 1) {   // diving, drifting toward Samus
        e.vy += 22; if (e.vy > 640) e.vy = 640;
        e.acc += e.vy;
        while (e.acc >= 256) {
          e.acc -= 256;
          if (enemySolidAt(e, e.x, e.y + 1)) { e.st = 2; e.t = 36; break; }
          e.y++;
        }
        if ((frameN & 1) == 0) {
          int8_t d = samX > e.x + 4 ? 1 : samX < e.x + 4 ? -1 : 0;
          if (d && !enemySolidAt(e, e.x + d, e.y)) e.x += d;
        }
      } else if (e.st == 2) {   // dug in, shaking — then bursts into shards
        if (e.t) e.t--;
        if (!e.t) {
          int16_t cx = e.x + 4, cy = e.y + 4;
          enemyBullet(cx, cy, -400, -400); enemyBullet(cx, cy, 400, -400);
          enemyBullet(cx, cy, -560, 0);    enemyBullet(cx, cy, 560, 0);
          burst(cx, cy, 6, 5); beep(250, 25);
          e.live = false;
        }
      }
      break;
    case EN_RIPPER:
      e.acc += 128;   // 0.5 px/step
      while (e.acc >= 256) {
        e.acc -= 256;
        if (enemySolidAt(e, e.x + e.dx, e.y)) e.dx = -e.dx;
        else e.x += e.dx;
      }
      break;
    case EN_TURRET: turretStep(e, samX, samY); break;
    case EN_SWIPE: swipeStep(e, samX, samY); break;
    case EN_BOLT: boltStep(e, samX, samY); break;
    case EN_METROID: metroidStep(e, samX, samY); break;
    case EN_KRAID: kraidStep(e); break;
    case EN_RIDLEY: ridleyStep(e); break;
    case EN_MBRAIN: mbrainStep(e, samX, samY); break;
  }
  // left the room (fell out / crawled through a door): gone
  if (e.x + e.w < roomPx0(curRoom) || e.x > roomPx1(curRoom) || e.y > roomPy1(curRoom)) e.live = false;
}

bool overlap(int16_t ax0, int16_t ay0, int16_t ax1, int16_t ay1, int16_t bx0, int16_t by0, int16_t bx1, int16_t by1) {
  return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

void samusBox(int16_t &x0, int16_t &y0, int16_t &x1, int16_t &y1) {
  int16_t x = toPx(sX), y = toPx(sY);
  x0 = x - bodyHW(); x1 = x + bodyHW(); y0 = y - bodyH(); y1 = y;
}

// =====================================================================
// Shots
// =====================================================================
void shotStep(Shot &s) {
  if (s.missile) { s.vx += sgn(s.vx) * MISSILE_ACC; if (s.vx > MISSILE_VMAX) s.vx = MISSILE_VMAX; if (s.vx < -MISSILE_VMAX) s.vx = -MISSILE_VMAX; }
  int16_t px0 = toPx(s.x);
  s.x += s.vx;
  int16_t x = toPx(s.x), y = toPx(s.y);
  if (!s.life--) { s.live = false; return; }
  int16_t tx = x >> 3, ty = y >> 3;
  if (solidTile(tx, ty)) {
    int8_t d = doorAt(tx, ty);
    if (d >= 0) {
      if (doors[d].kind == 'D') { openDoor(d, false); beep(700, 25); }
      else if (doors[d].kind == 'R' && s.missile) { openDoor(d, true); beep(500, 60); burst(x, y, 6, 4); }
      else { beep(1800, 6); }
    }
    spark(x - sgn(s.vx) * 2, y, -sgn(s.vx) * 3, -2, 5); spark(x - sgn(s.vx) * 2, y, -sgn(s.vx) * 3, 2, 5);
    s.live = false; return;
  }
  for (uint8_t i = 0; i < itemN; i++) {       // a shot cracks a Chozo orb open
    if (((itemsGot >> i) & 1) || !orbClosed(i)) continue;
    int16_t ox = items[i].tx * TILE + 4, oy = items[i].ty * TILE + 4 - ORB_LIFT;
    // swept from where the shot was, so a point-blank shot from inside the orb still counts
    int16_t a = min(px0, x) - 2, b = max(px0, x) + 2;
    if (s.vx > 0) a -= 8; else b += 8;           // back to the muzzle
    if (!overlap(a, y - 2, b, y + 2, ox - ORB_R - 1, oy - ORB_R - 1, ox + ORB_R + 1, oy + ORB_R + 1)) continue;
    orbOpen |= 1 << i; s.live = false;
    burst(ox, oy, 14, 6); burst(ox, oy, 8, 3);
    watch::playTune(TUNE_ORB, 3);
    return;
  }
  for (auto &e : en) {
    if (!e.live) continue;
    if (!overlap(x - 2, y - 2, x + 2, y + 2, e.x, e.y, e.x + e.w, e.y + e.h)) continue;
    s.live = false;
    if (e.kind == EN_KRAID) {                      // only the open mouth counts; the hide is armour
      int16_t a0, b0, a1, b1; kraidMouth(e, a0, b0, a1, b1);
      if (e.st == 3 && overlap(x - 2, y - 2, x + 2, y + 2, a0, b0, a1, b1)) {
        uint8_t dmg = s.missile ? 2 : 1;
        e.hp = e.hp > dmg ? e.hp - dmg : 0; e.flash = 10; burst(x, y, 10, 5);
        if (!e.hp) { e.st = 5; e.t = 0; for (auto &b : bul) b.live = false; watch::stopTune(); }
        else { e.st = 4; e.t = 0; beep(140, 60); }
      } else { beep(2000, 6); spark(x, y, -sgn(s.vx) * 4, -3, 5); spark(x, y, -sgn(s.vx) * 3, 3, 5); }
      return;
    }
    if (e.kind == EN_MBRAIN) {
      if (e.st == 0) {                             // the glass: missiles crack it
        if (s.missile) {
          e.lost++; e.flash = 8; burst(x, y, 8, 4); beep(1800, 25);
          if (e.lost >= MB_GLASS) { e.st = 1; e.t = 0; for (int8_t k = 0; k < 4; k++) burst(e.x + 2, e.y + 6 + k * 8, 8, 6); beep(2400, 80); }
        } else { beep(2000, 6); spark(x, y, -sgn(s.vx) * 4, -3, 5); }
      } else if (e.st == 1 && !overlap(x - 2, y - 2, x + 2, y + 2, e.x + 4, e.y + 6, e.x + 32, e.y + 30) && y < e.y + 32) {
        s.live = true; continue;                   // the glass is gone: fly on in to the brain
      } else if (e.st == 1 && overlap(x - 2, y - 2, x + 2, y + 2, e.x + 4, e.y + 6, e.x + 32, e.y + 30)) {
        uint8_t dmg = s.missile ? 2 : 1;
        e.hp = e.hp > dmg ? e.hp - dmg : 0; e.flash = 8; burst(x, y, 8, 4); beep(300, 30);
        if (!e.hp) { e.st = 5; e.t = 0; for (auto &b : bul) b.live = false; }
      } else { beep(2000, 6); spark(x, y, -sgn(s.vx) * 4, -3, 5); }
      return;
    }
    if (e.kind == EN_RIDLEY) {                     // missiles, while he's landed, anywhere on his body
      int16_t a0, b0, a1, b1; ridleyHead(e, a0, b0, a1, b1);
      if (e.st == 3 && s.missile && overlap(x - 2, y - 2, x + 2, y + 2, a0, b0, a1, b1)) {
        e.hp = e.hp > 1 ? e.hp - 1 : 0; e.flash = 10; burst(x, y, 10, 5); beep(140, 60);
        if (!e.hp) { e.st = 5; e.t = 0; e.h = 40; for (auto &b : bul) b.live = false; }
      } else { beep(2000, 6); spark(x, y, -sgn(s.vx) * 4, -3, 5); spark(x, y, -sgn(s.vx) * 3, 3, 5); }
      return;
    }
    if (e.frozen) {                                // frozen: a missile shatters it, the beam glances off
      if (s.missile) { burst(e.x + e.w / 2, e.y + e.h / 2, 14, 5); killEnemy(e); }
      else { beep(2200, 6); spark(x, y, -sgn(s.vx) * 4, -3, 5); }
      return;
    }
    if (s.ice && !(e.kind == EN_METROID && e.st == 3)) {   // Ice Missile: freeze it solid (no damage)
      e.frozen = FREEZE_T; e.flash = 0; e.acc = 0; e.vy = 0;
      if (e.kind == EN_METROID) e.st = 0;
      beep(1800, 20); burst(x, y, 6, 3);
      return;
    }
    if (e.kind == EN_RIPPER && !s.missile) spark(x, y, -sgn(s.vx) * 4, -3, 5);
    damageEnemy(e, s.missile ? 4 : 1, s.missile ? HIT_MISSILE : HIT_BEAM, sgn(s.vx));
    return;
  }
}

// =====================================================================
// Pickups
// =====================================================================
// Major items (new abilities) sit in a Chozo statue's hands inside an orb that has to be shot
// open, and get the full ceremony. Tanks just float there; missile tanks only get a toast.
bool majorItem(char k) { return k == 'm' || k == 'l' || k == 'o' || k == 'j' || k == 'i'; }
bool orbClosed(uint8_t i) { return majorItem(items[i].kind) && !((orbOpen >> i) & 1); }

const char *itemName(char k) {
  switch (k) { case 'm': return "MORPH BALL"; case 'l': return "MISSILES"; case 'o': return "BOMBS"; case 'j': return "SPACE JUMP"; case 'i': return "ICE MISSILES"; case 't': return "MISSILE TANK"; default: return "ENERGY TANK"; }
}
const char *itemHint(char k) {
  switch (k) {
    case 'm': return "MODE TWICE: ROLL UP";
    case 'l': return "MODE+OK: SWITCH";
    case 'o': return "BALL, THEN OK: BOMB";
    case 'j': return "FLIP, JUMP AGAIN";
    case 'i': return "MISSILES FREEZE FOES";
    case 't': return "MISSILES +5";
    default: return "ENERGY +2";
  }
}
const char *itemHint2(char k) {
  switch (k) {
    case 'm': return "ROLL INTO TUNNELS";
    case 'l': return "OPENS RED DOORS";
    case 'o': return "BREAKS CRACKED ROCK";
    case 'j': return "THE FLIP HITS FOES";
    case 'i': return "MISSILE AGAIN: SHATTER";
    default: return "";
  }
}

void collectItem(uint8_t i) {
  itemsGot |= 1ULL << i;
  char k = items[i].kind;
  if (k == 'm') gear |= G_MORPH;
  if (k == 'l') { gear |= G_MISSILE; maxMissiles += 5; missiles = maxMissiles; }
  if (k == 'o') gear |= G_BOMB;
  if (k == 'j') gear |= G_SPACE;
  if (k == 'i') gear |= G_ICE;
  if (k == 't') { maxMissiles += 5; missiles += 5; }
  if (k == 'e') { maxEnergy += 2; energy = maxEnergy; }
  if (k == 't') {                     // small stuff: no stopping the game
    int16_t x = items[i].tx * TILE + 4, y = items[i].ty * TILE + 4;
    burst(x, y, 8, 4);
    showMsg("MISSILE TANK  +5", 50);
    watch::playTune(TUNE_TANK, 3);
    return;
  }
  msgT = 0; itemKind = k; itemT = 0; mode = M_ITEM;
  if (majorItem(k)) watch::playTune(TUNE_MAJOR, 9);
  else watch::playTune(TUNE_ITEM, 7);
}

void checkPickups() {
  int16_t x0, y0, x1, y1; samusBox(x0, y0, x1, y1);
  for (uint8_t i = 0; i < itemN; i++) {
    if ((itemsGot >> i) & 1) continue;
    int16_t ix = items[i].tx * TILE, iy = items[i].ty * TILE;
    if (!overlap(x0, y0, x1, y1, ix + 1, iy + 1, ix + 7, iy + 7)) continue;
    if (orbClosed(i)) { if (!msgT) showMsg("SHOOT THE ORB", 40); continue; }
    collectItem(i); return;
  }
  for (auto &d : drops) {
    if (!d.live) continue;
    if (overlap(x0, y0, x1, y1, d.x - 3, d.y - 3, d.x + 3, d.y + 3)) {
      d.live = false;
      if (d.kind == 1 && energy < maxEnergy) energy++;
      if (d.kind == 2) { missiles += 2; if (missiles > maxMissiles) missiles = maxMissiles; }
      beep(1300, 15);
    }
  }
}

// =====================================================================
// Bombs: drop in the ball with FIRE; after the fuse the blast breaks cracked rock ('B'), hurts
// enemies, and pops the ball upward if it's caught in it (the bomb jump - chain them to climb).
// =====================================================================
void dropBomb() {
  if (!(eg() & G_BOMB)) { beep(140, 15); return; }
  for (auto &b : bombs)
    if (!b.live) {
      b.live = true; b.t = 0;
      b.x = toPx(sX); b.y = toPx(sY) - 3;
      beep(900, 6);
      return;
    }
}

void breakBlock(int16_t tx, int16_t ty, char kind) {
  if (rawTile(tx, ty) != kind || isBroken(tx, ty) || brokenN >= MAX_BROKEN) return;
  brokenX[brokenN] = tx; brokenY[brokenN] = ty; brokenN++;
  int16_t x = tx * 8 + 4, y = ty * 8 + 4;
  spark(x, y, -3, -4, 9); spark(x, y, 3, -4, 9); spark(x, y, -2, 2, 7); spark(x, y, 2, 2, 7);
}

void explodeBomb(Bomb &b) {
  beep(110, 45);
  burst(b.x, b.y, 10, 5);
  for (int16_t ty = (b.y - BOMB_R) >> 3; ty <= (b.y + BOMB_R) >> 3; ty++)        // cracked rock in reach
    for (int16_t tx = (b.x - BOMB_R) >> 3; tx <= (b.x + BOMB_R) >> 3; tx++) breakBlock(tx, ty, 'B');
  for (auto &e : en) {                                                          // enemies
    if (!e.live || !overlap(b.x - BOMB_R, b.y - BOMB_R, b.x + BOMB_R, b.y + BOMB_R, e.x, e.y, e.x + e.w, e.y + e.h)) continue;
    damageEnemy(e, 2, HIT_BOMB, e.x + e.w / 2 > b.x ? 1 : -1);
  }
  for (auto &e : en)              // a Metroid latched onto Samus gets blasted off by a bomb at her feet
    if (e.live && e.kind == EN_METROID && e.st == 3 && abs(toPx(sX) - b.x) <= BOMB_R + 4 && abs(toPx(sY) - 3 - b.y) <= BOMB_R + 4)
      damageEnemy(e, 0, HIT_BOMB, toPx(sX) >= b.x ? 1 : -1);
  if (ball) {                     // bomb jump: the ball's centre within reach pops up, keeping its roll
    int16_t cx = toPx(sX), cy = toPx(sY) - 3;
    if (abs(cx - b.x) <= BOMB_R && abs(cy - b.y) <= BOMB_R) { sVY = -BOMB_JUMP_V; ground = false; coyote = 0; bombPop = true; }
  }
}

void bombsStep() {
  for (auto &b : bombs) {
    if (!b.live) continue;
    b.t++;
    if (b.t == BOMB_FUSE) explodeBomb(b);
    if (b.t >= BOMB_FUSE + BOMB_BLAST_T) b.live = false;
  }
}

// =====================================================================
// Play step
// =====================================================================
void handleInput(uint8_t e) {
  uint8_t h = watch::held();
  if (inputLock) {
    if (h == 0 || (inputLock != 255 && --inputLock == 0)) inputLock = 0;
    prevHeld = h; chord = false;
    return;
  }
  if (h & B_DOWN) { if (downHeldT < 255) downHeldT++; } else downHeldT = 0;
  // DOWN + FIRE together: tap = beam/missile, hold = pause
  if ((h & B_DOWN) && (h & B_FIRE) && (e & (B_DOWN | B_FIRE)) && !chord) {
    chord = true; chordT = 0;
    if (e & B_FIRE) { if (downHeldT <= 8) undoDown(); }   // Down came first: put the posture back
    e &= ~B_DOWN;                                         // Fire came first: Down doesn't act
  }
  if (chord) {
    if ((h & B_DOWN) && (h & B_FIRE)) {
      if (++chordT == CHORD_HOLD) {
        chord = false; pauseSel = 0; pauseTab = 0; gearSel = 0; mode = M_PAUSE; beep(400, 30);
        inputLock = 255; prevHeld = h; return;
      }
    } else {
      if (chordT < CHORD_HOLD) switchWeapon();
      chord = false;
      inputLock = 255;                                    // wait for both to be let go
    }
    prevHeld = h;
    return;
  }
  if (e & B_DOWN) doDown();
  if ((e & B_FIRE) && !(h & B_DOWN)) fire();
  if (e & B_UP) {
    if (ball) tryStand();
    else { jumpBuf = JUMP_BUF; jumpPress = true; }
  }
  jumpHeld = (h & B_UP) != 0;
  prevHeld = h;
}

void playStep(uint8_t e) {
  frameN++; playFrames++;
  handleInput(e);
  if (mode != M_PLAY) return;
  if (cardT) cardT--;
  if (modeLabelT) modeLabelT--;
  if (msgT) msgT--;

  uint8_t h = inputLock ? 0 : watch::held();
  int8_t dir = 0;
  if ((h & B_LEFT) && !(h & B_RIGHT)) dir = -1;
  if ((h & B_RIGHT) && !(h & B_LEFT)) dir = 1;

  bool spinAtStart = spin;                // a flip that lands this frame still hits what it lands on
  for (uint8_t step = 0; step < 2; step++) {
    samusStep(dir, step == 0);
    for (auto &s : shots) if (s.live) shotStep(s);
    for (auto &en1 : en) if (en1.live) enemyStep(en1);
    for (auto &b : bul) if (b.live) {
      if (b.kind == 3) {                        // Kraid's spike: falls, then skids along the floor
        b.vy += 24; if (b.vy > 700) b.vy = 700;
        b.x += b.vx;
        if (!--b.life || solidTile(toPx(b.x) >> 3, toPx(b.y) >> 3)) { b.live = false; continue; }
        fx ny = b.y + b.vy;
        if (solidTile(toPx(b.x) >> 3, (toPx(ny) + 2) >> 3)) { b.vy = 0; b.y = toFx(((toPx(ny) + 2) & ~7) - 3); }
        else b.y = ny;
        continue;
      }
      b.x += b.vx; b.y += b.vy;
      if (!--b.life || solidTile(toPx(b.x) >> 3, toPx(b.y) >> 3)) b.live = false;
    }
  }
  for (uint8_t i = 0; i < doorN; i++) {
    if (!doors[i].timer) continue;
    int16_t x0, y0, x1, y1; samusBox(x0, y0, x1, y1);
    bool inside = overlap(x0, y0, x1, y1, doors[i].tx * 8, doors[i].ty * 8, doors[i].tx * 8 + 8, (doors[i].ty + doors[i].h) * 8);
    if (!inside || doors[i].timer > 1) doors[i].timer--;
  }
  for (auto &d : drops) if (d.live && !--d.life) d.live = false;
  for (auto &s : sparks) if (s.life) { s.life--; s.x += s.vx / 2; s.y += s.vy / 2; }
  bombsStep();

  // contact damage
  int16_t x0, y0, x1, y1; samusBox(x0, y0, x1, y1);
  int16_t cx = (x0 + x1) / 2;
  bool spinAttack = (spin || spinAtStart) && (eg() & G_SPACE);
  for (auto &en1 : en) {
    if (!en1.live) continue;
    if (en1.frozen && !spinAttack) continue;      // frozen: harmless, and solid to stand on
    if (spinAttack && !isBoss(en1.kind) && overlap(x0 - 1, y0 - 1, x1 + 1, y1 + 1, en1.x, en1.y, en1.x + en1.w, en1.y + en1.h)) {
      if (en1.flash) continue;                // one hit per pass
      if (en1.kind == EN_METROID) { if (en1.st != 3) { en1.st = 4; en1.t = 30; en1.y -= 6; } continue; }
      burst(en1.x + en1.w / 2, en1.y + en1.h / 2, 6, 4);
      damageEnemy(en1, SPIN_DMG, HIT_SPIN, cx < en1.x + en1.w / 2 ? 1 : -1);
      en1.flash = 8;
      continue;
    }
    if (en1.kind == EN_METROID) {
      if (en1.st <= 2 && overlap(x0, y0, x1, y1, en1.x, en1.y, en1.x + en1.w, en1.y + en1.h)) {
        bool other = false; for (auto &o : en) if (&o != &en1 && o.live && o.kind == EN_METROID && o.st == 3) other = true;
        if (!other) { en1.st = 3; en1.t = 0; beep(150, 60); }
      }
      continue;
    }
    if (invul) continue;
    if (en1.kind == EN_SWIPE && en1.st == 2) {
      int16_t a0, b0, a1, b1; swipeBox(en1, a0, b0, a1, b1);
      if (overlap(x0, y0, x1, y1, a0, b0, a1, b1)) { hurt(en1.dx, 2); continue; }
    }
    if ((en1.kind == EN_KRAID || en1.kind == EN_RIDLEY || en1.kind == EN_MBRAIN) && en1.st == 5) continue;
    if (overlap(x0 + 1, y0 + 1, x1 - 1, y1, en1.x, en1.y, en1.x + en1.w, en1.y + en1.h))
      hurt(cx < en1.x + en1.w / 2 ? -1 : 1, isBoss(en1.kind) ? 2 : 1);
  }
  for (auto &b : bul) {
    if (!b.live || invul) continue;
    int16_t bx = toPx(b.x), by = toPx(b.y);
    if (overlap(x0, y0, x1, y1, bx - 1, by - 1, bx + 2, by + 2)) { b.live = false; hurt(b.vx > 0 ? 1 : -1, 1); }
  }
  if (mode != M_PLAY) return;
  checkPickups();
  if (mode != M_PLAY) return;

  // an open door with nothing built behind it yet
  for (uint8_t i = 0; i < doorN; i++) {
    if (!doorIsOpen(i)) continue;
    int16_t tx = doors[i].tx, ty = doors[i].ty;
    int16_t out = roomAtTile(tx - 1, ty) < 0 ? tx - 1 : roomAtTile(tx + 1, ty) < 0 ? tx + 1 : 0;
    if (!out) continue;
    if (overlap(x0 - 2, y0, x1 + 2, y1, tx * 8, ty * 8, tx * 8 + 8, (ty + doors[i].h) * 8) && !msgT)
      showMsg(sandbox ? "TEST DOOR" : tx - 1 == out ? "KRAID'S LAIR: NEXT BUILD" : "NORFAIR: NEXT BUILD", 50);
  }

  // crossed into the next room?
  int8_t r = roomAtPx(toPx(sX), toPx(sY) - bodyH() / 2);
  if (r >= 0 && r != curRoom) { pendingRoom = r; transitionTo(enterRoomNow, 6); }

  updateCamera();
}

// =====================================================================
// Drawing
// =====================================================================
inline int16_t scrX(int16_t wx) { return wx - camX; }
inline int16_t scrY(int16_t wy) { return wy - camY + HUD_H; }

inline uint32_t thash(int16_t x, int16_t y) {
  uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return h;
}

void drawRock(int16_t tx, int16_t ty, int16_t x, int16_t y, char c) {
  bool T = !rockish(tx, ty - 1), Bt = !rockish(tx, ty + 1), L = !rockish(tx - 1, ty), R = !rockish(tx + 1, ty);
  uint32_t h = thash(tx, ty);
  int8_t l = L ? 1 : 0, r = R ? 1 : 0, t = T ? 1 : 0, b = Bt ? 1 : 0;
  if (T) { hLineSafe(x + l, y, 8 - l - r); if (h & 1) hLineSafe(x + 2 + ((h >> 1) & 3), y + 1, 2); }
  if (Bt) { hLineSafe(x + l, y + 7, 8 - l - r); if (h & 8) plot(x + 3 + ((h >> 4) & 1), y + 6); }
  if (L) { vLineSafe(x, y + t, 8 - t - b); if (h & 32) plot(x + 1, y + 2 + ((h >> 6) & 3)); }
  if (R) { vLineSafe(x + 7, y + t, 8 - t - b); if (h & 256) plot(x + 6, y + 2 + ((h >> 9) & 3)); }
  if (c == '=') {                        // ledge: a banded slab
    for (int8_t i = 1; i < 7; i += 2) plot(x + i, y + 3);
  } else if (c == 'B') {                 // bomb block: jagged seam
    for (int8_t i = 0; i < 8; i++) plot(x + i, y + 3 + ((i & 2) ? 1 : 0) - ((i & 1) ? 1 : 0));
  } else if (c == 'X') {                 // Space Jump block: a diagonal slash
    lineSafe(x + 1, y + 6, x + 6, y + 1); lineSafe(x + 2, y + 6, x + 6, y + 2);
  } else if (c == 'G') {                 // locked hatch
    frameSafe(x, y, 8, 8); lineSafe(x + 1, y + 1, x + 6, y + 6); lineSafe(x + 6, y + 1, x + 1, y + 6);
  } else if (((h >> 12) & 7) == 0) {     // sparse rock grain
    plot(x + 2 + ((h >> 16) & 3), y + 2 + ((h >> 18) & 3));
    plot(x + 3 + ((h >> 16) & 3), y + 2 + ((h >> 18) & 3));
  } else if (((h >> 12) & 7) == 1) plot(x + 1 + ((h >> 20) & 5), y + 4 + ((h >> 23) & 1));
}

void drawDoor(uint8_t i) {
  const Door &d = doors[i];
  int16_t x = scrX(d.tx * 8), y = scrY(d.ty * 8), hpx = d.h * 8;
  if (x > 128 || x < -8 || y > 64 || y + hpx < HUD_H) return;
  // frame caps above and below
  fillBoxClipped(x, y, 8, 2); fillBoxClipped(x, y + hpx - 2, 8, 2);
  if (doorIsOpen(i)) return;
  // closed capsule
  frameSafe(x + 1, y + 2, 6, hpx - 4);
  if (d.kind == 'R') { vLineSafe(x + 3, y + 3, hpx - 6); vLineSafe(x + 4, y + 3, hpx - 6); }   // double bar
  else for (int16_t yy = y + 4; yy < y + hpx - 4; yy += 3) plot(x + 3 + ((yy >> 1) & 1), yy);
}

void drawShip() {
  int16_t cx = scrX(padTX * 8 + 4), ty = scrY(padTY * 8);
  if (cx < -40 || cx > 168 || ty > 72 || ty < -20) return;
  // upper hull
  hLineSafe(cx - 17, ty, 35);
  lineSafe(cx - 17, ty, cx - 21, ty + 7); lineSafe(cx + 17, ty, cx + 21, ty + 7);
  // canopy band
  for (int16_t i = -12; i <= 12; i += 2) plot(cx + i, ty + 3);
  hLineSafe(cx - 14, ty + 5, 29);
  // lower hull and wings
  hLineSafe(cx - 21, ty + 8, 43);
  lineSafe(cx - 21, ty + 8, cx - 28, ty + 13); lineSafe(cx + 21, ty + 8, cx + 28, ty + 13);
  hLineSafe(cx - 28, ty + 14, 57);
  lineSafe(cx - 10, ty + 8, cx - 6, ty + 14); lineSafe(cx + 10, ty + 8, cx + 6, ty + 14);
  // engines
  discSafe(cx - 22, ty + 11, 2); discSafe(cx + 22, ty + 11, 2);
  // landing struts
  vLineSafe(cx - 18, ty + 15, 1); vLineSafe(cx + 18, ty + 15, 1);
  // save pad lights
  bool near = onPad();
  u8g2.setDrawColor(0); hLineSafe(cx - 4, ty, 9); u8g2.setDrawColor(1);
  if (!near || (frameN & 4)) { plot(cx - 4, ty - 1); plot(cx + 4, ty - 1); }
  if (near) for (int8_t i = -3; i <= 3; i += 2) plot(cx + i, ty - 1);
}

void drawWorld() {
  int16_t tx0 = camX >> 3, ty0 = camY >> 3;
  // the surface has a starfield (parallax)
  if (ROOMS[curRoom].my == 0 && curRoom == 0) {
    for (uint8_t i = 0; i < 26; i++) {
      int16_t x = (int16_t)((i * 53 + 11 - camX / 4) & 127);
      int16_t y = (int16_t)((i * 29 + 5) % 112) - camY / 3 + HUD_H;
      if (y >= HUD_H && y < 64 && !solidTile((camX + x) >> 3, (camY + y - HUD_H) >> 3)) plot(x, y);
    }
  }
  for (int16_t ty = ty0; ty <= ty0 + 7; ty++)
    for (int16_t tx = tx0; tx <= tx0 + 16; tx++) {
      char c = rawTile(tx, ty);
      int16_t x = scrX(tx * 8), y = scrY(ty * 8);
      if (c == '#' || c == '=' || c == 'G' || ((c == 'B' || c == 'X') && !isBroken(tx, ty))) drawRock(tx, ty, x, y, c);
    }
  for (uint8_t i = 0; i < doorN; i++) drawDoor(i);
  drawShip();
}

// 8×8 item icons, drawn at 1× in the world and blown up on the pickup screen
static const uint8_t ICON_MORPH[8] = {      // ball with a glowing band
  0b00111100, 0b01111110, 0b11000011, 0b10111101, 0b10111101, 0b11000011, 0b01111110, 0b00111100};
static const uint8_t ICON_MISSILE[8] = {    // missile, nose up
  0b00011000, 0b00111100, 0b00100100, 0b00111100, 0b00111100, 0b01111110, 0b11011011, 0b10011001};
static const uint8_t ICON_MTANK[8] = {      // capsule with a little missile
  0b01111110, 0b10000001, 0b10011001, 0b10111101, 0b10011001, 0b10100101, 0b10000001, 0b01111110};
static const uint8_t ICON_BOMB[8] = {       // round bomb with a lit fuse
  0b00001010, 0b00010000, 0b00111100, 0b01111110, 0b01101111, 0b01111111, 0b01111110, 0b00111100};
static const uint8_t ICON_SPACE[8] = {      // spread wings
  0b10000001, 0b11000011, 0b11100111, 0b01111110, 0b00111100, 0b00011000, 0b00100100, 0b01000010};
static const uint8_t ICON_ICE[8] = {        // snowflake
  0b00011000, 0b01011010, 0b00111100, 0b11111111, 0b11111111, 0b00111100, 0b01011010, 0b00011000};
static const uint8_t ICON_ETANK[8] = {      // four-cell energy tank
  0b11111111, 0b10011001, 0b10011001, 0b11111111, 0b11111111, 0b10011001, 0b10011001, 0b11111111};
const uint8_t *itemIcon(char k) {
  switch (k) { case 'm': return ICON_MORPH; case 'l': return ICON_MISSILE; case 'o': return ICON_BOMB; case 'j': return ICON_SPACE; case 'i': return ICON_ICE; case 't': return ICON_MTANK; default: return ICON_ETANK; }
}
// icon centred on (cx, cy), each bit drawn as an s×s block
void drawIcon(char k, int16_t cx, int16_t cy, uint8_t s) {
  const uint8_t *b = itemIcon(k);
  int16_t x0 = cx - 4 * s, y0 = cy - 4 * s;
  for (uint8_t r = 0; r < 8; r++)
    for (uint8_t c = 0; c < 8; c++)
      if (b[r] & (0x80 >> c)) {
        if (s == 1) plot(x0 + c, y0 + r);
        else fillBoxClipped(x0 + c * s, y0 + r * s, s, s);
      }
}

// Seated Chozo statue (SPR_CHOZO) for the item tile centred on (ox, oy), 3 tiles above the floor:
// it sits on the floor 20 px below, and holds the orb up at (ox, oy - ORB_LIFT).
// dir +1 = statue sits left of the orb, facing right.
void drawChozo(int16_t ox, int16_t oy, int8_t dir) {
  drawFrame(SPR_CHOZO, ox, oy + 20, dir < 0);
}

void drawItems() {
  for (uint8_t i = 0; i < itemN; i++) {
    char k = items[i].kind;
    int16_t x = scrX(items[i].tx * 8 + 4), y = scrY(items[i].ty * 8 + 4);
    if (x < -24 || x > 152 || y < -8 || y > 80) continue;
    bool got = (itemsGot >> i) & 1;
    if (majorItem(k)) {
      drawChozo(x, y, items[i].face);         // stays once its gift is taken
    }
    if (got) continue;
    if (majorItem(k)) {
      y -= ORB_LIFT;                            // the orb sits up in the statue's hand
      if (orbClosed(i)) {
        // Chozo orb: ring with a dithered core, a highlight and a turning glint
        circleSafe(x, y, ORB_R);
        for (int8_t dy = -5; dy <= 5; dy++)
          for (int8_t dx = -5; dx <= 5; dx++)
            if (dx * dx + dy * dy <= 26 && ((dx + dy + (frameN >> 3)) & 1)) plot(x + dx, y + dy);
        hLineSafe(x - 3, y - 4, 2); plot(x - 4, y - 3);
        uint8_t g = (frameN >> 2) & 7;
        static const int8_t GX[8] = {0, 4, 5, 4, 0, -4, -5, -4}, GY[8] = {-5, -4, 0, 4, 5, 4, 0, -4};
        u8g2.setDrawColor(0); plot(x + GX[g], y + GY[g]); u8g2.setDrawColor(1);
      } else {
        // revealed: the item hovers and shines
        int16_t by = y - ((frameN >> 3) & 1);
        drawIcon(k, x, by, 1);
        uint8_t p = (frameN >> 1) & 7;
        if (p < 4) {
          int8_t L = 6 + p;
          plot(x, by - L); plot(x, by + L); plot(x - L, by); plot(x + L, by);
          plot(x - L + 2, by - L + 2); plot(x + L - 2, by - L + 2);
        }
      }
    } else {
      // tanks: bob, with an energy tank blinking its frame
      int16_t by = y - ((frameN >> 3) & 1);
      drawIcon(k, x, by, 1);
      if (k == 'e' && (frameN & 8)) frameSafe(x - 5, by - 5, 10, 10);
      if (k == 't' && ((frameN >> 2) & 7) == 0) plot(x + 4, by - 5);
    }
  }
  for (auto &d : drops) {
    if (!d.live || (d.life < 60 && (frameN & 2))) continue;
    int16_t x = scrX(d.x), y = scrY(d.y);
    if (d.kind == 1) { circleSafe(x, y, 2); plot(x, y); }
    else { hLineSafe(x - 2, y, 4); plot(x + 2, y - 1); plot(x + 2, y + 1); plot(x - 3, y - 1); plot(x - 3, y + 1); }
  }
}

void drawZoomer(const Enemy &e, int16_t x, int16_t y) {
  // 8×8 spiked dome sitting on its surface; the normal (nx, ny) says which way is "up"
  static const uint8_t Z[8] = {0x24, 0x5A, 0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0x66};
  uint8_t f = (frameN >> 2) & 1;
  for (uint8_t r = 0; r < 8; r++)
    for (uint8_t c = 0; c < 8; c++) {
      uint8_t row = Z[r];
      if (r == 0 && f) row = 0x42;
      if (!(row & (0x80 >> c))) continue;
      int8_t px = 0, py = 0;
      if (e.ny < 0) { px = c; py = r; }               // on a floor
      else if (e.ny > 0) { px = 7 - c; py = 7 - r; }  // on a ceiling
      else if (e.nx > 0) { px = 7 - r; py = c; }      // on a wall to its left
      else { px = r; py = 7 - c; }                    // on a wall to its right
      plot(x + px, y + py);
    }
}

// ice shell round a frozen enemy: a frame with spikes at the corners and mid-edges
void drawFrozenShell(int16_t x, int16_t y, uint8_t w, uint8_t h) {
  frameSafe(x - 2, y - 2, w + 4, h + 4);
  plot(x - 3, y - 3); plot(x + w + 2, y - 3); plot(x - 3, y + h + 2); plot(x + w + 2, y + h + 2);
  plot(x + w / 2, y - 3); plot(x + w / 2, y + h + 2); plot(x - 3, y + h / 2); plot(x + w + 2, y + h / 2);
}

// Mother Brain's tank: a base with the floor cannon, the brain, and the glass front (cracking)
void drawMBrain(const Enemy &e, int16_t x, int16_t y) {
  int16_t base = y + 32;
  fillBoxClipped(x + 2, base, 32, 8);                         // base
  u8g2.setDrawColor(0); hLineSafe(x + 4, base + 2, 28); hLineSafe(x + 4, base + 5, 28); u8g2.setDrawColor(1);
  bool tell = e.acc >= MB_CANNON && (frameN & 1);             // the cannon flashes before it fires
  fillBoxClipped(x - 2, base + 3, 5, tell ? 4 : 3);
  frameSafe(x + 2, y, 32, 33);                                 // the tank's frame
  hLineSafe(x + 4, y - 1, 28);
  drawFrame(mbrainEyeOpen(e) ? SPR_MBRAIN_EYE : SPR_MBRAIN, x + 6, y + 32, false);
  if (e.st == 0) {                                             // glass: reflections, then cracks
    for (int16_t k = 0; k < 3; k++) lineSafe(x + 5 + k * 3, y + 3, x + 3 + k * 3, y + 9);
    static const int8_t CR[6][4] = {{0,14,6,10},{0,14,5,20},{5,10,9,4},{5,20,10,25},{6,10,11,14},{9,4,13,2}};
    for (uint8_t k = 0; k < e.lost && k < 6; k++) lineSafe(x + 2 + CR[k][0], y + CR[k][1], x + 2 + CR[k][2], y + CR[k][3]);
    vLineSafe(x + 1, y + 1, 31);                               // the glass front
  }
}

void drawEnemies() {
  for (const auto &e : en) {
    if (!e.live) continue;
    int16_t x = scrX(e.x), y = scrY(e.y);
    if (x < -24 || x > 148 || y < -24 || y > 72) continue;
    if (e.flash & 2) { frameSafe(x - 1, y - 1, e.w + 2, e.h + 2); continue; }
    if (e.frozen) {
      if (e.frozen < THAW_WARN && (frameN & 2)) x += 1;          // shivering: about to thaw
      drawFrozenShell(x, y, e.w, e.h);
    }
    if (e.kind == EN_TURRET) {
      // a plate on the wall, a round body and a barrel that flashes before it fires
      int16_t px = e.dx > 0 ? x : x + 6;
      vLineSafe(px, y - 1, 8); vLineSafe(px + 1, y, 6);
      int16_t bx = e.dx > 0 ? x + 2 : x + 1;
      fillBoxClipped(bx, y, 5, 6);
      u8g2.setDrawColor(0); plot(bx + 2, y + 2); u8g2.setDrawColor(1);
      bool tell = e.st == 1 && (frameN & 1);
      int16_t mx = e.dx > 0 ? x + 7 : x - 3;
      hLineSafe(mx, y + 2, tell ? 5 : 4); if (tell) hLineSafe(mx, y + 3, 4);
      continue;
    }
    if (e.kind == EN_SWIPE || e.kind == EN_BOLT) {
      const SprFrame *f = &SPR_PIRATE;
      if ((e.kind == EN_SWIPE && e.st == 1)) f = &SPR_PIRATE_RAISE;
      if ((e.kind == EN_SWIPE && e.st == 2) || (e.kind == EN_BOLT && e.st == 1)) f = &SPR_PIRATE_REACH;
      int16_t bob = (e.kind == EN_SWIPE && e.st == 0 && (e.lost & 8)) ? 1 : 0;
      drawFrame(*f, x + e.w / 2, y + e.h + bob, e.dx < 0);
      if (e.kind == EN_BOLT && e.st == 1 && (frameN & 2)) {     // muzzle glints before the bolt
        int16_t mx = e.dx > 0 ? x + e.w + 7 : x - 8;
        plot(mx, y + 4); plot(mx, y + 6); plot(mx - 1, y + 5); plot(mx + 1, y + 5);
      }
      continue;
    }
    if (e.kind == EN_KRAID) {
      if (e.st == 5 && (frameN & 2)) continue;  // coming apart: flickers
      const SprFrame *f = e.st == 3 ? &SPR_KRAID_ROAR : &SPR_KRAID;
      int16_t shake = (e.st == 4 || e.st == 5) ? ((frameN & 1) ? 1 : -1) : 0;
      drawFrame(*f, x + shake, y + e.h + (e.st == 1 ? 2 : 0), false);   // bracing: he hunches down
      continue;
    }
    if (e.kind == EN_MBRAIN) {
      if (e.st == 5 && (frameN & 2)) continue;
      int16_t shake = e.st == 5 ? ((frameN & 1) ? 1 : -1) : 0;
      drawMBrain(e, x + shake, y);
      continue;
    }
    if (e.kind == EN_RIDLEY) {
      if (e.st == 5 && (frameN & 2)) continue;
      bool landed = e.st == 3 || e.st == 5;
      int16_t ax = scrX(ridleyArtX(e)), wob = e.st == 1 ? ((frameN & 2) ? -2 : 1) : 0;   // frantic flapping
      bool fl = ridleyFlip(e);                  // mirrored art pivots on its anchor: pass its right edge
      if (landed) drawFrame(SPR_RIDLEY, fl ? ax + SPR_RIDLEY.w - 1 : ax, y + 40, fl);
      else drawFrame(SPR_RIDLEY_FLY, fl ? ax + SPR_RIDLEY_FLY.w - 1 : ax, y - 2 + 28 + wob, fl);
      continue;
    }
    if (e.kind == EN_METROID) {
      uint8_t ph = e.st == 1 ? (frameN & 1) : (e.st == 3 ? (frameN >> 1) & 1 : (frameN >> 3) & 1);
      drawFrame(*SPR_METROID[ph], x + 6, y + 10, false);
      if (e.st == 3 && (frameN & 4)) { plot(x + 2, y + 11); plot(x + 9, y + 11); }   // draining
      continue;
    }
    if (e.kind == EN_ZOOMER) drawZoomer(e, x, y);
    else if (e.kind == EN_SKREE) {
      int8_t sh = (e.st == 2) ? ((frameN & 1) ? 1 : -1) : 0;
      x += sh;
      // body hanging point-down; wings flare while diving
      hLineSafe(x + 1, y, 6);
      fillBoxClipped(x + 2, y + 1, 4, 5);
      plot(x + 3, y + 6); plot(x + 4, y + 6); plot(x + 3, y + 7); plot(x + 4, y + 7);
      plot(x + 3, y + 9); plot(x + 4, y + 8);
      u8g2.setDrawColor(0); plot(x + 3, y + 3); u8g2.setDrawColor(1);
      if (e.st == 1) { bool f = frameN & 2; lineSafe(x + 2, y + 1, x - 1, y + (f ? 0 : 4)); lineSafe(x + 5, y + 1, x + 8, y + (f ? 0 : 4)); }
      else { vLineSafe(x + 1, y + 1, 4); vLineSafe(x + 6, y + 1, 4); }
    } else if (e.kind == EN_RIPPER) {
      // armoured shell with a beak in the travel direction
      hLineSafe(x + 2, y, 8); hLineSafe(x + 1, y + 1, 10);
      fillBoxClipped(x, y + 2, 12, 2);
      hLineSafe(x + 1, y + 4, 10); hLineSafe(x + 3, y + 5, 6);
      u8g2.setDrawColor(0); hLineSafe(x + 2, y + 2, 8); u8g2.setDrawColor(1);
      int16_t bx = e.dx > 0 ? x + 12 : x - 2;
      hLineSafe(bx, y + 2, 2);
      plot(e.dx > 0 ? x + 9 : x + 2, y + 1);
    }
  }
  for (const auto &b : bul) {
    if (!b.live) continue;
    int16_t x = scrX(toPx(b.x)), y = scrY(toPx(b.y));
    if (b.kind == 4) {                       // Mother Brain's ring
      circleSafe(x, y, 2 + ((frameN >> 2) & 1));
    } else if (b.kind == 3) {                // Kraid's spike: a claw pointing the way it flies
      hLineSafe(x - 2, y, 5); plot(x - 1, y - 1); plot(x - 1, y + 1); plot(x + 2, y - 1);
    } else if (b.kind == 2) {                // pirate bolt: a crackling dash
      hLineSafe(x - 3, y, 7); plot(x + ((frameN & 2) ? -2 : 2), y - 1); plot(x + ((frameN & 2) ? 1 : -1), y + 1);
    } else if (b.kind == 1) {                // turret shot: a small diamond
      plot(x, y - 1); hLineSafe(x - 1, y, 3); plot(x, y + 1);
    } else { plot(x, y); plot(x + 1, y); plot(x, y + 1); plot(x + 1, y + 1); }
  }
}

void drawShots() {
  for (const auto &s : shots) {
    if (!s.live) continue;
    int16_t x = scrX(toPx(s.x)), y = scrY(toPx(s.y));
    int8_t d = sgn(s.vx);
    if (s.missile) {
      if (d > 0) { hLineSafe(x - 5, y, 6); plot(x + 1, y); plot(x - 5, y - 1); plot(x - 5, y + 1); if (frameN & 1) plot(x - 7, y); }
      else { hLineSafe(x, y, 6); plot(x - 1, y); plot(x + 5, y - 1); plot(x + 5, y + 1); if (frameN & 1) plot(x + 7, y); }
      plot(x, y - 1); plot(x, y + 1);
      if (s.ice) { int16_t n = x + d * 3; plot(n, y - 2); plot(n, y + 2); plot(n + d, y - 1); plot(n + d, y + 1); }
    } else {
      hLineSafe(d > 0 ? x - 3 : x, y, 4);
      plot(x, y - 1); plot(x, y + 1);
    }
  }
  for (const auto &s : sparks) if (s.life) plot(scrX(s.x), scrY(s.y));
  for (const auto &b : bombs) {
    if (!b.live) continue;
    int16_t x = scrX(b.x), y = scrY(b.y);
    if (b.t < BOMB_FUSE) {
      // a little round bomb; it blinks faster as the fuse runs down
      uint8_t left = BOMB_FUSE - b.t;
      bool lit = left > 9 ? ((b.t >> 2) & 1) : left > 4 ? ((b.t >> 1) & 1) : (b.t & 1);
      hLineSafe(x - 1, y - 2, 3); hLineSafe(x - 2, y - 1, 5); hLineSafe(x - 2, y, 5); hLineSafe(x - 2, y + 1, 5); hLineSafe(x - 1, y + 2, 3);
      if (!lit) { u8g2.setDrawColor(0); plot(x, y); u8g2.setDrawColor(1); }
    } else {
      // blast: a ring that grows to the reach, with a bright core at first
      uint8_t k = b.t - BOMB_FUSE;
      int16_t r = 3 + k * (BOMB_R - 3) / (BOMB_BLAST_T - 1);
      circleSafe(x, y, r);
      if (k < 3) discSafe(x, y, 4 - k);
      else if (k < 5) circleSafe(x, y, r - 3);
    }
  }
}

// Draw a sprite frame with its body centre at x and its last row on the line above feet
void drawFrame(const SprFrame &f, int16_t x, int16_t feet, bool flip) {
  uint8_t rb = (f.w + 7) / 8;
  int16_t left = x - (flip ? f.w - 1 - f.ax : f.ax), top = feet - f.h;
  for (uint8_t r = 0; r < f.h; r++)
    for (uint8_t c = 0; c < f.w; c++)
      if (f.bits[r * rb + (c >> 3)] & (0x80 >> (c & 7))) plot(left + (flip ? f.w - 1 - c : c), top + r);
}

void drawBall(int16_t cx, int16_t feet) {
  uint8_t ph = runPhase & 3;
  drawFrame(*SPR_BALL[faceR ? ph : 3 - ph], cx, feet, false);
}

void drawSpin(int16_t cx, int16_t cy) {
  // somersault: the knees-up jump pose turned a quarter every 4 steps — a forward flip
  const SprFrame &f = SPR_JUMP;
  uint8_t rb = (f.w + 7) / 8, q = (spinPhase >> 2) & 3;
  for (uint8_t r = 0; r < f.h; r++)
    for (uint8_t c = 0; c < f.w; c++) {
      if (!(f.bits[r * rb + (c >> 3)] & (0x80 >> (c & 7)))) continue;
      int16_t x = c - f.w / 2, y = r - f.h / 2, t;
      for (uint8_t i = 0; i < q; i++) { t = x; x = -y; y = t; }   // rotate 90° clockwise
      if (!faceR) x = -x;                                          // mirrored: flips the other way
      plot(cx + x, cy + y);
    }
}

void drawSamus() {
  if (invul && ((invul >> 2) & 1)) return;
  int16_t x = scrX(toPx(sX)), feet = scrY(toPx(sY));
  if (ball) { drawBall(x, feet); return; }
  if (spin) {
    drawSpin(x, feet - 11);
    if (eg() & G_SPACE) {                  // Space Jump: a ring of sparks turns round the flip
      for (uint8_t i = 0; i < 6; i++) {
        float ang = (i * 60 + spinPhase * 23) * 0.01745f;
        plot(x + (int16_t)(cosf(ang) * 12), feet - 11 + (int16_t)(sinf(ang) * 12));
      }
    }
    return;
  }
  const SprFrame *f = &SPR_STAND;
  if (crouch) f = &SPR_CROUCH;
  else if (!ground) f = &SPR_JUMP;
  else if (sVX > 64 || sVX < -64) f = SPR_RUN[runPhase % 3];
  drawFrame(*f, x, feet, !faceR);
}

void print3(uint16_t v) { if (v < 100) u8g2.print('0'); if (v < 10) u8g2.print('0'); u8g2.print(v); }

void drawHud() {
  u8g2.setDrawColor(0); u8g2.drawBox(0, 0, 128, HUD_H); u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setCursor(0, 6); u8g2.print(F("EN"));
  for (uint8_t i = 0; i < maxEnergy; i++) {
    int16_t x = 10 + i * 5;
    if (i < energy) fillBoxClipped(x, 1, 4, 5);
    else frameSafe(x, 1, 4, 5);
  }
  if (gear & G_MISSILE) {
    int16_t x = 96;
    if (weapon == 1) { fillBoxClipped(x - 1, 0, 31, 7); u8g2.setDrawColor(0); }
    // tiny missile icon
    hLineSafe(x + 1, 3, 6); plot(x + 7, 3); plot(x, 2); plot(x, 4); plot(x + 2, 2); plot(x + 2, 4);
    u8g2.setCursor(x + 11, 6); print3(missiles);
    u8g2.setDrawColor(1);
  }
  for (const auto &e : en) {                // a boss in the room: its health under the HUD centre
    if (!e.live || !isBoss(e.kind) || modeLabelT) continue;
    uint8_t mx = e.kind == EN_RIDLEY ? RIDLEY_HP : e.kind == EN_MBRAIN ? MB_HP : KRAID_HP;
    if (e.kind == EN_MBRAIN && e.st == 0) {   // the glass first: its own bar
      frameSafe(60, 1, 28, 5); for (int16_t k = 0; k < MB_GLASS - e.lost; k++) fillBoxClipped(61 + k * 4 + 1, 2, 3, 3);
      continue;
    }
    frameSafe(60, 1, 28, 5);
    fillBoxClipped(61, 2, (int16_t)26 * e.hp / mx, 3);
  }
  if (modeLabelT) {
    u8g2.setCursor(64, 6); u8g2.print(!weapon ? F("BEAM") : (eg() & G_ICE) ? F("ICE") : F("MISSILE"));
  }
}

void drawCenteredText(const char *s, int16_t y) {
  int16_t w = u8g2.getStrWidth(s);
  u8g2.setCursor((128 - w) / 2, y); u8g2.print(s);
}

void drawPlay() {
  drawWorld();
  drawItems();
  drawEnemies();
  drawShots();
  if (mode != M_DEAD || deadT < 20 || (deadT < 30 && (deadT & 1))) drawSamus();
  drawHud();
  u8g2.setFont(u8g2_font_4x6_tr);
  if (msgT && msgText) {
    int16_t w = u8g2.getStrWidth(msgText);
    u8g2.setDrawColor(0); u8g2.drawBox((128 - w) / 2 - 2, 56, w + 4, 8); u8g2.setDrawColor(1);
    drawCenteredText(msgText, 62);
  } else if (onPad() && mode == M_PLAY) {
    u8g2.setDrawColor(0); u8g2.drawBox(40, 56, 48, 8); u8g2.setDrawColor(1);
    drawCenteredText("MODE: SAVE", 62);
  }
  if (cardT) {      // zone card slides in from the right and off to the left
    int16_t off = cardT > 60 ? (cardT - 60) * 13 : cardT < 12 ? -(12 - cardT) * 11 : 0;
    u8g2.setFont(u8g2_font_6x10_tr);
    const char *zone = sandbox ? "TEST ROOM" : "BRINSTAR";
    int16_t w = u8g2.getStrWidth(zone);
    int16_t x = (128 - w) / 2 + off;
    u8g2.setDrawColor(0); u8g2.drawBox(x - 6, 22, w + 12, 14); u8g2.setDrawColor(1);
    frameSafe(x - 6, 22, w + 12, 14);
    u8g2.setCursor(x, 33); u8g2.print(zone);
  }
}

// ---------------------------------------------------------------- item fanfare
// Major items: the screen flashes, light pulls in to Samus, then a panel with the item drawn big.
// Energy tanks skip straight to the panel.
const uint8_t CEREMONY_T = 28;
void itemStep(uint8_t e) {
  if (itemT < 255) itemT++;
  bool major = majorItem(itemKind);
  uint8_t minT = major ? CEREMONY_T + 30 : 30;
  if (itemT > minT && (e & (B_UP | B_FIRE | B_DOWN)) && !watch::tunePlaying()) { mode = M_PLAY; inputLock = 4; }
  if (itemT > (major ? 220 : 150)) { mode = M_PLAY; inputLock = 4; }
}
void drawItemScreen() {
  bool major = majorItem(itemKind);
  int16_t sx = scrX(toPx(sX)), sy = scrY(toPx(sY)) - (ball ? 4 : 11);
  if (major && itemT < CEREMONY_T) {
    // three white flashes with Samus in black, then rings of light closing in on her
    if (itemT < 12 && (itemT & 3) < 2) {
      u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, 64);
      u8g2.setDrawColor(0); drawSamus(); u8g2.setDrawColor(1);
      return;
    }
    drawPlay();
    int16_t R = (CEREMONY_T - itemT) * 5;
    for (uint8_t a = 0; a < 12; a++) {
      float ang = a * 0.5236f + itemT * 0.08f;
      int16_t c = (int16_t)(cosf(ang) * 64), s = (int16_t)(sinf(ang) * 64);
      lineSafe(sx + c * R / 64, sy + s * R / 64, sx + c * (R + 8) / 64, sy + s * (R + 8) / 64);
    }
    if (R > 4) circleSafe(sx, sy, R);
    return;
  }
  drawPlay();
  uint8_t t = major ? itemT - CEREMONY_T : itemT;
  // the panel opens from the middle
  int16_t h = t < 8 ? 6 + t * 5 : 46;
  int16_t top = 36 - h / 2;
  u8g2.setDrawColor(0); u8g2.drawBox(4, top, 120, h); u8g2.setDrawColor(1);
  frameSafe(4, top, 120, h); frameSafe(6, top + 2, 116, h - 4);
  if (t < 8) return;
  // big icon in a frame on the left, with a glint running round it
  frameSafe(9, 21, 30, 30);
  drawIcon(itemKind, 24, 36, 3);
  uint8_t g = (t >> 1) % 28;
  int16_t gx = g < 7 ? 10 + g * 4 : g < 14 ? 38 : g < 21 ? 38 - (g - 14) * 4 : 10;
  int16_t gy = g < 7 ? 21 : g < 14 ? 22 + (g - 7) * 4 : g < 21 ? 50 : 50 - (g - 21) * 4;
  u8g2.setDrawColor(0); plot(gx, gy); u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(44, 30); u8g2.print(itemName(itemKind));
  u8g2.setFont(u8g2_font_4x6_tr);
  if (t > 14) { u8g2.setCursor(44, 41); u8g2.print(itemHint(itemKind)); }
  if (t > 20) { u8g2.setCursor(44, 49); u8g2.print(itemHint2(itemKind)); }
}

// ---------------------------------------------------------------- death
void deadStep(uint8_t) {
  deadT++;
  if (deadT == 30) {
    int16_t x = toPx(sX), y = toPx(sY) - 10;
    burst(x, y, 16, 7); burst(x, y, 10, 3);
  }
  for (auto &s : sparks) if (s.life) { s.life--; s.x += s.vx / 2; s.y += s.vy / 2; }
  if (deadT == 70) transitionTo(respawn, 14);
}

// ---------------------------------------------------------------- pause: map + status
// Pause: Left/Right switch tabs (MAP, ITEMS), Action moves the cursor, OK picks, Mode resumes.
// ITEMS lists the gear Samus owns; each can be switched ON or OFF (Ice sits under Missiles, Bombs
// under the Morph Ball and are off while their parent is).
struct GearRow { uint8_t bit, parent; const char *name; };
static const GearRow GEAR_ROWS[] = {
  {G_MISSILE, 0, "MISSILES"}, {G_ICE, G_MISSILE, "ICE"}, {G_MORPH, 0, "MORPH BALL"},
  {G_BOMB, G_MORPH, "BOMBS"}, {G_SPACE, 0, "SPACE JUMP"}};
uint8_t ownedRows(uint8_t *out) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < sizeof(GEAR_ROWS) / sizeof(GEAR_ROWS[0]); i++) if (gear & GEAR_ROWS[i].bit) out[n++] = i;
  return n;
}

void toggleGear(uint8_t bit) {
  bool turningOff = !(gearOff & bit);
  if (bit == G_MORPH && turningOff && ball) {
    if (!canStand()) { pauseMsgT = 45; beep(140, 20); return; }   // in a tunnel: stay rolled up
    ball = false;
  }
  gearOff ^= bit;
  if (!(eg() & G_MISSILE)) weapon = 0;
  beep(turningOff ? 500 : 900, 12);
}

void pauseStep(uint8_t e) {
  if (inputLock) { if (watch::held() == 0) inputLock = 0; return; }
  if (pauseMsgT) pauseMsgT--;
  uint8_t rows[5], n = ownedRows(rows);
  if ((e & (B_LEFT | B_RIGHT)) && n) { pauseTab ^= 1; beep(700, 8); }
  if (e & B_DOWN) { mode = M_PLAY; inputLock = 255; return; }
  if (pauseTab == 0) {
    if (e & B_UP) { pauseSel ^= 1; beep(700, 8); }
    if (e & B_FIRE) {
      if (pauseSel == 0) { mode = M_PLAY; inputLock = 255; beep(600, 15); }
      else { if (!sandbox) captureSave(lastSave); watch::exitToClock(); }
    }
  } else {
    if (gearSel >= n) gearSel = 0;
    if (e & B_UP) { gearSel = (gearSel + 1) % n; beep(700, 8); }
    if (e & B_FIRE) toggleGear(GEAR_ROWS[rows[gearSel]].bit);
  }
}

void drawPauseTabs() {
  uint8_t rows[5];
  if (!ownedRows(rows)) return;
  const char *t[2] = {"MAP", "ITEMS"};
  int16_t x = 82;
  for (uint8_t i = 0; i < 2; i++) {
    int16_t w = u8g2.getStrWidth(t[i]);
    if (pauseTab == i) { fillBoxClipped(x - 2, 0, w + 4, 8); u8g2.setDrawColor(0); }
    u8g2.setCursor(x, 6); u8g2.print(t[i]); u8g2.setDrawColor(1);
    x += w + 8;
  }
}

void drawPauseItems() {
  u8g2.setCursor(2, 6); u8g2.print(F("ITEMS"));
  uint8_t rows[5], n = ownedRows(rows);
  for (uint8_t k = 0; k < n; k++) {
    const GearRow &g = GEAR_ROWS[rows[k]];
    int16_t y = 13 + k * 9;
    bool sel = k == gearSel;
    if (sel) { fillBoxClipped(2, y - 1, 124, 9); u8g2.setDrawColor(0); }
    drawIcon(g.bit == G_MISSILE ? 'l' : g.bit == G_ICE ? 'i' : g.bit == G_MORPH ? 'm' : g.bit == G_BOMB ? 'o' : 'j',
             (g.parent ? 18 : 8), y + 3, 1);
    u8g2.setCursor(g.parent ? 26 : 16, y + 6); u8g2.print(g.name);
    bool parentOff = g.parent && (gearOff & g.parent);
    u8g2.setCursor(104, y + 6);
    if (parentOff) u8g2.print(F("--"));
    else u8g2.print((gearOff & g.bit) ? F("OFF") : F("ON"));
    u8g2.setDrawColor(1);
  }
  if (pauseMsgT) { u8g2.setDrawColor(0); u8g2.drawBox(36, 55, 56, 9); u8g2.setDrawColor(1); drawCenteredText("NEED ROOM", 62); }
}

void drawPause() {
  u8g2.setFont(u8g2_font_4x6_tr);
  frameN++;
  drawPauseTabs();
  if (pauseTab == 1) { drawPauseItems(); return; }
  u8g2.setCursor(2, 6); u8g2.print(sandbox ? F("TEST ROOM") : F("BRINSTAR"));
  // map: one cell per screen, 7×5 px
  const int16_t ox = 4, oy = 10, cw = 7, chh = 5;
  for (uint8_t r = 0; r < ROOM_N; r++) {
    if (!((visited >> r) & 1) || ROOMS[r].zone == ZONE_TEST) continue;
    int16_t x = ox + ROOMS[r].mx * cw, y = oy + ROOMS[r].my * chh;
    int16_t w = ROOMS[r].sw * cw + 1, h = ROOMS[r].sh * chh + 1;
    if (r == curRoom && (frameN & 8)) fillBoxClipped(x, y, w, h);
    else frameSafe(x, y, w, h);
    if (r == roomAtTile(padTX, padTY)) { u8g2.setCursor(x + 2, y + h - 1); if (r != curRoom || !(frameN & 8)) u8g2.print('S'); }
  }
  // status
  u8g2.setCursor(56, 16); u8g2.print(ROOMS[curRoom].name);
  u8g2.setCursor(56, 26); u8g2.print(F("ENERGY ")); u8g2.print(energy); u8g2.print('/'); u8g2.print(maxEnergy);
  if (gear & G_MISSILE) { u8g2.setCursor(56, 34); u8g2.print(F("MISSILE ")); u8g2.print(missiles); u8g2.print('/'); u8g2.print(maxMissiles); }
  uint32_t sec = playFrames / 30;
  u8g2.setCursor(56, 42); u8g2.print(F("TIME "));
  watch::print2(sec / 3600); u8g2.print(':'); watch::print2((sec / 60) % 60); u8g2.print(':'); watch::print2(sec % 60);
  // options (Action moves, OK picks)
  u8g2.setFont(u8g2_font_5x7_tr);
  const char *opt[2] = {"RESUME", "CLOCK"};
  for (uint8_t i = 0; i < 2; i++) {
    int16_t x = 56 + i * 38;
    if (pauseSel == i) { fillBoxClipped(x - 2, 52, 36, 10); u8g2.setDrawColor(0); }
    u8g2.setCursor(x, 60); u8g2.print(opt[i]);
    u8g2.setDrawColor(1);
  }
}

// ---------------------------------------------------------------- title + opening
void titleStep(uint8_t e) {
  frameN++;
  uint8_t h = watch::held();
  if ((h & B_LEFT) && (h & B_RIGHT)) {
    if (++titleHoldT == 45) { beep(1200, 40); watch::stopTune(); transitionTo(startSandbox, 14); }
    return;
  }
  titleHoldT = 0;
  if (haveSave && (e & (B_LEFT | B_RIGHT | B_DOWN))) { titleSel ^= 1; beep(700, 8); }
  if (e & (B_UP | B_FIRE)) {
    watch::stopTune();
    if (haveSave && titleSel == 0) transitionTo(continueGame, 14);
    else { storyPage = 0; storyT = 0; transitionTo([] { mode = M_STORY; }, 14); }
  }
}

void drawTitle() {
  for (uint8_t i = 0; i < 30; i++) {
    int16_t x = (i * 37 + (frameN >> 3) * (i & 1 ? 1 : 0)) & 127, y = (i * 23) % 64;
    plot(x, y);
  }
  // Zebes on the horizon
  for (int16_t x = 0; x < 128; x++) {
    int16_t y = 64 - (int16_t)(sqrtf(max(0.0f, 9000.0f - (x - 64) * (x - 64))) - 82);
    if (y < 64) plot(x, y);
  }
  u8g2.setFont(u8g2_font_ncenB18_tr);
  const char *t = "METROID";
  int16_t w = u8g2.getStrWidth(t);
  u8g2.setCursor((128 - w) / 2, 26); u8g2.print(t);
  hLineSafe((128 - w) / 2, 29, w);
  u8g2.setFont(u8g2_font_4x6_tr);
  drawCenteredText("WATCH MISSION", 37);
  u8g2.setFont(u8g2_font_5x7_tr);
  if (haveSave) {
    const char *o[2] = {"CONTINUE", "NEW GAME"};
    for (uint8_t i = 0; i < 2; i++) {
      int16_t x = 8 + i * 62, ww = u8g2.getStrWidth(o[i]);
      if (titleSel == i) { fillBoxClipped(x - 2, 44, ww + 4, 9); u8g2.setDrawColor(0); }
      u8g2.setCursor(x, 51); u8g2.print(o[i]); u8g2.setDrawColor(1);
    }
  } else if ((frameN >> 4) & 1) drawCenteredText("PUSH JUMP", 52);
}

static const char *const STORY[3][4] = {
  {"ZEBES ORBIT", "ON YOUR LAST VISIT,", "YOU SHUT DOWN A PIRATE", "RELAY BELOW BRINSTAR."},
  {"INCOMING SIGNAL", "THE RELAY IS LIVE AGAIN.", "IT IS BROADCASTING", "YOUR OLD DISTRESS CODE."},
  {"MISSION", "LAND AT THE OLD SITE.", "TRACE THE SIGNAL.", "FIND OUT WHO SENT IT."},
};

void storyStep(uint8_t e) {
  frameN++;
  if (storyT < 200) storyT += 2;
  if ((storyT & 7) == 0 && storyT < 70) beep(1500, 3);
  if (e & B_DOWN) { transitionTo(newGame, 14); return; }        // skip to the landing
  if (e & (B_UP | B_FIRE)) {
    if (storyT < 70) storyT = 200;                                // finish the text
    else if (storyPage < 2) { storyPage++; storyT = 0; beep(900, 10); }
    else transitionTo(newGame, 14);
  }
}

void drawStory() {
  u8g2.setFont(u8g2_font_5x7_tr);
  const char *const *p = STORY[storyPage];
  drawCenteredText(p[0], 10);
  hLineSafe(20, 13, 88);
  uint8_t shown = storyT;
  for (uint8_t l = 1; l < 4; l++) {
    const char *s = p[l]; uint8_t n = strlen(s);
    char buf[28]; uint8_t k = n < shown ? n : shown; if (k > 27) k = 27;
    memcpy(buf, s, k); buf[k] = 0;
    shown = shown > n ? shown - n : 0;
    u8g2.setCursor((128 - (int16_t)u8g2.getStrWidth(s)) / 2, 18 + l * 10); u8g2.print(buf);
  }
  if (storyT >= 70 && ((frameN >> 3) & 1)) { u8g2.setFont(u8g2_font_4x6_tr); u8g2.setCursor(118, 63); u8g2.print('>'); }
}

// =====================================================================
// Fade
// =====================================================================
void applyFade(uint8_t level, uint8_t steps) {
  static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  uint8_t thr = (uint8_t)(level * 16 / steps);
  uint8_t mask[4];
  for (uint8_t xm = 0; xm < 4; xm++) {
    uint8_t m = 0;
    for (uint8_t b = 0; b < 8; b++) if (BAYER[b & 3][xm] < thr) m |= (uint8_t)(1 << b);
    mask[xm] = m;
  }
  uint8_t *buf = u8g2.getBufferPtr();
  for (uint16_t i = 0; i < 8 * 128; i++) buf[i] &= (uint8_t)~mask[i & 3];
}

// =====================================================================
// WatchCore hooks
// =====================================================================
void gameStart() {
  static bool eepromReady = false;
  if (!eepromReady) { EEPROM.begin(256); eepromReady = true; }
  buildWorld();
  haveSave = readSave();
  titleSel = 0; frameN = 0;
  trState = TR_NONE; trLevel = 0; trAction = nullptr;
  mode = M_TITLE;
  watch::playTune(TUNE_TITLE, 7);
}

void gameUpdate(uint8_t e) {
  if (trState == TR_OUT) {
    if (++trLevel >= trSteps) {
      trLevel = trSteps; trState = TR_IN;
      if (trAction) { void (*a)() = trAction; trAction = nullptr; a(); }
    }
    return;
  }
  if (trState == TR_IN) {
    if (trLevel) trLevel--;
    if (!trLevel) trState = TR_NONE;
    e = 0;
  }
  switch (mode) {
    case M_TITLE: titleStep(e); break;
    case M_STORY: storyStep(e); break;
    case M_PLAY: playStep(e); break;
    case M_ITEM: itemStep(e); break;
    case M_PAUSE: pauseStep(e); break;
    case M_DEAD: deadStep(e); break;
  }
}

void gameDraw() {
  switch (mode) {
    case M_TITLE: drawTitle(); break;
    case M_STORY: drawStory(); break;
    case M_PLAY: case M_DEAD: drawPlay(); break;
    case M_ITEM: drawItemScreen(); break;
    case M_PAUSE: drawPause(); break;
  }
  if (trLevel) applyFade(trLevel, trSteps);
}

// Alarm interrupted play: come back to the pause screen
void gameInterrupted() {
  if (mode == M_PLAY) { pauseSel = 0; pauseTab = 0; gearSel = 0; mode = M_PAUSE; inputLock = 255; }
}

// -------------------- Theme: Metroid clock face --------------------
int16_t wfX = -8;
uint8_t wfAnim = 0;

void stepWatchFace() {
  wfAnim++;
  wfX += 1;
  if (wfX > 140) wfX = -10;
}

void drawView() {
  using namespace watch;
  u8g2.setDrawColor(1);
  for (uint8_t i = 0; i < 16; i++)
    plot((i * 41 + wfAnim) & 127, 8 + (i * 7) % 18);
  u8g2.setFont(u8g2_font_logisoso24_tn);
  u8g2.setCursor(28, 36); print2(hours()); u8g2.print(':'); print2(minutes());
  drawFrame(*SPR_BALL[(wfX >> 2) & 3], wfX, 56, false);
  hLineSafe(0, 56, 128);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(2, 63); printMonth(month()); u8g2.print(' '); print2(day());
  u8g2.setCursor(100, 63); print2(seconds());
  if (alarmOn()) {
    u8g2.setCursor(40, 9); u8g2.print(F("ALM "));
    print2(alarmHours()); u8g2.print(':'); print2(alarmMinutes());
  }
}

const watch::WatchApp APP = {
  stepWatchFace, drawView,
  gameStart, gameUpdate, gameDraw,
  gameInterrupted,
  nullptr, nullptr,
  "ACTION",
};

void setup() { watch::begin(APP); }
void loop() { watch::run(); }
