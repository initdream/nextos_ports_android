/*
 * main.c — Cuphead (Unity 2017.4.40f1 IL2CPP) so-loader → NextOS/Mali-450 (arm64, GLES2).
 *
 * Receita Unity baseada no port re4 (Unity 2018 Mono), adaptada p/ arm64 + IL2CPP:
 *   - dlopen libz/libGLESv2/libEGL RTLD_GLOBAL (Unity resolve via dlsym RTLD_DEFAULT)
 *   - so_load libunity.so (engine) -> imports overrides -> init_array
 *   - so_load libil2cpp.so (lógica do jogo C#) + global-metadata.dat   [fase seguinte]
 *   - JNI_OnLoad -> janela GLES2 -> lifecycle (initJni/nativeRender)    [fase seguinte]
 * Alvo GLES2: passar -force-gles20 ao Unity (args via initJni/command line).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <SDL2/SDL.h>

#include "so_util.h"
#include "imports.h"
#include "jni_shim.h"
#include "opensles_shim.h"
#include "util.h"
#include <link.h>

#define HEAP_MB 96

/* ---- dl_iterate_phdr custom (INTERPÕE o da libc) ----
 * O unwinder C++ da libgcc acha o .eh_frame de cada lib via dl_iterate_phdr. Nossos
 * módulos (libunity/libil2cpp) são mapeados à mão -> invisíveis ao dl_iterate_phdr da
 * libc -> exceção C++ não acha o landing pad -> std::terminate -> abort (asset loading).
 * Como o EXE é -rdynamic e carrega 1º, este símbolo INTERPÕE o da libc: reportamos os
 * módulos do dynamic linker (via o real, RTLD_NEXT) + os NOSSOS (g_so_mods). */
int dl_iterate_phdr(int (*cb)(struct dl_phdr_info *, size_t, void *), void *data) {
  static int (*real)(int (*)(struct dl_phdr_info *, size_t, void *), void *);
  if (!real) real = (void *)dlsym(RTLD_NEXT, "dl_iterate_phdr");
  int r = real ? real(cb, data) : 0;
  if (r) return r;
  for (int i = 0; i < g_so_nmods; i++) {
    struct dl_phdr_info info; memset(&info, 0, sizeof info);
    info.dlpi_addr = (ElfW(Addr))g_so_mods[i].base;
    info.dlpi_name = g_so_mods[i].name;
    info.dlpi_phdr = (const ElfW(Phdr) *)g_so_mods[i].ph;
    info.dlpi_phnum = (ElfW(Half))g_so_mods[i].phnum;
    r = cb(&info, sizeof info, data);
    if (r) return r;
  }
  return r;
}

/* canary bionic: libunity lê o stack-guard de tpidr_el0+0x28 (TLS_SLOT_STACK_GUARD
 * do bionic); sob glibc esse offset cai no TLS de outra lib e MUDA em runtime →
 * __stack_chk_fail espúrio (e o "SEGV após neutralizar" era o no-op retornando em
 * código adjacente — noreturn). Pad TLS no exe (1º bloco após o TCB de 16B) cobre
 * offset 16..272 e NUNCA é escrito → slot estável. (causa-raiz achada no Dysmantle) */
/* = {1} → .tdata: fica ANTES das TLS .tbss do egl_shim (link order) no template,
 * senão o pad desliza p/ +0x30 e o slot +0x28 cai fora (visto no device). */
__attribute__((aligned(16))) static _Thread_local char g_bionic_guard_pad[256] = {1};

/* fsync(stderr→debug.log): garante que o log sobrevive a hang/power-cycle */
static void dbg_sync(void) { fsync(2); }

/* sem_shim: semáforos próprios (bionic sem_t 4B vs glibc 32B) — ver sem_shim.c.
   CAUSA-RAIZ do deadlock no boot: sem_post do glibc não acordava o sem_wait da
   thread pool do Unity. CUP_NOSEMSHIM=1 desliga (volta ao glibc cru). */
extern int sh_sem_init(void *, int, unsigned);
extern int sh_sem_wait(void *);
extern int sh_sem_trywait(void *);
extern int sh_sem_timedwait(void *, const struct timespec *);
extern int sh_sem_post(void *);
extern int sh_sem_getvalue(void *, int *);
extern int sh_sem_destroy(void *);
extern int g_main_tid;
extern void sh_tick_preload(void);
extern void sh_sem_set_poll(int ms);
static void set_import(const char *name, void *fn);
static int patch_got(const char *name, void *fn);
static void install_sem_shim(void) {
  if (getenv("CUP_NOSEMSHIM")) return;
  set_import("sem_init", (void *)sh_sem_init);
  set_import("sem_wait", (void *)sh_sem_wait);
  set_import("sem_trywait", (void *)sh_sem_trywait);
  set_import("sem_timedwait", (void *)sh_sem_timedwait);
  set_import("sem_post", (void *)sh_sem_post);
  set_import("sem_getvalue", (void *)sh_sem_getvalue);
  set_import("sem_destroy", (void *)sh_sem_destroy);
}
static void patch_sem_shim(void) {
  if (getenv("CUP_NOSEMSHIM")) return;
  patch_got("sem_init", (void *)sh_sem_init);
  patch_got("sem_wait", (void *)sh_sem_wait);
  patch_got("sem_trywait", (void *)sh_sem_trywait);
  patch_got("sem_timedwait", (void *)sh_sem_timedwait);
  patch_got("sem_post", (void *)sh_sem_post);
  patch_got("sem_getvalue", (void *)sh_sem_getvalue);
  patch_got("sem_destroy", (void *)sh_sem_destroy);
}

/* ---------- crash handler (arm64) ---------- */
static uintptr_t g_unity_base, g_il2cpp_base, g_unity_data;
static uintptr_t g_i2heap_base, g_i2heap_size;
extern size_t text_size;
/* /proc/self/maps lido UMA vez (sem malloc — open/read/parse manual; fopen não é
 * async-signal-safe e re-faulta no handler). Buffer estático grande o bastante. */
static char g_maps_buf[64 * 1024];
static int g_maps_len;
static void maps_snapshot(void) {
  int fd = open("/proc/self/maps", O_RDONLY);
  g_maps_len = 0;
  if (fd < 0) return;
  int n; char *p = g_maps_buf;
  while (g_maps_len < (int)sizeof(g_maps_buf) - 1 &&
         (n = read(fd, p + g_maps_len, sizeof(g_maps_buf) - 1 - g_maps_len)) > 0)
    g_maps_len += n;
  g_maps_buf[g_maps_len] = 0;
  close(fd);
}
/* acha a linha de maps que contém 'a'; preenche lo/hi/perm; retorna ptr p/ a linha
 * (NUL-terminada temporariamente) ou NULL. Parse manual sobre o snapshot. */
static const char *maps_find(uintptr_t a, uintptr_t *lo_o, uintptr_t *hi_o, char perm_o[5]) {
  const char *s = g_maps_buf;
  while (s < g_maps_buf + g_maps_len) {
    const char *eol = s; while (*eol && *eol != '\n') eol++;
    uintptr_t lo = 0, hi = 0; const char *q = s;
    while (*q && *q != '-') { lo = lo * 16 + (*q <= '9' ? *q - '0' : (*q | 32) - 'a' + 10); q++; }
    if (*q == '-') q++;
    while (*q && *q != ' ') { hi = hi * 16 + (*q <= '9' ? *q - '0' : (*q | 32) - 'a' + 10); q++; }
    if (a >= lo && a < hi) {
      if (lo_o) *lo_o = lo; if (hi_o) *hi_o = hi;
      if (perm_o) { const char *pp = q + 1; for (int i = 0; i < 4; i++) perm_o[i] = pp[i]; perm_o[4] = 0; }
      static char line[256]; int len = (int)(eol - s); if (len > 255) len = 255;
      for (int i = 0; i < len; i++) line[i] = s[i]; line[len] = 0;
      return line;
    }
    s = (*eol == '\n') ? eol + 1 : eol;
  }
  return NULL;
}
static int addr_readable(uintptr_t a) {
  char perm[5]; uintptr_t lo, hi;
  return maps_find(a, &lo, &hi, perm) && perm[0] == 'r';
}
/* imprime a linha de maps que contém 'a' + classifica vs nossas bases */
static void crash_classify(const char *tag, uintptr_t a) {
  fprintf(stderr, "[CR] %s=0x%lx", tag, (unsigned long)a);
  if (g_unity_base && a >= g_unity_base && a < g_unity_base + text_size)
    fprintf(stderr, " (libunity+0x%lx)", a - g_unity_base);
  else if (g_il2cpp_base && a >= g_il2cpp_base && a < g_il2cpp_base + 0x3000000)
    fprintf(stderr, " (libil2cpp+0x%lx)", a - g_il2cpp_base);
  else if (g_i2heap_base && a >= g_i2heap_base && a < g_i2heap_base + g_i2heap_size)
    fprintf(stderr, " (i2heap+0x%lx)", a - g_i2heap_base);
  char perm[5]; uintptr_t lo, hi;
  const char *line = maps_find(a, &lo, &hi, perm);
  if (line) fprintf(stderr, "  | %s", line);
  fprintf(stderr, "\n"); dbg_sync();
}
static void crash_dump_qwords(const char *tag, uintptr_t base, int n) {
  if (!addr_readable(base)) { fprintf(stderr, "[CR] %s @0x%lx ILEGÍVEL\n", tag, (unsigned long)base); dbg_sync(); return; }
  for (int k = 0; k < n; k += 2)
    fprintf(stderr, "[CR] %s +%02x: %016lx %016lx\n", tag, k * 8,
            (unsigned long)((uintptr_t *)base)[k], (unsigned long)((uintptr_t *)base)[k + 1]);
  dbg_sync();
}

static volatile int g_crashing = 0;
#define ARENA_LO 0x7f10000000UL
#define ARENA_HI 0x7f10200000UL
static volatile unsigned long g_skipbad_n = 0;
static int g_skipbad = 0;  /* lido 1× no startup (getenv não é async-signal-safe) */
/* recovery por-frame: sigsetjmp antes de nativeRender; on_crash siglongjmp de volta
   (só se o crash for na THREAD de render — longjmp cross-thread é UB). Pula o frame
   corrompido e continua → renderiza apesar das chamadas de método C# corrompidas. */
#include <setjmp.h>
/* GC stop-the-world: SIGPWR suspende a thread (espera o restart SIGXCPU); SIGXCPU
   é no-op (sua chegada acorda o sigsuspend). Mantém nossas threads vivas durante a
   coleta (sem isso, SIGPWR default mata o processo -> exit 158). */
void gc_suspend_handler(int sig);
void gc_suspend_handler(int sig) {
  (void)sig;
  /* CUP_GCSUSP=wait: protocolo real (suspende até SIGXCPU). Default: RETORNA imediato
     (não suspende) — o stop-the-world do GC está quebrado (handler corrompido) e nunca
     manda o restart, então suspender CONGELA a render. Retornar deixa a thread seguir
     (coleta vira racy, mas a render avança → imagem). */
  if (getenv("CUP_GCSUSP")) {
    sigset_t m; sigfillset(&m);
    sigdelset(&m, SIGXCPU); sigdelset(&m, SIGSEGV); sigdelset(&m, SIGBUS);
    sigsuspend(&m);
  }
}
void gc_restart_handler(int sig);
void gc_restart_handler(int sig) { (void)sig; }
static sigjmp_buf g_render_jmp;
static volatile int g_render_jmp_armed = 0;
static int g_render_tid = 0;
static volatile unsigned long g_recover_n = 0;
static void on_crash(int sig, siginfo_t *si, void *uc_) {
  ucontext_t *uc0 = (ucontext_t *)uc_;
  uintptr_t pc0 = uc0->uc_mcontext.pc, lr0 = uc0->uc_mcontext.regs[30];
  /* recovery: crash na thread de render (qualquer fault, não só arena) → volta pro
     loop e pula o frame. Só se armado e na thread certa. */
  if (g_render_jmp_armed && (int)syscall(SYS_gettid) == g_render_tid) {
    g_recover_n++;
    siglongjmp(g_render_jmp, 1);
  }
  /* skipbad: crash em thread NÃO-render (worker/job) → estaciona a thread em vez de
     matar o processo (mantém o jogo vivo p/ a render continuar). */
  if (g_skipbad && sig == SIGSEGV) {
    static volatile unsigned long parked = 0;
    if (parked++ < 40)
      fprintf(stderr, "[PARK] worker tid=%d crashou (pc=0x%lx) — estacionado\n",
              (int)syscall(SYS_gettid), (unsigned long)pc0);
    dbg_sync();
    for (;;) pause();
  }
  /* CUP_SKIPBAD: o ponteiro de método genérico corrompido (→ arena 2MB) é chamado
     em vários sites. Se o pc cai na arena (chamou o lixo), PULA a chamada: retoma
     no lr com retorno null (x0=0). Se as chamadas não forem críticas, o jogo passa
     e renderiza. Hack p/ destravar a imagem (não é fix definitivo). */
  if (g_skipbad && sig == SIGSEGV && pc0 >= ARENA_LO && pc0 < ARENA_HI) {
    if (lr0 && lr0 != pc0) {
      uc0->uc_mcontext.pc = lr0;        /* retoma no retorno */
      uc0->uc_mcontext.regs[0] = 0;     /* valor de retorno = null/0 */
      if (g_skipbad_n++ < 60)
        fprintf(stderr, "[SKIPBAD] #%lu pc=arena -> pula p/ lr=0x%lx\n",
                g_skipbad_n, (unsigned long)lr0);
      if ((g_skipbad_n & 0x3ff) == 0) dbg_sync();
      return;  /* resume */
    }
  }
  /* reentrância: se outra thread já está dumpando (vtable corrompido faz várias
     threads crasharem juntas), esta espera p/ não interleavar/re-faultar o dump. */
  if (__sync_lock_test_and_set(&g_crashing, 1)) {
    fprintf(stderr, "[CR] (2ª thread crashou sig=%d tid=%d — aguardando)\n",
            sig, (int)syscall(SYS_gettid));
    dbg_sync();
    for (;;) pause();
  }
  ucontext_t *uc = (ucontext_t *)uc_;
  uintptr_t pc = uc->uc_mcontext.pc, lr = uc->uc_mcontext.regs[30];
  uintptr_t tb = (uintptr_t)text_base;
  maps_snapshot();   /* sem malloc — antes de qualquer parse */
  fprintf(stderr, "\n=== CRASH sig=%d fault=%p pc=0x%lx", sig, si->si_addr,
          (unsigned long)pc);
  if (pc >= tb && pc < tb + text_size) fprintf(stderr, " (libunity+0x%lx)", pc - tb);
  fprintf(stderr, " lr=0x%lx", (unsigned long)lr);
  if (lr >= tb && lr < tb + text_size) fprintf(stderr, " (lr unity+0x%lx)", lr - tb);
  fprintf(stderr, " ===\n"); dbg_sync();
  for (int i = 0; i < 31; i++) {
    fprintf(stderr, " x%-2d=0x%016lx", i, (unsigned long)uc->uc_mcontext.regs[i]);
    if (i % 3 == 2) fprintf(stderr, "\n");
  }
  dbg_sync();
  /* stack scan limitado à região mapeada da pilha desta thread (evita ler além
     do fim do mapping e re-faultar dentro do handler). */
  fprintf(stderr, "[stack scan]\n");
  uintptr_t sp = uc->uc_mcontext.sp;
  uintptr_t slo = 0, shi = 0; char sperm[5];
  maps_find(sp, &slo, &shi, sperm);
  uintptr_t send = shi ? shi : sp + 400 * 8;
  for (uintptr_t a = sp, hits = 0; a + 8 <= send && hits < 32; a += 8) {
    uintptr_t v = *(uintptr_t *)a;
    if (v >= tb && v < tb + text_size) { fprintf(stderr, "  [sp+0x%lx] libunity+0x%lx\n", a - sp, v - tb); hits++; }
    else if (g_il2cpp_base && v >= g_il2cpp_base && v < g_il2cpp_base + 0x3000000)
      { fprintf(stderr, "  [sp+0x%lx] libil2cpp+0x%lx\n", a - sp, v - g_il2cpp_base); hits++; }
  }
  dbg_sync();

  /* ---- dump rico do crash 0x7f10000004 (vtable/delegate corrompido) ---- */
  uintptr_t fault = (uintptr_t)si->si_addr;
  fprintf(stderr, "[CR] ==== diagnóstico de corrupção ====\n");
  crash_classify("pc", pc);
  crash_classify("fault", fault);
  /* região do ponteiro-lixo (pc=0x7f10000004): o que é 0x7f10000000? */
  crash_classify("pc_region", pc & ~0xFFFUL);
  crash_dump_qwords("pc_target", pc & ~0xFUL, 8);
  /* singleton: *(libunity_data + 0xd18) → método[0] foi p/ o lixo */
  if (g_unity_data) {
    uintptr_t pslot = g_unity_data + 0xd18;
    crash_classify("singleton_slot(d18)", pslot);
    if (addr_readable(pslot)) {
      uintptr_t sgl = *(uintptr_t *)pslot;
      crash_classify("singleton_obj", sgl);
      crash_dump_qwords("singleton", sgl, 16);
    }
  }
  /* dispatcher std::function/delegate: x19 é o objeto; lê [x19+248/256/264] */
  uintptr_t x19 = uc->uc_mcontext.regs[19];
  crash_classify("x19(dispatch_obj)", x19);
  crash_dump_qwords("x19", x19, 40);   /* cobre +0..+312 (inclui 248/256/264) */
  /* x8 = ponteiro de função chamado (= pc no blr x8); x21 = this provável */
  crash_classify("x8", uc->uc_mcontext.regs[8]);
  crash_classify("x21", uc->uc_mcontext.regs[21]);
  /* x20/x22/x23/x24: candidatos a 'this'/objeto pai */
  crash_classify("x20", uc->uc_mcontext.regs[20]);
  crash_classify("x22", uc->uc_mcontext.regs[22]);
  /* SITE DA CHAMADA: lr = retorno após o `blr` que pulou p/ 0x7f10000004.
     Classifica lr e dumpa as 4 instruções em lr-12..lr (acha o blr Xn + o ldr
     que carregou o ponteiro lixo: revela DE ONDE vem 0x7f10000004). */
  crash_classify("lr(call-site)", lr);
  if (addr_readable((lr - 16) & ~0x3UL)) {
    fprintf(stderr, "[CR] insns @lr-16..lr:\n");
    for (uintptr_t a = (lr - 16) & ~0x3UL; a <= lr; a += 4)
      fprintf(stderr, "[CR]   0x%lx: %08x%s\n", (unsigned long)a,
              *(uint32_t *)a, a == lr - 4 ? "  <- blr (chamou o lixo)" : "");
    dbg_sync();
  }
  /* alvo dos ponteiros da singleton (campos = 0x7f..cXX espaçados 4B): o que há lá? */
  if (g_unity_data && addr_readable(g_unity_data + 0xd18)) {
    uintptr_t sgl = *(uintptr_t *)(g_unity_data + 0xd18);
    if (addr_readable(sgl)) {
      uintptr_t tgt = *(uintptr_t *)sgl;       /* singleton[0] = 1º ponteiro */
      crash_classify("singleton[0]_target", tgt);
      crash_dump_qwords("sgl[0]_tgt", tgt & ~0xFUL, 8);
    }
  }
  /* x3/x9/x27: ponteiros 0x7f14.. recorrentes — que região? */
  crash_classify("x3", uc->uc_mcontext.regs[3]);
  crash_classify("x9", uc->uc_mcontext.regs[9]);
  crash_classify("x17", uc->uc_mcontext.regs[17]);
  fprintf(stderr, "[CR] ==== fim ====\n");
  dbg_sync();
  _exit(128 + sig);
}

