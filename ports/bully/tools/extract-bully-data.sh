#!/bin/bash
# extract-bully-data.sh -- extrai os dados do SEU APK do Bully p/ a pasta do port.
# Uso: ./extract-bully-data.sh /caminho/Bully_1.4.311.apk [/caminho/ports/bully]
# Precisa de: unzip. Rode no PC (rápido) ou no device.
set -e
APK="$1"
DEST="${2:-.}"

if [ -z "$APK" ] || [ ! -f "$APK" ]; then
  echo "uso: $0 <Bully_1.4.311.apk> [pasta_do_port]"
  echo "  pasta_do_port padrão = pasta atual ($DEST)"
  exit 1
fi

echo "==> APK:  $APK"
echo "==> dest: $DEST"
mkdir -p "$DEST/assets"

echo "[1/2] libs (libGame.so + libc++_shared.so)..."
unzip -o -j "$APK" "lib/arm64-v8a/libGame.so" "lib/arm64-v8a/libc++_shared.so" -d "$DEST"

echo "[2/2] dados (data_0-4.zip + .idx ~2.9GB, demora)..."
unzip -o -j "$APK" \
  "assets/data_0.zip" "assets/data_1.zip" "assets/data_2.zip" "assets/data_3.zip" "assets/data_4.zip" \
  "assets/data_0.zip.idx" "assets/data_1.zip.idx" "assets/data_2.zip.idx" "assets/data_3.zip.idx" "assets/data_4.zip.idx" \
  -d "$DEST/assets"

echo "OK! Conteúdo do port:"
ls -la "$DEST" | grep -iE "libGame|libc\+\+|bully"
ls "$DEST/assets" | head
echo "Pronto -- abra 'Bully' no menu."
