#pragma once
// Sonic Watch — hand-designed act layouts.
//
// Each act = a terrain profile + a placed object list.
//   Terrain: {x, groundY} points, linearly interpolated (world y; bigger = lower).
//            Keep slopes gentle (≤ ~8 px rise per 32 px) — the ground is a surface, not walls.
//   Objects: {x, h, kind, arg}; h = height above the local ground (px).
//
// Rough sizes for designing: Sonic ≈ 16 px tall, jump ≈ 27 px, spring ≈ 60 px,
// screen = 128 × 64, top speed 6 px/step (~360 px/s).
//
// Par = the fastest realistic clear, in seconds (rank: A+ ≤ par+5, B ≤ par+12, else D).
// These are estimates (length / ~240 px/s + set pieces / boss) — tune them after playtesting.

#include <Arduino.h>

struct TerrainPt { int16_t x; uint8_t y; };
struct LvlObj { int16_t x; uint8_t h; uint8_t kind; uint8_t arg; };
struct ActDef {
  const TerrainPt *ter; uint8_t nTer;
  const LvlObj *obj; uint8_t nObj;
  int16_t len;        // signpost (Act 1) / boss arena (Act 2) x
  uint8_t parSec;
};

enum : uint8_t {
  // single objects (same ids as the game's object kinds)
  L_SPIKE = 0, L_RING = 1, L_CRAB = 2, L_SPRING = 3, L_BEE = 4, L_DECO = 5,
  L_WALL = 7,      // arg = height (px)
  L_FLAME = 8, L_BUMPER = 9, L_SAW = 10,
  // builders
  L_PLAT = 20,     // one-way platform: arg = width (≤ 48), h = top above ground
  L_RINGS = 21,    // arg = count, 12 px apart, at h
  L_ARC = 22,      // arg = count, 12 px apart, arcing up to h in the middle
  L_SPIKES = 23,   // arg = number of spike clusters, 12 px apart
  L_LOOP = 24,     // loop mouth at x (needs ~110 px flat before, ~130 after)
  L_CHEMX = 25,    // Chemical Plant wavy crossing tubes from x (~130 px, flat)
};

// deco args
enum : uint8_t { D_SUN = 0, D_PALM = 1, D_PIPE = 2, D_NEON = 3, D_GEAR = 4 };

#define ACT(t, o, len, par) { t, (uint8_t)(sizeof(t) / sizeof(t[0])), o, (uint8_t)(sizeof(o) / sizeof(o[0])), len, par }

