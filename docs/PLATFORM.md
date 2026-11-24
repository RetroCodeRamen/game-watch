# Platform overview

One-page map of the Game Watch stack. Read this first; drill into the other docs as needed.

```
┌─────────────────────────────────────────┐
│              128×64 SH1106 OLED         │
│            I2C  GP4 (SDA) / GP5 (SCL)   │
└─────────────────────────────────────────┘
          ▲ U8g2 full-frame ~30 FPS
┌─────────────────────────────────────────┐
│           Waveshare RP2040-Zero         │
│     Earle Philhower Arduino core        │
├─────────────┬─────────────┬─────────────┤
│  Buttons    │   Buzzer    │   USB       │
│  GP2/3/6/7/8│   GP15      │  flash/CDC  │
└─────────────┴─────────────┴─────────────┘
```

## Firmware layers

1. **Hardware glue** — pins, Wire, U8g2, `tone`
2. **Input** — debounce, edge + held masks
3. **Watch services** — clock, alarm, set screens
4. **Shell UI** — splash, view, menus, mode router
5. **Game module** — start / step / draw / over / win

Layers 1–4 are **`libraries/WatchCore`**, shared by every title. Layer 5 — plus each watch's themed clock-face art — lives in the sketch.

## Titles

| Sketch | Genre | Notes |
|--------|-------|-------|
| `sonic_watch/` | Platformer | 4 zones × 2 acts, loops, springs, Egg Wrecker bosses, Special Stages |
| `hellgate_watch/` | First-person demon shooter | Raycaster engine + test room: doors, keycard, switch, 3 monsters, 2 guns |
| `metroid_watch/` | Exploration adventure | 4 zones, 29 rooms, ability gates, Kraid / Ridley / Mother Brain, escape, saves + suspend |

Flash either sketch onto the same hardware: `./scripts/flash.sh sonic_watch` or `./scripts/flash.sh metroid_watch`. See [ADDING_A_GAME.md](ADDING_A_GAME.md).
