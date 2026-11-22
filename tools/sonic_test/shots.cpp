// Host screenshot runner for the real sonic_watch.ino (same idea as metroid_test/run.cpp):
//   out/shots "<script>" [prefix]
// tokens: <frames>:<buttons> (L R U=jump D=mode F=game, . = none) · shot · face:<frames> (animate and
//   dump the clock face) · title · act:<zone>:<act> · special · toboss · at:<x> · run:<x> (bot runs right until px >= x)
#include "pp.cpp"
#include <stdio.h>
#include <string>
#include <sstream>
unsigned long g_ms; uint8_t watch::g_held; int watch::g_tune;
static int shotN = 0; static std::string prefix = "s";
static bool faceView = false;
static int recEvery = 0, recLeft = 0, recCount = 0;   // "rec:<every>:<max>" dumps every Nth frame (GIF recording)
void dump() {
  u8g2.clearBuffer(); if (faceView) drawView(); else gameDraw();
  char fn[512]; snprintf(fn, sizeof fn, "%s_%03d.pbm", prefix.c_str(), shotN++);
  FILE *f = fopen(fn, "w"); fprintf(f, "P1\n128 64\n");
  for (int y = 0; y < 64; y++) { for (int x = 0; x < 128; x++) fputc((u8g2.buf[(y>>3)*128+x] >> (y&7)) & 1 ? '1' : '0', f); fputc('\n', f); }
  fclose(f);
}
uint8_t prevHeld;
void recTick() {
  if (recEvery && recLeft > 0 && ++recCount % recEvery == 0) { uint8_t iv = invul; if (iv <= 2) invul = 0; dump(); invul = iv; recLeft--; }
}
void frame(uint8_t held) {
  watch::g_held = held; uint8_t e = held & ~prevHeld; prevHeld = held;
  g_ms += 33; if (watch::g_tune > 0) watch::g_tune--;
  gameUpdate(e); lives = 99;
  recTick();
}
int main(int argc, char **argv) {
  if (argc > 2) prefix = argv[2];
  std::stringstream ss(argc > 1 ? argv[1] : ""); std::string tok;
  while (ss >> tok) {
    if (tok == "hit") { rings = 10; invul = 0; shieldOn = false; starT = 0; hurt(); continue; }   // take a hit (keeps the rings rule)
    if (tok == "die") { rings = 0; invul = 0; shieldOn = false; starT = 0; hurt(); continue; }   // lose a life now
    if (tok == "save") { SonicSave v; bool ok = readSave(); v = lastSave; printf("  save: %s zone=%d act=%d starX=%d t=%u lives=%d em=%d score=%lu  title shows save=%d\n", ok ? "yes" : "no", v.zone, v.act, v.starX, v.starTime, v.lives, v.emeralds, (unsigned long)v.score, haveSave); continue; }
    if (tok == "st") { printf("  zone=%d act=%d px=%d py=%d shield=%d invul=%d rings=%d mode=%d gnd=%d starX=%d t=%u lives=%d\n", zone, act, px, py, shieldOn, invul, rings, mode, onGround, starX, actFrames, lives); continue; }
    if (tok == "shot") { uint8_t iv = invul; invul = 0; dump(); invul = iv; continue; }   // never catch him mid-blink
    if (tok.rfind("rec:", 0) == 0) { sscanf(tok.c_str(), "rec:%d:%d", &recEvery, &recLeft); recCount = 0; continue; }
    if (tok.rfind("face:", 0) == 0) { faceView = true; for (int i = atoi(tok.c_str() + 5); i > 0; i--) { stepWatchFace(); g_ms += 33; recTick(); } if (!recEvery) dump(); faceView = false; continue; }
    if (tok == "title") { gameStart(); for (int i = 0; i < 40; i++) frame(0); continue; }
    if (tok.rfind("act:", 0) == 0) { int z, a; sscanf(tok.c_str(), "act:%d:%d", &z, &a); gameStart(); startGame(); startAct(z, a); trState = TR_NONE; trLevel = 0; continue; }
    if (tok == "special") { startSpecial(); trState = TR_NONE; trLevel = 0; continue; }
    if (tok.rfind("at:", 0) == 0) { int x = atoi(tok.c_str() + 3); setPlayerPos(x, groundAt(x) - RAD_STAND); fvx = fvy = 0; cam = x - 64; continue; }   // put Sonic on the ground at world x
    if (tok.rfind("rings:", 0) == 0) { rings = atoi(tok.c_str() + 6); continue; }
    if (tok.rfind("spbot:", 0) == 0) {      // Special Stage bot: N runs of stage k (emeralds held = k-1)
      // "spbot:<stage>:<runs>[:kid]" - kid: reacts 10 frames late, sloppier steering, misses
      // a third of the jumps (closer to a real 8-year-old than a frame-perfect bot)
      int k = 0, n = 0, kid = 0; sscanf(tok.c_str(), "spbot:%d:%d:%d", &k, &n, &kid);
      int wins = 0, legsDone[4] = {0};
      for (int run = 0; run < n; run++) {
        randomSeed(1000 + run * 7919);
        emeralds = k - 1; startSpecial(); trState = TR_NONE; trLevel = 0;
        int jumpCool = 0; float wantQ[16] = {0}; bool pairQ[16] = {0};
        for (int t = 0; t < 30 * 120 && mode == SPECIAL && spPhase == SP_RUN; t++) {
          // aim: the nearest ring 8-50 ahead that isn't right beside a bomb; dodge bombs 6-30 ahead
          float want = spAng; int bestZ = 999; bool bombAhead = false, pairAhead = false;
          for (auto &o : spObj) {
            if (!o.live) continue;
            int z = o.pos - (int)spDist;
            if (o.kind == SPO_BOMB && z > 4 && z < 26 && fabsf(o.ang - spAng) < 16) bombAhead = true;
            if (o.kind == SPO_BOMB && z > 4 && z < 14 && fabsf(o.ang - spAng) < 14) pairAhead = true;
          }
          for (auto &o : spObj) {
            if (!o.live || o.kind != SPO_RING) continue;
            int z = o.pos - (int)spDist;
            if (z < 8 || z > 50 || z >= bestZ) continue;
            bool nearBomb = false;
            for (auto &b : spObj) if (b.live && b.kind == SPO_BOMB && abs(b.pos - o.pos) < 8 && fabsf(b.ang - o.ang) < 14) nearBomb = true;
            if (!nearBomb) { bestZ = z; want = o.ang; }
          }
          if (bombAhead && fabsf(want - spAng) < 16) want = spAng + (spAng > 0 ? -30 : 30);
          bool jump = pairAhead;
          if (kid) {                               // act on what was seen 10 frames ago
            wantQ[t & 15] = want; pairQ[t & 15] = pairAhead;
            want = t >= 10 ? wantQ[(t - 10) & 15] : spAng; jump = t >= 10 && pairQ[(t - 10) & 15] && (rand() % 3);
          }
          uint8_t h = 0;
          int dead = kid ? 10 : 4;
          if (want < spAng - dead) h |= B_LEFT; else if (want > spAng + dead) h |= B_RIGHT;
          if (jumpCool) jumpCool--;
          if (jump && !jumpCool && !spAir) { h |= B_MID; jumpCool = 20; }
          frame(h);
        }
        int legs = spLeg; if (spWon) { wins++; legs = 3; }
        for (int L = 0; L < legs && L < 3; L++) legsDone[L]++;
      }
      printf("  stage %d: won %d/%d   legs passed: 1st %d, 2nd %d, 3rd %d\n", k, wins, n, legsDone[0], legsDone[1], legsDone[2]);
      continue;
    }
    if (tok.rfind("trace:", 0) == 0) {       // hold right n frames, printing his height above ground
      int n = atoi(tok.c_str() + 6);
      for (int t = 0; t < n; t++) { frame(B_RIGHT); invul = 2;
        printf("  t=%d px=%d feet=%d above=%d gnd=%d ledge=%d vx=%d vy=%d\n", t, px, py + RAD_STAND, groundAt(px) - (py + RAD_STAND), onGround, onLedge, (int)fvx, (int)fvy); }
      continue;
    }
    if (tok.rfind("route:", 0) == 0) {       // run right from here; how far does he travel up on platforms?
      int n = atoi(tok.c_str() + 6), upX0 = -1, upX1 = -1;
      for (int t = 0; t < n; t++) {
        frame(B_RIGHT); invul = 2;
        bool high = onGround && groundAt(px) - (py + RAD_STAND) > 40;   // standing well above the ground
        if (high) { if (upX0 < 0) upX0 = px; upX1 = px; }
      }
      printf("  route: on high platforms from x=%d to x=%d (%d px)\n", upX0, upX1, upX0 < 0 ? 0 : upX1 - upX0);
      continue;
    }
    if (tok == "fight") {                    // boss bot: wait on the side away from the ball, jump at the ship
      int t = 0, jumpT = 0, hits = 0, hp = bossHP, hurtN = 0;
      for (; t < 30 * 90 && bossOn && mode == GAME; t++) {
        int16_t side = ballWX < eggX ? 1 : -1;
        int16_t target = eggX + side * 16;
        uint8_t h = 0;
        if (jumpT) { jumpT--; h |= B_MID; }
        if (px < target - 3) h |= B_RIGHT; else if (px > target + 3) h |= B_LEFT;
        if (!jumpT && onGround && abs(px - target) < 8 && abs(ballWX - px) > 20 && bossPhase == EGG_FIGHT) { jumpT = 14; h |= B_MID; }
        uint8_t inv0 = invul;
        rings = 50;                            // measure hits and hurts, don't let a hurt end the run
        frame(h);
        if (invul > inv0 + 10) hurtN++;
        if (bossHP < hp) { hits++; hp = bossHP; }
      }
      printf("  fight: %d hits, %d hurts, boss %s after %.1f s\n", hits, hurtN, bossOn ? "still up" : "beaten", t / 30.0);
      continue;
    }
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
  printf("end mode=%d zone=%d act=%d px=%d py=%d rings=%d boss=%d sx=%d sy=%d shield=%d shoes=%d star=%d lives=%d\n", mode, zone, act, px, py, rings, bossOn, px - cam, py - camY, shieldOn, shoesT, starT, lives);
}
