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

# Instancia anterior (crash/ssh) segura o DRM master -> a nova abre SEM TELA
# (pageflip -13) com som duplicado. Garante instancia UNICA antes de subir.
if pgrep -x bully >/dev/null 2>&1; then
  pkill -x bully; sleep 1
  pgrep -x bully >/dev/null 2>&1 && { pkill -9 -x bully; sleep 1; }
fi
pkill -x gptokeyb 2>/dev/null

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

# ---------- settings padrao da 1a execucao ----------
# O jogo cria o settings.ini com Clarity (resolution) = rs_low em instalacao
# nova. Semeia um padrao com rs_high (template gerado pelo proprio engine).
# So age quando NAO existe settings.ini — depois a escolha do usuario manda.
if [ ! -f "$GAMEDIR/settings.ini" ]; then
  cat > "$GAMEDIR/settings.ini" <<'SETEOF'
bullysettings={
	fxvolume=0.600000,
	musicvolume=0.600000,
	speechvolume=0.700000,
	brightness=0.800000,
	shadow=ss_off,
	resolution=rs_high,
	language=ls_english,
	sensitivity=0.400000,
	invertx=false,
	inverty=false,
	vibrate=vs_high,
	subtitles=false,
	lefthanded=false,
	autoclimb=false,
	recording=false,
	hasrated=false,
	steeringmode=vsm_digital,
	positions=[6,{
		center={
			x=0.830000,
			y=0.790000
		},
		size=3
	},{
		center={
			x=0.635000,
			y=0.835000
		},
		size=1
	},{
		center={
			x=0.680000,
			y=0.645000
		},
		size=1
	},{
		center={
			x=0.875000,
			y=0.595000
		},
		size=1
	},{
		center={
			x=0.870000,
			y=0.460000
		},
		size=0
	},{
		center={
			x=0.465000,
			y=0.895000
		},
		size=1
	}],
	vehiclesensitivity=0.800000
}
SETEOF
  echo "settings.ini semeado com Clarity=rs_high (1a execucao)"
fi

# ---------- ambiente ----------
# runtime/ = glibc 2.43 bundlada (ld+libc+libm) p/ devices com glibc velha (<2.34,
# ex: ArkOS). O binario usa interpretador RELATIVO runtime/ld-linux-aarch64.so.1
# (patchelf no empacote) -> o cd "$GAMEDIR" acima e OBRIGATORIO. Multiarch dirs
# no fim p/ SDL2/EGL de CFWs Debian-based. SDL2/EGL/GPU sao SEMPRE os do device.
export LD_LIBRARY_PATH="$GAMEDIR/runtime:/usr/lib:$GAMEDIR:$LD_LIBRARY_PATH:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export SDL2COMPAT_FORCE_FULLSCREEN_DESKTOP=1
export SDL_VIDEO_FULLSCREEN_DESKTOP=1
# Backend de display ADAPTATIVO (compat fbdev + kmsdrm/wayland). Em devices KMSDRM
# o pm_platform_helper ja forca SDL_VIDEODRIVER=kmsdrm; aqui garante o fbdev (Mali-450).
if [ -e /dev/dri/card0 ]; then
  export SDL_VIDEODRIVER=kmsdrm            # device com DRM/KMS (Mali-G310 Valhall, kernel mainline)
  # Anti-aliasing 4x OPCIONAL (v6: desligado por padrao). Ajuda em paineis
  # 480p e custa pouco em GPU tile-based (fallback automatico se a GPU
  # recusar). P/ LIGAR: descomente a linha abaixo (tire o "# " do inicio).
  # export BULLY_MSAA=4
else
  export SDL_VIDEODRIVER=mali              # EGL fbdev (Amlogic-old Mali-450, kernel 3.14)
  export BULLY_TEX_LIGHT=1                 # Mali-450: pula mapas _n/_s
  export BULLY_TEX_HALF=1                  # Mali-450: pula mipmaps + tex>=512 pela metade
fi                                         # (Mali-450: BULLY_MSAA nem setado -> caminho identico ao v4)

# X5M (Valhall s7d/s6/s5): o modeset do jogo CONGELA o PCM HDMI se ele estiver
# aberto durante a troca de modo (state XRUN, hw_ptr parado) -> jogo mudo.
# SOLTA o audio antes de abrir o jogo: espera o PCM fechar (o ES fecha o stream
# ao lancar; o sink suspende sozinho). So neste device; nos outros nem entra.
if grep -qE "s7d|s6|s5" /proc/device-tree/compatible 2>/dev/null; then
  for i in $(seq 1 32); do
    grep -q closed /proc/asound/card0/pcm0p/sub0/status 2>/dev/null && break
    sleep 0.25
  done
fi

$ESUDO chmod +x "$GAMEDIR/bully"

# Padrao PortMaster: o gptokeyb traduz o controle do CFW em TECLADO/MOUSE pelo
# bully.gptk (layout PS2) e o binario le essas teclas (BULLY_INPUT=gptk) — o
# mapeamento de botoes fica no .gptk, fora do binario, normalizado por device.
# Os ANALOGICOS continuam vindo do pad via SDL quando visivel (gradiente
# andar/correr); se o gptokeyb der grab, wasd+mouse do .gptk assumem.
# Sem gptokeyb no device -> fallback v5: controle NATIVO direto no binario.
if [ -n "$GPTOKEYB" ] && { set -- $GPTOKEYB; [ -x "$1" ]; }; then
  export BULLY_INPUT=gptk
  $GPTOKEYB "bully" -c "$GAMEDIR/bully.gptk" &
elif command -v gptokeyb >/dev/null 2>&1; then
  export BULLY_INPUT=gptk
  gptokeyb -1 "bully" -c "$GAMEDIR/bully.gptk" &
fi

command -v pm_platform_helper >/dev/null 2>&1 && pm_platform_helper "$GAMEDIR/bully"
./bully

pkill -f gptokeyb 2>/dev/null
command -v pm_finish >/dev/null 2>&1 && pm_finish
