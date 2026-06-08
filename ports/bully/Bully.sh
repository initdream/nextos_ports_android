#!/bin/bash
# Bully: Anniversary Edition (Android so-loader) -- NextOS / PortMaster
# Amlogic-old (S905X, Mali-450 Utgard, fbdev). Controle NATIVO (SDL_GameController).
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

# swap (device de 832MB -- só segurança; o jogo NAO e OOM-pesado)
$ESUDO swapon "$GAMEDIR/bully.swap" 2>/dev/null
$ESUDO swapon /storage/roms/gtavc.swap 2>/dev/null
$ESUDO sysctl -w vm.swappiness=80 >/dev/null 2>&1
$ESUDO chmod 666 /dev/uinput 2>/dev/null

# libs do device (SDL2 / GLESv2 / EGL / openal / mpg123) + libs do jogo no GAMEDIR
export LD_LIBRARY_PATH="/usr/lib:$GAMEDIR:$LD_LIBRARY_PATH"
# controle: o jogo le o SDL_GameController NATIVO (sem gptokeyb -> nao conflita)
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
# video: driver "mali" do SDL2 (EGL fbdev correto -> render na TV) + fullscreen
export SDL_VIDEODRIVER=mali
export SDL2COMPAT_FORCE_FULLSCREEN_DESKTOP=1
export SDL_VIDEO_FULLSCREEN_DESKTOP=1
# Mali-450 Utgard: texturas leves (limite de memoria de textura da GPU na escola)
export BULLY_TEX_LIGHT=1
export BULLY_TEX_HALF=1

$ESUDO chmod +x "$GAMEDIR/bully"
pm_platform_helper "$GAMEDIR/bully"
./bully

pm_finish
