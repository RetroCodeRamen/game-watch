#pragma once
// Metroid Watch rooms — Design Revision 4
// Playfield viewport 128×56 under 8px HUD. Room size is W×H in viewports.
// World coords: X right, Y down from HUD bottom (play y 0 = screen y 8).

enum RoomId : uint8_t {
  R_B01 = 0, R_B02, R_B03, R_B04, R_B05, R_B06, R_B07, R_B08,
  R_K01, R_K02, R_K03, R_K04, R_K05, R_K06, R_K07, R_K08, R_K09, R_K10,
  R_N01, R_N02, R_N03, R_N04, R_N05, R_N06, R_N07, R_N08, R_N09, R_N10,
  R_T01, R_T02, R_T03, R_T04, R_T05, R_T06, R_T07, R_T08,
  ROOM_COUNT
};

enum ExitSide : uint8_t { EX_LEFT = 0, EX_RIGHT = 1, EX_UP = 2, EX_DOWN = 3 };

enum GateKind : uint8_t {
  GATE_NONE = 0,
  GATE_MISSILE,
  GATE_BOMB,
  GATE_SCREW,
  GATE_BOSSES,
  GATE_METROID,
  GATE_ESCAPE,
  GATE_AFTER_BOSS
};

// Enemy kinds (Rev 4)
enum EnemyKind : uint8_t {
  EN_ZOOMER = 0,
  EN_SKREE,
  EN_RIPPER,
  EN_TURRET,
  EN_METROID,
  EN_SWIPE,
  EN_BOLT
};

// Pickup kinds
enum PickupKind : uint8_t {
  PK_MORPH = 0,
  PK_LAUNCHER,   // + missile tank bit
  PK_BOMBS,
  PK_MTANK,      // missile tank only
  PK_ETANK       // energy tank
};

struct RoomExit {
  uint8_t dest;
  uint8_t side;
  uint8_t gate;
  uint8_t gateFlag;
  int16_t wx, wy;   // door hotspot in room world px (playfield)
};

// Platform: x,y,w in world playfield pixels (y from top of playfield 0..roomH-1)
#define PL(x, y, w) ((uint32_t)((uint16_t)(x)) | ((uint32_t)((uint8_t)(y)) << 16) | ((uint32_t)((uint8_t)(w)) << 24))

struct RoomDef {
  uint8_t screensW;   // 1..2
  uint8_t screensH;   // 1..4
  uint8_t zone;       // 0 B 1 K 2 N 3 T
  uint8_t flags;      // bit0 save bit1 ship bit2 Kraid bit3 Ridley bit4 MB bit5 lava bit6 lowceil
  uint8_t exitCount;
  uint8_t platCount;
  uint8_t enemyCount;
  uint8_t pickupCount;
  const RoomExit *exits;
  const uint32_t *plats;
  const uint8_t *enemies; // xLo,xHi,y,kind
  const uint8_t *pickups; // xLo,xHi,y,kind,id
};

// -------------------- exits (hotspots in world playfield coords) --------------------
// Playfield height per screen = 56. Room height = screensH * 56.

static const RoomExit EX_B01[] = {
  {R_B02, EX_RIGHT, GATE_NONE, 0, 248, 48},
  {R_T07, EX_DOWN, GATE_ESCAPE, 0, 40, 52}  // shaft vent — alarm only in
};
static const RoomExit EX_B02[] = {
  {R_B01, EX_LEFT, GATE_NONE, 0, 8, 48},
  {R_B03, EX_RIGHT, GATE_NONE, 0, 248, 48},
  {R_B08, EX_DOWN, GATE_BOMB, 0, 120, 52}
};
static const RoomExit EX_B03[] = {
  {R_B02, EX_LEFT, GATE_NONE, 0, 8, 48},
  {R_B04, EX_DOWN, GATE_NONE, 0, 100, 52}
};
static const RoomExit EX_B04[] = {
  {R_B03, EX_UP, GATE_NONE, 0, 20, 8},
  {R_B05, EX_DOWN, GATE_NONE, 0, 100, 52}
};
static const RoomExit EX_B05[] = {
  {R_B04, EX_UP, GATE_NONE, 0, 64, 8},
  {R_B06, EX_DOWN, GATE_NONE, 0, 64, 108},
  {R_N09, EX_RIGHT, GATE_NONE, 0, 120, 40}  // grille from N09 one-way in
};
static const RoomExit EX_B06[] = {
  {R_B05, EX_UP, GATE_NONE, 0, 64, 8},
  {R_B07, EX_DOWN, GATE_NONE, 0, 64, 108},
  {R_K01, EX_LEFT, GATE_MISSILE, 1, 8, 40},
  {R_N01, EX_RIGHT, GATE_MISSILE, 2, 248, 90}
};
static const RoomExit EX_B07[] = {
  {R_B06, EX_UP, GATE_NONE, 0, 64, 8},
  {R_T01, EX_DOWN, GATE_BOSSES, 0, 64, 52},
  {R_K09, EX_LEFT, GATE_NONE, 0, 8, 40}  // drop-in from K09
};
static const RoomExit EX_B08[] = {
  {R_B02, EX_UP, GATE_NONE, 0, 64, 8}
};

