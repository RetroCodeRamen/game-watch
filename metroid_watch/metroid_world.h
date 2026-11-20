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
//   '@' test-room start (empty)
// Objects (the tile itself is empty):
//   'm' Morph Ball  'l' Missile Launcher (+5)  'o' Bombs  — major items: 3 tiles above the floor, in an orb on a
//       Chozo statue's palm. Lower case: statue sits left of the orb, facing right; upper case
//       ('M', 'L', 'O'): statue on the right, facing left. Face it toward where Samus arrives.
//   't' missile tank (+5)  'e' Energy Tank (+2)
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
static const uint8_t ZONE_TEST = 9;             // the hidden test room

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

// ---------------------------------------------------------------- B03 Brinstar Shaft 1×4
// The spine of Brinstar and the road home. Open air above the top ledges (room to jump across).
// Doors near the top: west back to the surface, east to the
// Morph Ball shrine. Shelves step down the middle (4 tiles apart; the bottom one needs the double
// jump). At the bottom a ball-sized hole in each wall: east comes back from the shrine, west goes on
// to the Missile Room — both seen, out of reach, before Samus has the ball.
static const char T_B03[] =
  "################"
  "####        ####"
  "##            ##"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "D              D"
  "D              D"
  "D              D"
  "#######  #######"
  "#              #"
  "#              #"
  "#              #"
  "#     ####     #"
  "#              #"
  "#              #"
  "#              #"
  "#####     ######"
  "#              #"
  "#              #"
  "#              #"
  "                "
  "################"
  "################";

// ---------------------------------------------------------------- B04 Morph Ball Shrine 1×3
// One way in: the door ledge drops 13 tiles into the statue's chamber — deeper than a double jump.
// The Chozo statue holds the Morph Ball in an orb (shoot it open). The only way out is the ball
// tunnel low on the west wall into the bottom of the shaft: the item is the key to its own escape,
// and the way home.
static const char T_B04[] =
  "################"
  "######    ######"
  "####        ####"
  "D             ##"
  "D             ##"
  "D             ##"
  "####          ##"
  "####          ##"
  "####          ##"
  "####          ##"
  "####          ##"
  "####          ##"
  "####          ##"
  "####          ##"
  "####          ##"
  "####          ##"
  "####      M   ##"
  "####          ##"
  "              ##"
  "################"
  "################";

// ---------------------------------------------------------------- B05 Missile Room 2×2
// Rolled into from the shaft through the east duct (top right). Steps and ledges lead down to the
// floor, where the Missile Launcher sits in a Chozo statue's hands; the same steps climb back to the
// duct. The door on the east wall leads on to the junction.
static const char T_B05[] =
  "################################"
  "####                      ######"
  "####                          ##"
  "##                        ######"
  "##                              "
  "##              ################"
  "##                             #"
  "#           r                  #"
  "#                              #"
  "#     l     ###                D"
  "#           ###                D"
  "#        z  ###                D"
  "################################"
  "################################";

// ---------------------------------------------------------------- B06 Junction 2×2
// Hub. Blue door (upper west) back to the Missile Room; red doors west (Kraid) and east (Norfair);
// locked hatch down to Tourian. A step and a ledge climb back up to the blue door.
static const char T_B06[] =
  "################################"
  "#                      #########"
  "D                       k    ###"
  "D                             ##"
  "D                              #"
  "######                         #"
  "#                              #"
  "#     ###         =====        #"
  "#                              #"
  "#                              #"
  "R         #####                R"
  "R         #####                R"
  "R         #####  z      z      R"
  "##############GG################";

// ---------------------------------------------------------------- SB Test Room 4×3 (hidden)
// Not part of the world: reached from the title (hold Left + Right). Samus starts at '@' with
// everything. Left to right: the Bombs orb and a cracked-rock plug at ball height; a pit under a
// cracked-rock lid (with a step to climb out); a wall whose only gap is a ball hole 2 tiles up
// (bomb jump); an enemy arena with a ledge and a red door.
static const char T_SB[] =
  "################################################################"
  "#              #                      ##            k          #"
  "#              #                      ##                       #"
  "#              #                      ##                       #"
  "#              #                      ##                       #"
  "#              #                      ##                       #"
  "#              #                      ##          r            #"
  "#              #                      ##                       #"
  "#              #                      ##    ====               #"
  "#          O   #                                               R"
  "#              #                      ##                       R"
  "#  @   z       B           z          ##     z          z      R"
  "###################BBBBB########################################"
  "###################     ########################################"
  "###################     ########################################"
  "###################     ########################################"
  "###################    #########################################"
  "###################    #########################################"
  "###################    #########################################"
  "################################################################"
  "################################################################";

static const RoomDef ROOMS[] = {
  {"B01", "LANDING SITE",   0, 0, 2, 2, 0, T_B01},
  {"B02", "SURFACE PATH",   2, 1, 2, 1, 0, T_B02},
  {"B03", "BRINSTAR SHAFT", 4, 0, 1, 4, 0, T_B03},
  {"B04", "MORPH SHRINE",   5, 1, 1, 3, 0, T_B04},
  {"B05", "MISSILE ROOM",   2, 3, 2, 2, 0, T_B05},
  {"B06", "JUNCTION",       4, 4, 2, 2, 0, T_B06},
  {"SB",  "TEST ROOM",      8, 0, 4, 3, ZONE_TEST, T_SB},
};
static const uint8_t ROOM_N = sizeof(ROOMS) / sizeof(ROOMS[0]);
