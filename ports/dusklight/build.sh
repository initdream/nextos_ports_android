#!/bin/bash
# build aarch64 do Dusklight (Twilight Princess/Aurora) so-loader — toolchain NextOS Amlogic-old.
# SDL3 é ESTÁTICO dentro do libmain.so; GLESv2/EGL/OpenSLES/z = dlopen/dlsym runtime.
set -e
TC=~/NextOS-Elite-Edition/build.NextOS-Retro-Elite-Edition-Amlogic-old.aarch64-4/toolchain
CC=$TC/bin/aarch64-libreelec-linux-gnu-gcc
SR=$TC/aarch64-libreelec-linux-gnu/sysroot
cd "$(dirname "$0")"
[ -x "$CC" ] || { echo "toolchain não encontrado: $CC"; exit 1; }
SRCS=$(ls src/*.c | grep -vE "android_shim|egl_shim")
$CC --sysroot="$SR" -D_GNU_SOURCE -I src -O2 -fPIC -fno-omit-frame-pointer -rdynamic \
    -o dusklight $SRCS \
    -ldl -lm -lpthread -l:libGLESv2.so.2 -l:libEGL.so.1
echo "BUILD OK -> $(file dusklight | cut -d, -f1-3)"
