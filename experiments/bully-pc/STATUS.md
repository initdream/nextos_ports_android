# bully-pc — STATUS

## ✅ FEITO (roda no PC, x86_64)
- **Loader x86_64 multi-módulo** (`so_util_x64.c`): carrega `libc++_shared.so` (companion) + `libGame.so`, relocate (R_X86_64_*, com fix ABS64-UNDEF), resolve.
- **Resolver**: companion (so_lookup_global) → shims bionic/NDK (`imports.c`) → host dlsym (libc/m/GLESv2/openal/z). **Resultado: libGame.so 0 UNRESOLVED.**
- **23 shims** implementados: bionic (`__errno`,`__sF`+wrappers stdio,`__str*_chk`,`__assert2`,`_ctype_`), `__android_log_print`, NDK `ANativeWindow_*` (→SDL), `AAsset*`/`AAssetManager_*` (→fopen em ASSET_DIR), `_ZTH*` (no-op), `NVThreadGetCurrentJNIEnv`.
- **504 init_array** do libGame rodam sem crash.
- **`JNI_OnLoad` roda → 0x10004** (JNI_VERSION_1_4).
- egl_shim (SDL2 GLES2 ctx 1280x720), jni_shim (JNI env 64-bit), build CMake PC.

## 🎯 ACHADO-CHAVE (corrige o rumo)
A **v1.4.311 NÃO usa RegisterNatives** (≠ bully_vita antigo). Usa **JNI estático**: exporta 38
`Java_com_rockstargames_oswrapper_GameNative_impl*`. `IsAndroidPaused` não existe mais.
→ Não dá pra dirigir via `jni_load`/RegisterNatives. Tem que **chamar as funções impl* na ordem certa**.

## 🟢 bully-NX = REFERÊNCIA EXATA da v1.4.311
`givethesourceplox/bully-NX` (Switch) usa **este mesmo `libGame.so` 64-bit** e tem o **driver completo**
(salvo em `ref-bully-NX/`, 6815 linhas). A sequência (`jni_patch.c` jni_start, ~linha 1244):
```
implOnInitialSetup(env, NULL,NULL,NULL,NULL,NULL)   // init AND_* subsystems; seta flag 0x158aad4
force gates: OS_IsGameSuspended=0, OS_CanGameRender=1, implIsInitialized=1
implOnActivityCreated(env, NULL, 0x42424242, 1)
eglMakeCurrent(NO_SURFACE)  // solta ctx do main; render thread pega
implOnSurfaceCreated(env, NULL)
implOnSurfaceChanged(env, NULL, NULL, W, H)
implOnResume(env, NULL)
[async file worker thread]
loop @60Hz: implOnDrawFrame(env, NULL, delta)   // render
```
Assinaturas exatas das impl* estão no topo do jni_patch.c do bully-NX. Endereços de gate
(`0x158aad4`, `0x158a960`, `0x126bb70/74`) ancorados por símbolo+offset (`srp - 0x174`) → transferem
se o build bater. Nosso libGame BuildID=`6139a628...` (validar vs bully-NX na implementação).
Entradas nativas tb: `_Z8GameInitb` (GameInit), `_Z9NvAPKInit...` (assets, é HOOKADO no NX → ignora apk/obb e usa asset_archive próprio).

## ▶️ PRÓXIMO (portar o driver do bully-NX, Switch→Linux/SDL)
1. Trocar `jni_load` (RegisterNatives) pelo **driver impl*** (sequência acima) — resolver os símbolos via `so_symbol`.
2. Portar `asset_archive.c` do bully-NX (lê dos OBB/APK reais; NvAPKInit hookado) — copiar `main.obb`/`patch.obb` pro `gamefiles/` (estão no zip do Felipe).
3. Threading: o jogo cria GameMain + render thread; tratar handoff de contexto EGL (no NX solta o ctx no main e a render thread pega). No PC/SDL: SDL_GL context + multiplos contextos compartilhados OU rodar GL na thread certa.
4. Gate flags + `OS_ScreenGetWidth/Height` + `sync_engine_egl_globals` (ver bully-NX).
5. 1º frame no PC → depois rebuild aarch64 (so_util do core) + Mali + empacotar.

