#!/bin/sh
# Screenshots of the GUI on the MiSTer, brought back as PNG.
#
#   tools/screenshot.sh set [name ...]   render the standard set into screenshots/ and rewrite
#                                        the gallery in README.md (tools/update-readme-screenshots.py)
#   tools/screenshot.sh one NAME FRAMES [gui arguments ...]
#                                        render any one screen into build/shots/NAME.png
#   tools/screenshot.sh live [NAME]      photograph what the running GUI shows right now
#   tools/screenshot.sh NAME             the same as "live NAME" (how this script always worked)
#
# "set" and "one" start a second copy of the GUI on the device for a moment, which draws its
# screen, writes its own canvas to a file and exits; the GUI that is already running is not
# touched. Nothing needs a controller: --tab, --system and --view choose the screen, and
# --press feeds it button presses, so a screen several presses deep can be reached.
#
#   --tab home|favorites|systems|arcade|games|settings
#   --system NAME          with --tab games: that system's list
#   --view grid|small|list|large|compact
#   --press LIST           comma separated: up down left right confirm back fav view prev next
#                          jumpprev jumpnext, and wait:N for N frames. One press per frame.
#   --frames N             how many frames to run before the picture is taken
#
# FRAMES is how long to let things load. Screens that fill in over time need more of them
# than the picture alone would: the Systems tab works out which systems have games (about 500
# frames), and Settings -> Manage Arcade -> Arcade Games reads every .mra and checks its ROMs
# (about 4000, several minutes on the device).
#
# Settings has a category list on the left (Library, Controllers, Interface, Updates, System);
# "down" moves between categories until "right" or "confirm" moves into the rows.
#
# The controller input test needs the pad's row in the Controllers list. Count down from the
# first row (0) and set CONTROLLER_ROW, e.g. CONTROLLER_ROW=1. Every row is listed, including
# sub-devices such as "Motion Sensors", so look at Settings -> Controllers first.
#
# Environment:
#   DEVICE          ssh target                         (default root@192.168.64.163)
#   REMOTE          the GUI's directory on the device  (default /media/fat/mister-pat)
#   GUI             the binary inside REMOTE           (default mister-gui)
#   CONTROLLER_ROW  see above                          (default 0)
#
# Only the GUI that is on the device is used, so deploy a build first (tools/deploy.sh). It
# must be one that knows --press; older ones ignore it and show the screen they started on.
set -e

DEVICE=${DEVICE:-root@192.168.64.163}
REMOTE=${REMOTE:-/media/fat/mister-pat}
GUI=${GUI:-mister-gui}
CONTROLLER_ROW=${CONTROLLER_ROW:-0}
HERE=$(cd "$(dirname "$0")/.." && pwd)
SHOTS="$HERE/build/shots"
SET_DIR="$HERE/screenshots"

SSH="ssh -o BatchMode=yes $DEVICE"

# ---------------------------------------------------------------------------------------
# one: render a screen on the device and fetch it
# ---------------------------------------------------------------------------------------
render() {
    name=$1; frames=$2; out=$3; shift 3
    mkdir -p "$out"

    $SSH "$REMOTE/$GUI --frames $frames --dump /tmp/shot.raw $*" > "$out/$name.log" 2>&1 \
        || { echo "  $name: the GUI failed"; tail -5 "$out/$name.log"; return 1; }

    $SSH 'cat /tmp/shot.raw' > "$out/$name.raw" 2>/dev/null
    python3 "$HERE/tools/fbshot.py" "$out/$name.raw" "$out/$name.png" > /dev/null
    rm -f "$out/$name.raw" "$out/$name.log"
    echo "  $out/$name.png"
}

# The controller row is a run of "down" presses.
downs() {
    n=$1; s=""
    while [ "$n" -gt 0 ]; do s="$s,down"; n=$((n - 1)); done
    echo "$s"
}

# ---------------------------------------------------------------------------------------
# set: the pictures the README uses
# ---------------------------------------------------------------------------------------
# arcade-games is not in the standard set: it takes several minutes on the device and the
# README does not show it. Ask for it by name.
STANDARD="home favorites systems snes snes-grid snes-small snes-list arcade games settings
settings-interface settings-updates controller-test"

