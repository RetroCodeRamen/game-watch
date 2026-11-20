// Room data rules for metroid_world.h:
//  - every room's tile string is exactly (sw*16) x (sh*7)
//  - rooms don't overlap on the map
//  - every opening on a room edge (empty tile or door) lines up with an opening in the room
//    next door; doors face a door; an edge opening into "no room" is listed as unbuilt
//  - doors are 3 tiles tall and sit in an edge column
#include <cstdio>
#include <cstring>
#include "metroid_world.h"

int bad = 0;
int roomAt(int tx, int ty) {
  for (int r = 0; r < ROOM_N; r++) {
    int x0 = ROOMS[r].mx * 16, y0 = ROOMS[r].my * 7;
    if (tx >= x0 && tx < x0 + ROOMS[r].sw * 16 && ty >= y0 && ty < y0 + ROOMS[r].sh * 7) return r;
  }
  return -1;
}
char tileAt(int tx, int ty) {
  int r = roomAt(tx, ty); if (r < 0) return 0;
  int w = ROOMS[r].sw * 16;
  return ROOMS[r].tiles[(ty - ROOMS[r].my * 7) * w + tx - ROOMS[r].mx * 16];
}
bool solid(char c) { return c == '#' || c == '=' || c == 'B' || c == 'G' || c == 'h' || c == 'S'; }
bool door(char c) { return c == 'D' || c == 'R'; }

