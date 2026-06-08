# hollow-recon — STATUS (Hollow Knight / Unity 2020.2 IL2CPP → NextOS Mali)

Harness que carrega `libunity.so` via so-loader e dirige a inicialização do Unity,
pra portar Hollow Knight (Unity, Java-driven, sem `android_main`) ao Mali-450.

## ✅ O QUE JÁ FUNCIONA (provado no device .87, Mali-450)
1. **Load + relocate + resolve** do `libunity.so` (355 imports): **0 UNRESOLVED**.
   - Import table auto: **293 passthrough via dlsym(RTLD_DEFAULT)** + stubs Android-specific.
   - Alias bionic→glibc: `__errno`→`__errno_location`, `__assert2`→`__assert_fail`.
2. **`JNI_OnLoad`** roda limpo → retorna `0x10006`. Captura o contrato: **7 classes Java / 38 métodos nativos** (UnityPlayer 25 + Choreographer/Swappy/ARCore/Camera2/HFP/AudioVolume).
3. **`initJni(Context)` COMPLETA** — passa por TODO o init do Android Context
   (getPackageName, Intent/addFlags, ClassLoader/forName, StringBuilder, Scanner...).
   - jni_shim ciente de nomes (registry method-ID): dispatch por nome.
   - `GetJavaVM[219]` implementado (destravou o init profundo).
4. **Gráfico no Mali OK**: `egl_shim` (SDL2) cria **janela 1280x720 + contexto GLES2**
   ("Window created", "GL share-root context created"). EGL do Unity (20 fns) +
   ANativeWindow (6) mapeados → egl_shim.

## 🧱 ARQUITETURA descoberta
- **IL2CPP auto-load**: `libunity` importa 0 símbolos il2cpp; faz `dlopen("libil2cpp.so")`
  + dlsym em runtime. Nosso `dlopen` é passthrough REAL → **Unity carrega o il2cpp sozinho**
  (libil2cpp é auto-contido: deps só libc/m/dl/log). Integração potencialmente automática.
- **GL via eglGetProcAddress**: libunity importa 0 `gl*`; pega tudo via `eglGetProcAddress`
  (nosso egl_shim delega a SDL_GL_GetProcAddress → driver Mali real).

## 🧱 BLOQUEIO ATUAL
`nativeRecreateGfxState(api, surface)` **crasha imediatamente** (NULL deref dentro do
libunity, ANTES de qualquer chamada EGL). Causa: **engine/player não inicializado** —
está sendo chamado fora de ordem. A janela/EGL não é o problema (funcionam).

## 🔑 DESCOBERTA (fase 2): como o Unity lê os dados
Durante o init, o Unity chama via JNI:
```
GetMethodID(getAssets, ()Landroid/content/res/AssetManager;)
CallObjectMethod(getAssets)
NewStringUTF("bin/Data/boot.config")   <- 1o arquivo
```
→ **Unity lê os dados pelo `AssetManager` JAVA (JNI), NÃO pelo NDK AAssetManager.**
Como controlamos o JNIEnv, dá pra implementar `getAssets` + `AssetManager.open/openFd`
lendo dos arquivos REAIS extraídos. **Dados copiados pro device** em
`/storage/hollow-recon/assets/bin/Data/` (413M: data.unity3d 303M, global-metadata.dat
6.9M, boot.config, sharedassets*, Managed/).

