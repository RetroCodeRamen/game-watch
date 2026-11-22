#!/bin/sh
# Rebuild the README demo GIFs in docs/img from the real game code (host simulators, no watch needed).
#   ./tools/readme_gifs.sh
# Each reel is a few scripted runs recorded at 15 fps (every 2nd frame, real-time playback):
# clock face -> title -> gameplay -> highlights. Frames are drawn to look like the OLED by tools/oled_gif.py.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd); IMG="$ROOT/docs/img"; W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
"$ROOT/tools/sonic_test/build.sh" >/dev/null; "$ROOT/tools/metroid_test/build.sh" >/dev/null
S="$ROOT/tools/sonic_test/out/shots"; R="$ROOT/tools/metroid_test/out/run"; G="python3 $ROOT/tools/oled_gif.py"
mkdir -p "$IMG" "$W/a" "$W/b" "$W/c" "$W/d" "$W/ma" "$W/mb"

echo "Sonic Watch"
$S "rec:2:2000 face:200 title 45:. 1:U 40:. run:3000" "$W/a/f" >/dev/null   # clock, title, Green Hill 1
$S "act:1:0 40:. rec:2:80 run:5000" "$W/b/f" >/dev/null                     # Chemical Plant 1
$S "act:0:0 rings:50 rec:2:130 spbot:1:1" "$W/c/f" >/dev/null               # Special Stage
$S "act:0:1 toboss 30:. rec:2:180 fight" "$W/d/f" >/dev/null                # Egg Wrecker
$G "$IMG/sonic_demo.gif" 66 "$W"/a/*.pbm "$W"/b/*.pbm "$W"/c/*.pbm "$W"/d/*.pbm

echo "Metroid Watch"
P="20:R 1:RF 6:R 1:RF 6:R 14:RU 10:R 1:RF 6:R 1:RF 20:R 14:RU 8:R 1:F 6:. 1:F 20:R 14:RU 20:R"
$R "every:2 face:200 40:. 1:F 100:. 1:D 40:. 90:. $P $P $P" "$W/ma/f" >/dev/null   # clock, title, story, landing
T="20:. 1:F 20:. 1:D 40:. 1:D 40:."
K="1:F 7:. 1:F 7:. 1:F 7:. 14:U 6:. 1:F 7:. 4:R 1:RF 7:. 1:F 7:. 14:U 6:. 1:F 7:."
$R "$T gear:31 god warp:14:3:9 60:. every:2 1:R 1:. $K $K $K $K $K" "$W/mb/f" >/dev/null   # Kraid
$G "$IMG/metroid_demo.gif" 66 "$W"/ma/*.pbm "$W"/mb/*.pbm
