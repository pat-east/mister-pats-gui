#!/bin/sh
# Upload a development build, or restore the previously backed-up release.
set -eu
MODE=${1:-deploy}
stage=none
case "$MODE" in deploy|--restore-release) :;; *) echo "unknown option: $MODE" >&2; exit 2;; esac
[ "$#" -le 1 ] || { echo 'too many arguments' >&2; exit 2; }
HERE=$(cd "$(dirname "$0")/.." && pwd)
DEVICE=${DEVICE:-root@192.168.64.163}
REMOTE=${REMOTE:-/media/fat/mister-pat}
case "$REMOTE" in /media/fat/*) :;; *) echo 'REMOTE must be under /media/fat' >&2; exit 2;; esac
case "$REMOTE" in *[!A-Za-z0-9_./-]*) echo 'unsafe REMOTE' >&2; exit 2;; esac
SSH_OPTS=${SSH_OPTS:--o BatchMode=yes}
cleanup_upload() {
    [ "$stage" = none ] || ssh $SSH_OPTS "$DEVICE" "rm -rf '$stage'" 2>/dev/null || :
}
trap cleanup_upload EXIT
VERSION=$(sed -n 's/^VERSION[[:space:]]*:=[[:space:]]*//p' "$HERE/release.mk")
printf '%s\n' "$VERSION" | awk -F. 'NF!=3 {exit 1} {for(i=1;i<=3;i++) if($i !~ /^(0|[1-9][0-9]*)$/ || $i+0>65535) exit 1}' || { echo 'invalid release.mk version' >&2; exit 1; }
LIB="mister-pats-gui-$VERSION.so"
LOADER="$HERE/build/dev/mister-gui"
GUI="$HERE/build/dev/$LIB"
MAIN="$HERE/third_party/Main_MiSTer/bin/MiSTer"
elf() { [ "$(od -An -tx1 -N4 "$1" | tr -d ' \n')" = 7f454c46 ]; }
if [ "$MODE" = deploy ]; then
    for file in "$LOADER" "$GUI" "$MAIN"; do [ -s "$file" ] && elf "$file" || { echo "invalid ELF: $file" >&2; exit 1; }; done
    ${CROSS:-arm-unknown-linux-gnueabihf}-nm -D "$GUI" | grep -Eq '[[:space:]]mister_gui_main_v1$' || { echo 'missing GUI entry point' >&2; exit 1; }
    strings "$GUI" | grep -Fq "$VERSION-dev" || { echo 'GUI is not a development build' >&2; exit 1; }
    strings "$MAIN" | grep -Fq '/media/fat/mister-pat/logs/loader.log' || { echo 'main binary lacks loader error handling' >&2; exit 1; }
    h_loader=$(sha256sum "$LOADER" | awk '{print $1}')
    h_gui=$(sha256sum "$GUI" | awk '{print $1}')
    h_main=$(sha256sum "$MAIN" | awk '{print $1}')
    stage=$(ssh $SSH_OPTS "$DEVICE" "umask 077; mkdir -p '$REMOTE/.deploy-staging'; mktemp -d '$REMOTE/.deploy-staging/run-XXXXXX'") || exit 1
    case "$stage" in "$REMOTE"/.deploy-staging/run-*) :;; *) echo 'unsafe remote stage' >&2; exit 1;; esac
    case "$stage" in *[!A-Za-z0-9_./-]*) echo 'unsafe remote stage characters' >&2; exit 1;; esac
    scp $SSH_OPTS -q "$LOADER" "$DEVICE:$stage/mister-gui"
    scp $SSH_OPTS -q "$GUI" "$DEVICE:$stage/library.so"
    scp $SSH_OPTS -q "$MAIN" "$DEVICE:$stage/MiSTer_gui"
else
    stage=none; h_loader=none; h_gui=none; h_main=none
fi
if ! ssh $SSH_OPTS "$DEVICE" /bin/sh -s -- "$MODE" "$REMOTE" "$stage" "$VERSION" "$h_loader" "$h_gui" "$h_main" < "$HERE/tools/deploy-remote.sh"; then
    echo "remote deploy failed; inspect $REMOTE/.development-build and .dev-backup" >&2
    exit 1
fi
stage=none
# Commit is complete. A reconnect failure does not make the copied files invalid.
echo 'Remote files committed; rebooting MiSTer.'
old_boot=$(ssh $SSH_OPTS "$DEVICE" 'cat /proc/sys/kernel/random/boot_id') || {
    echo 'files committed, but cannot read the current boot ID' >&2; exit 1;
}
ssh $SSH_OPTS "$DEVICE" reboot 2>/dev/null || :
n=0
until new_boot=$(ssh $SSH_OPTS -o ConnectTimeout=5 "$DEVICE" 'cat /proc/sys/kernel/random/boot_id' 2>/dev/null) &&
      [ -n "$new_boot" ] && [ "$new_boot" != "$old_boot" ]; do
    n=$((n+1))
    [ "$n" -lt 40 ] || { echo 'files committed, but MiSTer did not reconnect' >&2; exit 1; }
    sleep 5
done
echo 'MiSTer is back.'
