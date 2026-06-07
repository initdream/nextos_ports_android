#!/bin/bash
# build aarch64 do reVC (Android) so-loader — toolchain NextOS Amlogic-old.
# O loader é um ELF aarch64 glibc normal; SDL2/GLESv2/EGL/OpenAL/mpg123 são
# carregados em runtime (dlopen RTLD_GLOBAL), então só linkamos -ldl -lm -lpthread.
set -e
TC=~/NextOS-Elite-Edition/build.NextOS-Retro-Elite-Edition-Amlogic-old.aarch64-4/toolchain
CC=$TC/bin/aarch64-libreelec-linux-gnu-gcc
SR=$TC/aarch64-libreelec-linux-gnu/sysroot
cd "$(dirname "$0")"

[ -x "$CC" ] || { echo "toolchain não encontrado: $CC"; exit 1; }

SRCS="src/main.c src/so_util.c src/stubs.c src/pthread_bridge.c src/util.c src/error.c"

$CC --sysroot="$SR" -I src -O2 -fPIC \
    -o revc $SRCS \
    -ldl -lm -lpthread

echo "BUILD OK -> $(file revc | cut -d, -f1-3)"