static const RoomExit EX_K01[] = {
  {R_B06, EX_RIGHT, GATE_NONE, 0, 120, 40},
  {R_K02, EX_LEFT, GATE_NONE, 0, 8, 40}
};
static const RoomExit EX_K02[] = {
  {R_K01, EX_RIGHT, GATE_NONE, 0, 248, 40},
  {R_K03, EX_LEFT, GATE_NONE, 0, 8, 40}
};
static const RoomExit EX_K03[] = {
  {R_K02, EX_RIGHT, GATE_NONE, 0, 120, 40},
  {R_K04, EX_DOWN, GATE_BOMB, 3, 64, 52}
};
static const RoomExit EX_K04[] = {
  {R_K03, EX_UP, GATE_NONE, 0, 64, 8},
  {R_K05, EX_DOWN, GATE_NONE, 0, 64, 108}
};
static const RoomExit EX_K05[] = {
  {R_K04, EX_LEFT, GATE_NONE, 0, 8, 40},
  {R_K06, EX_RIGHT, GATE_NONE, 0, 248, 40}
};
static const RoomExit EX_K06[] = {
  {R_K05, EX_LEFT, GATE_NONE, 0, 8, 90},
  {R_K07, EX_RIGHT, GATE_NONE, 0, 248, 40}
};
static const RoomExit EX_K07[] = {
  {R_K06, EX_LEFT, GATE_NONE, 0, 8, 40},
  {R_K08, EX_RIGHT, GATE_NONE, 0, 120, 40}
};
static const RoomExit EX_K08[] = {
  {R_K07, EX_LEFT, GATE_NONE, 0, 8, 90},
  {R_K09, EX_RIGHT, GATE_AFTER_BOSS, 0, 248, 90}
};
static const RoomExit EX_K09[] = {
  {R_K08, EX_LEFT, GATE_NONE, 0, 8, 40},
  {R_K10, EX_DOWN, GATE_SCREW, 4, 64, 52},
  {R_B07, EX_DOWN, GATE_NONE, 0, 100, 52}  // one-way to B07
};
static const RoomExit EX_K10[] = {
  {R_K09, EX_UP, GATE_NONE, 0, 64, 8}
};

