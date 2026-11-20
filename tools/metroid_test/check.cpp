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
bool solid(char c) { return c == '#' || c == '=' || c == 'B' || c == 'X' || c == 'G' || c == 'h' || c == 'S'; }
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
      if (strchr("mlteojMLOJ", c)) items++;
      if (strchr("zkrupqnK", c)) enemies++;
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
  // ---- reachability ----
  // Samus's moves on the tile grid (feet on a solid tile): standing she walks, steps off ledges,
  // jumps up to 4 tiles (45 px peak), or 7 with the double jump (67 px) with up to 2 tiles of sideways travel, and jumps gaps up to 4
  // tiles. With the Morph Ball she can roll through 1-tile ducts and drop, but not climb, and stand
  // again where there's room. Blue doors count as open; red doors are rock until she has missiles.
  {
    static char G[300][120];
    for (int x = 0; x < 300; x++) for (int y = 0; y < 120; y++) G[x][y] = tileAt(x, y);
    bool hasBall = false, redShut = true;
    auto S = [&](int x, int y) { if (x < 0 || y < 0 || x >= 300 || y >= 120) return true; char c = G[x][y]; return !c || solid(c) || (redShut && c == 'R'); };
    auto body = [&](int x, int y, int h) { for (int i = 1; i <= h; i++) if (S(x, y - i)) return false; return true; };
    int padX = 0, padY = 0;
    for (int x = 0; x < 200; x++) for (int y = 0; y < 120; y++) if (G[x][y] == 'S') { padX = x; padY = y; }
    static bool seen[2][200][120]; static int qx[48000], qy[48000], qb[48000];
    auto flood = [&](int sx, int sy, int sb) {
      memset(seen, 0, sizeof seen); int qh = 0, qt = 0;
      auto push = [&](int x, int y, int b) { if (x < 0 || y < 0 || x >= 200 || y >= 120 || seen[b][x][y]) return; seen[b][x][y] = true; qx[qt] = x; qy[qt] = y; qb[qt] = b; qt++; };
      push(sx, sy, sb);
      while (qh < qt) {
        int x = qx[qh], y = qy[qh], b = qb[qh]; qh++;
        if (b) {                                                   // Morph Ball
          if (body(x, y, 3)) push(x, y, 0);
          for (int d = -1; d <= 1; d += 2) { int nx = x + d;
            if (S(nx, y - 1)) continue;
            int yy = y; while (yy < 119 && !S(nx, yy)) yy++; push(nx, yy, 1); }
          continue;
        }
        if (hasBall) push(x, y, 1);
        for (int d = -1; d <= 1; d += 2) {
          int nx = x + d;
          if (body(nx, y, 3)) { int yy = y; while (yy < 119 && !S(nx, yy)) yy++; push(nx, yy, 0); }   // walk / step off
          for (int up = 1; up <= 7; up++) {                       // jump up onto a ledge (4 tiles; 7 with the double jump)
            if (!body(x, y, 3 + up)) break;
            for (int k = 1; k <= 2; k++) { bool ok = true;
              for (int j = 1; j <= k; j++) if (!body(x + d * j, y - up, 3)) ok = false;
              if (ok && S(x + d * k, y - up)) push(x + d * k, y - up, 0); }
          }
          if (body(x, y, 4)) for (int k = 2; k <= 4; k++) {        // jump a gap at the same height
            bool ok = true; for (int j = 1; j <= k; j++) if (!body(x + d * j, y, 4) && !(j == k && body(x + d * j, y, 3))) ok = false;
            if (ok && S(x + d * k, y)) push(x + d * k, y, 0);
          }
        }
      }
      return qt;
    };
    // can standing Samus touch the item at tile (ix, iy)? (orbs float above the floor)
    auto touches = [&](int ix, int iy) {
      for (int x = ix - 1; x <= ix + 1; x++) for (int y = iy + 1; y <= iy + 3; y++) if (seen[0][x][y] && body(x, y, y - iy)) return true;
      return false;
    };
    int mX = -1, mY = -1, lX = -1, lY = -1;
    for (int x = 0; x < 200; x++) for (int y = 0; y < 120; y++) { if (G[x][y] == 'm' || G[x][y] == 'M') { mX = x; mY = y; } if (G[x][y] == 'l' || G[x][y] == 'L') { lX = x; lY = y; } }
    auto roomsSeen = [&](const char *label) {
      printf("  %s:", label);
      for (int r = 0; r < ROOM_N; r++) { int w = ROOMS[r].sw * 16, h = ROOMS[r].sh * 7, x0 = ROOMS[r].mx * 16, y0 = ROOMS[r].my * 7; bool v = false;
        for (int y = 0; y < h && !v; y++) for (int x = 0; x < w; x++) if (seen[0][x0 + x][y0 + y] || seen[1][x0 + x][y0 + y]) { v = true; break; }
        if (v) printf(" %s", ROOMS[r].id); }
      printf("\n");
    };

    // 1. before the Morph Ball: everything is done at full height
    printf("\n");
    flood(padX, padY, 0);
    roomsSeen("before Morph Ball, standing reaches");
    if (mX < 0 || !touches(mX, mY)) { printf("Morph Ball is NOT reachable at full height from the ship\n"); bad++; }
    for (int r = 0; r < ROOM_N; r++) {                     // every door of a room she reaches, at full height
      int w = ROOMS[r].sw * 16, h = ROOMS[r].sh * 7, x0 = ROOMS[r].mx * 16, y0 = ROOMS[r].my * 7;
      bool visited = false;
      for (int y = 0; y < h && !visited; y++) for (int x = 0; x < w; x++) if (seen[0][x0 + x][y0 + y]) { visited = true; break; }
      if (!visited) continue;
      for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        char c = ROOMS[r].tiles[y * w + x];
        if (!door(c) || (y > 0 && ROOMS[r].tiles[(y - 1) * w + x] == c)) continue;
        int gx = x0 + x, fy = y0 + y + 3;
        bool ok = seen[0][gx][fy] || seen[0][gx + 1][fy] || seen[0][gx - 1][fy];
        if (!ok && roomAt(gx + (x == 0 ? -1 : 1), y0 + y) >= 0) {
          printf("%s: door at %d,%d can't be reached at full height before the Morph Ball\n", ROOMS[r].id, x, y); bad++; }
      }
    }
    // 2. the Morph Ball shrine is a trap without the ball: from the item, standing only, no way home
    if (mX >= 0) {
      int fy = mY; while (fy < 119 && !S(mX, fy)) fy++;
      flood(mX, fy, 0);
      if (seen[0][padX][padY]) { printf("from the Morph Ball Samus can walk home without rolling\n"); bad++; }
      else printf("  Morph Ball shrine: the only way out is to roll\n");
    }
    // 3. backtracking: at each gear stage, from everywhere Samus can get to, she can get back to the ship
    struct Stage { const char *name; bool ball, missiles; } stages[] = {{"with Morph Ball", true, false}, {"with Morph Ball + Missiles", true, true}};
    for (auto &st : stages) {
      hasBall = st.ball; redShut = !st.missiles;
      flood(padX, padY, 0);
      if (st.ball && !st.missiles && lX >= 0 && !touches(lX, lY)) { printf("Missile Launcher is NOT reachable with the Morph Ball\n"); bad++; }
      char label[64]; snprintf(label, 64, "%s, Samus reaches", st.name); roomsSeen(label);
      static int fx[48000], fyy[48000], fb[48000]; int n = 0;
      for (int b = 0; b < 2; b++) for (int x = 0; x < 200; x++) for (int y = 0; y < 120; y++) if (seen[b][x][y]) { fx[n] = x; fyy[n] = y; fb[n] = b; n++; }
      int stuck[32] = {0}, sx[32], sy[32];
      for (int i = 0; i < n; i++) {
        flood(fx[i], fyy[i], fb[i]);
        if (seen[0][padX][padY] || seen[1][padX][padY]) continue;
        int r = roomAt(fx[i], fyy[i] - 1); if (r < 0) r = roomAt(fx[i], fyy[i]); if (r < 0) continue;
        if (!stuck[r]) { sx[r] = fx[i] - ROOMS[r].mx * 16; sy[r] = fyy[i] - ROOMS[r].my * 7; }
        stuck[r]++;
      }
      for (int r = 0; r < ROOM_N; r++) if (stuck[r]) {
        printf("%s: %d spots %s can't get back to the ship (e.g. feet on %d,%d)\n", ROOMS[r].id, stuck[r], st.name, sx[r], sy[r]); bad++; }
    }
  }
  printf(bad ? "\n%d problems\n" : "\nall checks passed\n", bad);
  return bad ? 1 : 0;
}
