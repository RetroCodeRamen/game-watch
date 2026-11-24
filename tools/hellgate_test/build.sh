#!/bin/sh
# Host-side Hellgate tests (no watch needed):
#   ./tools/hellgate_test/build.sh && ./tools/hellgate_test/out/check   # maps closed, art well-formed, doors sane
#   ./tools/hellgate_test/out/run "<script>" [prefix]                 # scripted play + screenshots (.pbm)
set -e
T=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$T/../.." && pwd); O="$T/out"
mkdir -p "$O"
CLIB=$("$T/../u8g2_host_lib.sh" "$O" | tail -1)
arduino-cli compile --fqbn rp2040:rp2040:waveshare_rp2040_zero --libraries "$ROOT/libraries" \
  --preprocess "$ROOT/hellgate_watch" > "$O/pp.cpp"
for p in run check; do
  g++ -std=gnu++17 -O2 -w -include cstddef -I"$O" -I"$T/inc" -I"$ROOT/libraries/WatchCore/src" -I"$ROOT/hellgate_watch" \
    -I"$CLIB" -x c++ "$T/$p.cpp" "$ROOT/libraries/WatchCore/src/WatchGfx.cpp" "$T/u8.cpp" -x none "$O/u8g2/libu8g2.a" -o "$O/$p"
done
echo "built $O/run and $O/check"
