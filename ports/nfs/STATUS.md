# NFS Most Wanted (2012) → NextOS Mali-450 (so-loader armhf)

**Jogo:** Need for Speed Most Wanted, `com.ea.games.nfs13_row` v1.3.128 (EA/Firemonkeys/Ironmonkey).
**APK:** `~/Downloads/need-for-speed-most-wanted-v1.3.128.apk` (19.5MB).
**OBB/dados:** `~/Downloads/need-for-speed-most-wanted-v1.3.128-cache-apkvision.zip` → `main.1003128.com.ea.games.nfs13_row.obb` (623MB).

## ✅ Análise de viabilidade (PROMISSOR)
- **GLES2** (libGLESv2 + libEGL, zero GLES3) → passa no filtro nº1 do Mali-450 Utgard. 🟢
- **ABI armhf** (armeabi-v7a, ELF32 ARM, NDK r18) → caminho 32-bit do so-loader (igual GTA CTW/VC, runtime /usr/lib32). 🟢
- **Entry = JNI_OnLoad** em libapp.so + Java `com.ea.ironmonkey.GameActivityMain.nativeOnCreate` → padrão GameActivity+JNI (dominado no DYSMANTLE/Bully). 🟢
- **Risco real = PERFORMANCE** (open-world 3D de 2012 num Mali-450 fraco). Só o teste dirá. 🟡

## Módulos (multi-módulo, igual DYSMANTLE)
- `libapp.so` (11MB) = ENGINE principal (JNI_OnLoad aqui)
- `libNimble.so` (13KB) = bridge JNI pequeno (GameActivity)
- `libfmodex.so` + `libfmodevent.so` = áudio FMOD (middleware comercial)
- `libc++_shared.so` = STL
- NEEDED: libc++_shared, libfmodex, libfmodevent, libGLESv2, libEGL, liblog, libjnigraphics, libNimble, libc/m/dl

## Recon de símbolos (new-port-arm.sh)
- 540 importados (UND), **352 auto-resolvidos (65%)**, 188 a implementar.
- 145 GLES, 48 pthread, 128 passthrough, 30 cxx/abi/log.
- **188 UNKNOWN = maioria libc/posix trivial** (dlopen/dlsym/clock/chmod/fork/feof/ferror/getaddrinfo/getauxval/...) → passthrough glibc.
- Específicos a tratar: `AndroidBitmap_*` (libjnigraphics), `FMOD_*` (áudio), dlopen/dlsym (nossa versão do loader).

## Roadmap (fases, estilo DYSMANTLE)
- [x] **F0 — build:** loader armhf (so_util REL do gtavc-build) + wire dos 188 (libc→glibc), compila.
- [x] **F1 — load:** libapp+libc++_shared+libNimble+FMOD multi-modulo, so_snapshot_symbols+fallback dlsym, 0 UNRESOLVED, JNI_OnLoad+nativeOnCreate achados. ✅ carrega libapp+libNimble+deps, relocs OK, JNI_OnLoad roda sem crash.
- [ ] **F2 — boot JNI:** replicar GameActivityMain.nativeOnCreate + JNI env (igual DYSMANTLE GameActivity struct). Pegar o init da engine.
- [ ] **F3 — GLES2/EGL:** EGL→SDL2 Mali fbdev, contexto GLES2, primeiro frame.
- [ ] **F4 — dados:** montar o OBB (623MB) no path que a engine espera.
- [ ] **F5 — áudio:** FMOD (rodar real ou bridge).
- [ ] **F6 — render/perf:** otimizar p/ Mali-450; controle USB; empacotar.

## Notas/pegadinhas (da experiência)
- ⚠️ Canary bionic tpidr+0x28: pad TLS 256B `_Thread_local` no exe (vale p/ todo so-loader). Ver [[DYSMANTLE]].
- ⚠️ armhf: runtime /usr/lib32 (glibc 2.43 32-bit + libMali.m450 + SDL2), launcher PORT_32BIT=Y. Kernel 3.14 sem time64 → shim nextclock.so (time32) como no GTA CTW.
- ⚠️ Buildar sempre da pasta do port. ES mascarado p/ testes.
- so_util armhf: `~/gtavc-build/loader/so_util.c` (REL relocs).

## F2 (boot JNI) — EM ANDAMENTO + bug de heap latente
- Adicionado ao main.c: so_execute_init_array() por módulo, setup jni_shim, chamada JNI_OnLoad
  + nativeOnCreate(env, fake_this, 0...). MEMORY_MB libapp 320→64 (suficiente, loada 11.6MB).
- ⚠️ **BUG: heap corruption `malloc(): invalid size (unsorted)`** no load do libapp (durante
  so_resolve→dlsym, que mallocaa). DIAGNÓSTICO: F1 commitado funciona (0 corrupção); minhas
  adições F2 EXPÕEM um overflow LATENTE de heap (não-trigger no layout do F1). Descartado por
  bissecção: NÃO é init_array (NFS_SKIPINIT segue quebrando), NÃO é a sonda de heap (removida,
  segue). Suspeito = setvbuf(stdout/stderr,_IONBF) pré-load deslocando o layout e expondo o
  overflow. Removido o setvbuf (não testado — device caiu). PRÓXIMO: testar sem setvbuf; se
  funcionar, AINDA achar o overflow real (em so_snapshot_symbols? comb_append? so_util load?)
  pra não voltar no F2+ (nativeOnCreate alocará muito). debugPrintf alternativo p/ logs sem setvbuf.