// =====================================================================
// GREEN HILL ACT 1 — rolling hills, an upper spring route early, the big
// downhill into the loop, a spike-pit bridge, a sky route over the peak,
// and a fast downhill finish.
// =====================================================================
static const TerrainPt GH1_T[] = {
  {0, 50}, {160, 50}, {240, 44}, {320, 38}, {400, 38}, {480, 46}, {560, 52},
  {640, 48}, {720, 40}, {800, 40}, {880, 48}, {960, 54}, {1040, 50}, {1120, 40},
  {1200, 32}, {1280, 32},
  {1470, 52},                       // long downhill → loop runway
  {1880, 52},                       // flat: loop at 1600
  {1960, 46}, {2040, 40}, {2120, 40}, {2200, 46}, {2260, 52},
  {2560, 52},                       // spike valley
  {2640, 46}, {2720, 40}, {2800, 36}, {2880, 36}, {2960, 44}, {3040, 52},
  {3080, 56}, {3360, 56},           // spike-pit bridge
  {3440, 50}, {3520, 44}, {3600, 40}, {3680, 44}, {3760, 50}, {3840, 54},
  {3920, 48}, {4000, 40}, {4080, 34}, {4160, 30}, {4260, 30},   // the peak
  {4340, 36}, {4420, 42}, {4500, 48}, {4580, 52}, {4660, 48}, {4740, 44},
  {4820, 44}, {4900, 48}, {4980, 52}, {5060, 44}, {5140, 38}, {5220, 38},
  {5420, 50}, {5600, 54}, {6100, 54},                           // downhill finish
};
static const LvlObj GH1_O[] = {
  {40, 0, L_DECO, D_SUN}, {96, 0, L_DECO, D_PALM},
  {120, 8, L_RINGS, 5},
  {270, 0, L_CRAB, 0},
  {320, 24, L_ARC, 6},                                   // over the first hill
  {430, 0, L_DECO, D_SUN},
  {520, 30, L_BEE, 0},
  {548, 8, L_RINGS, 3},
  // early upper route: spring → three ledges with rings
  {616, 0, L_SPRING, 0},
  {650, 44, L_PLAT, 40}, {656, 52, L_RINGS, 3},
  {706, 48, L_PLAT, 32}, {710, 56, L_RINGS, 2},
  {756, 44, L_PLAT, 40}, {762, 52, L_RINGS, 3},
  {740, 0, L_CRAB, 0},                                    // ground route below
  {850, 0, L_SPIKES, 2},
  {940, 0, L_DECO, D_PALM},
  {952, 8, L_RINGS, 5},
  {1010, 0, L_CRAB, 0},
  {1090, 0, L_WALL, 14},
  {1150, 28, L_BEE, 0},
  {1206, 20, L_ARC, 6},                                  // hilltop
  {1300, 0, L_DECO, D_SUN},
  {1320, 8, L_RINGS, 6},                                 // speed line down the hill
  {1420, 0, L_DECO, D_PALM},
  {1520, 8, L_RINGS, 4},
  {1600, 0, L_LOOP, 0},                                  // ★ the loop
  {1790, 0, L_DECO, D_PALM}, {1830, 8, L_RINGS, 5},
  {1934, 0, L_SPRING, 0}, {1970, 52, L_PLAT, 40}, {1976, 60, L_RINGS, 3},
  {2030, 0, L_CRAB, 0},
  {2080, 0, L_SPIKES, 1},
  {2120, 26, L_BEE, 0},
  {2210, 0, L_DECO, D_SUN},
  // spike valley: hop the ledges or take the high spring
  {2270, 18, L_PLAT, 40}, {2280, 0, L_SPIKES, 3}, {2276, 26, L_RINGS, 3},
  {2366, 0, L_CRAB, 0},
  {2410, 18, L_PLAT, 44}, {2420, 0, L_SPIKES, 3}, {2416, 26, L_RINGS, 3},
  {2500, 0, L_SPRING, 0}, {2530, 50, L_PLAT, 40}, {2534, 58, L_RINGS, 4},
  {2600, 30, L_BEE, 0},
  {2690, 8, L_RINGS, 5},
  {2800, 0, L_DECO, D_PALM},
  {2846, 0, L_CRAB, 0},
  {2900, 22, L_ARC, 5},
  // spike-pit bridge
  {3104, 16, L_PLAT, 24}, {3110, 24, L_RINGS, 2},
  {3120, 0, L_SPIKES, 6},
  {3150, 20, L_PLAT, 24}, {3156, 28, L_RINGS, 2},
  {3196, 16, L_PLAT, 24}, {3202, 24, L_RINGS, 2},
  {3240, 18, L_PLAT, 40}, {3250, 0, L_SPIKES, 3},
  {3310, 30, L_BEE, 0},
  {3400, 0, L_SPRING, 0}, {3446, 0, L_DECO, D_SUN},
  {3520, 0, L_CRAB, 0}, {3560, 8, L_RINGS, 4},
  {3640, 0, L_WALL, 16},
  {3700, 28, L_BEE, 0},
  {3780, 0, L_DECO, D_PALM},
  {3830, 8, L_RINGS, 5},
  {3910, 0, L_CRAB, 0},
  {4000, 24, L_ARC, 6},                                  // up the climb
  {4180, 0, L_DECO, D_SUN},
  {4204, 28, L_BEE, 0},
  // sky route over the peak
  {4222, 0, L_SPRING, 0},
  {4250, 56, L_PLAT, 40}, {4256, 64, L_RINGS, 3},
  {4310, 52, L_PLAT, 36}, {4316, 60, L_RINGS, 3},
  {4370, 8, L_RINGS, 6},
  {4460, 0, L_CRAB, 0},
  {4600, 0, L_SPIKES, 2},
  {4680, 0, L_DECO, D_PALM},
  {4720, 0, L_CRAB, 0},
  {4760, 8, L_RINGS, 4},
  {4830, 0, L_WALL, 14},
  {4866, 26, L_BEE, 0},
  {4960, 0, L_DECO, D_SUN},
  {5004, 0, L_CRAB, 0},
  {5110, 24, L_ARC, 5},
  {5240, 8, L_RINGS, 8},                                 // downhill finish
  {5500, 0, L_DECO, D_PALM},
  {5650, 0, L_DECO, D_SUN},
  {5700, 8, L_RINGS, 3},
};