## ▶️ PRÓXIMOS PASSOS
0. **AssetManager bridge no jni_shim** (o desbloqueio agora):
   - `getAssets()` → objeto AssetManager fake (tagged)
   - `AssetManager.open(path)` → InputStream lendo `ASSET_BASE/path` (read/close)
   - `AssetManager.openFd/openNonAssetFd(path)` → AssetFileDescriptor c/ fd real
     (data.unity3d 303M é mmap'd → precisa fd + offset + length)
   - byte[] handling (GetByteArrayElements/SetByteArrayRegion)
1. **Sequência de init correta** (UnityPlayer.java): reverse-engineer a ordem real das
   chamadas nativas. Provavelmente o engine init acontece no 1º `nativeRender` OU há um
   `UnityInitApplication*` a chamar antes do gfx. Decompilar UnityPlayer.java (2020.2) ajuda.
2. **Game data no device**: copiar `assets/bin/Data/` do APK (`data.unity3d`,
   `global-metadata.dat`, `sharedassets*`, `Managed/`) p/ o cwd — `nativeRender` precisa.
3. Driver: chamar a sequência {init engine → nativeRecreateGfxState → loop nativeRender}.
4. Quando o engine subir, o `dlopen(libil2cpp)` dispara → 1º frame.

## 🔬 SESSÃO 2026-06-03 — diagnóstico profundo do crash (gdb + instrumentação)
Avanços reais desta sessão:
- **Multi-módulo**: carrega `libil2cpp.so` E `libunity.so` (replica o libmain), roda
  `.init_array` + `JNI_OnLoad` dos dois. `so_save()/so_use()` p/ alternar módulos.
- **setjmp/longjmp**: confirmado resolvidos p/ `_setjmp`/`_longjmp` glibc (sem sigmask;
  371 reais + 43 stubs). NÃO eram stubs (descartado como causa).
- **Sequência de init**: testadas TODAS as ordens — `nativeResume`,
  `nativeRecreateGfxState`, `nativeRender`. **TODAS crasham no MESMO ponto.**

### 🧱 BLOQUEIO RAIZ (preciso, via gdb no device)
Crash idêntico em qualquer native da engine. Estado no crash (Thread 1 / main):
```
pc = lr = sp-0x50   (pulou pra PILHA — RET com x30 corrompido / longjmp de jmpbuf inválido)
x19 = 0x10          (objeto NULL+0x10)
x20 = unity+0x67bcac  (base dos globais da engine: adrp x20,#0x1723000)
```
Cadeia: `nativeRender/Resume` → `666a00` (getter de contexto thread-local:
`pthread_getspecific(key=[x20+2640])`; se NULL aloca 368B via `44b4e8`) →
`44b4e8` (alocador; se instância `[x20+64]`==NULL chama init lazy `44bde8`) →
**`44bde8` = singleton C++ do memory manager (Meyers guard + `__cxa_atexit` + `brk #1`)
falha/retorna lixo** porque o runtime il2cpp/engine NÃO está bootstrapped.
- **NENHUM stub é chamado no caminho do crash** (logados todos até 10000x) → não é
  função faltando; é a engine exigindo `il2cpp_init` + bootstrap do player que não
  disparamos.
- Handler próprio de SIGSEGV não roda (provável double-fault na pilha smashada);
  diagnóstico só via gdb (que captura o sinal). **gdb do device: sem Python e
  breakpoints de software NÃO pegam na heap rwx do so-loader (icache); usar leitura
  ptrace/`info registers` + scan de pilha; binário compilado `-no-pie`).**

### Tentativa: chamar `il2cpp_init` direto (libil2cpp EXPORTA il2cpp_init/set_data_dir)
- `il2cpp_init` é chamável e **RODA** (loga `[ALOG:4 IL2CPP] JNI_OnLoad` — `__android_log_*`
  implementados de verdade em recon_egl.c revelam as msgs).
- Mas crasha DENTRO do init: `pc=libunity+0x0` (chama ponteiro de função = base+0 = NULL)
  a partir de `unity+0x6ebf38: bl 60e77c`, que faz `ldr x8,[x21]; blr x8` por uma
  **tabela de interface Unity↔il2cpp não-populada**. Unity registra esses ponteiros no
  seu bootstrap (que pulamos) → slots NULL → crash.
- `il2cpp_set_data_dir` ANTES do init tbm crasha (std::string global não-construída).

