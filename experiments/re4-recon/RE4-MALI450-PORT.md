# RE4 (Resident Evil 4 fan, Unity 2018 Mono) -> Mali-450 armhf GLES2 -- STATUS

## Recon
- APK: ~/Downloads/Red... Resident Evil 4 fan-made em **Unity 2018.1.1f1 Mono** (NAO o nativo Capcom).
- libunity.so 18MB + libmono.so 3.8MB + libmain.so (boot). **v7a + x86 (SEM arm64)**. 208MB.
- **GLES2 path EXISTE** (force-gles20, "OpenGL ES 2.0 is required") -> NAO e GLES3-only (≠ HK).
- **ZERO Vulkan, ZERO IL2CPP**. Mono (Unity antigo, pre-Swappy/Choreographer).
- Assets: assets/bin/Data/ (Unity, 473 arquivos).

## Alvo (device Amlogic-old)
- ARM 32-bit armhf, interp /lib/ld-linux-armhf.so.3 (= GTA CTW). Runtime em /usr/lib32 + /lib.
- Toolchain: armv8a-emuelec-linux-gnueabihf-gcc (Amlogic-old).

## FASE 0 -- PASSOU (2026-06-08) ✅
Escrito do ZERO: `src/so_util.c` ARM32 (Elf32, relocs REL .rel.dyn/.rel.plt, R_ARM_RELATIVE/
GLOB_DAT/JUMP_SLOT/ABS32 addend in-place, hook_arm=LDR PC, init_array /4), `src/main_re4.c`
(load->reloc->recon imports, SEM executar), `build_re4.sh`.
RESULTADO no device: re4recon (ELF32 ARM) **carregou + relocou libunity.so** (39k R_ARM_RELATIVE)
e listou **308 imports**. "FASE 0 OK". Loader ARM32 Unity FUNCIONA.

## Contrato de imports (IMPORTS.txt, 308) -- como fornecer
- libc/pthread/__aeabi*/C++ABI (__cxa_*, _Unwind_*) -> passthrough glibc + libgcc.
- bionic-isms: _ctype_, __sF, __errno, _tolower_tab_, __system_property_get -> SHIM.
- __android_log_* -> stderr. __google_potentially_blocking_region_* -> no-op.
- ALooper_forThread/prepare -> Looper (Unity 2018 simples). ANativeWindow_* -> shim (dims da janela).
- egl* (API completa) -> egl_shim SDL2-mali (reusar do Bully/HK). gl* -> via eglGetProcAddress.

## FASE 1 (proximo) -- imports table -> init_array -> JNI_OnLoad -> boot Unity
1. Gerar dynlib_functions (tools/new-port.sh OU adaptar imports do HK) p/ os 308.
2. egl_shim + jni_shim (reusar HK, adaptar p/ ARM32 + Unity 2018 + Mono + libmain boot).
3. so_execute_init_array -> JNI_OnLoad (libmain->libunity) -> UnityPlayer init -> nativeRender.
4. Assets: extrair APK p/ o device (assets/bin/Data) + apontar o caminho.

## HIPOTESE-CHAVE (porque pode destravar Unity de vez)
Unity 2018 + GLES2 + Mono + pre-Swappy = as DUAS paredes do HK (GLES3 + Choreographer) podem
NAO existir aqui. Se a engine montar a cena, e o template Unity-armhf-GLES2 reusavel.

## Arquivos
- PC: ~/nextos_ports_android/experiments/re4-recon/ (src/, build_re4.sh, lib/, IMPORTS.txt)
- Device: /storage/re4-recon/ (re4recon + libunity.so)
- APK: ~/Downloads/Red_Dead_Redemption_v1.58... (nome enganoso; e RE4) | libs em ~/re4-build/

## FASE 1 -- BOOT (2026-06-09): engine INICIALIZA + janela GLES2 + lifecycle ate gfx-init 🏆
Sequencia que FUNCIONA no device (ports/re4/re4boot, ARM32 nativo):
- load + relocate (so_util ARM32) ✅
- imports (355: dlsym RTLD_DEFAULT + shims bionic _ctype_/__sF/_tolower_tab_) ✅
- 🔑🔑 PTHREAD BIONIC BRIDGE (pthread_shim recon_wire_pthread): SEM ele, init_array TRAVA em
  DEADLOCK FUTEX (ctor 76/372, mutex estatico bionic mal-interpretado pelo glibc). COM ele os
  372 ctors rodam. **ESSA E A CHAVE pra destravar Unity no so-loader (vale p/ qualquer Unity).**
