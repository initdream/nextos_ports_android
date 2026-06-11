#!/bin/sh
# Cuphead Mali-450 — config de TESTE DO MAPA (s12): reduz memória p/ sobreviver ao
# load do mapa-múndi (cena mais pesada; OOM-thrash matava o sshd).
# Diferenças vs go.sh: CUP_TEXHALF=256 (texturas no cap 256² = 1/4 da RAM das de 512²)
# + CUP_GPVIRT/GPLOG p/ navegar via /tmp/gpcmd. Player 2 fantasma bloqueado no binário.
cd /storage/roms/cuphead-recon
fuser -k /storage/roms/cuphead-recon/cuphead 2>/dev/null; sleep 2
: > /tmp/gpcmd
cp debug.log debug.prevmap.log 2>/dev/null
export CUP_NOEXTRACT=1 CUP_FORCEIL2=1 CUP_NOSIGINST=1 CUP_NOSIGH=1
export CUP_FORCEINTEG=1 CUP_NO872774=1 CUP_GCOFF=1
export CUP_CLAMPSIG=1 CUP_SIGCLAMP=4096 CUP_TEXHALF=256
export CUP_FORCESTARTCR=1 CUP_SAPATH=/storage/cuphead-sa CUP_NOREFRESHDLC=1
export CUP_GAMEPAD=1 CUP_GPVIRT=1 CUP_GPLOG=1 CUP_FRAMES=0 CUP_MEMLOG=1 CUP_GCEVERY=1800
export CUP_CRSPY=1 CUP_HASPEND_STALE=120
export SDL_VIDEODRIVER=mali LD_LIBRARY_PATH=/usr/lib
nohup ./cuphead > run.out 2>&1 &
echo "PID $! (TEXHALF=256, P2 bloqueado)"