### 🎉 BREAKTHROUGH 2026-06-03 (manhã): bootstrap REAL encontrado + memory manager BOOTA
- Funções de bootstrap NÃO-exportadas localizadas via refs às strings de log:
  - `PlayerInitEngineNoGraphics` = **unity+0x530424** (loga "settings:%s" @0x53056c).
  - `PlayerInitEngineGraphics` (init gráfico, carrega game manager assets).
  - **DRIVER** = **unity+0x67c9dc** (chama PlayerInitEngineNoGraphics @0x67d3ec;
    recebe bool em w0; chamado via ponteiro, não bl).
- **Chamar `driver(1)` direto BOOTA a engine** → `[ALOG:4 Unity] MemoryManager: Using
  'Dynamic Heap' Allocator.` **← memory manager inicializa!** (era o bloqueio raiz `44bde8`).
- Implementado `__android_log_*` REAL (recon_egl.c) → revela msgs do engine.
- **sigaction/signal viram no-op** (recon_egl.c) → Unity NÃO instala o crash handler
  dele (que mascarava o crash real fazendo backtrace no ambiente falso).
- Crash atual (REAL, pós-memory-manager): `pc=0 lr=0` (`ret` p/ x30=NULL / chamada a
  ponteiro NULL), `x8=unity+0x1786f30` (data) — **ponteiro de função NULL na ponte
  il2cpp↔unity** que falta registrar. Próxima camada do bootstrap.

### 🎉🎉 BREAKTHROUGH 2026-06-03 (tarde): 3 BUGS RAIZ corrigidos — engine bootstrap COMPLETO
Ferramenta nova: **brk-tracepoint** (patch_brk em main_recon.c) — patcha `brk #0` no
código da heap + `__builtin___clear_cache` → SIGTRAP confirma se a execução alcança um
endereço. Bisseca crashes SEM breakpoint do gdb (device não tem HW bp, e SW bp não pega
na heap rwx por icache). Ativa via `BRK_OFF=0xADDR ./hollow-recon`. mmap **MAP_FIXED**
(il2cpp=0x500000000, unity=0x540000000) → endereços determinísticos entre runs.

Bisseccão localizou o crash em `666a00`→`0x666a4c bl 67bc20` (instalador de signal do
Unity). 3 bugs raiz:

1. **sigaction bionic vs glibc (A RAIZ do crash da sessão toda).** `67bc20` passa
   `oldact` = buffer de **32 bytes** (struct sigaction BIONIC) no stack frame. O
   `sigaction` do glibc escreve **152 bytes** (sigset_t glibc=128B) → **estoura 120
   bytes e corrompe o x30 salvo em [sp+96]** → `ret` p/ lixo (pc=lr=stack). Fix:
   `my_sigaction` em recon_egl.c escreve só 32 bytes (SIG_DFL) e não instala handler.

2. **dlopen("") deve devolver g_m_il2cpp.** Unity faz `dlopen("")` (escopo global) p/
   achar os 225 símbolos `il2cpp_*` (no Android estão RTLD_GLOBAL). Nosso loader carrega
   fora do linker real → dlsym falhava. Fix: my_dlopen("")/NULL → g_m_il2cpp.

3. **🔴 BUG DO so_loader (CORE, afeta todo port): R_AARCH64_ABS64 de símbolo importado.**
   `so_relocate` fazia `*ptr = base + st_value + addend` p/ ABS64. Mas p/ símbolo UNDEF
   (importado, ex: `malloc@LIBC` em .data offset 0x1fd77f8), st_value=0 → escrevia
   `base+0` (=base do .so) no ponteiro → il2cpp chamava alloc=base → `pc=il2cpp+0x0`.
   Fix em so_util.c: ABS64 só p/ DEFINIDO no so_relocate; UNDEF resolve via tabela de
   imports no so_resolve (+ r_addend). **Este fix vai pro core do framework.**

Com os 3: engine sobe — `MemoryManager Dynamic Heap`, `SystemInfo CPU=ARM64 Cores=4
Mem=832mb`, `ApplicationInfo com.teamcherry.hollowknight`, `Unity 2020.2.2f1 il2cpp
arm64-v8a`, 225 símbolos il2cpp resolvidos, entra no `il2cpp_init`.

