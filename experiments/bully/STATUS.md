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

## SESSÃO 2026-06-08 (autônoma, profunda) — whitetexture caracterizada
**Cadeia do crash:** `BullyGameRenderer::Setup` (0x109f360) → `GameRenderer::Setup` (0x8a0c0d, +0x47d)
→ a whitetexture deveria existir mas é NULL. GameRenderer::Setup chama `Resource::LoadVerified()` 6×
(→ `ResourceManager::Reload(Resource*)`) + cria Material/Effect/SceneView.
**Origem da whitetexture = OPACA:** as 4 refs à string "whitetexture" (0x88a852 XMLParser,
0x8a09d1 GameRenderer::Setup, 0x97ef2e Material::CreateNew, 0xedb0fb GameSprite::Draw) são TODAS
GET/uso — nenhuma CRIA. Não vem de fopen (jogo só fopena data zips+resource_files.list) NEM de
NvAPK (nv_open só pede data zips+resource_files.list=MISS). BullyGameRenderer::Setup é chamado por
VTABLE (sem call direto -> caller não-rastreável por xref).
**DESCARTADO (não é a causa):** GL context (eglMakeCurrent ok=1 na thread), gamestate (forcing é
frame>240, crash é frame 31), screen dims/render-gates (hookados, sem efeito), __cxa_guard (idem),
fopen/zip_fs (whitetexture não usa fopen), NvAPK (não pede), resource_files.list (bully-NX roda sem
ele), RACE (só Thread 54 no libGame; ~119 threads idle em pool -> ninguém criando concorrente).
**CONCLUSÃO:** a etapa que CRIA a whitetexture (provável: programática 1x1 numa init de renderer/
default-resources, OU de um IMG pack via um caminho não-logado) simplesmente NÃO RODA na nossa
ordem/setup. bully-NX (mesmo lib arm64) RODA -> tem hook/sequência que dispara isso.
**PRÓXIMO ATAQUE (brute-force / match do bully-NX):**
1. Comparar driver jni_load (nosso) vs jni_patch.c jni_start (bully-NX) PASSO A PASSO — achar a
   chamada/sequência de init que bully-NX faz e nós não (esp. o que dispara create-defaults).
2. Re-derivar p/ x86_64 os offsets do frame-loop do bully-NX: tick flags (Switch 0x126bb70/74,
   "both=1 p/ Application::Tick rodar") + Application holder (0x12146a8) + gamestate (+0x68).
   No x86_64 são outros offsets -> achar por RE (Application::Tick, CreateApplication).
3. Se nada, portar TODOS os ~101 hooks do bully-NX patch_game de uma vez, confirmar que passa,
   bisectar. Hooks ainda não portados relevantes: OrigInitialize(trace), BullySettings_Load/
   ResetDisplay, AND_MiscInitialize, AND_MovieInitialize, OS_Movie*/Playlist* (provável irrelevantes).
**Já portado e funcionando:** thread orchestration (GameMain launcha), zip_fs (fopencookie+minizip),
EGL re-seed pós-surface, screen/cxa hooks. Crash continua 0x8a0c0d. Commits até 2096513+.

### + async file worker portado (ainda 0x8a0c0d):
Portado o async_file_worker do bully-NX (thread chama AND_FileUpdated(delta) enquanto
AndroidFile::firstAsyncFile != 0; ambos resolvidos por símbolo: _Z14AND_FileUpdated +
_ZN11AndroidFile14firstAsyncFileE). Worker inicia após OnResume. **Crash IDÊNTICO 0x8a0c0d.**
### ESTADO: portei TODOS os mecanismos-chave do bully-NX e o crash NÃO MOVE:
thread orchestration (GameMain launcha) + async file worker + EGL re-seed + gates (srp-rel) +
zip_fs (fopencookie) + screen/render-gate hooks + cxa_guard. Sempre crash em GameRenderer::Setup
0x8a0c0d (whitetexture NULL), frame ~31, logo após o gate Rockstar.
### CONCLUSÃO: muro FUNDAMENTAL — a whitetexture/resource não é criada e nenhum mecanismo
portado destrava. Hipóteses para a próxima frente (precisam de mais ferramenta/RE):
1. **Gate flags em offset ERRADO p/ x86_64**: nossos gates são srp-relativos (srp-0x174/-0x17c/
   -0x2e8) re-derivados; o Rockstar gate passa, mas talvez o flag que habilita o LOAD de recursos
   (bully-NX: implIsInitialized 0x158aad4, OS_CanGameRender 0x158a960 — offsets ARM64) esteja em
   outro lugar no x86_64 -> achar os símbolos/globais reais (nm: procurar isInitialized/gamestate).
2. **Trace do bully-NX rodando** (Switch/emu) p/ ver EXATAMENTE quando/como a whitetexture é
   criada — sem isso é adivinhação. Build do bully-NX seria a referência definitiva.
