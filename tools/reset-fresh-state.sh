#!/bin/sh
# Puts the GUI's user data on the MiSTer back to the state of a fresh installation, so the next start
# behaves like a first start (first-run Jump Start, default settings, no game list, no artwork).
# It is a testing tool. It only touches files inside /media/fat/mister-pat and never the program files
# (MiSTer_gui, mister-gui, mister-pats-gui*.so, the disc index, tools/, cacert.pem), MiSTer.ini, the
# games, the Wi-Fi settings or the cores.
#
# Make a backup first: Settings -> System -> Back up GUI settings (favorites, history, hidden systems,
# preferences, systems.conf, thegamesdb-settings.ini, screenscraper-fr-settings.ini). The backup does NOT
# contain the game list, the covers or the videos; this script leaves covers and videos alone by default
# (--also-media removes them too).
#
# Usage (on the MiSTer, e.g. from the Scripts menu or over SSH):
#   sh reset-fresh-state.sh              show what would be removed (nothing is changed)
#   sh reset-fresh-state.sh --yes        stop the GUI and remove it
# From the Mac, without copying the file:
#   ssh root@<mister-ip> 'sh -s -- --yes' < tools/reset-fresh-state.sh
# Options:
#   --yes            really remove (without it the script only lists)
#   --keep-logins    keep screenscraper-fr-settings.ini and thegamesdb-settings.ini
#   --keep-caches    keep ALL downloaded caches (game list, icons, fonts, core cache, logs) and only
#                    reset the settings and personal lists
#   --also-media     also remove covers and videos (artwork index, scrape index, ScreenScraper
#                    videos and results, disc artwork cache); by default they are kept
#   --reboot         reboot the MiSTer afterwards (default: no reboot; restart the GUI yourself)
set -eu

ROOT=/media/fat/mister-pat
DO=0; KEEP_LOGINS=0; KEEP_CACHES=0; ALSO_MEDIA=0; REBOOT=0
for arg in "$@"; do
    case "$arg" in
        --yes) DO=1 ;;
        --keep-logins) KEEP_LOGINS=1 ;;
        --keep-caches) KEEP_CACHES=1 ;;
        --also-media) ALSO_MEDIA=1 ;;
        --reboot) REBOOT=1 ;;
        *) printf 'Unknown option: %s\n' "$arg" >&2; exit 2 ;;
    esac
done

fail() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }
[ -d "$ROOT" ] && [ ! -L "$ROOT" ] || fail "$ROOT is not a directory (nothing to reset)."

# Settings and personal data (always).
FILES="favorites.txt history.txt hidden-systems.txt preferences.txt systems.conf"
[ "$KEEP_LOGINS" -eq 1 ] || FILES="$FILES screenscraper-fr-settings.ini thegamesdb-settings.ini"
# Everything the GUI generates or downloads itself, so that the first start shows the whole first-run path
# again: caches, logs, the Space Grotesk font, the system icons and all tab icons (disc.bmp too). This is NOT the
# state right after tools/install-update.sh (the installer downloads the font and the icons itself); it is the
# state in which the GUI has to fetch them in its first-run assets step. The folders stay, only their contents go. Covers and videos are not in this list:
# they are slow to fetch again and are only removed with --also-media.
CACHE_FILES=""
CACHE_DIRS="gamesdb cache version-groups logs .update-staging fonts icons"
MEDIA_FILES="boxart-index.tsv scrape-misses.txt"
MEDIA_DIRS="scrape-index screenscraper disc-cache"
# Leftovers of an interrupted update.
LEFTOVERS=".update-lock"

list=
add() { [ -e "$ROOT/$1" ] || [ -L "$ROOT/$1" ] && list="$list $1" || :; }
for f in $FILES; do add "$f"; done
if [ "$KEEP_CACHES" -eq 0 ]; then
    for f in $CACHE_FILES $CACHE_DIRS; do add "$f"; done