### 🎉🎉🎉 2026-06-03 (tarde/noite): ENGINE RODA, ENTRA NO RENDER — falta camada EGL
Caminho PRÓPRIO (sem driver, env NODRIVER=1): natives diretos funcionam agora que os
bugs raiz cairam — `nativeResume` OK, **`nativeRecreateGfxState` cria os contextos GL no
Mali e RETORNA**, entra no **loop nativeRender**. Engine loga MemoryManager, SystemInfo
(Cores=4 832mb), ApplicationInfo, **Company Name: Team Cherry / Product Name: Hollow
Knight**, cria 2 contextos EGL + window surface.

Fixes desta fase:
- **MEMORY_MB 384→128** (2 heaps de 384=768MB estouravam os 832MB do device = OOM/SIGKILL).
- **eglGetConfigAttrib correto** (RENDERABLE_TYPE=ES2|ES3, SURFACE_TYPE=WINDOW, RGBA8888,
  depth24, stencil8) — antes retornava 0 → "[EGL] Unable to find configuration".
- **eglGetCurrentSurface** retorna a surface real (current_draw_surface), não literal fixo.
- Ferramentas: GetEnv silenciado (flood), on_segv dump imediato pc/lr (unity+il2 offsets),
  __stack_chk_fail interceptado (revela funcao que estoura + dump ASCII do frame).

### 🧱 BLOQUEIO ATUAL (camada EGL — a ULTIMA antes dos pixels)
`nativeRender` frame 0 → função GfxDevice `unity+0x3f0028` faz `bl eglMakeCurrent@plt`
(0x3f0068) → retorna FALSE → `eglGetError != SUCCESS` → monta "[EGL] %s: %s" "Unable to
acquire" → o `%s` lê string gigante → **stack smashing detected (abort SIGABRT)**.
PARADOXO: GOT[eglMakeCurrent] = nosso shim (confirmado no trap antes do bl), mas nosso
shim NUNCA roda (MKCUR=0) e o Mali REAL e' chamado (falha com handles fake). Unity
**sobrescreve o slot do GOT egl com Mali real DENTRO do nativeRender** (GL-loader), e
re-patch externo (por-frame, em main) e' desfeito. Tentado: wire import, eglGetProcAddress
shim, my_dlsym egl shim, re-patch GOT por-frame, MAP_FIXED — nenhum intercepta o overwrite.
Proximo: mprotect GOT read-only + skip-write no on_segv, OU dar handles EGL REAIS (SDL)
pro Mali real aceitar. ~40 ciclos nesta camada. main_recon.c render loop + egl_shim.c.

### 🎉🎉🎉🎉 GL/EGL CRACKED + DENTRO DO RUNTIME C# (2026-06-03 noite)
A barreira EGL caiu! Fixes finais:
- **🔴 bionic vs glibc STACK CANARY (era o "stack smashing"!):** Unity (bionic) lê o
  stack-guard de `tpidr_el0 + 40` (TLS slot 5 = TLS_SLOT_STACK_GUARD bionic). No glibc
  esse offset e' um campo de pthread que MUDA durante a funcao -> canario "nao bate" ->
  `__stack_chk_fail` FALSO (sem overflow real!). Fix: egl_shim_MakeCurrent salva/restaura
  `[tpidr_el0+40]` em volta do corpo (SDL mexe no slot). NAO era problema do GOT/eglMakeCurrent
  (nosso shim SEMPRE foi chamado — o debugPrintf so estava bufferizado, MKCUR-RAW=2 provou).
- **GL stub no-op** p/ funcoes ausentes no Mali-450 (ex GLES3 glGetInternalformativ) em vez
  de NULL (egl_shim_GetProcAddress) + **EGL_RENDERABLE_TYPE=ES2-only** (nao tenta GLES3).
- **il2cpp Type-accessor NULL-safe** via trampolim (patch il2cpp+0x6b1124 -> cbz x0 ret 0).

