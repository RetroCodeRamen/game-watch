#pragma once
// Breach Watch maps. One character per tile, top row = north. The player starts on 'P' facing east.
//
//   walls    # tech panel   B brick   S switch (opens every L)   X exit switch
//   doors    D door (Use opens it)   K keycard door   L shutter (opened by a switch)
//   monsters h Husk   s Spitter   o Drone
//   pickups  + medkit   a ammo clip   c shells   k keycard   G scattergun
//
// Every map must be closed by walls on all four sides (tools/breach_test/check verifies this).

static const uint8_t MAX_MAP = 32;

// The test room: every engine feature in one small place.
//   start room (gear) -> D -> hall (Husk, Spitter, a switch) -> L opens -> closet (keycard)
//   -> K -> arena (Drone, Spitter, ammo) -> X ends the level.
static const char *const TEST_ROOM[] = {
  "#############BBBBBB#",
  "#P...#......#......B",
  "#....#..h...#...o..B",
  "#.G..D......K......B",
  "#..a.#......#......B",
  "#.+..#...s..#..s...B",
  "######......#......B",
  "######S##L###a.+...B",
  "######..+k..#.c....B",
  "###############XBBBB",
};
static const uint8_t TEST_ROOM_H = sizeof(TEST_ROOM) / sizeof(TEST_ROOM[0]);
