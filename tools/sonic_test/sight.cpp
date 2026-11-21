// Reaction-time check (rulebook 67, 124, 130, 187): a bot runs each act at normal speed with the real
// physics and camera; for every hazard it meets, how long was it on screen before Sonic got to it?
//   out/sight            all acts, hazards seen for less than WARN seconds
//   out/sight all        every hazard met, with its warning time
#include "pp.cpp"
#include <stdio.h>
#include <string.h>
unsigned long g_ms; uint8_t watch::g_held; int watch::g_tune;
uint8_t prevHeld;
void frame(uint8_t held) {
  watch::g_held = held; uint8_t e = held & ~prevHeld; prevHeld = held;
  g_ms += 33; if (watch::g_tune > 0) watch::g_tune--;
  gameUpdate(e); lives = 99; invul = 2;
}
static const char *NAME(uint8_t k) { switch (k) { case 0: return "spikes"; case 2: return "crab"; case 4: return "bee";
  case 8: return "flame"; case 10: return "saw"; case 11: return "fish"; default: return "?"; } }
int main(int argc, char **argv) {
  bool all = argc > 1 && !strcmp(argv[1], "all");
  const float WARN = 0.5f;                       // seconds of warning we ask for on the main path
  const char *ACTN[4][2] = {{"GH1","GH2"},{"CP1","CP2"},{"CN1","CN2"},{"SB1","SB2"}};
  int bad = 0;
  for (int z = 0; z < 4; z++) for (int a = 0; a < 2; a++) {
    startGame(); startAct(z, a); trState = TR_NONE; trLevel = 0;
    static int16_t seenAt[256]; static bool done[256];
    for (int i = 0; i < 256; i++) { seenAt[i] = -1; done[i] = false; }
    int best = 0, bestT = 0;
    for (int t = 0; t < 30 * 200 && mode == GAME && !bossOn && !goalT; t++) {
      if (px > best + 4) { best = px; bestT = t; }
      int stall = t - bestT; uint8_t h = B_RIGHT;
      if (stall > 6) h |= (stall % 24) < 18 ? B_MID : 0;
      if (stall > 60 && (stall % 90) < 20) h = B_LEFT;
      frame(h);
      for (int i = 0; i < 256; i++) {
        Obj &o = objs[i];
        if (!o.live || done[i]) continue;
        if (!(o.kind == 0 || o.kind == 2 || o.kind == 4 || o.kind == 8 || o.kind == 10 || o.kind == 11)) continue;
        int16_t ox = o.kind == 0 ? o.x + 6 : o.x;
        int16_t sxp = ox - cam, syp = o.y - camY;
        if (seenAt[i] < 0 && sxp >= 0 && sxp < 128 && syp > -4 && syp < 68) seenAt[i] = t;
        if (abs(px - ox) < 14 && abs(py - o.y) < 40) {         // got there (on or near his path)
          done[i] = true;
          float warn = seenAt[i] < 0 ? 0 : (t - seenAt[i]) / 30.0f;
          float spd = abs(fvx) / 256.0f;
          bool flag = warn < WARN && spd > 3.0f;               // fast and little warning
          if (flag) bad++;
          if (all || flag) printf("  %s %-6s at x=%5d (design %4d): seen %.2f s ahead at %.1f px/step%s\n",
                                  ACTN[z][a], NAME(o.kind), ox, ox * ZD / ZN, warn, spd, flag ? "  << too late" : "");
        }
      }
    }
  }
  printf("%d hazard(s) met at speed with under 0.5 s of warning\n", bad);
  return 0;
}
