/*
 * main.c -- DYSMANTLE (10tons NX engine, build Android) so-loader p/ NextOS
 * aarch64 + Mali-450 (Utgard, GLES2 via SDL2). Loader de DOIS módulos:
 *   Módulo A = libc++_shared.so (ABI __ndk1) -> std C++ runtime.
 *   Módulo B = libNativeGame.so (engine). Resolve via:
 *              dysmantle_overrides + revc_pthread_table + snapshot(libc++)
 *              + dlsym(RTLD_DEFAULT) das libs do device (SDL2/GLESv2/EGL/libc/m).
 * Entrada = android_main (NativeActivity), janela GLES2 via egl_shim/SDL2.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "so_util.h"
#include "android_shim.h"
#include "egl_shim.h"
#include "jni_shim.h"

#define CXX_SO  "libc++_shared.so"
#define GAME_SO "libNativeGame.so"
#define CXX_HEAP_MB  48
#define GAME_HEAP_MB 192

extern DynLibFunction dysmantle_overrides[];
extern const int dysmantle_overrides_count;
extern DynLibFunction revc_pthread_table[];
extern const int revc_pthread_count;

/* 🩹 CANARY BIONIC (causa-raiz do "stack smash" falso): a engine lê a
 * stack-guard de tpidr_el0+0x28 (bionic TLS_SLOT_STACK_GUARD). Sob glibc esse
 * endereço cai no TLS de alguma lib e MUDA durante a execução (ex: errno em
 * pthread_create) -> prólogo lê X, epílogo lê Y -> __stack_chk_fail. E como o
 * compilador trata __stack_chk_fail como noreturn, nosso no-op RETORNAVA e a
 * execução CAÍA no código adjacente (em NXTI_CreateThread caía na lambda de
 * thread-entry rodando no parent com x0=lixo -> memcpy crash "n=11").
 * FIX = reservar o início do TLS do exe (1º bloco após o TCB de 16 bytes,
 * offset 16..272 c/ aligned(16)) com um pad NUNCA escrito -> tpidr+0x28 fica
 * estável (zero) p/ toda thread -> canary nunca mais dá mismatch. */
__attribute__((aligned(16))) static _Thread_local char g_bionic_guard_pad[256];

static DynLibFunction *g_base;
static int g_base_n;
static void build_base_table(void) {
  g_base_n = dysmantle_overrides_count + revc_pthread_count;
  g_base = malloc(sizeof(DynLibFunction) * g_base_n);
  memcpy(g_base, dysmantle_overrides, sizeof(DynLibFunction) * dysmantle_overrides_count);
  memcpy(g_base + dysmantle_overrides_count, revc_pthread_table,
         sizeof(DynLibFunction) * revc_pthread_count);
}

static void resolve_addr(uintptr_t a, char *out, int outsz);
static void brk_handler(int sig, siginfo_t *info, void *uc);
static volatile uintptr_t g_load_base = 0; /* base do libNativeGame (vaddr 0) */

/* tabela de módulos custom-loaded p/ simbolicação no crash: vaddr = addr-base.
 * code_lo/hi = range do .text (vaddr) p/ filtrar retornos no stack scan. */
static struct { const char *name; uintptr_t base; size_t size;
                uintptr_t code_lo, code_hi; } g_mods[2];