// GREEN HILL ACT 2 — shorter: hills, a spring climb, a spike valley, then the Egg Wrecker.
static const TerrainPt GH2_T[] = {
  {0, 50}, {200, 50}, {280, 42}, {360, 36}, {440, 36}, {520, 44}, {600, 52},
  {760, 52}, {840, 46}, {920, 40}, {1000, 40}, {1080, 48}, {1160, 54}, {1440, 54},
  {1520, 46}, {1600, 38}, {1700, 38}, {1780, 44}, {1860, 50}, {1940, 46},
  {2020, 42}, {2100, 46}, {2200, 52}, {2800, 52},
};
static const LvlObj GH2_O[] = {
  {40, 0, L_DECO, D_PALM}, {100, 8, L_RINGS, 4},
  {230, 0, L_CRAB, 0},
  {360, 22, L_ARC, 6},
  {470, 0, L_DECO, D_SUN},
  {560, 28, L_BEE, 0},
  {640, 0, L_SPIKES, 2},
  {700, 8, L_RINGS, 4},
  {790, 0, L_SPRING, 0}, {826, 52, L_PLAT, 40}, {832, 60, L_RINGS, 3},
  {886, 50, L_PLAT, 36}, {890, 58, L_RINGS, 3},
  {900, 0, L_CRAB, 0},
  {1030, 0, L_DECO, D_PALM},
  {1100, 0, L_WALL, 14},
  {1180, 18, L_PLAT, 40}, {1190, 0, L_SPIKES, 3}, {1186, 26, L_RINGS, 3},
  {1260, 30, L_BEE, 0},
  {1300, 18, L_PLAT, 40}, {1310, 0, L_SPIKES, 3}, {1306, 26, L_RINGS, 3},
  {1400, 0, L_CRAB, 0},
  {1560, 24, L_ARC, 6},
  {1640, 0, L_DECO, D_SUN},
  {1720, 0, L_CRAB, 0},
  {1800, 8, L_RINGS, 5},
  {1900, 28, L_BEE, 0},
  {1980, 0, L_SPIKES, 1},
  {2060, 0, L_DECO, D_PALM},
  {2140, 8, L_RINGS, 6},
  {2260, 0, L_DECO, D_SUN},
  {2360, 8, L_RINGS, 4},
};

