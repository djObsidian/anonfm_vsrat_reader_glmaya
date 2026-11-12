#!/bin/sh
# Cross-build the reader for Windows with mingw-w64.
cd /mnt/d/workspace/perdolinque/qwenmaxxing/anonfm_vsrat_reader_glmaya || exit 1
mkdir -p build-win
cd build-win || exit 1
cmake -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
      -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
      -DWOLFSSL_ROOT="$HOME/wolfssl-win-install" \
      -DCMAKE_EXE_LINKER_FLAGS="-static" \
      .. || exit 1
make -j8 || exit 1
echo CROSS_OK