static int g_mods_n = 0;
/* resolve addr -> "modname@0xvaddr" se cair num módulo nosso; senão maps */
static void resolve_addr2(uintptr_t a, char *out, int outsz) {
  for (int i = 0; i < g_mods_n; i++) {
    if (a >= g_mods[i].base && a < g_mods[i].base + g_mods[i].size) {
      snprintf(out, outsz, "%s@0x%lx", g_mods[i].name,
               (unsigned long)(a - g_mods[i].base));
      return;
    }
  }
  resolve_addr(a, out, outsz);
}
static void crash_handler(int sig, siginfo_t *info, void *uc) {
  uintptr_t fault = (uintptr_t)info->si_addr;
  uintptr_t tb = (uintptr_t)text_base;
  fprintf(stderr, "\n=== CRASH sig=%d addr=%p tid=%d ===\n", sig, info->si_addr,
          (int)syscall(178));
  { /* comm da thread que crashou */
    int cfd = open("/proc/thread-self/comm", O_RDONLY);
    if (cfd >= 0) { char cm[32]; int cn = read(cfd, cm, 31);
      if (cn > 0) { cm[cn] = 0; fprintf(stderr, "  comm=%s", cm); }
      close(cfd); }
  }
  ucontext_t *u = (ucontext_t *)uc;
  uintptr_t pc = u->uc_mcontext.pc;
  char r[300];
  resolve_addr2(pc, r, sizeof(r));
  fprintf(stderr, "  PC=%p %s\n", (void *)pc, r);
  for (int i = 0; i < 29; i += 4)
    fprintf(stderr, "  x%-2d=%016lx x%-2d=%016lx x%-2d=%016lx x%-2d=%016lx\n",
            i, (unsigned long)u->uc_mcontext.regs[i],
            i+1, (unsigned long)u->uc_mcontext.regs[i+1],
            i+2, (unsigned long)u->uc_mcontext.regs[i+2],
            i+3, (unsigned long)u->uc_mcontext.regs[i+3]);
  fprintf(stderr, "  sp=%lx\n", (unsigned long)u->uc_mcontext.sp);
  { char rr[200]; resolve_addr2(u->uc_mcontext.regs[30], rr, sizeof(rr));
    fprintf(stderr, "  x30(LR) %s\n", rr); }
  uintptr_t fp = u->uc_mcontext.regs[29];
  for (int f = 0; f < 24 && fp; f++) {
    uintptr_t *p = (uintptr_t *)fp; uintptr_t next = p[0], lr = p[1];
    if (!lr) break;
    resolve_addr2(lr, r, sizeof(r));
    fprintf(stderr, "  #%-2d lr %p %s\n", f, (void *)lr, r);
    if (next <= fp) break; fp = next;
  }
  /* stack scan: retornos em qualquer módulo nosso (ordem = profundidade) */
  if (g_mods_n) {
    fprintf(stderr, "  --- stack scan (mods) ---\n");
    uintptr_t sp = u->uc_mcontext.sp;
    int n = 0;
    for (uintptr_t a = sp; a < sp + 0x3000 && n < 28; a += 8) {
      uintptr_t v = *(uintptr_t *)a;
      for (int i = 0; i < g_mods_n; i++) {
        uintptr_t off = v - g_mods[i].base;
        if (off >= g_mods[i].code_lo && off <= g_mods[i].code_hi) {
          fprintf(stderr, "    [sp+0x%lx] %s@0x%lx\n",
                  (unsigned long)(a - sp), g_mods[i].name, (unsigned long)off);
          n++;
        }
      }
    }
  }
  (void)fault;
  fflush(stderr);
  _exit(128 + sig);
}
/* resolve addr -> "mapname+off" lendo /proc/self/maps (async-signal: usa só read) */
static void resolve_addr(uintptr_t a, char *out, int outsz) {
  int fd = open("/proc/self/maps", O_RDONLY);
  out[0] = 0;
  if (fd < 0) return;
  char buf[8192]; int n; char line[400]; int li = 0;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (int i = 0; i < n; i++) {
      char c = buf[i];
      if (c == '\n' || li >= (int)sizeof(line) - 1) {
        line[li] = 0;
        unsigned long s, e; char perm[8]; char path[256]; path[0] = 0;
        if (sscanf(line, "%lx-%lx %7s %*x %*s %*d %255s", &s, &e, perm, path) >= 3) {
          if (a >= s && a < e) {
            const char *base = strrchr(path, '/'); base = base ? base + 1 : (path[0] ? path : "?");
            snprintf(out, outsz, "%s+0x%lx", base, (unsigned long)(a - s));
            close(fd); return;
          }
        }
        li = 0;
      } else line[li++] = c;
    }
  }
  close(fd);
}
/* SIGUSR1: dump backtrace da thread atual SEM sair, resolvendo cada frame. */
static void bt_handler(int sig, siginfo_t *info, void *uc) {
  (void)sig; (void)info;
  ucontext_t *u = (ucontext_t *)uc;
  uintptr_t tb = (uintptr_t)text_base, pc = u->uc_mcontext.pc;
  char r[300];
  resolve_addr2(pc, r, sizeof(r));
  fprintf(stderr, "\n[BT tid=%d] PC=%p %s", (int)syscall(178), (void *)pc, r);
  if (pc >= tb && pc < tb + text_size) fprintf(stderr, " {%s+0x%lx}", GAME_SO, (unsigned long)(pc - tb));
  fprintf(stderr, "\n");
  uintptr_t fp = u->uc_mcontext.regs[29];
  for (int f = 0; f < 28 && fp; f++) {
    uintptr_t *p = (uintptr_t *)fp; uintptr_t next = p[0], lr = p[1];
    if (!lr) break;
    resolve_addr2(lr, r, sizeof(r));
    fprintf(stderr, "  #%-2d lr %p %s", f, (void *)lr, r);
    if (lr >= tb && lr < tb + text_size) fprintf(stderr, " {%s+0x%lx}", GAME_SO, (unsigned long)(lr - tb));
    fprintf(stderr, "\n");
    if (next <= fp) break; fp = next;
  }
  fflush(stderr);
}
static void install_crash_handler(void) {
  struct sigaction sa; memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = crash_handler; sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL);
  sigaction(SIGILL, &sa, NULL); sigaction(SIGABRT, &sa, NULL);
  struct sigaction sb; memset(&sb, 0, sizeof(sb));
  sb.sa_sigaction = bt_handler; sb.sa_flags = SA_SIGINFO;
  sigaction(SIGUSR1, &sb, NULL);
  struct sigaction sc; memset(&sc, 0, sizeof(sc));
  sc.sa_sigaction = brk_handler; sc.sa_flags = SA_SIGINFO;
  sigaction(SIGTRAP, &sc, NULL);
}

