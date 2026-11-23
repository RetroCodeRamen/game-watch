// Breach Watch — a Doom-style first-person shooter for the Game Watch (working title).
//
// Engine: a grid raycaster in 16.16 fixed point that writes straight into the SH1106 page buffer.
//   - walls are 16x16 one-bit textures; walls facing north/south are dithered darker so corners read
//   - the floor gets perspective dots, so moving forward is visible even down a plain corridor
//   - doors slide sideways inside their tile; the doorway sides get a frame texture
//   - monsters, pickups and shots are scaled billboards, clipped by a per-column depth buffer
// View 128x56 (seven display pages) above an 8 px status bar. ~30 FPS from WatchCore's frame loop.
//
// Clock, alarm, set screens and buttons live in libraries/WatchCore.
// Host simulator and screenshots: tools/breach_test (build.sh, then out/run "<script>").

#include <Arduino.h>
#include <WatchCore.h>
#include "breach_art.h"
#include "breach_maps.h"

typedef int32_t q16;   // 16.16 fixed point; 1.0 = one map tile

// ---- all structs up here, before Arduino's generated prototypes ----
struct SprBits { uint8_t w, h; uint32_t lit[24], mask[24]; };            // outlined, bit x = column x
struct Door { uint8_t tx, ty, kind, st, timer; bool vert; q16 open; };     // vert: the door plane is x = tx + 0.5
struct Ent { bool live; uint8_t kind, st, hp, t, flash, anim, cool, stuck; int8_t sdx, sdy; q16 x, y; };
struct Shot { bool live; uint8_t kind, life; q16 x, y, vx, vy; };
struct Hit { q16 dist; uint8_t tex, u, side; };
struct DrawItem { q16 depth; int16_t sx; uint8_t idx, type; };
struct EnemyDef { uint8_t hp; q16 speed, radius, height, elev, range, keep; uint8_t dmg, cool, windup; bool ranged; };

// =====================================================================
// Buttons. Final layout (six buttons):
//   Left/Right turn · Up (Action/Mid) forward · Mode back up (cancel in menus)
//   Down fire, or Use when facing a door/switch · OK (Game) opens the menu
// The prototype has five, so until the 6th button is fitted, fire/use is on OK and
// holding Left+Right opens the menu. When it arrives: set SIX_BUTTONS to 1 and point B_DOWN
// at the new bit (and add the pin to WatchCore). Nothing else needs to move.
// =====================================================================
#define SIX_BUTTONS 0
const uint8_t B_FWD = B_MID;
const uint8_t B_BACK = B_MODE;
const uint8_t B_OK = B_GAME;
#if SIX_BUTTONS
const uint8_t B_FIRE = B_DOWN;
#else
const uint8_t B_FIRE = B_GAME;
#endif
const uint8_t MENU_CHORD = 10;   // frames of Left+Right that open the menu (5-button layout)

// ---- screen ----
const int16_t VIEW_W = 128, VIEW_H = 56, HZ = 28;   // HZ: the horizon row
const int32_t FOCAL = 91;                           // px; the horizontal field of view is ~70 degrees
const q16 ONE = 65536;
const q16 PLANE = (64L << 16) / FOCAL;              // camera plane half-width, ~0.70
const q16 BIG = 1L << 26;
const q16 FLOOR_DOTS_MAX = 5 * ONE;                 // no floor dots beyond this (they only shimmer)

// ---- tiles ----
enum : uint8_t { T_EMPTY, T_TECH, T_BRICK, T_SWITCH, T_SWITCH_ON, T_EXIT, T_EXIT_ON,
                 TX_DOOR, TX_KEYDOOR, TX_SHUTTER, TX_JAMB, TX_N };
const uint8_t T_DOORBIT = 0x80;                     // tile = 0x80 | door index
enum : uint8_t { DK_NORMAL, DK_KEY, DK_SHUTTER };
enum : uint8_t { DS_CLOSED, DS_OPENING, DS_OPEN, DS_CLOSING };

// ---- things ----
enum : uint8_t { K_HUSK, K_SPIT, K_DRONE, K_MEDKIT, K_CLIP, K_SHELLS, K_KEY, K_SCATTER };
enum : uint8_t { ST_IDLE, ST_CHASE, ST_ATTACK, ST_PAIN, ST_DIE, ST_DEAD };
const EnemyDef DEFS[3] = {
  //  hp  speed       radius     height     elev       range      keep away  dmg cool windup ranged
  {   30, ONE * 45 / 1000, ONE * 3 / 10, ONE * 78 / 100, 0,         ONE * 9 / 10, ONE * 6 / 10, 8, 30, 8,  false },  // Husk
  {   24, ONE * 30 / 1000, ONE * 3 / 10, ONE * 50 / 100, 0,         ONE * 7,      ONE * 5 / 2,  10, 50, 14, true },   // Spitter
  {   16, ONE * 60 / 1000, ONE / 4,      ONE * 38 / 100, ONE * 3 / 10, ONE * 6,   ONE * 2,      5, 36, 8,  true },   // Drone
};

// ---- sprites ----
enum : uint8_t { SP_HUSK_A, SP_HUSK_B, SP_HUSK_ATK, SP_SPIT_A, SP_SPIT_B, SP_SPIT_ATK,
                 SP_DRONE_A, SP_DRONE_B, SP_DRONE_ATK, SP_GLOB, SP_BOLT,
                 SP_MEDKIT, SP_CLIP, SP_SHELLS, SP_KEY, SP_SCATTER_PICK,
                 SP_PISTOL, SP_SCATTER, SP_FLASH, SP_N };
const ArtSpr ART[SP_N] = {
  {16, 20, HUSK_A}, {16, 20, HUSK_B}, {16, 20, HUSK_ATK},
  {16, 13, SPIT_A}, {16, 13, SPIT_B}, {16, 13, SPIT_ATK},
  {14, 12, DRONE_A}, {14, 12, DRONE_B}, {14, 12, DRONE_ATK},
  {6, 6, GLOB_ART}, {4, 4, BOLT_ART},
  {12, 8, MEDKIT_ART}, {8, 6, CLIP_ART}, {12, 7, SHELLS_ART}, {10, 9, KEY_ART}, {21, 6, SCATTER_PICKUP_ART},
  {22, 20, PISTOL_ART}, {22, 18, SCATTER_ART}, {12, 8, FLASH_ART},
};
// world size of each pickup / shot billboard (height above the floor it sits on, and its height)
const q16 PICK_H[5] = { ONE * 22 / 100, ONE * 16 / 100, ONE * 16 / 100, ONE * 22 / 100, ONE * 16 / 100 };