static const RoomExit EX_N01[] = {
  {R_B06, EX_LEFT, GATE_NONE, 0, 8, 40},
  {R_N02, EX_DOWN, GATE_NONE, 0, 64, 52}
};
static const RoomExit EX_N02[] = {
  {R_N01, EX_UP, GATE_NONE, 0, 32, 8},
  {R_N03, EX_DOWN, GATE_NONE, 0, 200, 52}
};
static const RoomExit EX_N03[] = {
  {R_N02, EX_UP, GATE_NONE, 0, 64, 8},
  {R_N04, EX_RIGHT, GATE_NONE, 0, 120, 150}
};
static const RoomExit EX_N04[] = {
  {R_N03, EX_LEFT, GATE_NONE, 0, 8, 40},
  {R_N05, EX_RIGHT, GATE_NONE, 0, 248, 40},
  {R_N06, EX_UP, GATE_SCREW, 5, 180, 12},
  {R_N10, EX_UP, GATE_NONE, 0, 40, 12}  // landing from N10
};
static const RoomExit EX_N05[] = {
  {R_N04, EX_LEFT, GATE_NONE, 0, 8, 90},
  {R_N06, EX_UP, GATE_NONE, 0, 200, 8},
  {R_N10, EX_UP, GATE_NONE, 0, 40, 8}
};
static const RoomExit EX_N06[] = {
  {R_N05, EX_DOWN, GATE_NONE, 0, 64, 52},
  {R_N07, EX_RIGHT, GATE_NONE, 0, 120, 40},
  {R_N04, EX_LEFT, GATE_SCREW, 5, 8, 40}
};
static const RoomExit EX_N07[] = {
  {R_N06, EX_LEFT, GATE_NONE, 0, 8, 40},
  {R_N08, EX_DOWN, GATE_NONE, 0, 64, 52}
};
static const RoomExit EX_N08[] = {
  {R_N07, EX_UP, GATE_NONE, 0, 64, 8},
  {R_N09, EX_RIGHT, GATE_AFTER_BOSS, 0, 248, 90}
};
static const RoomExit EX_N09[] = {
  {R_N08, EX_LEFT, GATE_NONE, 0, 8, 150},
  {R_B05, EX_UP, GATE_NONE, 0, 64, 8}  // one-way lift to B05
};
static const RoomExit EX_N10[] = {
  {R_N05, EX_DOWN, GATE_NONE, 0, 64, 52},
  {R_N04, EX_DOWN, GATE_NONE, 0, 100, 52}
};

static const RoomExit EX_T01[] = {
  {R_B07, EX_UP, GATE_NONE, 0, 64, 8},
  {R_T02, EX_DOWN, GATE_NONE, 0, 64, 52}
};
static const RoomExit EX_T02[] = {
  {R_T01, EX_UP, GATE_NONE, 0, 64, 8},
  {R_T03, EX_LEFT, GATE_METROID, 6, 8, 40}
};
static const RoomExit EX_T03[] = {
  {R_T02, EX_RIGHT, GATE_NONE, 0, 248, 40},
  {R_T04, EX_LEFT, GATE_NONE, 0, 8, 90}
};
static const RoomExit EX_T04[] = {
  {R_T03, EX_RIGHT, GATE_NONE, 0, 248, 40},
  {R_T05, EX_LEFT, GATE_NONE, 0, 8, 40},
  {R_T08, EX_DOWN, GATE_NONE, 0, 120, 52}
};
static const RoomExit EX_T05[] = {
  {R_T04, EX_RIGHT, GATE_NONE, 0, 120, 40},  // locked during alarm
  {R_T06, EX_DOWN, GATE_NONE, 0, 64, 52},
  {R_T07, EX_LEFT, GATE_ESCAPE, 0, 8, 40}
};
static const RoomExit EX_T06[] = {
  {R_T05, EX_UP, GATE_NONE, 0, 64, 8}
};
static const RoomExit EX_T07[] = {
  {R_T05, EX_RIGHT, GATE_ESCAPE, 0, 120, 200},
  {R_B01, EX_UP, GATE_ESCAPE, 0, 64, 8}
};
static const RoomExit EX_T08[] = {
  {R_T04, EX_UP, GATE_NONE, 0, 64, 8}
};

// -------------------- platforms --------------------
// Brinstar
static const uint32_t PL_B01[] = {
  PL(0, 52, 256), PL(40, 36, 48), PL(160, 44, 40)
};
static const uint32_t PL_B02[] = {
  PL(0, 52, 256), PL(60, 40, 40), PL(140, 32, 36), PL(100, 52, 40) // seam area
};
static const uint32_t PL_B03[] = {
  PL(0, 52, 128), PL(70, 40, 40)
};
static const uint32_t PL_B04[] = {
  PL(0, 52, 128), PL(0, 20, 128) // low ceiling via flags; floor only
};
static const uint32_t PL_B05[] = {
  PL(0, 52, 128), PL(0, 108, 128), PL(40, 36, 40), PL(30, 80, 50)
};
static const uint32_t PL_B06[] = {
  PL(0, 52, 256), PL(0, 108, 256),
  PL(40, 36, 40), PL(160, 36, 40),
  PL(40, 90, 40), PL(160, 90, 40),
  PL(100, 70, 56)
};
static const uint32_t PL_B07[] = {
  PL(0, 52, 128), PL(40, 40, 48)
};
static const uint32_t PL_B08[] = {
  PL(0, 52, 128), PL(40, 40, 32)
};