int main() {
  for (int r = 0; r < ROOM_N; r++) {
    const RoomDef &R = ROOMS[r];
    int w = R.sw * 16, h = R.sh * 7;
    if ((int)strlen(R.tiles) != w * h) { printf("%s: %zu tiles, expected %d (%dx%d)\n", R.id, strlen(R.tiles), w * h, w, h); bad++; continue; }
    for (int q = 0; q < r; q++) {
      const RoomDef &Q = ROOMS[q];
      if (R.mx < Q.mx + Q.sw && Q.mx < R.mx + R.sw && R.my < Q.my + Q.sh && Q.my < R.my + R.sh) { printf("%s overlaps %s\n", R.id, Q.id); bad++; }
    }
    int items = 0, enemies = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      char c = R.tiles[y * w + x];
      if (strchr("mlte", c)) items++;
      if (strchr("zkr", c)) enemies++;
      if (door(c)) {
        if (x != 0 && x != w - 1) { printf("%s: door not on an edge at %d,%d\n", R.id, x, y); bad++; }
        if (y == 0 || R.tiles[(y - 1) * w + x] != c) {
          int n = 0; while (y + n < h && R.tiles[(y + n) * w + x] == c) n++;
          if (n != 3) { printf("%s: door at %d,%d is %d tall (want 3)\n", R.id, x, y, n); bad++; }
        }
      }
      bool edge = x == 0 || y == 0 || x == w - 1 || y == h - 1;
      if (!edge || solid(c)) continue;
      int gx = R.mx * 16 + x, gy = R.my * 7 + y;
      // look across each boundary this tile sits on
      int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
      for (auto &d : dirs) {
        bool onIt = (d[0] < 0 && x == 0) || (d[0] > 0 && x == w - 1) || (d[1] < 0 && y == 0) || (d[1] > 0 && y == h - 1);
        if (!onIt) continue;
        int nr = roomAt(gx + d[0], gy + d[1]);
        char n = tileAt(gx + d[0], gy + d[1]);
        if (nr < 0) {
          if (door(c)) { if (y == 0 || R.tiles[(y - 1) * w + x] != c) printf("  note: %s door at %d,%d leads to an unbuilt room\n", R.id, x, y); }
          else if (d[1] < 0 && r == 0) {}   // the surface: open sky above B01 is fine
          else { printf("%s: opening at %d,%d leads out of the world\n", R.id, x, y); bad++; }
          continue;
        }
        if (solid(n)) { printf("%s: opening at %d,%d meets rock in %s\n", R.id, x, y, ROOMS[nr].id); bad++; }
        else if (door(c) != door(n)) { printf("%s: door/opening mismatch at %d,%d with %s\n", R.id, x, y, ROOMS[nr].id); bad++; }
      }
    }
    // headroom: wherever there's floor, the gap above it must fit standing Samus (3 tiles, she is 21 px)
    // or be a 1-tile ball duct. A 2-tile gap fits nobody standing — a pinch point.
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      char c = R.tiles[y * w + x];
      if (!solid(c) || y == 0) continue;
      int gx = R.mx * 16 + x, gy = R.my * 7 + y;
      char above = tileAt(gx, gy - 1);
      if (!above || solid(above) || door(above)) continue;
      int gap = 0;
      while (gap < 4) { char t = tileAt(gx, gy - 1 - gap); if (!t && r == 0) { gap = 4; break; } if (!t || solid(t) || door(t)) break; gap++; }
      if (gap == 2) { printf("%s: pinch point above floor at %d,%d (%d px of headroom; Samus needs 24, the ball 8)\n", R.id, x, y, gap * 8); bad++; }
    }
    printf("%s %-16s %dx%d at (%d,%d)  items %d  enemies %d\n", R.id, R.name, R.sw, R.sh, R.mx, R.my, items, enemies);
  }
  // ---- standing reachability before the Morph Ball ----
  // Samus at full height from the ship pad: walk, step off ledges, jump up to 4 tiles (36 px peak),
  // jump gaps up to 4 tiles. Doors count as open (any shot opens a blue door). The Morph Ball must be
  // reachable this way, and so must every door of every room she passes through on the way.
  {
    auto T = [](int x, int y) { char c = tileAt(x, y); return c; };
    auto S = [&](int x, int y) { char c = T(x, y); return !c || solid(c); };   // outside = rock
    auto body = [&](int x, int y, int h) { for (int i = 1; i <= h; i++) if (S(x, y - i)) return false; return true; };
    int padX = 0, padY = 0, mX = -1, mY = -1;
    for (int r = 0; r < ROOM_N; r++) { int w = ROOMS[r].sw * 16, h = ROOMS[r].sh * 7;
      for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { char c = ROOMS[r].tiles[y * w + x];
        if (c == 'S') { padX = ROOMS[r].mx * 16 + x; padY = ROOMS[r].my * 7 + y; }
        if (c == 'm') { mX = ROOMS[r].mx * 16 + x; mY = ROOMS[r].my * 7 + y; } } }
    static bool seen[200][120]; int qx[24000], qy[24000], qh = 0, qt = 0;
    auto push = [&](int x, int y) { if (x < 0 || y < 0 || x >= 200 || y >= 120 || seen[x][y]) return; seen[x][y] = true; qx[qt] = x; qy[qt] = y; qt++; };
    push(padX, padY);
    bool gotMorph = false;
    while (qh < qt) {
      int x = qx[qh], y = qy[qh]; qh++;
      if (mX >= 0 && abs(x - mX) <= 1 && mY <= y - 1 && mY >= y - 5 && body(mX == x ? x : x, y, y - mY)) gotMorph = true;
      for (int d = -1; d <= 1; d += 2) {
        int nx = x + d;
        if (body(nx, y, 3)) {
          if (S(nx, y)) push(nx, y);
          else { int yy = y; while (yy < 119 && !S(nx, yy)) yy++; push(nx, yy); }   // step off and fall
        }
        for (int up = 1; up <= 4; up++) {                       // jump up onto a ledge
          if (!body(x, y, 3 + up)) break;
          for (int k = 1; k <= 2; k++) { int tx = x + d * k; bool ok = true;
            for (int j = 1; j <= k; j++) if (!body(x + d * j, y - up, 3)) ok = false;
            if (ok && S(tx, y - up)) push(tx, y - up); }
        }
        if (body(x, y, 4)) for (int k = 2; k <= 4; k++) {        // jump a gap at the same height
          bool ok = true; for (int j = 1; j <= k; j++) if (!body(x + d * j, y, 4) && !(j == k && body(x + d * j, y, 3))) ok = false;
          if (ok && S(x + d * k, y)) push(x + d * k, y);
        }
      }
    }
    if (!gotMorph) { printf("Morph Ball is NOT reachable at full height from the ship\n"); bad++; }
    else printf("\nMorph Ball reachable at full height from the ship\n");
    // every door in a room standing Samus reaches must itself be reachable standing
    for (int r = 0; r < ROOM_N; r++) {
      int w = ROOMS[r].sw * 16, h = ROOMS[r].sh * 7, x0 = ROOMS[r].mx * 16, y0 = ROOMS[r].my * 7;
      bool visited = false;
      for (int y = 0; y < h && !visited; y++) for (int x = 0; x < w; x++) if (seen[x0 + x][y0 + y]) { visited = true; break; }
      if (!visited) continue;
      printf("  pre-Morph-Ball room %s\n", ROOMS[r].id);
      for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        char c = ROOMS[r].tiles[y * w + x];
        if (!door(c) || (y > 0 && ROOMS[r].tiles[(y - 1) * w + x] == c)) continue;
        int gx = x0 + x, fy = y0 + y + 3;   // standing in the doorway, feet on the tile below it
        bool ok = seen[gx][fy] || seen[gx + 1][fy] || seen[gx - 1][fy];
        if (!ok && roomAt(gx + (x == 0 ? -1 : 1), y0 + y) >= 0) {
          printf("%s: door at %d,%d can't be reached at full height before the Morph Ball\n", ROOMS[r].id, x, y); bad++; }
      }
    }
  }
  printf(bad ? "\n%d problems\n" : "\nall checks passed\n", bad);
  return bad ? 1 : 0;
}
