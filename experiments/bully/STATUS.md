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
2. Portar `asset_archive.c` do bully-NX (lê dos OBB/APK reais; NvAPKInit hookado) — copiar `main.obb`/`patch.obb` pro `gamefiles/` (estão no zip do o autor).
3. Threading: o jogo cria GameMain + render thread; tratar handoff de contexto EGL (no NX solta o ctx no main e a render thread pega). No PC/SDL: SDL_GL context + multiplos contextos compartilhados OU rodar GL na thread certa.
4. Gate flags + `OS_ScreenGetWidth/Height` + `sync_engine_egl_globals` (ver bully-NX).
5. 1º frame no PC → depois rebuild aarch64 (so_util do core) + Mali + empacotar.

## Risco
Driver multi-thread + endereços build-específicos = trabalho médio-alto, MAS todo o RE já existe no bully-NX (open). Não é descoberta (≠ Hollow); é **tradução** Switch→Linux.

---

## 🎉 MARCO 2026-06-03: init ponta-a-ponta no PC + dados v1.4.311 achados

**Dados resolvidos:** o `.apkm` v1.4.311 tem **`split_data_1.apk` (1GB)** com `assets/data_0.zip`+`data_1.zip`+`.idx` = dado real do jogo (Play Asset Delivery). Estagiado em `gamefiles/assets/` (1.1GB). O OBB antigo do o autor (2022, layout `Bully/*.msh`) NÃO serve; a v1.4.311 usa os data zips.

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

**CRASH ATUAL (atualizado):** thread **GameMain**, SIGSEGV em **`ZIPFile::Find(this=NULL)`** (libGame+0x11f33e5, `mov (%r14),%rax` com r14=rdi=NULL). Uma ZIPFile* ficou NULL e o jogo chamou ->Find() nela.

✅ **asset_archive CONFIRMADO funcionando**: indexou **1912 zip aliases** + IMG packs (act/598, scripts/532, stream/1787, objects/77 em data_0.zip). "missing data_2/3/4.zip.idx" é normal (só temos data_0/1). Logs em `gamefiles/debug.log` (debugPrintf escreve lá + stdout).

Então o ZIPFile::Find(NULL) NÃO é o asset_archive.

### RASTREADO (precisão total): ZIPFile::Find(NULL) ← OS_ZipFileOpen ← registro de zips vazio/NULL
- Crash: `ZIPFile::Find(this=NULL)` (libGame+0x11f33e5, `mov (%r14),%rax`, r14=this=NULL), na thread **GameMain**.
- Caller: **`OS_ZipFileOpen(path, &handle)`** (+0x107) — itera um **registro global de ZIPFiles** (begin=[0x16239a8], end=[0x16239b0]) e chama `ZIPFile::Find` em cada; uma entrada é NULL.
- O registro é populado por **`OS_ZipAdd(path)`** (exportado, `_Z9OS_ZipAddPKc`) que faz `ZIP_FileCreate(path)`. **xref=0: o libGame NUNCA chama OS_ZipAdd sozinho** — é API EXTERNA (o launcher Java registra os data zips). `AND_FileInitialize` usa NvFOpen/NvFRead/NvFIsApkFile.
- **TENTEI chamar OS_ZipAdd("data_0.zip")/"data_1.zip" no driver + symlink dos zips no cwd → NÃO resolveu** (ZIP_FileCreate ainda dá NULL; e o crash já ocorria sem isso). 
- **CAUSA REAL / FIX = portar o `zip_fs.c` do bully-NX**: ele intercepta **`fopen`** (via funopen + minizip) e serve os arquivos de DENTRO dos data_*.zip transparentemente — NÃO usa OS_ZipAdd. bully-NX combina: asset_archive (NvAPK) + **zip_fs (fopen)** + check_data (stat). É o componente de I/O que falta (ref-bully-NX/source/zip_fs.c, usa libminizip). Provável que o jogo abre arquivos via fopen/NvF que precisam ser servidos de dentro dos zips.
Crash anterior (perfprofile glGetString, AND_DestroyEglSurface abort) já resolvidos.

