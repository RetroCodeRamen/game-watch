# Build & flash

## One-time setup

```bash
# arduino-cli (if missing)
# https://arduino.github.io/arduino-cli/latest/installation/

arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json

arduino-cli core update-index
arduino-cli core install rp2040:rp2040
arduino-cli lib install "U8g2"
```

Your user must be in the `dialout` group to use `/dev/ttyACM0`.

## Board

| Setting | Value |
|---------|-------|
| FQBN | `rp2040:rp2040:waveshare_rp2040_zero` |
| Port (typical) | `/dev/ttyACM0` |
| USB id | `2e8a:000a` Raspberry Pi Pico |

Confirm:

```bash
arduino-cli board list
lsusb | grep -i pico
```

## Flash helper

From the repo root:

```bash
./scripts/flash.sh                  # defaults to sonic_watch
./scripts/flash.sh sonic_watch
./scripts/flash.sh games/my_game    # future sketches
PORT=/dev/ttyACM1 ./scripts/flash.sh
```

The script compiles then uploads over USB (UF2).

## Manual commands

```bash
FQBN=rp2040:rp2040:waveshare_rp2040_zero
SKETCH=sonic_watch

arduino-cli compile --fqbn "$FQBN" "$SKETCH"
arduino-cli upload -p /dev/ttyACM0 --fqbn "$FQBN" "$SKETCH"
```

## Bootloader recovery

If upload fails:

1. Unplug USB.
2. Hold **BOOT** on the RP2040-Zero, plug USB back in, release BOOT.
3. A `RPI-RP2` mass-storage volume appears — rerun upload, or copy the `.uf2` onto it.

## Verify after flash

1. Splash → `SONIC` (or your title) then clock.
2. Game button starts the game.
3. Mode returns to clock.
4. Jump / roll respond; OLED not garbled (if garbled, check I2C wiring GP4/GP5).