## Risco
Driver multi-thread + endereços build-específicos = trabalho médio-alto, MAS todo o RE já existe no bully-NX (open). Não é descoberta (≠ Hollow); é **tradução** Switch→Linux.

---

## 🎉 MARCO 2026-06-03: init ponta-a-ponta no PC + dados v1.4.311 achados

**Dados resolvidos:** o `.apkm` v1.4.311 tem **`split_data_1.apk` (1GB)** com `assets/data_0.zip`+`data_1.zip`+`.idx` = dado real do jogo (Play Asset Delivery). Estagiado em `gamefiles/assets/` (1.1GB). O OBB antigo do Felipe (2022, layout `Bully/*.msh`) NÃO serve; a v1.4.311 usa os data zips.

**Driver estático portado** (jni_shim.c reescrito): resolve os 38 `Java_..._impl*` por símbolo, ancora gates em `StorageRootPath` (init=srp-0x174/susp=-0x17c/render=-0x2e8), hooka 11 `NvAPK*`→`asset_archive.c` (vendorizado do bully-NX, lê dos data zips), `AttachCurrentThread`/`GetEnv` na fake_vm, dispatchers JNI com **`va_list`/`va_arg`** (NÃO uintptr_t* — crash 64-bit), métodos has/get/setAppLocalValue+getParameter.

**RESULTADO (roda no PC):** `implOnInitialSetup OK` → gates(1,0,1) → `implOnActivityCreated OK` → **GL ctx 1280x720 (ES 3.2)** → `implOnSurfaceCreated/Changed` → `implOnResume` → **entra no loop `implOnDrawFrame`** → `AND_RenderThread_OnCreateEglSurface` (spawna render thread) + `OS_RockstarShowInitial` → **SIGSEGV**. Dezenas de threads sobem (GameMain/render/áudio). asset_archive lê os data zips.

**BLOQUEIO ATUAL (= as 2 partes difíceis que faltam portar do bully-NX):**
1. **Gate online Rockstar**: o jogo chama `OS_RockstarShowInitial` e ESPERA o callback `OS_OnRockstarInitialComplete`/`OS_OnRockstarGateComplete`/`OS_OnRockstarSignInComplete` (no Android vem do Java async). bully-NX DEFERE esses no frame loop. Falta chamar.
2. **Handoff EGL pra render thread**: o jogo cria AND_RenderThread que quer a EGL surface; bully-NX faz `eglMakeCurrent(NO_SURFACE)` no main + sincroniza os globais `OS_EGLDisplay/Surface/Context` (srp-0x2d0/-0x2c8/-0x2c0) com os objetos EGL reais (`sync_engine_egl_globals`). No PC/SDL precisa expor o EGLDisplay/Context do SDL e compartilhar com a render thread.

**PRÓXIMO:** portar do bully-NX (jni_patch.c): a deferral dos OS_OnRockstar* no loop + sync_engine_egl_globals + release/handoff do ctx EGL. Aí deve ir pro 1º frame. Endereços (srp±) provavelmente batem (mesmo build; validar). Tudo está em `ref-bully-NX/`.

---

## 🎉🎉 MARCO 2 (2026-06-03, mesma sessão): FRAME LOOP RODANDO + GATE ROCKSTAR PASSOU

