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

struct TerrainPt { int16_t x; int16_t y; };   // y may go negative: high ground
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
  L_BRIDGE = 26,   // sagging log bridge from x, arg = length / 4, at the ground height at x.
                   // Dip the terrain underneath into a pool and bring it back up at the far end.
  L_FISH = 27,     // fish that leaps up out of the pool under a bridge
};
// Rule: spikes need open sky — never put a platform lower than 32 px (2 Sonic heights)
// over a spike; put spikes in the gaps between ledges instead.

// deco args
enum : uint8_t { D_SUN = 0, D_PALM = 1, D_PIPE = 2, D_NEON = 3, D_GEAR = 4, D_TOTEM = 5 };

#define ACT(t, o, len, par) { t, (uint8_t)(sizeof(t) / sizeof(t[0])), o, (uint8_t)(sizeof(o) / sizeof(o[0])), len, par }

// =====================================================================
// GREEN HILL ACT 1 — rolling hills, an upper spring route early, the big
// downhill into the loop, a spike-pit bridge, a sky route over the peak,
// and a fast downhill finish.
// =====================================================================
static const TerrainPt GH1_T[] = {
  {0, 50}, {160, 33}, {240, 19}, {320, 5}, {400, -4}, {480, -4}, {560, 2}, {640, -2},
  {720, -10}, {760, -10}, {800, -14}, {880, -15}, {960, -18}, {1040, -31}, {1120, -50},
  {1200, -50}, {1280, -43}, {1440, -11}, {1470, -8}, {1880, -8}, {1900, -10}, {1960, -20},
  {2040, -34}, {2120, -42}, {2200, -44}, {2260, -44}, {2300, -48}, {2560, -48},
  {2620, -52}, {2640, -51}, {2720, -46}, {2800, -38}, {2880, -27}, {2960, -7}, {3040, 12},
  {3060, 12}, {3108, 24}, {3252, 24}, {3300, 12}, {3320, 12}, {3360, 5}, {3440, -10},
  {3500, -24}, {3520, -28}, {3600, -38}, {3680, -40}, {3760, -40}, {3780, -40},
  {3828, -28}, {3892, -28}, {3940, -40}, {3960, -43}, {4000, -55}, {4080, -72},
  {4160, -87}, {4260, -100}, {4340, -94}, {4400, -90}, {4420, -86}, {4500, -70},
  {4580, -56}, {4660, -50}, {4740, -44}, {4800, -36}, {4820, -35}, {4900, -26},
  {4980, -17}, {5060, -20}, {5140, -22}, {5220, -17}, {5300, -7}, {5420, 15}, {5600, 42},
  {5700, 54}, {6100, 54},
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
  {2250, 18, L_PLAT, 30}, {2256, 26, L_RINGS, 2}, {2290, 0, L_SPIKES, 3}, {2296, 30, L_RINGS, 3},
  {2340, 0, L_DECO, D_TOTEM},
  {2366, 0, L_CRAB, 0},
  {2390, 18, L_PLAT, 30}, {2396, 26, L_RINGS, 2}, {2430, 0, L_SPIKES, 3}, {2436, 30, L_RINGS, 3},
  // ★ spring tower: three springs up to a sky route, then a long drop
  {2500, 0, L_SPRING, 0}, {2530, 50, L_PLAT, 40}, {2534, 58, L_RINGS, 2},
  {2562, 50, L_SPRING, 0},
  {2580, 100, L_PLAT, 40}, {2584, 108, L_RINGS, 2},
  {2612, 100, L_SPRING, 0},
  {2630, 150, L_PLAT, 44}, {2636, 158, L_RINGS, 3},
  {2690, 146, L_PLAT, 40}, {2696, 154, L_RINGS, 3},
  {2750, 140, L_PLAT, 40}, {2756, 148, L_RINGS, 3},
  {2810, 130, L_PLAT, 44}, {2814, 140, L_ARC, 4},
  {2600, 30, L_BEE, 0},
  {2690, 8, L_RINGS, 5},
  {2800, 0, L_DECO, D_PALM},
  {2846, 0, L_CRAB, 0},
  {2900, 22, L_ARC, 5},
  // ★ log bridge over a pool — fish leap up through it
  {3060, 0, L_BRIDGE, 60},
  {3076, 8, L_RINGS, 5}, {3190, 24, L_ARC, 7},
  {3124, 0, L_FISH, 0}, {3176, 0, L_FISH, 0}, {3232, 0, L_FISH, 0},
  {3310, 30, L_BEE, 0},
  {3400, 0, L_SPRING, 0}, {3446, 0, L_DECO, D_SUN},
  {3520, 0, L_CRAB, 0}, {3560, 8, L_RINGS, 4},
  {3640, 0, L_WALL, 16},
  {3700, 28, L_BEE, 0},
  {3760, 0, L_DECO, D_PALM},
  {3780, 0, L_BRIDGE, 40},                                // ★ second bridge
  {3796, 8, L_RINGS, 5},
  {3836, 0, L_FISH, 0}, {3884, 0, L_FISH, 0},
  {3966, 0, L_CRAB, 0},
  {4000, 24, L_ARC, 6},                                  // up the climb
  {4180, 0, L_DECO, D_SUN},
  {4204, 28, L_BEE, 0},
  // sky route over the peak
  {4222, 0, L_SPRING, 0},
  {4250, 56, L_PLAT, 40}, {4256, 64, L_RINGS, 3},
  {4310, 52, L_PLAT, 36}, {4316, 60, L_RINGS, 3},
  {4370, 8, L_RINGS, 6},
  {4460, 0, L_CRAB, 0},
  {4560, 0, L_DECO, D_TOTEM},
  {4600, 0, L_SPIKES, 2},
  {4680, 0, L_DECO, D_PALM},
  {4720, 0, L_CRAB, 0},
  {4760, 8, L_RINGS, 4},
  {4830, 0, L_WALL, 14},
  {4866, 26, L_BEE, 0},
  {4960, 0, L_DECO, D_SUN},
  {5004, 0, L_CRAB, 0},
  {5110, 24, L_ARC, 5},
  {5200, 0, L_DECO, D_TOTEM},
  {5240, 8, L_RINGS, 8},                                 // downhill finish
  {5500, 0, L_DECO, D_PALM},
  {5650, 0, L_DECO, D_SUN},
  {5700, 8, L_RINGS, 3},
};

