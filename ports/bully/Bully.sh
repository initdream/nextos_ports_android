#!/bin/bash
# Bully: Anniversary Edition (Android so-loader) — Amlogic-old Mali-450.
# Starter de TESTE com log PERSISTENTE (sobrevive a travamento do device).
# Rodar em FOREGROUND: systemctl stop emustation && bash este.sh
GAMEDIR="/storage/roms/ports/bully"
LOG="$GAMEDIR/bully.log"
cd "$GAMEDIR"

# swap (segurança contra OOM no device 832MB)
swapon /storage/roms/gtavc.swap 2>/dev/null
sysctl -w vm.swappiness=80 >/dev/null 2>&1

chmod 666 /dev/uinput 2>/dev/null
export BULLY_TEX_LIGHT=1   # pula mapas _n/_s (detalhe)
export BULLY_TEX_HALF=1    # pula mipmaps + reduz texturas >=512 pela metade (memória de textura GPU)
# vídeo: SDL2 driver mali (faz o EGL fbdev certo -> render na TV)
export SDL_VIDEODRIVER=mali
export SDL2COMPAT_FORCE_FULLSCREEN_DESKTOP=1
export SDL_VIDEO_FULLSCREEN_DESKTOP=1
# DB de mapeamento de controle do sistema (pro SDL reconhecer o USB Gamepad)
for db in "$GAMEDIR/gamecontrollerdb.txt" /storage/roms/ports/PortMaster/gamecontrollerdb.txt /usr/share/gamecontrollerdb.txt; do
  [ -f "$db" ] && export SDL_GAMECONTROLLERCONFIG_FILE="$db" && break
done
[ -n "$sdl_controllerconfig" ] && export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

echo "=== Bully run $(date) | free: $(free -m | awk '/Mem/{print $7}')MB ===" > "$LOG"

# loop de sync + MONITOR de memória (sobrevive ao wedge -> confirma OOM na escola)
( while true; do echo "MEM=$(free -m|awk '/Mem/{print $3}') SWAP=$(free -m|awk '/Swap/{print $3}') livre=$(free -m|awk '/Mem/{print $4}')" >> "$GAMEDIR/mem.log"; sync; sleep 2; done ) &
SYNCPID=$!

# foreground + tee (line-buffered) -> log persistente + console
stdbuf -oL -eL ./bully 2>&1 | stdbuf -oL tee -a "$LOG"

kill "$SYNCPID" 2>/dev/null
sync
echo "=== fim (exit) $(date) ===" | tee -a "$LOG"
