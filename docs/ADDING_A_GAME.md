# Adding a game

Use this when building the next title for the same watch hardware.

## Choose an approach

### A. New sketch (recommended for distinct games)

Create `<name>/<name>.ino` that:

1. `#include <WatchCore.h>` — buttons, clock, alarm, set screens and power save come for free.
2. Draws its own **themed clock face** (`faceStep` / `faceDraw`) — every watch has its own time art.
3. Implements the game in `appStart` / `appUpdate` / `appDraw` and calls `watch::exitToClock()` to leave.

Flash with:

```bash
./scripts/flash.sh path/to/sketch_folder
```

### B. Multi-game single firmware

Later option: from `VIEW`, `GAME` opens a title picker; each game is a module with `start` / `step` / `draw`. Worth it once you have 3+ titles and hate reflashing.

For now, prefer **A**.

## Hard constraints

| Constraint | Guidance |
|------------|----------|
| 128×64 1-bit OLED | High contrast silhouettes; minimal text |
| 5 buttons | One primary action on Mid; holds on Left/Right |
| ~30 FPS | Simple physics; few entities |
| Watch UX | Always escape to clock via Mode |
| Short sessions | Clear goal in &lt; 2 minutes; 3 lives / stages / score |

## Suggested skeleton

```cpp
#include <Arduino.h>
#include <WatchCore.h>

enum : uint8_t { PLAY, PAUSE, OVER };
uint8_t mode;

// ---- theme: clock face ----
uint8_t anim;
void faceStep() { anim++; }
void faceDraw() {
  u8g2.setFont(u8g2_font_logisoso24_tn);
  u8g2.setCursor(28, 36);
  watch::print2(watch::hours()); u8g2.print(':'); watch::print2(watch::minutes());
  // ... your theme's art around it
}

// ---- game ----
void gameStart() { /* reset state */ mode = PLAY; }
void gameUpdate(uint8_t e) {
  if (mode == PLAY) {
    if (e & B_GAME) { mode = PAUSE; return; }
    if (e & B_MODE) { watch::exitToClock(); return; }
    // input (e edges, watch::held()) → simulate → collide
  }
}
void gameDraw() { /* world, sprites, HUD */ }
void gameInterrupted() { if (mode == PLAY) mode = PAUSE; }

const watch::WatchApp APP = {
  faceStep, faceDraw, gameStart, gameUpdate, gameDraw,
  gameInterrupted, nullptr, nullptr, "MID",
};
void setup() { watch::begin(APP); }
void loop()  { watch::run(); }
```

## Input conventions (stay consistent)

| Action | Binding |
|--------|---------|
| Primary | Mid (edge) |
| Secondary / hold | Right held |
| Brake / alt | Left |
| Pause | Game |
| Exit to clock | Mode |

Players should not relearn the watch for each title.

## Art tips for SH1106

- Draw player facing right when the camera scrolls right.
- Animate with 2–4 frames max (`animFrame` counter).
- Invert color briefly for hits; blink during invulnerability.
- Zone / stage banners: filled box + frame + two lines of 6x10 text.

## Checklist before flash

- [ ] Compiles for `rp2040:rp2040:waveshare_rp2040_zero`
- [ ] Clock still works from Mode
- [ ] Alarm interrupts the game and returns to your pause menu
- [ ] Game starts from Game button on clock
- [ ] Pause + quit paths work
- [ ] No `delay()` in the frame path
- [ ] Beeps are short

## Ideas that fit this hardware

Good fits: runners, dodge-em-ups, tiny puzzle (sliding / match), breakout, flappy-likes, turn-based micro RPGs with huge pixels.

Poor fits: anything needing analog sticks, color, fine text paragraphs, or complex 3D.
