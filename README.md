# Game Watch

Handheld watch + game console built around a **Waveshare RP2040-Zero**, a **128×64 SH1106 OLED**, five buttons, and a piezo buzzer.

This repo holds the shared platform docs and two firmwares for the same hardware: **Sonic Watch** (auto-runner) and **Metroid Watch** (Zebes side-view adventure). Flash one sketch per watch.

## Quick start

```bash
# Flash Sonic (device on USB /dev/ttyACM0)
./scripts/flash.sh sonic_watch

# Flash Metroid on the second watch
./scripts/flash.sh metroid_watch
```

Requirements: [`arduino-cli`](https://arduino.github.io/arduino-cli/), Earle Philhower RP2040 core, U8g2 library. See [docs/FLASH.md](docs/FLASH.md).

## Repo layout

| Path | Purpose |
|------|---------|
| `docs/` | Hardware, software architecture, flash guide, how to add games |
| `libraries/WatchCore/` | Shared watch shell: buttons, clock, alarm, set screens, power save, clip-safe drawing |
| `scripts/flash.sh` | Compile + upload helper (passes `--libraries libraries`) |
| `sonic_watch/` | Clock + Sonic runner |
| `metroid_watch/` | Clock + Metroid adventure (design doc included) |

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

## Controls (Metroid firmware — Design Rev 4)

Logical inputs are **Left / Right / Up / Down / Menu**. On the current 5-button board:

| Logical | Pin today | Clock | Game |
|---------|-----------|-------|------|
| Left | GP2 | Adjust − | Move left |
| Up | GP3 (Mid) | Next field | Jump |
| Right | GP6 | Adjust + | Move right |
| Down | GP7 (Mode) | Set menus / confirm | Fire / bomb / confirm |
| Menu | GP8 (Game) | Open title | Tap = Gun→Missiles→Morph; Hold ≈350 ms = Start menu |

Start menu: **Items** · **Map** · **Mission** (Save & Clock). Pin map is isolated at the top of `metroid_watch.ino` for the future button layout.

**Metroid campaign:** 36 rooms, Morph Ball, missiles, bombs, Screw Attack, Ice Missiles, two Energy Tanks; both boss branches → Tourian → Mother Brain escape.
