#!/bin/sh
# Installs the prebuilt, decoder-only FFmpeg libraries and headers from third_party/ffmpeg-prebuilt
# into third_party/sysroot, where the Makefile looks for them. Called by third_party/build.sh.
# Nothing is compiled here; see third_party/ffmpeg-prebuilt/README.md.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/ffmpeg-prebuilt"
PREFIX="$HERE/sysroot"
[ -d "$SRC/lib" ] || { echo "missing $SRC (run third_party/build-ffmpeg.sh once)" >&2; exit 1; }
mkdir -p "$PREFIX/lib" "$PREFIX/include"
cp "$SRC"/lib/*.a "$PREFIX/lib/"
cp -R "$SRC"/include/* "$PREFIX/include/"
echo "=== ffmpeg: installed prebuilt libraries into $PREFIX ==="
ls -la "$PREFIX"/lib/libav*.a "$PREFIX"/lib/libsw*.a