SprBits spr[SP_N];
uint16_t tex[TX_N][16];
q16 sinT[1024];
bool tablesReady, artOk = true;

// ---- the map ----
uint8_t tiles[MAX_MAP][MAX_MAP];
uint8_t mapW, mapH;
const uint8_t MAX_DOORS = 16, MAX_ENTS = 32, MAX_SHOTS = 10;
Door doors[MAX_DOORS]; uint8_t nDoors;
Ent ents[MAX_ENTS]; uint8_t nEnts;
Shot shots[MAX_SHOTS];
bool mapLoaded;

// ---- the player ----
q16 px, py, pSpeed;
uint16_t pAng;                       // 1/16 of a table step: 16384 = a full turn
uint8_t health, weapon, fireCool, flashT, hurtT, turnHold, chordT;
uint16_t bullets, shells;
bool haveScatter, haveKey;
uint8_t kills, totalKills;
uint32_t playFrames, frameN;
uint16_t bobPhase;
char msg[24]; uint8_t msgT;
bool god;                            // host tests only

// ---- modes ----
enum : uint8_t { M_TITLE, M_PLAY, M_PAUSE, M_DEAD, M_CLEAR };
uint8_t mode = M_TITLE;
bool inRun;                          // a game is in progress (resume it after the clock)
uint8_t menuSel, deadT;
uint16_t faceAng, titleAng;

// ---- camera (set before casting) ----
q16 camX, camY, dirX, dirY, plX, plY;
q16 zbuf[VIEW_W];
int16_t wallBot[VIEW_W];

// =====================================================================
// Math, tables and art
// =====================================================================
static inline q16 mulQ(q16 a, q16 b) { return (q16)(((int64_t)a * b) >> 16); }
static inline q16 qabs(q16 a) { return a < 0 ? -a : a; }
static inline q16 sinA(uint16_t a) { return sinT[a & 1023]; }
static inline q16 cosA(uint16_t a) { return sinT[(a + 256) & 1023]; }

void buildSprite(SprBits &s, const ArtSpr &a) {
  if (strlen(a.art) != (size_t)a.w * a.h || a.w > 22 || a.h > 22) artOk = false;
  memset(&s, 0, sizeof s);
  s.w = a.w + 2; s.h = a.h + 2;
  uint32_t solid[24] = {0};
  for (uint8_t y = 0; y < a.h; y++)
    for (uint8_t x = 0; x < a.w; x++) {
      char c = a.art[y * a.w + x];
      if (c == '#') s.lit[y + 1] |= 1UL << (x + 1);
      if (c == '#' || c == 'x') solid[y + 1] |= 1UL << (x + 1);
    }
  for (uint8_t y = 0; y < s.h; y++) {   // dilate by one pixel: the dark outline
    uint32_t m = solid[y] | (y ? solid[y - 1] : 0) | (y + 1 < s.h ? solid[y + 1] : 0);
    s.mask[y] = m | (m << 1) | (m >> 1);
  }
}

void buildTex(uint8_t id, const char *art) {
  if (strlen(art) != 256) artOk = false;
  for (uint8_t y = 0; y < 16; y++) {
    uint16_t row = 0;
    for (uint8_t x = 0; x < 16; x++) if (art[y * 16 + x] == '#') row |= 1u << x;
    tex[id][y] = row;
  }
}

void initTables() {
  if (tablesReady) return;
  for (int i = 0; i < 1024; i++) sinT[i] = (q16)lround(sin(i * TWO_PI / 1024.0) * 65536.0);
  for (uint8_t i = 0; i < SP_N; i++) buildSprite(spr[i], ART[i]);
  memset(tex, 0, sizeof tex);
  buildTex(T_TECH, TX_TECH_ART);   buildTex(T_BRICK, TX_BRICK_ART);
  buildTex(T_SWITCH, TX_SWITCH_ART); buildTex(T_SWITCH_ON, TX_SWITCH_ON_ART);
  buildTex(T_EXIT, TX_EXIT_ART);   buildTex(T_EXIT_ON, TX_EXIT_ON_ART);
  buildTex(TX_DOOR, TX_DOOR_ART);  buildTex(TX_KEYDOOR, TX_KEYDOOR_ART);
  buildTex(TX_SHUTTER, TX_SHUTTER_ART); buildTex(TX_JAMB, TX_JAMB_ART);
  tablesReady = true;
}

// =====================================================================
// Map
// =====================================================================
void addEnt(uint8_t kind, uint8_t tx, uint8_t ty) {
  if (nEnts >= MAX_ENTS) return;
  Ent &e = ents[nEnts++];
  memset(&e, 0, sizeof e);
  e.live = true; e.kind = kind; e.st = ST_IDLE;
  e.x = ((q16)tx << 16) + ONE / 2; e.y = ((q16)ty << 16) + ONE / 2;
  if (kind <= K_DRONE) { e.hp = DEFS[kind].hp; totalKills++; }
}

void loadMap(const char *const *rows, uint8_t h) {
  initTables();
  mapH = h; mapW = strlen(rows[0]);
  nDoors = nEnts = 0; totalKills = 0;
  memset(tiles, 0, sizeof tiles);
  memset(shots, 0, sizeof shots);
  px = py = ONE + ONE / 2;
  for (uint8_t y = 0; y < mapH; y++)
    for (uint8_t x = 0; x < mapW; x++) {
      char c = rows[y][x];
      uint8_t t = T_EMPTY;
      switch (c) {
        case '#': t = T_TECH; break;
        case 'B': t = T_BRICK; break;
        case 'S': t = T_SWITCH; break;
        case 'X': t = T_EXIT; break;
        case 'D': case 'K': case 'L':
          if (nDoors < MAX_DOORS) {
            Door &d = doors[nDoors];
            d.tx = x; d.ty = y; d.st = DS_CLOSED; d.open = 0; d.timer = 0;
            d.kind = c == 'D' ? DK_NORMAL : c == 'K' ? DK_KEY : DK_SHUTTER;
            t = T_DOORBIT | nDoors++;
          }
          break;
        case 'P': px = ((q16)x << 16) + ONE / 2; py = ((q16)y << 16) + ONE / 2; break;
        case 'h': addEnt(K_HUSK, x, y); break;
        case 's': addEnt(K_SPIT, x, y); break;
        case 'o': addEnt(K_DRONE, x, y); break;
        case '+': addEnt(K_MEDKIT, x, y); break;
        case 'a': addEnt(K_CLIP, x, y); break;
        case 'c': addEnt(K_SHELLS, x, y); break;
        case 'k': addEnt(K_KEY, x, y); break;
        case 'G': addEnt(K_SCATTER, x, y); break;
      }
      tiles[y][x] = t;
    }
  // a door between walls on its left and right blocks north-south travel: its plane runs east-west
  for (uint8_t i = 0; i < nDoors; i++) {
    Door &d = doors[i];
    bool wallsLR = d.tx > 0 && d.tx + 1 < mapW && tiles[d.ty][d.tx - 1] && tiles[d.ty][d.tx + 1];
    d.vert = !wallsLR;
  }
  mapLoaded = true;
}