- init_array: 372 ctors C++ ✅
- JNI_OnLoad -> 0x10006 (JNI 1.6); RegisterNatives UnityPlayer(18)+ReflectionHelper+ARCore ✅
- janela SDL2/GLES2 criada (egl_shim core, provado Syberia/lswtcs/Bully) ✅
- initJni(Context) ✅
- nativeRecreateGfxState -> CRASHA aqui

PAREDE ATUAL: SIGSEGV em glibc **pthread_key_delete+0x138** (lr TAMBEM em libc = chamada
glibc-INTERNA, nao via GOT). r1 = valor-lixo que VARIA por run = TLS/thread-list do glibc
corrompida pela engine bionic. Override do import NAO pega (interna). Wrapper TLS bionic-safe
feito (sh_key_create/delete) mas a chamada e interna -> nao resolve.
=> proximo: bridge de TLS/thread bionic mais profundo (pthread_self/__get_tls/thread-list proprios)
   OU isolar a engine do modelo de thread do glibc.

### Estado (tudo no device + repo)
- ports/re4/ (src completo: so_util ARM32, imports.gen.c, main_re4.c, shims). Build: gen_imports.py +
  armv8a-emuelec-linux-gnueabihf-gcc. Diagnostico: segv handler + maps/regs/stack scan.
- Device /storage/roms/re4-recon/: re4boot + libunity.so + assets/ (351MB) + run.sh.
- Marco: PRIMEIRA Unity (2018 Mono) bootando engine+JNI+janela GLES2 no Mali-450 ARM32.

## FASE 1 -- parede do gfx-init (2026-06-09, sessao 2)
Apos a engine bootar (init_array+JNI+janela GLES2+initJni), trava em nativeRecreateGfxState:
- crash = gettid+tgkill(SIGSEGV) em glibc (offset 0x7c61c) = a engine/glibc faz **raise(SIGSEGV)
  DELIBERADO** (caminho de erro FATAL da Unity), NAO um segfault real.
- A RAZAO do erro esta ENGOLIDA: __android_log (implementado -> stderr) NAO mostra nada antes;
  a Unity provavelmente loga via __sF (stdio bionic, layout != glibc -> incompativel).
- raise/abort/pthread_kill sao chamados GLIBC-INTERNO (lr no libc) -> override por GOT NAO pega.

TENTADO (nao resolveu o abort): (1) TLS bridge auto-contido (sh_key_create/delete/get/setspecific
em slots proprias -- CONFIRMADO funcionando p/ as keys do engine, mas o crash e interno);
(2) bloquear sigaction da Unity p/ SIGSEGV/ABRT (MEU handler pega, mas crash = no proprio tgkill);
(3) interceptar abort/raise/pthread_kill (nao pega -- glibc interno); (4) boot.config single-thread
(gfx-disable-mt-rendering=1, gfx-enable-*-jobs=0) -- abort persiste.

RAIZ: paredes FUNDAS de ABI bionic!=glibc no gfx-init (pthread/TLS interno, stdio/__sF, sinais).
Proximo nivel = ambiente fake-bionic mais completo (TCB/pthread_self/__sF bionic) OU backtrace/
instrumentacao melhor p/ achar a razao FATAL engolida. Frente grande e nova.

### Diagnostico/infra que ja temos (no main_re4.c)
segv handler (fault/pc/lr + maps + regs + stack-scan unity), __android_log real, sigaction-block,
abort/raise/pthread_kill intercept, TLS bridge, init_array index+addr logging, RE4_SKIP.

## FASE 1 -- Unity+Mono RODANDO, agora no JIT do Mono (2026-06-09, sessao 3)
SEQUENCIA DE BREAKTHROUGHS (cada um destravou o proximo):
1. sigaction bionic->glibc traduzido (handler @ offset 0) -> passou nativeRecreateGfxState; a engine
   loga SystemInfo+versao 2018.1.1f1 mono; o crash-handler DA UNITY funciona (via __android_log bridge).