3. **Deep trace ResourceManager::Reload + a população do registro de recursos no x86_64**
   (a whitetexture é um Resource registrado em algum init que não roda).
4. **Ordem/timing**: o gate Rockstar dispara no frame 31 e o GameMain corre pro Setup; talvez
   precise segurar até a fila async esvaziar / um gamestate específico (re-derivar offsets x86_64
   do app holder 0x12146a8 + gamestate +0x68 + tick flags).

## 🎉🎉🎉 BREAKTHROUGH 2026-06-08 — CRASH DA WHITETEXTURE RESOLVIDO! (causa = DADO FALTANDO)
**CAUSA RAIZ:** `data_2.zip/data_3.zip/data_4.zip` eram **STUBS VAZIOS** (22 bytes). A whitetexture é
um ARQUIVO REAL `bully/whitetexture.tex` que estava DENTRO de data_2/3/4! `ResourceManager::Load
<Texture2D>("whitetexture")` falhava (arquivo ausente) → NULL → GameRenderer::Setup 0x8a0c0d crash.
**FIX:** extrair os data_2/3/4.zip(.idx) REAIS do APK 60FPS Mod (v1.4.311):
  `unzip APK "assets/data_[234].zip*" -d bully-pc/gamefiles/` (1.85GB) + symlinks gamefiles/data_N.zip
  -> assets/data_N.zip. APK tem data_0-4 completos (175M/864M/537M/537M/774M).
**RESULTADO:** `[nvapk] open "bully/whitetexture.tex" -> OK`, **0 SIGSEGV**, engine roda CONTÍNUO
(frame 2880+). O crash que travou o port por sessões MORREU.
**LIÇÃO:** todo o RE de código (thread orchestration, async worker, zip_fs, etc) foi necessário pra
CHEGAR no render, mas o muro final era simplesmente DADOS INCOMPLETOS. Sempre checar completude dos
data files cedo. (analogia: igual reVC precisava dos dados certos).
**PENDENTE (próximo):** `eglMakeCurrent ok=0` na render thread = NVIDIA dri2 fail (EGL DESTE PC, não
do jogo). No device Mali-450 fbdev é outro caminho EGL (igual reVC funcionou). Próximo no PC: resolver
o EGL/contexto multi-thread (NVIDIA surfaceless/pbuffer) OU partir pro device arm64 (extrair libGame.so
arm64 do mesmo APK + os data já temos a receita). Engine LÓGICO já roda — falta a APRESENTAÇÃO GL.

## 🎉🎉🎉🎉 JOGO RODANDO 2026-06-08 — ÁUDIO + GL MULTI-THREAD OK (o autor ouviu o som!)
**FIX multi-thread GL:** hookado `_Z22OS_ThreadUnmakeCurrentv` -> bully_release_current (pareia com
OS_ThreadMakeCurrent). Antes o GameMain segurava o ctx EGL (single-thread) e a render thread falhava
(ok=0). Agora intercala release/make -> **ok=1: 2172, ok=0: 0** (render thread pega GL todo frame).
**ESTADO: O JOGO RODA.** o autor OUVIU o som do jogo no PC. Logs mostram carga de gameplay:
sfx_*.snd (efeitos), mx_ms_runninglow.snd (música), hud_jump/punch.tex (HUD), mission_bg.tex.
Engine: boot->JNI->gates->Rockstar->GameMain->resources(data_0-4)->whitetexture OK->render loop
(frame 2280+) com áudio OpenAL tocando. PC bring-up ESSENCIALMENTE COMPLETO (engine roda o jogo).
**ÚNICA limitação PC:** render é offscreen (pbuffer) — NVIDIA+X11 não faz window surface EGL fácil
(dri2 fail). Por isso não-VISÍVEL no PC, mas RODA (áudio prova). **PRÓXIMO:** (a) render VISÍVEL no
PC (window surface NVIDIA / SDL window) p/ ver, OU (b) PIVÔ DEVICE arm64 (libGame.so arm64 já temos
no APK 60FPS + data_0-4 + receita; Mali fbdev faz render visível, igual reVC). Recomendo (b): o PC
já provou a engine; o device é o alvo. Commit pós-8162e14.

## arm64 EXTRAÍDO/PRONTO pro device (2026-06-08)
gamefiles-arm64/lib/arm64-v8a/: libGame.so + libc++_shared + libopenal + libVendor_mpg123 + libz
(todos ARM aarch64). **libGame.so arm64 BuildID 6139a628aa7a... = MESMA VERSÃO do x86_64** -> mesmos
offsets conceituais, 38 impl* (contrato JNI idêntico). **TODA a lógica do PC reaproveita** (jni_shim,
asset_archive, thread orchestration, async worker, zip_fs, hooks make/unmake/screen/cxa).
### PLANO FASE DEVICE (ports/bully/, igual ports/revc):
1. so_util: trocar so_util_x64 pelo core AArch64 (core/so_util.c) — relocs R_AARCH64_*.
2. egl_shim: trocar pbuffer EGL pelo caminho Mali fbdev (SDL2-mali-fbdev OU EGL fbdev direto, igual
   reVC) -> render VISÍVEL na TV.
