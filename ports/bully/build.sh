#!/bin/bash
# build aarch64 do Bully (Android) so-loader — toolchain NextOS Amlogic-old.
# Loader ELF aarch64 glibc; SDL2/GLESv2/EGL/OpenAL/mpg123 = runtime (dlopen).
# Linkamos -lSDL2 (input no jni_shim) e -lEGL (egl_shim Mali fbdev).
set -e
TC=~/NextOS-Elite-Edition/build.NextOS-Retro-Elite-Edition-Amlogic-old.aarch64-4/toolchain
CC=$TC/bin/aarch64-libreelec-linux-gnu-gcc
SR=$TC/aarch64-libreelec-linux-gnu/sysroot
cd "$(dirname "$0")"

[ -x "$CC" ] || { echo "toolchain não encontrado: $CC"; exit 1; }

SRCS="src/main.c src/so_util.c src/jni_shim.c src/imports.c src/egl_shim.c \
      src/asset_archive.c src/zip_fs.c src/pthread_bridge.c src/util.c src/error.c"

$CC --sysroot="$SR" -I src -O2 -fPIC \
    -Wno-unused-parameter -Wno-unused-function \
    -o bully $SRCS \
    -lSDL2 -lEGL -ldl -lm -lpthread

echo "BUILD OK -> $(file bully | cut -d, -f1-3)"
