#pragma once
// Sonic Watch — hand-designed act layouts.
//
// Each act = a terrain profile + a placed object list.
//   Terrain: {x, groundY} points, linearly interpolated (world y; bigger = lower).
//            Slopes: ≤ 12 px rise per 32 px (~21°, the steepest you can walk up from rest).
//            Cliffs: two points 1 px apart, e.g. {1061, -59}, {1062, -104}. A rise is a wall
//            (jump it if ≤ 30 px; taller needs a spring ~26 px before the face); a drop is a
//            ledge to fall off. Keep crabs ≥ 30 px and spikes/walls/flames ≥ 14 px from a cliff.
//   Objects: {x, h, kind, arg}; h = height above the local ground (px).
//
// Everything here is in DESIGN units; the game draws the world 5/4 bigger (see Z() in the
// sketch), so on screen 1 design px = 1.25 px and the 128 × 64 screen shows ~102 × 51 of it.
// Rough sizes (design units): Sonic ≈ 18 tall, jump ≈ 38, spring ≈ 62, top speed 6 px/step.
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
  {0, 50}, {160, 33}, {224, 22}, {225, -2}, {240, -5}, {320, -19}, {400, -28}, {480, -28},
  {560, -22}, {640, -26}, {720, -34}, {760, -34}, {800, -38}, {880, -39}, {960, -42}, {1040, -55},
  {1061, -59}, {1062, -104}, {1120, -118}, {1179, -118}, {1180, -62}, {1200, -62}, {1280, -55}, {1440, -23},
  {1470, -20}, {1880, -20}, {1900, -22}, {1960, -32}, {2040, -46}, {2120, -54}, {2171, -55}, {2172, -95},
  {2200, -96}, {2260, -96}, {2300, -100}, {2560, -100}, {2620, -104}, {2640, -103}, {2720, -98}, {2800, -90},
  {2880, -79}, {2960, -59}, {3004, -49}, {3005, 3}, {3040, 12}, {3060, 12}, {3108, 24}, {3252, 24},
  {3300, 12}, {3320, 12}, {3354, 7}, {3355, -22}, {3360, -23}, {3440, -38}, {3477, -46}, {3478, -74},
  {3500, -80}, {3520, -84}, {3600, -94}, {3680, -96}, {3760, -96}, {3780, -96}, {3828, -84}, {3892, -84},
  {3940, -96}, {3960, -99}, {4000, -111}, {4080, -128}, {4129, -137}, {4130, -173}, {4160, -179}, {4260, -192},
  {4340, -186}, {4400, -182}, {4420, -178}, {4500, -162}, {4514, -160}, {4515, -112}, {4580, -100}, {4660, -94},
  {4740, -88}, {4800, -80}, {4820, -79}, {4900, -70}, {4919, -68}, {4920, -24}, {4980, -17}, {5060, -20},
  {5140, -22}, {5220, -17}, {5300, -7}, {5420, 15}, {5600, 42}, {5700, 54}, {6100, 54},
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
  // springs up the tall cliff faces
  {1036, 0, L_SPRING, 0}, {2146, 0, L_SPRING, 0}, {4104, 0, L_SPRING, 0},
};

// GREEN HILL ACT 2 — shorter: hills, a spring climb, a spike valley, then the Egg Wrecker.
static const TerrainPt GH2_T[] = {
  {0, 50}, {200, 30}, {280, 14}, {299, 11}, {300, -17}, {360, -28}, {400, -32}, {440, -32},
  {520, -24}, {600, -16}, {700, -16}, {760, -22}, {840, -36}, {920, -50}, {989, -56}, {990, -97},
  {1000, -98}, {1080, -84}, {1140, -76}, {1160, -76}, {1208, -64}, {1352, -64}, {1400, -76}, {1420, -76},
  {1440, -78}, {1474, -84}, {1475, -108}, {1520, -117}, {1600, -133}, {1700, -144}, {1759, -131}, {1760, -83},
  {1780, -78}, {1860, -60}, {1900, -56}, {1939, -51}, {1940, -6}, {2020, 6}, {2100, 26}, {2200, 52},
  {2800, 52},
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
  // springs up the tall cliff faces
  {964, 0, L_SPRING, 0},
};

