#!/bin/sh
# Host-side Metroid tests (no watch needed):
#   ./tools/metroid_test/build.sh && ./tools/metroid_test/out/check   # room data rules
#   ./tools/metroid_test/out/run "<script>" [prefix]                  # scripted play + screenshots
set -e
T=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$T/../.." && pwd); O="$T/out"
mkdir -p "$O"
arduino-cli compile --fqbn rp2040:rp2040:waveshare_rp2040_zero --libraries "$ROOT/libraries" \
  --preprocess "$ROOT/metroid_watch" > "$O/pp.cpp"
g++ -std=gnu++17 -O1 -w -include cstddef -I"$O" -I"$T/inc" -I"$ROOT/libraries/WatchCore/src" -I"$ROOT/metroid_watch" \
  -x c++ "$T/run.cpp" "$ROOT/libraries/WatchCore/src/WatchGfx.cpp" "$T/u8.cpp" -o "$O/run"
g++ -std=gnu++17 -O1 -I"$ROOT/metroid_watch" -I"$T/inc" "$T/check.cpp" -o "$O/check"
echo "built $O/run and $O/check"
