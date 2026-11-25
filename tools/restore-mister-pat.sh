#!/bin/sh
set -eu
umask 077

ROOT=/media/fat/mister-pat
fail() { printf '[ERROR] %s\n' "$1" >&2; exit 1; }
[ "$#" -eq 1 ] || fail 'Usage: restore-mister-pat.sh PATH_TO_BACKUP.tar.gz'
archive=$1
[ -f "$archive" ] && [ ! -L "$archive" ] || fail 'Backup archive is not a regular file.'
[ -d "$ROOT" ] && [ ! -L "$ROOT" ] || fail 'GUI settings folder is unavailable.'
command -v tar >/dev/null 2>&1 || fail 'tar is unavailable.'
command -v mktemp >/dev/null 2>&1 || fail 'mktemp is unavailable.'
stage=$(mktemp -d /tmp/mister-pat-restore.XXXXXX) || fail 'Could not create a temporary restore folder.'
cleanup() { rm -rf "$stage"; }
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

tar -tzf "$archive" > "$stage/list" 2>/dev/null || fail 'Backup archive is invalid or damaged.'
awk '
    BEGIN { allowed["favorites.txt"]=1; allowed["history.txt"]=1; allowed["hidden-systems.txt"]=1; allowed["preferences.txt"]=1; allowed["systems.conf"]=1; allowed["thegamesdb-settings.ini"]=1; allowed["screenscraper-fr-settings.ini"]=1 }
    { if (!allowed[$0] || seen[$0]++ || $0 ~ /\//) exit 1 }
    END { if (NR==0) exit 1 }
' "$stage/list" || fail 'Archive contains unexpected or duplicate files.'
tar -xzf "$archive" -C "$stage" || fail 'Could not unpack backup archive.'

# Validate every source and destination and prepare all replacements before changing settings.
while IFS= read -r name; do
    [ -f "$stage/$name" ] && [ ! -L "$stage/$name" ] || fail 'Archive contains an unsafe file.'
    if [ -e "$ROOT/$name" ] || [ -L "$ROOT/$name" ]; then
        [ -f "$ROOT/$name" ] && [ ! -L "$ROOT/$name" ] || fail "Unsafe destination file: $name"
    fi
    cp "$stage/$name" "$ROOT/.$name.restore-$$" || fail "Could not prepare $name."
    chmod 600 "$ROOT/.$name.restore-$$" || fail "Could not protect $name."
done < "$stage/list"
while IFS= read -r name; do
    mv -f "$ROOT/.$name.restore-$$" "$ROOT/$name" || fail "Could not restore $name."
done < "$stage/list"
printf 'Restored GUI settings from %s\n' "$archive"
