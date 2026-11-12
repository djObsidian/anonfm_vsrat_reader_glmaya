#!/bin/sh
# Build wolfSSL 5.7.4 statically into ~/wolfssl-install without root rights.
set -e
cd "$HOME"
test -f wolfssl-src.tar.gz || curl -sSL -o wolfssl-src.tar.gz https://github.com/wolfSSL/wolfssl/archive/refs/tags/v5.7.4-stable.tar.gz
rm -rf wolfssl-5.7.4-stable
tar xzf wolfssl-src.tar.gz
cd wolfssl-5.7.4-stable
./autogen.sh
./configure --prefix="$HOME/wolfssl-install" --enable-static --disable-shared --disable-debug
make -j4
make -j4
make install
echo "BUILD_DONE $(date)" >> "$HOME/wolfssl_build_done.log"
