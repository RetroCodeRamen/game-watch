// Stop finder: bot plays every act and prints each time Sonic is stopped with no wall,
// spike, enemy or real cliff nearby (ALL=1 prints every stop). Usage: out/stops [rightDuty/6] [minSpeed fx]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
struct StopRec { const char *r; int a, b; };
static StopRec g_stop[16]; static int g_nstop;
#define STOP_HOOK(reason, A, B) do { if (g_nstop < 16) g_stop[g_nstop++] = {reason, (int)(A), (int)(B)}; } while (0)
#define main bot_main_unused
#include "bot.cpp"
#undef main
// Plays each act with the same bot; logs each stop where Sonic was moving at speed.
int main(int argc,char**argv) { int duty = argc>1?atoi(argv[1]):6; int thr = argc>2?atoi(argv[2]):384;
  for (int z = 0; z < 4; z++) for (int a = 0; a < 2; a++) {
    startGame(); startAct(z, a); lives = 99;
    int best = 0, bestT = 0, lastLogX = -999;
    for (int t = 0; t < 30 * 240; t++) {
      if (mode == CLEAR || mode == SPECIAL || bossOn) break;
      if (mode == DEAD) { frame(0); continue; }
      if (px > best + 4) { best = px; bestT = t; }
      uint8_t h = (t % 6) < duty ? B_RIGHT : 0; int stall = t - bestT;
      if (stall > 6 && stall <= 60) h = B_RIGHT | ((stall % 24) < 18 ? B_MID : 0);
      if (stall > 60) { static const int BACK[] = {4,10,20,35,6,50,15,70}; static const int J[] = {0,0,0,0,1,1,1,1};
        int s = ((stall-60)/90)%8, ph=(stall-60)%90; if (ph < BACK[s]) h = B_LEFT; else h = B_RIGHT | (J[s] && ph > BACK[s]+12 && ph < BACK[s]+30 ? B_MID : 0); }
      if (stall > 30*25) break;
      int vx0 = fvx, x0 = px, y0 = py, g0 = onGround, roll0 = groundRoll || rolling;
      g_nstop = 0;
      frame(h);
      invul = 2; lives = 99;
      if (g_nstop && abs(vx0) >= thr && abs(px - lastLogX) > 6) {
        lastLogX = px;
        bool vis = false;
        for (auto &o : objs) if (o.live && abs(o.x - x0) < 24 && (o.kind==0||o.kind==7||o.kind==10||o.kind==4||o.kind==2)) vis = true;
        for (int i = 0; i < g_nstop; i++) if (!strcmp(g_stop[i].r,"cliff") && groundAt(x0) - groundAt(g_stop[i].a) > CLIFF_STEP) vis = true;
        if (vis && !getenv("ALL")) continue;
        printf("Z%dA%d x=%d y=%d vx=%.1f gnd=%d roll=%d ->vx=%.1f :", z+1, a+1, x0, y0, vx0/256.0, g0, roll0, fvx/256.0);
        for (int i = 0; i < g_nstop; i++) printf(" %s(%d,%d)", g_stop[i].r, g_stop[i].a, g_stop[i].b);
        printf(" | terr");
        for (int dx = -8; dx <= 16; dx += 4) printf(" %d", groundAt(x0 + dx));
        printf(" | near:");
        for (auto &o : objs) if (o.live && abs(o.x - x0) < 30) printf(" k%d@%d,%d", o.kind, o.x, o.y);
        for (auto &p : plats) if (p.live && p.x < x0 + 30 && p.x + p.w > x0 - 30) printf(" plat%d-%d@%d", p.x, p.x + p.w, p.y);
        printf("\n");
      }
    }
  }
}
