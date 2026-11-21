// Host runner: plays the real metroid_watch.ino with scripted buttons and dumps screenshots.
//   out/run "<script>" [prefix]
// script tokens: <frames>:<buttons>  buttons from L R U(action) D(mode/down) F(fire/ok) . = none
//   e.g. "40:. 1:U 60:R 1:D"  ·  "shot" dumps the current frame  ·  "every:N" dumps every N frames
//   "warp:<room>:<tx>:<ty>" puts Samus on a tile  ·  "gear:<bits>"  ·  "god" makes her immune
//   "visit:<hex>" marks rooms explored · "playtime:<s>" sets the play clock
//   "face:<frames>" animates and dumps the clock face
//   "frz:<tx>" freezes the room's Rippers at column tx  ·  "keep:<kind>" removes every enemy but that kind (4 turret, 5 Swipe, 6 Bolt, 7 Metroid)
#include "pp.cpp"
#include <stdio.h>
#include <string>
#include <sstream>
unsigned long g_ms; uint8_t watch::g_held; int watch::g_tune;
EEPROMClass EEPROM;
void watch::print2(uint8_t v) { char b[4]; snprintf(b, 4, "%02d", v); u8g2.print(b); }
static int shotN = 0; static std::string prefix = "f";
void dump() {
  char fn[512]; snprintf(fn, sizeof fn, "%s_%03d.pbm", prefix.c_str(), shotN++);
  FILE *f = fopen(fn, "w"); fprintf(f, "P1\n128 64\n");
  for (int y = 0; y < 64; y++) { for (int x = 0; x < 128; x++) fputc((u8g2.buf[(y>>3)*128+x] >> (y&7)) & 1 ? '1' : '0', f); fputc('\n', f); }
  fclose(f);
}
bool god = false;
void frame(uint8_t held, uint8_t prev) {
  watch::g_held = held; g_ms += 33;
  if (watch::g_tune > 0) watch::g_tune -= 33;
  if (god) { invul = 250; energy = maxEnergy; }
  gameUpdate(held & ~prev);
  u8g2.clearBuffer(); gameDraw();
}
int main(int argc, char **argv) {
  if (argc > 2) prefix = argv[2];
  gameStart();
  std::stringstream ss(argc > 1 ? argv[1] : "");
  std::string tok; uint8_t prev = 0; int every = 0, fcount = 0;
  while (ss >> tok) {
    if (tok == "shot") { dump(); continue; }
    if (tok.rfind("face:", 0) == 0) { for (int i = atoi(tok.c_str() + 5); i > 0; i--) stepWatchFace(); u8g2.clearBuffer(); drawView(); dump(); continue; }   // the clock face
    if (tok.rfind("en:", 0) == 0) { energy = atoi(tok.c_str() + 3); continue; }
    if (tok == "god") { god = true; continue; }
    if (tok.rfind("visit:", 0) == 0) { visited |= strtoull(tok.c_str() + 6, nullptr, 16); continue; }   // mark rooms explored (hex bit mask by room index)
    if (tok.rfind("playtime:", 0) == 0) { playFrames = (uint32_t)atoi(tok.c_str() + 9) * 30; continue; }   // seconds on the play clock
    if (tok == "corrupt") { SaveData a2, b2; EEPROM.get(EE_SLOT_A, a2); EEPROM.get(EE_SLOT_B, b2); int at = a2.seq > b2.seq ? EE_SLOT_A : EE_SLOT_B; EEPROM.d[at + 9] ^= 0xFF; continue; }   // a power cut mid-save: damage the newer slot
    if (tok == "reboot") { gameStart(); continue; }   // the watch restarted: back to the title, flash kept
    if (tok == "havesave") { haveSave = true; continue; }   // pretend a save exists (title shows CONTINUE / NEW GAME)
    if (tok.rfind("beat:", 0) == 0) { bossesBeaten = atoi(tok.c_str() + 5); continue; }   // mark bosses beaten (1 Kraid 2 Ridley 4 Mother Brain)
    if (tok.rfind("frz:", 0) == 0) { int tx = atoi(tok.c_str() + 4); for (auto &e : en) if (e.live && e.kind == EN_RIPPER) { e.x = (rTX(curRoom) + tx) * 8 - 2; e.frozen = FREEZE_T; } continue; }   // freeze Rippers at a column
    if (tok.rfind("keep:", 0) == 0) { int k = atoi(tok.c_str() + 5); for (auto &e : en) if (e.kind != k) e.live = false; continue; }   // only this enemy kind
    if (tok.rfind("every:", 0) == 0) { every = atoi(tok.c_str() + 6); continue; }
    if (tok.rfind("gear:", 0) == 0) { gear = atoi(tok.c_str() + 5); if (gear & G_MISSILE) { maxMissiles = missiles = 10; } continue; }
    if (tok.rfind("warp:", 0) == 0) { int r, tx, ty; sscanf(tok.c_str(), "warp:%d:%d:%d", &r, &tx, &ty);
      sX = toFx((rTX(r) + tx) * 8 + 4) + 128; sY = toFx((rTY(r) + ty + 1) * 8); sVX = sVY = 0; pendingRoom = r; enterRoomNow(); continue; }
    if (tok == "enemies") { int n = 0, f = 0; for (auto &e : en) { n += e.live; f += e.live && e.frozen; } printf("t=%d live enemies %d frozen %d", fcount, n, f); for (auto &e : en) if (e.live && isBoss(e.kind)) printf(" boss hp=%d st=%d t=%d x=%d y=%d", e.hp, e.st, e.t, e.x, e.y); printf("\n"); continue; }
    if (tok == "state") { printf("t=%d mode=%d room=%s x=%d y=%d vx=%d vy=%d gnd=%d ball=%d spin=%d crouch=%d wpn=%d en=%d/%d ms=%d gear=%d orb=%x\n", fcount, mode,
        ROOMS[curRoom].id, toPx(sX), toPx(sY), sVX, sVY, ground, ball, spin, crouch, weapon, energy, maxEnergy, missiles, gear, orbOpen); continue; }
    int n = atoi(tok.c_str()); auto c = tok.find(':'); std::string b = c == std::string::npos ? "." : tok.substr(c + 1);
    uint8_t held = 0;
    for (char ch : b) held |= ch=='L'?B_LEFT: ch=='R'?B_RIGHT: ch=='U'?B_MID: ch=='D'?B_MODE: ch=='F'?B_GAME: 0;
    for (int i = 0; i < n; i++) { frame(held, prev); prev = held; fcount++; if (every && fcount % every == 0) dump(); }
  }
  printf("end t=%d esc=%d mode=%d room=%s x=%d y=%d en=%d/%d ms=%d gear=%d items=%llx\n", fcount, escapeT, mode, ROOMS[curRoom].id, toPx(sX), toPx(sY), energy, maxEnergy, missiles, gear, (unsigned long long)itemsGot);
}
