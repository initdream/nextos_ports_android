#!/usr/bin/env python3
# fb2png.py <raw> [out.png] -- fb0 do .89 = 1280x720 BGRA stride 1280, DUPLA pagina
# (dump 8.3MB = 2 paginas de 1280x720x4=3.686.400 + resto). Gera pagina 0 e 1.
import sys
from PIL import Image
raw = open(sys.argv[1], 'rb').read()
base = sys.argv[2][:-4] if len(sys.argv) > 2 else sys.argv[1]
W, H = 1280, 720
PG = W * H * 4
for pg in range(min(2, len(raw) // PG)):
    img = Image.frombytes('RGBA', (W, H), raw[pg*PG:(pg+1)*PG])
    b, g, r, a = img.split()
    rgb = Image.merge('RGB', (r, g, b))
    out = f'{base}.p{pg}.png'
    rgb.save(out)
    colors = rgb.getcolors(16)
    nb = sum(c for c, col in (rgb.getcolors(2**24) or []) if col != (0,0,0))
    print(f'{out}: {W}x{H} pg{pg} px_nao_pretos={nb} cores={colors if colors else ">16"}')