2. dlopen("")/libc/libunity/libmono -> SELF handle; dlsym -> so_find_addr+RTLD_DEFAULT.
3. dlerror/dladdr/dlclose stubados -> evita _dl_exception_create NULL-deref (glibc dl-error path).
4. LIBMONO carregado como 2o modulo (so_load+reloc+resolve+init_array, g_m_mono); dlsym de mono_*
   resolve nele -> "Could not load symbol mono_*" = 0.
5. so_resolve cai pro re4_resolve (dlsym+aliases, bsd_signal->signal) p/ os imports do libmono ->
   UNRESOLVED=0.
ESTADO: a engine roda Unity+Mono, lifecycle ate nativeRender, e o **JIT do Mono compila C#** ->
crasha com Assertion em mini.c:2215 + "stack walk not installed" (pc em libmono = o JIT do Mono).

PAREDE ATUAL: o JIT do Mono (mini-arm) -- precisa de setup de unwind/stack-walk OU exec-mem do JIT.
Frente nova e funda (interna do runtime C#). Muito alem do HK (que nem passou do init da engine).

### Infra (main_re4.c) p/ continuar
multi-modulo (g_m_unity+g_m_mono), bridges: pthread+TLS+stdio+dl+sigaction(bionic->glibc), __android_log
real, segv handler gdb-friendly, getpwuid stub. Device: /storage/roms/re4-recon (re4boot+libunity+libmono+assets).

## FASE 1b -- furamos ATE o GC do Mono (2026-06-09, sessao 3 cont.)
SEQUENCIA (cada fix destravou o proximo, TUDO confirmado rodando):
6. mmap absurdo: o Mono pede mmap de 3.8GB (0xE3A00000) e 973MB(RWX) -> falham -> buffer NULL
   -> crash no CODE-GEN do JIT (str instrucao ARM em [r2=NULL] @ mono_arch +0x11e4).
7. CAUSA: caller = mono_valloc (alocador VM do Mono). O 3.8GB = 0xE3A00000 = 4GB-454MB =
   o GC do Mono reservando TODO o espaco de endereco restante (normal em 64-bit; IMPOSSIVEL
   em processo 32-bit que ja tem so-loader+libunity+libmono+SDL+Mali).
8. cap 256MB (tudo) -> code-gen PASSA (CRASH=0!) mas o GC usa o tamanho PEDIDO (3.8GB) ->
   acessa alem dos 256MB -> core dump depois. Cap quebra o GC.
9. Tentado SEM sucesso p/ o reserve do GC: cap (quebra GC), MAP_NORESERVE (espaco 32-bit nao
   cabe 3.8GB mesmo sem reservar), GC env vars (GC_MAXIMUM_HEAP_SIZE/MONO_GC_PARAMS, sem efeito),
   sysconf override (ig_sysconf NAO e chamado -> a memoria nao vem de sysconf), footprint reduzido.

ESTADO: Unity+Mono RODAM, JIT inicializa, code-gen FUNCIONA (com cap), e o muro e o MODELO DE
MEMORIA DO GC DO MONO em 32-bit (reserva ~3.6GB do espaco de endereco). Provavelmente arquitetural.

PROXIMO PASSO PRECISO (frente nova focada): achar a fonte do underflow (memoria=0 -> 0-X=negativo
-> 3.8GB) -- hookar o CALLER de mono_valloc + desmontar a logica de reserva do GC; OU descobrir a
query de memoria que da 0 (NAO e sysconf); OU patchar a reserva do GC no libmono p/ pedir um
tamanho fixo sano (ex: 256MB) E usar esse tamanho consistentemente. Alternativa: investigar se o
libmono tem build sgen com max-heap configuravel por env que REALMENTE limite a reserva.

## FASE 1c -- Boehm GC e o handler turbinado (2026-06-09 madrugada, sessao 4)
DESCOBERTAS:
- O "crash mini.c:2215 / stack walk not installed" era o **crash-REPORTER** do Mono falhando,
  NAO o crash real. O crash real e um SIGSEGV/ABORT no init do GC.
- libmono usa **GC BOEHM** (GC_push_all_stack, GC_end_blocking, GC_unix_get_mem).
- **RE4_NOSIGH=1** (my_sigaction bloqueia install de SIGSEGV/ABRT/ILL/TRAP do Mono) +
  abort/raise/pthread_kill non-fatal + on_segv turbinado (pega sig 4/5/6/7/8/11 + mapeia
  offsets libmono via g_mono_base) => backtrace REAL do crash.
- Cadeia do crash: mono_jit_thread_attach(0x1a820) -> mono_jit_init(0xbd5d0) -> GC init ->
  GC_push_all_stack/GC_end_blocking(0x2b-0x2c) -> "Bad GET_MEM arg" -> ABORT (tgkill cru SIGABRT).
- FIX aplicado: bionic pthread_attr_t e **32-bit** (stack_base@4, stack_size@8), nao 64-bit
  (8/16) -- corrigido em pthread_shim (getattr_np/getstack/setstacksize/create). [getattr_np
  nao e chamado pelo Boehm, mas o fix e correto p/ ARM32.]

PAREDE ATUAL: GC Boehm "Bad GET_MEM arg" (libmono+~0x2bXXXX) -> tamanho de bloco invalido/
desalinhado no GC_unix_get_mem durante o init do GC. String em .rodata 0x3884c0 (vaddr==offset).
Tentado s/ sucesso: GC_INITIAL/MAXIMUM_HEAP_SIZE, GC_DONT_GC, sysconf/sysinfo/getpagesize override,
/proc/meminfo fake (engine nao le meminfo; le /sys/.../cpufreq). A memoria=0mb do Unity vem de
fonte ainda nao identificada (nem sysconf/sysinfo/JNI/proc).

PROXIMO: achar GC_unix_get_mem (xref a 0x3884c0 -- padrao movw/movt ou GOT, nao ldr+add) e
hookar p/ alinhar/corrigir o size; OU descobrir a fonte do memory=0 que enviesa o GC.
Infra de debug toda no main_re4 (on_segv multi-sinal + hooks). RE4_NOSIGH=1 chega mais longe.

## FASE 2 -- C# RUNTIME CARREGANDO!! (madrugada 2026-06-09, sessao 5) -- INEDITO
CADEIA DE BREAKTHROUGHS (cada um destravou o proximo, TODOS confirmados):
1. **GC Boehm "Bad GET_MEM arg"** (a parede MAIS DURA): GC_unix_get_mem (libmono+0x2bed14) aborta
   se (bytes & (GC_page_size-1)); GC_page_size==0 -> tudo aborta. FIX: hook+trampolim no GET_MEM
   -> bypass (mmap page-aligned direto). **GC PASSADO.**
2. getmem_trace (debug meu) estourava a pilha -> desabilitado.
3. **mscorlib "invalid CIL image"**: Unity (bionic) lia st_size do **fstatat64** no offset bionic,
   mas glibc preenchia layout glibc -> size=32KB -> lia so 32KB do .dll (metadata em 1.27MB fora).
   FIX: **fstatat64/fstat64/stat64/lstat64 via syscall CRU** (kernel preenche layout bionic).
   Unity le os 2.49MB inteiros -> **mscorlib + assemblies C# CARREGAM.** (achado via strace!)
4. assemblies copiados em Managed/mono/{1.0,2.0}/ (FS exFAT sem symlink -> cp).
5. mono_jit_init_version forca "v2.0.50727" (Unity pedia v4.0.30319=unavailable).
6. **threads.c:928** (SP nos bounds da pilha) + **mini-posix:382** (sigaction!=-1): FIX=
   pthread_getattr_np preenche attr BIONIC (base@4 size@8) com bounds reais; sigaction nunca -1.
   **C# RUNTIME INICIALIZA** -> registra metodos nativos (tangoOnImageAvailable etc).

ESTADO: o runtime C# (Mono v2.0/Boehm) CARREGA e INICIALIZA. Passamos GC + carga de assembly +
bounds de thread. Janela GLES2 ja existe; falta o C# do jogo rodar + 1a cena desenhar.
PAREDE ATUAL: chamada via ponteiro NULL (pc=0) fundo no init do runtime (libmono+0x13e018-area,
mono_register_bundled_assemblies / setup de callbacks do GC). Pilha corrompe no crash. vtable JNI
ja toda preenchida (nao e ela). Provavel callback/ponteiro do runtime nao instalado.

Infra de fix toda em main_re4.c (gated) + imports.gen.c (stat syscalls, ig_attr_getstack/getattr_np).
strace no device = /usr/bin/strace (autoritativo, achou o fstat). assemblies em mono/1.0+2.0.
