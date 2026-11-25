#!/bin/sh
# Records the MiSTer's screen once per second while you test. Runs on the MAC: it asks the MiSTer over SSH for
# its framebuffer (read-only, nothing is written on the MiSTer or its SD card) and stores a picture here.
#
#   tools/fb-record.sh start [options]     record until you press Ctrl-C (or -t seconds have passed)
#   tools/fb-record.sh note "text"         (second terminal) mark the current second in notes.log
#   tools/fb-record.sh status              (second terminal) running? seconds, pictures, size
#   tools/fb-record.sh stop                (second terminal) stop it like Ctrl-C
#   tools/fb-record.sh video <folder>      make session.mp4 from a recording folder (also done by --video)
#
# Output: <dir>/<timestamp>/<second-of-recording>.png, e.g. recordings/20261124-093015/000042.png
#   meta.txt     device, geometry, start time, interval
#   frames.log   one line per second: "<second> saved" / "<second> same" / "<second> late" / "<second> unreadable"
#   notes.log    your note lines with the second they were written
# A second in which the screen did not change gets no new conversion: the previous picture is copied.
#
# Options for start:
#   --device USER@HOST   the MiSTer, default $DEVICE or root@192.168.64.163 (key login is needed, like tools/shot.sh)
#   -i SECONDS           interval, default 1
#   -t SECONDS           stop after this many seconds
#   -d DIR               recordings folder, default <project>/recordings (git-ignored)
#   --scale PERCENT      shrink the pictures, default 100
#   --video              also write session.mp4 at the end (one picture per second; ffmpeg)
#   --raw-transfer       do not compress on the MiSTer (about 8 MB per second over the network; not for Wi-Fi)
#
# Cost on the MiSTer: one read of /dev/fb0 per second and gzip -1 at the lowest priority (about 8 MB in, 0.1 to 2 MB out).
# If the GUI feels slower while recording, use -i 2 or --raw-transfer on a cable. Needs ImageMagick (magick) here.
set -eu

HERE=$(cd "$(dirname "$0")/.." && pwd)
DEVICE=${DEVICE:-root@192.168.64.163}
FB_DEV=${FB_DEV:-/dev/fb0}
FB_SYS=${FB_SYS:-/sys/class/graphics/fb0}
SSH=${FB_SSH:-ssh}
STATEFILE=${FB_RECORD_STATE:-/tmp/fb-record-mac.state}

fail() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }
say() { printf '%s\n' "$1"; }
state() { sed -n "s/^$1=//p" "$STATEFILE" 2>/dev/null | head -1; }
alive() { pid=$(state pid); [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; }

make_video() {
    folder=$1
    command -v ffmpeg >/dev/null 2>&1 || fail 'ffmpeg is required for the video.'
    [ -d "$folder" ] || fail "$folder is not a folder."
    ffmpeg -y -loglevel error -framerate 1 -pattern_type glob -i "$folder/*.png" -c:v libx264 -pix_fmt yuv420p \
        -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" "$folder/session.mp4"
    say "Wrote $folder/session.mp4"
}

cmd=${1:-}
[ -n "$cmd" ] || { sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
shift
case "$cmd" in
status)
    if alive; then
        dir=$(state dir); now=$(date +%s)
        say "Recording to $dir, $((now - $(state begin))) s so far, $(ls "$dir" | grep -c '\.png$') pictures, $(du -sk "$dir" | awk '{printf "%d MB", $1/1024}')."
    else say 'Not recording.'; fi
    exit 0 ;;
stop)
    alive || { say 'Not recording.'; exit 0; }
    kill -INT "$(state pid)" 2>/dev/null || :
    say 'Stopping (the last pictures are finished first).'
    exit 0 ;;
note)
    [ "$#" -ge 1 ] || fail 'Usage: fb-record.sh note "text"'
    alive || fail 'Not recording.'
    now=$(date +%s); sec=$((now - $(state begin)))
    printf '%06d  %s\n' "$sec" "$*" >> "$(state dir)/notes.log"
    say "noted at second $sec"
    exit 0 ;;
video) [ "$#" -ge 1 ] || fail 'Usage: fb-record.sh video <folder>'; make_video "$1"; exit 0 ;;
start) ;;
*) fail "Unknown command: $cmd (start, stop, status, note, video)" ;;
esac

INTERVAL=1; MAXSEC=0; DIR=$HERE/recordings; SCALE=100; VIDEO=0; GZ=1
while [ "$#" -gt 0 ]; do
    case "$1" in
        --device) DEVICE=${2:?--device needs user@host}; shift 2 ;;
        -i) INTERVAL=${2:?-i needs seconds}; shift 2 ;;
        -t) MAXSEC=${2:?-t needs seconds}; shift 2 ;;
        -d) DIR=${2:?-d needs a folder}; shift 2 ;;
        --scale) SCALE=${2:?--scale needs a percent}; shift 2 ;;
        --video) VIDEO=1; shift ;;
        --raw-transfer) GZ=0; shift ;;
        *) fail "Unknown option: $1" ;;
    esac