// =====================================================================
// CHEMICAL PLANT ACT 1 — the wavy crossing tubes at the start, flat industrial
// slabs joined by ramps, flame-jet trap corridors with tube runs above them,
// spring routes to high tubes, and a second wavy cross mid-act.
// =====================================================================
static const TerrainPt CP1_T[] = {
  {0, 50}, {360, 50}, {392, 42}, {640, 42}, {672, 50}, {900, 50}, {932, 46},
  {1200, 46}, {1264, 54}, {1600, 54}, {1632, 46}, {1664, 38}, {1900, 38},
  {1964, 50}, {2640, 50},          // second wavy cross at 2400
  {2672, 42}, {2900, 42}, {2964, 54}, {3300, 54}, {3332, 46}, {3600, 46},
  {3632, 38}, {3900, 38}, {3964, 50}, {4300, 50}, {4332, 42}, {4600, 42},
  {4664, 54}, {5000, 54}, {5032, 48}, {5800, 48},
};
static const LvlObj CP1_O[] = {
  {36, 0, L_CHEMX, 0},                                   // ★ wavy crossing tubes
  {200, 0, L_DECO, D_PIPE}, {226, 8, L_RINGS, 4},
  {300, 0, L_FLAME, 0}, {330, 8, L_RINGS, 3},
  {420, 0, L_CRAB, 0},
  {470, 0, L_SPIKES, 2},
  {530, 0, L_FLAME, 0},
  {570, 0, L_DECO, D_PIPE}, {600, 8, L_RINGS, 3},
  // spring to a high tube run; the floor below is a trap corridor
  {690, 0, L_SPRING, 0},
  {724, 48, L_PLAT, 40}, {730, 56, L_RINGS, 3},
  {784, 52, L_PLAT, 40}, {790, 60, L_RINGS, 3},
  {844, 48, L_PLAT, 40}, {850, 56, L_RINGS, 3},
  {730, 0, L_FLAME, 0}, {766, 0, L_SPIKES, 2}, {820, 0, L_FLAME, 0},
  {880, 0, L_WALL, 16},
  {950, 0, L_DECO, D_PIPE},
  {990, 28, L_BEE, 0},
  {1040, 8, L_RINGS, 5},
  {1110, 0, L_CRAB, 0},
  {1160, 0, L_FLAME, 0},
  // low trap corridor with tubes overhead
  {1296, 20, L_PLAT, 44}, {1302, 28, L_RINGS, 3},
  {1300, 0, L_FLAME, 0}, {1336, 0, L_SPIKES, 2},
  {1356, 24, L_PLAT, 44}, {1362, 32, L_RINGS, 3},
  {1384, 0, L_FLAME, 0},
  {1416, 28, L_PLAT, 44}, {1422, 36, L_RINGS, 3},
  {1424, 0, L_WALL, 14},
  {1464, 0, L_FLAME, 0},
  {1476, 20, L_PLAT, 44}, {1482, 28, L_RINGS, 3},
  {1504, 0, L_SPIKES, 2},
  {1570, 0, L_DECO, D_PIPE},
  {1700, 0, L_CRAB, 0},
  {1740, 8, L_RINGS, 5},
  {1806, 26, L_BEE, 0},
  {1856, 0, L_FLAME, 0},
  {2000, 0, L_DECO, D_PIPE},
  {2040, 8, L_RINGS, 4},
  {2104, 0, L_CRAB, 0},
  {2164, 0, L_SPIKES, 2},
  {2226, 0, L_FLAME, 0},
  {2270, 0, L_DECO, D_PIPE},
  {2400, 0, L_CHEMX, 0},                                 // ★ second wavy cross
  {2560, 8, L_RINGS, 4},
  {2700, 0, L_FLAME, 0},
  {2730, 0, L_CRAB, 0},
  {2770, 18, L_PLAT, 44}, {2780, 0, L_SPIKES, 3}, {2776, 26, L_RINGS, 3},
  {2860, 0, L_WALL, 14},
  {2900, 28, L_BEE, 0},
  // second high tube run over a flame/spike floor
  {3000, 0, L_SPRING, 0},
  {3034, 50, L_PLAT, 40}, {3040, 58, L_RINGS, 3},
  {3094, 54, L_PLAT, 40}, {3100, 62, L_RINGS, 3},
  {3154, 50, L_PLAT, 40}, {3160, 58, L_RINGS, 3},
  {3030, 0, L_FLAME, 0}, {3060, 0, L_SPIKES, 3}, {3126, 0, L_FLAME, 0},
  {3180, 0, L_SPIKES, 3}, {3246, 0, L_FLAME, 0},
  {3384, 0, L_CRAB, 0},
  {3420, 8, L_RINGS, 5},
  {3490, 0, L_DECO, D_PIPE},
  {3524, 26, L_BEE, 0},
  {3566, 0, L_FLAME, 0},
  {3680, 0, L_WALL, 14},
  {3720, 8, L_RINGS, 4},
  {3786, 0, L_CRAB, 0},
  {3846, 0, L_SPIKES, 2},
  // triple flame-jet timing run
  {4000, 0, L_FLAME, 0}, {4040, 0, L_FLAME, 0}, {4080, 0, L_FLAME, 0},
  {4024, 24, L_RINGS, 4},
  {4146, 0, L_DECO, D_PIPE},
  {4206, 0, L_CRAB, 0},
  {4266, 28, L_BEE, 0},
  {4380, 0, L_SPRING, 0}, {4414, 50, L_PLAT, 44}, {4420, 58, L_RINGS, 4},
  {4400, 0, L_SPIKES, 4},
  {4486, 0, L_WALL, 16},
  {4540, 8, L_RINGS, 3},
  {4700, 0, L_FLAME, 0}, {4740, 0, L_SPIKES, 2},
  {4806, 0, L_CRAB, 0},
  {4866, 0, L_FLAME, 0},
  {4920, 8, L_RINGS, 5},
  {5100, 0, L_DECO, D_PIPE},
  {5160, 8, L_RINGS, 6},
  {5266, 26, L_BEE, 0},
  {5366, 0, L_CRAB, 0},
  {5440, 0, L_DECO, D_PIPE},
};