// GREEN HILL ACT 2 — shorter: hills, a spring climb, a spike valley, then the Egg Wrecker.
static const TerrainPt GH2_T[] = {
  {0, 50}, {200, 30}, {280, 14}, {360, 0}, {400, -4}, {440, -4}, {520, 4}, {600, 12},
  {700, 12}, {760, 6}, {840, -8}, {920, -22}, {1000, -30}, {1080, -16}, {1140, -8},
  {1160, -8}, {1208, 4}, {1352, 4}, {1400, -8}, {1420, -8}, {1440, -10}, {1520, -25},
  {1600, -41}, {1700, -52}, {1780, -34}, {1860, -16}, {1900, -12}, {1940, -6}, {2020, 6},
  {2100, 26}, {2200, 52}, {2800, 52},
};
static const LvlObj GH2_O[] = {
  {40, 0, L_DECO, D_PALM}, {100, 8, L_RINGS, 4},
  {230, 0, L_CRAB, 0},
  {360, 22, L_ARC, 6},
  {470, 0, L_DECO, D_SUN},
  {560, 28, L_BEE, 0},
  {640, 0, L_SPIKES, 2},
  {700, 8, L_RINGS, 4},
  {740, 0, L_DECO, D_TOTEM},
  {790, 0, L_SPRING, 0}, {826, 52, L_PLAT, 40}, {832, 60, L_RINGS, 3},
  {886, 50, L_PLAT, 36}, {890, 58, L_RINGS, 3},
  {900, 0, L_CRAB, 0},
  {1030, 0, L_DECO, D_PALM},
  {1100, 0, L_WALL, 14},
  {1160, 0, L_BRIDGE, 60},                                // ★ log bridge, fish below
  {1176, 8, L_RINGS, 4}, {1270, 22, L_ARC, 6},
  {1216, 0, L_FISH, 0}, {1284, 0, L_FISH, 0}, {1344, 0, L_FISH, 0},
  {1260, 34, L_BEE, 0},
  {1436, 0, L_CRAB, 0},
  {1500, 0, L_DECO, D_TOTEM},
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
  {0, 50}, {200, 50}, {360, 35}, {392, 25}, {640, 2}, {672, 10}, {900, 10}, {932, 6},
  {1200, 6}, {1264, 14}, {1600, 14}, {1632, 3}, {1664, -9}, {1900, -32}, {1964, -20},
  {2640, -20}, {2672, -24}, {2900, 1}, {2964, 20}, {3000, 24}, {3300, 24}, {3332, 12},
  {3600, -19}, {3632, -31}, {3900, -62}, {3964, -50}, {4300, -50}, {4332, -54},
  {4600, -20}, {4664, 0}, {4700, 4}, {5000, 26}, {5032, 23}, {5100, 28}, {5800, 48},
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
  {1300, 0, L_FLAME, 0}, {1344, 0, L_SPIKE, 0},
  {1356, 24, L_PLAT, 44}, {1362, 32, L_RINGS, 3},
  {1384, 0, L_FLAME, 0},
  {1416, 28, L_PLAT, 44}, {1422, 36, L_RINGS, 3},
  {1424, 0, L_WALL, 14},
  {1464, 0, L_FLAME, 0},
  {1476, 20, L_PLAT, 44}, {1482, 28, L_RINGS, 3},
  {1524, 0, L_SPIKES, 2},
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
  {2750, 18, L_PLAT, 28}, {2756, 26, L_RINGS, 2}, {2786, 0, L_SPIKES, 3}, {2790, 30, L_RINGS, 3},
  {2860, 0, L_WALL, 14},
  {2900, 28, L_BEE, 0},
  // second high tube run over a flame/spike floor
  {3000, 0, L_SPRING, 0},
  {3034, 50, L_PLAT, 40}, {3040, 58, L_RINGS, 3},
  {3094, 54, L_PLAT, 40}, {3100, 62, L_RINGS, 2},
  // ★ spring tower off the tube run
  {3122, 54, L_SPRING, 0},
  {3140, 104, L_PLAT, 40}, {3146, 112, L_RINGS, 2},
  {3172, 104, L_SPRING, 0},
  {3190, 154, L_PLAT, 44}, {3196, 162, L_RINGS, 3},
  {3250, 150, L_PLAT, 40}, {3256, 158, L_RINGS, 3},
  {3310, 140, L_PLAT, 40}, {3316, 148, L_RINGS, 3},
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
  {0, 50}, {300, 30}, {332, 20}, {600, 2}, {632, 10}, {900, 10}, {964, 14}, {1000, 14},
  {1300, -6}, {1332, -16}, {1600, -34}, {1632, -40}, {1900, -26}, {1964, -10}, {2200, 40},
  {2232, 48}, {2250, 52}, {2800, 52},
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
  {1080, 0, L_SPIKES, 2}, {1110, 0, L_FLAME, 0},
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
  {0, 48}, {200, 33}, {280, 33}, {360, 30}, {440, 21}, {520, 9}, {600, -1}, {680, -3},
  {760, -3}, {800, -4}, {840, -3}, {920, -6}, {1000, -12}, {1160, -12}, {1200, -9},
  {1240, -9}, {1320, -13}, {1400, -22}, {1480, -34}, {1560, -45}, {1700, -56}, {1780, -50},
  {1860, -44}, {1940, -42}, {2020, -44}, {2100, -50}, {2180, -56}, {2360, -56},
  {2400, -53}, {2440, -45}, {2520, -30}, {2600, -19}, {2680, -10}, {2760, -7}, {2840, -1},
  {2900, 6}, {3000, -4}, {3080, -6}, {3160, -9}, {3240, -22}, {3320, -36}, {3400, -48},
  {3560, -48}, {3640, -42}, {3720, -36}, {3800, -33}, {3880, -36}, {3960, -42},
  {4120, -42}, {4200, -36}, {4280, -24}, {4360, -20}, {4440, -18}, {4520, -14}, {4700, 4},
  {4780, 13}, {4860, 20}, {4940, 20}, {5020, 19}, {5200, 26}, {5800, 46},
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
  {2380, 18, L_PLAT, 28}, {2386, 26, L_RINGS, 2}, {2416, 0, L_SPIKES, 3}, {2420, 30, L_RINGS, 3},
  // bowl: bumper ladder up to a ring stash
  {2596, 0, L_SPRING, 0},
  {2576, 36, L_BUMPER, 0}, {2616, 50, L_BUMPER, 0}, {2576, 64, L_BUMPER, 0},
  {2604, 80, L_PLAT, 40}, {2610, 88, L_RINGS, 2},
  // ★ keep climbing: spring off the stash ledge to the casino roof
  {2636, 80, L_SPRING, 0},
  {2654, 130, L_PLAT, 44}, {2660, 138, L_RINGS, 3},
  {2714, 124, L_PLAT, 40}, {2720, 132, L_RINGS, 3},
  {2774, 116, L_PLAT, 40}, {2776, 126, L_ARC, 4},
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
  {0, 48}, {240, 31}, {320, 31}, {400, 28}, {480, 20}, {560, 8}, {700, -2}, {760, -2},
  {840, 4}, {920, 8}, {1000, 4}, {1080, -2}, {1280, -2}, {1300, 0}, {1360, 8}, {1440, 18},
  {1520, 20}, {1600, 20}, {1700, 28}, {2000, 48}, {2080, 52}, {2800, 52},
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
  {0, 50}, {200, 37}, {224, 31}, {400, 19}, {424, 22}, {600, 10}, {632, 2}, {800, 2},
  {832, 10}, {1000, 10}, {1024, 12}, {1300, -16}, {1332, -24}, {1500, -24}, {1524, -30},
  {1700, -30}, {1732, -22}, {1900, -22}, {1924, -14}, {2300, 24}, {2332, 12}, {2500, -9},
  {2524, -8}, {2700, -30}, {2732, -38}, {2900, -38}, {2932, -30}, {3100, -30}, {3124, -24},
  {3500, 14}, {3532, 2}, {3700, -18}, {3724, -27}, {3900, -48}, {3932, -44}, {4000, -52},
  {4100, -54}, {4124, -48}, {4500, -56}, {4532, -60}, {4700, -39}, {4724, -32},
  {4900, -10}, {4932, -13}, {5100, 4}, {5132, 13}, {5300, 30}, {5900, 50},
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
  {1040, 16, L_PLAT, 24}, {1046, 24, L_RINGS, 2}, {1066, 0, L_SPIKES, 2},
  {1090, 20, L_PLAT, 24}, {1096, 28, L_RINGS, 2}, {1116, 0, L_SPIKES, 2},
  {1140, 16, L_PLAT, 24}, {1146, 24, L_RINGS, 2}, {1166, 0, L_SPIKES, 2},
  {1150, 44, L_SAW, 0},
  {1190, 20, L_PLAT, 24}, {1196, 28, L_RINGS, 2}, {1216, 0, L_SPIKES, 2},
  {1240, 30, L_BEE, 0},
  {1360, 0, L_FLAME, 0}, {1400, 0, L_FLAME, 0},
  {1440, 0, L_DECO, D_GEAR},
  {1480, 6, L_SAW, 0},
  {1560, 0, L_WALL, 16},
  {1600, 8, L_RINGS, 5},
  {1660, 0, L_CRAB, 0},
  // ★ spring tower up the machinery
  {1760, 0, L_SPRING, 0}, {1794, 50, L_PLAT, 40}, {1800, 58, L_RINGS, 2},
  {1826, 50, L_SPRING, 0},
  {1846, 100, L_PLAT, 40}, {1852, 108, L_RINGS, 2},
  {1878, 100, L_SPRING, 0},
  {1896, 150, L_PLAT, 44}, {1902, 158, L_RINGS, 3},
  {1956, 144, L_PLAT, 36}, {1960, 152, L_RINGS, 2},
  {2010, 136, L_PLAT, 36}, {2014, 144, L_RINGS, 2},
  {1780, 0, L_SPIKES, 3}, {1840, 0, L_FLAME, 0},
  {1950, 0, L_DECO, D_GEAR},
  {1990, 6, L_SAW, 0}, {2050, 6, L_SAW, 0},
  {2010, 30, L_RINGS, 4},
  {2120, 0, L_FLAME, 0},
  {2160, 28, L_BEE, 0},
  {2190, 18, L_PLAT, 28}, {2196, 26, L_RINGS, 2}, {2226, 0, L_SPIKES, 3}, {2230, 30, L_RINGS, 3},
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
  {3140, 16, L_PLAT, 24}, {3146, 24, L_RINGS, 2}, {3166, 0, L_SPIKES, 2},
  {3190, 22, L_PLAT, 24}, {3196, 30, L_RINGS, 2}, {3216, 0, L_SPIKES, 2},
  {3240, 16, L_PLAT, 24}, {3246, 24, L_RINGS, 2}, {3266, 0, L_SPIKES, 2},
  {3250, 46, L_SAW, 0},
  {3290, 22, L_PLAT, 24}, {3296, 30, L_RINGS, 2}, {3316, 0, L_SPIKES, 2},
  {3340, 16, L_PLAT, 24}, {3346, 24, L_RINGS, 2}, {3366, 0, L_SPIKES, 2},
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
  {0, 50}, {200, 35}, {224, 29}, {400, 16}, {424, 20}, {600, 20}, {624, 24}, {1000, 24},
  {1032, 13}, {1200, -4}, {1224, -12}, {1400, -30}, {1432, -19}, {1700, 8}, {1724, 15},
  {2000, 52}, {2800, 52},
};
static const LvlObj SB2_O[] = {
  {40, 0, L_DECO, D_GEAR}, {80, 8, L_RINGS, 4},
  {200, 6, L_SAW, 0},
  {300, 0, L_FLAME, 0}, {340, 0, L_FLAME, 0},
  {450, 0, L_WALL, 14},
  {520, 28, L_SAW, 0}, {500, 8, L_RINGS, 5},
  {640, 16, L_PLAT, 24}, {646, 24, L_RINGS, 2}, {666, 0, L_SPIKES, 2},
  {690, 20, L_PLAT, 24}, {696, 28, L_RINGS, 2}, {716, 0, L_SPIKES, 2},
  {740, 16, L_PLAT, 24}, {746, 24, L_RINGS, 2}, {766, 0, L_SPIKES, 2},
  {790, 20, L_PLAT, 24}, {796, 28, L_RINGS, 2}, {816, 0, L_SPIKES, 2},
  {760, 44, L_SAW, 0},
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