/* ---------- overrides bionic->glibc (do re4) ---------- */
/* sysconf: Unity lê _SC_* com constantes BIONIC (≠ glibc) → page/nproc/phys errados. */
static long my_sysconf(int name) {
  int ncpu = getenv("CUP_1CORE") ? 1 : 4;
  switch (name) {
    case 39: case 40: return 4096;                 /* _SC_PAGE_SIZE/_SC_PAGESIZE bionic */
    case 6: return 100;                            /* _SC_CLK_TCK */
    case 96: case 97: return ncpu;                 /* _SC_NPROCESSORS_CONF/_ONLN (1 core => Unity desliga MT rendering) */
    case 98: return (512L*1024*1024)/4096;         /* _SC_PHYS_PAGES -> 512MB */
    case 99: return (256L*1024*1024)/4096;         /* _SC_AVPHYS_PAGES -> 256MB */
  }
  long r = sysconf(name);
  if ((name == _SC_PHYS_PAGES || name == _SC_AVPHYS_PAGES) && r <= 0)
    r = (512L*1024*1024)/4096;
  return r;
}
/* mmap spy: a arena de 2MB @ 0x7f10000000 (onde os vtables corrompidos apontam)
 * é um mmap de 0x200000. Logamos alocações desse tamanho + o caller (RA→libunity/
 * il2cpp offset) p/ identificar QUAL alocador/subsistema cria a arena. CUP_MMAPLOG. */
static int g_mmaplog;
extern void *mmap(void *, size_t, int, int, int, long);  /* glibc real */
static void *my_mmap(void *addr, size_t len, int prot, int flags, int fd, long off) {
  void *r = mmap(addr, len, prot, flags, fd, off);
  if (g_mmaplog && (len == 0x200000 || (len >= 0x100000 && len <= 0x400000))) {
    uintptr_t ra = (uintptr_t)__builtin_return_address(0);
    const char *lib = "?"; uintptr_t off2 = ra;
    if (g_unity_base && ra >= g_unity_base && ra < g_unity_base + text_size) { lib = "libunity"; off2 = ra - g_unity_base; }
    else if (g_il2cpp_base && ra >= g_il2cpp_base && ra < g_il2cpp_base + 0x3000000) { lib = "libil2cpp"; off2 = ra - g_il2cpp_base; }
    fprintf(stderr, "[MMAP] len=0x%zx prot=%d -> %p  caller=%s+0x%lx\n",
            len, prot, r, lib, (unsigned long)off2);
    fsync(2);
  }
  return r;
}
/* /proc/cpuinfo + /sys/.../cpu: Unity conta cores p/ dimensionar job workers. */
static int g_dllog;
static const char *asset_redirect(const char *p, char *buf, size_t bufsz);
static FILE *my_fopen(const char *p, const char *m) {
  if (p && !strcmp(p, "/proc/meminfo")) {
    FILE *t = tmpfile(); if (t) { fputs("MemTotal:      524288 kB\nMemFree:       262144 kB\nMemAvailable:  262144 kB\n", t); rewind(t); return t; }
  }
  if (p && (!strcmp(p, "/sys/devices/system/cpu/possible") || !strcmp(p, "/sys/devices/system/cpu/present") || !strcmp(p, "/sys/devices/system/cpu/online"))) {
    FILE *t = tmpfile(); if (t) { fputs(getenv("CUP_1CORE") ? "0\n" : "0-3\n", t); rewind(t); return t; }
  }
  char rb[512]; const char *r = asset_redirect(p, rb, sizeof rb);
  if (r) {
    if (g_dllog) fprintf(stderr, "[fopen-redir] %s -> %s\n", p, r);
    return fopen(r, m);
  }
  return fopen(p, m);
}
#define ASSET_BASE_M "/storage/roms/cuphead-recon/"
/* redirect genérico de assets: o engine monta paths de dados com bases erradas
   (APK inexistente, filesdir). Mapeia qualquer tentativa p/ os arquivos REAIS
   deployados em bin/Data (mesma receita do global-metadata.dat, generalizada:
   pega o sufixo após "bin/Data/", senão o basename de arquivos conhecidos do
   engine — globalgamemanagers, level*, sharedassets*, *.assets/.resS/.resource). */
static const char *asset_redirect(const char *p, char *buf, size_t bufsz) {
  if (!p) return NULL;
  /* QUALQUER path .../AssetBundles/<nome> -> /storage/cuphead-sa/AssetBundles/<nome>.
     Resolve o load do DLC (base-path vinha lixo: "Шестигранные врата 1/AssetBundles/..")
     e qualquer base estranha; o path correto redireciona p/ si mesmo (anti-loop). */
  const char *ab = strstr(p, "/AssetBundles/");
  /* path RELATIVO "AssetBundles/<nome>" (cutscene do livro monta sem base) */
  if (!ab && !strncmp(p, "AssetBundles/", 13)) ab = p - 1;
  if (ab) {
    const char *sap = getenv("CUP_SAPATH"); if (!sap) sap = "/storage/cuphead-sa";
    snprintf(buf, bufsz, "%s/AssetBundles/%s", sap, ab + 14);
    if (strcmp(buf, p) != 0 && access(buf, F_OK) == 0) return buf;
    return NULL;
  }
  /* anti-loop: só pula o que JÁ aponta pro alvo (bin/Data real); paths de
     userdata/ sob a base ainda precisam de redirect (il2cpp/Metadata) */
  if (!strncmp(p, ASSET_BASE_M "bin/Data/", sizeof(ASSET_BASE_M "bin/Data/") - 1)) return NULL;
  const char *sub = strstr(p, "bin/Data/");
  if (sub) {
    snprintf(buf, bufsz, ASSET_BASE_M "bin/Data/%s", sub + 9);
    if (access(buf, F_OK) == 0) return buf;
  }
  const char *base = strrchr(p, '/'); base = base ? base + 1 : p;
  if (!strcmp(base, "global-metadata.dat")) {
    snprintf(buf, bufsz, ASSET_BASE_M "bin/Data/Managed/Metadata/global-metadata.dat");
    return buf;
  }
  /* il2cpp procura <userdata>/il2cpp/Resources/*-resources.dat */
  if (strstr(base, "-resources.dat")) {
    snprintf(buf, bufsz, ASSET_BASE_M "bin/Data/Managed/Resources/%s", base);
    if (access(buf, F_OK) == 0) return buf;
  }
  if (!strncmp(base, "level", 5) || !strncmp(base, "sharedassets", 12) ||
      !strncmp(base, "globalgamemanagers", 18) || strstr(base, ".assets") ||
      strstr(base, ".resS") || strstr(base, ".resource") ||
      !strcmp(base, "data.unity3d") || !strcmp(base, "boot.config") ||
      !strcmp(base, "unity default resources") || !strcmp(base, "unity_builtin_extra")) {
    snprintf(buf, bufsz, ASSET_BASE_M "bin/Data/%s", base);
    if (access(buf, F_OK) == 0) return buf;
    snprintf(buf, bufsz, ASSET_BASE_M "bin/Data/Resources/%s", base);
    if (access(buf, F_OK) == 0) return buf;
  }
  return NULL;
}
/* command line do Unity: lido de /proc/<pid>/cmdline (args separados por \0).
   Injeta -force-gfx-st (single-threaded GFX) p/ matar o GfxDeviceWorker e o
   deadlock main<->worker no boot. CUP_GFXARGS sobrescreve. */
static int cmdline_fd(void) {
  const char *extra = getenv("CUP_GFXARGS");
  char buf[256]; int n = 0;
  n += sprintf(buf + n, "cuphead") + 1;
  if (extra && *extra) {
    /* CUP_GFXARGS="-a -b" -> cada token \0-terminado */
    char tmp[200]; strncpy(tmp, extra, sizeof tmp - 1); tmp[sizeof tmp - 1] = 0;
    for (char *t = strtok(tmp, " "); t; t = strtok(NULL, " ")) n += sprintf(buf + n, "%s", t) + 1;
  } else {
    n += sprintf(buf + n, "-force-gfx-st") + 1;
    n += sprintf(buf + n, "-force-gles20") + 1;
  }
  FILE *t = tmpfile();
  if (!t) return -1;
  fwrite(buf, 1, n, t); fflush(t);
  int fd = dup(fileno(t)); fclose(t); lseek(fd, 0, SEEK_SET);
  fprintf(stderr, "[CMDLINE] injetado (%d bytes): force-gfx-st\n", n);
  return fd;
}
static int my_open(const char *p, int fl, ...) {
  if (p && !strcmp(p, "/proc/cpuinfo")) {
    int nc = getenv("CUP_1CORE") ? 1 : 4;
    FILE *t = tmpfile();
    if (t) { for (int i = 0; i < nc; i++) fprintf(t, "processor\t: %d\nCPU implementer\t: 0x41\nCPU architecture: 8\n\n", i);
      fflush(t); int fd = dup(fileno(t)); fclose(t); lseek(fd, 0, SEEK_SET); return fd; }
  }
  if (p && strstr(p, "cmdline") && !getenv("CUP_NOGFXARGS")) return cmdline_fd();
  char rb[512];
  const char *r = asset_redirect(p, rb, sizeof rb);
  if (r) {
    if (g_dllog) fprintf(stderr, "[open-redir] %s -> %s\n", p, r);
    return open(r, fl);
  }
  va_list ap; va_start(ap, fl); int mo = va_arg(ap, int); va_end(ap);
  int fd = open(p, fl, mo);
  if (g_dllog && p) fprintf(stderr, "[open%s] %s\n", fd < 0 ? "-MISS" : "", p);
  return fd;
}
/* stat/lstat/access com o mesmo redirect — o engine checa existência antes de
   abrir ("No GlobalGameManagers file" pode vir de um stat, não do open).
   Layout de struct stat arm64 = kernel em bionic E glibc → pass-through ok. */
static int my_stat(const char *p, void *st) {
  char rb[512]; const char *r = asset_redirect(p, rb, sizeof rb);
  if (r && g_dllog) fprintf(stderr, "[stat-redir] %s -> %s\n", p, r);
  int rc = stat(r ? r : p, (struct stat *)st);
  if (g_dllog && rc < 0 && p) fprintf(stderr, "[stat-MISS] %s\n", p);
  return rc;
}
static int my_lstat(const char *p, void *st) {
  char rb[512]; const char *r = asset_redirect(p, rb, sizeof rb);
  return lstat(r ? r : p, (struct stat *)st);
}
static int my_access(const char *p, int m) {
  char rb[512]; const char *r = asset_redirect(p, rb, sizeof rb);
  if (r && g_dllog) fprintf(stderr, "[access-redir] %s -> %s\n", p, r);
  return access(r ? r : p, m);
}
/* exit() do jogo: loga QUEM chamou (lr) + stack antes de morrer — a morte
   silenciosa pos-FMOD não deixava rastro. */
static void my_exit(int code) {
  fprintf(stderr, "[EXIT] exit(%d) chamado! lr=%p\n", code, __builtin_return_address(0));
  uintptr_t tb = (uintptr_t)g_unity_base;
  uintptr_t lr = (uintptr_t)__builtin_return_address(0);
  if (tb && lr >= tb) fprintf(stderr, "[EXIT] (libunity+0x%lx)\n", lr - tb);
  fsync(2);
  _exit(code);
}
/* __system_property_get: FMOD checa ro.build.version.sdk antes de usar OpenSLES
   (vazio→SDK 0→desiste sem nem dar dlsym; receita Dysmantle: "25"). Resto vazio. */
static int my_sysprop(const char *name, char *value) {
  if (!value) return 0;
  if (name && strstr(name, "version.sdk")) { strcpy(value, "25"); return 2; }
  value[0] = 0; return 0;
}
/* __android_log -> stderr */
static int my_alog_print(int prio, const char *tag, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap); return 0;
}
static int my_alog_write(int prio, const char *tag, const char *msg) {
  fprintf(stderr, "[ALOG:%d %s] %s\n", prio, tag ? tag : "?", msg ? msg : ""); return 0;
}
/* __android_log_vprint é o canal do PLAYER LOG do Unity — jamais stubar */
static int my_alog_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
  fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); return 0;
}
/* ANativeWindow: Unity espera window !=NULL (nativeRecreateGfxState) senão trava p/ sempre.
   Os egl* do libunity são imports PLT que resolvem no libEGL REAL do Mali (dlopen GLOBAL);
   no fbdev a EGLNativeWindowType é só struct {u16 w, u16 h} → entrega uma DE VERDADE e o
   Unity cria a window surface direto no fb0 (sem shim). CUP_SHIMEGL=1 volta pro fake int. */
static struct { unsigned short w, h; } g_fbdev_win = {1280, 720};
static int g_anw = 0xA11;
static void *my_aw_fromSurface(void *e, void *s) { (void)e; (void)s;
  return getenv("CUP_SHIMEGL") ? (void *)&g_anw : (void *)&g_fbdev_win; }
static int my_aw_setgeom(void *w, int a, int b, int c) { (void)w; (void)a; (void)b; (void)c; return 0; }
static int my_aw_getWidth(void *w) { (void)w; return 1280; }
static int my_aw_getHeight(void *w) { (void)w; return 720; }
static int my_aw_getFormat(void *w) { (void)w; return 1; }
static void my_aw_noop(void *w) { (void)w; }
/* dlopen/dlsym: Unity dlopen libGLESv2/EGL/OpenSLES + dlsym em runtime */
/* ---------- egl_shim (janela GLES2 via SDL2, proven re4) ---------- */
extern void egl_shim_create_window(void);
extern void *egl_shim_GetDisplay(void *);
extern unsigned egl_shim_Initialize(void *, int *, int *);
extern unsigned egl_shim_Terminate(void *);
extern unsigned egl_shim_ChooseConfig(void *, const int *, void **, int, int *);
extern void *egl_shim_CreateWindowSurface(void *, void *, void *, const int *);
extern void *egl_shim_CreatePbufferSurface(void *, void *, const int *);
extern void *egl_shim_CreateContext(void *, void *, void *, const int *);
extern unsigned egl_shim_MakeCurrent(void *, void *, void *, void *);
extern unsigned egl_shim_SwapBuffers(void *, void *);
extern unsigned egl_shim_DestroySurface(void *, void *);
extern unsigned egl_shim_DestroyContext(void *, void *);
extern unsigned egl_shim_QuerySurface(void *, void *, int, int *);
extern unsigned egl_shim_GetConfigAttrib(void *, void *, int, int *);
extern int egl_shim_GetError(void);
extern void *egl_shim_GetProcAddress(const char *);
extern unsigned egl_shim_BindAPI(unsigned);
extern const char *egl_shim_QueryString(void *, int);
extern unsigned egl_shim_SwapInterval(void *, int);
extern void *egl_shim_GetCurrentContext(void);
extern void *egl_shim_GetCurrentSurface(int);
extern unsigned egl_shim_SurfaceAttrib(void *, void *, int, int);
static void *egl_route(const char *nm) {
  struct { const char *n; void *f; } m[] = {
    {"eglGetDisplay", egl_shim_GetDisplay}, {"eglInitialize", egl_shim_Initialize},
    {"eglTerminate", egl_shim_Terminate}, {"eglChooseConfig", egl_shim_ChooseConfig},
    {"eglCreateWindowSurface", egl_shim_CreateWindowSurface},
    {"eglCreatePbufferSurface", egl_shim_CreatePbufferSurface},
    {"eglCreateContext", egl_shim_CreateContext}, {"eglMakeCurrent", egl_shim_MakeCurrent},
    {"eglSwapBuffers", egl_shim_SwapBuffers}, {"eglDestroySurface", egl_shim_DestroySurface},
    {"eglDestroyContext", egl_shim_DestroyContext}, {"eglQuerySurface", egl_shim_QuerySurface},
    {"eglGetConfigAttrib", egl_shim_GetConfigAttrib}, {"eglGetError", egl_shim_GetError},
    {"eglGetProcAddress", egl_shim_GetProcAddress}, {"eglBindAPI", egl_shim_BindAPI},
    {"eglQueryString", egl_shim_QueryString}, {"eglSwapInterval", egl_shim_SwapInterval},
    {"eglGetCurrentContext", egl_shim_GetCurrentContext},
    {"eglGetCurrentSurface", egl_shim_GetCurrentSurface},
    {"eglGetCurrentDisplay", egl_shim_GetDisplay}, {"eglSurfaceAttrib", egl_shim_SurfaceAttrib},
    {0, 0}
  };
  for (int i = 0; m[i].n; i++) if (!strcmp(m[i].n, nm)) return m[i].f;
  return NULL;
}

/* glGetString wrapper (proven re4): o preprocessador de shader do Unity chama
 * glGetString(RENDERER/VERSION/EXT) numa thread sem contexto GL current -> real
 * devolve NULL -> parse char-a-char de NULL estoura o buffer (stack smash em
 * nativeRecreateGfxState). Cache + defaults Mali; NUNCA NULL. */
static const unsigned char *(*r_glGetString)(unsigned) = NULL;
static const unsigned char *g_glcache[5] = {0,0,0,0,0};
static int glstr_idx(unsigned n){ switch(n){case 0x1F00:return 0;case 0x1F01:return 1;case 0x1F02:return 2;case 0x1F03:return 3;case 0x8B8C:return 4;} return -1; }
/* GL_EXTENSIONS curado curto: a string real do Mali-450 é longa e o parser do
 * Unity pode estourar um buffer fixo (stack smash em nativeRecreateGfxState). */
static const char *GL_EXT_SHORT =
  "GL_OES_depth24 GL_OES_element_index_uint GL_OES_texture_npot "
  "GL_OES_rgb8_rgba8 GL_OES_packed_depth_stencil GL_OES_vertex_array_object "
  "GL_EXT_texture_format_BGRA8888 GL_OES_standard_derivatives";
static const unsigned char *my_glGetString(unsigned n){
  if(n==0x1F03) return (const unsigned char*)GL_EXT_SHORT;   /* GL_EXTENSIONS curto */
  if(!r_glGetString) r_glGetString=(const unsigned char*(*)(unsigned))dlsym(RTLD_DEFAULT,"glGetString");
  const unsigned char *s = r_glGetString ? r_glGetString(n) : NULL;
  int i = glstr_idx(n);
  if(s){ if(i>=0 && !g_glcache[i]) g_glcache[i]=(const unsigned char*)strdup((const char*)s); }
  else if(i>=0 && g_glcache[i]) s=g_glcache[i];
  else if(i>=0) s=(const unsigned char*)(n==0x1F00?"ARM":n==0x1F01?"Mali-450 MP":n==0x1F02?"OpenGL ES 2.0":n==0x8B8C?"OpenGL ES GLSL ES 1.00":"");
  return s;
}

/* ---- wrappers GL de shader (diagnóstico: shader falha/trava no Mali?) ---- */
static void (*r_glCompileShader)(unsigned);
static void (*r_glGetShaderiv)(unsigned, unsigned, int *);
static void (*r_glLinkProgram)(unsigned);
static void (*r_glGetProgramiv)(unsigned, unsigned, int *);
static void (*r_glGetShaderInfoLog)(unsigned, int, int *, char *);
static int g_shN, g_prN;
static void my_glCompileShader(unsigned sh) {
  if (!r_glCompileShader) r_glCompileShader = dlsym(RTLD_DEFAULT, "glCompileShader");
  if (!r_glGetShaderiv) r_glGetShaderiv = dlsym(RTLD_DEFAULT, "glGetShaderiv");
  if (!r_glGetShaderInfoLog) r_glGetShaderInfoLog = dlsym(RTLD_DEFAULT, "glGetShaderInfoLog");
  r_glCompileShader(sh);
  int st = -1; if (r_glGetShaderiv) r_glGetShaderiv(sh, 0x8B81, &st); /* COMPILE_STATUS */
  if (st != 1 && g_shN < 100) {
    char log[768] = {0}; if (r_glGetShaderInfoLog) r_glGetShaderInfoLog(sh, sizeof log - 1, NULL, log);
    fprintf(stderr, "[SHADER] compile FALHOU sh=%u status=%d LOG=%s\n", sh, st, log); dbg_sync();
    g_shN++;
  }
}
static void my_glLinkProgram(unsigned pr) {
  if (!r_glLinkProgram) r_glLinkProgram = dlsym(RTLD_DEFAULT, "glLinkProgram");
  if (!r_glGetProgramiv) r_glGetProgramiv = dlsym(RTLD_DEFAULT, "glGetProgramiv");
  r_glLinkProgram(pr);
  int st = -1; if (r_glGetProgramiv) r_glGetProgramiv(pr, 0x8B82, &st); /* LINK_STATUS */
  if (st != 1 && g_prN < 100) {
    char log[768] = {0}; if (r_glGetShaderInfoLog) { void (*gpil)(unsigned,int,int*,char*) = dlsym(RTLD_DEFAULT,"glGetProgramInfoLog"); if (gpil) gpil(pr, sizeof log-1, NULL, log); }
    fprintf(stderr, "[SHADER] link FALHOU pr=%u status=%d LOG=%s\n", pr, st, log); dbg_sync(); g_prN++;
  }
}

