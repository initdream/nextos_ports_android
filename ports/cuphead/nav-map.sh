#!/bin/sh
# Navegação autônoma até o MAPA (s12). Roda no device após boot chegar ao disclaimer.
# Requer binário com dir-hold curto (down = 1 passo). Sequência:
# disclaimer -> menu START -> save NEW -> livro -> dialogo casa -> controle ->
# PAUSE -> 3x down (EXIT TO MAP) -> accept.
g(){ echo "$1" > /tmp/gpcmd; sleep "${2:-1.5}"; }
echo "[nav] disclaimer..."; g go 1.5; g go 2
echo "[nav] menu START..."; g accept 3
echo "[nav] save NEW..."; g accept 4
echo "[nav] livro (accept x34)..."; i=0; while [ $i -lt 34 ]; do g accept 1.5; i=$((i+1)); done
echo "[nav] dialogo casa (accept x24)..."; i=0; while [ $i -lt 24 ]; do g accept 1.5; i=$((i+1)); done
echo "[nav] PAUSE..."; g pause 2.5
echo "[nav] 3x down -> EXIT TO MAP (passo unico)..."; g down 1; g down 1; g down 1.5
echo "[nav] EXIT TO MAP..."; g accept 8
echo "[nav] feito."
