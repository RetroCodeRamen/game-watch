#!/bin/sh
# Rebuild the README screenshots in docs/img from the real game code (host simulators, no watch needed).
#   ./tools/readme_shots.sh
# Each shot is a short button script (see tools/metroid_test/run.cpp and tools/sonic_test/shots.cpp),
# rendered to look like the watch's OLED by tools/oled_png.py. The clock reads 10:10 on Dec 25.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd); IMG="$ROOT/docs/img"; W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
"$ROOT/tools/sonic_test/build.sh" >/dev/null; "$ROOT/tools/metroid_test/build.sh" >/dev/null
S="$ROOT/tools/sonic_test/out/shots"; R="$ROOT/tools/metroid_test/out/run"; P="python3 $ROOT/tools/oled_png.py"
mkdir -p "$IMG"
shot() { # name, runner, script, which frame
  rm -f "$W"/x_*.pbm; "$2" "$3" "$W/x" >/dev/null; $P "$IMG/$1.png" "$W/x_$(printf %03d "$4").pbm"; echo "  $1.png"; }

echo "Sonic Watch"
shot sonic_clock   "$S" "face:40" 0
shot sonic_title   "$S" "title 30:. shot" 0
shot sonic_green_hill "$S" "act:0:0 40:. run:2600 30:. shot" 0
shot sonic_chemical   "$S" "act:1:0 40:. run:900 30:. shot" 0
shot sonic_casino     "$S" "act:2:0 40:. run:900 25:. shot" 0
shot sonic_scrap      "$S" "act:3:0 40:. run:2500 25:. shot" 0
shot sonic_boss    "$S" "act:0:1 toboss 50:. shot" 0
shot sonic_special "$S" "act:0:0 rings:20 special 90:. shot" 0

echo "Metroid Watch"
T="20:. 1:F 20:. 1:D 40:. 1:D 40:."          # title -> story -> landing
shot metroid_clock  "$R" "face:40" 0
shot metroid_title  "$R" "40:. shot" 0
shot metroid_story  "$R" "20:. 1:F 150:. shot" 0
shot metroid_landing "$R" "$T 90:. shot" 0
shot metroid_shrine "$R" "$T warp:3:5:18 10:. 12:R 20:. shot" 0
shot metroid_kraid  "$R" "$T gear:31 god warp:14:3:9 90:. 1:R 1:. 1:F 8:. shot" 0
shot metroid_ridley "$R" "$T gear:31 god warp:22:2:7 90:. shot" 0
shot metroid_brain  "$R" "$T gear:31 god beat:3 warp:28:3:9 90:. 1:R 1:. 1:F 6:. shot" 0
shot metroid_map    "$R" "$T gear:7 visit:6F playtime:2537 warp:2:5:20 90:. 25:D 10:. shot" 0
shot metroid_items  "$R" "$T gear:7 visit:6F playtime:2537 warp:2:5:20 90:. 25:D 10:. 1:R 10:. shot" 0
shot metroid_goal   "$R" "$T gear:7 visit:6F playtime:2537 warp:2:5:20 90:. 25:D 10:. 1:R 2:. 1:R 10:. shot" 0
