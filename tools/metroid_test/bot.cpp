// Physics bot: explores Metroid with the real game code and real button presses, to check that every
// room the room checker says is reachable really is (the checker's tile model misses low ceilings,
// overhangs and other things only the physics knows).
//   out/bot            all gear stages     ·   out/bot <stage>   one stage (0 = no items ... 5 = everything)
// How: the game keeps all its state in globals, so a "snapshot" is a copy of the program's data+bss
// (~7 KB, built without the U8g2 library). Breadth-first search over (room, tile, ball) spots: from each
// spot try a list of button macros (walks, jumps, double/space jumps, rolls, bomb jumps, shots), let
// Samus settle, and keep a snapshot of every new spot. God mode, endless missiles; the stage's gear is
// fixed (pickups don't add to it) and bosses that drop the stage's gear count as beaten.
// The bot's own state lives on main()'s stack, never in globals, so restoring a snapshot can't touch it.
#include "pp.cpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <unordered_map>
unsigned long g_ms; uint8_t watch::g_held; int watch::g_tune;
EEPROMClass EEPROM;
void watch::print2(uint8_t v) { (void)v; }

extern char __data_start, _end;
static uint8_t botPrev;
static void frame(uint8_t held) {
  watch::g_held = held; g_ms += 33;
  if (watch::g_tune > 0) watch::g_tune -= 33;
  uint8_t e = held & ~botPrev; botPrev = held;
  gameUpdate(e);
}

struct Step { uint8_t frames, held; };
struct Macro { const char *name; std::vector<Step> steps; uint8_t need; };   // need: gear bits required
enum : uint8_t { L = B_LEFT, R = B_RIGHT, U = B_MID, D = B_MODE, F = B_GAME };