static void preload_device_libs(void) {
  static const char *libs[] = {
      "libSDL2-2.0.so.0", "libGLESv2.so", "libEGL.so",
      "libm.so.6", "libdl.so.2", NULL };
  for (int i = 0; libs[i]; i++) {
    void *h = dlopen(libs[i], RTLD_NOW | RTLD_GLOBAL);
    fprintf(stderr, "preload: %s %s\n", libs[i], h ? "OK" : dlerror());
  }
}

/* Patch runtime: escreve instruções aarch64 num símbolo do jogo. */
static void patch_words(const char *sym, const uint32_t *words, int n) {
  uintptr_t a = so_find_addr(sym);
  if (!a) { fprintf(stderr, "patch: símbolo %s não encontrado\n", sym); return; }
  uintptr_t pg = a & ~0xFFFUL;
  if (mprotect((void *)pg, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
    fprintf(stderr, "patch: mprotect %s falhou\n", sym); return;
  }
  for (int i = 0; i < n; i++) ((uint32_t *)a)[i] = words[i];
  mprotect((void *)pg, 0x2000, PROT_READ | PROT_EXEC);
  __builtin___clear_cache((char *)a, (char *)a + n * 4);
  fprintf(stderr, "patch: %s @ %p (%d instr)\n", sym, (void *)a, n);
}
static void patch_func_ret(const char *sym) {
  uint32_t w[] = {0xd65f03c0}; /* ret */
  patch_words(sym, w, 1);
}
static void patch_func_ret0(const char *sym) {
  uint32_t w[] = {0x52800000, 0xd65f03c0}; /* mov w0,#0 ; ret */
  patch_words(sym, w, 2);
}
static void patch_func_ret1(const char *sym) {
  uint32_t w[] = {0x52800020, 0xd65f03c0}; /* mov w0,#1 ; ret */
  patch_words(sym, w, 2);
}
/* patch num vaddr arbitrário (load_base derivado de android_main vaddr 0x4651a4) */
static void patch_vaddr(uintptr_t vaddr, uint32_t word) {
  uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
  uintptr_t a = lb + vaddr;
  uintptr_t pg = a & ~0xFFFUL;
  if (mprotect((void *)pg, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
    fprintf(stderr, "patch_vaddr mprotect falhou\n"); return; }
  *(uint32_t *)a = word;
  mprotect((void *)pg, 0x2000, PROT_READ | PROT_EXEC);
  __builtin___clear_cache((char *)a, (char *)a + 4);
  fprintf(stderr, "patch_vaddr: 0x%lx = 0x%08x @ %p\n", (unsigned long)vaddr, word, (void *)a);
}

/* ---- BRK-trap tracer (funciona em função LOCAL, ≠ GOT-hook) ----
 * Arma BRK #0 no entry; no SIGTRAP loga, restaura a instrução original e
 * re-executa (one-shot). O ÚLTIMO ENTER antes do crash = função que corrompe. */
#define MAX_BRK 24
static struct { uintptr_t addr; const char *name; uint32_t orig; } g_brk[MAX_BRK];
static volatile int g_brk_n = 0;
static void arm_brk(uintptr_t vaddr, const char *name) {
  if (g_brk_n >= MAX_BRK) return;
  uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
  uintptr_t a = lb + vaddr;
  uintptr_t pg = a & ~0xFFFUL;
  if (mprotect((void *)pg, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return;
  g_brk[g_brk_n].addr = a; g_brk[g_brk_n].name = name; g_brk[g_brk_n].orig = *(uint32_t *)a;
  *(uint32_t *)a = 0xd4200000; /* brk #0 */
  mprotect((void *)pg, 0x2000, PROT_READ | PROT_EXEC);
  __builtin___clear_cache((char *)a, (char *)a + 4);
  fprintf(stderr, "arm_brk: %s @ 0x%lx\n", name, (unsigned long)vaddr);
  g_brk_n++;
}
static volatile uintptr_t g_canary_addr = 0;
static volatile int g_rearm = -1;        /* índice do brk a re-armar no single-step */
static int safe_str(uintptr_t p, char *out, int n) {
  if (p < 0x1000) { out[0] = 0; return 0; }
  int i = 0; char *s = (char *)p;
  for (; i < n - 1; i++) { char c = s[i]; if (c == 0) break; out[i] = (c >= 32 && c < 127) ? c : '.'; }
  out[i] = 0; return i;
}
static void brk_handler(int sig, siginfo_t *info, void *uc) {
  (void)sig; (void)info;
  ucontext_t *u = (ucontext_t *)uc;
  uintptr_t pc = u->uc_mcontext.pc;
  /* single-step após restaurar: re-arma o brk e segue */
  if (g_rearm >= 0 && pc != g_brk[g_rearm].addr) {
    int i = g_rearm; g_rearm = -1;
    uintptr_t a = g_brk[i].addr, pg = a & ~0xFFFUL;
    mprotect((void *)pg, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
    *(uint32_t *)a = 0xd4200000;
    mprotect((void *)pg, 0x2000, PROT_READ | PROT_EXEC);
    __builtin___clear_cache((char *)a, (char *)a + 4);
    u->uc_mcontext.pstate &= ~0x200000UL;   /* limpa single-step */
    return;
  }
  for (int i = 0; i < g_brk_n; i++) {
    if (g_brk[i].addr == pc) {
      const char *nm = g_brk[i].name;
      if (nm[0] == 'S' && nm[1] == 'Q') { /* compile: loga sourcename(x3 p/ buffer, x1 p/ compile)+source */
        char a1[120], a3[120];
        safe_str(u->uc_mcontext.regs[1], a1, sizeof(a1));
        safe_str(u->uc_mcontext.regs[3], a3, sizeof(a3));
        fprintf(stderr, "[SQ] %s x1=\"%s\" x3=\"%s\"\n", nm, a1, a3);
      } else {
        /* genérico: tid + x0..x2 + strings em x0/x1 + dump de 8 qwords em x0 */
        uintptr_t x0 = u->uc_mcontext.regs[0], x1 = u->uc_mcontext.regs[1];
        char s0[64], s1[64];
        safe_str(x0, s0, sizeof(s0)); safe_str(x1, s1, sizeof(s1));
        fprintf(stderr, "[BRK tid=%d] %s x0=0x%lx \"%s\" x1=0x%lx \"%s\" x2=0x%lx\n",
                (int)syscall(178), nm, (unsigned long)x0, s0,
                (unsigned long)x1, s1, (unsigned long)u->uc_mcontext.regs[2]);
        if (x0 > 0x10000 && x0 < 0x8000000000UL) {
          uintptr_t *q = (uintptr_t *)x0;
          fprintf(stderr, "       [x0]: %lx %lx %lx %lx %lx %lx %lx %lx\n",
                  q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7]);
        }
      }
      fflush(stderr);
      uintptr_t pg = pc & ~0xFFFUL;
      mprotect((void *)pg, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
      *(uint32_t *)pc = g_brk[i].orig;   /* restaura -> one-shot */
      mprotect((void *)pg, 0x2000, PROT_READ | PROT_EXEC);
      __builtin___clear_cache((char *)pc, (char *)pc + 4);
      return;
    }
  }
  fprintf(stderr, "[BRK] SIGTRAP inesperado @ %p\n", (void *)pc); fflush(stderr);
  _exit(133);
}
static void install_brk_traps(void) {
  /* cadeia de criação de thread nomeada (crash memcpy NULL n=11 "UI AUTOEXEC") */
  arm_brk(0x5716b8, "NXT_CreateThread");        /* x0=nome, x1=func, x2=arg */
  arm_brk(0x573e94, "NXTI_CreateThread");       /* x0=ThreadInfo* */
  arm_brk(0x573f24, "thread_entry");            /* x0=ThreadInfo* (nova thread) */
  arm_brk(0x571d9c, "NXTI_InitializeThread");   /* x0=ThreadInfo& */
  arm_brk(0x75cbe4, "nString_Set");             /* x0=this(TLS emutls!), x1=src */
  arm_brk(0x573f74, "SetThreadNamePlatform");   /* x0=data, x1=len */
}

/* GOT-hook de NXI_GetProductValue -> força opengl_version="2.0" (caminho ES2) */
static const char *(*orig_getprod)(const char *) = NULL;
static const char *my_getprod(const char *key) {
  const char *r = orig_getprod ? orig_getprod(key) : NULL;
  if (key && strcmp(key, "opengl_version") == 0) {
    fprintf(stderr, "[cfg] opengl_version real='%s' -> forçando '2.0'\n", r ? r : "(null)");
    return "2.0";
  }
  return r;
}
static void hook_getproductvalue(void) {
  uintptr_t slot = so_find_rel_addr_safe("_Z19NXI_GetProductValuePKc");
  if (!slot) { fprintf(stderr, "hook: GOT de NXI_GetProductValue não achado\n"); return; }
  uintptr_t *p = (uintptr_t *)slot;
  orig_getprod = (const char *(*)(const char *))*p;
  *p = (uintptr_t)my_getprod;
  fprintf(stderr, "hook: NXI_GetProductValue GOT %p orig=%p\n", (void *)slot, (void *)orig_getprod);
}

static void load_module(const char *name, int heap_mb, DynLibFunction *tbl, int n) {
  size_t hs = (size_t)heap_mb * 1024 * 1024;
  void *heap = mmap(NULL, hs, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap == MAP_FAILED) { fprintf(stderr, "mmap %d MB falhou (%s)\n", heap_mb, name); exit(1); }
  fprintf(stderr, "== carregando %s (heap %p, %d MB) ==\n", name, heap, heap_mb);
  if (so_load(name, heap, hs) < 0) { fprintf(stderr, "so_load(%s) falhou\n", name); exit(1); }
  if (so_relocate() < 0) { fprintf(stderr, "so_relocate(%s) falhou\n", name); exit(1); }
  so_resolve(tbl, n, 0);
  so_finalize();
  so_flush_caches();
  so_execute_init_array();
  fprintf(stderr, "== %s: text=%p+%zu data=%p+%zu ==\n", name, text_base, text_size, data_base, data_size);
  if (g_mods_n < 2) {
    g_mods[g_mods_n].name = strstr(name, "c++") ? "libc++" : "game";
    g_mods[g_mods_n].base = (uintptr_t)heap;
    g_mods[g_mods_n].size = hs;
    if (strstr(name, "c++")) { /* ranges .text (vaddr) dos ELFs */
      g_mods[g_mods_n].code_lo = 0x9ecb0; g_mods[g_mods_n].code_hi = 0x1352f0;
    } else {
      g_mods[g_mods_n].code_lo = 0x463000; g_mods[g_mods_n].code_hi = 0xd8e000;
    }
    g_mods_n++;
  }
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  install_crash_handler();
  fprintf(stderr, "=== DYSMANTLE (Android) so-loader / NextOS aarch64 Mali-450 ===\n");

  /* valida que tpidr_el0+0x28 (canary bionic) caiu DENTRO do nosso pad TLS */
  { uintptr_t tp; __asm__ volatile("mrs %0, tpidr_el0" : "=r"(tp));
    uintptr_t slot = tp + 0x28, lo = (uintptr_t)g_bionic_guard_pad;
    fprintf(stderr, "TLS guard: tpidr=0x%lx slot+0x28=0x%lx pad=[0x%lx..0x%lx] %s (val=0x%lx)\n",
            (unsigned long)tp, (unsigned long)slot, (unsigned long)lo,
            (unsigned long)(lo + sizeof(g_bionic_guard_pad)),
            (slot >= lo && slot + 8 <= lo + sizeof(g_bionic_guard_pad))
                ? "DENTRO ✓" : "FORA ⚠️ (canary instável!)",
            *(unsigned long *)slot);
  }

  jni_shim_set_package("com.dysmantle53.soco", 0);

  preload_device_libs();
  build_base_table();

  /* Módulo A: libc++_shared.so */
  load_module(CXX_SO, CXX_HEAP_MB, g_base, g_base_n);
  int cxx_n = 0;
  DynLibFunction *cxx_tbl = so_snapshot_symbols(&cxx_n);
  if (!cxx_tbl || cxx_n <= 0) { fprintf(stderr, "snapshot %s vazio\n", CXX_SO); exit(1); }
  fprintf(stderr, "libc++: %d símbolos exportados\n", cxx_n);

  /* expõe basic_filebuf::open real (do snapshot) p/ o override em imports.c */
  extern void *g_real_filebuf_open;
  for (int i = 0; i < cxx_n; i++)
    if (strcmp(cxx_tbl[i].symbol, "_ZNSt6__ndk113basic_filebufIcNS_11char_traitsIcEEE4openEPKcj") == 0)
      g_real_filebuf_open = (void *)cxx_tbl[i].func;
  fprintf(stderr, "filebuf::open real = %p\n", g_real_filebuf_open);

  int comb_n = g_base_n + cxx_n;
  DynLibFunction *comb = malloc(sizeof(DynLibFunction) * comb_n);
  memcpy(comb, g_base, sizeof(DynLibFunction) * g_base_n);
  memcpy(comb + g_base_n, cxx_tbl, sizeof(DynLibFunction) * cxx_n);

  /* Módulo B: libNativeGame.so */
  load_module(GAME_SO, GAME_HEAP_MB, comb, comb_n);

  /* SwappyGL (frame pacing AGDK): a engine checa o retorno de SwappyGL_init
   * (tbz w0,#0 -> se par=falha -> "Unable to initialize the renderer").
   * Stub init=return 1 (sucesso, sem rodar o JNI pesado). isEnabled=0 -> a
   * engine usa eglSwapBuffers direto via ContextImpEGL::SwapBuffers. */
  patch_func_ret1("SwappyGL_init");
  patch_func_ret0("SwappyGL_isEnabled");

  /* SoundImpOboe::Initialize(float): crashava ANTES do fix do canary TLS (o
   * "crash STL/JNI" era provavelmente o mesmo falso-positivo). Tentando rodar
   * o Oboe de verdade agora; fallback p/ patch ret0 se voltar a crashar.
   * DYSMANTLE_NO_OBOE=1 -> re-aplica o patch (som Null). */
  if (getenv("DYSMANTLE_NO_OBOE"))
    patch_func_ret0("_ZN12SoundImpOboe10InitializeEf");

  /* Desabilita popups de erro: a textura grunge-scratched.jpg falha e a engine
   * mostra um popup que crasha. nx_run_no_popups=1 + NXD_ShowPopup=no-op ->
   * pula o popup e segue (textura faltante é só decoração de UI). */
  { uintptr_t g = so_find_addr("nx_run_no_popups");
    if (g) { *(int *)g = 1; fprintf(stderr, "nx_run_no_popups=1 @ %p\n", (void *)g); } }
  patch_func_ret("_Z13NXD_ShowPopupi12NX_PopupTypePKcb");
  /* ImageWriterJPEG::Initialize: na falha do bitmap a engine tenta cachear via
   * JPEG-encode e crasha (libjpeg). Falha gracioso (return 0) -> pula o cache. */
  patch_func_ret0("_ZN15ImageWriterJPEG10InitializeEv");

  /* 🔑 NX_Graphics_IsTextureFormatSupported -> 1 (true): o APK modado tem os JPEGs
   * de UI VAZIOS (size 0) e só a versão .ktx (ETC2). A engine, achando que ETC2
   * não é suportado (Mali-450=GLES2), cai no .jpg vazio -> crash. Forçando
   * "suportado", carrega o .ktx (TEM dados) via KtxImageLoader -> sem crash. */
  patch_func_ret1("_Z36NX_Graphics_IsTextureFormatSupportedRK22nx_bitmap_parameters_t");

  /* GOT-hook NXI_GetProductValue: a engine lê "opengl_version" do config; se
   * pedir ES3 ela monta o APIManager ES3 (mais funções) num buffer de pilha
   * dimensionado p/ ES2 -> stack smash no nosso contexto Utgard (ES2).
   * Forçamos "2.0" p/ alinhar a engine ao caminho ES2. */
  hook_getproductvalue();
  install_brk_traps();

  /* registra o .eh_frame do jogo no unwinder C++: o módulo é custom-loaded (não
   * dlopen), então o unwinder não o conhece -> exceções (ex: falha de textura)
   * não conseguem desenrolar a pilha -> crash. .eh_frame @ vaddr 0x349900. */
  {
    extern void __register_frame(void *);
    uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
    __register_frame((void *)(lb + 0x349900));
    fprintf(stderr, "__register_frame eh_frame @ %p\n", (void *)(lb + 0x349900));
  }

  g_load_base = so_find_addr("android_main") - 0x4651a4;
  uintptr_t am = so_find_addr("android_main");
  if (!am) { fprintf(stderr, "android_main NÃO encontrado\n"); exit(1); }
  fprintf(stderr, "android_main @ %p\n", (void *)am);

  struct android_app *app = android_shim_init();
  if (!app) { fprintf(stderr, "android_shim_init falhou\n"); exit(1); }

  /* cria janela SDL + contexto GLES2 (Mali fbdev) ANTES do jogo usar EGL */
  egl_shim_create_window();

  android_shim_send_cmd(app, APP_CMD_INIT_WINDOW);
  android_shim_send_cmd(app, APP_CMD_GAINED_FOCUS);

  fprintf(stderr, "=== chamando android_main ===\n");
  void (*android_main_func)(struct android_app *) = (void (*)(struct android_app *))am;
  android_main_func(app);

  fprintf(stderr, "=== android_main retornou ===\n");
  _exit(0);
}
