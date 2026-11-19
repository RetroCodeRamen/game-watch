# Software design

Target pattern for all Game Watch firmwares. The Sonic sketch (`sonic_watch/`) is the reference implementation.

## Runtime model

The shell lives in **`libraries/WatchCore`** and is shared by every firmware. A sketch supplies a
`watch::WatchApp` table of hooks and hands control to the library:

```cpp
const watch::WatchApp APP = {
  faceStep, faceDraw,          // theme: the clock face art (required)
  gameStart, gameUpdate, gameDraw,
  gameInterrupted,             // optional: alarm took over mid-game
  nullptr, nullptr,            // optional themed set / alarm screens
  "MID",                       // set-screen hint for the Mid button
};
void setup() { watch::begin(APP); }
void loop()  { watch::run(); }
```

`watch::run()`:

```
pending |= debounced button edges        (every spin of loop())
catch up the clock 1 s at a time         (alarm checked every second)
if the face is dark, a press only wakes it
every 33 ms (~30 FPS):
    update(pending)  → face / set / ring / your appUpdate()
    power-save step  (face only)
    clearBuffer → render → sendBuffer
```

**Rules**

1. Never block for more than a few ms (no `delay()` in the game path).
2. Read continuous input with `watch::held()`; react to presses via the `edges` argument.
3. Leave the game with `watch::exitToClock()` — don't draw your own clock.

## Mode machine

```
FACE ⇄ SET (time / date / alarm)
  │ Game
  ▼
 APP ── your own modes (splash, play, pause, over, win…) ── exitToClock() → FACE

any mode ──alarm──▶ RING ──any key / 60 s──▶ back where you were
```

| System mode | Owner | Role |
|-------------|-------|------|
| FACE | sketch art, library logic | Themed clock face; dims at 10 s, off at 20 s |
| SET | library (art overridable) | Mode cycles time → date → alarm; Mid next field; L/R adjust |
| RING | library (art overridable) | Fires from any mode; game is frozen, then resumed |
| APP | sketch | Everything game-side |

If the alarm interrupts the game, `appInterrupted()` runs first — use it to switch into your
pause menu so play doesn't resume instantly when the alarm is dismissed.

## Theming the watch

Each firmware has its own clock art. The **face is always the sketch's job**: `faceDraw()`
draws everything on screen (time, date, alarm badge, animation) using:

| Call | Returns |
|------|---------|
| `watch::hours()` / `minutes()` / `seconds()` | Current time |
| `watch::month()` / `day()` | Date (1-based) |
| `watch::alarmOn()` / `alarmHours()` / `alarmMinutes()` | Alarm |
| `watch::print2(v)` / `printMonth(m)` | Zero-padded digits / `JAN`..`DEC` at the cursor |
| `watch::frameCount()` | Rendered-frame counter for animation |

`faceStep()` advances the face animation and is only called while the face is lit.
Set and alarm screens use plain defaults; supply `setDraw` / `ringDraw` (reading
`watch::setPage()` / `setField()`) to theme those too.

## Shared services (in WatchCore)

| Service | API |
|---------|-----|
| Buttons | `B_LEFT B_MID B_RIGHT B_MODE B_GAME` bits, `watch::held()`; alias to logical names per game |
| Clock + alarm | `watch::hours()` … above; set via the SET screens |
| Sound | `beep(freq, ms)`; `watch::playTune(notes, n)` / `tunePlaying()` / `stopTune()` for non-blocking jingles (`watch::Note{freq, ms}`, freq 0 = rest). Beeps are skipped while a tune or the alarm plays |
| Drawing | `plot`, `hLineSafe`, `vLineSafe`, `lineSafe`, `fillBoxClipped`, `frameSafe`, `discSafe`, `circleSafe`, `triSafe`, `fillTriSafe` |
| Display | global `u8g2` (SH1106, full buffer) |
| Pins | `watch::DEFAULT_PINS`; pass a different `watch::Pins` to `begin()` for new hardware |

## Drawing stack

- U8g2 full buffer, color 1 = lit pixel, 0 = clear (useful for eyes / cutouts).
- Always `setDrawColor(1)` after using 0.
- Small fonts that fit: `u8g2_font_6x10_tr` for HUD; larger fonts only on menus / splash.

## Game loop pattern (inside `GAME`)

```
gameStep(edges):
  handle pause / quit
  advance timers (invul, intro banners, animFrame)
  read held for continuous actions
  physics / scroll / spawn
  collisions → score / hurt / win
  clamp entities to screen or cull off-screen

drawGame():
  background / terrain
  entities (back to front)
  player (respect invul blink)
  HUD
  optional intro banner overlay
```

## Memory / performance budget

RP2040 has headroom, but keep watch games tiny:

