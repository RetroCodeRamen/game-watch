# Game Watch

Handheld watch + game console built around a **Waveshare RP2040-Zero**, a **128×64 SH1106 OLED**, five buttons, and a piezo buzzer.

This repo holds the shared platform docs and two firmwares for the same hardware: **Sonic Watch** (side-scrolling platformer) and **Metroid Watch** (Zebes exploration adventure). Flash one sketch per watch.

## Quick start

```bash
# Flash Sonic (device on USB /dev/ttyACM0)
./scripts/flash.sh sonic_watch

# Flash Metroid on the second watch
./scripts/flash.sh metroid_watch
```

Flashing also sets the watch's clock from the computer over USB; `./scripts/settime.sh` resets it any time the
watch is plugged in. On the watch, the set screens (Mode on the clock) are time, date, alarm and SOUND on/off
(off silences beeps and game music; the alarm still rings).

Requirements: [`arduino-cli`](https://arduino.github.io/arduino-cli/), Earle Philhower RP2040 core, U8g2 library. See [docs/FLASH.md](docs/FLASH.md).

## Repo layout

| Path | Purpose |
|------|---------|
| `docs/` | Hardware, software architecture, flash guide, how to add games |
| `libraries/WatchCore/` | Shared watch shell: buttons, clock, alarm, set screens, power save, clip-safe drawing |
| `scripts/flash.sh` | Compile + upload helper (passes `--libraries libraries`) |
| `sonic_watch/` | Clock + Sonic platformer |
| `metroid_watch/` | Clock + Metroid adventure (design doc included) |
| `tools/` | Host tests: `metroid_test` (room checker + scripted runner), `sonic_test` (act bot) |

## Docs map

- [Platform overview](docs/PLATFORM.md) — one-page stack map
- [Hardware](docs/HARDWARE.md) — pins, display, buttons, audio
- [Software design](docs/SOFTWARE.md) — modes, loop timing, shared services
- [Adding a game](docs/ADDING_A_GAME.md) — constraints and recommended structure
- [Flash / build](docs/FLASH.md) — install tools and upload

## Controls (Sonic firmware)

| Button | Pin | Clock | Game |
|--------|-----|-------|------|
| Left | GP2 | Adjust − | Run left (double-tap = spin-roll; tap L/R again to stand) |
| Mid | GP3 | Next field / confirm | Jump (keeps spin momentum) |
| Right | GP6 | Adjust + | Run right (double-tap = spin-roll; tap L/R again to stand) |
| Mode | GP7 | Enter set menus | Quit to clock |
| Game | GP8 | Start game | Pause menu |

Spin-roll ends on a Left/Right tap or hitting a wall — not on jump.

**Sonic campaign:** 4 zones × 2 acts (long Act 1, short Act 2 + Egg Wrecker). Chance of a Special Stage emerald mini-game after clearing an act.

| Zone | Decoration | Signature gameplay |
|------|-------------|--------------------|
| Green Hill | Sunflowers, palm trees | Springs, platforms, crabs |
| Chemical Plant | Pipes | Flame jets, spikes, traps |
| Casino Night | Neon signs | Pinball bumpers + bounce chains |
| Scrap Brain | Gears | Saws, industrial hazards |

## Controls (Metroid firmware)

Five buttons today (a sixth is planned: it becomes Down, and Mode goes back to cancel/exit).

| Button | Clock | Game |
|--------|-------|------|
| Left / Right | Adjust | Move (hold to run; a running jump flips) |
| Mid (Action) | Next field | Jump, again in mid-air to double jump; stand up from the ball |
| Mode | Set menus (time, date, alarm, sound) | Down: crouch, again to roll into the Morph Ball; save on a save pad |
| Game (Select/OK) | Start the game | Fire; in the ball, drop a bomb |
| Mode + Game together | — | Tap: switch beam/missiles · Hold: pause (MAP / ITEMS / GOAL, RESUME / CLOCK) |

**Metroid campaign:** Brinstar, Kraid's Lair, Norfair and Tourian (29 rooms). Morph Ball, Missiles and Bombs in
Chozo shrines; Kraid gives Space Jump, Ridley gives Ice Missiles; 8 missile tanks and 2 Energy Tanks. Beat both
bosses to open Tourian, destroy Mother Brain, and escape to the ship before the timer runs out. Save stations,
suspend to the clock (CONTINUE resumes in place), an easy mode and a next-goal hint in the pause menu. A hidden test
room (hold Left + Right on the title) has every item, enemy and boss.
