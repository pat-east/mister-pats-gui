#!/bin/sh
# Replaces the GUI binary on the device.
#
# The patched main binary restarts the GUI a few seconds after it exits, and Linux refuses to
# overwrite a file that is still mapped as a running program. So: stop it, wait until the
# process is really gone, copy, and let it come back on its own.
set -e

DEVICE=${DEVICE:-root@192.168.64.163}
REMOTE=${REMOTE:-/media/fat/mister-pat}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BINARY=${1:-$HERE/build/mister-gui}

[ -f "$BINARY" ] || { echo "not found: $BINARY"; exit 1; }

echo "stopping the GUI and waiting for it to exit"
ssh -o BatchMode=yes "$DEVICE" '
    # Stop the parent first so nothing restarts the GUI while we replace it.
    killall MiSTer_gui 2>/dev/null
    killall -9 mister-gui 2>/dev/null

    i=0
    while [ $i -lt 15 ]; do
        ps -o args 2>/dev/null | grep -q "[d]ev/mister-gui" || exit 0
        sleep 1
        i=$((i + 1))
    done
    echo "still running" >&2
    exit 1
'

scp -q "$BINARY" "$DEVICE:$REMOTE/mister-gui"
ssh -o BatchMode=yes "$DEVICE" "chmod +x $REMOTE/mister-gui"
echo "copied"

# Without the main binary there is nothing to bring the GUI back, so restart the machine.
echo "rebooting the device"
ssh -o BatchMode=yes "$DEVICE" reboot 2>/dev/null || true

n=0
until ssh -o BatchMode=yes -o ConnectTimeout=5 "$DEVICE" 'exit 0' 2>/dev/null; do
    n=$((n + 1))
    [ $n -gt 40 ] && { echo "device did not come back"; exit 1; }
    sleep 5
done

echo "device is back"
