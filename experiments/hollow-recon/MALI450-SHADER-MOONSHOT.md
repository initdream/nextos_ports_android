# Hollow Knight no Mali-450 (Amlogic-old / GLES2) — MOONSHOT do tradutor de shader

Objetivo: rodar Hollow Knight (Unity 2020.2.2f1 IL2CPP, so-loader) no **Mali-450 (GLES2, .87)**.
O HK só tem shaders **GLES3** no blob → no GLES2 dá `platform 5 not available` → tela preta + SEGV.
Plano: **enganar a Unity p/ usar os shaders GLES3 + traduzir GLES3→GLES2 em runtime**.

## ✅ Vitórias confirmadas (não refazer)
1. **Hardware PASSA:** o Mali-450 (.87) TEM as extensões que os shaders do HK pedem —
   `GL_EXT_shader_texture_lod`, `GL_OES_standard_derivatives`, `GL_OES_vertex_array_object`.
   (visto em `/storage/hollow-recon/run87.log`). Então o Utgard CONSEGUE rodar os shaders traduzidos.
2. **Todas as versões do HK são GLES3-only.** Testados: APK do device, `..._LQ.apk`, `..._Mod-GLES.apk`
   — todos Unity 2020.2.2f1, `libunity.so` **idêntico** (md5 8a6029548178f9e76a292cbba9a24846),
   `boot.config` idêntico, data.unity3d com `#version 300 es` (GLES3), 0 GLES2. O "Mod-GLES" é
   clickbait (não adiciona shaders GLES2). `force-gles20` é recurso STOCK do Unity (não resolve sozinho).
3. **A Unity pega o GL via `dlopen("libGLESv2.so")` + `dlsym`** — NÃO pela import table.
   Logo o spoof tem que ser no **`my_dlsym`** (não no `set_import`, que fica inerte).
4. **Referência externa:** port do HK pra PS Vita (`PatnosDD/Hollow-Knight-PsVita`) — usa a versão
   PC (Steam/GOG) + APPLYPATCH + vitaGL (GXM). Caminho diferente; confirma que HK roda em GPU classe GLES2.

## ❌ Becos sem saída
- **Hackear as tripas da libunity (achar a função do "platform 5"):** a string
  `"Desired shader compiler platform %u is not available in shader blob"` (file off 20233866,
  vaddr **0x134be8a**) **NÃO tem xref** — nem `adr`, nem `adrp+add`, nem `adrp+ldr` (varri os 4.48M
  insns do .text com capstone+resync), nem ponteiro em dados, nem relocação. Só Ghidra/radare2
  (análise recursiva) acharia. Por isso pivotamos pro spoof no GL (sidesteppa a libunity).

## 🎯 Abordagem (spoof + tradutor) — 3 partes
1. **Spoof:** `my_glGetString(GL_VERSION)` → `"OpenGL ES 3.0"` + `GL_SHADING_LANGUAGE_VERSION` →
   `"OpenGL ES GLSL ES 3.00"`; `my_glGetIntegerv(GL_MAJOR/MINOR_VERSION)` → 3/0.
   → a Unity carrega os shaders **GLES3** do blob (some o `platform 5`).
2. **Tradutor:** `my_glShaderSource` captura + traduz GLES3→GLES2 (`#version 300 es`→`100`,
   `in/out`→`attribute/varying`, `texture()`→`texture2D()`, `textureLod`→`texture2DLodEXT`,
   dropar `layout`, achatar UBO se houver). Shaders são 2D moderados (sprites + luz/sombra).
3. **Bridge GLES3→GLES2 nas FUNÇÕES de GL** que a Unity chamar no caminho GLES3 e o Mali não tem
   (`glGetStringi` p/ extensões, VAO via `*OES`, `glVertexAttribIPointer`, `glDrawBuffers`...).
   Interceptar no `my_dlsym`: se a real der NULL, dar shim (extensão OES) ou stub logado.

## 🔧 Já implementado (em `src/recon_egl.c`)
- `my_glGetString` / `my_glGetIntegerv` / `my_glShaderSource` (com fwd-decls no topo).
- Intercept no **`my_dlsym`** (glGetString/glGetIntegerv só se !HK_NOSPOOF; glShaderSource sempre captura).
- `my_glShaderSource` despeja em `/storage/hollow-recon/shdump/sh_NNN.glsl`.
- **Gate do pthread_shim:** `recon_wire_pthread` só roda com `HK_PTHREAD_SHIM=1` (era p/ Amlogic-no;
  no .87 estava quebrando). `pthread_shim.c` foi adicionado ao `CMakeLists.txt` (faltava → link falhava).
- Envs: `HK_GLES2=1` (contexto EGL GLES2, obrigatório no Mali-450), `HK_NOSPOOF=1` (desliga o spoof),
  `HK_PTHREAD_SHIM=1` (liga o shim pthread).

## 🚧 BLOQUEIO ATUAL (começar daqui na próxima sessão)
O build da **fonte local atual** regrediu vs o **binário ANTIGO do .87** (de 03/06) que FUNCIONAVA:
- Baseline bom: `run87.log` (binário velho) chegava em `nativeRender frame 1` + `platform 5`.
- Meu build com **pthread ON** → crash no GL setup (`[dlopen] libGLESv2.so` → `[NULLR]` chamada null).
- Meu build com **pthread OFF (gate)** → crash em **`IL2CPP+0x68014c`** (e o `on_segv` re-crasha
  ao reportar, em `main_recon.c:on_segv` ~0x40771c, mascarando o fault real).
