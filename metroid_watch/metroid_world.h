#pragma once
// Metroid Watch — world map (Design Revision 4, Brinstar slice B01–B06)
//
// The world is one grid of "screens" (128×56 px = 16×7 tiles of 8 px). Each room sits at a map
// position (mx, my) and covers sw×sh screens. Rooms touch edge to edge, so walking or falling out
// of one room puts Samus straight into the neighbour at the same world position — openings only
// have to line up (tools/metroid_test/out/check verifies they do).
//
// Tiles (one char each, rows top to bottom):
//   ' ' empty       '#' rock          '=' rock ledge       'B' bomb block (cracked seam)
//   'D' blue door (any shot)   'R' red door (missile, then stays open)   'G' locked hatch
//   'h' ship hull (solid, drawn as the ship)   'S' save pad on the ship (solid)
// Objects (the tile itself is empty):
//   'm' Morph Ball  'l' Missile Launcher (+5)  't' missile tank (+5)  'e' Energy Tank (+2)
//   'z' Zoomer (crawls round surfaces)  'k' Skree (hangs from the ceiling, dives)
//   'r' Ripper (flies a lane, beam-proof)
// Doors sit in a room's edge column, 3 tiles tall; the room next door has its own door facing it.

#include <Arduino.h>

struct RoomDef {
  const char *id;
  const char *name;
  int8_t mx, my;      // map position (screens)
  uint8_t sw, sh;     // size in screens
  uint8_t zone;       // 0 Brinstar 1 Kraid 2 Norfair 3 Tourian
  const char *tiles;  // (sw*16) × (sh*7) chars
};

static const uint8_t TILE = 8;
static const uint8_t SCR_TW = 16, SCR_TH = 7;   // tiles per screen
static const int16_t SCR_W = 128, SCR_H = 56;   // px per screen

// ---------------------------------------------------------------- B01 Ship and Landing Site 2×2
static const char T_B01[] =
  "#                     ##########"
  "#                      #########"
  "#                        #######"
  "#                          #####"
  "#    ====                  #####"
  "#                          #####"
  "##                         #####"
  "##            ===       ########"
  "#                         ######"
  "#                          #####"
  "#                              D"
  "#        hhShh                 D"
  "#       hhhhhhh                D"
  "################################";

// ---------------------------------------------------------------- B02 Surface Path 2×1
// The cracked seam shows the way down to tank #2 (B08) before Samus has Bombs.
static const char T_B02[] =
  "################################"
  "#####    #######         #######"
  "##         ###           #######"
  "D           k                  D"
  "D                   ###        D"
  "D          z        ###   z    D"
  "##############BB################";

// ---------------------------------------------------------------- B03 Morph Ball Chamber 1×1
// Pedestal pickup; the low outlet on the right only fits the ball, then drops to B04.
static const char T_B03[] =
  "################"
  "####       #####"
  "##          ####"
  "D     m     ####"
  "D    ###    ####"
  "D    ###       #"
  "#############  #";

// ---------------------------------------------------------------- B04 Low Duct 1×1
// Standing pockets at both ends, a ball-only duct between them, drop to B05 on the left.
static const char T_B04[] =
  "#############  #"
  "#############  #"
  "#  ########    #"
  "#  ########    #"
  "#  ########    #"
  "#              #"
  "#  #############";

// ---------------------------------------------------------------- B05 Missile Landing 1×2
// Land on the left ledge; the launcher (with tank #1) sits on the right shelf.
static const char T_B05[] =
  "#  #############"
  "#  ######  #####"
  "#            ###"
  "#      r       #"
  "#              #"
  "####           #"
  "#              #"
  "#         l    #"
  "#       ########"
  "#         k    #"
  "#              #"
  "####           #"
  "#           z  #"
  "#####  #########";

// ---------------------------------------------------------------- B06 Junction Shaft 2×2
// Hub: red doors west (Kraid) and east (Norfair), locked hatch down to Tourian.
static const char T_B06[] =
  "#####  #########################"
  "#####  #######       ###########"
  "###       k            k   #####"
  "#                             ##"
  "#                              #"
  "#  ====                        #"
  "#               =====          #"
  "#                              #"
  "#      ====                ##  #"
  "###                        #####"
  "R                              R"
  "R   ===                        R"
  "R           z        z         R"
  "##############GG################";

static const RoomDef ROOMS[] = {
  {"B01", "LANDING SITE",   0, 0, 2, 2, 0, T_B01},
  {"B02", "SURFACE PATH",   2, 1, 2, 1, 0, T_B02},
  {"B03", "MORPH CHAMBER",  4, 1, 1, 1, 0, T_B03},
  {"B04", "LOW DUCT",       4, 2, 1, 1, 0, T_B04},
  {"B05", "MISSILE LANDING",4, 3, 1, 2, 0, T_B05},
  {"B06", "JUNCTION SHAFT", 4, 5, 2, 2, 0, T_B06},
};
static const uint8_t ROOM_N = sizeof(ROOMS) / sizeof(ROOMS[0]);