### 🔬 MÉTODO DE DESCOBERTA (como achamos cada coisa — responde "como descobrir isso?")
1. **Espião de fopen/open** (wrapper em imports.c que loga path+OK/FALHA) → revelou EXATAMENTE os arquivos que o jogo pede e quais faltam. Achou: `data_2.zip`/`data_3.zip`/`data_4.zip` (FALHA) + `bully/resource_files.list`.
2. **gdb backtrace + offset (addr−libGame_base) + objdump/nm** → identifica a função que crasha e a instrução.
3. **ler bully-NX** (mesmo lib) p/ saber como cada peça deveria funcionar.

### DESCOBERTAS via fopen-spy (e fixes que avançaram o jogo)
- Jogo faz `fopen("data_2.zip".."data_4.zip")` que NÃO existem (só data_0/1) → registrava ZIPFile NULL → ZIPFile::Find(NULL). **FIX: criar zips VAZIOS válidos** (EOCD 22 bytes) `gamefiles/data_2.zip/3/4.zip` → fopen OK → ZIPFile vazia válida, sem NULL. **PASSOU do data-layer.** (+ symlink data_0/1.zip no cwd; OS_ZipAdd data_0/1 no driver).
- `bully/resource_files.list` (FALHA) = probe opcional (não existe nos zips; ok). Dados reais estão sob `bullyorig/` nos data zips (config/audio/dat/...). Path do jogo "bully/"→"bullyorig/" (asset_archive mapeia p/ NvAPK; fopen direto precisaria do zip_fs).

### CRASH ATUAL (avançou pra inicialização gráfica!)
Thread GameMain, **`GameRenderer::Setup()` +0x47d** (libGame+0x8a0c0d): `cmpl $0,0x6c(%r14)` com r14=objeto de render NULL. Saímos do data-layer e entramos no **setup do renderer** (GfxDevice/render device não inicializado). Trajetória: load→gate Rockstar→GameMain→data zips→**GameRenderer::Setup**.
### RASTREADO: GameRenderer::Setup crasha em ResourceManager::Get<Texture2D>("whitetexture")=NULL
- O objeto NULL (r14+0x6c) = `r14 = ResourceManager::Get<Texture2D>("whitetexture")` (libGame+0x8a09ea) retornou NULL.
- **"whitetexture" = textura INTERNA da engine** (1x1 branca p/ render sem-textura), NÃO asset. As 3 refs à string (GameRenderer::Setup, Material::CreateNew, GameSprite::Draw) são todas `Get` — **nenhuma cria** → a whitetexture deveria ser criada por um **init de recursos default** que NÃO rodou.
- Causa provável: (a) **contexto GL não current na thread GameMain** (criar Texture2D precisa de glGenTextures/glTexImage), e/ou (b) **orquestração GameMain do bully-NX não portada** (forçar gamestate 0→2, tick flags 0x126bb70/74, g_gamemain_alive) — o init gráfico que cria os defaults não dispara.

### 🎮 FASE GRÁFICA (a maior peça restante)
Próximo = fazer o renderer subir:
1. **GL context handoff correto pras threads do jogo** (GameMain/render thread chamam makeCurrent; garantir que a textura-creation tenha GL current). No PC o ctx é EGL pbuffer.
2. **Portar a orquestração GameMain do bully-NX** (frame loop ~linha 1349: gamestate forcing via 0x12146a8 deref+0x68, tick flags, g_gamemain_alive, async file worker) — provável que destrave o init de recursos default (whitetexture).
3. Depois: render visível (window surface) + assets reais (zip_fs servindo bullyorig/ via fopen, p/ texturas/modelos).

Setup atual em gamefiles/: data_0/1.zip(symlink) + data_2/3/4.zip(vazios) + assets/(flutter+data) + libs.
Trajetória: load→gate Rockstar→GameMain→data zips→**GameRenderer::Setup (whitetexture/graphics init)**.

