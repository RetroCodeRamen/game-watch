# Hardware

## MCU

- **Board:** Waveshare RP2040-Zero
- **Chip:** Raspberry Pi RP2040 (dual Cortex-M0+, 133 MHz)
- **Flash:** 2 MB onboard
- **Arduino core:** Earle Philhower `rp2040:rp2040`
- **FQBN:** `rp2040:rp2040:waveshare_rp2040_zero`

USB enumerates as a Raspberry Pi Pico-class device (`ttyACM0`). Upload uses UF2 (auto reset into bootloader).

## Display

| Item | Value |
|------|-------|
| Controller | SH1106 |
| Resolution | 128 × 64, 1-bit monochrome |
| Bus | I2C |
| SDA | **GP4** |
| SCL | **GP5** |
| Library | U8g2 — `U8G2_SH1106_128X64_NONAME_F_HW_I2C` |
| I2C clock | 800 kHz (`u8g2.setBusClock` after `begin`; at 400 kHz sending the screen took ~23 ms of the 33 ms frame) |
| Contrast (current FW) | `u8g2.setContrast(60)` |

Full-frame buffer (`_F_`) is used: clear → draw → `sendBuffer()` each frame (~30 FPS).

Design notes for games:

- Prefer filled shapes / 1–2 px strokes; thin single pixels vanish on this panel.
- Keep interactive sprites roughly **6–12 px** tall.
- HUD should stay in the top ~10 px or bottom strip so it does not collide with playfield.

## Buttons

Active-low with internal pull-ups. Debounced in software (~20 ms).

| Name | Pin | Firmware bit | Typical use |
|------|-----|--------------|-------------|
| Left | **GP2** | `B_LEFT` (1) | Adjust / move left / slow |
| Mid | **GP3** | `B_MID` (2) | Confirm / jump / primary action |
| Right | **GP6** | `B_RIGHT` (4) | Adjust / move right / secondary hold |
| Mode | **GP7** | `B_MODE` (8) | System / back to clock / settings |
| Game | **GP8** | `B_GAME` (16) | Enter game / pause / quit |

`held` tracks current presses; `readButtons()` returns **edge** bits (press events). Games that need “hold to charge” should also read `held`.

## Audio

| Item | Value |
|------|-------|
| Device | Piezo / buzzer |
| Pin | **GP15** |
| API | Arduino `tone(P_BUZ, freq, ms)` / `noTone(P_BUZ)` |

Keep beeps short (20–150 ms) so they do not stall the frame loop. The alarm mode may hold a continuous tone while ringing.

## Power / form factor

Treat this as a **watch-sized** device:

- One screen composition; no multi-panel UI.
- Avoid long blocking delays in `loop()`.
- Clock time is millis-based in the current firmware (resets on power loss) — not RTC-backed unless you add hardware later.
  `scripts/flash.sh` sets the clock over USB right after uploading; `scripts/settime.sh` does it any time the
  watch is plugged in (it sends `T <seconds since midnight> <month> <day>` on the USB serial port; the watch
  answers `OK`). Test hooks on the same port: `P <buttons> <ms>` holds buttons (bit mask: 1 Left, 2 Mid, 4 Right,
  8 Mode, 16 Game), `F` reports `work_us` (last frame) and `max_us` (worst since the last `F`).
- Measured on the watch (Metroid test room): ~19 ms per frame at 800 kHz, ~33 ms at 400 kHz (the 30 FPS
  budget is 33 ms).

## Pin summary

```
GP2  LEFT button
GP3  MID button
GP4  OLED SDA
GP5  OLED SCL
GP6  RIGHT button
GP7  MODE button
GP8  GAME button
GP15 BUZZER
```

Do not reassign these casually; every game firmware should keep the same mapping so hardware stays interchangeable.
