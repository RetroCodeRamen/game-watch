#pragma once
// Hellgate Watch maps. One character per tile, top row = north. The player starts on 'P' facing east.
//
//   walls    # tech panel   B brick   R rune stone   F flesh   Z sigil
//            S switch (opens every L)   X exit switch
//   doors    D door (Use opens it)   K keycard door   L shutter (opened by a switch)
//   demons   t Thrall   m Maw   f Fiend
//   pickups  + medkit   a ammo clip   c shells   k keycard   G scattergun
//
// Every map must be closed by walls on all four sides (tools/hellgate_test/check verifies this).

static const uint8_t MAX_MAP = 32;

// The test room: every engine feature in one small place.
//   start room (gear) -> D -> hall (Thrall, Maw, a switch) -> L opens -> closet (keycard)
//   -> K -> the gate: an arena where the base turns to hell (Fiend, Maw, ammo) -> X ends the level.
static const char *const TEST_ROOM[] = {
  "#############RRFRRR#",
  "#P...#......#......R",
  "#....#..t...#...f..F",
  "#.G..D......K......R",
  "#..a.#......#......Z",
  "#.+..#...m..#..m...R",
  "######......#......F",
  "######S##L###a.+...R",
  "######..+k..#.c....R",
  "###############XZRRR",
};
static const uint8_t TEST_ROOM_H = sizeof(TEST_ROOM) / sizeof(TEST_ROOM[0]);