static inline bool inMap(int16_t x, int16_t y) { return x >= 0 && y >= 0 && x < mapW && y < mapH; }

bool cellSolid(int16_t x, int16_t y) {
  if (!inMap(x, y)) return true;
  uint8_t t = tiles[y][x];
  if (!t) return false;
  if (t & T_DOORBIT) return doors[t & 0x7f].open < ONE * 3 / 4;
  return true;
}

bool spotFree(q16 x, q16 y, q16 r) {
  return !cellSolid((x - r) >> 16, (y - r) >> 16) && !cellSolid((x + r) >> 16, (y - r) >> 16) &&
         !cellSolid((x - r) >> 16, (y + r) >> 16) && !cellSolid((x + r) >> 16, (y + r) >> 16);
}

// slide along walls: try each axis separately. Returns true if it moved at all.
bool moveBody(q16 &x, q16 &y, q16 dx, q16 dy, q16 r) {
  bool moved = false;
  if (dx && spotFree(x + dx, y, r)) { x += dx; moved = true; }
  if (dy && spotFree(x, y + dy, r)) { y += dy; moved = true; }
  return moved;
}

// can a straight line from a to b pass? (closed doors block it)
bool lineClear(q16 ax, q16 ay, q16 bx, q16 by) {
  q16 dx = bx - ax, dy = by - ay;
  int32_t n = (int32_t)((qabs(dx) > qabs(dy) ? qabs(dx) : qabs(dy)) >> 14) + 1;   // quarter-tile steps
  if (n > 80) return false;
  q16 sx = dx / n, sy = dy / n, x = ax, y = ay;
  for (int32_t i = 0; i < n; i++) {
    x += sx; y += sy;
    if (cellSolid(x >> 16, y >> 16)) return false;
  }
  return true;
}

float distF(q16 ax, q16 ay, q16 bx, q16 by) {
  float dx = (bx - ax) / 65536.0f, dy = (by - ay) / 65536.0f;
  return sqrtf(dx * dx + dy * dy);
}

// =====================================================================
// Raycaster
// =====================================================================
void setCamera(q16 x, q16 y, uint16_t a) {
  camX = x; camY = y;
  dirX = cosA(a); dirY = sinA(a);
  plX = mulQ(-dirY, PLANE); plY = mulQ(dirX, PLANE);   // the camera plane points to the right
}

Hit castRay(int16_t col) {
  Hit h = { BIG, 0, 0, 0 };
  q16 off = ((int32_t)(2 * col + 1 - VIEW_W) << 16) / VIEW_W;   // -1 .. +1 across the screen
  q16 rdx = dirX + mulQ(plX, off), rdy = dirY + mulQ(plY, off);
  int16_t mx = camX >> 16, my = camY >> 16;
  q16 ddx = rdx == 0 ? BIG : (q16)(0xFFFFFFFFu / (uint32_t)qabs(rdx));
  q16 ddy = rdy == 0 ? BIG : (q16)(0xFFFFFFFFu / (uint32_t)qabs(rdy));
  if (ddx > BIG || ddx < 0) ddx = BIG;
  if (ddy > BIG || ddy < 0) ddy = BIG;
  int8_t stx = rdx < 0 ? -1 : 1, sty = rdy < 0 ? -1 : 1;
  q16 sdx = rdx < 0 ? mulQ(camX - ((q16)mx << 16), ddx) : mulQ(((q16)(mx + 1) << 16) - camX, ddx);
  q16 sdy = rdy < 0 ? mulQ(camY - ((q16)my << 16), ddy) : mulQ(((q16)(my + 1) << 16) - camY, ddy);
  bool lastDoor = false;
  for (uint8_t i = 0; i < 64; i++) {
    q16 dist; uint8_t side;
    if (sdx < sdy) { dist = sdx; sdx += ddx; mx += stx; side = 0; }
    else           { dist = sdy; sdy += ddy; my += sty; side = 1; }
    if (!inMap(mx, my)) { h.dist = dist; h.tex = T_TECH; h.side = side; return h; }
    uint8_t t = tiles[my][mx];
    if (!t) { lastDoor = false; continue; }
    if (t & T_DOORBIT) {
      const Door &d = doors[t & 0x7f];
      q16 exitD = sdx < sdy ? sdx : sdy;
      q16 tD = -1, along = 0;
      bool inCell = false;
      if (d.vert && rdx) {
        tD = (q16)(((int64_t)(((q16)mx << 16) + ONE / 2 - camX) << 16) / rdx);
        along = camY + mulQ(tD, rdy); inCell = (along >> 16) == my;
      } else if (!d.vert && rdy) {
        tD = (q16)(((int64_t)(((q16)my << 16) + ONE / 2 - camY) << 16) / rdy);
        along = camX + mulQ(tD, rdx); inCell = (along >> 16) == mx;
      }
      if (inCell && tD >= dist && tD <= exitD) {
        q16 f = along & 0xFFFF;
        if (f >= d.open) {
          h.dist = tD; h.side = d.vert ? 0 : 1;
          h.tex = d.kind == DK_KEY ? TX_KEYDOOR : d.kind == DK_SHUTTER ? TX_SHUTTER : TX_DOOR;
          h.u = ((f - d.open) * 16) >> 16;
          return h;
        }
      }
      lastDoor = true;
      continue;
    }
    q16 wx = side == 0 ? camY + mulQ(dist, rdy) : camX + mulQ(dist, rdx);
    uint8_t u = ((wx & 0xFFFF) * 16) >> 16;
    if ((side == 0 && rdx < 0) || (side == 1 && rdy > 0)) u = 15 - u;
    h.dist = dist; h.side = side; h.u = u; h.tex = lastDoor ? TX_JAMB : t;
    return h;
  }
  return h;
}

