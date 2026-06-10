# DYSMANTLE (10tons NX engine / GameActivity AGDK, Android) → NextOS aarch64 / Mali-450 (Utgard, GLES2)

so-loader do `libNativeGame.so` (arm64) + `libc++_shared.so`. APK: DYSMANTLE 1.4.1.12 (modado "APK_Award").
Device: Amlogic-old (S905X, Mali-450 Utgard, GLES2, fbdev), nextos-87.

## 🏆 MARCO 2026-06-10: JOGO ABRIU NA TELA! "Program initialization OK. Running program"

**CAUSA-RAIZ DO MURO FINAL (era TUDO o canary, não Squirrel/texturas!):**
A engine (bionic) lê a stack-guard de **`tpidr_el0+0x28`** (bionic TLS_SLOT_STACK_GUARD)
no prólogo e compara no epílogo. Sob glibc esse endereço caía no TLS de outra lib e
**MUDAVA de valor no meio da função** (ex: errno durante pthread_create) → mismatch →
`bl __stack_chk_fail`. Como o compilador trata __stack_chk_fail como **noreturn**, nosso
no-op RETORNAVA e a execução **caía no código adjacente**: em `NXTI_CreateThread` o
fall-through era a lambda de thread-entry (0x573f24) rodando NO PARENT com x0=lixo →
`NXTI_InitializeThread(lixo)` → memcpy(0, lixo, 11) — o famoso "crash do Squirrel".
("UI AUTOEXEC" no buffer e os frames SQLexer/SQCompiler eram lixo de stack = red herring.
O autoexec.nut compilava OK; a thread "Loader" do ScreenLoading é que matava o parent.)

**FIX = pad TLS no exe** (`main.c g_bionic_guard_pad[256]`, `_Thread_local aligned(16)`,
NUNCA escrito): o 1º bloco TLS após o TCB (16 bytes) é do exe → tpidr+0x28 cai DENTRO do
pad → valor estável (0) em toda thread → canary nunca mais dá mismatch (0 ocorrências de
stack_chk_fail no run). Log de validação na partida: "TLS guard ... DENTRO ✓".

**Diagnóstico que destravou:** crash handler com tabela de módulos (g_mods: game/libc++
base+range, frames `game@vaddr`) + BRK-traps one-shot com tid+args na cadeia
NXT_CreateThread→NXTI_CreateThread→thread_entry→InitializeThread. O frame #2
`lr=0x573f24` (= retorno do `bl __stack_chk_fail` em 0x573f20) entregou o fall-through.

## 🔊 SOM RESOLVIDO (2026-06-10): Oboe REAL via OpenSLES→SDL2
1. Sem patch ret0 no SoundImpOboe::Initialize (env `DYSMANTLE_NO_OBOE=1` reativa fallback Null).
2. jni_shim: `GetDefaultAudioStreamParameters()[I]` → jintArray {44100,1024} + GetIntArrayElements(187)/Release(195).
3. `__system_property_get("ro.build.version.sdk")="25"` → Oboe aceita Float (≥21), sem AAudio (<27).
4. Oboe faz `dlopen("libOpenSLES.so")` runtime → my_dlopen→SL_MAGIC, my_dlsym→slCreateEngine_shim + &SL_IID_*_v.
5. **SL_IID com identidades DO SHIM** (ctor sl_iid_init; ANDROIDSIMPLEBUFFERQUEUE→sl_IID_BUFFERQUEUE; shim compara ponteiro).
6. **PCM_EX float32 (formatType=4) → S16 no bq_Enqueue** (is_float, clamp ×32767).
7. Volume: ganho master do soft-clip era 0.30 (tuning Bully) → **1.0 default**, ajustável `SLSHIM_GAIN=x` sem rebuild.

