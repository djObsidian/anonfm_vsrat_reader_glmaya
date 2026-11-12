#!/bin/sh
# Build the reader inside the WSL with a locally built wolfSSL.
cd /mnt/d/workspace/perdolinque/qwenmaxxing/anonfm_vsrat_reader_glmaya || exit 1
mkdir -p build
cd build || exit 1
cmake -DWOLFSSL_ROOT="$HOME/wolfssl-install" .. "$@" || exit 1
make -j8 || exit 1
echo BUILD_OK
