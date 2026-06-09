#!/bin/bash
# Build re4boot (so-loader Unity 2018 Mono ARM32 -> Mali-450). NAO versionar binario.
set -e
TC=$HOME/NextOS-Elite-Edition/build.NextOS-Retro-Elite-Edition-Amlogic-old.aarch64-4/toolchain
CC=$TC/bin/armv8a-emuelec-linux-gnueabihf-gcc
cd "$(dirname "$0")"
$CC -O2 -fPIC -fno-omit-frame-pointer -o re4boot \
  src/main_re4.c src/so_util.c src/util.c src/error.c \
  src/imports.gen.c src/pthread_shim.c src/jni_shim.c \
  src/egl_shim.c src/android_shim.c src/opensles_shim.c src/jni_idx_stubs.gen.c \
  src/softfp_shim.c \
  -Isrc -I$TC/armv8a-emuelec-linux-gnueabihf/sysroot/usr/include \
  -lSDL2 -ldl -lm -lpthread
echo "BUILD OK -> re4boot: $(file re4boot | cut -d, -f1-3)"