fi
if [ "$ALSO_MEDIA" -eq 1 ]; then
    for f in $MEDIA_FILES $MEDIA_DIRS; do add "$f"; done
fi
for f in $LEFTOVERS; do add "$f"; done
# All tab icons go, disc.bmp included (the GUI downloads them again in the first-run assets step).
tabicons=
if [ "$KEEP_CACHES" -eq 0 ] && [ -d "$ROOT/tab-icons" ] && [ ! -L "$ROOT/tab-icons" ]; then
    for f in "$ROOT"/tab-icons/*; do
        [ -e "$f" ] || continue
        tabicons="$tabicons $f"
    done
fi

if [ -z "$list" ] && [ -z "$tabicons" ]; then
    echo 'Nothing to remove: the installation is already in a fresh state.'
    exit 0
fi

echo "Inside $ROOT the following would be removed:"
for f in $list; do
    if [ -d "$ROOT/$f" ]; then printf '  %s/   (%s)\n' "$f" "$(du -sk "$ROOT/$f" 2>/dev/null | awk '{printf "%d KB", $1}')"
    else printf '  %s\n' "$f"; fi
done
for f in $tabicons; do printf '  tab-icons/%s\n' "$(basename "$f")"; done
echo 'Kept: program files, disc index, tools/, cacert.pem, MiSTer.ini, mister-ini-backups/.'
[ "$ALSO_MEDIA" -eq 1 ] || echo 'Kept: covers and videos (boxart-index.tsv, scrape-index/, screenscraper/, disc-cache/).'

if [ "$DO" -ne 1 ]; then
    echo
    echo 'Nothing was changed. Run again with --yes to remove it (the GUI is stopped first).'
    exit 0
fi

# Stop the GUI the way tools/deploy.sh does, so it cannot write its settings again on exit.
: > /tmp/mister-pat-suspend-gui
launcher_pids=$(ps -o pid,comm,args | awk '$2=="script" && $0 ~ /\/tmp\/script/ {print $1}')
for pid in $launcher_pids; do kill -KILL "$pid" 2>/dev/null || :; done
killall -TERM mister-gui 2>/dev/null || :
killall -TERM MiSTer_gui 2>/dev/null || :
running() { ps -o comm | awk '$1=="mister-gui" || $1=="MiSTer_gui" {found=1} END {exit !found}'; }
i=0
while running && [ "$i" -lt 20 ]; do sleep 1; i=$((i+1)); done
if running; then killall -KILL mister-gui MiSTer_gui 2>/dev/null || :; sleep 1; fi
running && fail 'The GUI or the main program is still running; nothing was removed.'

# Remove. Every path is checked to be directly inside $ROOT and not to climb out of it.
safe_remove() {
    case "$1" in
        ''|*/*|.|..) fail "Refusing to remove an unexpected name: $1" ;;
    esac
    rm -rf "$ROOT/$1"
}
# A cache folder keeps its place; only what is inside goes.
empty_folder() {
    case "$1" in
        ''|*/*|.|..) fail "Refusing to empty an unexpected name: $1" ;;
    esac
    [ -d "$ROOT/$1" ] && [ ! -L "$ROOT/$1" ] || return 0
    find "$ROOT/$1" -mindepth 1 -maxdepth 1 -exec rm -rf {} + 2>/dev/null || :
}
for f in $tabicons; do rm -f "$f"; done
for f in $list; do
    case " $CACHE_DIRS " in
        *" $f "*) empty_folder "$f" ;;
        *) safe_remove "$f" ;;
    esac
done
sync
rm -f /tmp/mister-pat-suspend-gui
echo 'Done. The next start of the GUI is a fresh start.'
if [ "$REBOOT" -eq 1 ]; then
    echo 'Rebooting.'
    reboot
else
    echo 'Restart the MiSTer (or the GUI) yourself to see the first start.'
fi