/* ===== CUP_DRAWSPY: ring dos últimos draws p/ achar o que wedga o Utgard =====
 * O bt do wedge mostra a main presa no frame-builder lock do Mali no draw
 * SEGUINTE ao culpado (o GPU não termina o job já submetido) → registramos os
 * últimos DS_RING draws (programa/textura/FBO/count) num ring; um watchdog
 * detecta o stall (seq parado >6s) e dumpa o ring. ⚠️ sem glGetError/glFinish
 * por draw (glFinish satura o Utgard). Bisseção: CUP_SKIPFBO=1 pula draws com
 * FBO!=0 (render-to-texture); CUP_SKIPPROG=a,b,c pula programas específicos. */
static int g_drawspy = 0;
volatile int g_render_frame = -1;          /* setado no render loop (F2) */
static void (*ds_r_DrawElements)(unsigned, int, unsigned, const void *);
static void (*ds_r_DrawArrays)(unsigned, int, int);
static void (*ds_r_TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
static void (*ds_r_CompTexImage2D)(unsigned, int, unsigned, int, int, int, int, const void *);
static void (*ds_r_GetIntegerv)(unsigned, int *);

#define DS_RING 128
typedef struct {
  unsigned seq; int frame; unsigned char kind, in_progress; /* kind 0=elems 1=arrays */
  unsigned mode, type; int count, prog, tex, fbo, unit, texw, texh;
} ds_rec;
static ds_rec ds_ring[DS_RING];
static volatile unsigned ds_seq = 0;
static volatile unsigned ds_skipped = 0;

#define DS_MAXTEXID 32768
static unsigned short ds_tw[DS_MAXTEXID], ds_th[DS_MAXTEXID];

static int g_skipfbo = 0;
static int g_skipprog[8], g_nskipprog = 0;

static int ds_geti(unsigned pname) {
  int v = 0;
  if (!ds_r_GetIntegerv) ds_r_GetIntegerv = dlsym(RTLD_DEFAULT, "glGetIntegerv");
  if (ds_r_GetIntegerv) ds_r_GetIntegerv(pname, &v);
  return v;
}
static ds_rec *ds_enter(int kind, unsigned mode, int count, unsigned type) {
  unsigned s = __atomic_fetch_add(&ds_seq, 1, __ATOMIC_RELAXED);
  ds_rec *r = &ds_ring[s % DS_RING];
  r->in_progress = 0; r->seq = s; r->frame = g_render_frame;
  r->kind = (unsigned char)kind; r->mode = mode; r->count = count; r->type = type;
  r->prog = ds_geti(0x8B8D);          /* GL_CURRENT_PROGRAM */
  r->unit = ds_geti(0x84E0) - 0x84C0; /* GL_ACTIVE_TEXTURE - GL_TEXTURE0 */
  r->tex  = ds_geti(0x8069);          /* GL_TEXTURE_BINDING_2D */
  r->fbo  = ds_geti(0x8CA6);          /* GL_FRAMEBUFFER_BINDING */
  r->texw = (r->tex > 0 && r->tex < DS_MAXTEXID) ? ds_tw[r->tex] : 0;
  r->texh = (r->tex > 0 && r->tex < DS_MAXTEXID) ? ds_th[r->tex] : 0;
  r->in_progress = 1;
  return r;
}
static int ds_skip(const ds_rec *r) {
  if (g_skipfbo && r->fbo != 0) return 1;
  for (int i = 0; i < g_nskipprog; i++) if (r->prog == g_skipprog[i]) return 1;
  return 0;
}
static void my_glDrawElements(unsigned mode, int count, unsigned type, const void *idx) {
  ds_rec *r = ds_enter(0, mode, count, type);
  if (r->seq < 8) fprintf(stderr, "[DS] draw#%u f=%d ELM cnt=%d prog=%d fbo=%d tex=%d(%dx%d)\n",
                          r->seq, r->frame, count, r->prog, r->fbo, r->tex, r->texw, r->texh);
  if (ds_skip(r)) { r->in_progress = 0; ds_skipped++; return; }
  ds_r_DrawElements(mode, count, type, idx);
  r->in_progress = 0;
}
static void my_glDrawArrays(unsigned mode, int first, int count) {
  ds_rec *r = ds_enter(1, mode, count, 0);
  if (ds_skip(r)) { r->in_progress = 0; ds_skipped++; return; }
  ds_r_DrawArrays(mode, first, count);
  r->in_progress = 0;
}
static void ds_rectex(int w, int h, const char *what) {
  int t = ds_geti(0x8069);
  if (t > 0 && t < DS_MAXTEXID) {
    if ((unsigned short)w > ds_tw[t]) ds_tw[t] = (unsigned short)w;
    if ((unsigned short)h > ds_th[t]) ds_th[t] = (unsigned short)h;
  }
  if (w > 2048 || h > 2048) { fprintf(stderr, "[DS] BIG TEX %s id=%d %dx%d\n", what, t, w, h); fsync(2); }
}
/* CUP_TEXHALF=N: downscale (nearest) das texturas nivel-0 não-comprimidas até
 * caberem no teto N (max(w,h) <= N). N=512 → 2048 vira 512 (1/16 da RAM/VRAM!).
 * Reduz drasticamente p/ o load de assets persistentes caber em 832MB + evita o
 * limite do Utgard. Receita Bully/Castlevania (agressiva). px!=NULL só. */
static int g_texhalf = 0;
static int gl_bpp(unsigned fmt, unsigned type) {
  switch (type) {
    case 0x8033: case 0x8034: case 0x8363: return 2;  /* 4444 / 5551 / 565 */
    case 0x1401:                                       /* UNSIGNED_BYTE */
      switch (fmt) { case 0x1908: return 4;            /* RGBA */
                     case 0x1907: return 3;            /* RGB */
                     case 0x190A: return 2;            /* LUMINANCE_ALPHA */
                     case 0x1909: case 0x1906: return 1; } /* LUMINANCE / ALPHA */
  }
  return 0;  /* desconhecido -> não mexe */
}
static unsigned char ds_shift[DS_MAXTEXID];  /* fator de downscale (log2) por tex id */
static void my_glTexImage2D(unsigned tgt, int lvl, int ifmt, int w, int h, int b, unsigned fmt, unsigned type, const void *px) {
  if (lvl == 0 && tgt == 0x0DE1) ds_rectex(w, h, "tex");
  int tid = (g_texhalf && tgt == 0x0DE1) ? ds_geti(0x8069) : 0;
  int shift = 0;
  if (g_texhalf && tgt == 0x0DE1 && px && gl_bpp(fmt, type) > 0) {
    if (lvl == 0) {
      int mw = w, mh = h;
      while ((mw > g_texhalf || mh > g_texhalf) && mw > 1 && mh > 1) { mw >>= 1; mh >>= 1; shift++; }
      if (tid > 0 && tid < DS_MAXTEXID) ds_shift[tid] = (unsigned char)shift;
    } else if (tid > 0 && tid < DS_MAXTEXID) {
      shift = ds_shift[tid];                          /* mesma chain do nivel-0 */
      while (shift > 0 && ((w >> shift) < 1 || (h >> shift) < 1)) shift--;
    }
  }
  if (shift > 0) {
    int bpp = gl_bpp(fmt, type);
    int nw = w >> shift, nh = h >> shift, st = 1 << shift;
    unsigned char *dst = malloc((size_t)nw * nh * bpp);
    if (dst) {
      const unsigned char *src = px;
      for (int y = 0; y < nh; y++) {
        const unsigned char *srow = src + (size_t)(y * st) * w * bpp;
        unsigned char *drow = dst + (size_t)y * nw * bpp;
        for (int x = 0; x < nw; x++)
          memcpy(drow + x * bpp, srow + (size_t)(x * st) * bpp, bpp);
      }
      static int n; if (n++ < 40) { fprintf(stderr, "[TEXHALF] tex=%d %dx%d -> %dx%d (/%d lvl%d)\n", tid, w, h, nw, nh, st, lvl); fsync(2); }
      ds_r_TexImage2D(tgt, lvl, ifmt, nw, nh, b, fmt, type, dst);
      free(dst);
      return;
    }
  }
  ds_r_TexImage2D(tgt, lvl, ifmt, w, h, b, fmt, type, px);
}
static void my_glCompTexImage2D(unsigned tgt, int lvl, unsigned ifmt, int w, int h, int b, int sz, const void *px) {
  if (lvl == 0 && tgt == 0x0DE1) ds_rectex(w, h, "ctex");
  ds_r_CompTexImage2D(tgt, lvl, ifmt, w, h, b, sz, px);
}
static void ds_dump(void) {
  unsigned end = ds_seq;
  unsigned n = end < DS_RING ? end : DS_RING;
  fprintf(stderr, "[DS] ===== DUMP ring (seq=%u frame=%d skipped=%u) =====\n", end, g_render_frame, ds_skipped);
  for (unsigned i = end - n; i != end; i++) {
    ds_rec *r = &ds_ring[i % DS_RING];
    if (r->seq != i) continue;  /* slot já sobrescrito (race benigna) */
    fprintf(stderr, "[DS] #%u f=%d %s mode=%u cnt=%d prog=%d fbo=%d u%d tex=%d(%dx%d)%s\n",
            r->seq, r->frame, r->kind ? "ARR" : "ELM", r->mode, r->count,
            r->prog, r->fbo, r->unit, r->tex, r->texw, r->texh,
            r->in_progress ? "  <== IN-PROGRESS (bloqueado no driver)" : "");
  }
  fsync(2);
}
static void *ds_watchdog(void *a) {
  (void)a;
  unsigned last = 0; int still = 0, dumped = 0, beat = 0;
  for (;;) {
    sleep(2);
    unsigned s = ds_seq;
    if (++beat % 30 == 0) { fprintf(stderr, "[DS] alive seq=%u f=%d skipped=%u\n", s, g_render_frame, ds_skipped); fsync(2); }
    if (s != last) { last = s; still = 0; dumped = 0; continue; }
    if (s == 0) continue;
    if (++still >= 3 && !dumped) {
      fprintf(stderr, "[DS] STALL: draws parados ha %ds (seq=%u)\n", still * 2, s);
      ds_dump(); dumped = 1;
    }
  }
  return NULL;
}
static void *ds_route(const char *nm, void *real) {
  void *w = real;
  if (!g_drawspy || !nm || !real) return real;
  if (!strcmp(nm, "glDrawElements")) { ds_r_DrawElements = real; w = (void *)my_glDrawElements; }
  else if (!strcmp(nm, "glDrawArrays"))   { ds_r_DrawArrays = real;   w = (void *)my_glDrawArrays; }
  else if (!strcmp(nm, "glTexImage2D"))   { ds_r_TexImage2D = real;   w = (void *)my_glTexImage2D; }
  else if (!strcmp(nm, "glCompressedTexImage2D")) { ds_r_CompTexImage2D = real; w = (void *)my_glCompTexImage2D; }
  else if (!strcmp(nm, "glCompileShader")) { r_glCompileShader = real; w = (void *)my_glCompileShader; }
  else if (!strcmp(nm, "glLinkProgram"))   { r_glLinkProgram = real;   w = (void *)my_glLinkProgram; }
  if (w != real) { fprintf(stderr, "[DS] route %s (real=%p)\n", nm, real); fsync(2); }
  return w;
}
static void ds_init(void) {
  if (getenv("CUP_TEXHALF")) { g_texhalf = atoi(getenv("CUP_TEXHALF")); if (g_texhalf < 2) g_texhalf = 1024; }
  if (!getenv("CUP_DRAWSPY") && !g_texhalf) return;
  g_drawspy = 1;  /* liga roteamento de gl* (DRAWSPY e/ou TEXHALF precisam de glTexImage2D) */
  g_skipfbo = getenv("CUP_SKIPFBO") ? 1 : 0;
  const char *sp = getenv("CUP_SKIPPROG");
  if (sp) {
    char buf[128]; strncpy(buf, sp, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    for (char *t = strtok(buf, ","); t && g_nskipprog < 8; t = strtok(NULL, ","))
      g_skipprog[g_nskipprog++] = atoi(t);
  }
  pthread_t th; pthread_create(&th, NULL, ds_watchdog, NULL);
  fprintf(stderr, "[DS] drawspy ON (skipfbo=%d nskipprog=%d)\n", g_skipfbo, g_nskipprog);
}

/* my_eglGetProcAddress: o Unity resolve as funções GL/extensões via
 * eglGetProcAddress (PLT→Mali real). Se uma extensão é ANUNCIADA (glGetString
 * EXTENSIONS) mas a função NÃO resolve (NULL), o Unity guarda um ponteiro
 * inválido e CRASHA ao chamá-lo (fault 0x7f10000004). Loga TODAS as resoluções
 * (com NULL destacado) p/ achar a culpada. CUP_NOVAO força NULL p/ as funções de
 * VAO (testa a hipótese de que GL_OES_vertex_array_object é a culpada). */
static void *(*r_eglGetProcAddress)(const char *);
static unsigned g_egp_n = 0;
static void *my_eglGetProcAddress(const char *nm) {
  if (!r_eglGetProcAddress) r_eglGetProcAddress = dlsym(RTLD_DEFAULT, "eglGetProcAddress");
  void *p = r_eglGetProcAddress ? r_eglGetProcAddress(nm) : NULL;
  if (nm && getenv("CUP_NOVAO") && strstr(nm, "VertexArray")) p = NULL;  /* (vira no-op no Unity) */
  if (g_egp_n++ < 400)
    fprintf(stderr, "[EGP] %s -> %p%s\n", nm ? nm : "(null)", p, p ? "" : "  <== NULL!");
  return ds_route(nm, p);
}

static char g_dl_self, g_dl_il2cpp;
static so_module *g_m_unity = NULL, *g_m_il2cpp = NULL;

/* ---- probe MemoryManager do libunity (RE: GetMemoryManager=0x3cbe2c) ----
 * gMemoryManager (bss)  vaddr 0x1292B48; cursor da arena estatica vaddr 0x11EF4D0;
 * data segment vaddr 0x11e6000. Detecta corrupcao do singleton entre fases. */
static void mm_probe(const char *tag) {
  if (!g_unity_data) return;
  void *mm  = *(void **)(g_unity_data + (0x1292B48 - 0x11e6000));
  void *cur = *(void **)(g_unity_data + (0x11EF4D0 - 0x11e6000));
  fprintf(stderr, "[MM:%s] gMemoryManager=%p cursor-arena=%p\n", tag, mm, cur);
}

/* ---- spy na entrada do operator-new tagueado (vaddr 0x3cbf2c) ----
 * Na entrada: x0=mgr x1=size x2=align(0x10) x3=kind x4=flag x5=tag-string.
 * O canario estoura nesta funcao durante RecreateGfxState -> capturar a chamada
 * culpada (size/kind gigante). Loga so' qdo g_in_gfx setado (evita flood).
 * O hook clobbera 4 insns; o tramp re-executa e segue em entry+16. */
uintptr_t g_gfx_cont = 0;            /* entry+16 (usado pelo asm) */
uintptr_t g_alloc_ub = 0, g_alloc_ib = 0;
volatile int g_in_gfx = 0;
static unsigned g_ospy_n = 0;
void onew_spy_log(uintptr_t mgr, uintptr_t size, uintptr_t kind, uintptr_t tag);
void onew_spy_log(uintptr_t mgr, uintptr_t size, uintptr_t kind, uintptr_t tag) {
  if (!g_in_gfx) return;
  const char *t = "?";
  if (g_alloc_ub && tag >= g_alloc_ub && tag < g_alloc_ub + 0x11e6000)
    t = (const char *)tag;
  fprintf(stderr, "[ONEW] #%u mgr=%lx size=%lu kind=%lu tag=%s\n",
          ++g_ospy_n, mgr, size, kind, t);
  fflush(stderr);
}
__asm__(
  ".text\n"
  ".global onew_spy_tramp\n"
  "onew_spy_tramp:\n"
  "  stp x29, x30, [sp, #-112]!\n"
  "  stp x0, x1, [sp, #16]\n"
  "  stp x2, x3, [sp, #32]\n"
  "  stp x4, x5, [sp, #48]\n"
  "  stp x6, x7, [sp, #64]\n"
  "  str x8, [sp, #80]\n"
  "  mov x0, x0\n"               /* mgr */
  "  mov x2, x3\n"               /* kind */
  "  mov x3, x5\n"               /* tag */
  "  bl onew_spy_log\n"          /* (mgr,size,kind,tag) */
  "  ldr x8, [sp, #80]\n"
  "  ldp x6, x7, [sp, #64]\n"
  "  ldp x4, x5, [sp, #48]\n"
  "  ldp x2, x3, [sp, #32]\n"
  "  ldp x0, x1, [sp, #16]\n"
  "  ldp x29, x30, [sp], #112\n"
  /* prologo original clobberado (0x3cbf2c..0x3cbf38) */
  "  stp x28, x27, [sp, #-96]!\n"
  "  stp x26, x25, [sp, #16]\n"
  "  stp x24, x23, [sp, #32]\n"
  "  stp x22, x21, [sp, #48]\n"
  "  adrp x17, g_gfx_cont\n"
  "  add x17, x17, :lo12:g_gfx_cont\n"
  "  ldr x17, [x17]\n"
  "  br x17\n"
);
extern void onew_spy_tramp(void);

/* ===== CUP_WAITGATE: FORCEINTEG cirúrgico (só durante WaitForAll) =====
 * O FORCEINTEG global (NOP em 0x872774) integra ops cedo demais até FORA do
 * WaitForAll → corrompe um delegate → crash 0x7f10000004 ~60 frames depois.
 * Aqui só ignoramos o gate de budget QUANDO a main está dentro de
 * WaitForAllAsyncOperationsToComplete (0x873a90, force-complete legítimo).
 *
 * (1) hook 0x873a90 → my_waitall (C): liga g_in_waitall, chama o original
 *     (waitall_orig_tramp re-executa o prólogo clobberado e segue em +16),
 *     desliga o flag. (2) hook do gate 0x871844 → my_gate: se in_waitall=1
 *     retorna 1; senão replica a lógica original (budget 0x871884 AND
 *     (jobmgr==null OR NOT pending 0x6cdad0)). */
volatile int g_in_waitall = 0;
uintptr_t g_waitall_cont = 0;   /* 0x873a90 + 16 (usado pelo asm) */
/* gate replica — usa as bases já capturadas (g_unity_base/g_unity_data) */
int my_gate(void *op);
static int jobs_pending(void) {
  void *mgr = *(void **)(g_unity_data + 0xd3380);  /* job-scheduler 0x12b9380 */
  if (!mgr) return 0;
  return ((int (*)(void *))(g_unity_base + 0x6cdad0))(mgr);
}
int g_gatewait = 0;   /* CUP_GATEWAIT: gate sempre bypassa budget + spin-wait nos jobs */
int my_gate(void *op) {
  if (g_gatewait) {
    /* SEMPRE ignora budget (time-slice quebrado no so-loader), mas ESPERA os jobs
       do worker terminarem — spin com sched_yield (dá CPU aos workers) até jobmgr
       limpar. Mata a race da integração forçada (objeto malformado -> crash $PC=9). */
    for (int i = 0; i < 200000 && jobs_pending(); i++) sched_yield();
    return 1;
  }
  if (g_in_waitall) {
    if (getenv("CUP_GATEJOBS")) return jobs_pending() ? 0 : 1;
    return 1;
  }
  int budget = ((int (*)(void *))(g_unity_base + 0x871884))((char *)op + 0x98);
  if (!budget) return 0;
  return jobs_pending() ? 0 : 1;
}
/* trampolim que re-executa o prólogo clobberado de 0x873a90 e segue em +16 */
__asm__(
  ".text\n"
  ".global waitall_orig_tramp\n"
  "waitall_orig_tramp:\n"
  "  stp x22, x21, [sp, #-48]!\n"   /* 0x873a90 */
  "  stp x20, x19, [sp, #16]\n"     /* 0x873a94 */
  "  stp x29, x30, [sp, #32]\n"     /* 0x873a98 */
  "  add x29, sp, #0x20\n"          /* 0x873a9c */
  "  adrp x17, g_waitall_cont\n"
  "  add x17, x17, :lo12:g_waitall_cont\n"
  "  ldr x17, [x17]\n"
  "  br x17\n"
);
extern long waitall_orig_tramp(void *thiz, long a1);
long my_waitall(void *thiz, long a1);
long my_waitall(void *thiz, long a1) {
  g_in_waitall++;
  long r = waitall_orig_tramp(thiz, a1);
  g_in_waitall--;
  return r;
}

/* ===== CUP_CLAMPSIG: clampa o count do Semaphore::Signal (0x65850c) =====
 * Signal(x0=sem, w1=count) posta sem(x0+4) `count` vezes (loop do-while w19=w1).
 * O count deriva p/ um valor enorme (storm/livelock ~frame 110). Hookamos a entrada
 * e clampamos w1 a um máximo são (>nº real de threads ~20) → mata o storm.
 * Prólogo clobberado (4 stp em 0x65850c..0x658518); o tramp re-executa e segue +16. */
uintptr_t g_signal_cont = 0;   /* 0x65850c + 16 */
static int g_signal_clamp = 4096;  /* passa counts legítimos (~dezenas/centenas), só pega o storm */
static volatile unsigned g_signal_clamps = 0;
__asm__(
  ".text\n"
  ".global signal_orig_tramp\n"
  "signal_orig_tramp:\n"
  "  stp x26, x25, [sp, #-80]!\n"   /* 0x65850c */
  "  stp x24, x23, [sp, #16]\n"     /* 0x658510 */
  "  stp x22, x21, [sp, #32]\n"     /* 0x658514 */
  "  stp x20, x19, [sp, #48]\n"     /* 0x658518 */
  "  adrp x17, g_signal_cont\n"
  "  add x17, x17, :lo12:g_signal_cont\n"
  "  ldr x17, [x17]\n"
  "  br x17\n"
);
extern long signal_orig_tramp(void *sem, long count);
long my_signal(void *sem, long count);
long my_signal(void *sem, long count) {
  int c = (int)count;
  if (c > g_signal_clamp) {
    if (g_signal_clamps++ < 12) {
      /* caller = quem chamou Signal (job-scheduler?) */
      uintptr_t ra = (uintptr_t)__builtin_return_address(0);
      const char *lib = "?"; uintptr_t off = ra;
      if (g_unity_base && ra >= g_unity_base && ra < g_unity_base + text_size) { lib = "libunity"; off = ra - g_unity_base; }
      else if (g_il2cpp_base && ra >= g_il2cpp_base && ra < g_il2cpp_base + 0x3000000) { lib = "libil2cpp"; off = ra - g_il2cpp_base; }
      fprintf(stderr, "[CLAMPSIG] Signal(sem=%p) count=%d (0x%x) -> %d  caller=%s+0x%lx\n",
              sem, c, (unsigned)c, g_signal_clamp, lib, (unsigned long)off);
      /* vizinhança do sem (objeto Semaphore/fila de jobs): contador interno + campos */
      uintptr_t b = ((uintptr_t)sem - 0x20) & ~0x7UL;
      for (long d = 0; d < 0x40; d += 8)
        fprintf(stderr, "[CLAMPSIG]   sem%+ld: %016lx\n", d - 0x20, *(unsigned long *)(b + d));
      fsync(2);
    }
    count = (long)g_signal_clamp;
  }
  return signal_orig_tramp(sem, count);
}

/* ===== CUP_CRSPY: espião das coroutines de boot do CupheadStartScene =====
 * O boot (disclaimer) é dirigido por start_cr (RVA il2cpp 0x9A58D0, iterator $PC
 * em +0xBC) que encadeia: settings load → fonts → preload atlases/music →
 * WaitForUserInputBeforeContinue (RVA 0x9A619C, $PC em +0x1C) → load do título.
 * Logamos transições de $PC p/ ver exatamente em qual passo o boot estaciona. */
uintptr_t g_cr1_cont = 0, g_cr2_cont = 0;
__asm__(
  ".text\n"
  ".global cr1_tramp\n"
  "cr1_tramp:\n"
  "  stp x24, x23, [sp, #-64]!\n"    /* 0x9A58D0 */
  "  stp x22, x21, [sp, #16]\n"
  "  stp x20, x19, [sp, #32]\n"
  "  stp x29, x30, [sp, #48]\n"
  "  adrp x17, g_cr1_cont\n"
  "  add x17, x17, :lo12:g_cr1_cont\n"
  "  ldr x17, [x17]\n"
  "  br x17\n"
  ".global cr2_tramp\n"
  "cr2_tramp:\n"
  "  stp x22, x21, [sp, #-48]!\n"    /* 0x9A619C */
  "  stp x20, x19, [sp, #16]\n"
  "  stp x29, x30, [sp, #32]\n"
  "  add x29, sp, #0x20\n"
  "  adrp x17, g_cr2_cont\n"
  "  add x17, x17, :lo12:g_cr2_cont\n"
  "  ldr x17, [x17]\n"
  "  br x17\n"
);
extern long cr1_tramp(void *it);
extern long cr2_tramp(void *it);
long my_start_cr(void *it);
static const char *il2cpp_classname(void *obj) {
  /* obj->klass (off 0) -> klass->name (off 0x10 nesta versão il2cpp 2017) */
  if (!obj) return "(null)";
  void *klass = *(void **)obj;
  if (!klass || ((uintptr_t)klass >> 40)) return "(?)";
  const char *nm = *(const char **)((char *)klass + 0x10);
  return (nm && ((uintptr_t)nm >> 40) == 0) ? nm : "(?)";
}
void *volatile g_startcr_it = NULL;  /* iterator do start_cr capturado (CUP_DRIVECR) */
/* CUP_GATERESTORE: FORCEINTEG (NOP no gate budget 0x871854/0x872774) só é necessário
 * p/ o FontLoader ($PC 7->8). Forçar integração GLOBAL integra ops cujo worker job não
 * rodou -> objeto malformado (vtable/offset uninit) que crasha depois (Cuphead.Init $PC=9
 * na desserialização do CupheadCore: fault = fragmento de string = heap uninit). Restaura
 * o gate ORIGINAL assim que o $PC passa de 8, ANTES do Cuphead.Init. */
static void restore_gate_once(void) {
  static int done = 0;
  if (done) return;
  done = 1;
  uintptr_t a1 = g_unity_base + 0x871854, a2 = g_unity_base + 0x872774;
  long pg = sysconf(_SC_PAGESIZE);
  uintptr_t p1 = a1 & ~(uintptr_t)(pg - 1), p2 = a2 & ~(uintptr_t)(pg - 1);
  mprotect((void *)p1, pg, PROT_READ | PROT_WRITE | PROT_EXEC);
  if (p2 != p1) mprotect((void *)p2, pg, PROT_READ | PROT_WRITE | PROT_EXEC);
  *(uint32_t *)a1 = 0x360000c0u;  /* tbz w0,#0,0x87186c (original) */
  *(uint32_t *)a2 = 0x360004e0u;  /* tbz w0,#0,0x872810 (original) */
  __builtin___clear_cache((char *)a1, (char *)a1 + 4);
  __builtin___clear_cache((char *)a2, (char *)a2 + 4);
  mprotect((void *)p1, pg, PROT_READ | PROT_EXEC);
  if (p2 != p1) mprotect((void *)p2, pg, PROT_READ | PROT_EXEC);
  fprintf(stderr, "[GATERESTORE] gate budget restaurado (0x871854/0x872774) apos FontLoader\n");
  fsync(2);
}
long my_start_cr(void *it) {
  static int lastpc = -99; static unsigned n, samepc;
  g_startcr_it = it;
  int pc = *(int *)((char *)it + 0xBC);
  if (pc != lastpc)
    { fprintf(stderr, "[CRSPY] start_cr tick#%u $PC=%d f=%d\n", n, pc, g_render_frame); fsync(2); samepc = 0; }
  else if (++samepc <= 4) {
    void *cur = *(void **)((char *)it + 0xB0);
    fprintf(stderr, "[CRSPY] start_cr RE-ENTER $PC=%d (samepc=%u) $cur=%p cls=%s f=%d\n",
            pc, samepc, cur, il2cpp_classname(cur), g_render_frame); fsync(2);
  }
  else if (samepc % 180 == 0) {
    void *cur = *(void **)((char *)it + 0xB0);  /* $current (objeto yieldado) */
    fprintf(stderr, "[CRSPY] start_cr SPIN $PC=%d x%u $current=%p cls=%s f=%d\n",
            pc, samepc, cur, il2cpp_classname(cur), g_render_frame); fsync(2);
  }
  lastpc = pc; n++;
  long r = cr1_tramp(it);
  int pc2 = *(int *)((char *)it + 0xBC);
  void *cur = *(void **)((char *)it + 0xB0);
  if (pc2 != pc) {
    fprintf(stderr, "[CRSPY] start_cr $PC %d -> %d (ret=%ld $cur=%p cls=%s f=%d)\n",
            pc, pc2, r, cur, il2cpp_classname(cur), g_render_frame);
    fsync(2); lastpc = pc2;
    if (pc2 >= 9 && getenv("CUP_GATERESTORE")) restore_gate_once();
  } else if (samepc <= 4) {
    fprintf(stderr, "[CRSPY] start_cr POST $PC=%d ret=%ld $cur=%p cls=%s f=%d\n",
            pc, r, cur, il2cpp_classname(cur), g_render_frame); fsync(2);
  }
  return r;
}
long my_inputwait_cr(void *it);
long my_inputwait_cr(void *it) {
  static int lastpc = -99; static unsigned n;
  int pc = *(int *)((char *)it + 0x1C);
  if (pc != lastpc)
    { fprintf(stderr, "[CRSPY] inputwait tick#%u $PC=%d f=%d\n", n, pc, g_render_frame); fsync(2); }
  lastpc = pc; n++;
  long r = cr2_tramp(it);
  int pc2 = *(int *)((char *)it + 0x1C);
  if (pc2 != pc) {
    fprintf(stderr, "[CRSPY] inputwait $PC %d -> %d (ret=%ld f=%d)\n", pc, pc2, r, g_render_frame);
    fsync(2); lastpc = pc2;
  }
  return r;
}

/* ===== CUP_BOOTSPY: log de entrada nas funções da cadeia de boot (il2cpp) =====
 * Hooks de log genéricos: trampolim runtime copia as 4 insns clobberadas pelo
 * hook_arm64; stp/add/mov copiam direto, adrp é recomputado (ldr-literal com o
 * endereço absoluto da página). Mostra qual elo da cadeia
 * Start→LoadFromCloud→LoadCloudData→OnLoaded→OnSettingsDataLoaded→start_cr morre. */
static uint32_t *bs_page = NULL; static int bs_used = 0;
static void *mk_tramp(uintptr_t target, const char *name) {
  if (!bs_page) {
    bs_page = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (bs_page == MAP_FAILED) { bs_page = NULL; return NULL; }
  }
  uint32_t *st = bs_page + bs_used;
  uint32_t *p = st;
  const uint32_t *src = (const uint32_t *)target;
  for (int i = 0; i < 4; i++) {
    uint32_t in = src[i];
    if ((in & 0x9F000000u) == 0x90000000u) {            /* adrp rd, page */
      int rd = in & 31;
      long immlo = (in >> 29) & 3, immhi = (in >> 5) & 0x7FFFF;
      long imm = (immhi << 2) | immlo;
      if (imm & (1L << 20)) imm -= (1L << 21);          /* sign extend 21 bits */
      uint64_t page = ((target + i * 4) & ~0xFFFUL) + (imm << 12);
      *p++ = 0x58000040u | rd;                          /* ldr rd, +8 */
      *p++ = 0x14000003u;                               /* b +12 */
      *(uint64_t *)p = page; p += 2;
    } else if ((in & 0x7C000000u) == 0x14000000u || (in & 0xFF000000u) == 0x58000000u ||
               (in & 0x7C000000u) == 0x94000000u || (in & 0xFE000000u) == 0x54000000u) {
      fprintf(stderr, "[BOOTSPY] %s: insn %d não-relocável (%08x) — hook ABORTADO\n", name, i, in);
      return NULL;
    } else {
      *p++ = in;                                        /* stp/add/mov etc: PI, copia */
    }
  }
  *p++ = 0x58000051u;                                   /* ldr x17, #8 */
  *p++ = 0xd61f0220u;                                   /* br x17 */
  *(uint64_t *)p = target + 16; p += 2;
  bs_used += (int)(p - st) + (4 - ((p - st) & 3));      /* avança alinhado */
  __builtin___clear_cache((char *)st, (char *)p);
  return st;
}
#define BS_WRAP(idx, label) \
  static long (*bs_orig_##idx)(long, long, long, long, long, long, long, long); \
  static long bs_hook_##idx(long a, long b, long c, long d, long e, long f, long g, long h) { \
    static unsigned n; \
    if (n++ < 24) { fprintf(stderr, "[BOOTSPY] %s (#%u) x0=%lx x1=%lx f=%d\n", label, n, a, b, g_render_frame); fsync(2); } \
    return bs_orig_##idx(a, b, c, d, e, f, g, h); \
  }
BS_WRAP(0, "CupheadStartScene.Start")
BS_WRAP(1, "CupheadStartScene.OnSettingsDataLoaded")
BS_WRAP(2, "SettingsData.LoadFromCloud")
BS_WRAP(3, "OnlineInterfaceSteam.LoadCloudData")
BS_WRAP(4, "OnlineManager.Init")
BS_WRAP(5, "SettingsData.Save")
BS_WRAP(6, "CupheadStartScene.start_cr(factory)")
BS_WRAP(7, "SettingsData.OnLoadedCloudData")
/* ===== CUP_MASKGUARD (s12): 2º crash do load do mapa =====
 * libunity 0x8f9914 monta arrays de índice (SpriteMask/Tilemap mesh): recebe a
 * CONTAGEM em w0 e escreve em [obj+128][w0-1]. Na cena do mapa, w0 vem LIXO
 * (~0x10000000) -> escrita fora dos limites -> SIGSEGV em 0x8f9b1c. Mesma raiz do
 * SCENEGUARD (objeto da cena do mapa mal-inicializado no so-loader). Clampa a
 * contagem insana p/ 0 (mask vazia) em vez de estourar. */
/* ===== CUP_SCENESKIP (s12): RAIZ da cadeia de crashes do mapa =====
 * A função 0x541c9c processa o tilemap/mesh de um GameObject; resolve a scene via
 * helper 0x8f7c48 = ldp x8,x1,[arg0,#56] (scene = *(void**)(arg0+56)). No mapa, vários
 * GameObjects têm scene NULL (não registrados na cena pelo so-loader) -> a função
 * deref nulls em cascata (0x541cdc, 0x8f9b1c via 0x8f9914, 0x8f9b88, 0x541e54...).
 * Em vez de remendar cada deref (whack-a-mole), PULA a função inteira quando a scene
 * é NULL: o GameObject não monta o mesh (não renderiza), mas nada crasha. Epílogo é
 * void (caller 0x541c2c ignora o retorno). Substitui a abordagem fake-scene do island. */
static long (*scene541_orig)(long, long, long, long, long, long, long, long);
static volatile uint32_t g_sceneskip_hits;
static long scene541_hook(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  void *scene = a0 ? *(void **)((char *)a0 + 56) : NULL;
  if (!scene) {
    if (g_sceneskip_hits < 8) fprintf(stderr, "[SCENESKIP] 0x541c9c scene=NULL -> skip GO (f=%d)\n", g_render_frame);
    g_sceneskip_hits++;
    return 0;
  }
  return scene541_orig(a0, a1, a2, a3, a4, a5, a6, a7);
}
/* ===== CUP_NULLGUARD (s12): 3º crash do load do mapa =====
 * libunity 0x8f9b88 (função de tilemap/mesh, chamada de 0x541dcc) faz
 * `ldr x14,[x0,#24]` SEM null-check; no mapa x0 (arg0) vem NULL (deriva da
 * fake-scene do SCENEGUARD) -> SIGSEGV fault=0x18. Skip quando arg0==NULL. */
static long (*nullfn_orig)(long, long, long, long, long, long, long, long);
static volatile uint32_t g_nullguard_hits;
static long nullfn_hook(long a, long b, long c, long d, long e, long f, long g, long h) {
  if (a == 0) {
    if (g_nullguard_hits < 8) fprintf(stderr, "[NULLGUARD] 0x8f9b88 arg0=NULL -> skip (f=%d)\n", g_render_frame);
    g_nullguard_hits++;
    return 0;
  }
  return nullfn_orig(a, b, c, d, e, f, g, h);
}
static long (*maskfn_orig)(long, long, long, long, long, long, long, long);
static volatile uint32_t g_maskguard_hits;
static long maskfn_hook(long a, long b, long c, long d, long e, long f, long g, long h) {
  uint32_t n = (uint32_t)a;
  /* A função faz `w10 = count-1` e escreve array[count-1] SEM checar count>0:
   *   count==0 -> w10 = 0xffffffff -> store OOB gigante -> SIGSEGV em 0x8f9b1c.
   *   count enorme (lixo) -> idem. No mapa do Cuphead aparece count==0 (mesh/tilemap
   *   vazio no so-loader). Clampa p/ [1, 0x40000]: count=1 -> w10=0 -> array[0]
   *   (slot que a função JÁ escreve incondicionalmente em 0x8f9b14, logo existe). */
  if (n == 0 || n > 0x40000u) {
    if (g_maskguard_hits < 8)
      fprintf(stderr, "[MASKGUARD] count=%u (0x%x) -> 1 (f=%d)\n", n, n, g_render_frame);
    g_maskguard_hits++;
    a = 1;
  }
  return maskfn_orig(a, b, c, d, e, f, g, h);
}
static void bootspy_install(uintptr_t base) {
  struct { uintptr_t rva; void *hook; void **orig; const char *nm; } T[] = {
    {0x9A55CC, (void *)bs_hook_0, (void **)&bs_orig_0, "Start"},
    {0x9A5828, (void *)bs_hook_1, (void **)&bs_orig_1, "OnSettingsDataLoaded"},
    {0xB73C60, (void *)bs_hook_2, (void **)&bs_orig_2, "LoadFromCloud"},
    {0xB2398C, (void *)bs_hook_3, (void **)&bs_orig_3, "LoadCloudData"},
    {0xB23EF0, (void *)bs_hook_4, (void **)&bs_orig_4, "OnlineMgr.Init"},
    {0xB73798, (void *)bs_hook_5, (void **)&bs_orig_5, "Settings.Save"},
    {0x9A5750, (void *)bs_hook_6, (void **)&bs_orig_6, "start_cr fac"},
    {0xB7422C, (void *)bs_hook_7, (void **)&bs_orig_7, "OnLoadedCloud"},
  };
  for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) {
    void *tr = mk_tramp(base + T[i].rva, T[i].nm);
    if (!tr) continue;
    *T[i].orig = tr;
    hook_arm64(base + T[i].rva, (uintptr_t)T[i].hook);
  }
  fprintf(stderr, "[BOOTSPY] %u hooks de boot instalados\n", (unsigned)(sizeof T / sizeof T[0]));
}

/* ===== CUP_MENUSPY: espião do SlotSelectScreen (menu principal pós-título) =====
 * O Update (0xAB4FF0) despacha pelo state(+0x50): se state==0 (InitializeStorage)
 * NÃO faz NADA (ret) — o menu renderiza mas ignora input até o save/storage init
 * completar: OnPlayerDataInitialized(success=true) seta dataStatus(+0x1C8)=1
 * (Received) -> Update inicia allDataLoaded_cr -> SetState(1=MainMenu). Logamos
 * cada elo p/ ver onde a cadeia para no so-loader. */
static long (*ms_orig_update)(void *);
static long ms_hook_update(void *self) {
  static int ls = -1, ld = -1;
  int st = *(int *)((char *)self + 0x50), ds = *(int *)((char *)self + 0x1C8);
  if (st != ls || ds != ld) {
    fprintf(stderr, "[MENUSPY] SlotSelect state=%d dataStatus=%d f=%d\n", st, ds, g_render_frame);
    fsync(2); ls = st; ld = ds;
  }
  return ms_orig_update(self);
}
static long (*ms_orig_setstate)(void *, int);
static long ms_hook_setstate(void *self, int v) {
  fprintf(stderr, "[MENUSPY] SetState(%d) f=%d\n", v, g_render_frame); fsync(2);
  return ms_orig_setstate(self, v);
}
static long (*ms_orig_pdata)(void *, int);
static long ms_hook_pdata(void *self, int ok) {
  fprintf(stderr, "[MENUSPY] OnPlayerDataInitialized(success=%d) f=%d\n", ok & 1, g_render_frame); fsync(2);
  return ms_orig_pdata(self, ok);
}
static long (*ms_orig_sdata)(void *, int);
static long ms_hook_sdata(void *self, int ok) {
  fprintf(stderr, "[MENUSPY] OnSettingsDataLoaded(success=%d) f=%d\n", ok & 1, g_render_frame); fsync(2);
  return ms_orig_sdata(self, ok);
}
static long (*ms_orig_awake)(void *);
static long ms_hook_awake(void *self) {
  fprintf(stderr, "[MENUSPY] SlotSelectScreen.Awake f=%d\n", g_render_frame); fsync(2);
  return ms_orig_awake(self);
}
static void menuspy_install(uintptr_t base) {
  struct { uintptr_t rva; void *hook; void **orig; const char *nm; } T[] = {
    {0xAB4FF0, (void *)ms_hook_update,   (void **)&ms_orig_update,   "SlotSelect.Update"},
    {0xAB670C, (void *)ms_hook_setstate, (void **)&ms_orig_setstate, "SlotSelect.SetState"},
    {0xAB8868, (void *)ms_hook_pdata,    (void **)&ms_orig_pdata,    "OnPlayerDataInitialized"},
    {0xAB89A0, (void *)ms_hook_sdata,    (void **)&ms_orig_sdata,    "OnSettingsDataLoaded"},
    {0xAB4BA4, (void *)ms_hook_awake,    (void **)&ms_orig_awake,    "SlotSelect.Awake"},
  };
  for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) {
    void *tr = mk_tramp(base + T[i].rva, T[i].nm);
    if (!tr) continue;
    *T[i].orig = tr;
    hook_arm64(base + T[i].rva, (uintptr_t)T[i].hook);
  }
  fprintf(stderr, "[MENUSPY] hooks SlotSelectScreen instalados\n"); fsync(2);
}

