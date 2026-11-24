// Host runner: plays the real hellgate_watch.ino with scripted buttons and dumps screenshots.
//   out/run "<script>" [prefix]
// script tokens: <frames>:<buttons>   buttons: L R U(forward) D(back, Mode) F(fire/use, OK) . = none
//   "shot" dumps the frame · "every:N" dumps every N frames · "face:N" animates and dumps the clock face
//   "start" starts a new game from the title · "god" makes the player immune
//   "at:<x>:<y>:<deg>" puts the player at tile x,y (centre) facing deg (0 east, 90 south)
//   "state" prints the player and monster state
#include "pp.cpp"
#include <stdio.h>
#include <string>
#include <sstream>
unsigned long g_ms; uint8_t watch::g_held; int watch::g_tune;
static int shotN = 0; static std::string prefix = "f";
void dump() {
  char fn[512]; snprintf(fn, sizeof fn, "%s_%03d.pbm", prefix.c_str(), shotN++);
  FILE *f = fopen(fn, "w"); fprintf(f, "P1\n128 64\n");
  for (int y = 0; y < 64; y++) { for (int x = 0; x < 128; x++) fputc((u8g2.buf[(y>>3)*128+x] >> (y&7)) & 1 ? '1' : '0', f); fputc('\n', f); }
  fclose(f);
}
void frame(uint8_t held, uint8_t prev) {
  watch::g_held = held; g_ms += 33;
  if (watch::g_tune > 0) watch::g_tune -= 33;
  gameUpdate(held & ~prev);
  u8g2.clearBuffer(); gameDraw();
}
void state(int t) {
  printf("t=%d mode=%d x=%.2f y=%.2f ang=%.0f hp=%u ammo=%u/%u wpn=%u key=%d kills=%u/%u\n", t, mode, px / 65536.0, py / 65536.0,
         (pAng >> 4) * 360.0 / 1024, health, bullets, shells, weapon, haveKey, kills, totalKills);
  for (int i = 0; i < nEnts; i++) if (ents[i].live && ents[i].kind <= K_FIEND)
    printf("  enemy %d kind=%d st=%d hp=%u at %.2f,%.2f\n", i, ents[i].kind, ents[i].st, ents[i].hp, ents[i].x / 65536.0, ents[i].y / 65536.0);
  for (int i = 0; i < nDoors; i++) printf("  door %d (%d,%d) kind=%d st=%d open=%.2f\n", i, doors[i].tx, doors[i].ty, doors[i].kind, doors[i].st, doors[i].open / 65536.0);
}
int main(int argc, char **argv) {
  if (argc > 2) prefix = argv[2];
  gameStart();
  std::stringstream ss(argc > 1 ? argv[1] : "");
  std::string tok; uint8_t prev = 0; int every = 0, fcount = 0;
  while (ss >> tok) {
    if (tok == "shot") { dump(); continue; }
    if (tok == "start") { newGame(); continue; }
    if (tok == "god") { god = true; continue; }
    if (tok == "state") { state(fcount); continue; }
    if (tok.rfind("every:", 0) == 0) { every = atoi(tok.c_str() + 6); continue; }
    if (tok.rfind("face:", 0) == 0) { int n = atoi(tok.c_str() + 5); for (int i = 1; i <= n; i++) { stepWatchFace(); g_ms += 33; if (every && i % every == 0) { u8g2.clearBuffer(); drawView(); dump(); } } if (!every) { u8g2.clearBuffer(); drawView(); dump(); } continue; }
    if (tok.rfind("at:", 0) == 0) { int x, y, d; sscanf(tok.c_str(), "at:%d:%d:%d", &x, &y, &d);
      px = ((q16)x << 16) + ONE / 2; py = ((q16)y << 16) + ONE / 2; pAng = (uint16_t)((d % 360 + 360) % 360 * 16384L / 360); pSpeed = 0; continue; }
    int n = atoi(tok.c_str()); auto c = tok.find(':'); std::string b = c == std::string::npos ? "." : tok.substr(c + 1);
    uint8_t held = 0;
    for (char ch : b) held |= ch=='L'?B_LEFT: ch=='R'?B_RIGHT: ch=='U'?B_FWD: ch=='D'?B_BACK: ch=='F'?B_FIRE: 0;
    for (int i = 0; i < n; i++) { frame(held, prev); prev = held; fcount++; if (every && fcount % every == 0) dump(); }
  }
  state(fcount);
}