static inline void setPix(uint8_t *buf, int16_t x, int16_t y) { buf[(y >> 3) * 128 + x] |= 1 << (y & 7); }
static inline void clrPix(uint8_t *buf, int16_t x, int16_t y) { buf[(y >> 3) * 128 + x] &= ~(1 << (y & 7)); }

void drawWalls(uint8_t *buf) {
  for (int16_t x = 0; x < VIEW_W; x++) {
    Hit h = castRay(x);
    q16 dist = h.dist < 4096 ? 4096 : h.dist;
    zbuf[x] = h.dist;
    int32_t half = (int32_t)((FOCAL << 15) / dist);
    if (half > 2000) half = 2000;
    int32_t top = HZ - half, bot = HZ + half;        // bot is exclusive
    wallBot[x] = bot > VIEW_H ? VIEW_H : bot;
    if (!h.tex) continue;
    int32_t y0 = top < 0 ? 0 : top, y1 = bot > VIEW_H ? VIEW_H : bot;
    int32_t vstep = (16L << 16) / (2 * half);
    int32_t v = (y0 - top) * vstep;
    const uint16_t *tx = tex[h.tex];
    uint16_t ub = 1u << h.u;
    bool dim = h.side == 1 || h.dist > 7 * ONE;
    for (int32_t y = y0; y < y1; y++, v += vstep) {
      if ((tx[(v >> 16) & 15] & ub) && !(dim && ((x ^ y) & 1))) setPix(buf, x, y);
    }
    if (top >= 0) setPix(buf, x, top);               // the wall's top and bottom edge, always solid
    if (bot - 1 < VIEW_H && bot - 1 >= 0) setPix(buf, x, bot - 1);
  }
}

void drawFloor(uint8_t *buf) {
  q16 ldx = dirX - plX, ldy = dirY - plY;
  q16 spanX = 2 * plX, spanY = 2 * plY;
  for (int16_t y = HZ + 2; y < VIEW_H; y++) {
    q16 rowD = (q16)((FOCAL << 15) / (y - HZ));
    if (rowD > FLOOR_DOTS_MAX) continue;
    q16 fx = camX + mulQ(rowD, ldx), fy = camY + mulQ(rowD, ldy);
    q16 sx = mulQ(rowD, spanX) / VIEW_W, sy = mulQ(rowD, spanY) / VIEW_W;
    for (int16_t x = 0; x < VIEW_W; x++, fx += sx, fy += sy)
      if (y >= wallBot[x] && !(fx & 0x3800) && !(fy & 0x3800)) setPix(buf, x, y);   // a dot every quarter tile
  }
}

// One billboard: world height h, raised elev above the floor. squash16 < 16 flattens it (dying).
void drawBillboard(uint8_t *buf, const SprBits &s, q16 depth, int16_t sx, q16 h, q16 elev,
                   bool white, uint8_t squash16) {
  if (depth < ONE / 6) return;
  int32_t hpx = (int32_t)(((int64_t)FOCAL * h) / depth);
  int32_t wpx = hpx * s.w / s.h;
  if (squash16 < 16) hpx = hpx * squash16 / 16;
  if (hpx < 1 || wpx < 1 || wpx > 600 || hpx > 600) return;
  int32_t floorY = HZ + (int32_t)((FOCAL << 15) / depth);
  int32_t bottom = floorY - (int32_t)((FOCAL * elev) / depth);
  int32_t top = bottom - hpx, left = sx - wpx / 2;
  int32_t ustep = ((int32_t)s.w << 16) / wpx, vstep = ((int32_t)s.h << 16) / hpx;
  int32_t c0 = left < 0 ? 0 : left, c1 = left + wpx > VIEW_W ? VIEW_W : left + wpx;
  int32_t r0 = top < 0 ? 0 : top, r1 = bottom > VIEW_H ? VIEW_H : bottom;
  for (int32_t c = c0; c < c1; c++) {
    if (depth >= zbuf[c]) continue;
    uint32_t ub = 1UL << (((c - left) * ustep) >> 16);
    int32_t v = (r0 - top) * vstep;
    for (int32_t r = r0; r < r1; r++, v += vstep) {
      uint8_t row = v >> 16;
      if (!(s.mask[row] & ub)) continue;
      if (white || (s.lit[row] & ub)) setPix(buf, c, r); else clrPix(buf, c, r);
    }
  }
}

// 1:1 blit (the gun), clipped to the view
void blit(uint8_t *buf, const SprBits &s, int16_t x, int16_t y) {
  for (uint8_t r = 0; r < s.h; r++) {
    int16_t yy = y + r;
    if (yy < 0 || yy >= VIEW_H) continue;
    for (uint8_t c = 0; c < s.w; c++) {
      int16_t xx = x + c;
      if (xx < 0 || xx >= VIEW_W || !(s.mask[r] >> c & 1)) continue;
      if (s.lit[r] >> c & 1) setPix(buf, xx, yy); else clrPix(buf, xx, yy);
    }
  }
}

// camera-space depth and screen column of a world point
bool project(q16 x, q16 y, q16 &depth, int16_t &sx) {
  q16 dx = x - camX, dy = y - camY;
  depth = mulQ(dx, dirX) + mulQ(dy, dirY);
  if (depth < ONE / 6) return false;
  q16 lat = mulQ(dx, -dirY) + mulQ(dy, dirX);
  int32_t s = 64 + (int32_t)(((int64_t)lat * FOCAL) / depth);
  if (s < -400 || s > 528) return false;
  sx = s;
  return true;
}

uint8_t enemySprite(const Ent &e) {
  uint8_t base = e.kind == K_HUSK ? SP_HUSK_A : e.kind == K_SPIT ? SP_SPIT_A : SP_DRONE_A;
  if (e.st == ST_ATTACK) return base + 2;
  if (e.kind == K_DRONE) return base + ((frameN >> 2) & 1);     // rotors spin even when still
  return base + ((e.anim >> 3) & 1);
}