/* ===== CUP_TAPINPUT: pulsa AnyPlayerInput.GetAnyButtonDown (il2cpp 0xCC2854) =====
 * A coroutine WaitForUserInputBeforeContinue do disclaimer espera
 * WaitUntil(() => AnyPlayerInput.GetAnyButtonDown()). Sem plumbing real de input,
 * fica preso. Hookamos o método: retorna TRUE em janelas periódicas (~3 frames a
 * cada CUP_TAPPERIOD frames) — equivale a um "toque" que destrava o disclaimer e
 * confirma menus, mas sem ficar true p/ sempre (evita auto-navegar descontrolado).
 * CUP_TAPSTART=frame inicial (default 200, dá tempo do disclaimer subir). */
static int g_tap_period = 240, g_tap_start = 200, g_tap_width = 3;
uintptr_t g_tapinput_cont = 0;
__asm__(
  ".text\n"
  ".global tapinput_tramp\n"
  "tapinput_tramp:\n"
  "  stp x28, x27, [sp, #-96]!\n"    /* 0xCC2854 */
  "  stp x26, x25, [sp, #16]\n"      /* 0xCC2858 */
  "  stp x24, x23, [sp, #32]\n"      /* 0xCC285C */
  "  stp x22, x21, [sp, #48]\n"      /* 0xCC2860 */
  "  adrp x17, g_tapinput_cont\n"
  "  add x17, x17, :lo12:g_tapinput_cont\n"
  "  ldr x17, [x17]\n"
  "  br x17\n"
);
extern int tapinput_tramp(void *self);
int my_getanybuttondown(void *self);
int my_getanybuttondown(void *self) {
  int f = g_render_frame;
  if (f >= g_tap_start) {
    int ph = (f - g_tap_start) % g_tap_period;
    if (ph < g_tap_width) {
      static int lastf = -1;
      if (f != lastf) { fprintf(stderr, "[TAPINPUT] pulse f=%d\n", f); fsync(2); lastf = f; }
      return 1;
    }
  }
  return tapinput_tramp(self);
}