// =====================================================================
// CHEMICAL PLANT ACT 1 — the wavy crossing tubes at the start, flat industrial
// slabs joined by ramps, flame-jet trap corridors with tube runs above them,
// spring routes to high tubes, and a second wavy cross mid-act.
// =====================================================================
static const TerrainPt CP1_T[] = {
  {0, 50}, {200, 50}, {360, 35}, {392, 25}, {640, 2}, {672, 10}, {900, 10}, {932, 6},
  {1200, 6}, {1229, 9}, {1230, -35}, {1264, -30}, {1600, -30}, {1632, -41}, {1639, -43}, {1640, -72},
  {1664, -81}, {1900, -104}, {1929, -99}, {1930, -63}, {1964, -56}, {2339, -56}, {2340, -20}, {2640, -20},
  {2672, -24}, {2900, 1}, {2964, 20}, {3000, 24}, {3300, 24}, {3332, 12}, {3600, -19}, {3632, -31},
  {3649, -32}, {3650, -69}, {3900, -98}, {3934, -92}, {3935, -116}, {3964, -110}, {4300, -110}, {4319, -112},
  {4320, -82}, {4332, -84}, {4600, -50}, {4639, -38}, {4640, -8}, {4664, 0}, {4700, 4}, {5000, 26},
  {5032, 23}, {5100, 28}, {5800, 48},
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
  // springs up the tall cliff faces
  {1204, 0, L_SPRING, 0}, {3624, 0, L_SPRING, 0},
};

// CHEMICAL PLANT ACT 2 — a short trap run to the boss.
static const TerrainPt CP2_T[] = {
  {0, 50}, {299, 31}, {300, 2}, {332, -8}, {600, -26}, {632, -18}, {900, -18}, {964, -14},
  {1000, -14}, {1299, -33}, {1300, -74}, {1332, -84}, {1599, -101}, {1600, -122}, {1632, -128}, {1707, -125},
  {1708, -81}, {1900, -70}, {1949, -58}, {1950, -14}, {1964, -10}, {2200, 40}, {2232, 48}, {2250, 52},
  {2800, 52},
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
  // springs up the tall cliff faces
  {1274, 0, L_SPRING, 0},
};

// =====================================================================
// CASINO NIGHT ACT 1 — rolling bowls; springs at the bottom of each bowl fire
// you into bumper clusters; long bumper chains that keep you bouncing; neon
// everywhere, lots of rings, and Buzz bees that shoot.
// =====================================================================
static const TerrainPt CN1_T[] = {
  {0, 48}, {200, 33}, {249, 33}, {250, 5}, {280, 5}, {360, 2}, {440, -7}, {520, -19},
  {600, -29}, {680, -31}, {711, -31}, {712, -71}, {760, -71}, {800, -72}, {840, -71}, {920, -74},
  {999, -79}, {1000, -52}, {1160, -52}, {1200, -49}, {1240, -49}, {1320, -53}, {1400, -62}, {1480, -74},
  {1509, -77}, {1510, -106}, {1560, -113}, {1700, -124}, {1739, -122}, {1740, -161}, {1780, -158}, {1860, -152},
  {1940, -150}, {2020, -152}, {2079, -156}, {2080, -116}, {2100, -118}, {2180, -124}, {2360, -124}, {2400, -121},
  {2440, -113}, {2515, -99}, {2516, -63}, {2520, -62}, {2600, -51}, {2680, -42}, {2760, -39}, {2840, -33},
  {2900, -26}, {3000, -36}, {3029, -36}, {3030, -64}, {3080, -66}, {3160, -69}, {3240, -82}, {3320, -96},
  {3400, -108}, {3560, -108}, {3640, -102}, {3649, -102}, {3650, -72}, {3720, -66}, {3800, -63}, {3880, -66},
  {3960, -72}, {4120, -72}, {4200, -66}, {4280, -54}, {4360, -50}, {4440, -48}, {4469, -47}, {4470, -17},
  {4520, -14}, {4700, 4}, {4780, 13}, {4860, 20}, {4940, 20}, {5020, 19}, {5200, 26}, {5800, 46},
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
  // springs up the tall cliff faces
  {686, 0, L_SPRING, 0}, {1714, 0, L_SPRING, 0},
};

