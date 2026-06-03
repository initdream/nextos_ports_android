#!/usr/bin/env bash
#
# build-port.sh — compila um port com o toolchain NextOS/Elite (Mali, aarch64).
#
# Uso:
#   tools/build-port.sh <port-dir> [gles2]
#     <port-dir>  pasta do port (com CMakeLists.txt + src/)
#     gles2       (opcional) linka GLESv2 em vez de GLESv1_CM
#
# Variável de ambiente:
#   NEXTOS_TOOLCHAIN  caminho do toolchain (default = Amlogic-old Elite)
#
# Encapsula o setup que validamos à mão:
#   - CMAKE_TOOLCHAIN_FILE = a conf aarch64-libreelec do toolchain
#   - PKG_CONFIG apontando pro sysroot aarch64 (acha sdl2.pc)
#   - GLES1 = libGLESv1_CM (nome NextOS, não "GLES_CM" do Trimui)
#
# Saída: <port-dir>/build/<nome-do-port>  (ELF aarch64, roda no device Mali)
#
set -euo pipefail

NEXTOS_TOOLCHAIN="${NEXTOS_TOOLCHAIN:-/home/runner/NextOS-Elite-Edition/build.NextOS-Retro-Elite-Edition-Amlogic-old.aarch64-4/toolchain}"
PORT="${1:?uso: build-port.sh <port-dir> [gles2]}"
GLES2="OFF"; [ "${2:-}" = "gles2" ] && GLES2="ON"

[ -f "$PORT/CMakeLists.txt" ] || { echo "ERRO: sem CMakeLists.txt em $PORT" >&2; exit 1; }
CONF="$NEXTOS_TOOLCHAIN/etc/cmake-aarch64-libreelec-linux-gnu.conf"
SR="$NEXTOS_TOOLCHAIN/aarch64-libreelec-linux-gnu/sysroot"
[ -f "$CONF" ] || { echo "ERRO: toolchain conf nao encontrada: $CONF" >&2; exit 1; }

export PKG_CONFIG_LIBDIR="$SR/usr/lib/pkgconfig:$SR/usr/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SR"

echo ">> toolchain: $NEXTOS_TOOLCHAIN"
echo ">> port:      $PORT   (GLES2=$GLES2)"
cmake -DCMAKE_TOOLCHAIN_FILE="$CONF" -DCMAKE_BUILD_TYPE=Release \
      -DUSE_GLES2="$GLES2" -B "$PORT/build" -S "$PORT"
cmake --build "$PORT/build" -j"$(nproc)"

BIN="$(find "$PORT/build" -maxdepth 1 -type f -executable | head -1)"
echo
echo "==> binario: $BIN"
file "$BIN" 2>/dev/null | sed 's/,.*aarch64/  (aarch64)/' | head -1
echo "    NEEDED: $(readelf -dW "$BIN" 2>/dev/null | awk '/NEEDED/{gsub(/[][]/,"");printf "%s ",$NF}')"
echo ">> copie pro device junto com o .so do jogo + assets, e rode ./launch.sh"