done
case "$INTERVAL$MAXSEC$SCALE" in *[!0-9]*|'') fail 'Interval, time and scale must be whole numbers.' ;; esac
[ "$INTERVAL" -ge 1 ] && [ "$SCALE" -ge 1 ] || fail 'Interval and scale must be at least 1.'
command -v magick >/dev/null 2>&1 || fail 'ImageMagick (magick) is required.'
alive && fail "Already recording to $(state dir)."

# One SSH connection is kept open, so each second costs one command, not a new login.
CTL=/tmp/fb-record-ssh.$$
SSHO="-o BatchMode=yes -o ConnectTimeout=5 -o ControlMaster=auto -o ControlPath=$CTL -o ControlPersist=120"
remote() { $SSH $SSHO "$DEVICE" "$1"; }

geo=$(remote "cut -d, -f1 $FB_SYS/virtual_size; cut -d, -f2 $FB_SYS/virtual_size; cat $FB_SYS/stride; cat $FB_SYS/bits_per_pixel") \
    || fail "Cannot reach $DEVICE over SSH (key login needed; check DEVICE or --device)."
w=$(printf '%s\n' "$geo" | sed -n 1p); h=$(printf '%s\n' "$geo" | sed -n 2p)
stride=$(printf '%s\n' "$geo" | sed -n 3p); bpp=$(printf '%s\n' "$geo" | sed -n 4p)
case "$w$h$stride$bpp" in *[!0-9]*|'') fail "Could not read the framebuffer geometry from $DEVICE." ;; esac
[ "$bpp" -eq 32 ] && [ "$stride" -eq $((w * 4)) ] || fail "Unsupported framebuffer format (bpp $bpp, stride $stride, width $w)."

begin=$(date +%s)
stamp=$(date '+%Y%m%d-%H%M%S')
mkdir -p "$DIR" || fail "Cannot create $DIR."
session=$DIR/$stamp
mkdir "$session" || fail "Cannot create $session."
printf 'device=%s\nwidth=%s\nheight=%s\nstride=%s\nbpp=%s\nstart=%s\nstart_epoch=%s\ninterval=%s\nscale=%s\n' \
    "$DEVICE" "$w" "$h" "$stride" "$bpp" "$stamp" "$begin" "$INTERVAL" "$SCALE" > "$session/meta.txt"
: > "$session/frames.log"
printf 'pid=%s\ndir=%s\nbegin=%s\n' "$$" "$session" "$begin" > "$STATEFILE"

tmp=${TMPDIR:-/tmp}/fb-record.$$
pending=
finish() {
    [ -z "$pending" ] || wait "$pending" 2>/dev/null || :
    $SSH -o ControlPath="$CTL" -O exit "$DEVICE" >/dev/null 2>&1 || :
    rm -f "$tmp".* "$STATEFILE"
    say "Recording finished: $session ($(ls "$session" | grep -c '\.png$') pictures)"
    [ "$VIDEO" -eq 0 ] || make_video "$session" || :
}
trap 'exit 0' INT TERM
trap finish EXIT

say "Recording $DEVICE (${w}x${h}) to $session"
say 'Ctrl-C stops. Mark a moment from a second terminal: tools/fb-record.sh note "text"'
last=-1; prevsum=; prevpng=
remote_cmd="dd if=$FB_DEV bs=$stride count=$h 2>/dev/null"
[ "$GZ" -eq 0 ] || remote_cmd="$remote_cmd | nice -n 19 gzip -1 -n"
while :; do
    now=$(date +%s)
    sec=$(( (now - begin) / INTERVAL * INTERVAL ))
    if [ "$sec" -le "$last" ]; then sleep 0.2; continue; fi
    if [ "$MAXSEC" -gt 0 ] && [ "$sec" -gt "$MAXSEC" ]; then break; fi
    if [ "$last" -ge 0 ] && [ "$sec" -gt $((last + INTERVAL)) ]; then printf '%06d late (skipped to this second)\n' "$sec" >> "$session/frames.log"; fi
    last=$sec
    name=$(printf '%06d' "$sec")
    out=$session/$name.png
    if remote "$remote_cmd" > "$tmp.cur" 2>/dev/null && [ -s "$tmp.cur" ]; then
        sum=$(cksum < "$tmp.cur")
        if [ -n "$prevpng" ] && [ "$sum" = "$prevsum" ]; then
            cp "$prevpng" "$out"
            printf '%s same\n' "$name" >> "$session/frames.log"
        else
            # Convert in the background so the next second is not delayed; one conversion at a time.
            [ -z "$pending" ] || wait "$pending" 2>/dev/null || :
            mv "$tmp.cur" "$tmp.$name"
            if [ "$GZ" -eq 1 ]; then
                ( gunzip -c "$tmp.$name" | magick -size "${w}x${h}" -depth 8 bgra:- -alpha off -resize "${SCALE}%" "$out"; rm -f "$tmp.$name" ) &
            else
                ( magick -size "${w}x${h}" -depth 8 "bgra:$tmp.$name" -alpha off -resize "${SCALE}%" "$out"; rm -f "$tmp.$name" ) &
            fi
            pending=$!
            prevsum=$sum; prevpng=$out
            printf '%s saved\n' "$name" >> "$session/frames.log"
        fi
    else
        printf '%s unreadable\n' "$name" >> "$session/frames.log"
    fi
done
