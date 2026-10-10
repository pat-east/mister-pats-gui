#!/bin/sh
# Build the five immutable assets for one stable GitHub release.
set -eu
cd "$(dirname "$0")/.."
VERSION=$(sed -n 's/^VERSION[[:space:]]*:=[[:space:]]*//p' release.mk)
GAMESDB_FORMAT=$(sed -n 's/^GAMESDB_FORMAT[[:space:]]*:=[[:space:]]*//p' release.mk)
case "$VERSION" in
    *-dev*|*+*|*/*|*' '*) echo 'invalid release version' >&2; exit 1 ;;
esac
printf '%s\n' "$VERSION" | awk -F. 'NF!=3 {exit 1} {for(i=1;i<=3;i++) if($i !~ /^(0|[1-9][0-9]*)$/ || $i+0>65535) exit 1}' || exit 1
case "$GAMESDB_FORMAT" in ''|*[!0-9]*) echo 'invalid database format' >&2; exit 1;; esac
[ "$GAMESDB_FORMAT" = 0 ] || case "$GAMESDB_FORMAT" in 0*) echo 'leading zero in database format' >&2; exit 1;; esac
[ "$GAMESDB_FORMAT" -le 65535 ] || exit 1
LIB="mister-pats-gui-$VERSION.so"
MAIN=third_party/Main_MiSTer/bin/MiSTer
for file in build/mister-gui "build/$LIB" "$MAIN"; do
    [ -s "$file" ] || { echo "missing $file" >&2; exit 1; }
    arm-unknown-linux-gnueabihf-readelf -h "$file" | grep -q 'Machine:.*ARM' || exit 1
    arm-unknown-linux-gnueabihf-readelf -h "$file" | grep -q 'Flags:.*hard-float ABI' || exit 1
    arm-unknown-linux-gnueabihf-readelf -V "$file" | awk '/Name: GLIBC_/{for(i=1;i<=NF;i++) if($i=="Name:"){split($(i+1),v,"_"); split(v[2],n,"."); if(n[1]>2 || (n[1]==2 && n[2]>31)) exit 1}} /Name: GLIBCXX_/{for(i=1;i<=NF;i++) if($i=="Name:"){split($(i+1),v,"_"); split(v[2],n,"."); if(n[1]>3 || (n[1]==3 && (n[2]>4 || (n[2]==4 && n[3]>28)))) exit 1}}' || exit 1
done
strings "$MAIN" | grep -Fq '/media/fat/mister-pat/logs/loader.log' || {
    echo 'patched MiSTer_gui has not been rebuilt with loader error handling' >&2; exit 1;
}
arm-unknown-linux-gnueabihf-nm -D "build/$LIB" | grep -Eq '[[:space:]]mister_gui_main_v1$' || { echo 'missing GUI entry point' >&2; exit 1; }
if arm-unknown-linux-gnueabihf-readelf -d "build/$LIB" | grep -q 'Shared library: \[libstdc++'; then
    echo 'libstdc++ must not be a dynamic dependency' >&2; exit 1
fi
OUT="${1:-build/release-package/v$VERSION}"
[ ! -e "$OUT" ] || { echo "output already exists: $OUT" >&2; exit 1; }
mkdir -p "$OUT"
cp build/mister-gui "$OUT/mister-gui"
cp "build/$LIB" "$OUT/$LIB"
cp "$MAIN" "$OUT/MiSTer_gui"
printf 'version=%s\nloader_abi=1\ngamesdb_format=%s\n' "$VERSION" "$GAMESDB_FORMAT" > "$OUT/release-info.txt"
(cd "$OUT" && sha256sum mister-gui MiSTer_gui "$LIB" release-info.txt > SHA256SUMS)
printf 'Release package: %s\n' "$OUT"