// CASINO NIGHT ACT 2 — two pinball bowls and a bumper chain, then the boss.
static const TerrainPt CN2_T[] = {
  {0, 48}, {240, 31}, {249, 31}, {250, 3}, {320, 3}, {400, 0}, {480, -8}, {503, -11},
  {504, -51}, {560, -60}, {700, -70}, {760, -70}, {840, -64}, {920, -60}, {1000, -64}, {1029, -66},
  {1030, -30}, {1080, -34}, {1249, -34}, {1250, -58}, {1280, -58}, {1300, -56}, {1360, -48}, {1440, -38},
  {1520, -36}, {1559, -36}, {1560, -8}, {1600, -8}, {1700, 0}, {1999, 19}, {2000, 48}, {2080, 52},
  {2800, 52},
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
  // springs up the tall cliff faces
  {478, 0, L_SPRING, 0},
};

// =====================================================================
// SCRAP BRAIN ACT 1 — choppy metal floor, buzz-saws low and high, flame
// jets, walls, spike pits crossed on girders, gears turning in the background.
// =====================================================================
static const TerrainPt SB1_T[] = {
  {0, 50}, {200, 37}, {224, 31}, {225, 7}, {400, -5}, {424, -2}, {600, -14}, {632, -22},
  {633, -22}, {634, -62}, {800, -62}, {832, -54}, {989, -54}, {990, -18}, {1000, -18}, {1024, -16},
  {1299, -43}, {1300, -72}, {1332, -80}, {1500, -80}, {1524, -86}, {1700, -86}, {1732, -78}, {1900, -78},
  {1924, -70}, {2299, -33}, {2300, -4}, {2332, -16}, {2500, -37}, {2509, -37}, {2510, -65}, {2524, -64},
  {2700, -86}, {2732, -94}, {2893, -94}, {2894, -130}, {2900, -130}, {2932, -122}, {3100, -122}, {3124, -116},
  {3479, -81}, {3480, -37}, {3500, -34}, {3532, -46}, {3700, -66}, {3724, -75}, {3899, -95}, {3900, -120},
  {3932, -116}, {4000, -124}, {4100, -126}, {4124, -120}, {4499, -127}, {4500, -92}, {4532, -96}, {4700, -75},
  {4724, -68}, {4899, -47}, {4900, -10}, {4932, -13}, {5100, 4}, {5132, 13}, {5300, 30}, {5900, 50},
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
  // springs up the tall cliff faces
  {608, 0, L_SPRING, 0}, {2868, 0, L_SPRING, 0},
};

// SCRAP BRAIN ACT 2 — saws, a girder bridge and flame jets, then the final boss.
static const TerrainPt SB2_T[] = {
  {0, 50}, {200, 35}, {224, 29}, {249, 28}, {250, 4}, {399, -7}, {400, -44}, {424, -40},
  {600, -40}, {624, -36}, {1000, -36}, {1009, -39}, {1010, -9}, {1032, -17}, {1200, -34}, {1224, -42},
  {1399, -59}, {1400, -84}, {1432, -73}, {1700, -46}, {1724, -39}, {1789, -31}, {1790, -3}, {2000, 26},
  {2024, 26}, {2025, 52}, {2800, 52},
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
  // springs up the tall cliff faces
  {374, 0, L_SPRING, 0},
};

// [zone][act]
static const ActDef ACTS[4][2] = {
  { ACT(GH1_T, GH1_O, 5900, 26), ACT(GH2_T, GH2_O, 2500, 24) },
  { ACT(CP1_T, CP1_O, 5520, 26), ACT(CP2_T, CP2_O, 2500, 24) },
  { ACT(CN1_T, CN1_O, 5560, 28), ACT(CN2_T, CN2_O, 2300, 24) },
  { ACT(SB1_T, SB1_O, 5700, 27), ACT(SB2_T, SB2_O, 2300, 24) },
};

#undef ACT
