#!/bin/sh
# Host-side Metroid tests (no watch needed):
#   ./tools/metroid_test/build.sh && ./tools/metroid_test/out/check   # room data rules
#   ./tools/metroid_test/out/run "<script>" [prefix]                  # scripted play + screenshots
set -e
T=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$T/../.." && pwd); O="$T/out"
mkdir -p "$O"
# the real U8g2 C library (for text), built once; only the fonts the watch uses
CLIB="${U8G2_CLIB:-$HOME/Arduino/libraries/U8g2/src/clib}"
if [ ! -f "$O/u8g2/libu8g2.a" ]; then
  mkdir -p "$O/u8g2"
  python3 "$T/cut_fonts.py" "$CLIB" "$O/u8g2/fonts.c"
  for f in "$CLIB"/*.c "$O/u8g2/fonts.c"; do
    case "$(basename "$f")" in u8g2_fonts.c|u8x8_fonts.c) continue;; esac
    gcc -O1 -w -I"$CLIB" -c "$f" -o "$O/u8g2/$(basename "$f" .c).o"
  done
  ar rcs "$O/u8g2/libu8g2.a" "$O/u8g2"/*.o
fi
arduino-cli compile --fqbn rp2040:rp2040:waveshare_rp2040_zero --libraries "$ROOT/libraries" \
  --preprocess "$ROOT/metroid_watch" > "$O/pp.cpp"
g++ -std=gnu++17 -O1 -w -include cstddef -I"$O" -I"$T/inc" -I"$ROOT/libraries/WatchCore/src" -I"$ROOT/metroid_watch" \
  -I"$CLIB" -x c++ "$T/run.cpp" "$ROOT/libraries/WatchCore/src/WatchGfx.cpp" "$T/u8.cpp" -x none "$O/u8g2/libu8g2.a" -o "$O/run"
g++ -std=gnu++17 -O1 -I"$ROOT/metroid_watch" -I"$T/inc" -I"$CLIB" "$T/check.cpp" "$O/u8g2/libu8g2.a" -o "$O/check"
echo "built $O/run and $O/check"