- Nenhum dos dois chega no frame 1. A fonte local tem mais divergências do que só o pthread.

### Próximos passos (ordem)
1. **Blindar o `on_segv`** (main_recon.c) p/ NÃO re-crashar (guardar o stack-scan/dladdr com
   leitura segura) → ver o **fault REAL** em IL2CPP+0x68014c.
2. **Reconciliar os shims** com o binário velho que funcionava: descobrir QUAL combinação de
   shims (sigaction/canary/GC/pthread) o binário de 03/06 usava p/ chegar no frame 1.
   (O binário velho ainda está em backup? Senão, bissecionar os `set_import` de shim.)
3. Com o build chegando no frame 1 de novo → rodar **com spoof** → confirmar `platform 5` some +
   shaders caem em `shdump/` → ler os shaders limpos.
4. Escrever o **tradutor GLES3→GLES2** no `my_glShaderSource`; iterar até `glCompileShader` passar.
5. Tapar as **funções GLES3** que faltarem (NULLR no dlsym) com shims OES/stubs.
6. Render → capturar `/dev/fb0` → iterar.

## Ambiente / tooling
- Device: `ssh nextos-87` (Amlogic-old, Mali-450). Tudo em `/storage/hollow-recon/`:
  `hollow-recon` (binário), `libunity.so`+`libil2cpp.so` (24/33MB), `assets/bin/Data/`, `shdump/`, logs.
- Build: `make -C build` (CMake, toolchain Amlogic-old). Avisos "bad subsection" do ld são NÃO-fatais.
- RE: `capstone`+`pyelftools` instalados (pip --user --break-system-packages).
  `/tmp/hk-gles/findxref.py` (caçador de xref com resync) + `/tmp/hk-gles/libunity.dis` (disasm).
- Rodar: parar `emustation`, `HK_GLES2=1 SDL_VIDEODRIVER=mali NODRIVER=1 GC_DONT_GC=1 ./hollow-recon`.
- ⚠️ Mali-450 é device PRIORITÁRIO do autor — não quebrar; crash do harness ≠ wedge (reboot raro).

## Filosofia
Igual o Bully: muitas sessões de grind, mas É POSSÍVEL (hardware confirmado). Não desistir.

## Atualização 2026-06-08 (parte 2) — spoof + stubs FUNCIONANDO; novo muro = features GLES3
AVANÇOS REAIS:
- ✅ Spoof movido pro `my_dlsym` (a Unity pega GL via dlsym; `set_import` era inerte).
- ✅ Gate do `pthread_shim` (`HK_PTHREAD_SHIM`): no .87 o shim quebrava o GL setup.
- ✅ Recuperação de chamada-NULL no `on_segv` (pc<0x10000 -> ret lr, x0=0) — mas LOOPAVA.
- 🔑 Loop achado: `unity+0x74a980 blr [x20+1888]` = **glGetInternalformativ** (consulta MSAA por
  formato: GL_RENDERBUFFER/GL_SAMPLES). NULL+x0=0 deixava params com lixo -> loop interno girava.
- ✅ FIX: STUBS de verdade no `my_dlsym` p/ GLES3 ausentes (não NULL): `stub_glGetInternalformativ`
  zera params (0 samples = quebra o loop); `stub_gl_noop` pro resto. + trava no on_segv (200k->_exit).
  RESULTADO: null-loop MORTO (nullcall=0, SEGV=0, 555 linhas, avança bem mais fundo).
- 🚧 NOVO MURO: a Unity tenta **Texture2DArray** (`got 1 max supported 0` = recurso GLES3 que o
  Mali-450 NÃO tem). Stubs no-op (glTexStorage3D/glTexImage3D) deixam textura inválida ->
  **driver Mali Utgard TRAVA em GPU/kernel** (o `timeout` nem mata; device engasga).
- ⚠️ Cada run WEDGEa o device (priority). Iterar assim é arriscado -> precisa de harness mais seguro.

### Armadilhas operacionais (importantes p/ proxima sessao)
- Após um run morto, o `./hollow-recon` fica ÓRFÃO ~40s (o `timeout` da device) segurando o binário
  -> `scp` falha com **ETXTBSY** ("dest open Failure"). Fix: `rm` o binário no device (desvincula) OU
  esperar/`pkill -9 -x hollow-recon` + `sleep 3` antes do scp.
- Sempre `systemctl start emustation` no fim (device usável).

### Próximos passos (revisados)
1. Iteração SEGURA primeiro: o run tem que autoterminar sem travar a GPU (stubs que façam a Unity
   PULAR o recurso GLES3, não chamar GL inválido).
2. Texture2DArray: forçar a Unity a NÃO usar (max_layers=0 já reportado, mas ela tenta depth=1) OU
   emular array como N texturas 2D no stub.
3. SÓ DEPOIS ligar o spoof GLES3 (shaders) — agora que o caminho de GLES3-funcs tem stubs.

## Atualização 2026-06-08 (parte 3) — 🏆 ENGINE RODA EM GLES2 + TRADUZ SHADERS + CHEGA NA TELA DE IDIOMA
MARCO (de IMPOSSIVEL -> a engine do HK rodando no Mali-450 via GLES2):
- ✅ ITERACAO SEGURA resolvida: `saferun.sh` (watchdog interno HK_WD + nice + timeout -s KILL +
  captura periodica fb_N.raw + religa ES). O device NUNCA mais trava sem saida.
