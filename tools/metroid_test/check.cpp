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
    printf("%s %-16s %dx%d at (%d,%d)  items %d  enemies %d\n", R.id, R.name, R.sw, R.sh, R.mx, R.my, items, enemies);
  }
  printf(bad ? "\n%d problems\n" : "\nall checks passed\n", bad);
  return bad ? 1 : 0;
}
