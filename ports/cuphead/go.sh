#!/bin/sh
# Cuphead Mobile -> Mali-450. CONFIG VENCEDORA s10: BOOT 100% CONFIAVEL ATE O TITULO.
# Raiz do crash flaky $PC=9 RESOLVIDA: era use-after-free do GC (CUP_GCOFF) + race da
# integracao forcada (CUP_NO872774). Controle real via USB Gamepad js0 (CUP_GAMEPAD).
cd /storage/roms/cuphead-recon
fuser -k /storage/roms/cuphead-recon/cuphead 2>/dev/null; sleep 2
export CUP_NOEXTRACT=1 CUP_FORCEIL2=1 CUP_NOSIGINST=1 CUP_NOSIGH=1
export CUP_FORCEINTEG=1 CUP_NO872774=1 CUP_GCOFF=1
export CUP_CLAMPSIG=1 CUP_SIGCLAMP=4096 CUP_TEXHALF=512
export CUP_FORCESTARTCR=1 CUP_SAPATH=/storage/cuphead-sa CUP_NOREFRESHDLC=1
export CUP_GAMEPAD=1 CUP_FRAMES=0
export SDL_VIDEODRIVER=mali LD_LIBRARY_PATH=/usr/lib
nohup ./cuphead > run.out 2>&1 &
echo "PID $! - aperte o controle no disclaimer p/ ir ao titulo"
