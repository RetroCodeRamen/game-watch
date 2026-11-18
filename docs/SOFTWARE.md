# Software design

Target pattern for all Game Watch firmwares. The Sonic sketch (`sonic_watch/`) is the reference implementation.

## Runtime model

```
setup()
  pinMode pull-ups, Wire I2C, u8g2.begin()

loop()  (as fast as possible for buttons)
  pending |= readButtons()
  if 1s elapsed → tick clock
  if ≥33 ms since last frame (~30 FPS):
      update(pending edges)
      clearBuffer → render → sendBuffer
```

**Rules**

1. Sample buttons every spin of `loop()`; only consume edges on the frame tick.
2. Cap logic + draw to ~**30 FPS** so I2C OLED traffic stays stable.
3. Never block for more than a few ms (no `delay()` in the game path).

## Mode machine

```
SPLASH → VIEW ⇄ SETM
           ↕
         RING (alarm)
           ↕
         GAME ⇄ MENU
           ↓
       OVER / WIN → VIEW or restart
```

| Mode | Role |
|------|------|
| `SPLASH` | Boot title |
| `VIEW` | Clock face (always recoverable) |
| `SETM` | Set time / date / alarm |
| `RING` | Alarm ringing |
| `GAME` | Active game |
| `MENU` | In-game pause |
| `OVER` / `WIN` | End screens |

**Platform convention:** `MODE` returns to the clock from gameplay when possible. `GAME` starts or pauses the game from the clock / run.

## Shared services (keep these)

These belong in every firmware, even if the game changes:

| Service | Responsibility |
|---------|----------------|
| Button debounce | `readButtons()`, `held`, edge bits |
| Clock | `tod`, `mon`, `dday`, `tick()`, alarm |
| Display helpers | `two()`, `monthName()`, fonts |
| Sound | `beep(freq, ms)` |
| Frame router | `update(e)` + `render()` by `mode` |

Games plug into `GAME` / `MENU` / `OVER` / `WIN` (and optionally splash art). Leave clock + settings intact so the device stays a watch.

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
