# 🏆 Bully: Anniversary Edition → Mali-450 (NextOS / PortMaster) — INÉDITO MUNDIAL

Primeiro port do **Bully** rodando em **aarch64 / Linux / PortMaster**, **100% jogável** no
**Mali-450 MP (Utgard) + OpenGL ES 2.0 + fbdev** (device Amlogic-old, S905X, 832 MB RAM, kernel 3.14).
Mundo aberto, **escola (hub principal)**, controle, áudio, **personagem vestido**, resolução máxima,
estável. Conseguido em 2026-06-08.

> Antes só existiam ports **Vita** (32-bit, TheFloW) e **Switch** (64-bit, givethesourceplox/bully-NX).
> Ninguém tinha feito aarch64/Linux/PortMaster. Este é o primeiro.

---

## 1. O que é

Port via **so-loader**: carregar o `libGame.so` (Android arm64) dentro de um ELF Linux glibc,
resolver os imports, satisfazer o contrato JNI/EGL com as libs do device, e dirigir o loop do jogo.

- **Jogo:** Bully AE **v1.4.311**, `libGame.so` (BuildID `6139a628`), entry **JNI estático**
  (métodos `Java_com_rockstargames_oswrapper_GameNative_impl*`), **não** usa RegisterNatives.
- **APK certo:** `Bully_1.4.311-60FPS-...apk` — tem `lib/arm64-v8a/libGame.so` (19 MB) +
  `assets/data_0-4.zip` (2.9 GB, completos). APKs 32-bit/incompletos são inúteis.
- **Referências (mesmo libGame):** `bully-NX` (Switch, driver) e `bully_vita` (contrato JNI), em
  `experiments/bully/ref-*`.
- **Este repo guarda só o CÓDIGO** (`ports/bully/src/*`). Os dados/libs do jogo (data_*.zip,
  libGame.so, libc++_shared.so) **não** entram no git (são do APK; BYO-data estilo PortMaster).

## 2. Arquitetura (cadeia que funciona no device)

```
main.c  (loader 2 módulos, padrão reVC AArch64)
 ├─ preload libs do device (SDL2/GLESv2/EGL/openal/mpg123, RTLD_GLOBAL)
 ├─ so_load libc++_shared.so  → snapshot de símbolos
 ├─ so_load libGame.so        → resolve via bully_stub_table + pthread_bridge + libc++ + dlsym
 └─ jni_load()  [jni_shim.c — driver do JNI estático]
     ├─ build_env (fake_vm/fake_env, JNINativeInterface 64-bit)
     ├─ HOOKS (so_make_text_writable → hook_arm64 → executable)
     ├─ JNI_OnLoad → implOnInitialSetup → gates (srp)
     ├─ implOnActivityCreated → bully_init_gl (SDL2-mali EGL GLES2) → seed dos OS_EGL globals
     ├─ implOnSurfaceCreated/Changed/Resume
     ├─ worker de arquivo async + OS_ZipAdd(data_0/1)
     └─ loop implOnDrawFrame + gate Rockstar (frame>30) → GameMain → render
```

Arquivos-chave (`ports/bully/src/`): `main.c` (loader) · `jni_shim.c` (driver JNI + input + hooks) ·
`so_util.c` (loader AArch64 + **hook_arm64 pool**) · `egl_shim.c` (**SDL2-mali**) ·
`imports.c` (stubs bionic + **fixes GLES2 do Utgard**) · `asset_archive.c` (lê os data zips) ·
`pthread_bridge.c` (ABI bionic→glibc). `build.sh` compila; `Bully.sh` é o launcher PortMaster.

## 3. Os fixes críticos (com o PORQUÊ)

### 3.1 Loader / sistema
- **`hook_arm64` com POOL de trampolins (4 bytes no entry).** ⭐ O trampolim de 16 bytes
  (`LDR X17`+`BR X17`+`.quad`) COLIDIA em funções de 4 bytes adjacentes: hookar `NvAPKSize`
  corrompia o entry de `NvAPKRead` → `ZIPFile::Find` loopava 100% CPU. **Fix:** o hook escreve só
  **4 bytes** (um `B` para um slot num pool na cauda do heap do libGame, dentro de ±128 MB).