**Trajetória completa:** load→0 unresolved→init_array→JNI_OnLoad→implOnInitialSetup→ActivityCreated→EGL→Surface→Resume→frame loop(0..31)→**gate Rockstar PASS**→GameMain sobe→asset_archive indexa 1912→**ZIPFile::Find(NULL)**. Cada sessão avança várias camadas.

## SESSÃO 2026-06-08 (scaffold limpo experiments/bully/) — narrowed whitetexture
Base limpa que BOOTA reproduz o crash em `GameRenderer::Setup` libGame+**0x8a0c0d** (whitetexture=NULL), Thread worker, após o gate Rockstar (frame 31).
**Portado (jni_shim.c):** orquestração de thread Switch-safe — `my_OS_ThreadLaunch` (handle calloc 0x400: byte 0x69=running, qword 0x28=pthread_t), `my_OS_ThreadWait` (join), `my_NVThreadSpawnJNIThread` (bypass), `g_gamemain_alive`, `hook_threads()` chamado no jni_load. **Resultado:** GameMain/Sound launcham limpo via nossa gerência (antes era pthread default).
**+ re-seed EGL globals pós-OnSurfaceChanged** (OS_EGLDisplay/Surface/Context = srp-0x2d0/-0x2c8/-0x2c0; igual bully-NX sync post-surface-changed).
**DESCARTADO como causa da whitetexture:**
- **GL context**: instrumentei OS_ThreadMakeCurrent → eglMakeCurrent **ok=1** na thread do GameMain. A thread TEM contexto GL. Não é isso.
- **gamestate forcing**: o bully-NX força gamestate 0→2 só em **frame>240** (jni_patch.c ~1380); o crash é frame ~31, MUITO antes. Não é isso.
**Refs à string "whitetexture" (x86_64, .rodata 0x5caaf6):** 0x88a852, 0x8a09d1 (GameRenderer::Setup, logo antes do crash), 0x97ef2e, 0xedb0fb (GameSprite::Draw). **TODAS são GET — nenhuma CRIA.** A whitetexture é criada de forma opaca (manifest/hash de default resources), NÃO por string literal.
**PRÓXIMO (sessão dedicada de RE):** achar O QUE popula o ResourceManager com os recursos default (whitetexture). Candidatos: (a) um init de "default resources" que não roda na nossa ordem/threading (race GameRenderer::Setup vs Initialize); (b) portar MAIS hooks do bully-NX patch_game que faltam (OrigInitialize trace, BullySettings_Load/ResetDisplay, LoadingScreen) — bully-NX RODA no Switch, então tem hook(s) que destravam isso. Método: rodar bully-NX-style com TODOS os hooks portados e ver se a whitetexture aparece; OU rastrear ResourceManager::Add / o populate dos defaults no disasm.

### Tentativas que NÃO moveram o crash (ainda 0x8a0c0d whitetexture):
- re-seed EGL globals pós-surface (mantido, correto).
- hook OS_ScreenGetWidth/Height=1280/720 + OS_CanGameRender=1 + OS_IsGameSuspended=0 (mantido).
- hook __cxa_guard_acquire/release/abort (versão simples Itanium) (mantido).
Nenhum destravou -> a whitetexture NÃO depende de: GL ctx, gamestate, screen dims, render gates, cxa_guard.
### HIPÓTESE RESTANTE + PRÓXIMO ATAQUE:
**RACE de ordem entre threads.** Thread 54 (render) roda GameRenderer::Setup ANTES da thread
GameMain criar os recursos default (whitetexture). No jogo real há sync. PRÓXIMO:
1. Identificar se Thread 54 = render thread vs GameMain (gdb `thread apply all bt` no crash).
2. Achar o CREATE da whitetexture: o Get em 0x8a09d1 hasheia "whitetexture"; achar o
   ResourceManager::Add com o MESMO hash (não usa string literal -> rastrear a hash fn + call site),
   OU o populate de "default resources" no init. Ver QUE thread/quando cria.
3. Alternativa brute-force: portar TODOS os 101 hooks do bully-NX patch_game de uma vez (bully-NX
   RODA no Switch), confirmar que passa, depois bisectar qual hook era essencial.