static const uint32_t PL_K01[] = {PL(0, 52, 128)};
static const uint32_t PL_K02[] = {
  PL(0, 52, 256), PL(40, 44, 32), PL(100, 36, 40), PL(180, 44, 32)
};
static const uint32_t PL_K03[] = {PL(0, 52, 128)};
static const uint32_t PL_K04[] = {
  PL(0, 52, 128), PL(0, 108, 128),
  PL(20, 40, 28), PL(60, 70, 28), PL(30, 90, 28)
};
static const uint32_t PL_K05[] = {
  PL(0, 52, 256), PL(80, 40, 40), PL(160, 36, 40)
};
static const uint32_t PL_K06[] = {
  PL(0, 52, 256), PL(0, 108, 256),
  PL(30, 40, 40), PL(100, 70, 40), PL(160, 40, 40),
  PL(50, 90, 36), PL(180, 90, 40)
};
static const uint32_t PL_K07[] = {PL(0, 52, 128)};
static const uint32_t PL_K08[] = {
  PL(0, 108, 256), PL(20, 90, 40), PL(180, 90, 40)
};
static const uint32_t PL_K09[] = {PL(0, 52, 128)};
static const uint32_t PL_K10[] = {PL(0, 52, 128), PL(40, 40, 40)};

static const uint32_t PL_N01[] = {PL(0, 52, 128)};
static const uint32_t PL_N02[] = {
  PL(0, 52, 60), PL(100, 40, 40), PL(180, 52, 76)
};
static const uint32_t PL_N03[] = {
  PL(0, 52, 128), PL(0, 108, 128), PL(0, 164, 128),
  PL(20, 40, 28), PL(70, 70, 28), PL(30, 100, 28),
  PL(70, 130, 28), PL(20, 150, 40)
};
static const uint32_t PL_N04[] = {
  PL(0, 52, 256), PL(60, 36, 40), PL(160, 28, 40)
};
static const uint32_t PL_N05[] = {
  PL(0, 52, 256), PL(0, 108, 256),
  PL(40, 40, 40), PL(140, 70, 40), PL(200, 40, 40),
  PL(30, 90, 40)
};
static const uint32_t PL_N06[] = {PL(0, 52, 128), PL(50, 36, 40)};
static const uint32_t PL_N07[] = {PL(0, 52, 128)};
static const uint32_t PL_N08[] = {
  PL(0, 108, 256), PL(20, 90, 40), PL(180, 90, 40)
};
static const uint32_t PL_N09[] = {
  PL(0, 52, 128), PL(0, 108, 128), PL(0, 164, 128),
  PL(40, 40, 40), PL(40, 90, 40), PL(40, 140, 40)
};
static const uint32_t PL_N10[] = {PL(0, 52, 128)};

static const uint32_t PL_T01[] = {PL(0, 52, 128)};
static const uint32_t PL_T02[] = {PL(0, 52, 128)};
static const uint32_t PL_T03[] = {
  PL(0, 52, 256), PL(0, 108, 256),
  PL(40, 40, 40), PL(140, 70, 50), PL(40, 90, 60)
};
static const uint32_t PL_T04[] = {
  PL(0, 52, 256), PL(60, 40, 40), PL(160, 40, 40)
};
static const uint32_t PL_T05[] = {PL(0, 52, 128)};
static const uint32_t PL_T06[] = {
  PL(0, 108, 256), PL(20, 90, 40), PL(100, 70, 40)
};
static const uint32_t PL_T07[] = {
  PL(0, 52, 128), PL(0, 108, 128), PL(0, 164, 128), PL(0, 220, 128),
  PL(30, 40, 36), PL(70, 70, 36), PL(30, 100, 36),
  PL(70, 130, 36), PL(30, 160, 36), PL(70, 190, 36)
};
static const uint32_t PL_T08[] = {PL(0, 52, 128)};