- **DADOS completos** (causa da `whitetexture` NULL → crash em `GameRenderer::Setup`): copiar os
  `data_0-4.zip` REAIS do APK (a teoria "textura interna" estava errada — é arquivo).
- **`w_fopen`** redireciona `data_N.zip` → `assets/data_N.zip` (vfat não faz symlink).
- compat `so_util_x64.h` (jni_shim escrito p/ o multi-módulo x86_64 compila sem mudança);
  stubs bionic (`android_set_abort_message`, `__system_property_get`); `so_make_text_writable`
  antes dos hooks; pthread_bridge (ABI bionic→glibc, do reVC).

### 3.2 EGL + GLES2 (Mali-450 Utgard)
- **EGL via SDL2-mali** (não raw EGL): `eglCreateWindowSurface(fbdev_window)` cru dava
  **BAD_ALLOC 0x3003**; o driver SDL2 "mali" faz o EGL fbdev certo. O libGame importa `egl*` direto,
  então expomos os objetos EGL que o SDL criou via `eglGetCurrent{Display,Surface,Context}`.
- **`highp → mediump` só no FRAGMENTO.** A GP (vertex) do Utgard É FP32 e suporta highp; só a PP
  (fragment) não tem. Forçar mediump no vertex quebraria a precisão do skinning.
- **`GL_LUMINANCE` → RGBA8888 na CPU.** O Mali amostra `GL_LUMINANCE` como `(L,L,L,L)` (alpha=L),
  não `(L,L,L,1)` → leitura errada. Convertemos `L→(L,L,L,255)` (idem `LUMINANCE_ALPHA`). Inclui o
  caso `px=NULL` (alvo de render-to-texture): aloca como RGBA (o Mali não renderiza p/ LUMINANCE).
- **`glTexImage2D`**: `GL_RGBA8`(0x8058)→`GL_RGBA`, `GL_RGB8`→`GL_RGB` (GLES2 não aceita sized).
- **`glTexParameteri`**: ignora `GL_TEXTURE_MAX_LEVEL` (inexistente em GLES2) e troca os filtros
  `*_MIPMAP_*` por `GL_LINEAR` (mipmap incompleto = preto).
- **`OS_ThreadMakeCurrent`/`OS_ThreadUnmakeCurrent`** hookados (passa o contexto GL entre o GameMain
  e a render thread).

### 3.3 ⭐ A ROUPA DO JIMMY (o boss final) — `glClear` force-COLOR só na tela
**Sintoma:** o corpo/camisa/braços do player **apareciam e sumiam** (no guarda-roupa, parado ou
mexendo). **Causa-raiz:** o nosso `my_glClear` forçava `GL_COLOR_BUFFER_BIT` em **TODO** clear (era um
fix antigo de tela preta). A roupa do player é **composta numa textura via render-to-texture**; no
estado estável o jogo faz um clear **só de PROFUNDIDADE** nessa FBO → o nosso force-COLOR **apagava a
cor** (a roupa já composta) → sumia. **Fix:**
```c
unsigned m = g_in_fbo ? mask : (mask | 0x4000);  // força COR só FORA de FBO (a tela)
```
Dentro de FBO respeita a máscara do jogo → o clear de profundidade não apaga a roupa. **Jimmy vestido.**

> Pistas que levaram lá: *aparece-e-some* (renderiza certo); *trocar roupa rápido mantém visível*
> (re-compõe sem parar); *some parado e mexendo* (descarta skinning/animação); um trace de
> `draws/clears` por render-to-texture mostrou composições "draws=0 clears=1" (os clears de
> profundidade que o nosso force-COLOR transformava em erase).

### 3.4 ⭐ A ESCOLA (hub principal) travava o device
A escola é densa (muitos NPCs). Travava **DURO** (wedge). Diagnóstico provou que **NÃO era OOM**
(sempre sobrava RAM; o swap quase não era usado) **nem áudio nem skinning**. Dois muros:
1. **Loading lento:** o `find_zip_entry` do `asset_archive` fazia **varredura LINEAR** nos 60 418
   itens **por open**; a escola faz milhares de opens → centenas de milhões de `strcmp` no S905X.
   **Fix:** `qsort` + `bsearch` (**O(log n)**, lazy-sort na 1ª busca).
