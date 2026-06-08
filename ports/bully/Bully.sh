#!/bin/bash
# Bully: Anniversary Edition -- Android so-loader -> NextOS / PortMaster
# Porter: felc18-blip. INEDITO em aarch64/Linux/PortMaster (Mali-450 testado).
# BYO-DATA: voce precisa do APK do Bully 1.4.311 (sua copia legal). Veja README.md.
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/bully"
cd "$GAMEDIR"
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

# ---------- BYO-DATA: extrai do APK do usuario na 1a vez ----------
# Coloque seu APK (Bully 1.4.311) em $GAMEDIR/ e abra o jogo: ele extrai sozinho.
if [ ! -f "$GAMEDIR/libGame.so" ] || [ ! -f "$GAMEDIR/assets/data_0.zip" ]; then
  APK=$(ls "$GAMEDIR"/*.apk "$GAMEDIR"/*.APK 2>/dev/null | head -1)
  if [ -n "$APK" ]; then
    echo "Extraindo os dados do seu APK (2.9GB, pode demorar minutos na 1a vez)..."
    mkdir -p "$GAMEDIR/assets"
    $ESUDO unzip -o -j "$APK" "lib/arm64-v8a/libGame.so" "lib/arm64-v8a/libc++_shared.so" -d "$GAMEDIR" 2>/dev/null
    $ESUDO unzip -o -j "$APK" "assets/data_0.zip" "assets/data_1.zip" "assets/data_2.zip" "assets/data_3.zip" "assets/data_4.zip" \
      "assets/data_0.zip.idx" "assets/data_1.zip.idx" "assets/data_2.zip.idx" "assets/data_3.zip.idx" "assets/data_4.zip.idx" \
      -d "$GAMEDIR/assets" 2>/dev/null
    sync
  fi
fi
if [ ! -f "$GAMEDIR/libGame.so" ] || [ ! -f "$GAMEDIR/assets/data_0.zip" ]; then
  echo "############################################################"
  echo " FALTAM OS DADOS DO JOGO (BYO-data)."
  echo " 1) Tenha o APK do Bully: Anniversary Edition v1.4.311"
  echo "    (sua copia legal). 2) Copie-o para:"
  echo "      $GAMEDIR/"
  echo " 3) Abra 'Bully' de novo: ele extrai e roda sozinho."
  echo " Detalhes: $GAMEDIR/README.md"
  echo "############################################################"
  sleep 15
  pm_finish
  exit 1
fi

# ---------- ambiente ----------
export LD_LIBRARY_PATH="/usr/lib:$GAMEDIR:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export SDL_VIDEODRIVER=mali               # EGL fbdev correto -> render na TV
export SDL2COMPAT_FORCE_FULLSCREEN_DESKTOP=1
export SDL_VIDEO_FULLSCREEN_DESKTOP=1
export BULLY_TEX_LIGHT=1                   # Mali-450: pula mapas _n/_s
export BULLY_TEX_HALF=1                    # Mali-450: pula mipmaps + tex>=512 pela metade

$ESUDO chmod +x "$GAMEDIR/bully"

# gptokeyb2 SO p/ o hotkey de SAIR (o jogo le o controle NATIVO via SDL; sem .gptk
# = nao mapeia/conflita). SELECT+START (segurado) mata o jogo.
$GPTOKEYB2 "bully" &

pm_platform_helper "$GAMEDIR/bully"
./bully

pm_finish
