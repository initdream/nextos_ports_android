# Bully: Anniversary Edition (NextOS Elite) — CHANGELOG

---

## v6 — 2026-06-12 — Controles PS2 via gptokeyb + troca de itens + binario novo

### Resumo
Os CONTROLES sairam do binario e foram pro padrao PortMaster (gptokeyb +
`bully.gptk`): layout PS2 completo, remapeavel SEM recompilar, padronizado
em qualquer CFW. E a troca de ITENS — que nao existia em botao nenhum do
build mobile — foi destravada via toque sintetico no HUD.

### Controles (layout PS2, editavel no bully.gptk)
| Botao | Funcao |
|-------|--------|
| Cruz / Circulo / Quadrado / Triangulo | correr / pular / bater / agarrar-interagir |
| L1 | mira/lock-on (abas p/ esquerda nos menus) |
| R1 | disparar arma (abas p/ direita nos menus) |
| L2 / R2 | trocar item anterior/proximo (toque sintetico no slot do HUD) |
| L3 / R3 | olhar p/ tras / agachar |
| dpad | zoom mapa (cima/baixo) + tarefas (esq/dir) |
| SELECT+START | sair do jogo |

### Por dentro (engenharia reversa do libGame 1.4.311)
1. **Enum real de eventos** confirmado no binario (__type_GamepadButton):
   o build mobile ignora shoulders/triggers (16-19) e religa as acoes extras
   no cluster 6/7/12-15 (6=mira 7=tiro 12=olhar-tras 13=agachar). O jogo le
   SO eventos (GetGamepadButtons/Axis nunca sao chamados no gameplay).
2. **Troca de item nao tem botao** no build touch — solucao: o binario injeta
   um TOQUE (AND_TouchEvent) no slot de arma do HUD quando L2/R2 sao
   apertados, com coordenadas relativas a resolucao (vale 480p/720p/1080p).
3. **bully.gptk** = mapeamento fisico->acao por device. ATENCAO: comentario
   `#` SOMENTE em linha propria (o parser do gptokeyb invalida a linha se o
   comentario vier apos o valor). Pads que reportam shoulder<->gatilho
   trocados: basta cruzar as linhas l1/l2 (e r1/r2) no .gptk.
4. Sem gptokeyb no CFW -> fallback automatico pro controle nativo da v5.

### Outras mudancas
- **Clarity (resolution) vem em HIGH por padrao**: em instalacao nova o jogo
  criava o settings.ini com rs_low; o launcher semeia um settings.ini padrao
  com `resolution=rs_high` na 1a execucao (depois a escolha do usuario manda).
- **MSAA 4x agora e OPCIONAL e vem DESLIGADO** (linha comentada no Bully.sh;
  descomente `export BULLY_MSAA=4` p/ ligar em paineis 480p).
- **Instancia unica**: o launcher mata instancia anterior presa (crash) antes
  de subir — evita "jogo sem tela + som duplicado" (briga de DRM master).
- **Saves do Android sao compativeis**: copie `BullyFile*`+`FileInfo*` de
  `Android/data/com.rockstargames.bully/files/` pra pasta `bully/` e eles
  aparecem no Load Game.
- Ferramentas de diagnostico embutidas (so agem se voce pedir):
  `echo N > /dev/shm/bully_btn` dispara evento de botao; `echo "x y" >
  /dev/shm/bully_tap` injeta toque; `touch /dev/shm/bully_shot` salva
  screenshot RGBA em /dev/shm/bully_shot.raw.
- X5M/Valhall (KMSDRM Amlogic vendor): o launcher espera o PCM HDMI fechar
  antes de abrir o jogo (modeset com PCM aberto congela o audio do sistema
  nesse kernel). Gateado por device-tree; nos demais devices nem executa.

---

## v5 — 2026-06-12 — Roda em qualquer CFW: glibc bundlada + fallback de vídeo + MSAA 4x

### Resumo
Versão multi-device de verdade, motivada por testes de terceiros (Trimui Smart Pro,
CFWs Debian-based tipo ArkOS). Três muros, três fixes — Mali-450 (NextOS) intocado.

### Fixes
1. **glibc 2.43 bundlada** (`runtime/`: ld-linux-aarch64.so.1 + libc.so.6 + libm.so.6).
   Devices com glibc < 2.34 recusavam o binário (`GLIBC_2.34 not found`). O binário
   agora usa interpretador RELATIVO (`runtime/ld-linux-aarch64.so.1`, via patchelf) —
   por isso o launcher PRECISA do `cd "$GAMEDIR"` antes de executar. O `runtime/` vem
   primeiro no LD_LIBRARY_PATH; SDL2/EGL/GPU continuam sendo os do device (glibc nova
   executa libs compiladas contra glibc velha numa boa, o contrário é que não).
