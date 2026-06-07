#!/bin/bash
# reVC (GTA Vice City, Android so-loader) — Mali-450
if [ -d "/opt/system/Tools/PortMaster/" ]; then controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then controlfolder="/opt/tools/PortMaster"
elif [ -d "/storage/roms/ports/PortMaster" ]; then controlfolder="/storage/roms/ports/PortMaster"
else controlfolder="/roms/ports/PortMaster"; fi
source $controlfolder/control.txt
get_controls

GAMEDIR="/storage/roms/ports/revc"
cd "$GAMEDIR/gamedata"
$ESUDO chmod 666 /dev/uinput 2>/dev/null
# garante a pasta de dados que o reVC procura (hardcoded /storage/emulated/0/reVC)
mkdir -p /storage/emulated/0 2>/dev/null
ln -sfn "$GAMEDIR/gamedata" /storage/emulated/0/reVC 2>/dev/null

export SDL2COMPAT_FORCE_FULLSCREEN_DESKTOP=1
export SDL_VIDEO_FULLSCREEN_DESKTOP=1
export SDL_VIDEODRIVER=mali
# mapeamento do controle: o reVC já carrega o gamecontrollerdb, mas garantimos
# via env tb (o fix de SDL_InitSubSystem +JOYSTICK+GAMECONTROLLER está no binário)
export SDL_GAMECONTROLLERCONFIG_FILE="$GAMEDIR/gamedata/gamecontrollerdb.txt"
[ -n "$sdl_controllerconfig" ] && export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

/storage/roms/ports/revc/revc 2>./revc 2>&11 | tee run.log
$ESUDO systemctl restart oga_events 2>/dev/null &