// CHEMICAL PLANT ACT 2 — a short trap run to the boss.
static const TerrainPt CP2_T[] = {
  {0, 50}, {300, 50}, {332, 42}, {600, 42}, {632, 50}, {900, 50}, {964, 54},
  {1300, 54}, {1332, 46}, {1600, 46}, {1632, 38}, {1900, 38}, {1964, 50},
  {2200, 50}, {2232, 52}, {2800, 52},
};
static const LvlObj CP2_O[] = {
  {60, 0, L_DECO, D_PIPE}, {100, 8, L_RINGS, 4},
  {200, 0, L_FLAME, 0},
  {380, 0, L_CRAB, 0}, {440, 0, L_SPIKES, 2}, {500, 0, L_FLAME, 0},
  {560, 8, L_RINGS, 3},
  {660, 0, L_SPRING, 0}, {694, 48, L_PLAT, 40}, {700, 56, L_RINGS, 3},
  {754, 52, L_PLAT, 40}, {760, 60, L_RINGS, 3},
  {700, 0, L_FLAME, 0}, {740, 0, L_SPIKES, 3}, {810, 0, L_FLAME, 0},
  {870, 0, L_WALL, 14},
  {1000, 0, L_FLAME, 0}, {1030, 20, L_PLAT, 44}, {1036, 28, L_RINGS, 3},
  {1060, 0, L_SPIKES, 2}, {1110, 0, L_FLAME, 0},
  {1170, 26, L_BEE, 0},
  {1240, 0, L_CRAB, 0},
  {1380, 8, L_RINGS, 5},
  {1440, 0, L_DECO, D_PIPE},
  {1500, 0, L_FLAME, 0}, {1540, 0, L_FLAME, 0},
  {1680, 0, L_WALL, 14},
  {1740, 0, L_CRAB, 0},
  {1800, 8, L_RINGS, 4},
  {1860, 0, L_SPIKES, 2},
  {2000, 0, L_DECO, D_PIPE},
  {2060, 28, L_BEE, 0},
  {2140, 8, L_RINGS, 6},
  {2300, 0, L_DECO, D_PIPE},
  {2400, 8, L_RINGS, 4},
};

// =====================================================================
// CASINO NIGHT ACT 1 — rolling bowls; springs at the bottom of each bowl fire
// you into bumper clusters; long bumper chains that keep you bouncing; neon
// everywhere, lots of rings, and Buzz bees that shoot.
// =====================================================================
static const TerrainPt CN1_T[] = {
  {0, 48}, {200, 48}, {280, 54}, {360, 57}, {440, 54}, {520, 48}, {600, 44},
  {680, 48}, {760, 54}, {840, 57}, {920, 54}, {1000, 48}, {1160, 48},
  {1240, 54}, {1320, 57}, {1400, 54}, {1480, 48}, {1560, 44}, {1700, 44},
  {1780, 50}, {1860, 56}, {1940, 58}, {2020, 56}, {2100, 50}, {2180, 44},
  {2360, 44}, {2440, 50}, {2520, 56}, {2600, 57}, {2680, 56}, {2760, 50},
  {2840, 46}, {3000, 46}, {3080, 52}, {3160, 57}, {3240, 52}, {3320, 46},
  {3400, 42}, {3560, 42}, {3640, 48}, {3720, 54}, {3800, 57}, {3880, 54},
  {3960, 48}, {4120, 48}, {4200, 54}, {4280, 58}, {4360, 54}, {4440, 48},
  {4520, 44}, {4700, 44}, {4780, 50}, {4860, 54}, {4940, 50}, {5020, 46},
  {5800, 46},
};
static const LvlObj CN1_O[] = {
  {40, 0, L_DECO, D_NEON}, {90, 8, L_RINGS, 5},
  // bowl 1: spring into a bumper triangle
  {356, 0, L_SPRING, 0},
  {336, 44, L_BUMPER, 0}, {376, 44, L_BUMPER, 0}, {356, 62, L_BUMPER, 0},
  {330, 30, L_RINGS, 5},
  {470, 0, L_DECO, D_NEON},
  {560, 30, L_BEE, 0},
  {600, 10, L_ARC, 5},
  // bowl 2: bumper chain across the dip
  {760, 18, L_BUMPER, 0}, {800, 22, L_BUMPER, 0}, {840, 18, L_BUMPER, 0}, {880, 22, L_BUMPER, 0},
  {770, 40, L_RINGS, 9},
  {960, 0, L_CRAB, 0},
  {1040, 0, L_DECO, D_NEON},
  {1080, 8, L_RINGS, 5},
  {1150, 0, L_SPIKES, 2},
  // bowl 3: deep spring shot to a high ledge
  {1316, 0, L_SPRING, 0}, {1340, 56, L_PLAT, 44}, {1346, 64, L_RINGS, 4},
  {1290, 24, L_BUMPER, 0}, {1366, 24, L_BUMPER, 0},
  {1450, 28, L_BEE, 0},
  {1560, 0, L_DECO, D_NEON},
  {1600, 8, L_RINGS, 6},
  {1680, 0, L_WALL, 14},
  // the big bowl: pinball table
  {1860, 0, L_SPRING, 0}, {1940, 0, L_SPRING, 0}, {2020, 0, L_SPRING, 0},
  {1880, 40, L_BUMPER, 0}, {1920, 50, L_BUMPER, 0}, {1960, 40, L_BUMPER, 0},
  {2000, 50, L_BUMPER, 0}, {1900, 64, L_BUMPER, 0}, {1980, 64, L_BUMPER, 0},
  {1870, 28, L_RINGS, 6}, {1916, 76, L_RINGS, 6},
  {2120, 0, L_DECO, D_NEON},
  {2200, 0, L_CRAB, 0},
  {2250, 28, L_BEE, 0},
  {2290, 8, L_RINGS, 5},
  {2400, 18, L_PLAT, 40}, {2410, 0, L_SPIKES, 3}, {2406, 26, L_RINGS, 3},
  // bowl: bumper ladder up to a ring stash
  {2596, 0, L_SPRING, 0},
  {2576, 36, L_BUMPER, 0}, {2616, 50, L_BUMPER, 0}, {2576, 64, L_BUMPER, 0},
  {2604, 80, L_PLAT, 40}, {2610, 88, L_RINGS, 3},
  {2760, 0, L_DECO, D_NEON},
  {2840, 0, L_CRAB, 0},
  {2900, 8, L_RINGS, 6},
  {2980, 26, L_BEE, 0},
  {3080, 20, L_BUMPER, 0}, {3120, 24, L_BUMPER, 0}, {3160, 20, L_BUMPER, 0},
  {3200, 24, L_BUMPER, 0}, {3240, 20, L_BUMPER, 0},
  {3090, 42, L_RINGS, 12},
  {3340, 0, L_DECO, D_NEON},
  {3420, 0, L_WALL, 16},
  {3480, 8, L_RINGS, 5},
  {3540, 28, L_BEE, 0},
  {3796, 0, L_SPRING, 0},
  {3776, 44, L_BUMPER, 0}, {3816, 44, L_BUMPER, 0}, {3796, 62, L_BUMPER, 0},
  {3770, 30, L_RINGS, 5},
  {3940, 0, L_CRAB, 0},
  {4000, 0, L_DECO, D_NEON},
  {4050, 8, L_RINGS, 5},
  {4120, 0, L_SPIKES, 2},
  {4276, 0, L_SPRING, 0}, {4300, 56, L_PLAT, 44}, {4306, 64, L_RINGS, 4},
  {4240, 22, L_BUMPER, 0}, {4320, 22, L_BUMPER, 0},
  {4420, 30, L_BEE, 0},
  {4520, 0, L_DECO, D_NEON},
  {4560, 8, L_RINGS, 6},
  {4660, 0, L_CRAB, 0},
  {4800, 18, L_BUMPER, 0}, {4840, 22, L_BUMPER, 0}, {4880, 18, L_BUMPER, 0},
  {4810, 40, L_RINGS, 7},
  {4980, 0, L_DECO, D_NEON},
  {5060, 26, L_BEE, 0},
  {5120, 8, L_RINGS, 8},
  {5300, 0, L_DECO, D_NEON},
  {5400, 8, L_RINGS, 4},
};

