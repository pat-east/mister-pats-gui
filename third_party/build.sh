#!/bin/sh
# Builds zlib, libpng and freetype as static libraries for the MiSTer's ARM target.
# Result lands in third_party/sysroot; the app links against that and stays self-contained.
set -e

HOST=arm-unknown-linux-gnueabihf
ARCH_FLAGS="-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -O2"

HERE=$(cd "$(dirname "$0")" && pwd)
PREFIX="$HERE/sysroot"
BUILD="$HERE/build"

ZLIB=zlib-1.3.1
PNG=libpng-1.6.43
FREETYPE=freetype-2.13.2
JPEG=libjpeg-turbo-3.0.4

export CC="$HOST-gcc"
export CXX="$HOST-g++"
export AR="$HOST-ar"
export RANLIB="$HOST-ranlib"
export CFLAGS="$ARCH_FLAGS"
export CPPFLAGS="-I$PREFIX/include"
export LDFLAGS="-L$PREFIX/lib"

mkdir -p "$PREFIX" "$BUILD"
cd "$BUILD"

echo "=== zlib ==="
rm -rf "$ZLIB"
tar xf "$HERE/$ZLIB.tar.gz"
(cd "$ZLIB" && CHOST="$HOST" ./configure --prefix="$PREFIX" --static && make -j4 && make install)

echo "=== libpng ==="
rm -rf "$PNG"
tar xf "$HERE/$PNG.tar.gz"
(cd "$PNG" && ./configure --host="$HOST" --prefix="$PREFIX" \
    --disable-shared --enable-static --disable-dependency-tracking && \
    make -j4 && make install)

echo "=== freetype ==="
rm -rf "$FREETYPE"
tar xf "$HERE/$FREETYPE.tar.gz"
(cd "$FREETYPE" && ./configure --host="$HOST" --prefix="$PREFIX" \
    --disable-shared --enable-static --with-zlib=yes --with-png=no \
    --with-harfbuzz=no --with-brotli=no --with-bzip2=no && \
    make -j4 && make install)

echo "=== libjpeg-turbo ==="
# CMake rather than autotools, so it needs a toolchain file spelling out that we are not
# building for this machine. SIMD is on: the NEON paths are exactly what makes decoding a
# box art affordable on a Cortex-A9.
rm -rf "$JPEG"
tar xf "$HERE/$JPEG.tar.gz"
cat > "$BUILD/arm-toolchain.cmake" <<TOOLCHAIN
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER $HOST-gcc)
set(CMAKE_ASM_COMPILER $HOST-gcc)
set(CMAKE_C_FLAGS_INIT "$ARCH_FLAGS")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
TOOLCHAIN
(cd "$JPEG" && cmake -S . -B build-arm \
    -DCMAKE_TOOLCHAIN_FILE="$BUILD/arm-toolchain.cmake" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_SHARED=0 -DENABLE_STATIC=1 -DWITH_TURBOJPEG=0 -DWITH_SIMD=1 && \
    cmake --build build-arm -j4 && cmake --install build-arm)

echo
echo "=== installed ==="
ls -la "$PREFIX/lib"/*.a
