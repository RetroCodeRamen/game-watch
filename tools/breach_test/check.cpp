// Breach map and art checker: every map closed, every door between two walls, the art well-formed,
// and everything reachable from the start once doors, the switch and the keycard are used.
#include "pp.cpp"
#include <stdio.h>
unsigned long g_ms; uint8_t watch::g_held; int watch::g_tune;
int fails = 0;
#define FAIL(...) do { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } while (0)
void checkMap(const char *name, const char *const *rows, uint8_t h) {
  uint8_t w = strlen(rows[0]);
  if (w > MAX_MAP || h > MAX_MAP) FAIL("%s: bigger than %d tiles", name, MAX_MAP);
  int starts = 0;
  for (int y = 0; y < h; y++) {
    if (strlen(rows[y]) != w) FAIL("%s: row %d is %zu wide, not %d", name, y, strlen(rows[y]), w);
    for (int x = 0; x < w; x++) {
      char c = rows[y][x];
      if (!strchr("#BSXDKLPhsoackG+.", c)) FAIL("%s: unknown tile '%c' at %d,%d", name, c, x, y);
      bool edge = !x || !y || x == w - 1 || y == h - 1;
      if (edge && !strchr("#BSX", c)) FAIL("%s: the edge is open at %d,%d", name, x, y);
      if (c == 'P') starts++;
    }
  }
  if (starts != 1) FAIL("%s: %d player starts", name, starts);
  loadMap(rows, h);
  for (int i = 0; i < nDoors; i++) {   // a door needs walls on both sides of its plane
    const Door &d = doors[i];
    bool ok = d.vert ? (tiles[d.ty - 1][d.tx] && tiles[d.ty + 1][d.tx]) : (tiles[d.ty][d.tx - 1] && tiles[d.ty][d.tx + 1]);
    if (!ok) FAIL("%s: door at %d,%d isn't set between two walls", name, d.tx, d.ty);
  }
  // reachability: flood fill, opening normal doors; the switch opens shutters; the keycard opens key doors
  static bool seen[MAX_MAP][MAX_MAP];
  bool sw = false, key = false, exitSeen = false;
  for (int pass = 0; pass < 4; pass++) {
    memset(seen, 0, sizeof seen);
    int stack[MAX_MAP * MAX_MAP], sp = 0;
    stack[sp++] = (py >> 16) * MAX_MAP + (px >> 16); seen[py >> 16][px >> 16] = true;
    while (sp) {
      int c = stack[--sp], x = c % MAX_MAP, y = c / MAX_MAP;
      const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        int nx = x + dx[k], ny = y + dy[k];
        if (!inMap(nx, ny) || seen[ny][nx]) continue;
        char t = rows[ny][nx];
        if (t == 'S') sw = true;
        if (t == 'X') exitSeen = true;
        if (strchr("#BSX", t)) continue;
        if (t == 'L' && !sw) continue;
        if (t == 'K' && !key) continue;
        if (t == 'k') key = true;
        seen[ny][nx] = true; stack[sp++] = ny * MAX_MAP + nx;
      }
    }
  }
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++)
    if (strchr("hsoackG+.", rows[y][x]) && !seen[y][x]) FAIL("%s: %d,%d ('%c') can't be reached", name, x, y, rows[y][x]);
  if (!exitSeen) FAIL("%s: the exit can't be reached", name);
  printf("%s: %dx%d, %d doors, %d monsters\n", name, w, h, nDoors, totalKills);
}
int main() {
  initTables();
  if (!artOk) FAIL("a sprite or texture has the wrong size (check ART[] against breach_art.h)");
  for (int i = 0; i < SP_N; i++) if (strlen(ART[i].art) != (size_t)ART[i].w * ART[i].h) FAIL("sprite %d is %zu chars, expected %dx%d", i, strlen(ART[i].art), ART[i].w, ART[i].h);
  checkMap("TEST_ROOM", TEST_ROOM, TEST_ROOM_H);
  printf(fails ? "%d problem(s)\n" : "all checks passed\n", fails);
  return fails ? 1 : 0;
}
