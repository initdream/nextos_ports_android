# DYSMANTLE (10tons NX engine / GameActivity AGDK, Android) → NextOS aarch64 / Mali-450 (Utgard, GLES2)

so-loader do `libNativeGame.so` (arm64) + `libc++_shared.so`. APK: DYSMANTLE 1.4.1.12 (modado "APK_Award").
Device: Amlogic-old (S905X, Mali-450 Utgard, GLES2, fbdev), nextos-87.

## 🎉 MARCO: RENDERER GLES2 100% INICIALIZADO + JOGO CARREGANDO TEXTURAS

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

### 🧱 MURO ATUAL — crash na worker thread de loading (textura grunge corrompida)
`ScreenLoading_LoadingThread` (worker thread async, pthread_create + NXTI_InitializeThread +
nx_thread_state TLS) carrega os recursos. A textura **`ui/gfx/textures/grunge-scratched.jpg`**
lê dados decodificados `0xff 0xd9` (JPEG EOI/vazio) = **asset corrompido/stripado no APK modado**
("JpegImageLoader: Not a JPEG file: starts with 0xff 0xd9"). A engine loga "Loading bitmap failed"
e a thread crasha logo depois (SIGSEGV em memcpy/nString — tratamento da falha OU parse do
próximo recurso). A maioria das texturas carrega (ffffff00), só grunge falha → é específico.

### PRÓXIMOS PASSOS
1. Tornar a falha do bitmap graciosa: patchar BitmapLoader/JpegImageLoader p/ devolver textura
   dummy 1x1 em vez de falhar (a falha cascateia no crash). Achar a função que decide o erro.
2. OU prover dados válidos p/ grunge (asset corrompido) — mas é pós-decompressão (zlib estático),
   difícil interceptar.
3. Investigar TLS da worker thread (nx_thread_state via __emutls_get_address/libgcc).

## Build / Deploy / Teste
`cd ports/dysmantle && ./build.sh` → `scp dysmantle nextos-87:/storage/roms/dysmantle/`.
Dados: `~/dysmantle-build/stage/` → rsync p/ device (libNativeGame.so + libc++_shared.so + assets/ 733MB).
`gamedata/` precisa existir (mkdir) p/ o log. Rodar: `bash diag5.sh` (detached) → run.log + gamedata/DYSMANTLE.log.
BRK-trap tracer (install_brk_traps, comentado) p/ rastrear funções locais. Crash handler faz stack-scan
(g_load_base, range 0x463000-0xd8e000). DYSMANTLE_ASSETS=assets, SDL_VIDEODRIVER=mali.