void drawThings(uint8_t *buf) {
  DrawItem items[MAX_ENTS + MAX_SHOTS];
  uint8_t n = 0;
  for (uint8_t i = 0; i < nEnts; i++) {
    if (!ents[i].live) continue;
    DrawItem &it = items[n];
    if (project(ents[i].x, ents[i].y, it.depth, it.sx)) { it.idx = i; it.type = 0; n++; }
  }
  for (uint8_t i = 0; i < MAX_SHOTS; i++) {
    if (!shots[i].live) continue;
    DrawItem &it = items[n];
    if (project(shots[i].x, shots[i].y, it.depth, it.sx)) { it.idx = i; it.type = 1; n++; }
  }
  for (uint8_t i = 1; i < n; i++) {                      // far to near
    DrawItem t = items[i]; int8_t j = i - 1;
    while (j >= 0 && items[j].depth < t.depth) { items[j + 1] = items[j]; j--; }
    items[j + 1] = t;
  }
  for (uint8_t i = 0; i < n; i++) {
    const DrawItem &it = items[i];
    if (it.type == 1) {
      const Shot &s = shots[it.idx];
      drawBillboard(buf, spr[s.kind ? SP_BOLT : SP_GLOB], it.depth, it.sx, s.kind ? ONE / 8 : ONE / 6, ONE * 4 / 10, true, 16);
      continue;
    }
    const Ent &e = ents[it.idx];
    if (e.kind <= K_DRONE) {
      const EnemyDef &d = DEFS[e.kind];
      uint8_t sq = 16;
      q16 elev = d.elev;
      if (e.st == ST_DIE) { sq = 16 - e.t; elev = elev * (10 - e.t) / 10; }
      else if (e.st == ST_DEAD) { sq = 5; elev = 0; }
      bool white = e.flash > 0 && e.st != ST_DEAD;
      drawBillboard(buf, spr[enemySprite(e)], it.depth, it.sx, d.height, elev, white, sq);
    } else {
      uint8_t k = e.kind - K_MEDKIT;
      drawBillboard(buf, spr[SP_MEDKIT + k], it.depth, it.sx, PICK_H[k], 0, false, 16);
    }
  }
}

void renderView(uint8_t *buf, bool things) {
  drawWalls(buf);
  drawFloor(buf);
  if (things) drawThings(buf);
}

// =====================================================================
// Game rules
// =====================================================================
void say(const char *s) { strncpy(msg, s, sizeof msg - 1); msg[sizeof msg - 1] = 0; msgT = 60; }

void newGame() {
  loadMap(TEST_ROOM, TEST_ROOM_H);
  pAng = 0; pSpeed = 0;
  health = 100; bullets = 40; shells = 0; weapon = 0;
  haveScatter = haveKey = false;
  fireCool = flashT = hurtT = turnHold = chordT = 0;
  kills = 0; playFrames = 0; msgT = 0; bobPhase = 0;
  mode = M_PLAY; inRun = true;
  say("TEST ROOM");
}

void hurtPlayer(uint8_t dmg) {
  if (god || mode != M_PLAY) return;
  health = health > dmg ? health - dmg : 0;
  hurtT = 6;
  beep(110, 60);
  if (!health) { mode = M_DEAD; deadT = 0; inRun = false; }
}

bool occupied(const Door &d) {
  q16 cx = ((q16)d.tx << 16) + ONE / 2, cy = ((q16)d.ty << 16) + ONE / 2, lim = ONE / 2 + ONE / 4;
  if (qabs(px - cx) < lim && qabs(py - cy) < lim) return true;
  for (uint8_t i = 0; i < nEnts; i++) {
    const Ent &e = ents[i];
    if (e.live && e.kind <= K_DRONE && e.st != ST_DEAD && qabs(e.x - cx) < lim && qabs(e.y - cy) < lim) return true;
  }
  return false;
}

void updateDoors() {
  for (uint8_t i = 0; i < nDoors; i++) {
    Door &d = doors[i];
    switch (d.st) {
      case DS_OPENING:
        d.open += ONE / 15;
        if (d.open >= ONE) { d.open = ONE; d.st = DS_OPEN; d.timer = 120; }
        break;
      case DS_OPEN:
        if (d.kind == DK_SHUTTER) break;                // shutters stay open
        if (d.timer) d.timer--;
        else if (!occupied(d)) { d.st = DS_CLOSING; beep(300, 30); }
        break;
      case DS_CLOSING:
        if (occupied(d)) { d.st = DS_OPENING; break; }
        d.open -= ONE / 15;
        if (d.open <= 0) { d.open = 0; d.st = DS_CLOSED; }
        break;
    }
  }
}

// Down/Fire while facing something usable within reach: use it instead of shooting.
bool tryUse() {
  for (uint8_t step = 2; step <= 10; step++) {           // 1/4 .. 1.25 tiles ahead
    q16 x = px + mulQ(dirX, step * ONE / 8), y = py + mulQ(dirY, step * ONE / 8);
    int16_t tx = x >> 16, ty = y >> 16;
    if (!inMap(tx, ty)) return false;
    uint8_t t = tiles[ty][tx];
    if (!t) continue;
    if (t & T_DOORBIT) {
      Door &d = doors[t & 0x7f];
      if (d.st == DS_OPEN) continue;                    // standing in an open doorway: look past it
      if (d.kind == DK_SHUTTER) { say("OPENED ELSEWHERE"); beep(180, 40); return true; }
      if (d.kind == DK_KEY && !haveKey) { say("NEEDS THE KEYCARD"); beep(180, 40); return true; }
      if (d.st == DS_CLOSED || d.st == DS_CLOSING) { d.st = DS_OPENING; beep(420, 40); }
      return true;
    }
    if (t == T_SWITCH) {
      tiles[ty][tx] = T_SWITCH_ON;
      for (uint8_t i = 0; i < nDoors; i++) if (doors[i].kind == DK_SHUTTER) doors[i].st = DS_OPENING;
      say("SOMETHING OPENED"); beep(900, 60);
      return true;
    }
    if (t == T_EXIT) {
      tiles[ty][tx] = T_EXIT_ON;
      beep(1300, 120);
      mode = M_CLEAR; inRun = false;
      return true;
    }
    return false;                                       // a plain wall
  }
  return false;
}