// CASINO NIGHT ACT 2 — two pinball bowls and a bumper chain, then the boss.
static const TerrainPt CN2_T[] = {
  {0, 48}, {240, 48}, {320, 54}, {400, 57}, {480, 54}, {560, 48}, {760, 48},
  {840, 54}, {920, 58}, {1000, 54}, {1080, 48}, {1280, 48}, {1360, 54},
  {1440, 57}, {1520, 54}, {1600, 48}, {2000, 48}, {2080, 52}, {2800, 52},
};
static const LvlObj CN2_O[] = {
  {40, 0, L_DECO, D_NEON}, {80, 8, L_RINGS, 5},
  {396, 0, L_SPRING, 0},
  {376, 44, L_BUMPER, 0}, {416, 44, L_BUMPER, 0}, {396, 62, L_BUMPER, 0},
  {370, 30, L_RINGS, 5},
  {600, 0, L_DECO, D_NEON},
  {660, 28, L_BEE, 0},
  {700, 8, L_RINGS, 5},
  {916, 0, L_SPRING, 0},
  {880, 40, L_BUMPER, 0}, {956, 40, L_BUMPER, 0}, {918, 56, L_BUMPER, 0},
  {890, 26, L_RINGS, 5},
  {1100, 0, L_CRAB, 0},
  {1160, 0, L_DECO, D_NEON},
  {1360, 18, L_BUMPER, 0}, {1400, 22, L_BUMPER, 0}, {1440, 18, L_BUMPER, 0},
  {1480, 22, L_BUMPER, 0},
  {1370, 40, L_RINGS, 9},
  {1640, 0, L_DECO, D_NEON},
  {1700, 28, L_BEE, 0},
  {1760, 0, L_SPIKES, 2},
  {1840, 8, L_RINGS, 6},
  {2100, 0, L_DECO, D_NEON},
  {2200, 8, L_RINGS, 5},
};

