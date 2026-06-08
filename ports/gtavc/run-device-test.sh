#!/bin/bash
set -euo pipefail

HOST="${GTAVC_HOST:-DEVICE_IP}"
PASS="${GTAVC_PASS:-DEVICE_PASS}"
PORT_DIR="/storage/roms/ports/gtavc"
SWAP_FILE="/storage/roms/gtavc.swap"

sshpass -p "$PASS" ssh -F /dev/null \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  "root@$HOST" "
set -e
cd '$PORT_DIR'
systemctl stop emustation 2>/dev/null || systemctl stop emulationstation 2>/dev/null || killall -9 emulationstation 2>/dev/null || killall -9 EmulationStation 2>/dev/null || true
swapon '$SWAP_FILE' 2>/dev/null || true
sysctl -w vm.swappiness=20 >/dev/null 2>&1 || true
sysctl -w vm.vfs_cache_pressure=50 >/dev/null 2>&1 || true
killall -9 gtavc 2>/dev/null || true
: > debug.log
: > run.out
LD_PRELOAD=./nextclock.so ./gtavc >run.out 2>&1 &
echo gtavc_pid=\$!
free -m
swapon --show 2>/dev/null || cat /proc/swaps
"
