#!/bin/sh
# Runs as one SSH transaction while holding the same lock as the GUI and installer.
set -eu
MODE=$1; ROOT=$2; STAGE=$3; VERSION=$4; H_LOADER=$5; H_GUI=$6; H_MAIN=$7
LOCK=$ROOT/.update-lock
BACKUP=$ROOT/.dev-backup
MARKER=$ROOT/.development-build
LINK=$ROOT/mister-pats-gui.so
OWN=0
COMMIT=0
SUCCESS=0
fail() { echo "dev deploy: $*" >&2; exit 1; }
sha() { sha256sum "$1" | awk '{print $1}'; }
valid_name() {
    case "$1" in mister-pats-gui-*.so) :;; *) return 1;; esac
    v=${1#mister-pats-gui-}; v=${v%.so}
    printf '%s\n' "$v" | awk -F. 'NF!=3 {exit 1} {for(i=1;i<=3;i++) if($i !~ /^(0|[1-9][0-9]*)$/ || $i+0>65535) exit 1}'
}
valid_slots() {
    list=$1
    [ -n "$list" ] || return 1
    case "$list" in ,*|*,|*,,*) return 1;; esac
    saved_ifs=$IFS; IFS=,
    for entry in $list; do valid_name "$entry" || { IFS=$saved_ifs; return 1; }; done
    IFS=$saved_ifs
}
elf() { [ "$(od -An -tx1 -N4 "$1" | tr -d ' \n')" = 7f454c46 ]; }
check_backup() {
    [ -d "$BACKUP" ] && [ ! -L "$BACKUP" ] || fail "missing release backup $BACKUP"
    [ "$(wc -l < "$BACKUP/backup.sha256")" -eq 4 ] || fail 'invalid backup manifest'
    awk 'BEGIN {n[1]="target.txt";n[2]="mister-gui";n[3]="MiSTer_gui";n[4]="library.so"} {if(NR>4 || length($0)!=66+length(n[NR]) || substr($0,65,2)!="  " || substr($0,67)!=n[NR] || substr($0,1,64) ~ /[^0-9a-f]/) exit 1} END {if(NR!=4) exit 1}' "$BACKUP/backup.sha256" || fail 'invalid backup manifest'
    for name in target.txt mister-gui MiSTer_gui library.so; do
        [ -f "$BACKUP/$name" ] && [ ! -L "$BACKUP/$name" ] || fail "unsafe backup $name"
        expected=$(awk -v n="$name" '$2==n && length($1)==64 && $1 !~ /[^0-9a-f]/ {print $1}' "$BACKUP/backup.sha256")
        [ -n "$expected" ] && [ "$(sha "$BACKUP/$name")" = "$expected" ] || fail "bad backup $name"
    done
    saved_target=$(cat "$BACKUP/target.txt")
    case "$saved_target" in
        auto:*) backup_mode=auto; original=${saved_target#auto:};;
        *) backup_mode=pinned; original=$saved_target;;
    esac
    valid_name "$original" || fail 'unsafe backed-up link'
}
newest_library() {
    newest=
    newest_version=
    for path in "$ROOT"/mister-pats-gui-*.so; do
        [ -f "$path" ] && [ ! -L "$path" ] || continue
        name=${path##*/}
        valid_name "$name" || continue
        version=${name#mister-pats-gui-}; version=${version%.so}
        if [ -z "$newest_version" ] || [ "$(awk -v a="$newest_version" -v b="$version" 'BEGIN {split(a,x,".");split(b,y,".");for(i=1;i<=3;i++) if(x[i]+0<y[i]+0){print -1;exit} else if(x[i]+0>y[i]+0){print 1;exit} print 0}')" -lt 0 ]; then
            newest=$name; newest_version=$version
        fi
    done
    [ -n "$newest" ]
}
copy_verified() {
    source=$1; destination=$2; hash=$3
    rm -f "$destination.new-$$" || fail "cannot clear staging path: $destination"
    cp "$source" "$destination.new-$$" || fail "copy failed: $destination"
    chmod 755 "$destination.new-$$"
    [ "$(sha "$destination.new-$$")" = "$hash" ] || fail "copy hash changed: $destination"
    mv -f "$destination.new-$$" "$destination" || fail "rename failed: $destination"
}
restore_release() {
    check_backup
    copy_verified "$BACKUP/library.so" "$ROOT/$original" "$(sha "$BACKUP/library.so")"
    copy_verified "$BACKUP/MiSTer_gui" "$ROOT/MiSTer_gui" "$(sha "$BACKUP/MiSTer_gui")"
    copy_verified "$BACKUP/mister-gui" "$ROOT/mister-gui" "$(sha "$BACKUP/mister-gui")"
    sync
    if [ "$backup_mode" = auto ]; then
        rm -f "$LINK" || fail 'cannot restore automatic library selection'
    else
        rm -f "$LINK.new-$$"
        ln -s "$original" "$LINK.new-$$" || fail 'cannot stage release link'
        mv -f "$LINK.new-$$" "$LINK" || fail 'cannot switch release link'
    fi
    sync
}
cleanup() {
    status=$?
    [ "$SUCCESS" -eq 1 ] || if [ "$COMMIT" -eq 1 ] && [ "$MODE" != --restore-release ]; then
        echo 'dev deploy failed during commit; attempting release restore' >&2
        restore_release || echo "manual repair required: $BACKUP $LINK" >&2
    fi
    [ "$STAGE" = none ] || rm -rf "$STAGE"
    if [ "$OWN" -eq 1 ]; then rm -f "$LOCK/owner"; rmdir "$LOCK" || :; fi
    exit "$status"
}
trap cleanup EXIT
boot=$(cat /proc/sys/kernel/random/boot_id) || fail 'cannot read boot ID'
attempt=0
while [ "$attempt" -lt 2 ]; do
    if mkdir "$LOCK" 2>/dev/null; then
        OWN=1; printf 'pid=%s\nboot=%s\n' "$$" "$boot" > "$LOCK/owner"
        break
    fi
    [ -d "$LOCK" ] && [ ! -L "$LOCK" ] || fail "unsafe lock path: $LOCK"
    [ -r "$LOCK/owner" ] || fail "unreadable lock: $LOCK"
    pid=$(sed -n '1s/^pid=\([0-9][0-9]*\)$/\1/p' "$LOCK/owner")
    oldboot=$(sed -n '2s/^boot=\([0-9a-f-][0-9a-f-]*\)$/\1/p' "$LOCK/owner")
    [ -n "$pid" ] && [ "$pid" -gt 0 ] && [ "${#oldboot}" -eq 36 ] || fail "malformed lock: $LOCK"
    if [ "$oldboot" = "$boot" ] && kill -0 "$pid" 2>/dev/null; then fail 'update already running'; fi
    rm -f "$LOCK/owner" && rmdir "$LOCK" || fail 'cannot clear stale lock'
    attempt=$((attempt+1))
done
[ "$OWN" -eq 1 ] || fail 'cannot acquire lock'
if [ "$MODE" = deploy ]; then
    for pair in "mister-gui:$H_LOADER" "library.so:$H_GUI" "MiSTer_gui:$H_MAIN"; do
        name=${pair%%:*}; hash=${pair#*:}
        case "$hash" in *[!0-9a-f]*|'') fail 'unsafe transfer hash';; esac
        [ "${#hash}" -eq 64 ] && [ -f "$STAGE/$name" ] && elf "$STAGE/$name" && [ "$(sha "$STAGE/$name")" = "$hash" ] || fail "invalid staged $name"
    done
    if [ -e "$MARKER" ]; then
        check_backup
        [ -f "$MARKER" ] && [ ! -L "$MARKER" ] &&
            [ "$(wc -l < "$MARKER")" -eq 4 ] || fail 'invalid dev marker'
        marker_version=$(sed -n '1s/^version=\(.*\)$/\1/p' "$MARKER")
        [ "$marker_version" = "$VERSION-dev" ] || fail 'dev marker has another version'
        [ "$(sed -n '3p' "$MARKER")" = 'backup=.dev-backup' ] || fail 'invalid dev backup path'
        previous_slot=$(sed -n '2s/^library=\(.*\)$/\1/p' "$MARKER")
        valid_name "$previous_slot" || fail 'invalid dev library in marker'
        active=$(readlink "$LINK") || fail 'missing active library link'
        [ "$active" = "$previous_slot" ] || [ "$active" = "$original" ] || fail 'unknown active library'
        slots=$(sed -n '4s/^slots=\(.*\)$/\1/p' "$MARKER")
        valid_slots "$slots" || fail 'invalid dev slots'
    else
        if [ -L "$LINK" ]; then
            release_target=$(readlink "$LINK")
            backup_target=$release_target
        else
            [ ! -e "$LINK" ] || fail 'invalid release version pin'
            newest_library || fail 'no release library to back up'
            release_target=$newest
            backup_target=auto:$release_target
        fi
        valid_name "$release_target" && [ -f "$ROOT/$release_target" ] && [ ! -L "$ROOT/$release_target" ] || fail 'invalid release library'
        [ -f "$ROOT/MiSTer_gui" ] && [ -f "$ROOT/mister-gui" ] || fail 'missing release binaries'
        if [ -e "$BACKUP" ] || [ -L "$BACKUP" ]; then
            check_backup
            [ "$original" = "$release_target" ] &&
                [ "$(sha "$ROOT/MiSTer_gui")" = "$(sha "$BACKUP/MiSTer_gui")" ] &&
                [ "$(sha "$ROOT/mister-gui")" = "$(sha "$BACKUP/mister-gui")" ] &&
                [ "$(sha "$ROOT/$release_target")" = "$(sha "$BACKUP/library.so")" ] ||
                fail "release differs from existing backup $BACKUP"
            # Only the pin mode changed; the active release is byte-identical, so the
            # old backup is replaceable before any process is stopped or marker is set.
            if [ "$saved_target" != "$backup_target" ]; then
                rm -rf "$BACKUP" || fail 'cannot replace backup for changed pin mode'
            fi
        fi
        if [ ! -e "$BACKUP" ]; then
            pending=$(mktemp -d "$ROOT/.dev-backup.new-XXXXXX") || fail 'cannot stage backup'
            printf '%s\n' "$backup_target" > "$pending/target.txt"
            cp "$ROOT/MiSTer_gui" "$pending/MiSTer_gui"
            cp "$ROOT/mister-gui" "$pending/mister-gui"
            cp "$ROOT/$release_target" "$pending/library.so"
            (cd "$pending" && sha256sum target.txt mister-gui MiSTer_gui library.so > backup.sha256)
            mv "$pending" "$BACKUP" || fail 'cannot publish release backup'
            check_backup
        fi
        slots=
    fi
    slot=
    n=65535
    while [ "$n" -ge 0 ]; do
        candidate="mister-pats-gui-65535.65535.$n.so"
        if [ ! -e "$ROOT/$candidate" ] && [ ! -L "$ROOT/$candidate" ]; then slot=$candidate; break; fi
        n=$((n-1))
    done
    [ -n "$slot" ] || fail 'no free development library slot'
    slots=${slots:+$slots,}$slot
    printf 'version=%s-dev\nlibrary=%s\nbackup=.dev-backup\nslots=%s\n' "$VERSION" "$slot" "$slots" > "$MARKER.new-$$"
    mv -f "$MARKER.new-$$" "$MARKER" || fail 'cannot publish dev marker'
else
    [ -r "$MARKER" ] || fail 'no development build to restore'
    check_backup
    [ -f "$MARKER" ] && [ ! -L "$MARKER" ] &&
        [ "$(wc -l < "$MARKER")" -eq 4 ] || fail 'invalid dev marker'
    [ "$(sed -n '3p' "$MARKER")" = 'backup=.dev-backup' ] || fail 'invalid dev marker'
    slots=$(sed -n '4s/^slots=\(.*\)$/\1/p' "$MARKER")
    valid_slots "$slots" || fail 'invalid dev marker slots'
    current=$(sed -n '2s/^library=\(.*\)$/\1/p' "$MARKER")
    valid_name "$current" || fail 'invalid active dev library in marker'
    active=$(readlink "$LINK") || fail 'missing active library link'
    [ "$active" = "$current" ] || [ "$active" = "$original" ] || fail 'active library differs from marker and saved release'
fi
: > /tmp/mister-pat-suspend-gui
launcher_pids=$(ps -o pid,comm,args | awk '$2=="script" && $0 ~ /\/tmp\/script/ {print $1}')
for pid in $launcher_pids; do kill -KILL "$pid" 2>/dev/null || :; done
killall -TERM mister-gui 2>/dev/null || :
killall -TERM MiSTer_gui 2>/dev/null || :
running() { ps -o comm | awk '$1=="mister-gui" || $1=="MiSTer_gui" {found=1} END {exit !found}'; }
i=0
while running && [ "$i" -lt 20 ]; do sleep 1; i=$((i+1)); done
if running; then killall -KILL mister-gui MiSTer_gui 2>/dev/null || :; sleep 1; fi
running && fail 'GUI or main process is still running'
COMMIT=1
if [ "$MODE" = deploy ]; then
    copy_verified "$STAGE/library.so" "$ROOT/$slot" "$H_GUI"
    copy_verified "$STAGE/MiSTer_gui" "$ROOT/MiSTer_gui" "$H_MAIN"
    copy_verified "$STAGE/mister-gui" "$ROOT/mister-gui" "$H_LOADER"
    sync
    rm -f "$LINK.new-$$"
    ln -s "$slot" "$LINK.new-$$" || fail 'cannot stage dev link'
    mv -f "$LINK.new-$$" "$LINK" || fail 'cannot activate dev library'
    sync
else
    restore_release
    oldifs=$IFS; IFS=,
    for slot in $slots; do valid_name "$slot" || fail 'unsafe dev slot'; rm -f "$ROOT/$slot" || fail "cannot remove $slot"; done
    IFS=$oldifs
    rm -f "$MARKER" || fail 'cannot clear dev marker'
    sync
fi
SUCCESS=1
echo 'dev deploy transaction complete'
