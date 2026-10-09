#!/bin/sh
# Replaces the GUI and patched MiSTer binaries on the device.
#
# The patched main binary can restart the GUI after it exits. Stop its launcher and wait for
# both processes to disappear before replacing either executable.
set -e

DEVICE=${DEVICE:-root@192.168.64.191}
REMOTE=${REMOTE:-/media/fat/mister-pat}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BINARY=${1:-$HERE/build/mister-gui}
MISTER_BINARY=${MISTER_BINARY:-$HERE/third_party/Main_MiSTer/bin/MiSTer}
SSH_OPTS=${SSH_OPTS:--o BatchMode=yes}

[ -f "$BINARY" ] || { echo "not found: $BINARY"; exit 1; }
[ -f "$MISTER_BINARY" ] || {
    echo "not found: $MISTER_BINARY" >&2
    echo "Build the patched Main_MiSTer binary first (see docs/INSTALL.md)." >&2
    exit 1
}

echo "stopping the GUI and waiting for it to exit"
ssh $SSH_OPTS "$DEVICE" /bin/sh <<'REMOTE_SH'
set -eu

# The marker keeps MiSTer_gui from launching another GUI during the handoff. A reboot clears it.
: > /tmp/mister-pat-suspend-gui

# /tmp/script is the shell that starts the GUI and displays the crash screen after a nonzero
# exit. Stop it before stopping the GUI, or it can create a new process during our wait.
launcher_pids=$(ps -o pid,comm,args | awk '$2 == "script" && $0 ~ /\/tmp\/script/ { print $1 }')
for pid in $launcher_pids; do kill -KILL "$pid" 2>/dev/null || true; done

killall -TERM mister-gui 2>/dev/null || true
killall -TERM MiSTer_gui 2>/dev/null || true

running() {
    ps -o comm | awk '$1 == "mister-gui" || $1 == "MiSTer_gui" { found = 1 }
                       END { exit !found }'
}

i=0
while running && [ "$i" -lt 20 ]; do
    sleep 1
    i=$((i + 1))
done

if running; then
    # A download or decoder can delay a graceful exit. The launcher is already stopped, so
    # a forced exit here cannot start the crash screen.
    killall -KILL mister-gui 2>/dev/null || true
    killall -KILL MiSTer_gui 2>/dev/null || true
    sleep 1
fi

if running; then
    echo "GUI or MiSTer_gui is still running" >&2
    exit 1
fi
REMOTE_SH

# Stage complete files beside their destinations before replacing either binary.
scp $SSH_OPTS -q "$BINARY" "$DEVICE:$REMOTE/mister-gui.deploy"
scp $SSH_OPTS -q "$MISTER_BINARY" "$DEVICE:$REMOTE/MiSTer_gui.deploy"
ssh $SSH_OPTS "$DEVICE" "chmod +x '$REMOTE/mister-gui.deploy' '$REMOTE/MiSTer_gui.deploy' && mv -f '$REMOTE/mister-gui.deploy' '$REMOTE/mister-gui' && mv -f '$REMOTE/MiSTer_gui.deploy' '$REMOTE/MiSTer_gui'"
echo "copied"

# Without the main binary there is nothing to bring the GUI back, so restart the machine.
echo "rebooting the device"
ssh $SSH_OPTS "$DEVICE" reboot 2>/dev/null || true

n=0
until ssh $SSH_OPTS -o ConnectTimeout=5 "$DEVICE" 'exit 0' 2>/dev/null; do
    n=$((n + 1))
    [ $n -gt 40 ] && { echo "device did not come back"; exit 1; }
    sleep 5
done

echo "device is back"
