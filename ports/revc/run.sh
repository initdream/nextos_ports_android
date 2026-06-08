#!/bin/bash
# reVC (GTA Vice City Android so-loader) — Mali-450
for cf in /opt/system/Tools/PortMaster /opt/tools/PortMaster /storage/roms/ports/PortMaster /roms/ports/PortMaster; do
  [ -d "$cf" ] && controlfolder="$cf" && break
done
source "$controlfolder/control.txt"
get_controls

GAMEDIR="/storage/roms/ports/revc"
cd "$GAMEDIR"
$ESUDO chmod 666 /dev/uinput 2>/dev/null
mkdir -p /storage/emulated/0 2>/dev/null
ln -sfn "$GAMEDIR/gamedata" /storage/emulated/0/reVC 2>/dev/null

export SDL2COMPAT_FORCE_FULLSCREEN_DESKTOP=1
export SDL_VIDEO_FULLSCREEN_DESKTOP=1
export SDL_VIDEODRIVER=mali
export SDL_GAMECONTROLLERCONFIG_FILE="$GAMEDIR/gamedata/gamecontrollerdb.txt"
[ -n "$sdl_controllerconfig" ] && export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

./revc 2>&1 | tee run.log
$ESUDO systemctl restart oga_events 2>/dev/null &