3. Cross-build aarch64 (toolchain NextOS Amlogic-old). data_0-4 + libs no /storage/roms/ports/bully.
4. Aplicar receitas Mali-450 GLES2 do reVC: shaders highp->mediump/MAX_LIGHTS, GL_RGBA8->GLES2,
   GL_TEXTURE_MAX_LEVEL->força GL_LINEAR, pthread ABI bionic->glibc (já temos pthread_bridge no reVC).
5. Deploy + teste no device (precisa device ON/SSH). gptokeyb p/ controle.

## 🏆🏆🏆 BULLY RODANDO + RENDERIZANDO NO MALI-450 (2026-06-08) — INÉDITO MUNDIAL
**O JOGO RENDERIZA:** cena de abertura "Welcome to Bullworth" (carro na rua, academia, grama/folhas,
personagem) — mundo 3D completo, texturas e CORES CORRETAS no Mali-450 MP / OpenGL ES 2.0!
**fbgrab ENGANAVA:** convertia o fb pra branco (formato errado). O raw /dev/fb0 estava PRETO+conteúdo
o tempo todo. CAPTURA CERTA = `dd if=/dev/fb0 + PIL frombytes RGBA raw BGRA` (fb 1280x720x4 BGRA).
**Cadeia completa que FUNCIONA no device:** loader AArch64 (so_util multi-modulo) -> libc++ + libGame
(18.5MB) -> JNI estatico -> gates -> OS_ZipAdd (zip loading, fix do pool de trampolins 4-byte) ->
SDL2-mali EGL (1280x720 GLES2) -> Rockstar gate -> whitetexture.tex (data_2/3/4 reais) -> render loop
(eglMakeCurrent ok=1 + unmake handoff) -> fixes GLES2 (highp->mediump, RGBA8->RGBA, MAX_LEVEL/mipmap,
glClear cor) -> MENU/MUNDO na TV. swap via eglSwapBuffers do game + SDL_GL_SwapWindow.
**Screenshot: /home/runner/BULLY-MALI450-PRIMEIRO-RENDER.png**
**FALTA (polish):** controle (jni_shim ja tem SDL gamecontroller; testar/gptokeyb), audio (OpenAL,
funcionava no PC), empacotar ES, gerar gamecontrollerdb. Mas o CORE esta FEITO.

## 🎮🏆 CONTROLE FUNCIONANDO + MENU PRINCIPAL (2026-06-08) — JOGÁVEL
o autor: "controle PERFEITO". O jogo NÃO faz polling (GetGamepadButtons nunca chamado); usa EVENTOS
JNI. FIX: pump_gamepad() no loop empurra implOnGamepadButtonDown/Up/AxesChanged/CountChanged a cada
frame (abre SDL_GameController "USB Gamepad" via gamecontrollerdb do sistema; init SDL_INIT_GAMECONTROLLER
+ jni_init_input que NUNCA eram chamados). Mapa SDL->GamepadButton enum libGame (0=A 1=B 2=X 3=Y
4=START 5=BACK 6=L3 7=R3 8-11=NAV 12-15=DPAD 16=LB 18=RB 17/19=gatilhos). **MENU PRINCIPAL renderiza
PERFEITO** (arte da capa Jimmy, logo, cores) — navegável. Screenshot bully_MENU_PRINCIPAL.png.
BULLY JOGÁVEL no Mali-450 (render+som+controle). Commit 81d496b. Falta: corpos 3D dos personagens
(skinning), empacotar ES.

## 🔎 Camisa/torso do Jimmy (jogável) invisível — INVESTIGAÇÃO PROFUNDA (2026-06-08)
SÓ o personagem JOGÁVEL (NPCs OK). Renderiza: cabeça, pescoço, gola, mão, calça, tênis.
NÃO renderiza: CAMISA/torso, braços (superior), brasão. = "transparente" (vê o fundo).
**CAUSA confirmada:** alpha-test discard no fragment shader do ped:
  `gl_FragColor = getlitcolor(texture2D(diffuse,In_uv1)); if (gl_FragColor.a < 0.7) discard; gl_FragColor *= fadeandcolor;`