/* ===== CUP_SAPATH: override Application.get_streamingAssetsPath (il2cpp 0x17C7C1C) =====
 * No so-loader o getter retorna "jar:file://!" (caminho do APK vazio) -> os
 * AssetBundles do título (AssetBundle.LoadFromFile(streamingAssetsPath+"/AssetBundles/"+n))
 * falham com "Unable to open archive file" -> NullReferenceException mata a coroutine
 * de boot. Apontamos p/ um diretório REAL do filesystem (CUP_SAPATH=/storage/cuphead-sa)
 * onde deployamos os bundles -> LoadFromFile abre o arquivo de verdade.
 * Cria a string il2cpp 1× via il2cpp_string_new (não chama o original). */
static void *g_sa_string = NULL;
void *my_streamingAssetsPath(void);
void *my_streamingAssetsPath(void) {
  if (!g_sa_string && g_il2cpp_base) {
    void *(*isn)(const char *) = (void *(*)(const char *))(g_il2cpp_base + 0x1b62c38); /* il2cpp_string_new */
    const char *p = getenv("CUP_SAPATH"); if (!p) p = "/storage/cuphead-sa";
    g_sa_string = isn(p);
    fprintf(stderr, "[SAPATH] streamingAssetsPath -> \"%s\" (il2cpp str=%p)\n", p, g_sa_string);
    fsync(2);
  }
  return g_sa_string;
}
/* AssetBundleLoader.getBasePath(location) (il2cpp 0x1031C8C): p/ StreamingAssets usa
 * streamingAssetsPath (já overridado), mas p/ DLC (location=1) usa OUTRA fonte que no
 * so-loader retorna string-lixo ("Шестигранные врата 1") → o load de DLC persistente
 * falha. Override: retorna SEMPRE o nosso path real (ignora location). */
void *my_getbasepath(int location);
void *my_getbasepath(int location) {
  (void)location;
  return my_streamingAssetsPath();  /* mesmo dir; loader anexa "/AssetBundles/"+nome */
}

static char g_dl_sl; /* sentinela do handle de libOpenSLES (FMOD → opensles_shim) */
static void *my_dlopen(const char *nm, int flag) {
  if (g_dllog) fprintf(stderr, "[dlopen] \"%s\"\n", nm ? nm : "(null)");
  /* il2cpp: nosso modulo ja' carregado (F1). Casa "il2cpp" em qualquer forma. */
  if (nm && strstr(nm, "il2cpp")) { fprintf(stderr, "[DLOPEN] %s -> il2cpp module\n", nm); return &g_dl_il2cpp; }
  /* FMOD (audio do Unity) faz dlopen("libOpenSLES.so") em runtime. CUP_NOSL=1
     desliga o shim (volta ao estado imagem-OK: FMOD cai no null output). */
  if (nm && strstr(nm, "OpenSLES") && !getenv("CUP_NOSL")) {
    fprintf(stderr, "[DLOPEN] %s -> opensles_shim\n", nm); return &g_dl_sl; }
  if (!nm || !nm[0] || strstr(nm, "libc") || strstr(nm, "libunity") || strstr(nm, "libmain"))
    return &g_dl_self;
  void *h = dlopen(nm, flag); return h ? h : &g_dl_self;
}
static void *my_dlsym(void *h, const char *nm) {
  if (!nm) return NULL;
  if (g_dllog) fprintf(stderr, "[dlsym] h=%p \"%s\"\n", h, nm);
  if (!strcmp(nm, "glGetString")) return (void *)my_glGetString;
  if (g_drawspy && nm[0] == 'g' && nm[1] == 'l') {   /* cobre resolução de gl* via dlsym tb */
    void *p = dlsym(RTLD_DEFAULT, nm);
    void *w = ds_route(nm, p);
    if (w != p) return w;
  }
  if (getenv("CUP_SHLOG")) {
    if (!strcmp(nm, "glCompileShader")) return (void *)my_glCompileShader;
    if (!strcmp(nm, "glLinkProgram")) return (void *)my_glLinkProgram;
  }
  if (nm[0] == 'e' && nm[1] == 'g' && nm[2] == 'l') { void *p = egl_route(nm); if (p) return p; }
  /* AUDIO: dlsym do handle de libOpenSLES -> opensles_shim (slCreateEngine + SL_IID_*
     com as identidades DO SHIM — ele compara ponteiro, receita re4/Dysmantle) */
  if (h == &g_dl_sl) {
    fprintf(stderr, "[DLSYM:SL] pede \"%s\"\n", nm);
    if (!strcmp(nm, "slCreateEngine")) return (void *)slCreateEngine_shim;
    if (!strcmp(nm, "SL_IID_ENGINE")) return (void *)&sl_IID_ENGINE;
    if (!strcmp(nm, "SL_IID_PLAY")) return (void *)&sl_IID_PLAY;
    if (!strcmp(nm, "SL_IID_VOLUME")) return (void *)&sl_IID_VOLUME;
    if (!strcmp(nm, "SL_IID_BUFFERQUEUE") || !strcmp(nm, "SL_IID_ANDROIDSIMPLEBUFFERQUEUE"))
      return (void *)&sl_IID_BUFFERQUEUE;
    if (!strcmp(nm, "SL_IID_EFFECTSEND")) return (void *)&sl_IID_EFFECTSEND;
    if (!strcmp(nm, "SL_IID_ENGINECAPABILITIES")) return (void *)&sl_IID_ENGINECAPABILITIES;
    if (!strcmp(nm, "SL_IID_ENVIRONMENTALREVERB")) return (void *)&sl_IID_ENVIRONMENTALREVERB;
    fprintf(stderr, "[DLSYM:SL] %s -> NULL\n", nm);
    return NULL;
  }
  /* qualquer simbolo il2cpp_* resolve no modulo il2cpp (qualquer handle) */
  if (!strncmp(nm, "il2cpp", 6) && g_m_il2cpp) {
    so_module *c = so_save(); so_use(g_m_il2cpp);
    void *p = (void *)so_find_addr_safe(nm);
    so_use(c); free(c);
    fprintf(stderr, "[DLSYM:il2cpp*] %s -> %p\n", nm, p);
    return p;
  }
  if (h == &g_dl_il2cpp && g_m_il2cpp) {
    so_module *c = so_save(); so_use(g_m_il2cpp);
    void *p = (void *)so_find_addr_safe(nm);
    so_use(c); free(c);
    fprintf(stderr, "[DLSYM:il2cpp] %s -> %p\n", nm, p);
    return p;
  }
  if (h == &g_dl_self) {
    void *p = (void *)so_find_addr_safe(nm);
    if (!p && g_m_il2cpp) { so_module *c = so_save(); so_use(g_m_il2cpp); p = (void *)so_find_addr_safe(nm); so_use(c); free(c); }
    if (!p) p = dlsym(RTLD_DEFAULT, nm);
    return p;
  }
  return dlsym(h, nm);
}
static const char *my_dlerror(void) { return NULL; }
static int my_dladdr(const void *a, void *i) { (void)a; (void)i; return 0; }
static int my_dlclose(void *h) { (void)h; return 0; }

/* ---------- TLS bridge (bionic keys -> slots nossos; 1 glibc key) ---------- */
#define NSLOT 1024
static pthread_key_t g_tls_base; static int g_tls_init = 0;
static int g_slot_next = 1; static pthread_mutex_t g_slot_mtx = PTHREAD_MUTEX_INITIALIZER;
static void tls_dtor(void *p) { free(p); }
static void **tls_slots(void) {
  if (!g_tls_init) { pthread_key_create(&g_tls_base, tls_dtor); g_tls_init = 1; }
  void **s = (void **)pthread_getspecific(g_tls_base);
  if (!s) { s = (void **)calloc(NSLOT, sizeof(void *)); pthread_setspecific(g_tls_base, s); }
  return s;
}
static int sh_key_create(unsigned *k, void (*d)(void *)) { (void)d; pthread_mutex_lock(&g_slot_mtx);
  int n = g_slot_next++; pthread_mutex_unlock(&g_slot_mtx); if (n >= NSLOT) return 11; *k = (unsigned)n; return 0; }
static int sh_key_delete(unsigned k) { (void)k; return 0; }
static void *sh_getspecific(unsigned k) { if ((int)k <= 0 || (int)k >= NSLOT) return NULL; return tls_slots()[(int)k]; }
static int sh_setspecific(unsigned k, const void *v) { if ((int)k <= 0 || (int)k >= NSLOT) return 22; tls_slots()[(int)k] = (void *)v; return 0; }

/* ---------- abort/raise/tgkill: loga o CALLER (achar a origem do fatal) ---------- */
static void map_caller(const char *tag, uintptr_t ra) {
  if (g_unity_base && ra >= g_unity_base && ra < g_unity_base + 0x2000000)
    fprintf(stderr, "%s caller=libunity+0x%lx\n", tag, ra - g_unity_base);
  else if (g_il2cpp_base && ra >= g_il2cpp_base && ra < g_il2cpp_base + 0x3000000)
    fprintf(stderr, "%s caller=libil2cpp+0x%lx\n", tag, ra - g_il2cpp_base);
  else fprintf(stderr, "%s caller=0x%lx (?)\n", tag, ra);
  fflush(stderr);
}
static int my_raise(int sig) { map_caller("[RAISE]", (uintptr_t)__builtin_return_address(0));
  fprintf(stderr, "[RAISE] sig=%d\n", sig); if (getenv("CUP_NORAISE")) return 0; return raise(sig); }
static void my_abort(void) { map_caller("[ABORT]", (uintptr_t)__builtin_return_address(0));
  if (getenv("CUP_NORAISE")) return; abort(); }
static int my_tgkill(int tgid, int tid, int sig) { map_caller("[TGKILL]", (uintptr_t)__builtin_return_address(0));
  fprintf(stderr, "[TGKILL] sig=%d\n", sig); if (getenv("CUP_NORAISE")) return 0; return syscall(__NR_tgkill, tgid, tid, sig); }

/* __stack_chk_fail: o operator-new tagueado (0x3cbf2c) tem canario; numa chamada
 * do RecreateGfxState ele falha -> abort. Neutraliza p/ diagnosticar (loga caller). */
static int g_scf_n = 0;
static void my_stack_chk_fail(void) {
  uintptr_t ra = (uintptr_t)__builtin_return_address(0);
  if (g_scf_n++ == 0) {
    fprintf(stderr, "\n[SCF] __stack_chk_fail caller=%lx", ra);
    if (g_alloc_ub && ra >= g_alloc_ub && ra < g_alloc_ub + 0x11e6000)
      fprintf(stderr, " (libunity+0x%lx)", ra - g_alloc_ub);
    fprintf(stderr, "\n[SCF] stack scan (callers unity FORA do operator-new):\n");
    uintptr_t sp = (uintptr_t)__builtin_frame_address(0);
    for (int k = 0, hits = 0; k < 800 && hits < 30; k++) {
      uintptr_t v = *(uintptr_t *)(sp + k * 8);
      if (g_alloc_ub && v >= g_alloc_ub && v < g_alloc_ub + 0x11e6000) {
        uintptr_t off = v - g_alloc_ub;
        const char *tag = (off >= 0x3cbe90 && off <= 0x3cc1a0) ? " [op-new]" : " <==";
        fprintf(stderr, "  sp+0x%04x libunity+0x%lx%s\n", k * 8, off, tag);
        hits++;
      } else if (g_alloc_ib && v >= g_alloc_ib && v < g_alloc_ib + 0x2325000) {
        fprintf(stderr, "  sp+0x%04x libil2cpp+0x%lx\n", k * 8, v - g_alloc_ib);
        hits++;
      }
    }
    fflush(stderr);
  }
  /* retorna em vez de abort */
}

/* ---------- _ctype_ (tabela BSD/bionic de classes de caractere) ----------
 * libunity (bionic) importa `_ctype_` — uma `const char*` que aponta p/ uma tabela
 * de 257 bytes; isalpha/isdigit/tolower fazem `_ctype_[(int)c+1] & BITS`. O glibc
 * NÃO exporta `_ctype_` → ficava UNRESOLVED (NULL) → o processamento de string do
 * engine (nomes de asset etc.) fazia `ldr [_ctype_]; ldr [x0]` = NULL deref (crash
 * libunity+0xe449d4 no asset loading). Provemos a tabela (preenchida via glibc) e
 * resolvemos o símbolo. Bits bionic: _U=1 _L=2 _N=4 _S=8 _P=0x10 _C=0x20 _X=0x40 _B=0x80. */
#include <ctype.h>
static unsigned char g_ctype_table[257];
static const unsigned char *g_ctype_ptr = g_ctype_table;  /* _ctype_ aponta p/ a base; idx [c+1] */
static unsigned char g_tolower_table[257], g_toupper_table[257];
static const unsigned char *g_tolower_ptr = g_tolower_table, *g_toupper_ptr = g_toupper_table;
static void ctype_init(void) {
  g_ctype_table[0] = 0;                 /* slot do EOF (c=-1) */
  g_tolower_table[0] = 0; g_toupper_table[0] = 0;
  for (int c = 0; c < 256; c++) {
    unsigned char b = 0;
    if (isupper(c)) b |= 0x01;          /* _U */
    if (islower(c)) b |= 0x02;          /* _L */
    if (isdigit(c)) b |= 0x04;          /* _N */
    if (isspace(c)) b |= 0x08;          /* _S */
    if (ispunct(c)) b |= 0x10;          /* _P */
    if (iscntrl(c)) b |= 0x20;          /* _C */
    if (isxdigit(c) && !isdigit(c)) b |= 0x40; /* _X (só hex-letra; dígitos já têm _N) */
    if (c == ' ')  b |= 0x80;           /* _B (blank imprimível) */
    g_ctype_table[c + 1] = b;
    g_tolower_table[c + 1] = (unsigned char)tolower(c);
    g_toupper_table[c + 1] = (unsigned char)toupper(c);
  }
}
/* resolve _ctype_/_tolower_tab_/_toupper_tab_ na GOT do módulo ATIVO. A GOT guarda
 * o ENDEREÇO da variável-ponteiro (code: ldr [got]→ptr_var; ldr [ptr_var]→tabela). */
static void ctype_resolve(void) {
  so_patch_got("_ctype_", (uintptr_t)&g_ctype_ptr);
  so_patch_got("_tolower_tab_", (uintptr_t)&g_tolower_ptr);
  so_patch_got("_toupper_tab_", (uintptr_t)&g_toupper_ptr);
}

/* ---------- helper: override import na tabela ---------- */
static void set_import(const char *name, void *fn) {
  for (size_t i = 0; i < dynlib_numfunctions; i++)
    if (!strcmp(dynlib_functions[i].symbol, name)) { dynlib_functions[i].func = (uintptr_t)fn; return; }
}

/* patch_got: sobrescreve o slot da GOT DIRETO (apos so_resolve). Necessario p/
 * simbolos que NAO estao em dynlib_functions (NDK: ANativeWindow_*, __android_log_*,
 * ASensor*, ...) — p/ esses set_import e' no-op e ficam UNRESOLVED com GOT lixo. */
static int patch_got(const char *name, void *fn) {
  int n = so_patch_got(name, (uintptr_t)fn);
  if (!n) fprintf(stderr, "[GOT] %s: 0 slots (nao achado)\n", name);
  return n;
}

static void *volatile g_preload_mgr;  /* PreloadManager capturado pelo spy (CUP_PSPY) */

/* ===== CUP_SCENEGUARD (s12): crash casa→mapa =====
 * Instantiate na cena do mapa chega em libunity 0x541c9c (resolve {scene,idx}
 * do GameObject via helper 0x8f7c48 = ldp x8,x1,[GO+0x38]) e deref [scene+24]
 * SEM null-check. GO destruído/fora-de-cena tem scene NULL → SIGSEGV fault=0x18
 * (dump s11: x0=0 x1=0xffffffffff). Guard = ilha de código substituindo o bl;
 * hits contados aqui e logados no render loop. */