2. **Wedge na escola cheia:** o **limite de memória de textura da GPU Utgard** (separado da RAM!) —
   muitos NPCs (cada um `_d/_n/_s` 512²) estouram e o driver trava. **Fix** (env no launcher):
   - `BULLY_TEX_LIGHT=1` — pula os mapas de detalhe `_n`/`_s` (corta 2/3 por NPC).
   - `BULLY_TEX_HALF=1` — pula mipmaps (o Mali usa `GL_LINEAR`, então mips são desperdício) e reduz
     texturas ≥512 pela metade (512→256 = 1/4 da memória; UV normalizado não quebra).

   (Inspiração do Felipe: o GTA SA roda liso porque gerencia melhor a textura dos pedestres.)

### 3.5 Controle (perfeito) e áudio
- **Controle por EVENTOS** (o jogo não faz polling; usa `implOnGamepad{ButtonDown,Up,AxesChanged,
  CountChanged}`, igual o bully-NX). `pump_gamepad()` empurra os eventos do `SDL_GameController`.
  Enum do libGame: 0=A 1=B 2=X 3=Y 4=START 5=BACK 6=L3 7=R3 8-11=NAV 12-15=DPAD 16=LB 17=LT 18=RB 19=RT.
- Áudio: OpenAL + mpg123 do device (thread Sound + bancos).

## 4. Build / Run

- **Build:** `ports/bully/build.sh` (toolchain Amlogic-old `aarch64-libreelec-linux-gnu-gcc`, linka
  `-lSDL2 -lEGL -ldl -lm -lpthread`). Saída: `bully` (~90 KB).
- **No device (PortMaster):** colocar em `ports/bully/` o binário `bully`, `libGame.so`,
  `libc++_shared.so` e `assets/data_0-4.zip(+.idx)`; o launcher é `ports_scripts/Bully.sh` (aparece
  como "Bully" no menu PORTS do ES). O `Bully.sh` usa `control.txt`/`get_controls`/
  `pm_platform_helper`/`pm_finish` e seta `SDL_VIDEODRIVER=mali` + `BULLY_TEX_LIGHT`/`BULLY_TEX_HALF`.
- ⚠️ **Dados via cartão no PC** (rede = ~30 min p/ 2.9 GB). Só `assets/` basta (o `w_fopen`
  redireciona). Device `vfat` não tem symlink.

## 5. Métodos de debug (truques que destravaram)

- **Screenshot CERTO** (o `fbgrab` engana, mostra branco): `dd if=/dev/fb0 of=x.raw bs=4096 count=900`
  + PIL `Image.frombytes('RGBA',(1280,720),d,'raw','BGRA')` (fb 1280×720×4 BGRA).
- **Achar offset de crash:** o crash_handler imprime `libGame+0x(PC-text_base)` → mapear com
  `nm -DC libGame.so`.
- **Achar loop (sem gdb):** `kill -SEGV <pid>` no processo travado → imprime o PC do loop.
- **Wedge vs OOM vs CPU:** **log persistente** (sync na SD a cada 1-2s, sobrevive ao wedge) +
  **monitor de memória** (`free -m` no log) — provou que a escola NÃO era OOM.
- **GL trace por env-flag** (ex.: `BULLY_RTT_TRACE`) — contar draws/clears por render-to-texture.
- ⚠️ **`glFinish` satura/wedga** se chamado na fase de loading; só usar in-game e com cuidado.
- **PC-first** (`experiments/bully/`, x86_64) valida a engine rápido (gdb instantâneo, sem wedge Mali);
  a mesma lógica (jni_shim/asset_archive) reaproveita no device.

## 6. Estado

✅ **100% jogável**: mundo + escola, controle, áudio, **Jimmy vestido**, resolução máxima, estável,
iluminação correta. Falta só **empacotar/imagem** no menu do ES (o `Bully.sh` já é PortMaster-padrão).
