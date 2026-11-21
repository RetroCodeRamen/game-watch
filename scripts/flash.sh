#!/usr/bin/env bash
# Compile and upload a Game Watch sketch to the RP2040-Zero.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FQBN="${FQBN:-rp2040:rp2040:waveshare_rp2040_zero}"
PORT="${PORT:-/dev/ttyACM0}"
SKETCH="${1:-sonic_watch}"
LIBS="$ROOT/libraries"   # shared WatchCore lives here

# Allow "sonic_watch" or a path relative to repo root
if [[ "$SKETCH" != /* ]]; then
  SKETCH="$ROOT/$SKETCH"
fi

if [[ ! -d "$SKETCH" ]]; then
  echo "Sketch folder not found: $SKETCH" >&2
  exit 1
fi

NAME="$(basename "$SKETCH")"
if [[ ! -f "$SKETCH/$NAME.ino" ]]; then
  echo "Expected $SKETCH/$NAME.ino" >&2
  exit 1
fi

if [[ ! -e "$PORT" ]]; then
  echo "Serial port missing: $PORT" >&2
  echo "Plug in the watch USB and check: arduino-cli board list" >&2
  exit 1
fi

echo "==> Compiling $NAME ($FQBN)"
arduino-cli compile --fqbn "$FQBN" --libraries "$LIBS" "$SKETCH"

echo "==> Uploading to $PORT"
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"

# start on the right time (the clock isn't battery-backed)
"$ROOT/scripts/settime.sh" || echo "(couldn't set the clock; run scripts/settime.sh)"

echo "==> Done"
