// Host screenshot runner for the real sonic_watch.ino (same idea as metroid_test/run.cpp):
//   out/shots "<script>" [prefix]
// tokens: <frames>:<buttons> (L R U=jump D=mode F=game, . = none) · shot · face:<frames> (animate and
//   dump the clock face) · title · act:<zone>:<act> · special · toboss · run:<x> (bot runs right until px >= x)
#include "pp.cpp"
#include <stdio.h>
#include <string>
#include <sstream>
unsigned long g_ms; uint8_t watch::g_held; int watch::g_tune;
static int shotN = 0; static std::string prefix = "s";
static bool faceView = false;
void dump() {
  u8g2.clearBuffer(); if (faceView) drawView(); else gameDraw();
  char fn[512]; snprintf(fn, sizeof fn, "%s_%03d.pbm", prefix.c_str(), shotN++);
  FILE *f = fopen(fn, "w"); fprintf(f, "P1\n128 64\n");
  for (int y = 0; y < 64; y++) { for (int x = 0; x < 128; x++) fputc((u8g2.buf[(y>>3)*128+x] >> (y&7)) & 1 ? '1' : '0', f); fputc('\n', f); }
  fclose(f);
}
uint8_t prevHeld;
void frame(uint8_t held) {
  watch::g_held = held; uint8_t e = held & ~prevHeld; prevHeld = held;
  g_ms += 33; if (watch::g_tune > 0) watch::g_tune--;
  gameUpdate(e); lives = 99;
}
int main(int argc, char **argv) {
  if (argc > 2) prefix = argv[2];
  std::stringstream ss(argc > 1 ? argv[1] : ""); std::string tok;
  while (ss >> tok) {
    if (tok == "shot") { uint8_t iv = invul; invul = 0; dump(); invul = iv; continue; }   // never catch him mid-blink
    if (tok.rfind("face:", 0) == 0) { faceView = true; for (int i = atoi(tok.c_str() + 5); i > 0; i--) stepWatchFace(); dump(); faceView = false; continue; }
    if (tok == "title") { gameStart(); for (int i = 0; i < 40; i++) frame(0); continue; }
    if (tok.rfind("act:", 0) == 0) { int z, a; sscanf(tok.c_str(), "act:%d:%d", &z, &a); gameStart(); startGame(); startAct(z, a); trState = TR_NONE; trLevel = 0; continue; }
    if (tok == "special") { startSpecial(); trState = TR_NONE; trLevel = 0; continue; }
    if (tok.rfind("rings:", 0) == 0) { rings = atoi(tok.c_str() + 6); continue; }
    if (tok == "toboss") { for (int t = 0; t < 30 * 240 && !bossOn && mode == GAME; t++) { frame(B_RIGHT | ((t % 40) < 10 ? B_MID : 0)); invul = 2; } continue; }
    if (tok.rfind("run:", 0) == 0) {          // hold right, hop when stalled (like bot.cpp)
      int goal = atoi(tok.c_str() + 4), best = px, bestT = 0;
      for (int t = 0; t < 30 * 120 && px < goal && (mode == GAME || mode == DEAD); t++) {
        if (px > best + 4) { best = px; bestT = t; }
        int stall = t - bestT; uint8_t h = B_RIGHT;
        if (stall > 6) h |= (stall % 24) < 18 ? B_MID : 0;
        if (stall > 60 && (stall % 90) < 20) h = B_LEFT;
        frame(h); invul = 2;
      }
      continue;
    }
    int n = atoi(tok.c_str()); auto c = tok.find(':'); std::string b = c == std::string::npos ? "." : tok.substr(c + 1);
    uint8_t held = 0;
    for (char ch : b) held |= ch=='L'?B_LEFT: ch=='R'?B_RIGHT: ch=='U'?B_MID: ch=='D'?B_MODE: ch=='F'?B_GAME: 0;
    for (int i = 0; i < n; i++) frame(held);
  }
  printf("end mode=%d zone=%d act=%d px=%d py=%d rings=%d boss=%d sx=%d sy=%d\n", mode, zone, act, px, py, rings, bossOn, px - cam, py - camY);
}
