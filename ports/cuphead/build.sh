#!/bin/bash
# build arm64 do Cuphead (Unity 2017.4 IL2CPP) so-loader — toolchain NextOS Amlogic-old.
set -e
TC=~/NextOS-Elite-Edition/build.NextOS-Retro-Elite-Edition-Amlogic-old.aarch64-4/toolchain
CC=$TC/bin/aarch64-libreelec-linux-gnu-gcc
SR=$TC/aarch64-libreelec-linux-gnu/sysroot
cd "$(dirname "$0")"
[ -x "$CC" ] || { echo "toolchain não encontrado: $CC"; exit 1; }
# inclui egl_shim (janela SDL2); exclui android_shim (NativeActivity) e opensles (audio=depois)
SRCS=$(ls src/*.c | grep -vE "android_shim|opensles_shim")
$CC --sysroot="$SR" -D_GNU_SOURCE -I src -I "$SR/usr/include" -O2 -fPIC -fno-omit-frame-pointer -rdynamic \
    -o cuphead $SRCS \
    -lSDL2 -ldl -lm -lpthread
echo "BUILD OK -> $(file cuphead | cut -d, -f1-3)"
