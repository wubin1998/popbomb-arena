#!/bin/bash
# PopBomb Arena — PortMaster launcher for R36S-class handhelds (RK3326).

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

if [ -f "$controlfolder/control.txt" ]; then
  source "$controlfolder/control.txt"
  [ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
  get_controls
fi

GAMEDIR="/$directory/ports/popbomb"
[ -d "$GAMEDIR" ] || GAMEDIR="$(cd "$(dirname "$0")" && pwd)/popbomb"
cd "$GAMEDIR" || exit 1

> "$GAMEDIR/log.txt"
exec > >(tee -a "$GAMEDIR/log.txt") 2>&1

export LD_LIBRARY_PATH="$GAMEDIR/libs.aarch64:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export POPBOMB_DATA="$GAMEDIR/data"
export POPBOMB_FULLSCREEN=1

# Both helpers live in newer PortMaster builds; older ones do not ship them.
command -v pm_platform_helper >/dev/null 2>&1 || pm_platform_helper() { :; }
command -v pm_finish >/dev/null 2>&1 || pm_finish() { :; }

if [ -n "$GPTOKEYB" ] && [ -f "$GAMEDIR/popbomb.gptk" ]; then
  $GPTOKEYB "popbomb-sdl.aarch64" -c "$GAMEDIR/popbomb.gptk" &
fi

pm_platform_helper "$GAMEDIR/popbomb-sdl.aarch64"
./popbomb-sdl.aarch64
pm_finish
