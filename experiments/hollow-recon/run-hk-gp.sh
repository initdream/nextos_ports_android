#!/bin/bash
# Hollow Knight + gptokeyb (gamepad -> teclado virtual que o HK le como teclado)
systemctl stop essway 2>/dev/null; sleep 4
cd /storage/roms/hollow-recon
killall -9 hollow-recon gptokeyb 2>/dev/null
trap 'killall -9 hollow-recon gptokeyb 2>/dev/null; systemctl start essway 2>/dev/null' EXIT INT TERM
# 1) gptokeyb cria o teclado virtual ANTES do HK enumerar /dev/input
gptokeyb "hollow-recon" -c /storage/roms/hollow-recon/hk.gptk &
sleep 2
# 2) HK (le o teclado virtual via evdev)
NODRIVER=1 ./hollow-recon
