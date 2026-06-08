#!/bin/bash
# build armhf do GTA VC so-loader (toolchain NextOS + sysroot armhf)
set -e
TC=~/NextOS-Elite-Edition/build.NextOS-Retro-Elite-Edition-Amlogic-old.aarch64-4/toolchain
CC=$TC/bin/armv8a-emuelec-linux-gnueabihf-gcc
SR=$TC/armv8a-emuelec-linux-gnueabihf/sysroot
cd "$(dirname "$0")"
# fontes do VC (sem android_shim/jni_shim/opensles — VC usa jni_patch + patches stub)
SRCS="src/main.c src/so_util.c src/jni_patch.c src/vc_impl.c src/pthread_fake.c src/patches.c src/crash.c src/imports.c src/egl_shim.c src/error.c src/util.c"
$CC --sysroot=$SR -I src -O2 -fPIC -marm \
    -o gtavc $SRCS \
    -lSDL2 -lGLESv2 -lEGL -lm -lpthread -ldl -lrt
echo "BUILD OK -> $(file gtavc | cut -d, -f1-2)"
