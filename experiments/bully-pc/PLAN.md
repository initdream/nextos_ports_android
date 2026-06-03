# bully-pc — Bully: Anniversary Edition (so-loader) — bring-up no PC primeiro

Estratégia (ideia do o autor): **fazer funcionar no PC x86_64 primeiro** (iteração rápida, gdb,
sem freeze do Mali / SSH), depois portar pro device (aarch64 + Mali). NÃO mexe no `core/` nem na
sessão do Hollow — tudo vive aqui em `experiments/bully-pc/`.

## Por que PC-first é possível
O APK v1.4.311 (APKMirror "2arch") traz **`lib/x86_64/libGame.so`** (ELF x86-64, NDK r26d) além do
arm64. Então no PC a gente carrega o `.so` **x86_64 nativo, SEM QEMU** → roda em velocidade real,
gdb normal. O código do jogo é idêntico entre arquiteturas; só o build do framework e o GL final
(Mali) mudam na fase device.

## Recon do libGame.so (já feito)
- **64-bit** (x86_64 no PC / arm64 no device). Entry = **`JNI_OnLoad`** exportado, **sem `android_main`** → Java-driven (precisa emular JNI).
- **415 imports.** Categorias: GL/EGL 89, **OpenAL 31**, NDK (AAsset/ANativeWindow) 12, **libc++/STL 121**, resto libc/m.
- NEEDED: libandroid, liblog, libEGL, **libGLESv3**, libVendor_mpg123, **libopenal**, libm, **libc++_shared**, libdl, libc.
- **GLES:** usa GLES2 + 5 funções GLES3 (`glBindVertexArray`, `glGenVertexArrays`, `glDeleteVertexArrays`, `glTexStorage2D`, `glDrawBuffers`). No PC (Mesa) rodam direto. **No Mali-450** são extensão (`OES_vertex_array_object`, `EXT_texture_storage`, `EXT_draw_buffers`) → validar na fase device (risco baixo; o Vita roda em GLES2-class).
- Áudio = **OpenAL** (não OpenSL ES). PC tem openal; device tem `libopenal.so.1`.

## Contrato JNI (do bully_vita — `ref-bully_vita/jni_patch.c`) — A CHAVE
`jni_load()` dirige tudo:
1. `*IsAndroidPaused = 0` (var exportada; default 1).
2. Monta `fake_vm` (vtable: `GetEnv` @ +0x18) e `fake_env` (vtable JNI).
3. `JNI_OnLoad(fake_vm, NULL)` → durante isso o jogo chama **`RegisterNatives`** (guardamos a tabela em `natives`).
4. **`init = natives[1].fnPtr; init(fake_env, 0, /*init_graphics=*/1)`** ← **entry REAL do jogo** (loop interno).

⚠️ **bully_vita é 32-bit (Vita, lib armhf)** — nós somos **64-bit**. Ajustar:
- `JNINativeMethod` = {name*, sig*, fnPtr*}: no 64-bit a fnPtr do 1º método está em `natives+16` (não `+8`); o `init` (2º método) provavelmente em `natives + 2*24 + 16` — **confirmar lendo a tabela em runtime** (logar nome/sig/ptr de cada método no RegisterNatives).
- Offsets do `fake_env` vtable (0x84 GetMethodID, 0x98 CallBooleanMethodV, 0x35C RegisterNatives...) são de **JNIEnv 32-bit (ptr 4B)**. No 64-bit usar o **layout `JNINativeInterface` de 8 bytes/slot** (índice × 8). Mapear pelos índices da spec JNI, não copiar os offsets crus.

### Métodos JNI a implementar (só ~16, do `name_to_method_ids`)
`InitEGLAndGLES2` (init GL), `swapBuffers` (→ SDL_GL_SwapWindow), `makeCurrent`/`unMakeCurrent`,
`GetGamepadType/Buttons/Axis` (→ SDL_GameController), `GetDeviceInfo/Type/Locale`,
`getAppLocalValue("STORAGE_ROOT")` (→ data path), `FileGetArchiveName` (1→main.obb, 2→patch.obb),
`DeleteFile`, `ShareText`/`ShareImage` (stub).

### Hook conhecido
`hook(_Z24NVThreadGetCurrentJNIEnvv) → retorna fake_env` (thread-local JNIEnv da NVIDIA).

## Componentes a montar (em `src/`)
1. **so_util x86_64**: o loader do core é AArch64-only. Aqui precisa carregar **x86_64** (machine `EM_X86_64`, relocs `R_X86_64_64/GLOB_DAT/JUMP_SLOT/RELATIVE`). Base: copiar `core/so_util.c` + trocar a lógica de reloc (e já incluir o fix ABS64-UNDEF do hollow). Na fase device, volta pro AArch64 do core.
2. **jni_shim** (porta do `ref-bully_vita/jni_patch.c`, 64-bit + SDL2 input em vez de SceCtrl).
3. **egl_shim → SDL2** (reusa a ideia do `core/egl_shim.c`; cria janela 1280x720 + contexto GLES2 no PC/Mesa).
4. **imports table** (415): a maioria passthrough via `dlsym(RTLD_DEFAULT)` (libc/m), OpenAL real (link `-lopenal`), libc++ via **módulo companheiro** `libc++_shared.so` (carregar com o so_loader, igual multi-módulo do hollow) OU link `-lc++` do sistema. GL via `eglGetProcAddress`→SDL. NDK `AAsset*`/`ANativeWindow*` → bridge (ler OBB) / SDL window. Os que sobrarem = stub `ret0` (bully_vita lista vários: renderbuffers, AAssetManager → ret0).
5. **AAssetManager/OBB**: o jogo lê de `main.obb`/`patch.obb`. bully_vita aponta `FileGetArchiveName` pros .obb e usa fopen normal (os OBB são zips/arquivos). Copiar os 2 OBB (do zip que o o autor baixou) pro `gamefiles/`.

## Passos
1. [ ] `so_util` x86_64 + carregar libGame.so, relocate, resolve (logar UNRESOLVED).
2. [ ] companheiros: carregar/linkar libc++_shared, openal, mpg123, z.
3. [ ] `jni_load` 64-bit: JNI_OnLoad → RegisterNatives (logar tabela) → achar `init`.
4. [ ] egl_shim SDL2 + InitEGLAndGLES2/swapBuffers → 1º frame no PC.
5. [ ] input SDL → GetGamepad*.
6. [ ] OBB/assets.
7. [ ] **fase device**: rebuild aarch64 (so_util do core) + validar os 5 GLES3 no Mali + gptokeyb + empacotar port.

## Arquivos
- `gamefiles/` (gitignored): libGame.so x86_64 + companheiros. **OBB a copiar** (main 1.78G + patch 1.16G do zip do o autor).
- `ref-bully_vita/`: referência (jni_patch.c, main.c com a import table, openal_patch.c).
- APK arm64 (device): `split_config.arm64_v8a.apk` do `.apkm` (libGame.so arm64 = 19MB).

## Dificuldade
Médio. Contrato JNI **já resolvido** pelo bully_vita (não é Hollow). Risco real só os 5 GLES3 no Mali (fase device). 64-bit ✅, engine C++ nativa ✅, áudio OpenAL ✅.
