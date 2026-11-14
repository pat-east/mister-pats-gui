#!/bin/sh
set -eu
umask 077

ROOT=/media/fat/mister-pat
fail() { printf 'BACKUP_ERROR=%s\n' "$1"; exit 1; }
[ -d "$ROOT" ] && [ ! -L "$ROOT" ] || fail 'GUI settings folder is unavailable.'
command -v tar >/dev/null 2>&1 || fail 'tar is unavailable.'
command -v date >/dev/null 2>&1 || fail 'date is unavailable.'

set --
for name in favorites.txt history.txt hidden-systems.txt preferences.txt systems.conf thegamesdb-settings.ini; do
    if [ -e "$ROOT/$name" ] || [ -L "$ROOT/$name" ]; then
        [ -f "$ROOT/$name" ] && [ ! -L "$ROOT/$name" ] || fail "Unsafe settings file: $name"
        set -- "$@" "$name"
    fi
done
[ "$#" -gt 0 ] || fail 'No GUI settings files were found.'

found=
for usb in /media/usb[0-9]*; do
    [ -d "$usb" ] && [ ! -L "$usb" ] || continue
    mountpoint=$(awk -v wanted="$usb" '$2==wanted {found=1} END {print found+0}' /proc/mounts 2>/dev/null || printf 0)
    [ "$mountpoint" = 1 ] || continue
    [ -w "$usb" ] || continue
    [ -z "$found" ] || fail 'Connect only one writable USB drive for backup.'
    found=$usb
done
[ -n "$found" ] || fail 'Connect a writable USB drive and try again.'
destination=$found/mister-pat-backups
mkdir -p "$destination" || fail 'Could not create the backup folder.'
[ -d "$destination" ] && [ ! -L "$destination" ] || fail 'The backup folder is unsafe.'
chmod 700 "$destination" || fail 'Could not protect the backup folder.'
stamp=$(date '+%Y%m%d-%H%M%S') || fail 'Could not read the current time.'
archive=$destination/mister-pat-settings-$stamp.tar.gz
temporary=$archive.tmp-$$
trap 'rm -f "$temporary"' EXIT
trap 'exit 1' HUP INT TERM
tar -czf "$temporary" -C "$ROOT" "$@" || fail 'Could not create the settings archive.'
tar -tzf "$temporary" >/dev/null 2>&1 || fail 'The settings archive could not be verified.'
mv "$temporary" "$archive" || fail 'Could not save the settings archive.'
trap - EXIT HUP INT TERM
printf 'BACKUP_PATH=%s\n' "$archive"