Implementado e FUNCIONA no PC:
- **EGL real (pbuffer offscreen)**: NVIDIA+X11 não cria window surface (BAD_CONFIG/GLX); SDL forçado a EGL falhou tb. Solução bring-up: `eglGetDisplay(EGL_DEFAULT_DISPLAY)` + `eglCreatePbufferSurface` + ctx ES3 (Mesa llvmpipe ES 3.2) → objetos EGL reais (dpy/surf/ctx), setados nos globais OS_EGL* do jogo. (NÃO-visível ainda; depois trocar por window surface / no device é fbdev Mali.)
- **Handoff EGL**: `makeCurrent`/`unMakeCurrent`/`swapBuffers` JNI → eglMakeCurrent/eglSwapBuffers; main solta o ctx (`bully_release_current`) antes das surface callbacks (render thread do jogo pega).
- **Gate Rockstar** (deferido no frame loop, frame>30): `rockstarShowInitial/Gate` setam pending → loop dispara `OS_OnRockstarStateChanged(0)`/`OS_OnRockstarInitialComplete`/`OS_OnRockstarGateComplete(type,1)`/`OS_ApplicationEvent(9=Resume)`/`implOnRockstarSetup` + re-força gates; signin no frame>45.
- **2 BUGS RAIZ corrigidos (64-bit JNI):** (1) **variantes varargs** `CallObjectMethod`/Boolean/Int/Float/Void (idx 34/37/49/55/61 = offsets 0x110/0x128/0x188/0x1B8/0x1E8) faltavam (só tinha as `...MethodV`); o jogo usa AMBAS. (2) **`GetMethodID` desconhecido DEVE retornar não-zero** (usei 0x7777): o jogo faz `if(methodID)` antes de chamar (ex: `OS_GetAppVersion` → se ID=0 pula a chamada e faz `GetStringUTFChars(NULL)`→`strlen(NULL)`💥). Tb `GetStringUTFChars/NewStringUTF(NULL)→""`. + `AttachCurrentThread`/daemon na fake_vm (idx4/7=0x20/0x38) setam *env.

**RESULTADO:** roda `frame 0,1,2,3,4...31`, no frame 31 **gate Rockstar completa** (`OS_OnRockstarGateComplete: pass` + `OSET_Resume`), thread nova anexa → **crash DEPOIS do gate** (fase GameMain/loading pós-gate). Cada fix avança mais.

**PRÓXIMO (sessão dedicada):** orquestração GameMain pós-gate (bully-NX frame loop: forçar gamestate 0→2 em 0x12146a8→+0x68, tick flags 0x126bb70/74, g_gamemain_alive, async file worker) + tornar o render VISÍVEL (window surface no PC / fbdev no device). Crash atual: pós-`OSET_Resume`+AttachCurrentThread.

---

## 🎉 MARCO 3 (2026-06-03, mesma sessão): passa Rockstar gate, entra no GameMain
Mais fixes (cada um avança o jogo):
- **glGetString nunca-NULL** (shim em imports.c): `perfprofile::LoadRendererDetails` chamava glGetString na thread GameMain (sem ctx GL) → NULL → strlen crash. Wrapper retorna "" se NULL.
- **Hooks EGL surface lifecycle** (jni_shim hook_egl): `AND_CreateEglSurface`/`AND_DestroyEglSurface`/`OS_ThreadMakeCurrent` hookados → create=bully_make_current, destroy=no-op, pq o jogo tentava destruir/recriar a surface (no PC é pbuffer, não window) → `AND_DestroyEglSurface` abortava.
- Resultado: passa do abort do EGL; **thread "GameMain" sobe e roda** (é a thread de lógica/loading do jogo).

**CRASH ATUAL:** thread **GameMain** (pós OSET_Resume + AttachCurrentThread), SIGSEGV; backtrace embaralhado (stack precisa debug por-thread cuidadoso). Estamos na fase de **bring-up iterativo do GameMain** (loading/render setup) — cada crash = 1 fix (gdb thread GameMain + objdump no .so). bully-NX faz a orquestração: forçar gamestate, tick flags, async file worker. **Provável próximo: o GameMain precisa do contexto GL na thread certa (makeCurrent) E/OU dos tick-flags/gamestate forçados.** Tudo no ref-bully-NX/jni_patch.c (frame loop ~1349+).