A textura `diffuse` do corpo **lê ZERO (preto + alpha 0)** no Mali -> alpha<0.7 -> discard -> some.
(Remover o discard global -> a camisa APARECE PRETA + quebra folhagem; confirma o discard.)
**A textura do corpo é RENDER-TO-TEXTURE** (composição de roupa do Bully, "skins"): 12 glBindFramebuffer,
0 glReadPixels. Os FBOs com COLOR_ATTACHMENT0 (tex 588/589/713/777=personagem) dão status COMPLETE.
**=> o FBO está OK mas o PASS de composição renderiza VAZIO no Mali** (a roupa não é desenhada na
textura). Próximo (sessão focada): debugar o que o pass de RTT desenha (texturas-fonte da roupa +
draws) e por que sai zero no Mali-450.
**DESCARTADO:** bones (clamp 59/array 240), vertex-precision (highp mantido no vertex), formato
textura (RGBA4444/RGB565 ok, 0 erro), compressed (nenhum), GL_LUMINANCE-sample (convertido L->RGBA
incl px=NULL), alpha-blend (glEnable BLEND off não ajudou), threshold (baixei 0.7->0.04, não bastou
=> alpha é ~0), FBO incompleto (attach dá COMPLETE).
**Fixes que FICAM (corretos):** highp->mediump (frag), LUMINANCE->RGBA, RGBA8->RGBA, glTexParameteri
MAX_LEVEL/mipmap, glClear cor, threshold 0.04. Jogo 100% JOGÁVEL exceto a camisa do Jimmy.

## 🏫✅ ESCOLA DESTRAVADA (2026-06-08) — era LIMITE DE TEXTURA DA GPU (Mali Utgard)
Sintoma: entrar na escola (hub principal) wedgava o device DURO (SSH timeout). Investigação:
- NÃO era OOM: mem.log mostrou sempre 200-346MB de RAM LIVRE (swap 6GB usou só 39MB).
- NÃO era áudio: stubar vozes/multidão (speech_/ambs_) não ajudou.
- NÃO era tempestade de RTT: contador g_fbo_binds só 1424 em 6240 frames (cresce devagar).
- Hang ABRUPTO carregando textura de NPC (prgirl_pinky_md_n.tex), frames estáveis e param secos.
CAUSA: o Mali-450 (Utgard) tem limite de memória/MMU de TEXTURA separado da RAM. A escola popula
com MUITOS NPCs (cada um _d/_n/_s 512²) -> estoura o limite de textura da GPU -> driver TRAVA (sem
erro limpo). [O GTA SA roda liso pq gerencia melhor a textura dos pedestres — sacada do o autor.]
FIX (3 reduções de memória de textura, env no starter):
1. BULLY_TEX_LIGHT: pula os mapas de DETALHE _n/_s (nv_open retorna NULL) = -2/3 por NPC.
2. BULLY_TEX_HALF (a) pula MIPMAPS (lvl>0): como forço MIN_FILTER=LINEAR, mips nunca são usados
   = desperdício puro de memória de textura. (b) reduz texturas >=512 pela METADE (512->256 = 1/4;
   nearest, UV normalizado não quebra).
RESULTADO: escola JOGÁVEL, sem wedge ("perfeito velho"). Texturas um pouco mais suaves (refinável).
Também: asset_archive O(log n) (qsort+bsearch) destravou o LOADING da escola (antes: varredura
linear 60418 x milhares de opens). Tudo confirmado NÃO-memória via monitor mem.log no starter.

## 🏆👕 ROUPA DO JIMMY RESOLVIDA (2026-06-08) — JOGO 100%
Sintoma: a roupa do player (torso/camisa/braços) APARECIA e SUMIA (no guarda-roupa, parado ou mexendo).
CAUSA-RAIZ (achada com o o autor — "algo que mexemos no código"): o my_glClear forçava
GL_COLOR_BUFFER_BIT em TODO glClear (fix antigo de tela preta). A composição de roupa (render-to-
texture) faz clear só de PROFUNDIDADE no estado estável -> o nosso force-COLOR APAGAVA a cor (a roupa
já composta) -> some. Pistas decisivas: aparece-e-some (renderiza OK); trocar roupa RÁPIDO mantém
visível (re-compõe sem parar); some PARADO e MEXENDO (descarta skinning/animação); BULLY_DEFER_CLEAR
(pular clear-only) deu TELA PRETA (o force-COLOR também era da FBO da cena).
FIX: forçar COR só FORA de FBO (a tela); DENTRO de FBO respeita a máscara do jogo -> um clear de
profundidade não apaga a roupa. `unsigned m = g_in_fbo ? mask : (mask | 0x4000);`
RESULTADO: Jimmy VESTIDO (colete Bullworth, braços, tudo), fixo. Jogo 100% jogável/lindo/estável.
Diagnóstico que levou lá: trace draws/clears por RTT (g_in_fbo, g_rtt_draws/clears) — mostrou as
composições "draws=0 clears=1" (os clears de profundidade que o force-COLOR transformava em erase).
