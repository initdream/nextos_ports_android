#!/bin/bash
# build aarch64 do DYSMANTLE (Android, 10tons NX) so-loader — toolchain NextOS Amlogic-old.
# Loader ELF aarch64 glibc; SDL2/GLESv2/EGL = runtime (dlopen RTLD_GLOBAL).
set -e
TC=~/NextOS-Elite-Edition/build.NextOS-Retro-Elite-Edition-Amlogic-old.aarch64-4/toolchain
CC=$TC/bin/aarch64-libreelec-linux-gnu-gcc
SR=$TC/aarch64-libreelec-linux-gnu/sysroot
cd "$(dirname "$0")"

[ -x "$CC" ] || { echo "toolchain não encontrado: $CC"; exit 1; }

SRCS="src/main.c src/so_util.c src/imports.c src/pthread_bridge.c \
      src/egl_shim.c src/android_shim.c src/opensles_shim.c src/jni_shim.c \
      src/util.c src/error.c"

$CC --sysroot="$SR" -I src -O2 -fPIC \
    -Wno-unused-parameter -Wno-unused-function -Wno-comment \
    -Wl,--export-dynamic \
    -o dysmantle $SRCS \
    -lSDL2 -lEGL -lGLESv2 -ldl -lm -lpthread -lgcc

echo "BUILD OK -> $(file dysmantle | cut -d, -f1-3)"