void wakeNear(q16 range) {
  for (uint8_t i = 0; i < nEnts; i++) {
    Ent &e = ents[i];
    if (e.live && e.kind <= K_DRONE && e.st == ST_IDLE && distF(px, py, e.x, e.y) < range / 65536.0f) {
      e.st = ST_CHASE; e.cool = 10 + random(20);
    }
  }
}

void damageEnemy(Ent &e, uint8_t dmg) {
  e.flash = 3;
  if (e.hp <= dmg) {
    e.hp = 0; e.st = ST_DIE; e.t = 0; kills++;
    beep(160, 90);
    return;
  }
  e.hp -= dmg;
  beep(260, 20);
  if (e.st == ST_IDLE || random(3) == 0) { e.st = ST_PAIN; e.t = 5; }
  if (e.st == ST_IDLE) e.st = ST_CHASE;
}

// hitscan along screen column col; aim = extra px of forgiveness either side of the monster
void hitscan(int16_t col, int16_t aim, uint8_t dmg) {
  if (col < 0) col = 0;
  if (col >= VIEW_W) col = VIEW_W - 1;
  q16 wall = castRay(col).dist;
  int8_t best = -1; q16 bestD = wall;
  for (uint8_t i = 0; i < nEnts; i++) {
    const Ent &e = ents[i];
    if (!e.live || e.kind > K_DRONE || e.st == ST_DIE || e.st == ST_DEAD) continue;
    q16 depth; int16_t sx;
    if (!project(e.x, e.y, depth, sx)) continue;
    int32_t halfW = (int32_t)(((int64_t)FOCAL * DEFS[e.kind].radius) / depth);
    int32_t off = sx - col; if (off < 0) off = -off;
    if (off <= halfW + aim && depth < bestD) { best = i; bestD = depth; }
  }
  if (best >= 0) damageEnemy(ents[best], dmg);
}

void fireWeapon() {
  if (weapon == 1 && !shells) weapon = 0;
  if (weapon == 0 && !bullets) {
    if (haveScatter && shells) { weapon = 1; say("SCATTERGUN"); }
    else { say("OUT OF AMMO"); beep(150, 40); fireCool = 15; return; }
  }
  if (weapon == 0) {
    bullets--;
    hitscan(64, 4, 9 + random(6));
    fireCool = 9; flashT = 3;
    beep(1100, 25);
  } else {
    shells--;
    for (int8_t k = -3; k <= 3; k++) hitscan(64 + k * 6, 2, 6 + random(4));
    fireCool = 26; flashT = 4;
    beep(220, 70);
  }
  wakeNear(8 * ONE);
}

void pickUp(Ent &e) {
  switch (e.kind) {
    case K_MEDKIT: if (health >= 100) return; health = health + 25 > 100 ? 100 : health + 25; say("MEDKIT"); break;
    case K_CLIP:   if (bullets >= 200) return; bullets = bullets + 20 > 200 ? 200 : bullets + 20; say("AMMO CLIP"); break;
    case K_SHELLS: if (shells >= 50) return; shells = shells + 8 > 50 ? 50 : shells + 8; say("SHELLS"); break;
    case K_KEY:    haveKey = true; say("KEYCARD"); break;
    case K_SCATTER: haveScatter = true; shells = shells + 8 > 50 ? 50 : shells + 8; weapon = 1; say("SCATTERGUN!"); break;
  }
  e.live = false;
  beep(1500, 40);
}

void enemyAttack(Ent &e, const EnemyDef &d) {
  if (!d.ranged) {
    if (distF(px, py, e.x, e.y) < (d.range + ONE / 4) / 65536.0f) hurtPlayer(d.dmg);
    beep(90, 40);
    return;
  }
  for (uint8_t i = 0; i < MAX_SHOTS; i++) {
    Shot &s = shots[i];
    if (s.live) continue;
    float dist = distF(e.x, e.y, px, py);
    if (dist < 0.1f) dist = 0.1f;
    float spd = e.kind == K_DRONE ? 0.18f : 0.11f;
    s.live = true; s.kind = e.kind == K_DRONE ? 1 : 0; s.life = 120;
    s.x = e.x; s.y = e.y;
    s.vx = (q16)((px - e.x) / dist * spd); s.vy = (q16)((py - e.y) / dist * spd);
    beep(e.kind == K_DRONE ? 700 : 240, 30);
    return;
  }
}

void updateEnemy(Ent &e) {
  const EnemyDef &d = DEFS[e.kind];
  if (e.flash) e.flash--;
  switch (e.st) {
    case ST_DEAD: return;
    case ST_DIE:  if (++e.t >= 11) e.st = ST_DEAD; return;
    case ST_PAIN: if (e.t) e.t--; else e.st = ST_CHASE; return;
    case ST_IDLE:
      if (((frameN + e.x) & 3) == 0 && distF(px, py, e.x, e.y) < 12 && lineClear(e.x, e.y, px, py)) {
        e.st = ST_CHASE; e.cool = 10 + random(20);
      }
      return;
    case ST_ATTACK:
      if (e.t) { e.t--; return; }
      enemyAttack(e, d);
      e.st = ST_CHASE; e.cool = d.cool + random(12);
      return;
  }
  // ST_CHASE
  if (e.cool) e.cool--;
  float dist = distF(e.x, e.y, px, py);
  bool see = lineClear(e.x, e.y, px, py);
  if (see && !e.cool && dist < d.range / 65536.0f) { e.st = ST_ATTACK; e.t = d.windup; return; }
  if (see && dist < d.keep / 65536.0f) return;            // close enough: hold ground
  q16 mx, my;
  if (e.stuck) { e.stuck--; mx = e.sdx * d.speed; my = e.sdy * d.speed; }
  else {
    if (dist < 0.05f) return;
    mx = (q16)((px - e.x) / dist * (d.speed / 65536.0f));
    my = (q16)((py - e.y) / dist * (d.speed / 65536.0f));
  }
  q16 nx = e.x, ny = e.y;
  bool moved = moveBody(nx, ny, mx, my, d.radius);
  if (distF(nx, ny, px, py) < (d.radius + ONE / 4) / 65536.0f) return;   // don't walk into the player
  e.x = nx; e.y = ny;
  if (moved) e.anim++;
  else if (!e.stuck) {                                     // blocked: sidestep for a moment
    e.stuck = 15 + random(15);
    int8_t s = random(2) ? 1 : -1;
    if (qabs(mx) > qabs(my)) { e.sdx = 0; e.sdy = s; } else { e.sdx = s; e.sdy = 0; }
  }
}

