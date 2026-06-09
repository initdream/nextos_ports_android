# RE4 (Unity 2018 Mono) → Mali-450 / NextOS Amlogic-old — RESUMO PRA PRÓXIMA SESSÃO

> Última sessão: 2026-06-09 (sessão 9). Atualize este arquivo ao avançar.

## TL;DR — onde estamos (SESSÃO 9)
**2 muros CAÍRAM**, 1 muro real restante:
- ✅ **Crash do render loop RESOLVIDO** — era **MISMATCH DE ABI FLOAT** (softfp × hardfp), NÃO "race".
  libunity/libmono são SOFTFP (passam double/float em regs inteiros r0:r1); o glibc do device é HARDFP
  (espera em d0..d7). `modf(512.0,iptr)` lia o ponteiro do word-baixo de 512.0=0 → escrita em NULL
  (`modfl+132 vstr d16,[r0]`). FIX = `src/softfp_shim.c` (44 wrappers `pcs("aapcs")`, roteados em
  `re4_resolve`). **SEGV=0.** Agora a engine CARREGA assemblies Mono + assets de verdade.
- ✅ **OOM no load RESOLVIDO** — não era RAM (rss 114MB, 500MB livre); era **contabilidade de overcommit**
  (proc 32-bit reserva ~1GB virtual). FIX = `vm.overcommit_memory=1` + **swap 2GB persistente** (infra
  do device, já montada e no boot — ver `reference_nextos_swap_overcommit_setup`).
- ⛔ **MURO REAL = GC cooperativo AUTOMÁTICO durante o load.** Passando do crash+OOM, a engine entra no
  load pesado da cena e **trava no GC** (o `RE4_NOGCCOLLECT` só cobre `GC.Collect` EXPLÍCITO; o GC
  automático por-alocação dispara o stop-the-world cooperativo que deadlocka — mesmo muro da FASE 2i/2j).

### ⚠️ ARMADILHA DESCOBERTA (sessão 9): NÃO usar `GC_DONT_GC=1`
Desligar o GC de vez faz a memória **explodir** e martelar os 2GB de swap no eMMC lento →
**thrash que trava o device por completo** (load avg 16 = IO-wait, nem o watchdog on-device roda o
kill) → **precisa REBOOT físico**. O swap de 2GB virou faca de dois gumes: runaway de memória agora
**thrasha até morrer** em vez de OOM-killar rápido. **Antes de testar runaway de memória, pôr um CAP de
memória via cgroup v1** (igual Toziuha/Crash: `/sys/fs/cgroup/memory/<grupo>/memory.limit_in_bytes`
~580-650MB) pra OOM-killar rápido e iterar sem reboot. O device ESTÁ travado agora (fim da sessão 9),
aguardando reboot do Felipe.

## Como buildar / deployar / testar
```bash
# build (toolchain Amlogic-old em $HOME/NextOS-Elite-Edition/.../toolchain)
cd ~/nextos_ports_android/ports/re4 && ./build_re4boot.sh
scp -q re4boot nextos-87:/storage/roms/re4-recon/re4boot

# RUNNER SEGURO (não trava o ssh; watchdog on-device mata sozinho + reinicia ES):
#   /storage/roms/re4-recon/testrun.sh "<ENV EXTRA>" <segundos>
# lançar destacado com NOHUP (⚠️ setsid NÃO existe no device):
ssh nextos-87 'nohup sh /storage/roms/re4-recon/testrun.sh "" 50 </dev/null >/tmp/testrun.boot 2>&1 &'
# depois reconectar e ler /tmp/re4.err (log) + /tmp/re4.threads (amostras de wchan a cada 5s)
# overcommit=1 já vem do boot (nextos-swap.sh). swap 2GB ativo via /dev/loop1.
```
Conferir: `grep -c "\[render" /tmp/re4.err`, swap=`grep -c "REAL Swap"`, crash=`grep -c "SEGV\]"`.
⚠️ SEMPRE rodar com timeout/watchdog — o GC-hang trava o ssh.