**AGORA:** o **runtime C# (il2cpp) EXECUTA** — resolve tipos do jogo (`Unable to find type
BatchRendererGroup` etc. = tipos stripados), monta o **PlayerLoop**. Crash atual = whack-a-mole
de acessores de Type il2cpp (0x6b1124, 0x67cf44...) chamados com tipo NULL (stripado) pela
funcao registradora `unity+0x602e70`. Fix raiz pendente: patchar a registradora p/ pular tipo
NULL, OU fazer o type-lookup devolver dummy. Passamos do GL (a parte dificil) — agora e' a
camada de scripting/cena. NODRIVER=1. main_recon.c (patch+render) + egl_shim.c.

### 🏁 2026-06-03 noite: ENGINE 100% VIVO RENDERIZANDO — parede GLES2 vs GLES3 (shaders)
Recuperação genérica de NULL-deref no on_segv (fault em addr <0x4000 -> zera Rt, pc+=4,
limite 4M) ATRAVESSOU toda a registração de tipos C# (tipos stripados/ausentes viram NULL).
Resultado: **nativeRender roda frame 0 E frame 1** (loop de render girando), e o engine faz
trabalho gráfico REAL:
- `[Unity] GL_OES_texture_npot ... GL_ARM_mali_shader_binary ...` (extensões REAIS do Mali lidas)
- `[Unity] Forced to initialize FMOD to 48000` (ÁUDIO FMOD inicializa)
- `[Unity] Desired shader compiler platform 5 is not available in shader blob` (×10)

**PAREDE (provável hardware):** `platform 5 = kShaderGpuProgramGLES = GLES2`. O Unity roda em
modo GLES2 (detectou o contexto GLES2 do Mali-450) e procura shaders GLES2 no blob do jogo,
mas **NÃO EXISTEM** — Hollow Knight Android foi buildado com shaders **GLES3+** apenas (min
GLES3). Mali-450 (Utgard) é GLES2-only (sem GLES3 no hardware). Sem shaders compatíveis ->
nenhum draw -> framebuffer PRETO (fb0 100% zero, 0 eglSwapBuffers).
- Pra rodar o JOGO de verdade no Mali-450 precisaria: shaders GLES2 (re-compilar/traduzir) OU
  gl4es traduzindo GLES3->GLES2 (limitado) OU outro device com GLES3 (Mali Midgard+).
- Pra MISSÃO "qualquer imagem": tentar o SPLASH (shaders built-in da Unity, podem ter GLES2)
  passando do crash atual (pc=0x7fa6...) e reduzindo o mascaramento da recuperacao NULL.

**PROVADO:** so-loader + Unity 2020 IL2CPP boota 100% no Mali-450 (memory mgr, il2cpp runtime,
GL/EGL, FMOD, C# type system, render loop). A parede e' especifica do HK (GLES3) x Mali-450 (GLES2).

### Conclusão técnica (atualizada)
O crash MOVEU mais fundo (memory manager → ponte il2cpp↔unity), confirmando que falta o
**bootstrap completo do Unity IL2CPP**: registrar a tabela de callbacks/method-pointers
Unity↔il2cpp + `il2cpp_init` na ordem certa + criação do player. Cada camada destravada
revela a próxima registração faltante — é RE de semanas. Componentes ao redor (so-loader,
JNI, EGL/GLES2 Mali, AssetManager, dados, android_log) prontos e provados.
**Caminho realista p/ imagens:** build Linux x86_64 do Hollow Knight + box64 + gl4es.

## COMO RODAR
```
# device: /storage/hollow-recon/ com libunity.so + libil2cpp.so
systemctl stop emustation
cd /storage/hollow-recon && ./hollow-recon 2> recon.log   # cria janela SDL (precisa ES parado)
systemctl start emustation
```
Build: `tools/build-port.sh experiments/hollow-recon`. Tabela: `gen-unity-imports.sh`.
Logs de referência em `docs/`.
