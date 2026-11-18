# Adding a game

Use this when building the next title for the same watch hardware.

## Choose an approach

### A. New sketch (recommended for distinct games)

Create `games/<name>/<name>.ino` (or top-level `<name>/<name>.ino`) that:

1. Copies the **platform shell** from `sonic_watch` (pins, buttons, clock, modes, `setup`/`loop`).
2. Replaces only the `GAME` / `MENU` / `OVER` / `WIN` logic and drawing.
3. Keeps the same pin map from [HARDWARE.md](HARDWARE.md).

Flash with:

```bash
./scripts/flash.sh path/to/sketch_folder
```

Pros: simple, no shared-library tooling. Cons: clock code duplicated until factored later.

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
// --- keep platform pins, buttons, clock, SPLASH/VIEW/SETM/RING ---

enum : uint8_t { /* ... */ GAME, MENU, OVER, WIN };

void startGame() { /* reset state, mode = GAME */ }

void gameStep(uint8_t e) {
  if (e & B_GAME) { /* pause */ }
  if (e & B_MODE) { mode = VIEW; return; }
  // input → simulate → collide
}

void drawGame() {
  // world, sprites, HUD
}

// wire into update()/render() switch like sonic_watch
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
- [ ] Alarm can interrupt or at least is not broken
- [ ] Game starts from Game button on clock
- [ ] Pause + quit paths work
- [ ] No `delay()` in the frame path
- [ ] Beeps are short

## Ideas that fit this hardware

Good fits: runners, dodge-em-ups, tiny puzzle (sliding / match), breakout, flappy-likes, turn-based micro RPGs with huge pixels.

Poor fits: anything needing analog sticks, color, fine text paragraphs, or complex 3D.