## 🎮 CONTROLE USB RESOLVIDO (2026-06-10): Paddleboat NATIVO alimentado direto
- **-2002 era `NewObject` ausente** no jni_shim (slot 28-30) → NULL → GCM_FAILURE. Fix: NewObject→fake.
- **Registro**: chamar `Java_..._GameControllerManager_onControllerConnected(env,NULL, jintArray[7], 4×jfloatArray[48])`
  direto do C (pb_try_connect, depois de Paddleboat_isInitialized). deviceInfo={devId 7777, vendor, product,
  axisBitsLow=X|Y|Z|RZ|HAT_X|HAT_Y|LT|RT, 0, 1, 0}. Precisa GetIntArrayRegion(203)+GetFloatArrayRegion(205)
  (⚠️ExceptionCheck é 228 na spec, estava errado em 205).
- **Eventos**: SDL→`Paddleboat_processGameActivityKeyInputEvent` (PbKeyEvent 56B: devId@0,src@4,action@8,keyCode@48)
  e `...MotionInputEvent` (PbMotionEvent 1704B: ptrCount@56, pointers@64, stride 204 = id+axes[48]float; axes:
  0=LX 1=LY 11=RX 14=RY 15/16=HAT 17/18=triggers). src: key=0x401, motion=0x1000010. deviceId TEM que casar.
  Retorno 1 = consumido ✓. Layouts extraídos do binário (_Static_assert nos tamanhos).
- BACK→BUTTON_SELECT(109) (AKEYCODE_BACK=4 é especial no Paddleboat); A/B sem swap.

**PRÓXIMOS PASSOS:** validar resposta in-game do controle (Felipe), lag (comparar c/ DYSMANTLE_NO_OBOE=1),
volume fino (SLSHIM_GAIN), estabilidade gameplay longa, empacotar p/ ES + R2 (desmascarar emustation!).

## (histórico) MARCO 1: RENDERER GLES2 100% INICIALIZADO + JOGO CARREGANDO TEXTURAS

### O QUE FUNCIONA ✅ (evidência: gamedata/DYSMANTLE.log + run.log)
1. **Loader 2-módulos** libc++_shared + libNativeGame, 379 ctors, JNI fake, SDL2.
2. **🔑 GameActivity (AGDK), não NativeActivity** — struct android_app layout próprio
   (window@56, mutex@200, cond@240, msgread@288, pendingWindow@344, motionEventFilter@376)
   + process_cmd chama android_app_pre_exec_cmd/post_exec_cmd do glue. Destravou hang.
3. **CONTEXTO GLES2 NO MALI-450 + "Renderer Initialization done."** ← renderer COMPLETO.
   - Engine: "Using OpenGL renderer", "EGL context version is 1.4", glGetString/glGetIntegerv OK.