int main(int argc, char **argv) {
  const size_t SNAP = &_end - &__data_start;
  struct Stage { const char *name; uint8_t gear, bosses; } stages[] = {
    {"no items", 0, 0}, {"Morph Ball", G_MORPH, 0}, {"Missiles", G_MORPH | G_MISSILE, 0},
    {"Bombs", G_MORPH | G_MISSILE | G_BOMB, 0}, {"Space Jump", G_MORPH | G_MISSILE | G_BOMB | G_SPACE, 1},
    {"everything", G_MORPH | G_MISSILE | G_BOMB | G_SPACE | G_ICE, 3}};
  std::vector<Macro> macros;
  // jumps, generated: direction held (none/L/R), from when (steer), how long Up is held, and whether
  // and when a second press comes (double jump) - so timing-sensitive climbs get tried properly
  for (uint8_t d : {(uint8_t)0, (uint8_t)L, (uint8_t)R})
    for (uint8_t steer : {(uint8_t)0, (uint8_t)5, (uint8_t)10})
      for (uint8_t hold : {(uint8_t)4, (uint8_t)9, (uint8_t)14})
        for (uint8_t again : {(uint8_t)0, (uint8_t)3, (uint8_t)7, (uint8_t)12})
         for (uint8_t letGo : {(uint8_t)0, (uint8_t)1}) {     // keep steering after landing, or let go
          if (!d && (steer || letGo)) continue;
          std::vector<Step> st;
          uint8_t t = 0;
          auto add = [&](uint8_t n, uint8_t h) { if (n) st.push_back({n, h}); };
          // first jump
          for (uint8_t f = 0; f < hold; f++, t++) add(1, (uint8_t)(U | (t >= steer ? d : 0)));
          if (again) {
            for (uint8_t f = 0; f < again; f++, t++) add(1, (uint8_t)(t >= steer ? d : 0));
            for (uint8_t f = 0; f < 10; f++, t++) add(1, (uint8_t)(U | (t >= steer ? d : 0)));
          }
          for (uint8_t f = 0; f < 26; f++, t++) add(1, (uint8_t)(t >= steer && !(letGo && f >= 8) ? d : 0));
          macros.push_back({"jump", st, 0});
        }
  for (uint8_t d : {L, R}) {
    macros.push_back({"nudge", {{1, d}}, 0});
    macros.push_back({"step", {{2, d}}, 0});
    macros.push_back({"walk", {{4, d}}, 0});
    macros.push_back({"walk far", {{12, d}}, 0});
    macros.push_back({"run jump", {{6, d}, {12, (uint8_t)(U | d)}, {24, d}}, 0});
    macros.push_back({"space climb", {{8, U}, {1, 0}, {7, U}, {1, 0}, {7, U}, {1, 0}, {7, U}, {1, 0}, {7, U}, {1, 0}, {7, (uint8_t)(U | d)}, {1, d}, {7, (uint8_t)(U | d)}, {20, d}}, G_SPACE});
    macros.push_back({"roll", {{8, d}}, G_MORPH});
    macros.push_back({"roll far", {{20, d}}, G_MORPH});
    macros.push_back({"bomb jump", {{1, F}, {17, 0}, {12, d}, {10, d}}, G_BOMB});
    macros.push_back({"turn and shoot", {{1, d}, {1, 0}, {1, F}, {4, 0}}, 0});
    // a shot opens a blue door for a while: shoot and go through in one move
    macros.push_back({"shoot and walk through", {{1, d}, {1, 0}, {1, F}, {3, 0}, {30, d}}, 0});
    macros.push_back({"shoot and run through", {{1, d}, {1, 0}, {1, F}, {3, 0}, {60, d}}, 0});
  }
  macros.push_back({"space climb up", {{8, U}, {1, 0}, {7, U}, {1, 0}, {7, U}, {1, 0}, {7, U}, {1, 0}, {7, U}, {1, 0}, {7, U}, {24, 0}}, G_SPACE});
  macros.push_back({"morph", {{1, D}, {2, 0}, {1, D}, {4, 0}}, G_MORPH});
  macros.push_back({"stand", {{1, U}, {6, 0}}, G_MORPH});
  macros.push_back({"bomb", {{1, F}, {24, 0}}, G_BOMB});
  macros.push_back({"shoot", {{1, F}, {4, 0}}, 0});

  int only = argc > 1 ? atoi(argv[1]) : -1;
  for (int si = 0; si < 6; si++) {
    if (only >= 0 && si != only) continue;
    const Stage &st = stages[si];
    // a fresh game from the title, then this stage's gear
    gameStart();
    for (int i = 0; i < 20; i++) frame(0);
    frame(F); for (int i = 0; i < 20; i++) frame(0);
    frame(D); for (int i = 0; i < 40; i++) frame(0);
    frame(D); for (int i = 0; i < 40; i++) frame(0);
    gear = st.gear; gearOff = 0; bossesBeaten = st.bosses; maxMissiles = missiles = 99; weapon = (st.gear & G_MISSILE) ? 1 : 0;
    crouch = false; ball = false; for (int i = 0; i < 10; i++) frame(0);
    std::vector<std::vector<char>> snaps;
    std::unordered_map<uint64_t, int> seen;
    std::vector<int> queue;
    std::vector<std::vector<int>> edges;          // moves found: spot -> spots
    std::vector<std::vector<int>> edgeMacro;
    std::vector<char> isSave;                     // standing on the ship's pad or a station
    std::vector<int> spotRoom;
    // a spot: room, 2 px across, tile down, ball, facing (fine enough that small steps add up)
    auto key = [&]() -> uint64_t { return ((uint64_t)curRoom << 40) | ((uint64_t)(toPx(sX) >> 1) << 20) | ((uint64_t)(toPx(sY) >> 3) << 2) | (ball ? 2 : 0) | (faceR ? 1 : 0); };
    auto keep = [&]() -> int {
      uint64_t k = key();
      auto it = seen.find(k);
      if (it != seen.end()) return it->second;
      int id = (int)snaps.size();
      seen[k] = id;
      snaps.emplace_back(&__data_start, &_end);
      edges.emplace_back(); edgeMacro.emplace_back();
      char t = rawTile(toPx(sX) >> 3, toPx(sY) >> 3);
      isSave.push_back(!ball && (t == 'S' || t == 'P'));
      spotRoom.push_back(curRoom);
      queue.push_back(id);
      return id;
    };
    keep();
    bool roomSeen[64] = {false}; roomSeen[curRoom] = true;
    size_t qi = 0;
    const size_t MAX_SPOTS = 60000;
    while (qi < queue.size() && snaps.size() < MAX_SPOTS) {
      int n = queue[qi++];
      for (const Macro &m : macros) {
        if (m.need && !(st.gear & m.need)) continue;
        memcpy(&__data_start, snaps[n].data(), SNAP);
        if (!strcmp(m.name, "stand") && !ball) continue;
        if ((!strcmp(m.name, "roll") || !strcmp(m.name, "roll far") || !strcmp(m.name, "bomb") || !strcmp(m.name, "bomb jump")) && !ball) continue;
        if (!strcmp(m.name, "morph") && ball) continue;
        bool ok = true;
        for (const Step &s : m.steps) {
          for (int f = 0; f < s.frames; f++) {
            frame(s.held);
            invul = 250; energy = maxEnergy; missiles = 99; gear = st.gear; bossesBeaten |= st.bosses;
            if (mode == M_ITEM) { itemT = 255; frame(F); frame(0); }
            if (mode != M_PLAY && mode != M_ITEM) { ok = false; break; }
          }
          if (!ok) break;
        }
        if (!ok) continue;
        // settle: until she's on the ground (or the ball has stopped)
        for (int f = 0; f < 90 && !(ground && sVY == 0 && trState == TR_NONE); f++) {
          frame(0); invul = 250; energy = maxEnergy; gear = st.gear;
          if (mode == M_ITEM) { itemT = 255; frame(F); frame(0); }
          if (mode != M_PLAY && mode != M_ITEM) { ok = false; break; }
        }
        if (!ok || !ground || curRoom < 0) continue;
        if (ROOMS[curRoom].zone == ZONE_TEST) continue;
        roomSeen[curRoom] = true;
        int v = keep();
        edges[n].push_back(v); edgeMacro[n].push_back((int)(&m - &macros[0]));
      }
    }
    printf("  with %s, the bot reaches (%zu spots%s):", st.name, snaps.size(), snaps.size() >= MAX_SPOTS ? ", capped" : "");
    for (int r = 0; r < ROOM_N; r++) if (roomSeen[r]) printf(" %s", ROOMS[r].id);
    printf("\n");
    // backtracking with real physics: every spot must be able to get back to a save
    std::vector<std::vector<int>> back(snaps.size());
    for (size_t u = 0; u < edges.size(); u++) for (int v : edges[u]) back[v].push_back((int)u);
    std::vector<char> ok(snaps.size(), 0); std::vector<int> stack;
    for (size_t i = 0; i < snaps.size(); i++) if (isSave[i]) { ok[i] = 1; stack.push_back((int)i); }
    while (!stack.empty()) { int v = stack.back(); stack.pop_back(); for (int u : back[v]) if (!ok[u]) { ok[u] = 1; stack.push_back(u); } }
    int stuck[64] = {0};
    for (size_t i = 0; i < snaps.size(); i++) if (!ok[i]) stuck[spotRoom[i]]++;
    bool any = false;
    for (int r = 0; r < ROOM_N; r++) if (stuck[r]) {
      // the shrines are meant to be dead ends until Samus has their item
      bool shrine = false; int nt = rTW(r) * rTH(r);
      for (int i = 0; i < nt; i++) { char c = ROOMS[r].tiles[i]; if (((c == 'm' || c == 'M') && !(st.gear & G_MORPH)) || ((c == 'o' || c == 'O') && !(st.gear & G_BOMB))) shrine = true; }
      printf("    %s%s: %d spots can't get back to a save\n", ROOMS[r].id, shrine ? " (shrine, expected)" : "", stuck[r]);
      if (!shrine) any = true;
    }
    if (!any) printf("    every spot can get back to a save\n");
    if (getenv("ROOM")) {                          // debug: spots in one room by height, and whether they get home
      int want = -1; for (int r = 0; r < ROOM_N; r++) if (!strcmp(ROOMS[r].id, getenv("ROOM"))) want = r;
      std::unordered_map<int, std::pair<int,int>> byY;
      for (size_t i = 0; i < snaps.size(); i++) if (spotRoom[i] == want) {
        memcpy(&__data_start, snaps[i].data(), SNAP);
        auto &c = byY[toPx(sY)]; c.first++; if (ok[i]) c.second++;
      }
      for (auto &kv : byY) printf("      y=%d: %d spots, %d get home\n", kv.first, kv.second.first, kv.second.second);
      if (getenv("UP")) {                          // debug: moves out of the room's lowest spots that go up/out
        int lowY = 0; for (auto &kv : byY) lowY = std::max(lowY, kv.first);
        int shown = 0;
        for (size_t i = 0; i < snaps.size() && shown < 12; i++) if (spotRoom[i] == want) {
          memcpy(&__data_start, snaps[i].data(), SNAP); if (toPx(sY) != lowY) continue; int x0 = toPx(sX);
          for (size_t j = 0; j < edges[i].size(); j++) { int v = edges[i][j];
            memcpy(&__data_start, snaps[v].data(), SNAP);
            if (spotRoom[v] != want || toPx(sY) < lowY - 8) { printf("        from x=%d: macro #%d (%s) -> %s x=%d y=%d\n", x0, edgeMacro[i][j], macros[edgeMacro[i][j]].name, ROOMS[spotRoom[v]].id, toPx(sX), toPx(sY)); shown++; } }
        }
      }
      if (getenv("Y")) for (size_t i = 0; i < snaps.size(); i++) if (spotRoom[i] == want) {
        memcpy(&__data_start, snaps[i].data(), SNAP);
        if (toPx(sY) != atoi(getenv("Y"))) continue;
        int x0 = toPx(sX); bool fr = faceR; uint8_t gnd = ground; uint8_t lock = inputLock; uint8_t md = mode;
        frame(U | R); for (int f = 0; f < 10; f++) { frame(U | R); invul = 250; } for (int f = 0; f < 30; f++) { frame(0); invul = 250; }
        printf("        x=%d face=%d gnd=%d lock=%d mode=%d -> x=%d y=%d\n", x0, fr, gnd, lock, md, toPx(sX), toPx(sY));
      }
    }
    fflush(stdout);
  }
  return 0;
}
