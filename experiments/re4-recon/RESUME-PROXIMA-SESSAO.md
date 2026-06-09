# RE4 (Unity 2018 Mono) → Mali-450 / NextOS Amlogic-old — RESUMO PRA PRÓXIMA SESSÃO

> Última sessão: 2026-06-09 (sessão 8). Atualize este arquivo ao avançar.

## TL;DR — onde estamos
A **render loop do jogo RODA 100+ frames** (a main loop do RE4 executa). Banner confirma:
`com.teamcherry.hollowknight`, Unity **2018.1.1f1**, `SystemInfo Cores=2 Memory=512mb`, extensões GL_OES.
Falta: o jogo está em **estado de LOADING** (não chama `eglSwapBuffers` → `swap=0` → nada na tela ainda).
Saímos de "engine não inicializa" → "main loop do jogo rodando". ~95% do caminho.

## Como buildar / deployar / testar
```bash
# build (toolchain Amlogic-old em $HOME/NextOS-Elite-Edition/.../toolchain)
cd ~/nextos_ports_android/ports/re4 && ./build_re4boot.sh
scp -q re4boot nextos-87:/storage/roms/re4-recon/re4boot

# rodar no device (alias ssh = nextos-87). RECEITA que chega na render loop:
ssh nextos-87 'cd /storage/roms/re4-recon; systemctl stop emustation; sleep 1; pkill -9 re4boot
  GC_INITIAL_HEAP_SIZE=134217728 GC_FREE_SPACE_DIVISOR=50000 RE4_NOGCCOLLECT=1 RE4_NOSIGH=1 \
  SDL_VIDEODRIVER=mali LD_LIBRARY_PATH=/usr/lib32:/usr/lib ./re4boot > /tmp/re4.err 2>&1'
# SEMPRE: systemctl start emustation no fim. dd if=/dev/fb0 ... pra capturar tela.
# ⚠️ NÃO deixar loop dd em background no mesmo ssh (estoura o timeout do ssh).
```
Conferir progresso: `grep -E "\[render [0-9]" /tmp/re4.err` (loga f<5 || f%100). `swap`: `grep -c "REAL SwapBuffers"`.

## Os 3 MUROS restantes (ordem sugerida)
1. **swap=0 / por que não aparece imagem** ← ATACAR PRIMEIRO. `nativeRender` roda mas nunca chama
   `eglSwapBuffers` (egl_shim_SwapBuffers não é chamado). Jogo preso em loading: a **carga async da
   cena** não termina. Investigar: qual thread/Task faz o load e por que não completa; achar onde
   nativeRender decide "ainda carregando, não renderiza". Ver assets abertos (`globalgamemanagers.assets`,
   level0, etc) e se a leitura completa.
2. **GC cooperativo (fix REAL)** — o bypass atual (GC_INITIAL_HEAP_SIZE+FREE_SPACE_DIVISOR+NOGCCOLLECT)
   é HACK (memória cresce → insustentável). O GC do Mono é **COOPERATIVO, não por sinal** (PTKILL=0!):
   a coletora (main) seta flag e espera cada thread chegar num SAFEPOINT e dar `sem_post` no ack-sem
   ESTÁTICO do libmono (mono+0x273da4). Uma thread **GC-Unsafe bloqueada** nunca dá ack → deadlock.
   FIX: achar essa thread (provável thread NATIVA do Unity attached ao Mono, bloqueada num wait
   não-cooperativo) e fazer ela entrar em GC-Safe region antes de bloquear; OU rotear o wait dela
   pela API coop do Mono. (Bt da coletora travada: sh_sem_wait <- mono+0x2c3954.)
3. **Crash de race** não-determinístico entre [render 4] e [render 100+], em
   `unity+0xfc3704 / 0xb8e4a4 / 0x872a88` (símbolos template não confiáveis), fault=0x64de0, raise
   deliberado. Provável race de threading no path de render/loading.

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