shot_set() {
    wanted=${*:-$STANDARD}

    for name in $wanted; do
        case $name in
        home)        render $name 120 "$SET_DIR" --no-grab --tab home ;;
        favorites)   render $name 120 "$SET_DIR" --no-grab --tab favorites ;;
        systems)     render $name 500 "$SET_DIR" --no-grab --tab systems ;;
        snes)        render $name 150 "$SET_DIR" --no-grab --tab games --system SNES ;;
        snes-grid)   render $name 150 "$SET_DIR" --no-grab --tab games --system SNES --view grid ;;
        snes-small)  render $name 150 "$SET_DIR" --no-grab --tab games --system SNES --view small ;;
        snes-list)   render $name 150 "$SET_DIR" --no-grab --tab games --system SNES --view list ;;
        arcade)      render $name 150 "$SET_DIR" --no-grab --tab arcade ;;
        games)       render $name 150 "$SET_DIR" --no-grab --tab games ;;
        settings)    render $name 60 "$SET_DIR" --no-grab --tab settings --press "right,wait:5" ;;
        settings-interface)
            render $name 60 "$SET_DIR" --no-grab --tab settings --press "down,down,right,wait:5" ;;
        settings-updates)
            # Offline or not, the page shows the running and selected versions and the check
            # state; "wait" gives the update service time to settle.
            render $name 90 "$SET_DIR" --no-grab --tab settings --press "down,down,down,right,wait:30" ;;
        controller-test)
            # Needs the input devices, so no --no-grab. Controllers is the second category and
            # holds one row of the same name. The test hands the screen back after ten idle
            # seconds, hence the short run.
            render $name 50 "$SET_DIR" --tab settings \
                --press "down,confirm,confirm,wait:5$(downs "$CONTROLLER_ROW"),confirm,wait:20"
            ;;
        arcade-games)
            # Library row 5 is Manage Arcade, its first row Arcade Games. "view" at the end
            # switches the table's filter from All to Working, which leaves out the games
            # whose ROMs are not there.
            echo "  arcade-games: reads every .mra, this takes several minutes"
            render $name 4100 "$SET_DIR" --no-grab --tab settings \
                --press "confirm,down,down,down,down,down,confirm,confirm,wait:3950,view,wait:10"
            ;;
        *)
            echo "  unknown picture: $name (known: $STANDARD arcade-games)" ; return 1 ;;
        esac
    done

    python3 "$HERE/tools/update-readme-screenshots.py"
}

# ---------------------------------------------------------------------------------------
# live: what the running GUI shows right now
# ---------------------------------------------------------------------------------------
shot_live() {
    NAME=${1:-screen}
    mkdir -p "$SHOTS"

    # Prefers asking the running GUI for its own canvas (SIGUSR1), which is exact and cannot
    # be disturbed by anything else drawing. Falls back to reading the framebuffer directly,
    # which also captures foreign output — useful for seeing what is really on the television.
    SIZE=$($SSH 'cat /sys/class/graphics/fb0/virtual_size 2>/dev/null' | tr -d '\r')
    WIDTH=${SIZE%%,*}
    HEIGHT=${SIZE##*,}
    [ -n "$WIDTH" ] && [ -n "$HEIGHT" ] || { echo "cannot read the framebuffer size"; exit 1; }

    SOURCE=$($SSH '
        pid=$(ps -o pid,args 2>/dev/null | grep "[d]ev/mister-gui" | awk "{print \$1}" | head -1)
        [ -n "$pid" ] || pid=$(ps -o pid,args 2>/dev/null | grep "[m]ister-pat/mister-gui" | awk "{print \$1}" | head -1)
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
        $SSH 'cat /tmp/mister-gui-shot.raw' > "$SHOTS/$NAME.raw"
    else
        $SSH 'cat /dev/fb0' > "$SHOTS/$NAME.raw"
    fi

    python3 "$HERE/tools/fbshot.py" "$SHOTS/$NAME.raw" "$SHOTS/$NAME.png" "$WIDTH" "$HEIGHT"
    rm -f "$SHOTS/$NAME.raw"
    echo "$SHOTS/$NAME.png  (source: $SOURCE)"
}

case "${1:-}" in
set)   shift; shot_set "$@" ;;
one)
    shift
    [ $# -ge 2 ] || { echo "usage: tools/screenshot.sh one NAME FRAMES [gui arguments ...]"; exit 1; }
    name=$1; frames=$2; shift 2
    render "$name" "$frames" "$SHOTS" "$@"
    ;;
live)  shift; shot_live "$@" ;;
"")    shot_live ;;
*)     shot_live "$1" ;;
esac