// =====================================================================
// SCRAP BRAIN ACT 1 — choppy metal floor, buzz-saws low and high, flame
// jets, walls, spike pits crossed on girders, gears turning in the background.
// =====================================================================
static const TerrainPt SB1_T[] = {
  {0, 50}, {200, 50}, {224, 46}, {400, 46}, {424, 50}, {600, 50}, {632, 42},
  {800, 42}, {832, 50}, {1000, 50}, {1024, 54}, {1300, 54}, {1332, 46},
  {1500, 46}, {1524, 40}, {1700, 40}, {1732, 48}, {1900, 48}, {1924, 54},
  {2300, 54}, {2332, 46}, {2500, 46}, {2524, 50}, {2700, 50}, {2732, 42},
  {2900, 42}, {2932, 50}, {3100, 50}, {3124, 54}, {3500, 54}, {3532, 46},
  {3700, 46}, {3724, 40}, {3900, 40}, {3932, 48}, {4100, 48}, {4124, 54},
  {4500, 54}, {4532, 46}, {4700, 46}, {4724, 50}, {4900, 50}, {4932, 44},
  {5100, 44}, {5132, 50}, {5900, 50},
};
static const LvlObj SB1_O[] = {
  {40, 0, L_DECO, D_GEAR}, {80, 8, L_RINGS, 4},
  {180, 6, L_SAW, 0},
  {260, 0, L_DECO, D_GEAR},
  {300, 8, L_RINGS, 4},
  {360, 0, L_FLAME, 0},
  {450, 0, L_WALL, 14},
  {520, 28, L_SAW, 0}, {500, 8, L_RINGS, 5},             // high saw over a ring line
  {580, 30, L_BEE, 0},
  {680, 0, L_SPIKES, 2},
  {740, 0, L_FLAME, 0},
  {780, 8, L_RINGS, 3},
  {880, 6, L_SAW, 0},
  {930, 0, L_DECO, D_GEAR},
  // girder bridge over a spike pit
  {1040, 16, L_PLAT, 28}, {1050, 0, L_SPIKES, 8}, {1046, 24, L_RINGS, 2},
  {1090, 20, L_PLAT, 28}, {1096, 28, L_RINGS, 2},
  {1140, 16, L_PLAT, 28}, {1146, 24, L_RINGS, 2},
  {1150, 40, L_SAW, 0},
  {1190, 20, L_PLAT, 28}, {1196, 28, L_RINGS, 2},
  {1240, 30, L_BEE, 0},
  {1360, 0, L_FLAME, 0}, {1400, 0, L_FLAME, 0},
  {1440, 0, L_DECO, D_GEAR},
  {1480, 6, L_SAW, 0},
  {1560, 0, L_WALL, 16},
  {1600, 8, L_RINGS, 5},
  {1660, 0, L_CRAB, 0},
  {1760, 0, L_SPRING, 0}, {1794, 50, L_PLAT, 40}, {1800, 58, L_RINGS, 4},
  {1780, 0, L_SPIKES, 3}, {1840, 0, L_FLAME, 0},
  {1950, 0, L_DECO, D_GEAR},
  {1990, 6, L_SAW, 0}, {2050, 6, L_SAW, 0},
  {2010, 30, L_RINGS, 4},
  {2120, 0, L_FLAME, 0},
  {2160, 28, L_BEE, 0},
  {2220, 0, L_SPIKES, 3}, {2210, 18, L_PLAT, 44}, {2216, 26, L_RINGS, 3},
  {2360, 0, L_CRAB, 0},
  {2420, 8, L_RINGS, 4},
  {2470, 0, L_DECO, D_GEAR},
  {2560, 0, L_WALL, 14},
  {2620, 28, L_SAW, 0}, {2600, 8, L_RINGS, 5},
  {2760, 0, L_FLAME, 0}, {2800, 0, L_FLAME, 0}, {2840, 0, L_FLAME, 0},
  {2780, 24, L_RINGS, 4},
  {2960, 6, L_SAW, 0},
  {3000, 30, L_BEE, 0},
  {3040, 0, L_DECO, D_GEAR},
  // long girder bridge
  {3140, 16, L_PLAT, 28}, {3150, 0, L_SPIKES, 10}, {3146, 24, L_RINGS, 2},
  {3190, 22, L_PLAT, 28}, {3196, 30, L_RINGS, 2},
  {3240, 16, L_PLAT, 28}, {3246, 24, L_RINGS, 2},
  {3250, 42, L_SAW, 0},
  {3290, 22, L_PLAT, 28}, {3296, 30, L_RINGS, 2},
  {3340, 16, L_PLAT, 28}, {3346, 24, L_RINGS, 2},
  {3400, 30, L_BEE, 0},
  {3560, 0, L_CRAB, 0},
  {3600, 6, L_SAW, 0},
  {3640, 8, L_RINGS, 4},
  {3760, 0, L_WALL, 16},
  {3800, 0, L_FLAME, 0},
  {3840, 0, L_DECO, D_GEAR},
  {3960, 0, L_SPRING, 0}, {3994, 50, L_PLAT, 40}, {4000, 58, L_RINGS, 4},
  {3980, 0, L_SPIKES, 3}, {4040, 0, L_FLAME, 0},
  {4140, 6, L_SAW, 0}, {4200, 6, L_SAW, 0}, {4260, 6, L_SAW, 0},
  {4160, 30, L_RINGS, 6},
  {4320, 28, L_BEE, 0},
  {4380, 0, L_FLAME, 0},
  {4440, 0, L_DECO, D_GEAR},
  {4560, 0, L_CRAB, 0},
  {4620, 8, L_RINGS, 5},
  {4700, 28, L_SAW, 0},
  {4760, 0, L_WALL, 14},
  {4820, 0, L_SPIKES, 2},
  {4960, 0, L_FLAME, 0},
  {5000, 30, L_BEE, 0},
  {5060, 8, L_RINGS, 4},
  {5200, 0, L_DECO, D_GEAR},
  {5260, 6, L_SAW, 0},
  {5340, 8, L_RINGS, 6},
  {5500, 0, L_DECO, D_GEAR},
  {5560, 8, L_RINGS, 3},
};

