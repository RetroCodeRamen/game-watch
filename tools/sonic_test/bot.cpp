// Host harness: compiles the real sonic_watch.ino and lets a bot play each act.
#include "pp.cpp"   // the sketch after Arduino preprocessing (build.sh makes it)
#include <stdio.h>
#include <vector>
#include <string>
unsigned long g_ms; uint8_t watch::g_held; int watch::g_tune;


uint8_t prevHeld;
void frame(uint8_t held) {
  watch::g_held = held;
  uint8_t e = held & ~prevHeld; prevHeld = held;
  g_ms += 33; if (watch::g_tune > 0) watch::g_tune--;
  gameUpdate(e);
}
void savePBM(const char *fn) {
  u8g2.clearBuffer(); gameDraw();
  FILE *f = fopen(fn, "w"); fprintf(f, "P1\n128 64\n");
  for (int y = 0; y < 64; y++) { for (int x = 0; x < 128; x++) fputc((u8g2.buf[(y>>3)*128+x] >> (y&7)) & 1 ? '1' : '0', f); fputc('\n', f); }
  fclose(f);
}

// Bot: hold right; when stalled, try jumping, then back up and take a run at it.
int playAct(int z, int a, bool verbose, int shotX) {
  startGame(); startAct(z, a); lives = 99;
  int best = 0, bestT = 0, t = 0, deaths = 0, plan = 0, planT = 0;
  bool shot = false;
  for (t = 0; t < 30 * 240; t++) {
    if (mode == CLEAR || mode == SPECIAL) { if (verbose) printf("  cleared at t=%.1fs deaths=%d\n", t / 30.0, deaths); return -1; }
    if (mode == DEAD) { frame(0); best = 0; bestT = t; continue; }
    if (bossOn) { if (verbose) printf("  reached boss at t=%.1fs deaths=%d\n", t / 30.0, deaths); return -1; }
    if (z == zone && a == act && px > best + 4) { best = px; bestT = t; plan = 0; }
    if (shotX && !shot && px >= shotX && mode == GAME) { savePBM("shot.pbm"); shot = true; }
    uint8_t h = B_RIGHT;
    int stall = t - bestT;
    if (stall > 6 && stall <= 60) h = B_RIGHT | ((stall % 24) < 18 ? B_MID : 0);
    if (stall > 60) {           // stuck: cycle "back up N frames, then run (and maybe jump)"
      static const int BACK[] = {4, 10, 20, 35, 6, 50, 15, 70};
      static const int JUMP[] = {0, 0, 0, 0, 1, 1, 1, 1};
      int s = ((stall - 60) / 90) % 8, ph = (stall - 60) % 90;
      if (ph < BACK[s]) h = B_LEFT;
      else h = B_RIGHT | (JUMP[s] && ph > BACK[s] + 12 && ph < BACK[s] + 30 ? B_MID : 0);
    }
    if (getenv("STALL") && stall == 90) printf("    stall 3s at x=%d y=%d g=%d gnd=%d\n", px, py, groundAt(px), onGround);
    if (stall > 30 * 25) { printf("  STUCK zone%d act%d at x=%d y=%d (best %d) onGround=%d inLoop=%d hollow=%d\n", z+1, a+1, px, py, best, onGround, inLoop, inHollow); return best; }
    frame(h);
    if (getenv("PROF") && t % 30 == 0) printf("    t=%ds x=%d y=%d vx=%.1f\n", t/30, px, py, fvx/256.0);
    if (getenv("TRACE") && (getenv("TX") ? (px > atoi(getenv("TX")) - 40 && px < atoi(getenv("TX")) + 10 && t < atoi(getenv("TT"))) : (stall > 60 && stall < 200))) printf("t=%d h=%d px=%d py=%d vx=%d vy=%d gnd=%d roll=%d g=%d\n", t, h, px, py, (int)fvx, (int)fvy, onGround, rolling, groundAt(px));
    if (mode == DEAD) { deaths++; if (verbose) printf("    died at x=%d y=%d\n", px, py); }
    lives = 99;
    if (getenv("GOD")) { invul = 2; if (fpy > I2F(groundAt(px) + 40)) {} }
  }
  printf("  TIMEOUT best=%d\n", best); return best;
}

int main(int argc, char **argv) {
  if (argc > 2) { playAct(atoi(argv[1]), atoi(argv[2]), true, argc > 3 ? atoi(argv[3]) : 0); return 0; }
  for (int z = 0; z < 4; z++) for (int a = 0; a < 2; a++) { printf("Zone %d Act %d\n", z+1, a+1); playAct(z, a, true, 0); }
}