- Prefer fixed arrays (`Obj objs[12]`) over dynamic allocation.
- One full 128×64 buffer is fine; avoid a second shadow buffer unless needed.
- Procedural sprites (discs, boxes, triangles) beat large bitmaps for this panel.

## Sonic firmware specifics

- Platformer: Left/Right run, Mid jump, double-tap forward/back to ground-roll.
- Campaign: Green Hill → Chemical Plant → Casino Night → Scrap Brain (2 acts each + Egg Wrecker).
- Levels are hand-designed in `sonic_levels.h`: per act a terrain profile (`{x, groundY}` points) and an object list (`{x, h above ground, kind, arg}`) with builders for ring rows/arcs, spike runs, platforms, the loop and the Chemical Plant wavy cross. Act 1 ≈ 5.5–5.9k px, Act 2 ≈ 2.3–2.5k px + boss.
- Terrain can have **cliffs** (two points 1 px apart): a rise is a wall to jump (≤ 30 px) or spring up (a spring flush against the face, at cliff x − 6 — miss the top and you land back on it), a drop is a ledge. Act 1s span ~3–5 screens of height.
- **Zoom:** the world is drawn 5/4 bigger than the level data (`Z()` in the sketch). `sonic_levels.h` and its rules stay in design units; positions, sizes, hitboxes, speeds and accelerations are all scaled ×1.25, so timing is unchanged.
- Physics follows Sonic 1 proportions: jump ≈ 38 design px (48 on screen, ~2.2× Sonic's height), yellow spring ≈ 62 (77 on screen), slope pull 0.125·sin, Sonic sticks to the ground over crests and down slopes, side contact with walls/spikes just stops him (no knock-back).
- Sprite art lives in `assets/sonic_sprite/sonic.txt` (20×22 ASCII frames); `python3 scripts/gen_sonic_sprite.py` regenerates `sonic_sprites.h`.
- Host tests (no watch needed): `tools/sonic_test/build.sh`, then `out/check` (level-data rules: slopes, cliffs, spike clearance, bridges, capacity) and `out/stops` (reports every place Sonic gets stopped with no wall, spike, enemy or real cliff nearby, via the sketch's `STOP_HOOK`) and `out/bot` (a bot plays the real game code through every act; `GOD=1` makes hazards harmless so it only tests "can you get through").
- **Egg Wrecker** (end of every Act 2): flat arena. The Egg Mobile flies in from the top right, lowers a checkered ball on a chain, then glides between the arena ends (pausing ~1.3 s to turn) while the ball swings as a true pendulum (~65°, rising at the ends). 8 spin hits; it flashes after each. On defeat it shudders and smokes, drops the ball and limps off to the right. Ship speed and swing rate rise a little each zone. The camera rises to keep Sonic in view above the ship.
- Launch heights are set against the jump (46 px): hurt ≈ 20, boss-top bounce ≈ 30, bumper ≈ 60, stomp = reverse of the fall (½–1 jump).
- Act end: passing the Act 1 signpost starts the fanfare; the camera settles with the sign in the middle while it spins to a stop, Sonic runs off to the right on his own, and "STAGE CLEAR" slides in at the top. Once he's off screen and the fanfare ends, the screen fades to the tally (rows slide in from the right; bonus count-up, speed rank). Act 2 tallies after the boss flies off.
- **Transitions:** no hard cuts. `transitionTo(action, white)` dither-fades the screen out (~0.47 s, game frozen), runs the action, and fades the new screen in. Used for start, death restart, game over, boss arena, tally, next act, level select; special stage in/out fades to white. Title cards slide in from the right, hold ~0.8 s and slide off to the left.
- Cliffs in the air: a ledge catches Sonic if his feet are within 12 px of its top (Genesis wall sensors sit mid-body), so jumping up a step isn't pixel-precise. On the ground a rise > 7 px is a wall.
- Speed rank vs each act's `parSec`: **A+** ≤ par + 5 s, **B** ≤ par + 12 s, else **D**. Par values are estimates — tune after playtesting.
- Rings reset each act (and on a restart), as in Sonic.
- After each act clear: chance of Special Stage (collect 6 rings, grab emerald).
- Zone deco (kind 5): sunflower/palm/pipe/neon/gear — decoration only.
- Chemical flame jets (8), Casino bumpers (9), Scrap saws (10).
- Bees: fly → pause → shoot → continue (every zone). The shot is a slow ball (~1.6 px/frame) aimed at Sonic, only fired while the bee is on screen, and pops on the ground — about 1–2 s of flight.
- Spin-roll (double-tap): holding forward adds no speed; once the roll slows below 0.5 px/frame Sonic stands back up and runs (Genesis rule), so an accidental double-tap never parks him in a ball.
- Object kinds: spike, ring, crab, spring, bee, deco, goal, wall, flame, bumper, saw.