void updateShots() {
  for (uint8_t i = 0; i < MAX_SHOTS; i++) {
    Shot &s = shots[i];
    if (!s.live) continue;
    s.x += s.vx; s.y += s.vy;
    if (!s.life-- || cellSolid(s.x >> 16, s.y >> 16)) { s.live = false; continue; }
    if (qabs(s.x - px) < ONE * 3 / 10 && qabs(s.y - py) < ONE * 3 / 10) {
      hurtPlayer(DEFS[s.kind ? K_DRONE : K_SPIT].dmg);
      s.live = false;
    }
  }
}

void openMenu() { mode = M_PAUSE; menuSel = 0; chordT = 0; }

void play(uint8_t e, uint8_t h) {
  playFrames++;
  // ---- menu ----
#if SIX_BUTTONS
  if (e & B_OK) { openMenu(); return; }
#endif
  if ((h & (B_LEFT | B_RIGHT)) == (B_LEFT | B_RIGHT)) {
    if (++chordT >= MENU_CHORD) { openMenu(); return; }
  } else chordT = 0;

  // ---- turning: slow at first for fine aim, faster the longer it's held ----
  bool l = h & B_LEFT, r = h & B_RIGHT;
  if (l != r) {
    if (turnHold < 255) turnHold++;
    uint16_t rate = 40 + turnHold * 24;
    if (rate > 272) rate = 272;                         // 17 table steps: ~180 degrees a second
    pAng = (pAng + (r ? rate : 16384 - rate)) & 16383;
  } else turnHold = 0;
  setCamera(px, py, pAng >> 4);

  // ---- walking ----
  const q16 MAXF = ONE * 115 / 1000, MAXB = ONE * 75 / 1000, WALK_ACC = ONE * 30 / 1000, WALK_DEC = ONE * 40 / 1000;
  q16 want = (h & B_FWD) ? MAXF : (h & B_BACK) ? -MAXB : 0;
  if (pSpeed < want) pSpeed = pSpeed + WALK_ACC > want ? want : pSpeed + WALK_ACC;
  else if (pSpeed > want) pSpeed = pSpeed - WALK_DEC < want ? want : pSpeed - WALK_DEC;
  if (pSpeed) {
    moveBody(px, py, mulQ(dirX, pSpeed), mulQ(dirY, pSpeed), ONE / 4);
    bobPhase += qabs(pSpeed) >> 9;
    setCamera(px, py, pAng >> 4);
  }

  // ---- fire / use ----
  if (fireCool) fireCool--;
  if (flashT) flashT--;
  if (hurtT) hurtT--;
  bool used = (e & B_FIRE) && tryUse();
  if (mode != M_PLAY) return;
  if (!used && (h & B_FIRE) && !fireCool) fireWeapon();

  // ---- world ----
  updateDoors();
  for (uint8_t i = 0; i < nEnts; i++) {
    Ent &en = ents[i];
    if (!en.live) continue;
    if (en.kind <= K_DRONE) updateEnemy(en);
    else if (qabs(en.x - px) < ONE / 2 && qabs(en.y - py) < ONE / 2) pickUp(en);
  }
  updateShots();
  if (msgT) msgT--;
}

void menuUpdate(uint8_t e) {
  const uint8_t N = 4;   // RESUME, WEAPON, RESTART, CLOCK
  if (e & (B_LEFT | B_FWD)) { menuSel = (menuSel + N - 1) % N; beep(800, 15); }
  if (e & B_RIGHT) { menuSel = (menuSel + 1) % N; beep(800, 15); }
  if (e & B_BACK) { mode = M_PLAY; return; }
  if (!(e & B_OK)) return;
  switch (menuSel) {
    case 0: mode = M_PLAY; break;
    case 1: if (haveScatter) weapon ^= 1; beep(1000, 20); break;
    case 2: newGame(); break;
    case 3: mode = M_PLAY; openMenu(); watch::exitToClock(); break;
  }
}

void gameStart() {
  initTables();
  if (inRun) openMenu();       // back from the clock: the run is still in memory
  else { mode = M_TITLE; if (!mapLoaded) loadMap(TEST_ROOM, TEST_ROOM_H); }
}

void gameUpdate(uint8_t e) {
  frameN++;
  uint8_t h = watch::held();
  switch (mode) {
    case M_TITLE:
      titleAng += 2;
      if (e & (B_OK | B_FWD)) newGame();
      break;
    case M_PLAY:  play(e, h); break;
    case M_PAUSE: menuUpdate(e); break;
    case M_DEAD:
      if (deadT < 255) deadT++;
      if (deadT > 20 && (e & B_OK)) newGame();
      else if (deadT > 20 && (e & B_BACK)) { mode = M_TITLE; watch::exitToClock(); }
      break;
    case M_CLEAR:
      if (e & B_OK) newGame();
      else if (e & B_BACK) { mode = M_TITLE; watch::exitToClock(); }
      break;
  }
}

void gameInterrupted() { if (mode == M_PLAY) openMenu(); }

// =====================================================================
// Drawing
// =====================================================================
void textBox(int16_t x, int16_t y, int16_t w, int16_t h) {
  u8g2.setDrawColor(0); u8g2.drawBox(x, y, w, h);
  u8g2.setDrawColor(1); u8g2.drawFrame(x, y, w, h);
}

void centerText(const char *s, int16_t y) {
  u8g2.setCursor((128 - u8g2.getStrWidth(s)) / 2, y); u8g2.print(s);
}

void drawGun(uint8_t *buf) {
  const SprBits &g = spr[weapon ? SP_SCATTER : SP_PISTOL];
  int16_t bx = 0, by = 0;
  if (pSpeed) {
    q16 s = sinA(bobPhase);
    bx = (int16_t)(s * 3 >> 16);
    by = (int16_t)(qabs(s) * 2 >> 16);
  }
  int16_t recoil = flashT ? 2 : 0;
  int16_t x = 64 - g.w / 2 + bx, y = VIEW_H - g.h + 2 + by + recoil;
  if (flashT) blit(buf, spr[SP_FLASH], 64 - spr[SP_FLASH].w / 2 + bx, y - spr[SP_FLASH].h + 3);
  blit(buf, g, x, y);
}

