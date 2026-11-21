#!/bin/sh
# Build the host-side Sonic tests (no watch needed):
#   ./tools/sonic_test/build.sh && ./tools/sonic_test/out/check     # level-data rules
#   ./tools/sonic_test/out/bot              # a bot plays every act (hold right, jump when stuck)
#   GOD=1 ./tools/sonic_test/out/bot        # hazards can't hurt: tests pure "can you get through"
#   ./tools/sonic_test/out/stops            # report stops with no visible cause
#   ./tools/sonic_test/out/shots "<script>" # screenshots (see shots.cpp)
#   STALL=1 / PROF=1 / TRACE=1 TX=<x> TT=<frames> ./tools/sonic_test/out/bot <zone> <act>
# The bot compiles the real sonic_watch.ino (via arduino-cli --preprocess) against stub
# Arduino/U8g2/WatchCore headers, so it runs the actual game code.
set -e
T=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$T/../.." && pwd); O="$T/out"
mkdir -p "$O"
CLIB=$("$T/../u8g2_host_lib.sh" "$O" | tail -1)   # real U8g2 text in screenshots
arduino-cli compile --fqbn rp2040:rp2040:waveshare_rp2040_zero --libraries "$ROOT/libraries" \
  --preprocess "$ROOT/sonic_watch" > "$O/pp.cpp"
g++ -std=gnu++17 -O1 -w -I"$O" -I"$T/inc" -I"$ROOT/libraries/WatchCore/src" -I"$ROOT/sonic_watch" \
  -I"$CLIB" -x c++ "$T/bot.cpp" "$ROOT/libraries/WatchCore/src/WatchGfx.cpp" "$T/u8.cpp" -x none "$O/u8g2/libu8g2.a" -o "$O/bot"
g++ -std=gnu++17 -O1 -w -I"$O" -I"$T" -I"$T/inc" -I"$ROOT/libraries/WatchCore/src" -I"$ROOT/sonic_watch" \
  -I"$CLIB" -x c++ "$T/stops.cpp" "$ROOT/libraries/WatchCore/src/WatchGfx.cpp" "$T/u8.cpp" -x none "$O/u8g2/libu8g2.a" -o "$O/stops"
g++ -std=gnu++17 -O1 -w -I"$O" -I"$T/inc" -I"$ROOT/libraries/WatchCore/src" -I"$ROOT/sonic_watch" \
  -I"$CLIB" -x c++ "$T/shots.cpp" "$ROOT/libraries/WatchCore/src/WatchGfx.cpp" "$T/u8.cpp" -x none "$O/u8g2/libu8g2.a" -o "$O/shots"
g++ -O1 -I"$ROOT/sonic_watch" -I"$T/inc" -I"$CLIB" "$T/check.cpp" "$O/u8g2/libu8g2.a" -o "$O/check"
echo "built $O/bot, $O/stops, $O/shots and $O/check"
