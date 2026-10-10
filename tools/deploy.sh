#!/bin/sh
# Deploy the current GUI build to MiSTer.
set -eu
[ "$#" -eq 0 ] || { echo 'deploy.sh does not accept options' >&2; exit 2; }
HERE=$(cd "$(dirname "$0")/.." && pwd)
DEVICE=${DEVICE:-root@192.168.64.163}
REMOTE=${REMOTE:-/media/fat/mister-pat}
case "$REMOTE" in /media/fat/*) :;; *) echo 'REMOTE must be under /media/fat' >&2; exit 2;; esac
case "$REMOTE" in *[!A-Za-z0-9_./-]*) echo 'unsafe REMOTE' >&2; exit 2;; esac
SSH_OPTS=${SSH_OPTS:--o BatchMode=yes}
stage=
cleanup_upload() {
    [ -z "$stage" ] || ssh $SSH_OPTS "$DEVICE" "rm -rf '$stage'" 2>/dev/null || :
}
trap cleanup_upload EXIT HUP INT TERM
VERSION=$(sed -n 's/^VERSION[[:space:]]*:=[[:space:]]*//p' "$HERE/release.mk")
printf '%s\n' "$VERSION" | awk -F. 'NF!=3 {exit 1} {for(i=1;i<=3;i++) if($i !~ /^(0|[1-9][0-9]*)$/ || $i+0>65535) exit 1}' || { echo 'invalid release.mk version' >&2; exit 1; }
LIB="mister-pats-gui-$VERSION.so"
LOADER="$HERE/build/mister-gui"
GUI="$HERE/build/$LIB"
MAIN="$HERE/third_party/Main_MiSTer/bin/MiSTer"
elf() { [ "$(od -An -tx1 -N4 "$1" | tr -d ' \n')" = 7f454c46 ]; }
for file in "$LOADER" "$GUI" "$MAIN"; do
    [ -s "$file" ] && elf "$file" || { echo "invalid ELF: $file" >&2; exit 1; }
done
${CROSS:-arm-unknown-linux-gnueabihf}-nm -D "$GUI" | grep -Eq '[[:space:]]mister_gui_main_v1$' || { echo 'missing GUI entry point' >&2; exit 1; }
strings "$GUI" | grep -Fq "$VERSION" || { echo 'GUI build has a different version' >&2; exit 1; }
strings "$MAIN" | grep -Fq '/media/fat/mister-pat/logs/loader.log' || { echo 'main binary lacks loader error handling' >&2; exit 1; }
h_loader=$(sha256sum "$LOADER" | awk '{print $1}')
h_gui=$(sha256sum "$GUI" | awk '{print $1}')
h_main=$(sha256sum "$MAIN" | awk '{print $1}')
stage=$(ssh $SSH_OPTS "$DEVICE" "umask 077; mkdir -p '$REMOTE/.deploy-staging'; mktemp -d '$REMOTE/.deploy-staging/run-XXXXXX'")
case "$stage" in "$REMOTE"/.deploy-staging/run-*) :;; *) echo 'unsafe remote stage' >&2; exit 1;; esac
case "$stage" in *[!A-Za-z0-9_./-]*) echo 'unsafe remote stage characters' >&2; exit 1;; esac
scp $SSH_OPTS -q "$LOADER" "$DEVICE:$stage/mister-gui"
scp $SSH_OPTS -q "$GUI" "$DEVICE:$stage/library.so"
scp $SSH_OPTS -q "$MAIN" "$DEVICE:$stage/MiSTer_gui"
old_boot=$(ssh $SSH_OPTS "$DEVICE" 'cat /proc/sys/kernel/random/boot_id')
if ! ssh $SSH_OPTS "$DEVICE" /bin/sh -s -- "$REMOTE" "$stage" "$VERSION" "$h_loader" "$h_gui" "$h_main" <<'REMOTE_SCRIPT'
set -eu
ROOT=$1; STAGE=$2; VERSION=$3; H_LOADER=$4; H_GUI=$5; H_MAIN=$6
LOCK=$ROOT/.update-lock
fail() { echo "deploy: $*" >&2; exit 1; }
sha() { sha256sum "$1" | awk '{print $1}'; }
elf() { [ "$(od -An -tx1 -N4 "$1" | tr -d ' \n')" = 7f454c46 ]; }
cleanup() {
    status=$?
    rm -rf "$STAGE"
    if [ "${OWN_LOCK:-0}" -eq 1 ]; then rm -f "$LOCK/owner"; rmdir "$LOCK" || :; fi
    exit "$status"
}
trap cleanup EXIT
[ -d "$ROOT" ] && [ ! -L "$ROOT" ] || fail 'invalid installation directory'
[ -d "$STAGE" ] && [ ! -L "$STAGE" ] || fail 'invalid staging directory'
mkdir "$LOCK" 2>/dev/null || fail 'an update is already running'
OWN_LOCK=1
printf 'pid=%s\n' "$$" > "$LOCK/owner"
for pair in "mister-gui:$H_LOADER" "library.so:$H_GUI" "MiSTer_gui:$H_MAIN"; do
    name=${pair%%:*}; hash=${pair#*:}
    case "$hash" in *[!0-9a-f]*|'') fail 'invalid transfer hash';; esac
    [ "${#hash}" -eq 64 ] && [ -f "$STAGE/$name" ] && [ ! -L "$STAGE/$name" ] && elf "$STAGE/$name" && [ "$(sha "$STAGE/$name")" = "$hash" ] || fail "invalid staged $name"
done
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
install_file() {
    src=$1; dest=$2; hash=$3
    cp "$src" "$dest.new-$$" || fail "copy failed: $dest"
    chmod 755 "$dest.new-$$"
    [ "$(sha "$dest.new-$$")" = "$hash" ] || fail "copy verification failed: $dest"
    mv -f "$dest.new-$$" "$dest" || fail "install failed: $dest"
}
install_file "$STAGE/library.so" "$ROOT/mister-pats-gui-$VERSION.so" "$H_GUI"
install_file "$STAGE/MiSTer_gui" "$ROOT/MiSTer_gui" "$H_MAIN"
install_file "$STAGE/mister-gui" "$ROOT/mister-gui" "$H_LOADER"
rm -f "$ROOT/mister-pats-gui.so.new-$$"
ln -s "mister-pats-gui-$VERSION.so" "$ROOT/mister-pats-gui.so.new-$$"
mv -f "$ROOT/mister-pats-gui.so.new-$$" "$ROOT/mister-pats-gui.so"
# Remove leftovers from the retired development deployment mechanism.
rm -f "$ROOT/.development-build"
rm -rf "$ROOT/.dev-backup"
rm -f "$ROOT"/mister-pats-gui-*-dev-*.so "$ROOT"/mister-pats-gui-65535.65535.*.so
sync
rm -rf "$STAGE"
echo 'Remote files committed.'
REMOTE_SCRIPT
then
    echo 'remote deploy failed' >&2
    exit 1
fi
stage=
echo 'Remote files committed; rebooting MiSTer.'
ssh $SSH_OPTS "$DEVICE" reboot 2>/dev/null || :
n=0
until new_boot=$(ssh $SSH_OPTS -o ConnectTimeout=5 "$DEVICE" 'cat /proc/sys/kernel/random/boot_id' 2>/dev/null) &&
      [ -n "$new_boot" ] && [ "$new_boot" != "$old_boot" ]; do
    n=$((n+1))
    [ "$n" -lt 40 ] || { echo 'files committed, but MiSTer did not reconnect' >&2; exit 1; }
    sleep 5
done
echo 'MiSTer is back.'