// enemies: xLo,xHi,y,kind
static const uint8_t EN_NONE[] = {0};
static const uint8_t EN_B02[] = {160, 0, 48, EN_ZOOMER};
static const uint8_t EN_K02[] = {
  80, 0, 48, EN_ZOOMER,
  160, 0, 48, EN_SWIPE
};
static const uint8_t EN_K06[] = {
  90, 0, 100, EN_ZOOMER,
  180, 0, 48, EN_RIPPER
};
static const uint8_t EN_N02[] = {140, 0, 48, EN_ZOOMER};
static const uint8_t EN_N05[] = {
  160, 0, 100, EN_BOLT,
  80, 0, 48, EN_SKREE
};
static const uint8_t EN_N06[] = {70, 0, 48, EN_ZOOMER};
static const uint8_t EN_T02[] = {90, 0, 36, EN_METROID};
static const uint8_t EN_T03[] = {
  80, 0, 40, EN_METROID,
  180, 0, 80, EN_METROID
};
static const uint8_t EN_T04[] = {
  100, 0, 44, EN_TURRET,
  180, 0, 44, EN_TURRET
};
static const uint8_t EN_T08[] = {90, 0, 44, EN_TURRET};

// pickups: xLo,xHi,y,kind,id  (id = tank bit 0..7 for missiles, 0..1 for energy)
static const uint8_t PK_NONE[] = {0};
static const uint8_t PK_B03[] = {90, 0, 44, PK_MORPH, 0xFF};
static const uint8_t PK_B05[] = {70, 0, 32, PK_LAUNCHER, 0};   // tank #1
static const uint8_t PK_B08[] = {64, 0, 44, PK_MTANK, 1};      // tank #2
static const uint8_t PK_K03[] = {90, 0, 44, PK_BOMBS, 0xFF};
static const uint8_t PK_K05[] = {140, 0, 32, PK_MTANK, 2};     // tank #3
static const uint8_t PK_K06[] = {50, 0, 80, PK_ETANK, 0};      // energy #1
static const uint8_t PK_K10[] = {90, 0, 44, PK_MTANK, 3};      // tank #4
static const uint8_t PK_N03[] = {100, 0, 140, PK_ETANK, 1};    // energy #2
static const uint8_t PK_N04[] = {80, 0, 28, PK_MTANK, 4};      // tank #5
static const uint8_t PK_N06[] = {90, 0, 32, PK_MTANK, 5};      // tank #6
static const uint8_t PK_N10[] = {64, 0, 44, PK_MTANK, 6};      // tank #7
static const uint8_t PK_T08[] = {90, 0, 44, PK_MTANK, 7};      // tank #8

#define ROOM(sw, sh, zn, fl, ex, pl, en, pk) \
  {sw, sh, zn, fl, \
   (uint8_t)(sizeof(ex) / sizeof(ex[0])), \
   (uint8_t)(sizeof(pl) / sizeof(pl[0])), \
   (uint8_t)((sizeof(en) > 1) ? (sizeof(en) / 4) : 0), \
   (uint8_t)((sizeof(pk) > 1) ? (sizeof(pk) / 5) : 0), \
   ex, pl, en, pk}