static volatile uint32_t g_sceneguard_hits;
static uint64_t sg_fake_scene[8];   /* [3] (+24) -> sg_fake_arr (handle w27=0) */
static uint32_t sg_fake_arr[16];

/* stubs NDK no-op (sensores/looper/profiler google) — devolvem 0/NULL */
static long ndk_stub0(void) { return 0; }

extern int my_sigaction();  /* bionic_shims.c (ABI sigset bionic/glibc) */

/* thread de áudio do FMOD (output AudioTrack Java): o Cuphead registra
   org.fmod.FMODAudioDevice.fmodProcess(ByteBuffer) e espera uma thread Java
   chamá-la em loop p/ o mixer avançar. Sem JVM, replicamos em C: assim que
   fmodProcess existir, chamamos com nosso ByteBuffer a cada ~10ms. Isso destrava
   o boot (a main fica presa no nativeRender esperando o áudio progredir). */
static void *g_fmod_env;
static volatile int g_fmod_run = 1;

/* ---- CUP_PSPY: espião do PreloadManager (muro frame 111) ----
 * RE (libunity, offsets confirmados no disasm):
 *   [mgr+208/+224] = fila de PRELOAD  (array/count) — consumida pela thread
 *                    UnityPreload (entry 0x8736cc): roda op->vt[+80] em bg e
 *                    seta op[72]=1; move a op p/ fila de integração (push 0x873790).
 *   [mgr+240/+256] = fila de INTEGRAÇÃO (array/count) — consumida pela main em
 *                    UpdatePreloadingSingleStep (0x8733a8): chama op->vt[+88]
 *                    (timesliced); o POP ([+256]-- em 0x873500) SÓ acontece se
 *                    vt[+88] retornar true E op[72]==1.
 *   HasPendingOperations (0x8739c4) = lock(mgr+0x78); [+224]||[+256]; unlock.
 *   WaitForAllAsyncOperationsToComplete (0x873a90) gira enquanto pendente.
 * O spy substitui 0x8739c4 por uma réplica C (mesma semântica, mesmos locks
 * 0x8f5d0c/0x8f5d14) que captura o ponteiro do mgr; uma thread despeja as filas
 * (op, vtable, flags, alvos de vt[+80/+88/+112]) p/ identificar a op presa. */
#define UN_HASPEND   0x8739c4
#define UN_MTX_LOCK  0x8f5d0c
#define UN_MTX_UNLK  0x8f5d14
static volatile unsigned long g_haspend_calls;   /* quantas vezes a loading-screen consultou */
static int my_haspending(void *mgr) {
  g_preload_mgr = mgr;
  g_haspend_calls++;
  void (*lk)(void *) = (void (*)(void *))(g_unity_base + UN_MTX_LOCK);
  void (*ul)(void *) = (void (*)(void *))(g_unity_base + UN_MTX_UNLK);
  lk((char *)mgr + 0x78);
  uintptr_t pq = *(uintptr_t *)((char *)mgr + 224);
  uintptr_t iq = *(uintptr_t *)((char *)mgr + 256);
  uintptr_t pq_a = *(uintptr_t *)((char *)mgr + 208);
  uintptr_t iq_a = *(uintptr_t *)((char *)mgr + 240);
  uintptr_t pop = (pq && pq_a) ? ((uintptr_t *)pq_a)[0] : 0;
  uintptr_t iop = (iq && iq_a) ? ((uintptr_t *)iq_a)[0] : 0;
  ul((char *)mgr + 0x78);
  int pending = (pq || iq) ? 1 : 0;
  /* CUP_HASPEND_STALE=N: se as filas ficam IDÊNTICAS (mesmas ops, mesma contagem)
   * por N consultas seguidas, a(s) op(s) estão PRESAS — done72 nunca vira 1 (a thread
   * UnityPreload não processa a op no so-loader; ver 0x873900). O BOOT tolera essa op
   * persistente (não espera HasPendingOperations==0), mas a tela de loading do MAPA
   * espera -> trava eterna. Reportamos "sem pendências" p/ destravar a loading.
   * Só dispara em fila ESTÁVEL (sem progresso): se as ops mudam, é load real -> verdade. */
  static long stale_thr = -1;
  if (stale_thr == -1)
    stale_thr = getenv("CUP_HASPEND_STALE") ? atol(getenv("CUP_HASPEND_STALE")) : 0;
  if (pending && stale_thr > 0) {
    static uintptr_t last_pq, last_iq, last_pop, last_iop;
    static long stall;
    static int logged;
    if (pq == last_pq && iq == last_iq && pop == last_pop && iop == last_iop) {
      if (++stall >= stale_thr) {
        if (!logged) {
          logged = 1;
          fprintf(stderr, "[HASPEND] filas PRESAS %ld consultas (pq=%lu iq=%lu "
                  "pop=%lx iop=%lx) -> reportando SEM pendencias (destrava loading)\n",
                  stall, pq, iq, pop, iop);
          dbg_sync();
        }
        return 0;
      }
    } else {
      stall = 0; logged = 0;
      last_pq = pq; last_iq = iq; last_pop = pop; last_iop = iop;
    }
  }
  return pending;
}
static void pspy_dump_op(const char *qn, unsigned i, uintptr_t op) {
  uintptr_t vt = *(uintptr_t *)op;
  uintptr_t vt_off = (vt >= g_unity_base) ? vt - g_unity_base : vt;
  uintptr_t f80 = *(uintptr_t *)(vt + 80), f88 = *(uintptr_t *)(vt + 88);
  uintptr_t f112 = *(uintptr_t *)(vt + 112);
  fprintf(stderr,
          "[PSPY]  %s[%u] op=%lx vt=u+0x%lx done72=%d w64=%d w68=%d "
          "bg(+80)=u+0x%lx integ(+88)=u+0x%lx q(+112)=u+0x%lx\n",
          qn, i, op, vt_off, *(int *)(op + 72), *(int *)(op + 64),
          *(int *)(op + 68), f80 - g_unity_base, f88 - g_unity_base,
          f112 - g_unity_base);
  /* primeiros 0xC0 bytes da op (estado interno: progress/flags/ponteiros) */
  for (int k = 0; k < 24; k += 4)
    fprintf(stderr, "[PSPY]   +%02x: %016lx %016lx %016lx %016lx\n", k * 8,
            ((uintptr_t *)op)[k], ((uintptr_t *)op)[k + 1],
            ((uintptr_t *)op)[k + 2], ((uintptr_t *)op)[k + 3]);
}
static void *preload_spy_thread(void *arg) {
  (void)arg;
  fprintf(stderr, "[PSPY] thread ativa (2s)\n");
  while (g_fmod_run) {
    sleep(2);
    char *m = (char *)g_preload_mgr;
    if (!m) continue;
    /* leitura SEM lock (racy, mas nunca bloqueia/deadlocka o diagnóstico) */
    uintptr_t pq_a = *(uintptr_t *)(m + 208), pq_n = *(uintptr_t *)(m + 224);
    uintptr_t iq_a = *(uintptr_t *)(m + 240), iq_n = *(uintptr_t *)(m + 256);
    /* job-scheduler global (libunity bss 0x12b9380; o gate da integração só passa
       quando estes 3 contadores estão <=0 — 0x6cdad0). +0x70=jobs / +0x168 / +0x16c */
    void *jm = *(void **)(g_unity_data + 0xd3380);
    if (jm) fprintf(stderr, "[PSPY] jobmgr=%p +70=%d +168=%d +16c=%d\n", jm,
                    *(int *)((char *)jm + 0x70), *(int *)((char *)jm + 0x168),
                    *(int *)((char *)jm + 0x16c));
    fprintf(stderr, "[PSPY] mgr=%p preloadQ=%lu integQ=%lu\n", m, pq_n, iq_n);
    for (unsigned i = 0; i < pq_n && i < 2; i++)
      if (((uintptr_t *)pq_a)[i]) pspy_dump_op("PQ", i, ((uintptr_t *)pq_a)[i]);
    for (unsigned i = 0; i < iq_n && i < 2; i++)
      if (((uintptr_t *)iq_a)[i]) pspy_dump_op("IQ", i, ((uintptr_t *)iq_a)[i]);
    dbg_sync();
  }
  return NULL;
}
/* TESTE CUP_PRELOAD_TICK: posta periodicamente os sems em que threads não-main
   bloqueiam (acorda a UnityPreload p/ processar o item pendente da fila). */
