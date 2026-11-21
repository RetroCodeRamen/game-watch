#!/usr/bin/env bash
# Set a Game Watch's clock from this computer over USB (the watch must be running and plugged in).
#   ./scripts/settime.sh            # uses PORT=/dev/ttyACM0
# flash.sh runs this after uploading, so a freshly flashed watch starts on the right time.
set -euo pipefail
PORT="${PORT:-/dev/ttyACM0}"
for i in $(seq 1 20); do [[ -e "$PORT" ]] && break; sleep 0.5; done   # the watch re-enumerates after an upload
if [[ ! -e "$PORT" ]]; then echo "Serial port missing: $PORT" >&2; exit 1; fi
sleep 1.5                                                           # let it finish booting
stty -F "$PORT" 115200 raw -echo 2>/dev/null || true
read -r H M S MO D <<<"$(date '+%-H %-M %-S %-m %-d')"
printf 'T %d %d %d\n' $(( H * 3600 + M * 60 + S )) "$MO" "$D" > "$PORT"
echo "==> Clock set to $(date '+%H:%M:%S %b %d')"