// flags: bit0 save, bit1 ship, bit2 Kraid, bit3 Ridley, bit4 MB, bit5 lava, bit6 lowceil
static const RoomDef ROOMS[ROOM_COUNT] = {
  // Brinstar
  ROOM(2, 1, 0, 0x03, EX_B01, PL_B01, EN_NONE, PK_NONE),
  ROOM(2, 1, 0, 0x00, EX_B02, PL_B02, EN_B02, PK_NONE),
  ROOM(1, 1, 0, 0x00, EX_B03, PL_B03, EN_NONE, PK_B03),
  ROOM(1, 1, 0, 0x40, EX_B04, PL_B04, EN_NONE, PK_NONE),
  ROOM(1, 2, 0, 0x00, EX_B05, PL_B05, EN_NONE, PK_B05),
  ROOM(2, 2, 0, 0x00, EX_B06, PL_B06, EN_NONE, PK_NONE),
  ROOM(1, 1, 0, 0x00, EX_B07, PL_B07, EN_NONE, PK_NONE),
  ROOM(1, 1, 0, 0x00, EX_B08, PL_B08, EN_NONE, PK_B08),
  // Kraid
  ROOM(1, 1, 1, 0x00, EX_K01, PL_K01, EN_NONE, PK_NONE),
  ROOM(2, 1, 1, 0x00, EX_K02, PL_K02, EN_K02, PK_NONE),
  ROOM(1, 1, 1, 0x00, EX_K03, PL_K03, EN_NONE, PK_K03),
  ROOM(1, 2, 1, 0x00, EX_K04, PL_K04, EN_NONE, PK_NONE),
  ROOM(2, 1, 1, 0x00, EX_K05, PL_K05, EN_NONE, PK_K05),
  ROOM(2, 2, 1, 0x00, EX_K06, PL_K06, EN_K06, PK_K06),
  ROOM(1, 1, 1, 0x01, EX_K07, PL_K07, EN_NONE, PK_NONE),
  ROOM(2, 2, 1, 0x04, EX_K08, PL_K08, EN_NONE, PK_NONE),
  ROOM(1, 1, 1, 0x00, EX_K09, PL_K09, EN_NONE, PK_NONE),
  ROOM(1, 1, 1, 0x00, EX_K10, PL_K10, EN_NONE, PK_K10),
  // Norfair
  ROOM(1, 1, 2, 0x00, EX_N01, PL_N01, EN_NONE, PK_NONE),
  ROOM(2, 1, 2, 0x20, EX_N02, PL_N02, EN_N02, PK_NONE),
  ROOM(1, 3, 2, 0x00, EX_N03, PL_N03, EN_NONE, PK_N03),
  ROOM(2, 1, 2, 0x00, EX_N04, PL_N04, EN_NONE, PK_N04),
  ROOM(2, 2, 2, 0x20, EX_N05, PL_N05, EN_N05, PK_NONE),
  ROOM(1, 1, 2, 0x00, EX_N06, PL_N06, EN_N06, PK_N06),
  ROOM(1, 1, 2, 0x01, EX_N07, PL_N07, EN_NONE, PK_NONE),
  ROOM(2, 2, 2, 0x08, EX_N08, PL_N08, EN_NONE, PK_NONE),
  ROOM(1, 3, 2, 0x00, EX_N09, PL_N09, EN_NONE, PK_NONE),
  ROOM(1, 1, 2, 0x00, EX_N10, PL_N10, EN_NONE, PK_N10),
  // Tourian
  ROOM(1, 1, 3, 0x01, EX_T01, PL_T01, EN_NONE, PK_NONE),
  ROOM(1, 1, 3, 0x00, EX_T02, PL_T02, EN_T02, PK_NONE),
  ROOM(2, 2, 3, 0x00, EX_T03, PL_T03, EN_T03, PK_NONE),
  ROOM(2, 1, 3, 0x00, EX_T04, PL_T04, EN_T04, PK_NONE),
  ROOM(1, 1, 3, 0x01, EX_T05, PL_T05, EN_NONE, PK_NONE),
  ROOM(2, 2, 3, 0x10, EX_T06, PL_T06, EN_NONE, PK_NONE),
  ROOM(1, 4, 3, 0x00, EX_T07, PL_T07, EN_NONE, PK_NONE),
  ROOM(1, 1, 3, 0x00, EX_T08, PL_T08, EN_T08, PK_T08),
};

#undef ROOM
#undef PL

static inline uint16_t roomWpx(const RoomDef &r) { return (uint16_t)r.screensW * 128; }
static inline uint16_t roomHpx(const RoomDef &r) { return (uint16_t)r.screensH * 56; }

static inline uint16_t platX(const RoomDef &r, uint8_t i) {
  return (uint16_t)(r.plats[i] & 0xFFFF);
}
static inline uint8_t platY(const RoomDef &r, uint8_t i) {
  return (uint8_t)((r.plats[i] >> 16) & 0xFF);
}
static inline uint8_t platW(const RoomDef &r, uint8_t i) {
  return (uint8_t)((r.plats[i] >> 24) & 0xFF);
}
static inline uint16_t enemyX(const RoomDef &r, uint8_t i) {
  const uint8_t *p = &r.enemies[i * 4];
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint8_t enemyY(const RoomDef &r, uint8_t i) { return r.enemies[i * 4 + 2]; }
static inline uint8_t enemyKind(const RoomDef &r, uint8_t i) { return r.enemies[i * 4 + 3]; }
static inline uint16_t pickupX(const RoomDef &r, uint8_t i) {
  const uint8_t *p = &r.pickups[i * 5];
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint8_t pickupY(const RoomDef &r, uint8_t i) { return r.pickups[i * 5 + 2]; }
static inline uint8_t pickupKind(const RoomDef &r, uint8_t i) { return r.pickups[i * 5 + 3]; }
static inline uint8_t pickupId(const RoomDef &r, uint8_t i) { return r.pickups[i * 5 + 4]; }