static void *preload_tick_thread(void *arg) {
  (void)arg;
  fprintf(stderr, "[TICK] thread de preload-tick ativa (16ms)\n");
  while (g_fmod_run) { sh_tick_preload(); usleep(16000); }
  return NULL;
}
static void *fmod_audio_thread(void *arg) {
  (void)arg;
  void *fp = NULL;
  while (g_fmod_run && !(fp = jni_find_native("fmodProcess"))) usleep(20000);
  if (!fp) return NULL;
  fprintf(stderr, "[AUDIO] fmodProcess=%p; thread alimentando (10ms)\n", fp);
  static long dev = 0xFAD;            /* this (FMODAudioDevice) fake */
  void *bb = jni_fmod_bytebuffer();
  /* dá um tempo p/ o FMOD terminar o init antes de bater no mixer */
  usleep(300000);
  unsigned long n = 0;
  while (g_fmod_run) {
    int r = ((int (*)(void *, void *, void *))fp)(g_fmod_env, &dev, bb);
    if (n < 3 || n % 500 == 0) { fprintf(stderr, "[AUDIO] fmodProcess #%lu -> %d\n", n, r); dbg_sync(); }
    n++;
    usleep(10000);
  }
  return NULL;
}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  setvbuf(stdout, NULL, _IONBF, 0); setvbuf(stderr, NULL, _IONBF, 0);
  g_main_tid = (int)syscall(SYS_gettid);  /* p/ o sem_shim distinguir main de workers */
  if (getenv("CUP_SEMPOLL")) sh_sem_set_poll(atoi(getenv("CUP_SEMPOLL")));  /* polling do sem_wait */
  { extern void cond_set_poll(int);  /* polling do pthread_cond_wait (lost-wakeup futex) */
    if (getenv("CUP_CONDPOLL")) cond_set_poll(atoi(getenv("CUP_CONDPOLL"))); }

  /* log persistente: stderr -> debug.log (unbuffered + fsync nos marcos =
     sobrevive a hang/power-cycle do device). CUP_NOLOGFILE=1 desativa. */
  if (!getenv("CUP_NOLOGFILE")) {
    int lf = open(ASSET_BASE_M "debug.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (lf >= 0) {
      printf("stderr -> " ASSET_BASE_M "debug.log\n");
      dup2(lf, 2); if (lf != 2) close(lf);
    }
  }

  /* valida que tpidr_el0+0x28 (canary bionic) caiu DENTRO do nosso pad TLS */
  { uintptr_t tp; __asm__ volatile("mrs %0, tpidr_el0" : "=r"(tp));
    uintptr_t slot = tp + 0x28, lo = (uintptr_t)g_bionic_guard_pad;
    fprintf(stderr, "TLS guard: tpidr=0x%lx slot+0x28=0x%lx pad=[0x%lx..0x%lx] %s (val=0x%lx)\n",
            (unsigned long)tp, (unsigned long)slot, (unsigned long)lo,
            (unsigned long)(lo + sizeof(g_bionic_guard_pad)),
            (slot >= lo && slot + 8 <= lo + sizeof(g_bionic_guard_pad))
                ? "DENTRO ok" : "FORA (canary instavel!)",
            *(unsigned long *)slot);
  }
  /* sigaltstack: p/ o handler reportar STACK OVERFLOW (SIGSEGV na guard page →
     sem espaço na pilha normal p/ rodar o handler → morte silenciosa). */
  g_skipbad = getenv("CUP_SKIPBAD") ? 1 : 0;
  /* CUP_GCSIG: Boehm GC suspende threads via SIGPWR(30)/restart SIGXCPU(24) p/
     stop-the-world. Nossas threads (criadas via pthread_create_fake, fora do
     registro do GC) recebem SIGPWR com ação DEFAULT = mata o processo (exit 158).
     Instalamos handlers que implementam o protocolo (suspende+espera restart) p/
     a thread não morrer. (my_sigaction bloqueia o engine de sobrescrever.) */
  if (getenv("CUP_GCSIG")) {
    extern void gc_suspend_handler(int), gc_restart_handler(int);
    struct sigaction sp; memset(&sp, 0, sizeof sp);
    sp.sa_handler = gc_suspend_handler; sigfillset(&sp.sa_mask);
    sigdelset(&sp.sa_mask, SIGXCPU); sigdelset(&sp.sa_mask, SIGSEGV);
    sigaction(SIGPWR, &sp, 0);
    struct sigaction sr; memset(&sr, 0, sizeof sr);
    sr.sa_handler = gc_restart_handler; sigemptyset(&sr.sa_mask);
    sigaction(SIGXCPU, &sr, 0);
    fprintf(stderr, "[GCSIG] handlers SIGPWR(suspend)+SIGXCPU(restart) instalados\n");
  }
  { static char altstk[256 * 1024]; stack_t ss = {0};
    ss.ss_sp = altstk; ss.ss_size = sizeof altstk; ss.ss_flags = 0;
    sigaltstack(&ss, NULL); }
  struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_sigaction = on_crash; sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigaction(SIGSEGV, &sa, 0); sigaction(SIGBUS, &sa, 0); sigaction(SIGABRT, &sa, 0);
  sigaction(SIGILL, &sa, 0); sigaction(SIGFPE, &sa, 0);
  sigaction(SIGTRAP, &sa, 0); sigaction(SIGSYS, &sa, 0);  /* BRK/seccomp matam calado */

  fprintf(stderr, "=== Cuphead Unity 2017.4 IL2CPP (arm64 GLES2) so-loader ===\n");

  /* GL/EGL/z visíveis p/ dlsym(RTLD_DEFAULT) do Unity */
  dlopen("libz.so.1", RTLD_NOW | RTLD_GLOBAL);
  void *g = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_GLOBAL); if (!g) dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
  void *e = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL); if (!e) dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
  fprintf(stderr, "[libs] z/GLESv2/EGL dlopen (glClear=%p)\n", dlsym(RTLD_DEFAULT, "glClear"));

  /* ---- F0: carrega libunity.so ---- */
  size_t hs = (size_t)HEAP_MB * 1024 * 1024;
  void *heap = mmap(NULL, hs, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap == MAP_FAILED) { perror("mmap"); return 1; }
  fprintf(stderr, "[F0] heap %dMB @ %p, carregando libunity.so...\n", HEAP_MB, heap);
  if (so_load("libunity.so", heap, hs) < 0) { fprintf(stderr, "so_load libunity FALHOU\n"); return 1; }
  fprintf(stderr, "[F0] libunity: text=%p+%zu data=%p+%zu\n", text_base, text_size, data_base, data_size);
  g_unity_data = (uintptr_t)data_base;
  if (so_relocate() < 0) { fprintf(stderr, "relocate FALHOU\n"); return 1; }

  /* overrides */
  g_unity_base = (uintptr_t)text_base;
  set_import("abort", (void *)my_abort);
  set_import("raise", (void *)my_raise);
  set_import("tgkill", (void *)my_tgkill);
  set_import("exit", (void *)my_exit);
  set_import("_exit", (void *)my_exit);
  /* __stack_chk_fail nao esta na tabela de imports -> patch direto na GOT (apos resolve) */
  set_import("glGetString", (void *)my_glGetString);
  ds_init();  /* CUP_DRAWSPY: ring de draws + watchdog (intercepta via eglGetProcAddress) */
  if (getenv("CUP_EGPLOG") || getenv("CUP_NOVAO") || g_drawspy)
    set_import("eglGetProcAddress", (void *)my_eglGetProcAddress);
  set_import("sysconf", (void *)my_sysconf);
  g_mmaplog = getenv("CUP_MMAPLOG") ? 1 : 0;
  set_import("mmap", (void *)my_mmap);
  set_import("mmap64", (void *)my_mmap);
  set_import("fopen", (void *)my_fopen);
  set_import("open", (void *)my_open);
  set_import("stat", (void *)my_stat);
  set_import("lstat", (void *)my_lstat);
  set_import("access", (void *)my_access);
  set_import("__system_property_get", (void *)my_sysprop);
  set_import("__android_log_print", (void *)my_alog_print);
  set_import("__android_log_write", (void *)my_alog_write);
  set_import("sigaction", (void *)my_sigaction);
  set_import("dlopen", (void *)my_dlopen);
  set_import("dlsym", (void *)my_dlsym);
  set_import("dlerror", (void *)my_dlerror);
  set_import("dladdr", (void *)my_dladdr);
  set_import("dlclose", (void *)my_dlclose);
  set_import("pthread_key_create", (void *)sh_key_create);
  set_import("pthread_key_delete", (void *)sh_key_delete);
  set_import("pthread_getspecific", (void *)sh_getspecific);
  set_import("pthread_setspecific", (void *)sh_setspecific);
  set_import("ANativeWindow_fromSurface", (void *)my_aw_fromSurface);
  set_import("ANativeWindow_setBuffersGeometry", (void *)my_aw_setgeom);
  set_import("ANativeWindow_getWidth", (void *)my_aw_getWidth);
  set_import("ANativeWindow_getHeight", (void *)my_aw_getHeight);
  set_import("ANativeWindow_getFormat", (void *)my_aw_getFormat);
  set_import("ANativeWindow_acquire", (void *)my_aw_noop);
  set_import("ANativeWindow_release", (void *)my_aw_noop);

  install_sem_shim();  /* semáforos próprios bionic→glibc (fix deadlock boot) */

  fprintf(stderr, "[F0] resolvendo %zu imports...\n", dynlib_numfunctions);
  if (so_resolve(dynlib_functions, dynlib_numfunctions, 0) < 0) { fprintf(stderr, "resolve FALHOU\n"); return 1; }
  ctype_init(); ctype_resolve();   /* _ctype_/_tolower_tab_/_toupper_tab_ (bionic) p/ libunity */
  so_record_phdr("libunity.so");   /* p/ o dl_iterate_phdr custom (unwind de exceções C++) */
  if (so_register_eh_frame() == 0) fprintf(stderr, "[EH] .eh_frame libunity registrado (exceções C++)\n");
  /* PATCH-GOT: os imports NDK nao estao em dynlib_functions -> set_import foi
   * no-op e ficaram UNRESOLVED (GOT lixo). Sobrescreve os slots DIRETO. */
  patch_got("ANativeWindow_fromSurface", (void *)my_aw_fromSurface);
  patch_got("ANativeWindow_setBuffersGeometry", (void *)my_aw_setgeom);
  patch_got("ANativeWindow_getWidth", (void *)my_aw_getWidth);
  patch_got("ANativeWindow_getHeight", (void *)my_aw_getHeight);
  patch_got("ANativeWindow_getFormat", (void *)my_aw_getFormat);
  patch_got("ANativeWindow_acquire", (void *)my_aw_noop);
  patch_got("ANativeWindow_release", (void *)my_aw_noop);
  patch_got("__android_log_print", (void *)my_alog_print);
  patch_got("__android_log_write", (void *)my_alog_write);
  patch_got("__android_log_vprint", (void *)my_alog_vprint);
  if (getenv("CUP_EGPLOG") || getenv("CUP_NOVAO") || g_drawspy)
    patch_got("eglGetProcAddress", (void *)my_eglGetProcAddress);
  /* dl* estavam COMENTADOS em imports.gen.c -> set_import foi no-op e o dlopen@plt
     caiu no glibc REAL (falha ao carregar .so Android). Sem isso o il2cpp nao carrega. */
  patch_got("dlopen", (void *)my_dlopen);
  patch_got("dlsym", (void *)my_dlsym);
  patch_got("dlerror", (void *)my_dlerror);
  patch_got("dlclose", (void *)my_dlclose);
  patch_got("dladdr", (void *)my_dladdr);
  /* engine checa existência dos arquivos de dados antes de abrir */
  patch_got("open", (void *)my_open);
  patch_got("fopen", (void *)my_fopen);
  patch_got("stat", (void *)my_stat);
  patch_got("lstat", (void *)my_lstat);
  patch_got("access", (void *)my_access);
  patch_got("exit", (void *)my_exit);
  patch_got("_exit", (void *)my_exit);
  patch_sem_shim();  /* sem_* nos slots GOT do libunity */
  /* sensores/looper/profiler google: stub no-op (nao usados no path do gfx) */
  const char *ndk_noop[] = {
    "ALooper_forThread","ALooper_prepare","ASensorManager_getInstance",
    "ASensorManager_createEventQueue","ASensorManager_getSensorList",
    "ASensorManager_getDefaultSensor","ASensorManager_destroyEventQueue",
    "ASensorEventQueue_hasEvents","ASensorEventQueue_getEvents",
    "ASensorEventQueue_enableSensor","ASensorEventQueue_disableSensor",
    "ASensorEventQueue_setEventRate","ASensor_getType","ASensor_getResolution",
    "ASensor_getMinDelay","ASensor_getName","ASensor_getVendor",
    "__google_potentially_blocking_region_begin",
    "__google_potentially_blocking_region_end", NULL };
  for (int i = 0; ndk_noop[i]; i++) patch_got(ndk_noop[i], (void *)ndk_stub0);

  /* CUP_FORCEIL2: o helper "load library by name" do Unity (0x357938) faz o
     System.load do il2cpp via JNI -> falha no nosso ambiente ("Failed to load
     Il2CPP"). Mas NOS ja' carregamos libil2cpp.so no F1. Forca retorno 1 (sucesso):
       mov w0,#1 ; ret  */
  if (getenv("CUP_FORCEIL2")) {
    *(uint32_t *)((uintptr_t)text_base + 0x357938) = 0x52800020u; /* mov w0,#1 */
    *(uint32_t *)((uintptr_t)text_base + 0x35793c) = 0xd65f03c0u; /* ret */
    fprintf(stderr, "[FORCEIL2] 0x357938 -> mov w0,#1; ret\n");
  }
  /* CUP_NOEXTRACT: a extracao de recursos do APK (0x94184c) copia de um VFS source
     (o APK) que nao temos -> falha ("Failed to extract resources"). Mas os assets
     JA estao deployados em bin/Data/. Forca a extracao reportar sucesso. */
  if (getenv("CUP_NOEXTRACT")) {
    *(uint32_t *)((uintptr_t)text_base + 0x94184c) = 0x52800020u; /* mov w0,#1 */
    *(uint32_t *)((uintptr_t)text_base + 0x941850) = 0xd65f03c0u; /* ret */
    fprintf(stderr, "[NOEXTRACT] 0x94184c -> mov w0,#1; ret\n");
  }

  so_finalize(); so_flush_caches();
  g_alloc_ub = (uintptr_t)text_base;
  if (getenv("CUP_DLLOG")) g_dllog = 1;

  /* __stack_chk_fail nao esta na tabela -> patch direto no slot da GOT */
  if (getenv("CUP_NOSCF")) {
    extern uintptr_t so_find_rel_addr_safe(const char *);
    uintptr_t got = so_find_rel_addr_safe("__stack_chk_fail");
    if (got) { *(uintptr_t *)got = (uintptr_t)my_stack_chk_fail;
      fprintf(stderr, "[SCF] GOT __stack_chk_fail @ 0x%lx patcheado\n", got); }
    else fprintf(stderr, "[SCF] __stack_chk_fail nao achado na GOT\n");
  }

  /* DIAGNÓSTICO: a main fica presa num loop (libunity 0x873a90) esperando a
     função 0x8739c4 (fila do GfxDevice [+224]/[+256]) zerar — deadlock do
     threaded rendering no Mali. CUP_NOGFXWAIT patcha 0x8739c4 p/ retornar 0
     (não espera) e ver se o jogo avança. */
  if (getenv("CUP_NOGFXWAIT")) {
    extern void so_make_text_writable(void), so_make_text_executable(void);
    so_make_text_writable();
    *(uint32_t *)((uintptr_t)text_base + 0x8739c4) = 0x52800000u; /* mov w0,#0 */
    *(uint32_t *)((uintptr_t)text_base + 0x8739c8) = 0xd65f03c0u; /* ret */
    so_make_text_executable(); so_flush_caches();
    fprintf(stderr, "[NOGFXWAIT] 0x8739c4 -> mov w0,#0; ret\n");
  }

  /* CUP_PSPY: substitui HasPendingOperations (0x8739c4) pela réplica C que
     captura o ponteiro do PreloadManager (diagnóstico do muro frame 111).
     CUP_GCEVERY também precisa do mgr (gate de ociosidade da limpeza). */
  if (getenv("CUP_PSPY") || getenv("CUP_GCEVERY")) {
    extern void so_make_text_writable(void), so_make_text_executable(void);
    so_make_text_writable();
    hook_arm64((uintptr_t)text_base + UN_HASPEND, (uintptr_t)my_haspending);
    so_make_text_executable(); so_flush_caches();
    fprintf(stderr, "[PSPY] hook HasPendingOperations (0x%x) instalado\n", UN_HASPEND);
  }
  /* CUP_FORCEINTEG: dentro de IntegrateOp (0x872758), o gate (0x871844, budget
     time-slice) é checado em 0x872774 `tbz w0,#0, 872810` → se budget recusa,
     integração aborta retornando 0. Em WaitForAll (force-complete) o budget
     DEVERIA ser ignorado. NOP nesse branch: o gate ainda RODA (efeitos colaterais
     do predictor preservados), só o VEREDITO é ignorado → integração prossegue.
     Cirúrgico (só o path de integração; não mexe no gate compartilhado). */
  if (getenv("CUP_FORCEINTEG")) {
    extern void so_make_text_writable(void), so_make_text_executable(void);
    so_make_text_writable();
    /* ⚠️ 0x872774 (IntegrateOp) bypassa o VEREDITO INTEIRO do gate, incl. jobs-pending
       -> integra com job do worker pendente = RACE = objeto malformado (crash $PC=9 na
       desserializacao do CupheadCore). CUP_NO872774 pula este (mantem so o 0x871854 que
       bypassa SO o budget e PRESERVA a espera do job). */
    if (!getenv("CUP_NO872774"))
      *(uint32_t *)((uintptr_t)text_base + 0x872774) = 0xd503201fu; /* NOP (IntegrateOp 0x872758) */
    /* + ops cujo integrate É o gate 0x871844 DIRETO (materiais/shaders): NOP no branch
       0x871854 `tbz w0,#0, 87186c` que aborta no veredito do budget -> cai no check de
       jobs (passa qdo jobmgr=0). Cobre as 52 ops de shader/material presas (sessão 8). */
    if (!getenv("CUP_NOGATE854"))
      *(uint32_t *)((uintptr_t)text_base + 0x871854) = 0xd503201fu; /* NOP */
    so_make_text_executable(); so_flush_caches();
    fprintf(stderr, "[FORCEINTEG] 0x872774 + 0x871854 (gate budget-fail branches) -> NOP\n");
  }
  /* ===== CUP_SCENESKIP (default ON; CUP_NOSCENESKIP desliga) — RAIZ =====
   * Hook na ENTRADA de 0x541c9c: se a scene do GameObject ([arg0+56]) for NULL,
   * pula a função INTEIRA (não monta o mesh) em vez de cascatear nulls. Substitui
   * o antigo island fake-scene (que vazava e crashava downstream em 0x8f9b1c/b88/541e54). */
  if (!getenv("CUP_NOSCENESKIP")) {
    void *trs = mk_tramp((uintptr_t)text_base + 0x541c9c, "scene541");
    if (trs) {
      scene541_orig = (long (*)(long, long, long, long, long, long, long, long))trs;
      extern void so_make_text_writable(void), so_make_text_executable(void);
      so_make_text_writable();
      hook_arm64((uintptr_t)text_base + 0x541c9c, (uintptr_t)scene541_hook);
      so_make_text_executable(); so_flush_caches();
      fprintf(stderr, "[SCENESKIP] hook 0x541c9c (skip GO se scene[arg0+56]==NULL)\n");
    } else {
      fprintf(stderr, "[SCENESKIP] mk_tramp falhou — guard OFF\n");
    }
  }
  /* CUP_MASKGUARD (default ON): clampa contagem insana em 0x8f9914 (mesh de
     SpriteMask/Tilemap do mapa) — 2º crash do load do mapa (0x8f9b1c). */
  if (!getenv("CUP_NOMASKGUARD")) {
    void *tr = mk_tramp((uintptr_t)text_base + 0x8f9914, "maskfn");
    if (tr) {
      maskfn_orig = (long (*)(long, long, long, long, long, long, long, long))tr;
      extern void so_make_text_writable(void), so_make_text_executable(void);
      so_make_text_writable();
      hook_arm64((uintptr_t)text_base + 0x8f9914, (uintptr_t)maskfn_hook);
      so_make_text_executable(); so_flush_caches();
      fprintf(stderr, "[MASKGUARD] hook 0x8f9914 (clamp count [1,0x40000])\n");
    } else {
      fprintf(stderr, "[MASKGUARD] mk_tramp falhou — guard OFF\n");
    }
    /* NULLGUARD: 0x8f9b88 (tilemap, arg0 NULL no mapa) */
    void *trn = mk_tramp((uintptr_t)text_base + 0x8f9b88, "nullfn");
    if (trn) {
      nullfn_orig = (long (*)(long, long, long, long, long, long, long, long))trn;
      extern void so_make_text_writable(void), so_make_text_executable(void);
      so_make_text_writable();
      hook_arm64((uintptr_t)text_base + 0x8f9b88, (uintptr_t)nullfn_hook);
      so_make_text_executable(); so_flush_caches();
      fprintf(stderr, "[NULLGUARD] hook 0x8f9b88 (skip se arg0==NULL)\n");
    }
  }
  /* CUP_WAITGATE: FORCEINTEG cirúrgico — ignora o gate de budget SÓ dentro do
     WaitForAll (0x873a90). Hook do WaitForAll (flag in_waitall) + hook do gate
     (0x871844 → my_gate). NÃO combinar com CUP_FORCEINTEG. */
  if (getenv("CUP_WAITGATE")) {
    extern void so_make_text_writable(void), so_make_text_executable(void);
    g_waitall_cont = (uintptr_t)text_base + 0x873a90 + 16;
    so_make_text_writable();
    hook_arm64((uintptr_t)text_base + 0x873a90, (uintptr_t)my_waitall);
    hook_arm64((uintptr_t)text_base + 0x871844, (uintptr_t)my_gate);
    so_make_text_executable(); so_flush_caches();
    fprintf(stderr, "[WAITGATE] hook WaitForAll(0x873a90)+gate(0x871844); cont=0x%lx\n",
            (unsigned long)g_waitall_cont);
  }
  /* CUP_GATEWAIT: hook do gate (0x871844) SEMPRE-ativo — bypassa o budget (quebrado no
     so-loader) MAS faz spin-wait nos jobs do worker (sched_yield) antes de liberar a
     integração. Mata a race da integração forçada SEM os NOPs (0x872774/0x871854).
     NÃO combinar com CUP_FORCEINTEG. */
  if (getenv("CUP_GATEWAIT")) {
    extern void so_make_text_writable(void), so_make_text_executable(void);
    g_gatewait = 1;
    so_make_text_writable();
    hook_arm64((uintptr_t)text_base + 0x871844, (uintptr_t)my_gate);
    so_make_text_executable(); so_flush_caches();
    fprintf(stderr, "[GATEWAIT] hook gate(0x871844) sempre: bypass budget + spin-wait jobs\n");
  }
  /* CUP_CLAMPSIG: clampa o count de Semaphore::Signal (0x65850c) p/ matar o storm
     (count deriva p/ enorme ~frame 110 → posta bilhões de vezes = livelock). */
  if (getenv("CUP_CLAMPSIG")) {
    extern void so_make_text_writable(void), so_make_text_executable(void);
    if (getenv("CUP_SIGCLAMP")) g_signal_clamp = atoi(getenv("CUP_SIGCLAMP"));
    g_signal_cont = (uintptr_t)text_base + 0x65850c + 16;
    so_make_text_writable();
    hook_arm64((uintptr_t)text_base + 0x65850c, (uintptr_t)my_signal);
    so_make_text_executable(); so_flush_caches();
    fprintf(stderr, "[CLAMPSIG] hook Signal(0x65850c) clamp=%d; cont=0x%lx\n",
            g_signal_clamp, (unsigned long)g_signal_cont);
  }

  fprintf(stderr, "[F0] init_array...\n");
  so_execute_init_array();
  fprintf(stderr, "[F0] libunity init OK\n");
  mm_probe("pos-init_array-unity");

  /* ---- JNI_OnLoad da libunity ---- */
  void *vm = NULL, *env = NULL; jni_shim_init(&vm, &env);
  uintptr_t onload = so_find_addr_safe("JNI_OnLoad");
  if (onload) {
    int ver = ((int (*)(void *, void *))onload)(vm, NULL);
    fprintf(stderr, "[F0] JNI_OnLoad = 0x%x\n", ver);
  } else {
    fprintf(stderr, "[F0] JNI_OnLoad não encontrado em libunity\n");
  }
  fprintf(stderr, "[F0] === libunity OK ===\n");
  mm_probe("pos-JNI_OnLoad");
  dbg_sync();

  /* ---- F1: carrega libil2cpp.so (2º módulo, lógica C# do jogo) ---- */
  g_m_unity = so_save();
  size_t i2s = 96UL * 1024 * 1024;
  void *i2heap = mmap(NULL, i2s, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  g_i2heap_base = (uintptr_t)i2heap; g_i2heap_size = i2s;
  if (i2heap != MAP_FAILED && so_load("libil2cpp.so", i2heap, i2s) >= 0) {
    g_il2cpp_base = (uintptr_t)text_base;
    g_alloc_ib = g_il2cpp_base;
    fprintf(stderr, "[F1] libil2cpp: text=%p+%zu\n", text_base, text_size);
    so_relocate();
    so_resolve(dynlib_functions, dynlib_numfunctions, 0);
    ctype_resolve();   /* _ctype_/_tolower_tab_/_toupper_tab_ p/ libil2cpp tb */
    so_record_phdr("libil2cpp.so");   /* p/ o dl_iterate_phdr custom (unwind) */
    if (so_register_eh_frame() == 0) fprintf(stderr, "[EH] .eh_frame libil2cpp registrado (exceções C++)\n");
    /* il2cpp abre o global-metadata.dat via open() -> intercepta p/ redirecionar.
       patch_got opera no modulo ATIVO (=il2cpp agora). Tb dlopen/dlsym/log. */
    patch_got("open", (void *)my_open);
    patch_got("mmap", (void *)my_mmap);
    patch_got("mmap64", (void *)my_mmap);
    /* sigaction do libil2cpp (o GC instala SIGPWR/SIGXCPU por aqui). Sem patch, o GC
       instalava um handler CORROMPIDO (0x7f10000004) p/ SIGPWR -> stop-the-world
       crashava. Com my_sigaction + CUP_GCSIG, bloqueamos -> nosso handler válido fica. */
    { extern int my_sigaction(); patch_got("sigaction", (void *)my_sigaction); }
    patch_got("fopen", (void *)my_fopen);
    patch_got("stat", (void *)my_stat);
    patch_got("lstat", (void *)my_lstat);
    patch_got("access", (void *)my_access);
    patch_got("dlopen", (void *)my_dlopen);
    patch_got("dlsym", (void *)my_dlsym);
    patch_got("exit", (void *)my_exit);
    patch_got("_exit", (void *)my_exit);
    patch_got("__android_log_print", (void *)my_alog_print);
    patch_got("__android_log_write", (void *)my_alog_write);
    patch_got("__android_log_vprint", (void *)my_alog_vprint);
    patch_sem_shim();  /* sem_* nos slots GOT do libil2cpp */
    /* CUP_CRSPY: hooks nos MoveNext das coroutines de boot (antes do flush de caches) */
    if (getenv("CUP_CRSPY")) {
      g_cr1_cont = g_il2cpp_base + 0x9A58D0 + 16;
      g_cr2_cont = g_il2cpp_base + 0x9A619C + 16;
      hook_arm64(g_il2cpp_base + 0x9A58D0, (uintptr_t)my_start_cr);
      hook_arm64(g_il2cpp_base + 0x9A619C, (uintptr_t)my_inputwait_cr);
      fprintf(stderr, "[CRSPY] hooks start_cr(0x9A58D0 $PC+0xBC) + inputwait(0x9A619C $PC+0x1C)\n");
    }
    if (getenv("CUP_BOOTSPY")) bootspy_install(g_il2cpp_base);
    if (getenv("CUP_MENUSPY")) menuspy_install(g_il2cpp_base);
    /* CUP_FORCESTARTCR: CupheadStartScene.Start (0x9A55CC) faz 3 checks
     * op_Inequality em Application.version/productName/identifier e DÁ EARLY-RETURN
     * (0x9A56F8) antes de StartCoroutine(start_cr) se algum não bate. No so-loader
     * esses getters não retornam o esperado -> start_cr NUNCA roda -> disclaimer
     * congela. Forçamos o caminho de prosseguir: NOP nos 2 tbnz-return + B p/ o
     * bloco-proceed no 3º branch. (start_cr dirige disclaimer->preload->título.) */
    if (getenv("CUP_FORCESTARTCR")) {
      uint32_t *t = (uint32_t *)(g_il2cpp_base + 0x9A567C);
      t[0] = 0xd503201fu;                              /* 0x9A567C tbnz -> NOP (cai = prossegue) */
      *(uint32_t *)(g_il2cpp_base + 0x9A56B8) = 0xd503201fu; /* 0x9A56B8 tbnz -> NOP */
      *(uint32_t *)(g_il2cpp_base + 0x9A56F4) = 0x14000006u; /* 0x9A56F4 tbz -> B 0x9A570C (proceed) */
      __builtin___clear_cache((char *)(g_il2cpp_base + 0x9A567C), (char *)(g_il2cpp_base + 0x9A56F8));
      fprintf(stderr, "[FORCESTARTCR] Start() early-returns NOPados -> StartCoroutine(start_cr) forçado\n");
    }
    /* CUP_NOREFRESHDLC: case 9 do start_cr chama DLCManager.RefreshDLC (0xC91C44) que
       no so-loader crasha (blr p/ delegate de plataforma NULL = método il2cpp não-init)
       -> coroutine de boot quebra no $PC=9. NOP a função inteira (ret) p/ pular. */
    if (getenv("CUP_NOREFRESHDLC")) {
      *(uint32_t *)(g_il2cpp_base + 0xC91C44) = 0xd65f03c0u; /* ret */
      __builtin___clear_cache((char *)(g_il2cpp_base + 0xC91C44), (char *)(g_il2cpp_base + 0xC91C48));
      fprintf(stderr, "[NOREFRESHDLC] DLCManager.RefreshDLC(0xC91C44) -> ret\n");
    }
    if (getenv("CUP_SAPATH") || getenv("CUP_SAPATH_ON")) {
      hook_arm64(g_il2cpp_base + 0x17C7C1C, (uintptr_t)my_streamingAssetsPath);
      /* NÃO hookar getBasePath 0x1031C8C: é stub de 3 insns que JÁ tail-calleia
         get_streamingAssetsPath (hookado); o hook de 16B estoura na função seguinte. */
      fprintf(stderr, "[SAPATH] hook get_streamingAssetsPath(0x17C7C1C)\n");
    }
    if (getenv("CUP_TAPINPUT")) {
      if (getenv("CUP_TAPSTART")) g_tap_start = atoi(getenv("CUP_TAPSTART"));
      if (getenv("CUP_TAPPERIOD")) g_tap_period = atoi(getenv("CUP_TAPPERIOD"));
      g_tapinput_cont = g_il2cpp_base + 0xCC2854 + 16;
      hook_arm64(g_il2cpp_base + 0xCC2854, (uintptr_t)my_getanybuttondown);
      fprintf(stderr, "[TAPINPUT] hook GetAnyButtonDown(0xCC2854) start=%d period=%d\n", g_tap_start, g_tap_period);
    }
    /* CUP_GAMEPAD: controle REAL via USB Gamepad (js0). Substitui Rewired.Player
       .GetButton/Down/Up/GetAxis(string) lendo o estado do js0. (gamepad.c) */
    if (getenv("CUP_GAMEPAD")) {
      extern void gp_init(uintptr_t);
      gp_init(g_il2cpp_base);
    }
    so_finalize(); so_flush_caches();
    fprintf(stderr, "[F1] libil2cpp init_array...\n");
    so_execute_init_array();
    g_m_il2cpp = so_save();
    fprintf(stderr, "[F1] libil2cpp carregado OK\n");
    mm_probe("pos-init_array-il2cpp");
    dbg_sync();
  } else {
    fprintf(stderr, "[F1] FALHOU carregar libil2cpp (heap=%p)\n", i2heap);
  }
  /* informa o sem_shim das bases p/ o detector de livelock mapear callers */
  { extern void sh_set_bases(unsigned long, unsigned long, unsigned long, unsigned long);
    sh_set_bases(g_unity_base, 0x2000000, g_il2cpp_base, 0x3000000); }

  so_use(g_m_unity);  /* volta o contexto p/ libunity */

  /* lista os métodos nativos registrados (achar initJni/nativeRender) */
  extern void jni_dump_natives(void);
  extern void *jni_find_native(const char *);
  jni_dump_natives();

  /* ---- F2: janela GLES2 + lifecycle Unity ----
     Default = EGL REAL do Mali (Unity cria contexto/surface no fb0 via fbdev_window);
     CUP_SHIMEGL=1 = caminho antigo SDL2/egl_shim (não usado pelo Unity: egl* é PLT). */
  if (getenv("CUP_SHIMEGL")) {
    extern int egl_shim_ensure_current(void);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) fprintf(stderr, "[F2] SDL_Init: %s\n", SDL_GetError());
    egl_shim_create_window();
    egl_shim_ensure_current();   /* deixa o contexto GL current na thread do jogo */
    fprintf(stderr, "[F2] janela GLES2 criada (egl_shim/SDL2)\n");
  } else {
    /* áudio (opensles_shim usa SDL_OpenAudioDevice) */
    if (SDL_Init(SDL_INIT_AUDIO) != 0) fprintf(stderr, "[F2] SDL_Init(AUDIO): %s\n", SDL_GetError());
    fprintf(stderr, "[F2] EGL REAL Mali fbdev (fbdev_window %ux%u)\n",
            g_fbdev_win.w, g_fbdev_win.h);
  }

  static long thiz = 0xA1, ctx = 0xC0, surf = 0x5F;
  void *fn;
  if ((fn = jni_find_native("initJni"))) {
    fprintf(stderr, "[F2] initJni...\n");
    ((void (*)(void *, void *, void *))fn)(env, &thiz, &ctx);
    fprintf(stderr, "[F2] initJni OK\n");
    mm_probe("pos-initJni");
    dbg_sync();
  }
  if ((fn = jni_find_native("nativeRecreateGfxState"))) {
    mm_probe("pre-RecreateGfxState");
    /* TESTE: anula o instalador de signal-handlers do Unity (0x360af8) com RET.
       Esse caminho (sigaction QUERY -> map RB-tree de old-handlers via operator-new)
       e' onde o canario estoura. Nao precisamos dos handlers do Unity (temos on_crash). */
    if (getenv("CUP_NOSIGINST")) {
      extern void so_make_text_writable(void), so_make_text_executable(void);
      so_make_text_writable();
      *(uint32_t *)(g_alloc_ub + 0x360af8) = 0xd65f03c0u;  /* RET */
      so_make_text_executable();
      so_flush_caches();
      fprintf(stderr, "[NOSIGINST] 0x360af8 (install handlers) -> RET\n");
    }
    /* spy: hook na entrada do operator-new (0x3cbf2c) p/ capturar args da
       chamada que estoura o canario. Instala AQUI p/ pegar so' o gfx path. */
    if (getenv("CUP_ASPY")) {
      extern void so_make_text_writable(void), so_make_text_executable(void);
      g_gfx_cont = g_alloc_ub + 0x3cbf2c + 16;
      so_make_text_writable();
      hook_arm64(g_alloc_ub + 0x3cbf2c, (uintptr_t)onew_spy_tramp);
      so_make_text_executable();
      so_flush_caches();
      g_in_gfx = 1;
      fprintf(stderr, "[ONEW] hook em operator-new (0x3cbf2c) instalado\n");
    }
    fprintf(stderr, "[F2] nativeRecreateGfxState...\n");
    ((void (*)(void *, void *, int, void *))fn)(env, &thiz, 0, &surf);
    fprintf(stderr, "[F2] nativeRecreateGfxState OK\n");
    mm_probe("pos-RecreateGfxState");
    dbg_sync();
  }
  if ((fn = jni_find_native("nativeResume"))) ((void (*)(void *, void *))fn)(env, &thiz);
  if ((fn = jni_find_native("nativeFocusChanged"))) ((void (*)(void *, void *, int))fn)(env, &thiz, 1);

  /* dispara a thread de áudio do FMOD (alimenta fmodProcess em paralelo ao
     render — destrava o boot que espera o mixer). CUP_NOAUDIOTHREAD=1 desliga. */
  if (!getenv("CUP_NOAUDIOTHREAD")) {
    g_fmod_env = env;
    pthread_t at; pthread_create(&at, NULL, fmod_audio_thread, NULL);
    pthread_detach(at);
    fprintf(stderr, "[AUDIO] thread de áudio FMOD criada\n");
  }
  if (getenv("CUP_PRELOAD_TICK")) {
    pthread_t tt; pthread_create(&tt, NULL, preload_tick_thread, NULL);
    pthread_detach(tt);
  }
  if (getenv("CUP_PSPY")) {
    pthread_t st; pthread_create(&st, NULL, preload_spy_thread, NULL);
    pthread_detach(st);
  }
  void *render = jni_find_native("nativeRender");
  fprintf(stderr, "[F2] nativeRender=%p -> loop\n", render);
  int max_f = getenv("CUP_FRAMES") ? atoi(getenv("CUP_FRAMES")) : 600;
  void *fpump = jni_find_native("nativePause");  /* só p/ existência */ (void)fpump;
  g_render_tid = (int)syscall(SYS_gettid);   /* p/ recovery longjmp (CUP_SKIPBAD) */
  /* CUP_AUTOTAP: o disclaimer/menu espera "toque/botão pra continuar". Injeta
     periodicamente um botão de confirmação via nativeInjectEvent (KeyEvent) p/
     avançar. CUP_AUTOTAP=keycode (default 66=ENTER; 96=BUTTON_A, 23=DPAD_CENTER). */
  extern struct hk_inject_s { int action, keycode, source, deviceId, metaState, repeat,
                              scancode, flags, unicode; long eventTime, downTime; } g_hk_inject;
  extern void *hk_keyevent_object(void);
  void *inject = jni_find_native("nativeInjectEvent");
  int tapkey = getenv("CUP_AUTOTAP") ? atoi(getenv("CUP_AUTOTAP")) : 0;
  if (tapkey && inject) fprintf(stderr, "[AUTOTAP] keycode=%d via nativeInjectEvent=%p\n", tapkey, inject);
  /* CUP_DRAINPRELOAD=N: os ops de preload do título completam o background (jobmgr=0)
   * mas ficam presos na fila de INTEGRAÇÃO (integQ) pq a integração per-frame não roda
   * fora do WaitForAll. Dirigimos nós: N× UpdatePreloadingSingleStep(mgr,2,0x10) por frame
   * (limitado=não pendura; +FORCEINTEG p/ passar o gate de budget). mgr vem do PSPY. */
  int drainN = getenv("CUP_DRAINPRELOAD") ? atoi(getenv("CUP_DRAINPRELOAD")) : 0;
  void (*preload_step)(void *, int, int) = drainN ? (void (*)(void *, int, int))(g_unity_base + 0x8733a8) : NULL;
  if (drainN) fprintf(stderr, "[DRAINPRELOAD] %d steps/frame (UpdatePreloadingSingleStep=0x8733a8)\n", drainN);
  /* CUP_DRAINWAIT: chama WaitForAllAsyncOperationsToComplete(mgr) (0x873a90) 1×/frame.
   * Diferente do step cru, o WaitForAll roda o loop completo + a fase de "process"
   * (0x738a98) que DISPARA OS CALLBACKS de conclusão das async ops -> o FontLoader
   * vê as fontes como done e avança. ⚠️ pode pendurar se HasPendingOps nunca zerar. */
  int drainWait = getenv("CUP_DRAINWAIT") ? 1 : 0;
  void (*wait_all)(void *) = drainWait ? (void (*)(void *))(g_unity_base + 0x873a90) : NULL;
  if (drainWait) fprintf(stderr, "[DRAINWAIT] WaitForAll(mgr)=0x873a90 1x/frame\n");
  /* CUP_DRIVECR: o pump de coroutine do engine PARA de resumir o start_cr no $PC=9
   * (Cuphead.Init/RefreshDLC) — render voa mas $PC fica preso. Dirigimos o MoveNext
   * nós mesmos a cada N frames (cr1_tramp = MoveNext real). Só age a partir do
   * CUP_DRIVECR_FROM (default 200, dá tempo do boot normal rodar) p/ não atropelar
   * o pump do engine nas fases iniciais. */
  extern long cr1_tramp(void *it); extern void *volatile g_startcr_it;
  int drivecr = getenv("CUP_DRIVECR") ? 1 : 0;
  int drivecr_from = getenv("CUP_DRIVECR_FROM") ? atoi(getenv("CUP_DRIVECR_FROM")) : 200;
  if (drivecr) fprintf(stderr, "[DRIVECR] dirige start_cr MoveNext a partir do frame %d\n", drivecr_from);
  /* CUP_GCOFF: desabilita o GC do il2cpp durante o boot. Hipótese: o crash flaky do
     $PC=9 é use-after-free — o Boehm GC coleta um objeto que a desserialização do
     CupheadCore ainda referencia (a integração forçada cria objeto que o GC não
     rastreia). Sem GC no boot, nada é coletado -> sem UAF. (heap cresce; re-habilitar
     depois do título se preciso.) */
  int gcoff = (getenv("CUP_GCOFF") && g_il2cpp_base) ? 1 : 0;
  /* religa o GC no frame CUP_GCON_F (default 350, bem depois do boot ~frame 200) p/ o
     heap NÃO crescer indefinido (parado no disclaimer = OOM/thrash). 0 = nunca religa. */
  int gcon_f = getenv("CUP_GCON_F") ? atoi(getenv("CUP_GCON_F")) : 350;
  if (gcoff) {
    ((void (*)(void))(g_il2cpp_base + 0x1b62acc))();  /* il2cpp_gc_disable */
    fprintf(stderr, "[GCOFF] il2cpp_gc_disable() no boot; religa GC no frame %d\n", gcon_f);
  }
  int gamepad_on = getenv("CUP_GAMEPAD") ? 1 : 0;
  extern void gp_poll(void); extern void gp_frame_end(void);
  /* CUP_LOADYIELD=us: durante o boot/load, cede CPU aos WORKER threads (sched_yield+usleep)
     ANTES de cada frame integrar, p/ os jobs async COMPLETAREM antes da integração forçada
     (o check jobs-pending 0x6cdad0 é não-confiável aqui -> race -> objeto malformado ->
     crash $PC=9). Só nos primeiros LOADYIELD_F frames (fase de load). */
  int loadyield = getenv("CUP_LOADYIELD") ? atoi(getenv("CUP_LOADYIELD")) : 0;
  int loadyield_f = getenv("CUP_LOADYIELD_F") ? atoi(getenv("CUP_LOADYIELD_F")) : 350;
  if (loadyield) fprintf(stderr, "[LOADYIELD] %dus/frame nos 1ºs %d frames (CPU p/ workers)\n", loadyield, loadyield_f);
  /* CUP_MEMLOG: telemetria de memória a cada ~10s (600 frames) — p/ achar a curva
     do vazamento que mata o device ~6-7min de render (thrash->sshd starved->freeze) */
  int memlog = getenv("CUP_MEMLOG") ? 1 : 0;
  /* CUP_GCEVERY=N: força il2cpp_gc_collect a cada N frames (contém heap Boehm) */
  int gcevery = getenv("CUP_GCEVERY") ? atoi(getenv("CUP_GCEVERY")) : 0;
  int gc_pending = 0, gc_idle = 0;
  for (int f = 0; render && (max_f <= 0 || f < max_f); f++) {
    g_render_frame = f;  /* CUP_DRAWSPY: amarra os draws ao frame */
    if (memlog && f % 150 == 0) {
      static long last_swfree = -1; static int verbose_until = 0;
      long avail = -1, swfree = -1, rss = -1; char ln[128];
      FILE *mi = fopen("/proc/meminfo", "r");
      if (mi) { while (fgets(ln, sizeof ln, mi)) {
          sscanf(ln, "MemAvailable: %ld", &avail); sscanf(ln, "SwapFree: %ld", &swfree); }
        fclose(mi); }
      /* burst de swap (>8MB desde a última amostra) -> amostragem densa por 1200f */
      if (last_swfree >= 0 && last_swfree - swfree > 8192) verbose_until = f + 1200;
      last_swfree = swfree;
      if (f % 600 == 0 || f < verbose_until) {
        FILE *st = fopen("/proc/self/status", "r");
        if (st) { while (fgets(ln, sizeof ln, st)) sscanf(ln, "VmRSS: %ld", &rss); fclose(st); }
        long gch = ((long (*)(void))(g_il2cpp_base + 0x1b62ad4))();  /* il2cpp_gc_get_heap_size */
        long gcu = ((long (*)(void))(g_il2cpp_base + 0x1b62ad0))();  /* il2cpp_gc_get_used_size */
        fprintf(stderr, "[MEM] f=%d avail=%ldMB swfree=%ldMB rss=%ldMB gcheap=%ldMB gcused=%ldMB\n",
                f, avail / 1024, swfree / 1024, rss / 1024, gch >> 20, gcu >> 20);
        fsync(2);
      }
    }
    if (gcevery && f > gcon_f && f % gcevery == 0) gc_pending = 1;
    if (gc_pending) {
      /* limpeza de transição (ideia do usuário): solta assets da cena anterior +
         coleta o heap — sem isso o load da cena nova SOMA com a velha -> burst
         de ~150MB -> swap storm -> device asfixia.
         ⚠️ s12: SÓ com o PreloadManager OCIOSO (preloadQ[+224]==0 e integQ[+256]==0
         por 90 frames seguidos). O tick cego da s11 caiu NO MEIO do load assíncrono
         da cena do mapa (f=10800, rss subindo) e varreu objetos ainda não
         enraizados -> GameObject com scene NULL -> crash 0x541cdc. */
      char *m = (char *)g_preload_mgr;
      int idle = !m || (*(volatile uintptr_t *)(m + 224) == 0 &&
                        *(volatile uintptr_t *)(m + 256) == 0);
      gc_idle = idle ? gc_idle + 1 : 0;
      if (gc_idle >= (m ? 90 : 1200)) {   /* sem mgr capturado: espera ~20s */
        gc_pending = 0; gc_idle = 0;
        fprintf(stderr, "[GCEVERY] limpeza f=%d (mgr %s)\n", f, m ? "ocioso" : "n/d");
        ((void *(*)(void))(g_il2cpp_base + 0x178BAAC))(); /* Resources.UnloadUnusedAssets */
        ((void (*)(void))(g_il2cpp_base + 0x1b62ac0))();  /* il2cpp_gc_collect */
      }
    }
    { /* log de hits do SCENESKIP + MASKGUARD + NULLGUARD */
      static uint32_t sg_last, mg_last;
      if (g_sceneskip_hits != sg_last) {
        fprintf(stderr, "[SCENESKIP] GO sem scene pulado (%u hits, f=%d) — sobrevivido\n",
                g_sceneskip_hits, f); fsync(2);
        sg_last = g_sceneskip_hits;
      }
      if (g_maskguard_hits != mg_last) {
        fprintf(stderr, "[MASKGUARD] count insana clampada (%u hits, f=%d) — sobrevivido\n",
                g_maskguard_hits, f); fsync(2);
        mg_last = g_maskguard_hits;
      }
      static uint32_t ng_last;
      if (g_nullguard_hits != ng_last) {
        fprintf(stderr, "[NULLGUARD] arg0 NULL skipado (%u hits, f=%d) — sobrevivido\n",
                g_nullguard_hits, f); fsync(2);
        ng_last = g_nullguard_hits;
      }
    }
    if (gcoff && gcon_f > 0 && f == gcon_f) {
      ((void (*)(void))(g_il2cpp_base + 0x1b62ac8))();  /* il2cpp_gc_enable */
      ((void (*)(void))(g_il2cpp_base + 0x1b62ac0))();  /* il2cpp_gc_collect */
      fprintf(stderr, "[GCOFF] GC RELIGADO + collect no frame %d (boot ja passou)\n", f);
      fflush(stderr);
    }
    if (loadyield && f < loadyield_f) { for (int y = 0; y < 4; y++) sched_yield(); usleep(loadyield); }
    if (gamepad_on) gp_poll();   /* drena eventos do js0 ANTES do frame ler input */
    if (drivecr && f >= drivecr_from && g_startcr_it) cr1_tramp(g_startcr_it);
    if (drainN && g_preload_mgr) {
      for (int k = 0; k < drainN; k++) preload_step(g_preload_mgr, 2, 0x10);
    }
    if (drainWait && g_preload_mgr) {
      /* zera [mgr+0xE0] (flag GfxDevice +224) p/ WaitForAll só checar o integQ [+256]
       * e NÃO pendurar no loop (com MT-off o flag +224 fica setado p/ sempre). */
      if (getenv("CUP_DRAINWAIT_GFX")) *(volatile int *)((char *)g_preload_mgr + 0xE0) = 0;
      wait_all(g_preload_mgr);
    }
    if (f < 200) { fprintf(stderr, "[r%d>\n", f); dbg_sync(); }  /* ENTRA no render */
    if (g_skipbad) {
      /* arma o recovery: se nativeRender crashar nesta thread, volta aqui e pula o frame */
      if (sigsetjmp(g_render_jmp, 1) == 0) {
        g_render_jmp_armed = 1;
        ((unsigned char (*)(void *, void *))render)(env, &thiz);
      } else {
        if (g_recover_n < 80 || (g_recover_n % 60) == 0)
          fprintf(stderr, "[RECOVER] frame %d pulado (crash #%lu na render)\n", f, g_recover_n);
      }
      g_render_jmp_armed = 0;
    } else {
      ((unsigned char (*)(void *, void *))render)(env, &thiz);
    }
    if (f < 200) { fprintf(stderr, "<r%d]\n", f); dbg_sync(); }  /* SAIU do render */
    opensles_shim_pump_callbacks();
    /* bombeia eventos SDL (foco/janela) p/ o input do Unity não esfomear */
    SDL_Event ev; while (SDL_PollEvent(&ev)) {}
    /* AUTOTAP: a cada ~90 frames, manda DOWN; ~3 frames depois, UP (1 "toque") */
    if (tapkey && inject && f > 120) {
      int phase = f % 90;
      if (phase == 0 || phase == 3) {
        g_hk_inject.action = (phase == 0) ? 0 : 1;   /* 0=DOWN 1=UP */
        g_hk_inject.keycode = tapkey;
        g_hk_inject.source = 0x501;                  /* gamepad|keyboard */
        g_hk_inject.deviceId = 0; g_hk_inject.repeat = 0; g_hk_inject.flags = 0;
        g_hk_inject.metaState = 0; g_hk_inject.scancode = 0; g_hk_inject.unicode = 0;
        ((int (*)(void *, void *, void *))inject)(env, &thiz, hk_keyevent_object());
        if (f < 600) fprintf(stderr, "[AUTOTAP] %s key=%d (f=%d)\n", phase ? "UP" : "DOWN", tapkey, f);
      }
    }
    if (gamepad_on) gp_frame_end();  /* snapshot p/ edge-detect do GetButtonDown/Up */
    if (f % 60 == 0) { fprintf(stderr, "[render %d]\n", f); dbg_sync(); }
  }
  fprintf(stderr, "[F2] === render loop terminou ===\n");
  fflush(stderr); dbg_sync();
  _exit(0);  /* hard exit — destrutores do .so crasham no teardown normal */
}
