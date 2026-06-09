set -e
TC=~/NextOS-Elite-Edition/build.NextOS-Retro-Elite-Edition-Amlogic-old.aarch64-4/toolchain
CC=$TC/bin/aarch64-libreelec-linux-gnu-gcc
SR=$TC/aarch64-libreelec-linux-gnu/sysroot
cd "$(dirname "$0")"
[ -x "$CC" ] || { echo "toolchain nao encontrado: $CC"; exit 1; }
SRCS="src/main.c src/so_util.c src/jni_shim.c src/imports.gen.c src/egl_shim.c src/opensles_shim.c src/android_shim.c src/util.c src/error.c src/shims.c src/sonic_jni.c src/jni_log.c src/gl_trace.c src/pthread_bridge.c"
$CC --sysroot="$SR" -I src -O2 -fPIC -fno-stack-protector \
    -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable \
    -o sonicmania $SRCS \
    -lSDL2 -lEGL -lGLESv2 -ldl -lm -lpthread
echo "BUILD OK -> $(file sonicmania | cut -d, -f1-3)"
