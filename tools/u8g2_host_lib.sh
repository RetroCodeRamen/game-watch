#!/bin/sh
# Build U8g2's C library for the host test simulators (once per out dir), so screenshots draw real text.
#   tools/u8g2_host_lib.sh <out dir>     -> <out dir>/u8g2/libu8g2.a   (prints the clib path to use with -I)
set -e
T=$(cd "$(dirname "$0")" && pwd); O="$1"
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
echo "$CLIB"