4. **Som**: Oboe falha (patch) → **fallback "Null" succeeded** → segue sem áudio.
5. **"Initializing Program 'DYSMANTLE'"** + **CARREGANDO TEXTURAS** (a maioria carrega OK,
   dados reais ffffff00) via fread no pak (data.pak fopen'd, índice parseado certo).

### FIXES-CHAVE (todos em main.c/imports.c)
- **🩹 STACK SMASH = falso-positivo de TLS**: a engine (bionic) lê a stack-canary de
  `tpidr_el0+0x28`; sob glibc colidia com `_Thread_local` do egl_shim (has_real_gl etc.) →
  guard mudava no meio da função. FIX = tirar `_Thread_local` do egl_shim + neutralizar
  `__stack_chk_fail` (override no-op) p/ os demais paths (libc++ TLS).
- **SwappyGL_init** retornava ptr par → tbz interpretava como falha → "Unable to init renderer".
  FIX = patch `SwappyGL_init`→return 1, `isEnabled`→0.
- **SoundImpOboe::Initialize** crasha (Oboe STL/JNI) → patch return 0 → fallback Null.
- **__sF / android_set_abort_message** UNRESOLVED no libc++ (std::cerr) → providos (bionic_sF
  + wrappers fwrite/fputs/fprintf, do bully).
- **setjmp/longjmp** → mapeados p/ `_setjmp`/`_longjmp` glibc (sem sigmask, casa bionic; libjpeg).
- **__register_frame** do .eh_frame do jogo (@0x349900) — módulo custom-loaded, unwinder C++.
- **AAsset_openFileDescriptor** = dup(fileno) (offset compartilhado). fopen/ifstream redirect→assets/.
- **NXI_GetProductValue("opengl_version")** GOT-hook → "2.0" (caminho ES2).
- **nx_run_no_popups=1** + NXD_ShowPopup no-op + ImageWriterJPEG::Initialize=0 (anti-crash falha textura).

### ✅ TEXTURAS RESOLVIDAS (ETC2→JPEG/PNG) + 🧱 MURO = compilador Squirrel (autoexec.nut)

**TEXTURAS (resolvido):** o APK modado "APK_Award" tem os JPEG/PNG de UI com **size=0** (vazios),
só a versão **`.ktx` (ETC2_RGB8, glInternalFormat=0x9274 = GLES3)**. Mali-450 Utgard é GLES2
(sem ETC2) → a engine cai no .jpg/.png vazio → "Not a JPEG: starts with 0xff 0xd9". `NX_Graphics_
IsTextureFormatSupported`→1 NÃO redireciona (a engine escolhe .jpg por GL caps). **FIX = decodificar
os ETC2 KTX → JPEG/PNG no PC e preencher os slots vazios no pak** (`tools/fix_empty_textures.py`,
usa texture2ddecoder+PIL). data.pak (primário): 466 .jpg in-place + 3 .jpg + 70 .png anexados.
grunge passou (carrega JPEG real 1024x1024, ver tools/grunge_decoded_proof.jpg). Formato pak:
magic"PAK\0V11\0"(8)+idx_offset(u32)+filesize(u32); índice = nome\0+offset+size+hash+pad (4×u32).
In-place (sobrescreve slot .ktx, mesmo offset) OU anexa (move índice). Paks corrigidos em stage/assets/.

**🧱 MURO ATUAL = compilador SQUIRREL crasha compilando `autoexec.nut`.** BRK-trap em sq_compile
(0x64454c) revelou: o 1º script `autoexec.nut` (sourcename) crasha no lexer ao criar o token da
string literal **"UI AUTOEXEC"** (exatamente **11 chars = n do memcpy**). Crash = `memcpy(dst=NULL,
src=0x2964, n=11)` na worker thread (`ScreenLoading_LoadingThread`). Stack: SQLexer::ReadNumber /
SQCompiler::ShiftExp / SQFuncState::GetConstant/PushTarget / SQString. dst=NULL + src=ponteiro
**truncado/uninit (~0x2900, varia por run)** = **SQLexer._longstr (sqvector) NÃO inicializado**
e/ou string-table do SQSharedState NULL → **VM/compilador Squirrel mal-construído na worker thread**.
(grunge era red herring — o crash sempre foi o Squirrel, logado logo após o erro da grunge.)

### PRÓXIMOS PASSOS (muro Squirrel)
1. Investigar a criação do VM Squirrel (sq_open/NX wrapper): SQSharedState/string-table/_longstr
   uninit → ver se falta função stub, ou se é construção C++ (operator new/ctor) na worker thread.
2. Conferir se SQCompiler/SQLexer roda construído (ctor) vs malloc cru; checar ABI sqvector.
3. Comparável a [[project_hollow_knight_unity_soloader_mali450]] em profundidade.

## Build / Deploy / Teste
`cd ports/dysmantle && ./build.sh` → `scp dysmantle nextos-87:/storage/roms/dysmantle/`.
Dados: `~/dysmantle-build/stage/` → rsync p/ device (libNativeGame.so + libc++_shared.so + assets/ 733MB).
`gamedata/` precisa existir (mkdir) p/ o log. Rodar: `bash diag5.sh` (detached) → run.log + gamedata/DYSMANTLE.log.
BRK-trap tracer (install_brk_traps, comentado) p/ rastrear funções locais. Crash handler faz stack-scan
(g_load_base, range 0x463000-0xd8e000). DYSMANTLE_ASSETS=assets, SDL_VIDEODRIVER=mali.
