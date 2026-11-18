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
| Sound | `beep(freq, ms)` (muted while the alarm rings) |
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
- After each act clear: chance of Special Stage (collect 6 rings, grab emerald).
- Zone deco (kind 5): sunflower/palm/pipe/neon/gear — decoration only.
- Chemical flame jets (8), Casino bumpers (9), Scrap saws (10).
- Bees: fly → pause → shoot (Casino+) → continue.
- Object kinds: spike, ring, crab, spring, bee, deco, goal, wall, flame, bumper, saw.