## MURO ÚNICO RESTANTE = GC cooperativo automático no load (ordem de ataque sugerida)
Contexto: o GC do Mono é **COOPERATIVO, não por sinal** (PTKILL=0). A coletora (main) seta flag e
espera cada thread chegar num SAFEPOINT e dar `sem_post` no ack-sem ESTÁTICO (mono+0x273da4). Uma
thread **GC-Unsafe bloqueada** (nativa Unity attached ao Mono, num wait não-cooperativo) nunca dá ack
→ deadlock. (Bt da coletora travada: `sh_sem_wait <- mono+0x2c3954`.) O `RE4_NOGCCOLLECT` só no-opa o
`GC.Collect` EXPLÍCITO; o **auto-GC por alocação** (durante o load) ainda dispara e trava.

**PRIMEIRO PASSO obrigatório: cgroup memory cap** (senão runaway-memória thrasha o swap e trava o
device — ver armadilha acima). Algo como:
```bash
mkdir -p /sys/fs/cgroup/memory/re4; echo 620M > /sys/fs/cgroup/memory/re4/memory.limit_in_bytes
echo $$ > /sys/fs/cgroup/memory/re4/cgroup.procs   # no wrapper, antes do exec do re4boot
```
Assim, se a memória explodir, OOM-killa SÓ o re4boot (rápido), sem wedge. Integrar no testrun.sh.

**Planos pro GC (do mais promissor):**
1. **Achar a thread GC-Unsafe bloqueada** que não dá ack. Sob o cgroup cap + gdb no device:
   `for t in /proc/PID/task/*; do echo $t $(cat $t/comm) $(cat $t/wchan); done` no momento do hang;
   a thread em wchan de IO/futex que NÃO é a coletora é a suspeita. Provável: AsyncReadManager /
   Worker Thread (read() de asset) OU uma thread criada por `sh_create` attached ao Mono.
2. **Envolver os waits/blocking dela em GC-Safe region**: hookar o ponto onde ela bloqueia e chamar
   `mono_threads_enter_gc_safe_region()` / `..._exit_gc_safe_region()` ao redor (símbolos no libmono).
   Assim a coletora não a espera. Alternativa: `mono_thread_info_install_interrupt` / marcar como
   não-suspendível. (Investigar os símbolos `mono_threads_*gc_safe*` / `mono_thread_state_*` no libmono.)
3. **Reduzir a PRESSÃO de GC no load** (em vez de desligar): `GC_FREE_SPACE_DIVISOR` baixo + heap
   inicial GRANDE o bastante p/ NÃO disparar auto-GC durante o load (mas SEM `GC_DONT_GC`, que estoura).
   Calibrar o heap inicial pra caber sob o cgroup cap. Talvez `GC_MAXIMUM_HEAP_SIZE` ajude a estabilizar.
4. **Plano B — SGen em vez de Boehm?** Improvável trocar; o libmono embute Boehm. Focar em 1+2.

(HISTÓRICO: o antigo "muro 3 / crash de race unity+0xfc3704" foi DESMASCARADO = era o bug de ABI float,
agora resolvido. O antigo "muro 1 / swap=0" provavelmente é consequência do load não terminar por
causa do GC — atacar o GC primeiro deve destravar o swap junto.)

## DESTRAVES já feitos (reutilizáveis p/ QUALQUER Unity 2018 Mono so-loader)
- **Cross-thread GL** (insight do Bully): captura EGL real (eglGetCurrent*), pega config EXATO da
  surface (eglQuerySurface CONFIG_ID, senão BAD_MATCH 0x3009), **1 contexto EGL real compartilhado
  POR THREAD** (Unity passa o MESMO handle p/ várias threads; compartilhar 1 ctx = BAD_ACCESS 0x3002),
  **surface split**: thread de SETUP usa PBUFFER, thread de RENDER usa o WINDOW. Tudo via eglMakeCurrent
  REAL (dlsym libEGL), NÃO o SDL_GL_*. → egl_shim.c.
