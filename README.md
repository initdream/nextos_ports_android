# nextos_ports_android

Framework pra **portar jogos Android (ARM64, NativeActivity) pra Linux ARM64 / NextOS** — alvo principal os devices **Mali** (Amlogic-old/ng/nxtos, Utgard/Bifrost/Valhall).

Não recompila o jogo: **carrega o `.so` nativo do Android e roda direto** no Linux, com uma camada de shim que finge ser Android (fake JNI, OpenSL ES→SDL2, EGL→SDL2, bionic→glibc). Mesma linhagem dos so-loaders de PSVita (TheFloW), adaptada pra Linux ARM64 + SDL2.

> ✅ **Provado no Mali-450 (Utgard):** os ports de referência **Syberia** (GLES1) e **LEGO Star Wars: TCS** (GLES2) rodam perfeitos. O caminho de render (so-loader + EGL→SDL2 + GLES) está validado no Utgard.

> 🏆 **Primeiro jogo portado DO ZERO com este framework: [`ports/revc`](ports/revc/README.md) — GTA: Vice City (reVC Android), 100% JOGÁVEL no Mali-450** (mundo, controle, áudio, menu, NPCs). **Leia o [README do revc](ports/revc/README.md)** — documenta a arquitetura de **so-loader 2-módulos** (libc++_shared + engine) e as **receitas Mali-450/GLES2 reutilizáveis** (ABI pthread bionic→glibc, shaders highp/MAX_LIGHTS/im2d, texturas GL_RGBA8/`GL_TEXTURE_MAX_LEVEL`-mipmap, SDL resolução/input, redirect de `fopen`, patch de runtime). **Comece por lá ao portar o próximo jogo.**

## Por que funciona tão bem
Android é Linux. O código do jogo é **ARM nativo** rodando no ARM do device — zero emulação de CPU. GLES é GLES (mesma API). Nos teus TV boxes, é praticamente o hardware nativo do jogo (mesmo SoC/GPU classe Android). Só a "casca" Android é trocada por SDL2/glibc.

## Estrutura
```
core/            # REUTILIZÁVEL entre todos os ports (não-editar por jogo)
  so_util.*      #   loader ELF arm64 (relocs, GOT, init_array, hook_arm64)  ← coração
  egl_shim.*     #   EGL -> SDL2 (genérico p/ qualquer jogo GLES)
  opensles_shim.*#   OpenSL ES -> SDL2 (ring buffer SPSC + resample)
  util.* error.* hashmap.h
template/src/    # BASE por-jogo (copiada e adaptada pra cada port)
  main.c         #   loader flow + GOT hooks + crash recovery
  android_shim.* #   fake android_native_app_glue (paths, input, resolução)
  jni_shim.*     #   fake JNI (package name, OBB path, feature flags)
tools/
  new-port.sh    # << gera um port novo a partir de um APK/.so >>
ports/<jogo>/    # cada port gerado vive aqui
docs/            # arquitetura + receita + referência (lswtcs)
```

## Fluxo de um port novo
```bash
# 1. bootstrap: extrai .so, classifica os símbolos, gera o esqueleto compilável
tools/new-port.sh ~/meujogo.apk meujogo

# 2. o tool reporta: X auto-resolvidos / Y a implementar (UNKNOWN)
#    edite ports/meujogo/src/imports.gen.c  (resolva os UNKNOWN)
#    edite jni_shim.c (package name + OBB path do jogo)

# 3. build (toolchain NextOS) e roda no device
make -C ports/meujogo
```
O `new-port.sh` mata o trabalho mais chato — a tabela de 200-370 símbolos — auto-mapeando libc/libm/GLES/pthread e listando só o que é específico do jogo.

## GLES1 vs GLES2
Cada jogo usa uma versão. O build linka `GLES_CM` (GLES1, ex. Syberia) **ou** `GLESv2` (GLES2, ex. LEGO SW) — configurável por port. (O `new-port.sh` detecta pela presença de símbolos GLES1-only como `glMatrixMode`/`glOrthof`.)

## Legal — BYO game files
Este repo é **só a ferramenta/loader** (como o PortMaster). Ele **não** distribui jogo nenhum. Você fornece o `.so` + assets de um APK **que você possui legalmente**. Uso não-comercial/hobbyista.

## Créditos
Núcleo derivado dos ports **[syberia_arm64](https://github.com/mtojek/syberia_arm64)** e **[lswtcs_arm64](https://github.com/mtojek/lswtcs_arm64)** de **mtojek** (licença **Apache-2.0**). Este framework generaliza aquele approach. Veja `NOTICE` para atribuição.
