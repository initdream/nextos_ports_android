#!/bin/bash
# Bully: Anniversary Edition -- Android so-loader -> NextOS / PortMaster
# Porter: felc18-blip. Backend-agnostico (fbdev Mali-450 + KMSDRM/wayland Mali novo).
# BYO-DATA: requer o APK do Bully 1.4.311 (sua copia legal). Veja README.md.
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

# Acha a instalacao do PortMaster que REALMENTE tem control.txt (alguns devices tem
# uma pasta /storage/.config/PortMaster incompleta + a completa em /roms/ports/PortMaster).
controlfolder="/roms/ports/PortMaster"
for d in /opt/system/Tools/PortMaster /opt/tools/PortMaster "$XDG_DATA_HOME/PortMaster" /roms/ports/PortMaster /storage/.config/PortMaster; do
  [ -f "$d/control.txt" ] && controlfolder="$d" && break
done

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
export SDL2COMPAT_FORCE_FULLSCREEN_DESKTOP=1
export SDL_VIDEO_FULLSCREEN_DESKTOP=1
# Backend de display ADAPTATIVO (compat fbdev + kmsdrm/wayland). Em devices KMSDRM
# o pm_platform_helper ja forca SDL_VIDEODRIVER=kmsdrm; aqui garante o fbdev (Mali-450).
if [ -e /dev/dri/card0 ]; then
  export SDL_VIDEODRIVER=kmsdrm            # device com DRM/KMS (Mali-G310 Valhall, kernel mainline)
else
  export SDL_VIDEODRIVER=mali              # EGL fbdev (Amlogic-old Mali-450, kernel 3.14)
  export BULLY_TEX_LIGHT=1                 # Mali-450: pula mapas _n/_s
  export BULLY_TEX_HALF=1                  # Mali-450: pula mipmaps + tex>=512 pela metade
fi

$ESUDO chmod +x "$GAMEDIR/bully"

# Padrao PortMaster: gptokeyb2 p/ o hotkey de SAIR (SELECT+START). O jogo le o
# controle NATIVO via SDL. Prefere gptokeyb2 (PortMaster completo, ex: Amlogic-old);
# cai no gptokeyb, e por fim no gptokeyb do sistema (PortMaster parcial, ex: X5M).
if [ -n "$GPTOKEYB2" ] && [ -x "$controlfolder/gptokeyb2" ]; then
  $GPTOKEYB2 "bully" &
elif [ -n "$GPTOKEYB" ] && { set -- $GPTOKEYB; [ -x "$1" ]; }; then
  $GPTOKEYB "bully" &
elif command -v gptokeyb >/dev/null 2>&1; then
  gptokeyb -1 "bully" &
fi

command -v pm_platform_helper >/dev/null 2>&1 && pm_platform_helper "$GAMEDIR/bully"
./bully

pkill -f gptokeyb 2>/dev/null
command -v pm_finish >/dev/null 2>&1 && pm_finish