2. **Fallback de driver de vídeo** (`egl_shim.c`): se o SDL_VIDEODRIVER pedido não
   existir no SDL2 do device (ex.: Trimui stock sem backend kmsdrm), solta a variável
   e re-inicializa deixando o SDL escolher o backend que existe. Confirmado em campo:
   no Trimui Smart Pro, remover o SDL_VIDEODRIVER fez a imagem aparecer — agora o
   binário faz isso sozinho.
3. **MSAA 4x nos devices KMSDRM** (`BULLY_MSAA=4` no launcher + `egl_shim.c`): pede
   multisample na criação da janela, com fallback automático pra 0x se o driver
   recusar. Faz diferença visível em painéis 480p (serrilhado some); quase de graça
   em GPU tile-based. **Mali-450/fbdev: a variável nem é setada — caminho idêntico
   ao v4** (sem custo, sem risco). Ajustável: `BULLY_MSAA=0|2|4` no Bully.sh.
4. **Launcher**: multiarch dirs de Debian/Ubuntu (`/usr/lib/aarch64-linux-gnu`,
   `/lib/aarch64-linux-gnu`) no fim do LD_LIBRARY_PATH p/ achar SDL2/EGL nesses CFWs.
5. **SELECT+START confiável em TODO device** (`jni_shim.c`): o hotkey de sair já
   existia no binário, mas só no `GetGamepadButtons` (caminho de POLLING, que o
   jogo quase nunca usa — ele lê input por EVENTOS). Por isso v3/v4 funcionava em
   uns devices e não em outros (dependia do gptokeyb de cada CFW). Agora o check
   roda no `pump_gamepad()` (todo frame, no loop principal) — sai em qualquer
   device, com qualquer PortMaster, mesmo sem gptokeyb. O gptokeyb continua sendo
   lançado como backup (inofensivo).

---

## v4 — 2026-06-11 — Qualidade restaurada no KMSDRM (highp + trilinear) + launcher no padrão PortMaster

### Resumo
A v3 trouxe o suporte multi-backend (rodar em KMSDRM). A **v4 RESTAURA a QUALIDADE GRÁFICA**
que tinha sido cortada pro Mali-450 — mas só nos devices KMSDRM capazes (Mali-G310/R36S); o
Mali-450 (fbdev) mantém os cortes que ele precisa. Tudo no MESMO binário, adaptativo.
Rodando **60fps liso** no S905X5M (Mali-G310, 1080p).

### Qualidade restaurada (só no KMSDRM, via `bully_is_kmsdrm()`)
1. **Shaders em highp** (`imports.c`) — antes forçava mediump (limite do Mali-450 Utgard). No
   G310/R36S mantém highp → cores e iluminação com precisão total (sem banding).
2. **Filtragem trilinear / mipmap** (`imports.c`) — antes forçava bilinear (GL_LINEAR, sem
   mipmap, que o Utgard não tinha). No KMSDRM deixa o trilinear do jogo passar + gera a cadeia
   de mipmap (`glGenerateMipmap`) → texturas nítidas e estáveis ao longe (sem tremido/serrilhado).
   Ajuda ainda mais em telas 640x480 (R36S).
3. (já na v3) detail maps completos (sem TEX_LIGHT), textura full (sem TEX_HALF), resolução nativa.

### Launcher no padrão PortMaster (`Bully.sh`)
- Detecção de controlfolder cobrindo também `/storage/.config/PortMaster` (NextOS).
- Template padrão: `control.txt` + mod, `get_controls`, `$GPTOKEYB`, `pm_platform_helper`, `pm_finish`.
- Hotkey de SAIR **SELECT+START** via gptokeyb (padrão PortMaster), com fallback pro gptokeyb do
  sistema quando a instalação do PortMaster é parcial.

### Compatibilidade
- **Amlogic-old** (Mali-450 fbdev 720p): cortes do Utgard mantidos, 100% intacto.
- **S905X5M** (Mali-G310 KMSDRM 1080p): highp + trilinear + full, 60fps liso.
- **R36S** (KMSDRM 640x480): highp + trilinear + full.

---

## v3 — 2026-06-11 — Suporte multi-backend de vídeo (KMSDRM + Wayland) + fullscreen nativo automático

### Resumo
A v2 só rodava em devices **fbdev** (Mali-450 Utgard / Amlogic-old, kernel antigo).
A **v3 roda também em devices com DRM/KMS modernos** (Mali Bifrost/Valhall, kernel mainline) —
ex.: **S905X5M (Mali-G310)**, **R36S**, e similares — **sem quebrar o suporte fbdev**.

O port agora **se adapta sozinho** ao device. Tudo abaixo é automático (com fallback); o
usuário não precisa configurar nada.

---

### As 4 mudanças (cada uma com seu FALLBACK explicado)

#### 1) Launcher adaptativo — escolhe o driver de vídeo certo (`Bully.sh`)
O launcher detecta o tipo de display do device e escolhe o backend do SDL2:

```sh
if [ -e /dev/dri/card0 ]; then
  export SDL_VIDEODRIVER=kmsdrm        # device com DRM/KMS (Mali novo, kernel mainline)
else
  export SDL_VIDEODRIVER=mali          # device fbdev (Amlogic-old Mali-450, kernel 3.14)
  export BULLY_TEX_LIGHT=1             # otimização de textura SÓ pro Mali-450 (pula mapas _n/_s)
  export BULLY_TEX_HALF=1             # otimização de textura SÓ pro Mali-450 (mipmaps + tex÷2)
fi
```

- **Fallback / lógica:** existe `/dev/dri/card0` → é DRM/KMS → usa `kmsdrm`. Senão → é fbdev →
  usa o driver `mali` (EGL fbdev) **e liga as otimizações de textura do Mali-450** (que são
  necessárias só na GPU fraca; em GPUs modernas seriam desperdício de qualidade, então ficam
  desligadas no caminho kmsdrm).

#### 2) Criação da janela GBM/EGL robusta — fallback de formato de cor (`egl_shim.c`)
No KMSDRM, o plano de scanout primário é **XRGB8888 (sem canal alpha)**. Se a janela pedir
ARGB (com alpha), o SDL2 não acha um formato GBM compatível e falha com
`Can't window GBM/EGL surfaces on window creation`.

- **Fallback / lógica:** o port tenta criar a janela com `alpha=8` (ARGB, ideal pro fbdev mali);
  **se falhar, tenta de novo com `alpha=0` (XRGB, que o KMSDRM aceita)**. Assim funciona nos dois
  sem o usuário saber de nada. Log: `CreateWindow OK com alpha=0 (KMSDRM/XRGB)`.

#### 3) Apresentação correta no KMSDRM — o "page-flip" que faltava (`imports.c`)
**Esse era o motivo da tela ficar preta no KMSDRM mesmo o jogo renderizando.**
No fbdev, `eglSwapBuffers` cru já joga a imagem direto no framebuffer (visível). No KMSDRM,
quem faz o **page-flip** (mandar o buffer pro scanout da tela) é o `SDL_GL_SwapWindow` —
o `eglSwapBuffers` cru sozinho NÃO apresenta nada → tela preta.

- **Fallback / lógica:** o port detecta o backend (`bully_is_kmsdrm()` =
  `SDL_GetCurrentVideoDriver() != "mali"`) e:
  - **KMSDRM/Wayland:** roteia o `eglSwapBuffers` do jogo pro `SDL_GL_SwapWindow` (faz o page-flip).
  - **fbdev (mali):** mantém o `eglSwapBuffers` cru original (**Amlogic-old 100% intacto**).

#### 4) Resolução nativa automática — fullscreen em qualquer device (`jni_shim.c`)
Antes o jogo reportava **1280x720 fixo (hardcoded)**. Num device 1080p, ele desenhava só num
cantinho 1280x720 → não preenchia a tela.

- **Fallback / lógica:** agora o jogo reporta a **resolução REAL nativa do device**
  (`SDL_GetDesktopDisplayMode`) — cada device preenche a tela com a sua resolução:
  - **R36S** → 640x480
  - **Amlogic-old (Mali-450)** → 720p
  - **S905X5M** → 1080p

  Log: `implOnSurfaceChanged 1920x1080 (real)`.

---

### Compatibilidade
| Device | GPU | Display | Resolução | Status v3 |
|--------|-----|---------|-----------|-----------|
| Amlogic-old | Mali-450 (Utgard) | fbdev | 720p | ✅ inalterado (continua perfeito) |
| S905X5M | Mali-G310 (Valhall) | KMSDRM | 1080p | 🆕 NOVO |
| R36S | (KMSDRM) | KMSDRM | 640x480 | 🆕 NOVO |

> **Binário aarch64 ÚNICO** roda nos três — ele usa o `SDL2`/`EGL` instalado em cada device.
> Nada de versão separada por device.

---

## v2 — 2026-06-08 — Primeiro release (Mali-450 / Amlogic-old) — INÉDITO MUNDIAL

- Bully: Anniversary Edition (v1.4.311) rodando no **Mali-450 (Amlogic-old)** via so-loader —
  primeiro port **aarch64/Linux/PortMaster** do mundo (antes só existia Vita 32-bit e Switch).
- Cadeia: so-loader AArch64 → libGame.so (Android) → JNI estático → SDL2-mali EGL GLES2 fbdev.
- Mundo + escola jogáveis, controle + áudio OK, com fixes específicos do Utgard (texturas,
  shaders highp→mediump, alpha-test da roupa do Jimmy, limite de textura da GPU).
- BYO-data: o usuário fornece o APK do Bully (v1.4.311); o launcher extrai os dados no 1º boot.
