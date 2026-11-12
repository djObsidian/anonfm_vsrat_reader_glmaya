#!/bin/sh
# Cross-build wolfSSL 5.7.4 for Windows with the mingw-w64 compiler.
set -e
cd "$HOME" || exit 1
test -d wolfssl-5.7.4-stable || exit 1
mkdir -p wolfssl-build-win
cd wolfssl-build-win || exit 1
../wolfssl-5.7.4-stable/configure \
    CC=x86_64-w64-mingw32-gcc \
    --host=x86_64-w64-mingw32 \
    --prefix="$HOME/wolfssl-win-install" \
    --enable-static --disable-shared --disable-debug --disable-examples
make -j16
make install
echo "WIN_BUILD_OK"
