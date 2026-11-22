#!/bin/sh
# Builds a minimal, decoder-only static FFmpeg for the MiSTer's ARM target (the video previews).
# Result lands in third_party/sysroot next to the other libraries (libavformat, libavcodec,
# libswscale, libswresample, libavutil) and is also exported to third_party/ffmpeg-prebuilt/,
# which is kept in Git. A normal `make deps` does not run this script: it installs the prebuilt
# files (third_party/install-ffmpeg.sh). Run this script only to change the FFmpeg version or its
# configuration, then commit the refreshed ffmpeg-prebuilt/ directory.
#
# What is built, and why so little: the GUI only needs to read an MP4 file, decode H.264 video
# and AAC audio, and convert the pixels. Everything else is switched off, which keeps the
# library small (it ships inside the GUI library) and the build short. No GPL or non-free parts
# are enabled, so the result is LGPL.
set -e

HOST=arm-unknown-linux-gnueabihf
FFMPEG=ffmpeg-8.0
# Pinned. The hash was computed when the archive was first fetched from ffmpeg.org; it detects a
# changed or damaged file, it is not an upstream-published checksum.
FFMPEG_SHA256=b2751fccb6cc4c77708113cd78b561059b6fa904b24162fa0be2d60273d27b8e

HERE=$(cd "$(dirname "$0")" && pwd)
PREFIX="$HERE/sysroot"
BUILD="$HERE/build"

# The source archive is not kept in the repository (11 MB). Rebuilding is rare, so it is fetched
# on demand into the ignored build directory and checked against the pinned hash. The libraries
# built from it are kept in third_party/ffmpeg-prebuilt/ (see its README) and are what a normal
# `make deps` installs.
mkdir -p "$BUILD"
ARCHIVE="$BUILD/$FFMPEG.tar.xz"
if [ ! -f "$ARCHIVE" ]; then
    echo "fetching $FFMPEG.tar.xz from ffmpeg.org"
    curl -fsSL -o "$ARCHIVE.part" "https://ffmpeg.org/releases/$FFMPEG.tar.xz"
    mv "$ARCHIVE.part" "$ARCHIVE"
fi
if command -v sha256sum >/dev/null 2>&1; then
    ACTUAL=$(sha256sum "$ARCHIVE" | awk '{print $1}')
else
    ACTUAL=$(shasum -a 256 "$ARCHIVE" | awk '{print $1}')
fi
[ "$ACTUAL" = "$FFMPEG_SHA256" ] || { echo "$FFMPEG.tar.xz does not match its pinned SHA-256" >&2; exit 1; }

mkdir -p "$PREFIX" "$BUILD"
cd "$BUILD"

echo "=== ffmpeg ==="
rm -rf "$FFMPEG"
tar xf "$ARCHIVE"
cd "$FFMPEG"

# -fPIC: the libraries are linked into the GUI's shared library. No -mthumb: FFmpeg's NEON
# assembly is ARM code. --enable-neon with --cpu=cortex-a9 keeps the hand-written H.264 paths
# (inverse transforms, deblocking, motion compensation); a configuration that drops them would
# make any benchmark misleading, so the summary below is checked after configure.
./configure \
    --prefix="$PREFIX" \
    --enable-cross-compile --cross-prefix="$HOST-" \
    --arch=arm --cpu=cortex-a9 --target-os=linux \
    --enable-pic --enable-static --disable-shared \
    --disable-everything --disable-autodetect --disable-network \
    --disable-programs --disable-doc --disable-debug \
    --disable-avdevice --disable-avfilter \
    --enable-avformat --enable-avcodec --enable-swscale --enable-swresample \
    --enable-protocol=file \
    --enable-demuxer=mov \
    --enable-parser=h264 --enable-parser=aac \
    --enable-decoder=h264 --enable-decoder=aac \
    --enable-asm --enable-neon \
    --extra-cflags="-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -O2 -fPIC"

grep -E '^#define HAVE_NEON 1$' config.h >/dev/null || { echo "NEON is not enabled in this FFmpeg build" >&2; exit 1; }
grep -E '^#define CONFIG_H264_DECODER 1$' config_components.h >/dev/null || { echo "H.264 decoder missing" >&2; exit 1; }

make -j4
make install

echo
echo "=== ffmpeg installed ==="
ls -la "$PREFIX/lib"/libav*.a "$PREFIX/lib"/libsw*.a

# Export what the GUI build needs, so the source archive does not have to live in the repository.
EXPORT="$HERE/ffmpeg-prebuilt"
rm -rf "$EXPORT"
mkdir -p "$EXPORT/lib" "$EXPORT/include"
cp "$PREFIX"/lib/libavcodec.a "$PREFIX"/lib/libavformat.a "$PREFIX"/lib/libavutil.a \
   "$PREFIX"/lib/libswscale.a "$PREFIX"/lib/libswresample.a "$EXPORT/lib/"
for dir in libavcodec libavformat libavutil libswscale libswresample; do
    cp -R "$PREFIX/include/$dir" "$EXPORT/include/"
done
cp COPYING.LGPLv2.1 LICENSE.md "$EXPORT/"
# Record exactly how these files were made.
{
    echo "FFmpeg $FFMPEG, built $(date -u +%Y-%m-%d), source SHA-256 $FFMPEG_SHA256"
    echo "configure: $(grep -m1 '^configure ' ffbuild/config.log 2>/dev/null || echo 'see build-ffmpeg.sh')"
} > "$EXPORT/BUILD-INFO.txt"
echo "exported to $EXPORT"
du -sh "$EXPORT"