- **_ctype_/_tolower_tab_/_toupper_tab_ = VARIÁVEL PONTEIRO** (bionic, 2 derefs): resolver p/ o
  endereço da tabela (1 deref) crashava no preproc de shader (isspace). g_ctype_p=&g_ctype[0];
  _ctype_ -> &g_ctype_p. → imports.gen.c.
- **glGetString cache/fallback** (nunca NULL) → main_re4.c.
- **_ctype_ bits** batem com bionic (0x01 U,0x02 L,0x04 D,0x08 S,0x10 P,0x20 C,0x40 X,0x80 B).
- pmap_get **lock-free no lookup** (signal-safe). sh_create **wrapper** desbloqueia SIGPWR/SIGXCPU
  (não ajudou no GC pq é cooperativo, mas é correto).
- ig_getattr_np memset(24) (bionic pthread_attr_t=24B), sysconf bionic→glibc, jstring persistente
  (PlayerPrefs), 2 cores (job system), ANativeWindow shims, libz/inflate, EGL config match.
- **Áudio**: FMOD do Unity NÃO usa OpenSL — usa **AudioManager/AudioTrack via JNI** (stubado).
  "FMOD failed to initialize output device" é **NÃO-FATAL** (Unity continua). opensles_shim wirado
  (dlopen libOpenSLES->shim) mas não é acionado. Áudio NÃO é a causa de nada do hang.

## Gates de ambiente (env vars)
- `RE4_NOGCCOLLECT=1` — no-op no mono_gc_collect (pula GC.Collect explícito; necessário p/ render loop)
- `RE4_NOSIGH=1` — bloqueia install de handlers de sinal do Unity (necessário)
- `RE4_GLDIAG=1` — loga GL_VERSION/shaders/compile
- `RE4_SEMLOG=1` — loga sem_init/wait/post (achou o ack-sem do GC)
- `GC_INITIAL_HEAP_SIZE` / `GC_FREE_SPACE_DIVISOR` — env do Boehm (evitar auto-GC no init)

## Tooling de debug
- gdb no device (sem python, sem CFI p/ libs so-loaded). Mapear via base: `awk` pegando r-xp
  sem nome em /proc/PID/maps (1ª linha=libmono, 2ª=libunity). Stack scan: `x/Nxw $sp` + mapear.
- Símbolos: /tmp/usyms.txt (unity), /tmp/monosyms.txt (mono) — gerados de readelf --dyn-syms.
  ⚠️ símbolos template (Rb_tree/vector) com offset gigante = NÃO confiáveis (pega o símbolo errado).
- `for t in /proc/PID/task/*; do cat $t/wchan; done | sort | uniq -c` = estado das threads.
- Threads do jogo: UnityMain, __MONO__, UnityGfxDeviceW(render), ~16 BackgroundWorker(ThreadPool C#),
  Worker Thread, BatchDeleteObje, AsyncReadManage, FMOD mixer/stream.

## Arquivos-chave
- ~/nextos_ports_android/ports/re4/src/: main_re4.c (dlopen/dlsym/sysconf/hooks/render loop),
  imports.gen.c (_ctype_, resolver), pthread_shim.c (sem/cond/mutex bridge + sh_create wrapper),
  egl_shim.c (cross-thread EGL), jni_shim.c (jstring), opensles_shim.c (áudio), so_util.c (loader).
- build_re4boot.sh (usa $HOME). re4boot NÃO versionado (binário).
- Doc viva: experiments/re4-recon/RE4-MALI450-PORT.md. Memória: project_re4_unity_mono_soloader_mali450.md (FASE 2a..2j).

## Regras do Felipe
- Modo autônomo, responder em PT, não parar no 1º erro, commitar cada avanço.
- NUNCA Co-Authored-By Claude nos commits. NÃO commitar .so/binários/assets nem IP/senha (repo PÚBLICO).
- Device prioritário = Amlogic-old (Mali-450 fbdev, kernel 3.14).