- ⚠️ device nextos-87 caiu após ~15 runs (sem rota) — rebootar/power-cycle (lição
  [[reference]] DYSMANTLE: device degrada em sessões longas de teste).

## F2 — CAUSA-RAIZ da heap corruption ACHADA: construtor 11 do libapp
- Instrumentei so_execute_init_array (NFS_INITDBG): **construtor 11/189 do libapp (vaddr 0x68be0/
  file 0x68bfc) corrompe o heap glibc**. Construtores 0-10 passam a sonda; o 11 falha.
- Desmontado: o ctor 11 constrói uma std::string ("AI/Avoidance...") — chama um alocador
  (0x3de128) e escreve ~13 bytes. **Overflow pequeno** que o malloc do bionic tolera (layout de
  chunk diferente) mas o glibc detecta ("malloc(): invalid size (unsorted)").
- **FIX (proven em so-loaders): malloc/calloc/realloc/memalign com PADDING (+64B)** em
  src/imports.c (pad_malloc etc.) → absorve o overflow do engine na folga, sem corromper a
  metadata do glibc. free/realloc usam o mesmo ponteiro base (consistentes).
- ⚠️ NÃO TESTADO (device caiu — roteador reiniciou, possível mudança de IP DHCP). Build OK.
  PRÓXIMO: quando o device voltar, testar NFS_INIT=1 (init_array completa sem corromper?) →
  depois nativeOnCreate → EGL/render.
- init_array agora DEFAULT-OFF (NFS_INIT=1 liga); com o padding, a meta é reativar por default.

## F2 — progresso e MURO ATUAL (engine init multi-thread)
- ✅ **padding malloc (+64B)** ELIMINOU a heap corruption (ctor 11 = std::string overflow que
  bionic tolera). init_array agora roda os ctors 0-185 do libapp.
- ✅ **JNI_OnLoad funciona (0x10002 = JNI_VERSION_1_2)**, 0 crash, no estado DEFAULT (init OFF).
- ⚠️ **MURO: engine init multi-thread.** Com NFS_INIT=1: ctors finais do libapp (186,187,188 —
  config/strtoul que acessam globais nulos) crasham, E um ctor anterior spawna uma WORKER THREAD
  que crasha async em null+0x8 (não é FMOD — skip do FMOD não resolve). Skip por índice (NFS_SKIPCTOR)
  passa os finais mas a worker thread segue crashando. Recuperação por sinal (sigsetjmp/longjmp)
  NÃO funciona cross-thread.
- __stack_chk_guard/__stack_chk_fail: libapp usa o GLOBAL (resolve p/ glibc, estável) — NÃO o slot
  TLS, então o pad TLS do DYSMANTLE é desnecessário aqui (removido; mudava sig11→sig4).
- Infra adicionada (env-gated): NFS_INIT (liga init_array), NFS_SKIPINIT="lib", NFS_SKIPCTOR="i,j"
  (skip ctor por índice, só libapp), NFS_INITDBG (log por ctor), recuperação por sinal (so_util).
- **PRÓXIMO (multi-sessão):** a worker thread da engine precisa de um global que provavelmente é
  setado em nativeOnCreate (race: thread roda antes). Investigar: (1) qual ctor spawna a thread;
  (2) o que a thread deref nula em +0x8; (3) talvez adiar/bloquear as threads até nativeOnCreate,
  ou prover o global. Nível engine-comercial (DYSMANTLE/Hollow Knight). Estado DEFAULT estável
  (boot+JNI_OnLoad) preservado.

## F4 — ENGINE LÊ/PARSEIA O OBB! Frontier = objeto garbage no parse de asset
**PROGRESSO ENORME (do zero ao loading de assets):**
- ✅ softfp ABI shim (RE4): modf/math crashes resolvidos (+log10f e variantes f). A engine é
  SOFTFP (vmov r0,r1,d10 antes de bl modf); glibc é HARDFP → wrappers pcs("aapcs").
- ✅ jni_shim: cadeia File→getAbsolutePath, DisplayMetrics (widthPixels=1280/heightPixels=720 via
  GetIntField/GetFloatField), getObbFullPath, isObbAssets→1, getPackageName, getFilesDir.
- ✅ **OBB ABRE e a engine LÊ/PARSEIA** (fread header 1028B + parse byte-a-byte dos nomes de asset).
- ✅ render loop roda (tick recovery) + janela SDL2 1280x720 Mali fbdev.
- ⚠️ **MURO: durante o parse de asset, a engine cria um OBJETO GARBADE** (ponteiro wild) e ao
  destruí-lo (refcount→0, blx vtable[1]) crasha em memcpy(0,tid,11). Cascata: recuperar uma cópia
  leva à próxima (objeto todo é lixo). mIAssetV/mTextureV ficam vazios → tela preta.
  Frontier em libapp+0x47a920 (release-refcounted) ← 0x624a98 ← 0x4c971c.
