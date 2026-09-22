#!/bin/sh
# Renders one screen on the device and brings its canvas back as a PNG, so the interface can
# be reviewed without looking at the television. The app dumps its own buffer, so whatever
# else draws to the framebuffer cannot interfere.
#
#   tools/shot.sh <name> [mister-gui arguments...]
set -e

DEVICE=${DEVICE:-root@192.168.64.163}
REMOTE=${REMOTE:-/media/fat/mister-pat}
FRAMES=${FRAMES:-20}
HERE=$(cd "$(dirname "$0")/.." && pwd)
SHOTS="$HERE/build/shots"

NAME=$1
shift || true
[ -n "$NAME" ] || { echo "usage: tools/shot.sh <name> [args...]"; exit 1; }

mkdir -p "$SHOTS"

ssh -o BatchMode=yes "$DEVICE" \
    "$REMOTE/mister-gui --frames $FRAMES --no-grab --dump /tmp/shot.raw $*" \
    >"$SHOTS/$NAME.log" 2>&1 || { tail -5 "$SHOTS/$NAME.log"; exit 1; }

ssh -o BatchMode=yes "$DEVICE" "cat /tmp/shot.raw" >"$SHOTS/$NAME.raw" 2>/dev/null
python3 "$HERE/tools/fbshot.py" "$SHOTS/$NAME.raw" "$SHOTS/$NAME.png"
rm -f "$SHOTS/$NAME.raw"
grep -E "framebuffer|library|systems|games" "$SHOTS/$NAME.log" | head -5
