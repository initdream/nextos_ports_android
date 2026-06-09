#!/bin/sh
# saferun.sh -- roda o hollow-recon de forma SEGURA no device (nunca trava sem saida).
# Camadas: nice (nao starve o device) + timeout -s KILL (SIGKILL nao-capturavel) +
#          watchdog interno do harness (HK_WD) + cleanup duro + religa ES.
# Uso (env passados ANTES): HK_PTHREAD_SHIM=1 ... /storage/hollow-recon/saferun.sh [segundos]
cd /storage/hollow-recon || exit 1
SECS="${1:-35}"
: "${HK_WD:=$((SECS - 5))}"; export HK_WD   # watchdog interno dispara ANTES do KILL externo

systemctl stop emustation 2>/dev/null; sleep 1
pkill -9 -x hollow-recon 2>/dev/null; sleep 1        # mata orfaos antigos

( while true; do sync; sleep 1; done ) & SYNCPID=$!  # flusha o log
rm -f fb_*.raw
( i=0; while true; do sleep 10; dd if=/dev/fb0 of=fb_$i.raw bs=4096 count=2025 2>/dev/null; i=$((i+1)); done ) & CAPPID=$!  # captura DURANTE o run
nice -n 19 timeout -s KILL "$SECS" ./hollow-recon > run.log 2>&1
RC=$?
sync
kill "$SYNCPID" "$CAPPID" 2>/dev/null
dd if=/dev/fb0 of=/tmp/hk.raw bs=4096 count=2025 2>/dev/null   # captura final

pkill -9 -x hollow-recon 2>/dev/null; sleep 1
echo "[saferun] rc=$RC recon_vivo=$(pgrep -x hollow-recon | wc -l) load=$(cut -d' ' -f1 /proc/loadavg)"
systemctl start emustation 2>/dev/null
echo "[saferun] ES=$(systemctl is-active emustation 2>/dev/null)"
