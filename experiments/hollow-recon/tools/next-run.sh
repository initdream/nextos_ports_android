#!/bin/bash
# next-run.sh -- espera o .89 voltar, resgata logs do wedge, deploya e roda.
DEV=192.168.31.89
SSH="sshpass -p emuelec ssh -o ConnectTimeout=6 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@$DEV"
SCP="sshpass -p emuelec scp -o ConnectTimeout=6 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"
HR=~/nextos_ports_android/experiments/hollow-recon

echo "[next-run] esperando $DEV..."
until ping -c1 -W1 $DEV >/dev/null 2>&1; do sleep 5; done
echo "[next-run] device UP $(date +%H:%M:%S); esperando ssh..."
sleep 10
until timeout 8 $SSH true 2>/dev/null; do sleep 5; done

echo "[next-run] 1) resgatando logs do run wedgado"
$SCP root@$DEV:/storage/hollow-recon/run.log $HR/logs-wedge1-run.log 2>/dev/null
$SCP root@$DEV:/storage/hollow-recon/mem.log $HR/logs-wedge1-mem.log 2>/dev/null
echo "--- tail do run.log do wedge ---"
tail -25 $HR/logs-wedge1-run.log 2>/dev/null

echo "[next-run] 2) deploy binario novo + saferun"
$SCP $HR/build/hollow-recon root@$DEV:/storage/hollow-recon/hollow-recon || exit 1
$SCP $HR/saferun.sh root@$DEV:/storage/hollow-recon/saferun.sh || exit 1

echo "[next-run] 3) run 40s anti-wedge (PTHREAD_SHIM + TEXCAP + SWAPMS + glFinish noop)"
timeout 90 $SSH "HK_GLES2=1 HK_PTHREAD_SHIM=1 HK_TEXCAP=1024 HK_SWAPMS=20 SDL_VIDEODRIVER=mali NODRIVER=1 GC_DONT_GC=1 sh /storage/hollow-recon/saferun.sh 40"
RC=$?
echo "[next-run] saferun rc=$RC"

echo "[next-run] 4) coletando resultado"
sleep 2
if ping -c1 -W2 $DEV >/dev/null 2>&1; then
  $SCP root@$DEV:/storage/hollow-recon/run.log $HR/logs-run2.log 2>/dev/null
  $SCP root@$DEV:/storage/hollow-recon/mem.log $HR/logs-run2-mem.log 2>/dev/null
  echo "=== SOBREVIVEU. tail run2 ==="
  tail -40 $HR/logs-run2.log
else
  echo "=== DEVICE WEDGOU DE NOVO (ping morto pos-run) ==="
fi
