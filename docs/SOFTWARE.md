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
- Finish an act holding **20+ rings** → Special Stage. Fewer → straight on.
- **Special Stage (half-pipe):** Sonic runs into the screen down a twisting pseudo-3D half-pipe; Left/Right slide him around the wall (quick, with a gentle pull back to the bottom); Mid jumps off the wall with his running speed and flies under screen gravity like a ramp, landing wherever the arc meets the pipe. Rings and bombs come down the pipe (patterns: lines, slaloms, sweeps, bomb-beside-rings, bomb pairs to jump, bomb gates). Three legs, each ending at a checkpoint hoop with a quota ("GET n RINGS!"). Each of the 4 stages has its own row in `SP_STAGES` (quotas per leg, bombs, speed): **1** rings only, quotas 8/10/12 — hard to lose; **2** introduces bombs, 1–2 per leg; **3** the full mix (~6 bombs/leg); **4** (the ultimate-power emerald) faster with higher quotas. Rings carry over and the quotas add up (Sonic 2 style). A bomb knocks 3 rings loose (never below 0) and never ends the run; **the only loss is reaching a checkpoint short of its quota**. Clear all three and the Chaos Emerald comes down the pipe, then the emerald screen (4 slots). **4 emeralds exist, one per zone; 3 is the goal**, the 4th is the spare (later: Super Sonic). Course speed rises a little per emerald held.
- Spike sides and walls are solid every 60 Hz physics step and push Sonic back out the side he came from, so he can't sink into them at speed. Bees only fire while fully on screen (both axes).
- **Green Hill 1 is the teaching act:** no hazard within 80 px of a cliff top, a drop's landing or a spring route's landing (`out/check` enforces this for GH1 and counts it elsewhere); the first spikes have a ring arc showing the jump.
- Zone deco (kind 5): sunflower/palm/pipe/neon/gear — decoration only.
- Chemical flame jets (8), Casino bumpers (9), Scrap saws (10).
- Bees: fly → pause → shoot → continue (every zone). The shot is a slow ball (~1.6 px/frame) aimed at Sonic, only fired while the bee is on screen, and pops on the ground — about 1–2 s of flight.
- Spin-roll (double-tap): holding forward adds no speed; once the roll slows below 0.5 px/frame Sonic stands back up and runs (Genesis rule), so an accidental double-tap never parks him in a ball.
- Object kinds: spike, ring, crab, spring, bee, deco, goal, wall, flame, bumper, saw.

## Metroid firmware specifics

