// Room data rules for metroid_world.h:
//  - every room's tile string is exactly (sw*16) x (sh*7)
//  - rooms don't overlap on the map
//  - every opening on a room edge (empty tile or door) lines up with an opening in the room
//    next door; doors face a door; an edge opening into "no room" is listed as unbuilt
//  - doors are 3 tiles tall and sit in an edge column
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdlib>
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
bool solid(char c) { return c == '#' || c == '=' || c == 'B' || c == 'X' || c == 'G' || c == 'h' || c == 'S' || c == 'P' || c == '^' || c == '~'; }
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
      if (strchr("zkrupqnKYW", c)) enemies++;
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
        if (solid(n) && n != 'B' && n != 'X') { printf("%s: opening at %d,%d meets rock in %s\n", R.id, x, y, ROOMS[nr].id); bad++; }
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
  // Samus's moves on the tile grid (feet on a solid tile). Standing she walks, steps off ledges,
  // jumps up to 4 tiles (45 px), 7 with the double jump, any height with Space Jump (sideways
  // travel up to 2 tiles), and jumps gaps up to 4 tiles. In the Morph Ball she rolls through
  // 1-tile ducts and drops, stands again where there's room, and with Bombs hops up to 2 tiles.
  // Blue doors are open; red doors need Missiles, cracked rock ('B') Bombs, slashed rock ('X')
  // Space Jump. She never stands on spikes. Save points: the ship's pad and 'P' stations.
  {
    const int GW = 320, GH = 120;
    static char G[GW][GH];
    for (int x = 0; x < GW; x++) for (int y = 0; y < GH; y++) G[x][y] = tileAt(x, y);
    struct Gear { const char *name; bool ball, missiles, bombs, space; };
    Gear gr = {"", false, false, false, false};
    auto S = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= GW || y >= GH) return true;
      char c = G[x][y];
      if (!c) return true;
      if (c == 'R') return !gr.missiles;
      if (c == 'B') return !gr.bombs;
      if (c == 'X') return !gr.space;
      return solid(c);
    };
    auto body = [&](int x, int y, int h) { for (int i = 1; i <= h; i++) if (S(x, y - i)) return false; return true; };
    auto idx = [&](int x, int y, int b) { return (b * GW + x) * GH + y; };
    static bool seen[2 * GW * GH];
    static int q[2 * GW * GH];
    // all moves from a node; f(x, y, b) for each landing node
    auto moves = [&](int x, int y, int b, auto &&f) {
      auto land = [&](int nx, int ny, int nb) { if (nx >= 0 && ny >= 0 && nx < GW && ny < GH && G[nx][ny] != '^' && G[nx][ny] != '~') f(nx, ny, nb); };
      auto fall = [&](int nx, int yy, int nb) { while (yy < GH - 1 && !S(nx, yy)) yy++; land(nx, yy, nb); };
      if (b) {
        if (body(x, y, 3)) land(x, y, 0);
        for (int d = -1; d <= 1; d += 2) if (!S(x + d, y - 1)) fall(x + d, y, 1);
        if (gr.bombs) for (int up = 1; up <= 2; up++) {           // bomb hop
          if (!body(x, y, 1 + up)) break;
          for (int k = 0; k <= 1; k++) for (int d = -1; d <= 1; d += 2) {
            int nx = x + d * k;
            if (S(nx, y - up) && !S(nx, y - up - 1)) land(nx, y - up, 1);
          }
        }
        return;
      }
      if (gr.ball) land(x, y, 1);
      int maxUp = gr.space ? 14 : 7;
      for (int d = -1; d <= 1; d += 2) {
        if (body(x + d, y, 3)) fall(x + d, y, 0);
        for (int up = 1; up <= maxUp; up++) {
          if (!body(x, y, 3 + up)) break;
          for (int k = 1; k <= 2; k++) { bool ok = true;
            for (int j = 1; j <= k; j++) if (!body(x + d * j, y - up, 3)) ok = false;
            if (ok && S(x + d * k, y - up)) land(x + d * k, y - up, 0); }
        }
        if (body(x, y, 4)) for (int k = 2; k <= 4; k++) {
          bool ok = true; for (int j = 1; j <= k; j++) if (!body(x + d * j, y, 4) && !(j == k && body(x + d * j, y, 3))) ok = false;
          if (ok && S(x + d * k, y)) land(x + d * k, y, 0);
        }
      }
    };
    auto flood = [&](int sx, int sy, int sb) {
      memset(seen, 0, sizeof seen); int qh = 0, qt = 0;
      auto push = [&](int x, int y, int b) { int i = idx(x, y, b); if (seen[i]) return; seen[i] = true; q[qt++] = i; };
      push(sx, sy, sb);
      while (qh < qt) { int i = q[qh++]; int b = i / (GW * GH), x = (i / GH) % GW, y = i % GH; moves(x, y, b, push); }
    };
    int padX = 0, padY = 0;
    std::vector<std::pair<int,int>> saves;
    for (int x = 0; x < GW; x++) for (int y = 0; y < GH; y++) {
      if (G[x][y] == 'S') { padX = x; padY = y; saves.push_back({x, y}); }
      if (G[x][y] == 'P') saves.push_back({x, y});
    }
    auto roomsSeen = [&](const char *label) {
      printf("  %s:", label);
      for (int r = 0; r < ROOM_N; r++) { if (ROOMS[r].zone == ZONE_TEST) continue;
        int w = ROOMS[r].sw * 16, h = ROOMS[r].sh * 7, x0 = ROOMS[r].mx * 16, y0 = ROOMS[r].my * 7; bool v = false;
        for (int y = 0; y < h && !v; y++) for (int x = 0; x < w && !v; x++) if (seen[idx(x0 + x, y0 + y, 0)] || seen[idx(x0 + x, y0 + y, 1)]) v = true;
        if (v) printf(" %s", ROOMS[r].id); }
      printf("\n");
    };
    // can standing Samus touch the item at tile (ix, iy)? (orbs float 3 tiles above the floor)
    auto touches = [&](int ix, int iy) {
      for (int x = ix - 1; x <= ix + 1; x++) for (int y = iy + 1; y <= iy + 3; y++) if (x >= 0 && seen[idx(x, y, 0)] && body(x, y, y - iy)) return true;
      return false;
    };
    auto find = [&](const char *cs, int &fx, int &fy) {
      for (int x = 0; x < GW; x++) for (int y = 0; y < GH; y++) if (G[x][y] && strchr(cs, G[x][y])) {
        int r = roomAt(x, y); if (r >= 0 && ROOMS[r].zone == ZONE_TEST) continue; fx = x; fy = y; return true; }
      return false;
    };
    Gear stages[] = {{"no items", false, false, false, false}, {"Morph Ball", true, false, false, false},
                     {"Missiles", true, true, false, false}, {"Bombs", true, true, true, false}, {"Space Jump", true, true, true, true}};
    // each major item must be reachable with the gear from before it
    struct Need { const char *cs, *name; int stage; } needs[] = {{"mM", "Morph Ball", 0}, {"lL", "Missiles", 1}, {"oO", "Bombs", 2}};
    printf("\n");
    for (auto &n : needs) {
      int ix, iy; if (!find(n.cs, ix, iy)) continue;
      gr = stages[n.stage]; flood(padX, padY, 0);
      if (!touches(ix, iy)) { printf("%s is NOT reachable with %s\n", n.name, stages[n.stage].name); bad++; }
      else printf("  %s reachable with %s\n", n.name, stages[n.stage].name);
    }
    int kx, ky;
    if (find("Y", kx, ky)) { gr = stages[2]; flood(padX, padY, 0); bool ok = false;
      for (int x = kx - 8; x <= kx + 8; x++) if (x >= 0 && seen[idx(x, ky + 1, 0)]) ok = true;
      if (!ok) { printf("Ridley is NOT reachable with Missiles\n"); bad++; } else printf("  Ridley reachable with Missiles\n"); }
    if (find("K", kx, ky)) { gr = stages[3]; flood(padX, padY, 0); bool ok = false;
      for (int x = kx - 8; x <= kx; x++) if (seen[idx(x, ky + 1, 0)]) ok = true;
      if (!ok) { printf("Kraid is NOT reachable with Bombs\n"); bad++; } else printf("  Kraid reachable with Bombs\n"); }
    // before the Morph Ball everything is done at full height: every door of a room she reaches
    gr = stages[0]; flood(padX, padY, 0);
    roomsSeen("with no items, Samus reaches");
    for (int r = 0; r < ROOM_N; r++) {
      int w = ROOMS[r].sw * 16, h = ROOMS[r].sh * 7, x0 = ROOMS[r].mx * 16, y0 = ROOMS[r].my * 7;
      bool visited = false;
      for (int y = 0; y < h && !visited; y++) for (int x = 0; x < w; x++) if (seen[idx(x0 + x, y0 + y, 0)]) { visited = true; break; }
      if (!visited) continue;
      for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        char c = ROOMS[r].tiles[y * w + x];
        if (!door(c) || (y > 0 && ROOMS[r].tiles[(y - 1) * w + x] == c)) continue;
        int gx = x0 + x, fy = y0 + y + 3;
        bool ok = seen[idx(gx, fy, 0)] || seen[idx(gx + 1, fy, 0)] || (gx > 0 && seen[idx(gx - 1, fy, 0)]);
        if (!ok && roomAt(gx + (x == 0 ? -1 : 1), y0 + y) >= 0 && c == 'D') {
          printf("%s: door at %d,%d can't be reached at full height before the Morph Ball\n", ROOMS[r].id, x, y); bad++; }
      }
    }
    // the item is the key to its own escape: from it, without it, no way back to a save
    struct Trap { const char *cs, *name; int stage; } traps[] = {{"mM", "Morph Ball", 0}, {"oO", "Bombs", 2}};
    for (auto &t : traps) {
      int ix, iy; if (!find(t.cs, ix, iy)) continue;
      int fy = iy; while (fy < GH - 1 && !S(ix, fy)) fy++;
      gr = stages[t.stage]; flood(ix, fy, 0);
      bool home = false; for (auto &sv : saves) if (seen[idx(sv.first, sv.second, 0)]) home = true;
      if (home) { printf("from the %s Samus can get back to a save without using it\n", t.name); bad++; }
      else printf("  %s room: the only way out is to use it\n", t.name);
    }
    // backtracking: at each stage, from everywhere Samus can get to, a save point is reachable
    for (int si = 1; si < 5; si++) {
      gr = stages[si];
      flood(padX, padY, 0);
      char label[64]; snprintf(label, 64, "with %s, Samus reaches", gr.name); roomsSeen(label);
      std::vector<int> nodes; for (int i = 0; i < 2 * GW * GH; i++) if (seen[i]) nodes.push_back(i);
      // reverse edges among reached nodes, then search back from the save points
      std::vector<std::vector<int>> radj(2 * GW * GH);
      for (int u : nodes) { int b = u / (GW * GH), x = (u / GH) % GW, y = u % GH;
        moves(x, y, b, [&](int nx, int ny, int nb) { int v = idx(nx, ny, nb); if (seen[v]) radj[v].push_back(u); }); }
      std::vector<char> ok(2 * GW * GH, 0); std::vector<int> st;
      for (auto &sv : saves) for (int b = 0; b < 2; b++) { int i = idx(sv.first, sv.second, b); if (seen[i]) { ok[i] = 1; st.push_back(i); } }
      while (!st.empty()) { int v = st.back(); st.pop_back(); for (int u : radj[v]) if (!ok[u]) { ok[u] = 1; st.push_back(u); } }
      int stuck[64] = {0}, sx[64], sy[64];
      for (int u : nodes) if (!ok[u]) { int x = (u / GH) % GW, y = u % GH;
        int r = roomAt(x, y - 1); if (r < 0) r = roomAt(x, y); if (r < 0 || r >= 64) continue;
        if (!stuck[r]) { sx[r] = x - ROOMS[r].mx * 16; sy[r] = y - ROOMS[r].my * 7; } stuck[r]++; }
      for (int r = 0; r < ROOM_N; r++) if (stuck[r]) {
        // a shrine whose item opens its own way out is a dead end until Samus has that item
        bool shrine = false; int n = ROOMS[r].sw * 16 * ROOMS[r].sh * 7;
        for (int i = 0; i < n; i++) { char c = ROOMS[r].tiles[i]; if ((c == 'o' || c == 'O') && si < 3) shrine = true; }
        if (shrine) continue;
        printf("%s: %d spots with %s can't get back to a save (e.g. feet on %d,%d)\n", ROOMS[r].id, stuck[r], gr.name, sx[r], sy[r]); bad++; }
    }
  }
  printf(bad ? "\n%d problems\n" : "\nall checks passed\n", bad);
  return bad ? 1 : 0;
}