void drawHud() {
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_4x6_tr);
  // health: a cross
  u8g2.drawBox(2, 57, 1, 5); u8g2.drawBox(0, 59, 5, 1);
  u8g2.setCursor(7, 63); u8g2.print((unsigned)health);
  // ammo for the gun in hand
  if (weapon == 0) { u8g2.drawBox(26, 58, 2, 4); u8g2.drawPixel(26, 57); u8g2.drawPixel(27, 57); }
  else { u8g2.drawBox(25, 58, 3, 4); u8g2.drawBox(25, 62, 3, 1); }
  u8g2.setCursor(31, 63); u8g2.print((unsigned)(weapon ? shells : bullets));
  u8g2.setCursor(50, 63); u8g2.print(weapon ? "SCATTER" : "PISTOL");
  if (haveKey) {   // the keycard diamond
    u8g2.drawPixel(88, 57); u8g2.drawHLine(87, 58, 3); u8g2.drawHLine(86, 59, 5); u8g2.drawHLine(87, 60, 3); u8g2.drawPixel(88, 61);
  }
  char k[12]; snprintf(k, sizeof k, "%u/%u", kills, totalKills);
  u8g2.setCursor(127 - u8g2.getStrWidth(k), 63); u8g2.print(k);
}

void drawPlay() {
  uint8_t *buf = u8g2.getBufferPtr();
  setCamera(px, py, pAng >> 4);
  renderView(buf, true);
  if (mode != M_DEAD) drawGun(buf);
  if (hurtT) { u8g2.setDrawColor(1); u8g2.drawFrame(0, 0, 128, VIEW_H); u8g2.drawFrame(1, 1, 126, VIEW_H - 2); }
  drawHud();
  u8g2.setFont(u8g2_font_4x6_tr);
  if (msgT && mode == M_PLAY) {
    int16_t w = u8g2.getStrWidth(msg) + 4;
    u8g2.setDrawColor(0); u8g2.drawBox((128 - w) / 2, 0, w, 8); u8g2.setDrawColor(1);
    centerText(msg, 6);
  }
  // frame time readout (test room): how long the last frame took on the watch, ms
  char t[8]; snprintf(t, sizeof t, "%u", (unsigned)(watch::frameWorkUs() / 1000));
  u8g2.setDrawColor(0); u8g2.drawBox(0, 0, u8g2.getStrWidth(t) + 2, 7); u8g2.setDrawColor(1);
  u8g2.setCursor(1, 6); u8g2.print(t);

  if (mode == M_PAUSE) {
    static const char *const ITEMS[4] = { "RESUME", "WEAPON", "RESTART", "CLOCK" };
    textBox(30, 6, 68, 46);
    u8g2.setFont(u8g2_font_5x7_tr);
    for (uint8_t i = 0; i < 4; i++) {
      int16_t y = 16 + i * 10;
      if (i == menuSel) { u8g2.drawBox(32, y - 7, 64, 9); u8g2.setDrawColor(0); }
      u8g2.setCursor(36, y);
      if (i == 1) u8g2.print(weapon ? "GUN: SCAT" : "GUN: PISTOL");
      else u8g2.print(ITEMS[i]);
      u8g2.setDrawColor(1);
    }
  } else if (mode == M_DEAD) {
    textBox(24, 14, 80, 28);
    u8g2.setFont(u8g2_font_6x10_tr); centerText("YOU DIED", 26);
    u8g2.setFont(u8g2_font_4x6_tr); centerText("OK RETRY  MODE QUIT", 37);
  } else if (mode == M_CLEAR) {
    textBox(16, 8, 96, 42);
    u8g2.setFont(u8g2_font_6x10_tr); centerText("ROOM CLEAR", 20);
    u8g2.setFont(u8g2_font_5x7_tr);
    char s[24];
    snprintf(s, sizeof s, "KILLS %u/%u", kills, totalKills); centerText(s, 31);
    uint32_t sec = playFrames / 30;
    snprintf(s, sizeof s, "TIME %lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60)); centerText(s, 40);
    u8g2.setFont(u8g2_font_4x6_tr); centerText("OK AGAIN  MODE QUIT", 48);
  }
}

void drawTitle() {
  uint8_t *buf = u8g2.getBufferPtr();
  setCamera(9 * ONE, 3 * ONE + ONE / 2, titleAng >> 1);
  renderView(buf, true);
  textBox(14, 10, 100, 34);
  u8g2.setFont(u8g2_font_ncenB18_tr); centerText("BREACH", 32);
  u8g2.setFont(u8g2_font_4x6_tr); centerText("TEST ROOM", 41);
  u8g2.setDrawColor(0); u8g2.drawBox(0, 56, 128, 8); u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x7_tr);
  if ((frameN >> 4) & 1) centerText("PRESS OK", 63);
}

void gameDraw() {
  if (mode == M_TITLE) drawTitle(); else drawPlay();
}

// =====================================================================
// Clock face: the arena turns slowly behind the time
// =====================================================================
void stepWatchFace() { faceAng += 1; }

void drawView() {
  using namespace watch;
  initTables();
  if (!mapLoaded) loadMap(TEST_ROOM, TEST_ROOM_H);
  uint8_t *buf = u8g2.getBufferPtr();
  setCamera(15 * ONE + ONE / 2, 4 * ONE + ONE / 2, faceAng);
  renderView(buf, false);
  textBox(20, 10, 88, 32);
  u8g2.setFont(u8g2_font_logisoso24_tn);
  u8g2.setCursor(29, 38); print2(hours()); u8g2.print(':'); print2(minutes());
  u8g2.setDrawColor(0); u8g2.drawBox(0, 54, 128, 10); u8g2.setDrawColor(1);
  u8g2.drawHLine(0, 54, 128);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(2, 63); printMonth(month()); u8g2.print(' '); print2(day());
  u8g2.setCursor(112, 63); print2(seconds());
  if (alarmOn()) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(48, 63); u8g2.print(F("ALM ")); print2(alarmHours()); u8g2.print(':'); print2(alarmMinutes());
  }
}

const watch::WatchApp APP = {
  stepWatchFace, drawView,
  gameStart, gameUpdate, gameDraw,
  gameInterrupted,
  nullptr, nullptr,
  "MID",
};

void setup() { watch::begin(APP); }
void loop() { watch::run(); }