- HIPÓTESE p/ o objeto garbage: ABI softfp num campo de struct do asset-dir, OU detalhe de ABI
  libc++ (shared_ptr/string control block), OU leitura de tamanho/contagem com endian/softfp errado
  no parse. PRÓXIMO: instrumentar o parse (0x4c971c/0x624a98), ver o objeto sendo criado; comparar
  com mais softfp (sincos? __aeabi float helpers?); checar se mais funções math via ponteiro.
- Flags de teste: NFS_INIT=1 NFS_SKIPCTOR="186,187,188" + NFS_TICKRECOVER/NFS_FOPENLOG/NFS_PTLOG/
  NFS_DLSYMLOG/NFS_NORECOVER. Hooks: pthread(NULL-attr) fopen open fread dlsym(softfp). OBB em
  /storage/roms/nfs/data/Android/obb/<pkg>/main.1003128.<pkg>.obb (623MB, deployado).

## F4 — DESCOBERTA: os "crashes" são ASSERTS DELIBERADOS (raise SIGSEGV, si_code=-6)
- O crash recorrente é `pthread_kill/raise(SIGSEGV)` (libc+0x7c720 = gettid→tgkill(getpid,tid,11)),
  **si_code=-6 (SI_TKILL)** = a engine RAISA SIGSEGV DE PROPÓSITO nos asserts (debug build, estilo
  breakpoint de debugger). Crash handler agora IGNORA si_code<=0 (NFS_NOASSERTIGNORE p/ desligar).
- Mas ignorar o assert → a engine usa o objeto garbage → crash REAL (blx r1 com r1 lixo, 0xe12fff36).
  Então o assert PROTEGE contra um objeto genuinamente garbage criado no parse de asset.
- Com assert-ignore + tick-recovery: render loop roda 300 frames, mas assets vazios → tela preta.
- **HIPÓTESE LÍDER p/ o objeto garbage: ABI libc++ iostream/string.** A engine lê o OBB byte-a-byte
  (= std::istream::get) e cria objetos. Se uma VTABLE de iostream/streambuf/locale resolveu pra
  libstdc++ (GNU) em vez da libc++ (LLVM snapshot), ou um símbolo _ZTV* caiu no dlsym errado → ABI
  divergente → objeto garbage. PRECEDENTE: DYSMANTLE precisou expor basic_filebuf::open do snapshot.
- **PRÓXIMO:** (1) checar se _ZTVSt*/_ZTVNSt* (vtables iostream) e std::locale/ctype resolveram pro
  snapshot libc++ ou pro libstdc++ (grep UNRESOLVED + comparar endereços); (2) forçar as vtables
  iostream do snapshot na tabela; (3) instrumentar 0x624a40/0x626e50 (criação do objeto). Maps
  lookup no crash handler já mostra PC/LR por módulo.

## F4 — relocações DESCARTADAS (todas aplicadas)
- libapp usa relocações PADRÃO (DT_REL 388264B=48533 relocs, REL não-RELA, não-Android-packed).
  A seção .rel.dyn cobre os 388264B completos → so_relocate (que itera .rel.dyn/.rel.plt) aplica
  TODAS. Vtables (.data.rel.ro) relocadas OK. NÃO é relocação faltando.
- Logo o objeto garbage é genuíno do parse: objeto alocado-mas-não-construído (vtable=lixo do
  malloc), OU asset não-encontrado no OBB → objeto lixo retornado → assert/crash ao usar/destruir.
- PRÓXIMO: instrumentar o parse (qual asset/nome a engine procura e não acha; ver se o índice do
  OBB é lido certo; checar se falta um arquivo além do OBB — só 1 fopen). Possível: a engine espera
  um 2º arquivo (patch obb / config) ou o índice do OBB precisa de outro parse.

## F4 — config strings preenchidos (não era o garbage), frontier confirmado
- jni_shim agora retorna ApplicationVersion="1.3.128", DeviceName="NextOS", Locale="en_US",
  Language="en", OsVersion="9" (eram vazios). A engine loga corretos, MAS o objeto garbage
  persiste (mesmo crash 0xe12fff36 pós assert). Logo a versão/config NÃO era a causa.
- 0x626e50 = tokenizer (split por '.'/',') com callback (blx r7=global). Garbage object segue
  no parse de asset, antes de ler dados (só ~1046 bytes lidos do OBB: header 1028 + início do dir).
- RULED OUT cumulativo: relocações, softfp (todas+f variants), arquivos faltando, asserts
  (deliberados si_code=-6), version/config strings, screen size, OBB path/abertura.
- **O bug é fundo no parse do índice do OBB / criação do objeto de asset** — precisa de RE focada
  multi-sessão (instrumentar a criação do objeto entre o header-read e o crash; entender o formato
  do índice do OBB EA; ver se o objeto é não-construído vs asset-não-achado). Cheguei a ~95% do boot.
