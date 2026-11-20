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
//   '@' test-room start (empty)   'P' save station (solid)   '^' spikes (solid; 1 energy and back
//   to safe footing)
// Objects (the tile itself is empty):
//   'm' Morph Ball  'l' Missile Launcher (+5)  'o' Bombs  'j' Space Jump  — major items: 3 tiles above the floor, in an orb on a
//       Chozo statue's palm. Lower case: statue sits left of the orb, facing right; upper case
//       ('M', 'L', 'O', 'J'): statue on the right, facing left. Face it toward where Samus arrives.
//   't' missile tank (+5)  'e' Energy Tank (+2)
//   'z' Zoomer (crawls round surfaces)  'k' Skree (hangs from the ceiling, dives)
//   'r' Ripper (flies a lane, beam-proof)  'u' turret (on the wall beside it)
//   'p' Swipe Pirate  'q' Bolt Pirate (stand on the floor below)  'n' Metroid  'K' Kraid  'Y' Ridley  'W' Mother Brain
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
  "#####          #         #######"
  "##                       #######"
  "D           k                  D"
  "D                   ###        D"
  "D          z        ###   z    D"
  "##########BB####################";

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
  "####     M    ##"
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
  "################  ##############";

// ---------------------------------------------------------------- B07 Tourian Approach 2×2
// Below the junction: drop in through its floor, climb back on the ledges. The chute from
// Kraid's Lair (K09) comes in low on the west. The hatch in the floor to Tourian needs both bosses.
static const char T_B07[] =
  "################  ##############"
  "#                              #"
  "#                              #"
  "#                              #"
  "#              ####            #"
  "#                              #"
  "#                              #"
  "#                              #"
  "#          ###                 #"
  "                               #"
  "                               #"
  "                               #"
  "########################GG######"
  "########################GG######";

// ---------------------------------------------------------------- B08 Buried Alcove 1×1
// Missile tank #2 under the cracked seam on the Surface Path (bomb through it). From the low step
// under the hole a double jump takes Samus back up.
static const char T_B08[] =
  "##########  ####"
  "####        ####"
  "####        ####"
  "#           ####"
  "#         ######"
  "#    t    ######"
  "################";

// ---------------------------------------------------------------- K01 Lair Entrance 1×1
// Through the junction's west red door. West to the Spike Hall.
static const char T_K01[] =
  "################"
  "#####    #######"
  "###          ###"
  "D              R"
  "D              R"
  "D     z        R"
  "################";

// ---------------------------------------------------------------- K02 Spike Hall 2×1
// Westbound gallery: strips of spikes in the floor, a Swipe Pirate patrolling the flat middle, a
// Zoomer. The west door is high, on a platform, and leads to the Bombs shrine.
static const char T_K02[] =
  "################################"
  "D       ####        ######     #"
  "D                              #"
  "D                              D"
  "####                           D"
  "####       p              z    D"
  "######^^########^^######^^######";

// ---------------------------------------------------------------- K03 Bombs Shrine 1×2
// The door ledge drops 9 tiles into the statue's chamber - more than a double jump. The way on is
// to bomb through the cracked floor into the descent.
static const char T_K03[] =
  "################"
  "#####          D"
  "###            D"
  "##             D"
  "##          ####"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "#     o        #"
  "#              #"
  "#              #"
  "##########BB####";

// ---------------------------------------------------------------- K04 Cracked Descent 1×2
// Down from the shrine's bombed floor on staggered ledges (climbable back up). Missile tank #3 on
// the floor; the way on is east at the bottom.
static const char T_K04[] =
  "##########  ####"
  "#              #"
  "#              #"
  "#              #"
  "#       #####  #"
  "#              #"
  "#              #"
  "#  ####        #"
  "#              #"
  "#              #"
  "###             "
  "###             "
  "###     t       "
  "################";

// ---------------------------------------------------------------- K06 Lower Tunnels 1×2
// In from the descent at the bottom; climb the step and ledges (each clear of the jump onto it)
// up through the hole into the save room. Energy Tank #1 sits on the step.
static const char T_K06[] =
  "#####  #########"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "# ####         #"
  "#              #"
  "#              #"
  "#     ####     #"
  "#            e #"
  "           #####"
  "           #####"
  "      z    #####"
  "################";

// ---------------------------------------------------------------- K07 Kraid Save 1×1
// Save station; Kraid is through the east door. The hole in the floor goes down to the tunnels.
static const char T_K07[] =
  "################"
  "#######   ######"
  "####          ##"
  "#              D"
  "#              D"
  "#              D"
  "#####  ###P#####";

// ---------------------------------------------------------------- K08 Kraid 1×2
// Kraid holds the east end; the step at mouth height is on the west. The east door (behind him)
// leads on to the chute. A ledge under the west door to come back up.
static const char T_K08[] =
  "################"
  "#              #"
  "#              #"
  "D              #"
  "D              #"
  "D              #"
  "####           #"
  "#              #"
  "#              #"
  "#              D"
  "######         D"
  "######    K    D"
  "################"
  "################";

// ---------------------------------------------------------------- K09 Service Chute 1×1
// Slashed rock (Space Jump) both ways: east into B07 below the junction, and up into the cache.
static const char T_K09[] =
  "######XX########"
  "#              #"
  "D              X"
  "D              X"
  "D              X"
  "################"
  "################";

// ---------------------------------------------------------------- K10 Space Jump Cache 1×1
// Missile tank #4, above the chute's slashed ceiling; drop back down the hole.
static const char T_K10[] =
  "################"
  "####      ######"
  "###         ####"
  "##            ##"
  "##            ##"
  "##        t   ##"
  "######  ########";