// SCRAP BRAIN ACT 2 — saws, a girder bridge and flame jets, then the final boss.
static const TerrainPt SB2_T[] = {
  {0, 50}, {200, 50}, {224, 46}, {400, 46}, {424, 50}, {600, 50}, {624, 54},
  {1000, 54}, {1032, 46}, {1200, 46}, {1224, 40}, {1400, 40}, {1432, 48},
  {1700, 48}, {1724, 52}, {2800, 52},
};
static const LvlObj SB2_O[] = {
  {40, 0, L_DECO, D_GEAR}, {80, 8, L_RINGS, 4},
  {200, 6, L_SAW, 0},
  {300, 0, L_FLAME, 0}, {340, 0, L_FLAME, 0},
  {450, 0, L_WALL, 14},
  {520, 28, L_SAW, 0}, {500, 8, L_RINGS, 5},
  {640, 16, L_PLAT, 28}, {650, 0, L_SPIKES, 8}, {646, 24, L_RINGS, 2},
  {690, 20, L_PLAT, 28}, {696, 28, L_RINGS, 2},
  {740, 16, L_PLAT, 28}, {746, 24, L_RINGS, 2},
  {790, 20, L_PLAT, 28}, {796, 28, L_RINGS, 2},
  {760, 40, L_SAW, 0},
  {880, 30, L_BEE, 0},
  {960, 0, L_DECO, D_GEAR},
  {1080, 6, L_SAW, 0}, {1140, 6, L_SAW, 0},
  {1100, 30, L_RINGS, 4},
  {1260, 0, L_WALL, 16},
  {1300, 0, L_FLAME, 0}, {1340, 0, L_FLAME, 0},
  {1460, 0, L_CRAB, 0},
  {1500, 8, L_RINGS, 5},
  {1560, 28, L_BEE, 0},
  {1640, 0, L_SPIKES, 3},
  {1760, 0, L_DECO, D_GEAR},
  {1840, 6, L_SAW, 0},
  {1920, 8, L_RINGS, 6},
  {2060, 0, L_FLAME, 0},
  {2120, 0, L_DECO, D_GEAR},
  {2200, 8, L_RINGS, 5},
};

// [zone][act]
static const ActDef ACTS[4][2] = {
  { ACT(GH1_T, GH1_O, 5900, 26), ACT(GH2_T, GH2_O, 2500, 24) },
  { ACT(CP1_T, CP1_O, 5520, 26), ACT(CP2_T, CP2_O, 2500, 24) },
  { ACT(CN1_T, CN1_O, 5560, 28), ACT(CN2_T, CN2_O, 2300, 24) },
  { ACT(SB1_T, SB1_O, 5700, 27), ACT(SB2_T, SB2_O, 2300, 24) },
};

#undef ACT