- **Buttons (prototype, 5):** Left/Right move · Action = Up (jump; from the ball: stand, or crouch if
  that's all the room) · Mode = Down, acting on press: stand → crouch → Morph Ball (save on the ship pad).
  Crouching fires low (floor enemies); a direction stands her up and runs, Action jumps from the crouch · Select/OK = fire (bombs in the ball) · **Down + Select/OK** = "mode":
  tap switches beam/missiles (and undoes the crouch/morph Down just made), hold ~0.4 s opens the pause screen (map, status, Resume / Clock).
  When the 6th button is fitted it becomes Down and Mode goes back to cancel/exit — the aliases at the top
  of `metroid_watch.ino` are the only place to change.
- **World:** `metroid_world.h` holds ASCII tile rooms (8 px tiles, 16×7 per screen) placed on one map grid.
  Rooms touch edge to edge, so leaving a room's edge puts Samus in whichever room is there (fade, camera
  snaps to the new room's bounds). Doors are 3-tile columns on room edges: blue opens to any shot, red needs a
  missile and then stays open. Tile/object legend is at the top of the file.
- **Current build:** Brinstar B01–B06: ship → surface → Brinstar Shaft (1×4) → Morph Ball shrine (1×3, one-way
  drop) → ball tunnel back into the shaft → Missile Room (2×2) → Junction (2×2), with a climbable way home from every
  room. The junction's red doors lead to Kraid / Norfair, not built yet ("NEXT BUILD").
- **Items:** each kind has its own 8×8 icon (`ICON_*`). Major items (Morph Ball, Missiles) sit in a seated
  Chozo statue's hands inside an orb that must be shot open; taking one flashes the screen, pulls rings of
  light in to Samus, plays the long fanfare and opens a panel with the icon at 3×. Energy tanks get the panel
  only; missile tanks just a toast and a jingle. Map tile letter case picks the statue's side (`m` statue left, `M` right).
  Design rules (see the design doc): each major item is the key to its own escape, and Samus can always get back to
  the ship.
- **Sprites:** Samus comes from the Metroid II (Game Boy) sheet in `metroid_watch/`, converted by `python3 scripts/gen_metroid_sprites.py` (writes `metroid_gb_sprites.h`; `--preview` writes `assets/metroid_gb_preview.png`): stand, 3-frame run, knees-up jump (turned in quarter steps for the running somersault); the 7 px morph ball is hand-drawn in the script. `assets/metroid_sprite_compare.png` is the GB vs SNES comparison.
- **Physics (60 Hz):** run 1.5 px/step; jump peak ≈ 45 px (a 4-tile ledge with room to spare), double jump in
  mid-air ≈ 28 px more (always somersaults); release to cut the jump, jump buffer and ledge grace; running jumps
  somersault; hitbox 8×21 standing, 6×7 ball (fits a 1-tile duct).
- **Bombs:** in the ball, Select/OK drops a bomb (up to 3 live, ~0.6 s fuse). The blast (11 px) breaks cracked
  rock (`B`, re-forms when you re-enter the room), hurts enemies (2), and pops the ball ~23 px if it's caught in
  it: a bomb jump reaches a ball hole 2 tiles up. Bombs dropped in mid-air stay where they're dropped.
- **Space Jump:** while somersaulting, Up again once the last jump has nearly topped out gives another
  (unlimited); a ring of sparks turns round the flip. The flip hurts enemies it touches (4 damage, including the
  one it lands on) and Samus takes no contact damage while flipping. It breaks slashed rock (`X`, re-forms on
  room entry). Firing ends the flip.
- **Ice Missiles:** with Ice on, a missile freezes an enemy for ~5 s instead of hurting it (it shivers for the
  last 1.5 s). Frozen enemies are harmless and solid (stand on them; one frozen against Samus doesn't trap her);
  any missile shatters one, the beam glances off. Ice missiles still open red doors. HUD label reads ICE.
- **Pause:** Left/Right switch MAP / ITEMS tabs, Action moves the cursor, OK picks, Mode resumes. ITEMS lists owned
  gear with ON/OFF (Ice under Missiles, Bombs under Morph Ball show "--" while the parent is off); the settings
  (`gearOff`) are saved (save version 3). Turning the Morph Ball off inside a tunnel says NEED ROOM.
- **Chozo statue:** `SPR_CHOZO`, 38×40, traced from NES-style Chozo pixel art (highlight lines + outline); the
  orb (radius 7) sits in its raised hand 24 px above the floor.
- **Enemies (beyond Zoomer, Skree, Ripper):** every weapon hit goes through `damageEnemy`.
  - Turret (`u`): bolted to the wall beside it; flashes its barrel, then fires along its lane at shoulder height.
  - Swipe Pirate (`p`, 23 px): patrols, turns ledges; when Samus is close it raises its claw (tell), swipes in
    front (2 damage), then rests. 6 hp.
  - Bolt Pirate (`q`): guards its spot facing Samus; reaches out with a blinking muzzle (tell), fires one slow bolt
    at a time, high enough to crouch under and low enough to jump. 6 hp.
  - Metroid (`n`): drifts toward Samus, pulses (tell), dives; on contact it latches onto her head and drains 1
    energy every ~0.8 s. Only a bomb blast knocks it off. Beam, missiles and the flip don't hurt it (missiles
    shove it); Ice freezes it, then any missile shatters it.
- **Test room:** hold Left + Right on the title for ~1.5 s. A hidden 4×3 room (`SB`, zone `ZONE_TEST`, start
  `@`; its red door leads to `SB2`, the enemy test room) with every item (Morph Ball, Missiles, Ice, Bombs, Space Jump), 10 energy and 30 missiles; nothing is saved, dying restarts it, and it never shows on the
  map. It has Bombs and Space Jump orbs, a cracked-rock plug, a pit under a cracked lid, a bomb-jump wall, and an
  enemy arena with a slashed-rock wall and a high ledge.
- **Combat:** short Power Beam (~64 px), missiles (ammo, faster, 4 damage). Zoomers crawl round any surface,
  Skrees dive and burst into shards, Rippers are beam-proof. Energy starts at 6 (contact = 1), knock-back
  plus ~1.5 s recovery; zero energy → explosion → back at the last save.
- **Saving:** the ship's pad (Down) refills and writes the save to flash (`EEPROM`, checksummed). Title shows
  Continue when a save exists. New game shows the three-panel opening (Down skips).
- Host tests: `tools/metroid_test/build.sh`, then `out/check` (room sizes, overlaps, openings line up,
  door shape, no 2-tile pinch points, Morph Ball reachable standing, the shrine only left by rolling, and the
  ship reachable again from everywhere at each gear stage — climbs up to 4 tiles, 7 with the double jump) and `out/run "<script>"` (plays the real sketch with scripted buttons, dumps screenshots;
  see the header of `run.cpp`).