- ✅ O HK ABORTAVA num check de REQUISITOS DE HARDWARE ("Your device does not match the hardware
  requirements of this application") por detectar GLES2 -> mostrava AlertDialog -> travava.
  O **SPOOF** (glGetString="OpenGL ES 3.0") PASSA o gate.
- ✅ STUBS p/ GLES3 ausentes (em my_dlsym): `glGetInternalformativ` (zera params, mata o loop MSAA),
  `glTexStorage2D` (ALOCA via glTexImage2D!), `glFenceSync/glClientWaitSync/glGetSynciv` (fingem
  signaled), resto=noop. + recuperacao de null-call no on_segv (cap 200k = trava de seguranca).
- ✅ **TRADUTOR GLES3->GLES2** no my_glShaderSource (str_rep): `#version 300 es`->`100`,
  `in/out`->`attribute/varying`, `texture()`->`texture2D()`, `SV_Target0`->`gl_FragColor`,
  layout-out removido. **OS 18 SHADERS COMPILAM (compile_err=0, SEGV=0)!** Salvos em
  shdump/*.glsl (orig) + *.gles2 (traduzido).
- ✅ A engine RODA FUNDO (1300+ linhas): "Restored language" + Team Cherry + loop de input
  "GamePad Name TInputCustom" = **A MESMA TELA DE IDIOMA do trabalho GLES3, mas no Mali-450/GLES2!**
- 🚧 TELA AINDA PRETA: `eglSwapBuffers=0` (NUNCA apresenta!). `nativeRender(frame 1)` BLOQUEIA
  ANTES do swap (sem "render frame 120 vivo"). NAO eh sync (stubs de sync nao destravaram). Frames
  capturados = so uns pixels faint (max ~14). Suspeita: parede de input/UI da tela de idioma OU um
  job/worker (pthread ON) estolando num GL stubado -> main espera o worker -> trava antes do swap.

### Proximo (continuar daqui)
1. Achar ONDE nativeRender(frame 1) bloqueia ANTES do eglSwapBuffers: logar cada chamada GL/jni
   dentro do frame 1 (ou tracepoint no player loop) -> ver a ULTIMA antes do bloqueio.
2. Em GLES3 a tela RENDERIZAVA (o swap acontecia); aqui trava ANTES do swap = bloqueio
   GLES2-especifico (um stub que a engine espera completar). Candidatos: worker de upload async
   (glTexStorage3D no-op), um glClientWaitSync num caminho diferente, ou um glFinish/glReadPixels.
3. Tudo seguro p/ iterar (saferun.sh). Shaders traduzidos OK. O caminho ate aqui esta SOLIDO.

## Atualização 2026-06-08 (parte 4) — 2o swappy destravou + 8 frames apresentados; render quase-preto
- ✅ SWAP CONFIRMADO: SWAPCALL=8, SWAP=8 (real_gl=1, ctx ok, pbuf=0). A engine APRESENTA de verdade
  no /dev/fb0 (nao era problema de present). Log do swap ligado em egl_shim.c ([SWAPCALL]/[SWAP]).
- ✅ **2o patch swappy (0x6927c4: cbz->NOP)** adicionado ao caminho NODRIVER em main_recon.c (faltava;
  so o ramo do driver via apply_swappy_patches tinha os 2). Destravou: 3 -> 8 frames; brilho 14 -> 98.
- 🚧 Trava AINDA no setup do **UnityChoreographer** no frame ~8: HandlerThread.start + getLooper +
  Choreographer$FrameCallback. Depende do Looper Android real (stubado) -> o frame-callback nunca tica.
- 🚧 RENDER produz QUASE-PRETO: 8 frames apresentados mas so specks esparsos (bbox cresceu p/
  y225-729, mas ~0.02% nao-preto). A cena/UI nao desenha visivel.

### Proximo (continuar daqui)
1. RENDER-PIPELINE (a chave p/ a imagem): por que texturas/sprites saem pretos?
   - Conferir se o format do nosso `stub_glTexStorage2D` (glTexImage2D NULL) CASA com o
     glTexSubImage2D que a Unity usa depois (senao GL_INVALID_OPERATION -> textura preta).
   - Ver se as texturas do HK sao COMPRIMIDAS (ETC/ASTC -> glCompressedTexImage2D, outro caminho).
   - Ver se HK usa FBO/render-target (render-to-texture) que nao esta compondo na tela.
2. UnityChoreographer getLooper block: emular Looper/FrameCallback OU patchar o wait do getLooper.
3. Base solida: saferun.sh seguro, 8 frames presentes, shaders traduzidos compilam. Cada sessao avanca.

## Atualização 2026-06-08 (parte 5) — glMapBufferRange emulado; render sobe p/ cor viva (mas ~12 px)
- ✅ MAPA das funcoes GLES3 que o HK usa (do log [gl stub]): glMapBufferRange, glUnmapBuffer,
  glVertexAttribIPointer, glUniform1/2/3/4uiv, glUniformBlockBinding, glGetUniformBlockIndex,
  glGetUniformIndices, glTexStorage2D/3D, glTexImage3D, glTexSubImage3D, glTexImage2DMultisample,
  glRenderbufferStorageMultisample, glInvalidateFramebuffer, glSamplerParameteri, glProgramBinary,
  glProgramParameteri, glReadBuffer, glMapBufferRange, glIsVertexArray, glGetTexLevelParameteriv.
- ✅ **EMULEI glMapBufferRange/glUnmapBuffer** (malloc -> Unity escreve -> glBufferSubData no unmap):
  a geometria dinamica sobe. MAPBUF=8. Conteudo: brilho 73 -> **max (152,192,198) cor viva**.
- ✅ Corrigi formato do glTexStorage2D (RGB565=0x8D62 -> GL_RGB+5_6_5; RGBA4/RGB5_A1 c/ os tipos).
- ✅ Diagnosticos: glTexSubImage2D (err=0x500 INVALID_ENUM persistente, origem nao achada),
  glCompressedTexImage2D (CTEX=0, NAO comprimido), glCheckFramebufferStatus (HK nao chama -> 0).
- 🚧 Render ainda ~12 pixels (quase preto). Causas SOMADAS provaveis:
  (a) loop para cedo (~11 frames) no bloqueio UnityChoreographer/getLooper -> nao chega nos frames
      do splash/logo; (b) so ~3 glDrawElements; (c) glVertexAttribIPointer no-op pode quebrar
      geometria com attrs inteiros; (d) err=0x500 de enum GLES3.

### Proximo (frentes do render, em ordem de aposta)
1. **UnityChoreographer/getLooper**: destravar o loop p/ MUITOS frames (so ~11 hoje). Emular o
   Looper/FrameCallback do Android OU patchar o getLooper. Mais frames = mais chance de aparecer.
2. **glVertexAttribIPointer**: implementar via glVertexAttribPointer (se HK usa attrs inteiros).
3. Origem do err=0x500 (wrap glGetError por-call) + contar glDrawElements/attribs reais.
4. FBO: HK faz 3 binds de framebuffer -> ver se renderiza numa RT e nao compoe na tela.
- Base SOLIDA e SEGURA p/ iterar (saferun.sh). Cada camada GLES3 emulada = mais perto.

## Atualização 2026-06-08 (parte 6) — estudo Vita + RE do Choreographer (a parede final)
- PS Vita (PatnosDD): usa a versao **PC (Steam/GOG)**, NAO a Android -> foge do runtime Android todo.
  Exige build Unity-pra-ARM (a Vita tem; nos nao). => nosso caminho (Android so-loader, IL2CPP
  ja-ARM64 do APK) e o certo p/ ARM64. A PAREDE confirmada = runtime Android (Choreographer).
- RE do Choreographer (UnitySwappy em libunity): 4 pthread_cond_wait na regiao swappy
  (0x690f3c, 0x6915a8, 0x6927bc, 0x693124). 2 patchados (0x690f2c, 0x6927c4).
  - **0x693124 = O BLOQUEIO**: apos pthread_create da thread "UnityChoreographer" (@0x693100), o
    main faz cond_wait esperando a thread setar a flag-pronto [x20+32] (zerada em 0x6930a8). A thread
    roda o loop do Choreographer (libunity) que precisa de Looper Android real (JNI stubado) ->
    nunca seta -> trava. Skipar (0x693118, gated HK_SWAPPY34) NAO resolve (a thread e necessaria).
    SWAP count e VARIANTE (3-11) -> nao serve de metrica.
- VEREDITO: furar = **EMULAR o Looper/FrameCallback** (thread propria que dispara o vsync ~60Hz)
  OU fazer a Unity nao usar o Choreographer (cair no sleep-pacing ja patchado).

### Proximo (concreto)
1. Emular Looper: no jni_shim, na HandlerThread/getLooper/Choreographer.postFrameCallback, prover um
   loop que chama doFrame(~16ms) periodicamente -> a thread seta a flag -> main destrava -> render roda.
2. OU patchar o ponto de decisao "usar Choreographer?" p/ cair no fallback.
3. Render preto (~12px) em paralelo: com mais frames pos-Choreographer pode aparecer.

## Atualização 2026-06-08 (parte 7) — 🏆 PIPELINE DE RENDER FUNCIONA (force-red = VERMELHO na tela!)
- HK_SWAPPY34 agora default-ON (gate HK_NOSWAPPY34 desliga): passa do Choreographer, carrega cena
  (OnLevelWasLoaded GameMap), idioma EN, sobe geometria grande (MAPBUF 67584 bytes).
- draw_err=0: os glDrawElements (GL_TRIANGLES, GL_UNSIGNED_SHORT) SAO VALIDOS. pre=0x500
  (INVALID_ENUM) e acumulado de enum GLES3 anterior (inofensivo ao draw).
- 🏆 **HK_FORCERED (fragment -> vec4(1,0,0,1)) = PIXELS VERMELHOS (max 255,0,0)!** O pipeline
  (geom dinamica via emul_glMapBufferRange + shaders traduzidos GLES3->GLES2 + present
  eglSwapBuffers->SDL_GL_SwapWindow->fb0) FUNCIONA. A tela PODE mostrar.
- 🚧 Poucos pixels (cena esparsa, ~24 draws count 6/3 = sprites pequenos). Hipoteses: jogo ainda
  carregando, OU texturas pretas (sprite preto=invisivel) -> testando HK_FORCETEX.

### Envs de debug (no tradutor translate_gles3_gles2 em recon_egl.c)
- HK_FORCERED=1: fragment vermelho (testa geometria/pipeline). **PASSOU.**
- HK_FORCETEX=1: fragment = texture2D(_MainTex) (testa se a textura subiu).
- HK_NOSWAPPY34=1: desliga os patches 3/4 do Choreographer.

## Atualização 2026-06-08 (parte 8) — RENDER PIPELINE CONFIRMADO; render esparso = matrizes/estado
DIAGNOSTICO PROFUNDO (force-X tests + diag wrappers no my_dlsym em recon_egl.c):
- 🏆 HK_FORCERED (fragment->vermelho) = PIXELS VERMELHOS -> o pipeline (geom + shader traduzido +
  present eglSwapBuffers->fb0) FUNCIONA de verdade.
- HK_FORCETEX (->texture2D) = greenish -> a TEXTURA sobe OK (NAO e preta).
- HK_FORCECOL (->vs_COLOR0) = cinza 0.5 -> a COR do vertice CHEGA no shader.
- draw_err=0: glDrawElements (GL_TRIANGLES, count 6/3 = quads/sprites) SAO VALIDOS. pre=0x500
  (INVALID_ENUM acumulado de um enum GLES3 anterior, inofensivo ao draw).
- Vertices (dump no emul_glMapBufferRange): buffer com dados REAIS (5001/16896 nao-zero = normal 2D z=0).
- 🔑 MATRIZES (glUniform4fv count=4 = mat4): VARIAM por draw. loc1 ObjectToWorld=[0,0,0,0;0,0,0,0;..]
  (DEGENERADA), loc5/7 MatrixVP=IDENTIDADE, loc3=[0.02,0;0,0.04;..] (ortho-ish escala 0.02/0.04).
  Algumas matrizes reais, outras zeradas/identidade.
- => render ESPARSO/minusculo: ~24 draws de sprites pequenos = poucos pixels visiveis. A cena cheia
  nao montou. As matrizes parecem de ESTADO DE LOADING/TRANSICAO (camera real nao montou). O jogo
  carrega DEVAGAR (DRAW cresce 7->24 com o tempo).

### Envs/ferramentas de debug (recon_egl.c)
- Tradutor: HK_FORCERED (fragment vermelho), HK_FORCETEX (=textura), HK_FORCECOL (=cor vertice).
- diag wrappers (my_dlsym): glDrawElements (pre/draw_err), glUniform4fv/Matrix4fv (dump matriz),
  glTexSubImage2D (format/err), glCheckFramebufferStatus, glMapBufferRange/Unmap (emul + dump vtx).

### Proximo (o render FUNCIONA; o gargalo e o jogo EXECUTAR ate uma cena cheia)
1. A cena enche com tempo? (run 200s -> ver se DRAW/pixels crescem = loading lento, esperar/otimizar).
2. Se estagnar: jogo travado em menu/loading (input wall conhecido) -> poucos elementos. Avancar de
   estado precisa do input nativo OU as matrizes de gameplay nunca montam sem o runtime completo.
3. MatrixVP=identidade: confirmar se e by-design (jogo pre-multiplica no ObjectToWorld) ou bug nosso.

## Atualização 2026-06-08 (parte 9) — nOnChoreographer achado (mas precisa de contexto); diagnostico final do estado
- Achei o native do vsync: `Java_com_google_androidgamesdk_ChoreographerCallback_nOnChoreographer`
  @ unity+0xdddb90. DISASM: `ldr x8,[x2]; mov x0,x2; ldr x1,[x8+32]; br x1` = THUNK que faz
  virtual-call no x2 (3o arg JNI = OBJETO C++ Swappy/FrameTimeTracker, NAO o frametime). Chamar com
  x2=frametime cru CRASHA. Precisa do OBJETO de contexto real (que a Unity cria). Gated HK_CHOREO (off).
- ESTADO REAL (confirmado): o jogo CARREGA (GameMap, language EN) via nativeRender -> a logica
  AVANÇA. O render pipeline FUNCIONA 100% (force-red=vermelho na tela). MAS renderiza so uns sprites
  pequenos/escuros (cena esparsa) e ESTAGNA (200s = nao enche). Matrizes: MatrixVP setado por
  glUniform4fv como IDENTIDADE (loc5/7), ObjectToWorld varia (loc1 zerada, loc3 ortho 0.02/0.04).
  => o jogo esta num estado de SPLASH/loading inicial (Team Cherry?), nao monta a cena cheia.
- CONCLUSAO: render GLES2 = RESOLVIDO (inedito). O gargalo agora e o jogo EXECUTAR ate uma cena
  cheia, bloqueado pelo runtime Android (Choreographer/async-load/input) -- a mesma parede
  arquitetural do trabalho anterior, agora atras de um render que FUNCIONA.

### Proximo (multi-sessao)
1. Drive do vsync: achar/passar o OBJETO Swappy p/ nOnChoreographer (RE do FrameTimeTracker), OU
   emular o Looper Java de verdade no jni_shim (proxy doFrame).
2. Investigar por que a cena nao monta: o async-load do GameMap depende do frame-callback? (coroutine
   yield gated no Choreographer). Tracear o que o jogo espera.
3. Matrizes degeneradas: confirmar se sao de loading-state (resolvem ao avancar) ou bug de UBO.
- Build estavel (HK_CHOREO/HK_SWAPPY34/HK_FORCE* todos gated). saferun.sh seguro.

## Atualização 2026-06-08 (parte 10) — DIAGNOSTICO DEFINITIVO: render OK, estado do jogo e PARCIAL
Bateria de testes decisivos (todos gated em recon_egl.c/egl_shim.c):
- HK_CLEARTEST (glClear AZUL antes do swap): **AZUL ENCHE 89% da tela** -> present/viewport/display
  FUNCIONAM 100%. (89% = meu viewport usou g_win_h, nao a parede.)
- glViewport/glScissor do jogo = **0,0,1280,720 CHEIOS** (sem recorte). Nao e clip.
- Vertex shader traduzido (sh_000.gles2) = **CORRETO**: gl_Position = MatrixVP*(ObjectToWorld*pos),
  atributos in_POSITION0/COLOR0/TEXCOORD0 certos, varyings ok.
- Geometria sobe sem erro (glBufferData, upload_err=0).
- HK_VTXRAW (gl_Position = in_POSITION0, bypassa matrizes) = **PRETO TOTAL** -> as posicoes sao
  world/object-space e PRECISAM das matrizes. As matrizes trazem os poucos draws onscreen.
- MATRIZES (glUniform4fv): MatrixVP setado, ObjectToWorld = ortho real (loc3) p/ ALGUNS draws,
  ZERO (loc1) p/ MUITOS. Os draws com ObjectToWorld=0 -> degenerados (nao renderizam).

CONCLUSAO DEFINITIVA: **o RENDER GLES2 funciona** (present, shader, viewport, geometria, matrizes
nao-degeneradas -> tudo certo). O render e ESPARSO porque o JOGO sobe matrizes ObjectToWorld=0 p/
muitos objetos = **o jogo NAO montou a cena toda (estado PARCIAL)**. null_recov=20 (baixo, nao e a
recovery corrompendo). Da outra vez (GLES3 .103) o jogo progredia ATE a cena cheia; aqui (GLES2 .87)
progride MENOS -> menos objetos posicionados -> render esparso.

=> A PAREDE e o JOGO PROGREDIR (runtime Android: Choreographer/async-load/coroutines), NAO o render.
   Render = RESOLVIDO. Confirmado por exclusao total (present+shader+viewport+geometria+matriz testados).

### Caminhos restantes (todos = fazer o jogo progredir mais)
1. Choreographer real: nOnChoreographer @ unity+0xdddb90 e thunk que precisa do objeto Swappy C++
   (virtual-call em x2). Achar/forjar esse objeto -> dirigir o vsync -> coroutines/async avancam.
2. Emular o Looper Java no jni_shim (HandlerThread/getLooper/postFrameCallback + disparar doFrame).
3. Tracear o que o jogo espera no estado parcial (qual yield/await nao resolve).

## Atualização 2026-06-11 (parte 10) — retomada pós-Bogodroid: HK_PTHREAD_SHIM=1 é OBRIGATÓRIO
- Voltamos pro nosso loader (decisão do Felipe: controle total > Bogodroid fechado, que confirmou
  a parede do GfxDevice mas não dava pra patchar). Binário+assets ainda em /storage/hollow-recon/ (.87).
- 🔑 **CRASH REPRODUZIDO + DIAGNOSTICADO:** rodar SEM `HK_PTHREAD_SHIM=1` → SEGV `lr=libil2cpp+0x68014c`.
  Desmontagem (apkextract): `libil2cpp+0x680148 = bl pthread_cond_wait@plt` (x0=cond x24, x1=mutex x19;
  incrementa contador [x21+96] antes). = **ABI bionic↔glibc**: libil2cpp passa pthread_cond_t BIONIC
  (4B) p/ o pthread_cond_wait do glibc (48B) → lê lixo → fault. **FIX = HK_PTHREAD_SHIM=1** (traduz
  bionic→glibc, já implementado em pthread_shim.c). As vitórias das partes 3-9 usavam o shim.
- ⚠️ Run COM o shim **wedgou o device** (GPU/kernel travou, SSH inacessível, precisou power-cycle do
  Felipe) ANTES de eu confirmar se passou do crash. Confirmar no próximo run pós-reboot.
- ENV CORRETO p/ rodar: `HK_GLES2=1 HK_PTHREAD_SHIM=1 SDL_VIDEODRIVER=mali NODRIVER=1 GC_DONT_GC=1 sh saferun.sh 70`.
- Insight do Bogodroid (FakeJni VM completa) p/ o muro de INPUT da tela de idioma — explorar depois.
- Desmontagens cacheadas: ~/hollow-port/apkextract/lib/arm64-v8a/{libunity.so,libil2cpp.so} + /tmp/unity.dis.

## Atualização 2026-06-11 (parte 11) — ANTI-WEDGE implementado (build local, aguardando device)
- Run com HK_PTHREAD_SHIM=1 wedgou o device de novo (2o power-cycle do dia) ANTES de lermos o log.
- MITIGAÇÕES novas (recon_egl.c + egl_shim.c), todas baseadas em wedges já resolvidos em outros ports:
  - **glFinish → no-op** nos DOIS caminhos (my_dlsym + eglGetProcAddress). Religa: HK_ALLOWFINISH=1.
    (Bully: "glFinish satura/wedge" o Utgard.)
  - **HK_SWAPMS=N**: usleep N ms após SDL_GL_SwapWindow → fila de comando da GPU não acumula.
  - **HK_TEXCAP=N**: textura > NxN é alocada reduzida (shift) no stub_glTexStorage2D + upload
    downsampled point-sample no diag_glTexSubImage2D (rastreia bound tex via my_glBindTexture).
    (Cuphead: texturas 2048² travavam o Utgard → CUP_TEXHALF=512.)
- ⚠️ DESCOBERTA operacional: ~/.ssh/config mapeia 192.168.31.87 → HostName **192.168.31.89**.
  O device REAL é .89; ping em .87 não diz nada. Pingar/scriptar SEMPRE .89.
- ENV do próximo run (curto, 40s): HK_GLES2=1 HK_PTHREAD_SHIM=1 HK_SWAPMS=20 HK_TEXCAP=1024
  SDL_VIDEODRIVER=mali NODRIVER=1 GC_DONT_GC=1 sh saferun.sh 40
- 1o passo pós-reboot: LER run.log do wedge (sync 1s flushou) → onde travou decide o resto.

## Atualização 2026-06-11 (parte 12) — 🔑 INSTANCING descoberto (provável causa do render esparso) + emulação implementada
ANÁLISE DOS DUMPS (shdump/*.glsl): 4 dos 18 shaders (sh_006/010/014/016) são variantes de
**INSTANCING de sprite** com UBO REAL: `UNITY_BINDING(0) uniform UnityInstancing_PerDraw0
{ unity_Builtins0Array[2] }` (ObjectToWorld+WorldToObject por instância!) + `UnityInstancing_PerDrawSprite`
(cor+flip) + `gl_InstanceID`. **Com o spoof GLES3, a Unity desenha sprites por glDrawElementsInstanced
(era stub no-op SILENCIOSO = draws descartados!) com matrizes via UBO (stub = nunca chegam).**
=> explica o render esparso E possivelmente as ObjectToWorld=0 (sprites instanciados sumiam).
IMPLEMENTADO (recon_egl.c):
1. Tradutor: `#define HLSLCC_ENABLE_UNIFORM_BUFFERS 1→0` (o próprio boilerplate hlslcc remove os
   blocos UBO e os membros viram uniforms PLANOS struct-array, válido ES2); `gl_InstanceID`→
   `uniform int u_hk_instID` (injetado pós-#version); `<< int(N)`→multiplicação (shift é proibido
   em GLSL ES 100 — esses 4 shaders NUNCA compilaram antes, silenciosamente).
2. UBO shadow CPU: my_glBindBuffer/my_glBufferData/my_glBufferSubData/emul_glMapBufferRange
   desviam target 0x8A11 pra cópia CPU (não toca o driver GLES2 = sem INVALID_ENUM; o pre=0x500
   misterioso dos draws provavelmente era isso). glBindBufferRange/Base registram binding→buffer.
3. emul_glDrawElementsInstanced/ArraysInstanced: sobe o shadow dos 2 blocos como uniforms planos
   (layout std140 hardcoded: Builtins0=128B/inst, Sprite=32B/inst) + loop u_hk_instID=i + draw.
4. jni_shim: **NewObject/V/A implementados (slots 28-30)** capturando o COOKIE (jlong this C++)
   do ChoreographerCallback → g_hk_choreo_cookie → main_recon HK_CHOREO agora chama
   nOnChoreographer(env,clazz,cookie,frameTime) com o objeto REAL (antes crashava com lixo).
Build OK. tools/next-run.sh automatiza: resgata log do wedge → deploya → roda 40s anti-wedge → coleta.

## Atualização 2026-06-11 (parte 13) — 🏆 RODA CONTÍNUO 30fps no .89 + lições de memória
- **RUN5 (360s, TEXCAP=512 estilo Cuphead, SEM zram) = MELHOR RUN DA HISTÓRIA no Mali-450:**
  loading completo t≈67s (464MB de atlases downsampled), depois **7650 frames contínuos ~30fps**,
  watchdog drain limpo (rc=70), device saudável (load 1.62), ES religou. Tela de IDIOMA alcançada
  ("Restored language code EN" + draws de 42/48 índices = glifos de texto).
- **pre=0x500 (INVALID_ENUM) SUMIU** — era o glBindBuffer(GL_UNIFORM_BUFFER) no driver GLES2;
  o shadow de UBO comeu o bind. Draws agora pre=0x0 limpos.
- **fb: 6 pixels verdes** em (639-641, 240) e (639-641, 720) — 2 pontinhos de 3px, x=centro.
  Matrizes estado estável: loc3 ora ortho saudável [0.02,0.04] (=orthoSize 25, aspect 2.0 ≠ 1.78
  real — screen 2:1 em algum lugar?), ora DEGENERADA (colunas x/y zeradas, translação -1,-1).
  loc5/7 (MatrixVP) = identidade (Unity premultiplica). INST=0 (tela de idioma não usa instancing).
  Cookie do Choreographer NUNCA criado (NewObject(ChoreographerCallback) não acontece — a thread
  Java do Looper não roda; criação depende dela?).
- **zram REPROVADO**: run6 com zram 768MB estagnou t=19s no pico do loading (compressão rouba RAM
  da Unity em device de 832MB). saferun agora FORÇA zram OFF. Swap vfat-loop (run5) funciona.
- **GC_INITIAL_HEAP_SIZE 512MB→128MB** (HK_GCHEAP override) — 512MB pré-alocado afogava o .89.
- Watchdog gracioso (drain 2s pós-loop) FUNCIONA: rc=70 limpo em run5; o "_exit duro" do run6
  (preso dentro do nativeRender) também NÃO wedgou.
- PRÓXIMO: run7 = run5 + diags periódicos (matrizes/draws no estado estável); depois HK_FORCERED
  p/ discriminar geometria-fora-da-tela vs fragment invisível na tela de idioma.

## Atualização 2026-06-11 (parte 14) — runs 6/7 estagnam t=19s (NÃO era zram) + 2 fixes novos
- run7 (zram OFF) estagnou IGUAL ao run6: t=19s, pós TEXSUB-CAP #9, sistema INTEIRO congela
  (até o `echo >> mem.log` do shell para) com **338MB disponíveis** = NÃO é OOM. Destrava quando
  o processo morre (_exit duro) → família WEDGE do driver Mali (fila/lock), reversível.
  run5 idêntico passou = não-determinístico; run5 era o 1º run pós-boot (device fresco).
- **FIX 1 (UAF de upload)**: nosso downsample fazia free() imediato pós glTexSubImage2D; Utgard
  pode adiar a leitura pro flush → use-after-free no driver. Agora glFlush pós-upload + anel
  de 8 buffers antes do free.
- **FIX 2 (DisplayMetrics)**: GetIntField/GetFloatField NÃO EXISTIAM (stub 0) → widthPixels/
  heightPixels/density = 0 via JNI → suspeita raiz das matrizes ortho DEGENERADAS (escala 0 =
  ortho com largura infinita/lixo). Implementados (GetFieldID agora usa o registry de nomes;
  width/height vêm do egl_shim; density 1.5/240dpi; vtable 100/101/102).
- run8 = run5 env + os 2 fixes. Se as matrizes sararem → tela de idioma VISÍVEL?

## Atualização 2026-06-11 (parte 15) — 🔥 MATRIZES SARARAM (DisplayMetrics!) + receita Cuphead completa
- run9 (boot fresco, fix DisplayMetrics FIELDS): loading no ritmo do run5, 7410 swaps contínuos,
  e a partir de t=94s **matrizes REAIS**: perspectiva [2.65/4.70] (aspect 1.77 ✓ curado!), objetos
  com transform de verdade. ANTES: identidade/degenerada. **CAUSA-RAIZ era widthPixels=0 via JNI.**
- glFlush por upload (run8) REPROVADO: wedge mais cedo (t=4s). Substituído por anel de 8 buffers +
  usleep 10ms entre uploads grandes (HK_TEXSLEEP_MS).
- **fb0 do .89 DECODIFICADO: 1280x720 stride 1280, DUPLA PÁGINA** (não 1920x1080!). Reinterpretado:
  o conteúdo visível real é um pontinho no CENTRO (639-641, 358-360). Objetos calculados ~350px
  não chegam ao screen → suspeita: cena renderiza num RT (TEXSTOR 1280x720 RGB565 0x8d62 existe)
  e o composite final é o elo quebrado. FBO CheckStatus nunca é chamado pelo jogo.
- **RECEITA CUPHEAD portada 100%** (o port Unity que RENDERIZA neste hardware): Display.getWidth/
  getHeight/getRotation/getDisplayId no CallIntMethod (0 = "Unable to initialize Unity Engine") +
  CallFloatMethod getRefreshRate→60Hz (0 quebra o engine) + DisplayMetrics fields (já tinha).
- run10 = build com a receita completa. Plano B sem rebuild: HK_FORCERED discrimina geometria vs
  fragment; plano C: investigar o composite RT→screen (RGB565 + GLES2).

## Atualização 2026-06-11 (parte 16) — VEREDITO FORCERED + ponto de retomada EXATO
- run10 (receita Cuphead completa: Display.getWidth/getHeight/getRotation/getDisplayId +
  getRefreshRate 60Hz + DisplayMetrics): estável 7260 frames, respostas consumidas (9x), mas
  ainda ~6 px no fb (cores variando = conteúdo vivo).
- **run11 HK_FORCERED=1: 1 PIXEL VERMELHO PURO no centro** → NÃO é cor/textura/blend; a
  geometria do passe de TELA colapsa num ponto. (Run wedgou em t=55s no loading — 3º run
  pós-boot; padrão: 1º-2º runs pós-boot são os seguros.)
- **MATEMÁTICA-CHAVE (run9 matrizes saudáveis):** objeto típico projeta em NDC (1.19, -1.07) =
  POR UM FIO fora da tela (|1.0|=borda). Conteúdo existe e renderiza, mas ~20% "zoom-in demais".
  Suspeitos: FOV/safe-area/insets via JNI, ou View matrix deslocada.
- **PRÓXIMO PASSO PRONTO (build ok, NÃO testado):** run12 com `HK_VTXZOOM=1` (novo: vertex
  shader ganha gl_Position.xy *= 0.5 = zoom-out 2x) + rastreio de FBO nos DRAWs (fbo= no log,
  glBindFramebuffer interceptado). Se a cena aparecer em miniatura → confirmado "logo ali fora",
  caçar o fator na fonte (getSafeInsets? Camera.fov? View). Se nada → investigar composite RT
  (TEXSTOR 1280x720 RGB565) → tela.
- ENV run12: HK_VTXZOOM=1 HK_GLES2=1 HK_PTHREAD_SHIM=1 HK_TEXCAP=512 HK_SWAPMS=20
  SDL_VIDEODRIVER=mali NODRIVER=1 GC_DONT_GC=1 sh saferun.sh 300 (rodar como 1º run pós-boot!)
- fb0 do .89: 1280x720 BGRA stride 1280 DUPLA página; tools/fb2png.py decodifica certo.