// ---------------------------------------------------------------- SB Test Room 4×3 (hidden)
// Not part of the world: reached from the title (hold Left + Right). Samus starts at '@' with
// everything. Left to right: the Bombs orb and a cracked-rock plug at ball height; a pit under a
// cracked-rock lid (with a step to climb out); a wall whose only gap is a ball hole 2 tiles up
// (bomb jump); the Space Jump orb; an enemy arena with a ledge, a wall of slashed rock to flip
// through, a ledge 8 tiles up (Space Jump, or a perfect double jump) and a red door.
static const char T_SB[] =
  "################################################################"
  "#              #                      ##            k          #"
  "#              #                      ##              X        #"
  "#              #                      ##              X        #"
  "#              #                      ##              X  ===== #"
  "#              #                      ##              X        #"
  "#              #                      ##          r   X        #"
  "#              #                      ##              X        #"
  "#              #                      ##    ====      X        #"
  "#         O    #                 J                    X        R"
  "#              #                      ##              X        R"
  "#  @   z       B           z          ##     z        X z      R"
  "###################BBBBB########################################"
  "###################     ########################################"
  "###################     ########################################"
  "###################     ########################################"
  "###################    #########################################"
  "###################    #########################################"
  "###################    #########################################"
  "################################################################"
  "################################################################";

// ---------------------------------------------------------------- SB2 Enemy Test Room 3×2 (hidden)
// Through the test room's red door. Left to right: a Swipe Pirate on patrol, a low wall, a Bolt
// Pirate guarding the middle (a ledge above its lane), a turret on the far wall, two Metroids; the
// far door leads to the Kraid test.
static const char T_SB2[] =
  "################################################"
  "#                                              #"
  "#                                              #"
  "#                                         n    #"
  "#                                   n          #"
  "#                                              #"
  "#                                              #"
  "#                                              #"
  "#                         =====                #"
  "D                                              D"
  "D               #                             uD"
  "D       p       #     q                        D"
  "################################################"
  "################################################";

// ---------------------------------------------------------------- SB3 Boss Test: Kraid 1×2 (hidden)
// Through the far door of the enemy test room. One screen wide so Kraid is always in view; he
// holds the right side. The step on the left is at mouth height: from it a plain shot reaches his
// open mouth (from the floor it takes a hop), and it blocks his skidding spikes - but not his lob.
static const char T_SB3[] =
  "################"
  "#              #"
  "#              D"
  "#              D"
  "#              D"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "D              #"
  "D#####         #"
  "D#####    K    #"
  "################"
  "################";

// ---------------------------------------------------------------- SB4 Boss Test: Ridley 1×2 (hidden)
// Through the high door on the right of Kraid's arena (Space Jump over him). A flat floor for
// Ridley's swoops and landings; a small ledge under the door; a door on the right to Mother Brain.
static const char T_SB4[] =
  "################"
  "#              #"
  "D              #"
  "D              #"
  "D              #"
  "#==            #"
  "#              #"
  "#              #"
  "#              #"
  "#              D"
  "#              D"
  "#       Y      D"
  "################"
  "################";

// ---------------------------------------------------------------- SB5 Boss Test: Mother Brain 1×2 (hidden)
// Through the door on the right of Ridley's arena. Her tank stands on the right.
static const char T_SB5[] =
  "################"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "#              #"
  "D              #"
  "D              #"
  "D         W    #"
  "################"
  "################";

static const RoomDef ROOMS[] = {
  {"B01", "LANDING SITE",   0, 0, 2, 2, 0, T_B01},
  {"B02", "SURFACE PATH",   2, 1, 2, 1, 0, T_B02},
  {"B03", "BRINSTAR SHAFT", 4, 0, 1, 4, 0, T_B03},
  {"B04", "MORPH SHRINE",   5, 1, 1, 3, 0, T_B04},
  {"B05", "MISSILE ROOM",   2, 3, 2, 2, 0, T_B05},
  {"B06", "JUNCTION",       4, 4, 2, 2, 0, T_B06},
  {"B07", "TOURIAN GATE",   4, 6, 2, 2, 0, T_B07},
  {"B08", "BURIED ALCOVE",  2, 2, 1, 1, 0, T_B08},
  {"K01", "LAIR ENTRANCE",  3, 5, 1, 1, 1, T_K01},
  {"K02", "SPIKE HALL",     1, 5, 2, 1, 1, T_K02},
  {"K03", "BOMB SHRINE",    0, 5, 1, 2, 1, T_K03},
  {"K04", "CRACKED DESCENT",0, 7, 1, 2, 1, T_K04},
  {"K06", "LOWER TUNNELS",  1, 7, 1, 2, 1, T_K06},
  {"K07", "KRAID SAVE",     1, 6, 1, 1, 1, T_K07},
  {"K08", "KRAID",          2, 6, 1, 2, 1, T_K08},
  {"K09", "SERVICE CHUTE",  3, 7, 1, 1, 1, T_K09},
  {"K10", "CHUTE CACHE",    3, 6, 1, 1, 1, T_K10},
  {"SB",  "TEST ROOM",      8, 0, 4, 3, ZONE_TEST, T_SB},
  {"SB2", "ENEMY TEST",    12, 0, 3, 2, ZONE_TEST, T_SB2},
  {"SB3", "KRAID TEST",    15, 0, 1, 2, ZONE_TEST, T_SB3},
  {"SB4", "RIDLEY TEST",   16, 0, 1, 2, ZONE_TEST, T_SB4},
  {"SB5", "BRAIN TEST",    17, 0, 1, 2, ZONE_TEST, T_SB5},
};
static const uint8_t ROOM_N = sizeof(ROOMS) / sizeof(ROOMS[0]);
