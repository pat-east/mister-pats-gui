#!/bin/sh
# Photographs whatever is on the MiSTer's screen right now and brings it back as a PNG.
#
#   tools/screenshot.sh [name]
#
# Prefers asking the running GUI for its own canvas (SIGUSR1), which is exact and cannot be
# disturbed by anything else drawing. Falls back to reading the framebuffer directly, which
# also captures foreign output — useful for seeing what is really on the television.
set -e

DEVICE=${DEVICE:-root@192.168.64.163}
HERE=$(cd "$(dirname "$0")/.." && pwd)
SHOTS="$HERE/build/shots"
NAME=${1:-screen}

mkdir -p "$SHOTS"

SIZE=$(ssh -o BatchMode=yes "$DEVICE" 'cat /sys/class/graphics/fb0/virtual_size 2>/dev/null' | tr -d '\r')
WIDTH=${SIZE%%,*}
HEIGHT=${SIZE##*,}
[ -n "$WIDTH" ] && [ -n "$HEIGHT" ] || { echo "cannot read the framebuffer size"; exit 1; }

SOURCE=$(ssh -o BatchMode=yes "$DEVICE" '
    pid=$(ps -o pid,args 2>/dev/null | grep "[d]ev/mister-gui" | awk "{print \$1}" | head -1)
    if [ -n "$pid" ]; then
        rm -f /tmp/mister-gui-shot.raw
        kill -s USR1 "$pid" 2>/dev/null || kill -10 "$pid"
        # Give the frame loop a moment to write the file out.
        i=0
        while [ $i -lt 5 ]; do
            [ -s /tmp/mister-gui-shot.raw ] && { echo canvas; exit 0; }
            sleep 1
            i=$((i + 1))
        done
    fi
    echo framebuffer
')

if [ "$SOURCE" = canvas ]; then
    ssh -o BatchMode=yes "$DEVICE" 'cat /tmp/mister-gui-shot.raw' > "$SHOTS/$NAME.raw"
else
    ssh -o BatchMode=yes "$DEVICE" 'cat /dev/fb0' > "$SHOTS/$NAME.raw"
fi

python3 "$HERE/tools/fbshot.py" "$SHOTS/$NAME.raw" "$SHOTS/$NAME.png" "$WIDTH" "$HEIGHT"
rm -f "$SHOTS/$NAME.raw"
echo "Quelle: $SOURCE"
