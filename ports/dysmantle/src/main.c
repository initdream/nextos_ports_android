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
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
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
/* 🩹 Recuperação do crash do skinned-actor: ModelInstance::InitializeFromModel
 * crasha (race de init de singleton → cópia/deref de ponteiro nulo) ao importar
 * o mapa. O detour my_ifm (abaixo) faz sigsetjmp; se a função crashar, o
 * crash_handler dá siglongjmp de volta → InitializeFromModel é PULADO inteiro
 * (o ator skinned não renderiza, mas o mundo carrega). Per-thread. */
static __thread sigjmp_buf g_ifm_jmp;
static __thread volatile int g_ifm_armed;
static volatile int g_ifm_skips;
static void crash_handler(int sig, siginfo_t *info, void *uc) {
  uintptr_t fault = (uintptr_t)info->si_addr;
  uintptr_t tb = (uintptr_t)text_base;
  if (sig == SIGSEGV && g_ifm_armed && !getenv("DYSMANTLE_NORECOVER")) {
    g_ifm_armed = 0;
    if (__sync_fetch_and_add(&g_ifm_skips, 1) < 6)
      fprintf(stderr, "[IFM-SKIP] InitializeFromModel (skinned) crashou @%p -> pulado\n",
              info->si_addr);
    siglongjmp(g_ifm_jmp, 1); /* volta pro my_ifm, pula o resto da função */
  }
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
  /* a engine LÊ o controle? (one-shot: 1º hit de cada um loga e some) */
  arm_brk(0x46fe24, "PB_getControllerData");    /* (index, data*) */
  arm_brk(0x46f48c, "PBeng_FrameStart");        /* engine lê pads/frame */
  arm_brk(0x46f8c0, "PBeng_CheckConnStatuses"); /* engine scaneia status */
  arm_brk(0x46f980, "PBeng_OnControllerStatusChange"); /* callback fired! */
}

/* DIAG: detour inline em NX_Graphics_CreateVertexBufferWithVertices p/ logar
 * o vertex format que a engine pede (a malha do mundo/armas falha c/ "unknown
 * vertex format"). A função é chamada por ponteiro (0 BL diretos) → hook_arm64
 * inline pega tudo. Trampolim = 1ª instrução (PIC: sub sp,sp,#0x80) + B addr+4. */
static uint64_t (*real_createvb)(uint32_t, const void *, uint32_t, int);
static uint32_t g_vb_fmt0 = 0; /* 0=sem remap; genv fix trata format-0 corretamente */
static int g_vb_log = 0;
static int g_vb_dump = 0;
static uint64_t my_createvb(uint32_t fmt, const void *v, uint32_t cnt, int fl) {
  if (fmt == 0 && getenv("DYSMANTLE_VB_CALLER")) {
    static uintptr_t seen_callers[16]; static int ns = 0;
    uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
    uintptr_t ra = (uintptr_t)__builtin_return_address(0) - lb;
    int known = 0;
    for (int i = 0; i < ns; i++) if (seen_callers[i] == ra) { known = 1; break; }
    if (!known && ns < 16) {
      seen_callers[ns++] = ra;
      fprintf(stderr, "[VBCALLER] NOVO caller=game@0x%lx (cnt=%u, total callers=%d)\n",
              (unsigned long)ra, cnt, ns);
    }
  }
  if (g_vb_dump && fmt == 0 && v && cnt >= 4) {
    static int dn = 0;
    if (dn < 6) {
      const unsigned char *b = (const unsigned char *)v;
      const float *f = (const float *)v;
      fprintf(stderr, "[VBDUMP#%d] fmt0 cnt=%u flags=%d\n", dn, cnt, fl);
      for (int row = 0; row < 8; row++) {
        fprintf(stderr, "  +%02d:", row * 16);
        for (int c = 0; c < 4; c++)
          fprintf(stderr, " %11.4f", f[row * 4 + c]);
        fprintf(stderr, "   |");
        for (int c = 0; c < 16; c++)
          fprintf(stderr, "%02x", b[row * 16 + c]);
        fprintf(stderr, "\n");
      }
      dn++;
    }
  }
  uint32_t use = (fmt == 0) ? g_vb_fmt0 : fmt;
  /* informa o stride real (do formato) ao imports.c p/ o auto-fix de stride no
   * glVertexAttribPointer. Mapa formato→stride da ConvertVertexFormatToVertexElements. */
  { extern void dysmantle_record_vbsize(long, int);
    int st = 0;
    switch (use) {
      case 0x1: case 0x61: st = 12; break;
      case 0x7: st = 24; break;
      case 0xf: case 0x6f: st = 28; break;
      case 0x1f: case 0x67: st = 32; break;
      case 0x7f: st = 40; break;
      default: st = 0;
    }
    if (st) dysmantle_record_vbsize((long)st * cnt, st);
  }
  uint64_t r = real_createvb(use, v, cnt, fl);
  if (g_vb_log) {
    static uint64_t seen = 0;
    int isfail = (r == 0);
    if (isfail || !((seen >> (use & 63)) & 1)) {
      fprintf(stderr, "[VB] fmt=0x%x(->0x%x) count=%u flags=%d -> %s\n", fmt,
              use, cnt, fl, isfail ? "FALHOU" : "ok");
      seen |= (1ULL << (use & 63));
    }
  }
  return r;
}
/* detour GenerateVerticesByFormat: loga estado da ModelSurface (count + 5
 * stream ptrs em this+64..96) p/ saber se streams existem mas format=0 */
static uint32_t surface_format(uint8_t *s);
static void (*real_genverts)(void *, uint32_t);
static void (*gen_streams_from_interleaved)(void *) = NULL; /* 0xa06cd4 */
static void my_genverts(void *self, uint32_t fmt) {
  uint8_t *s = (uint8_t *)self;
  uint64_t *p64 = (uint64_t *)(s + 64);
  /* 🌍 FIX RISCO: terreno/props chegam fmt=0 c/ streams NULOS mas têm dados
   * INTERLEAVED (this+224). Chama GenerateVertexStreamsFromInterleaved p/
   * popular os streams (this+64..96), depois recomputa o formato real e segue.
   * (DYSMANTLE_GENSTREAMS=1 liga; arriscado, pode crashar.) */
  if (fmt == 0 && getenv("DYSMANTLE_GENSTREAMS") && !p64[0] && gen_streams_from_interleaved) {
    uint8_t *arr = *(uint8_t **)(s + 224);
    int32_t nent = *(int32_t *)(s + 232);
    if (arr && nent > 0) {
      gen_streams_from_interleaved(self);  /* popula streams dos interleaved */
      uint32_t nf = surface_format(s);
      static int z = 0;
      if (z < 6) { fprintf(stderr, "[GENSTREAMS] fmt0 -> streams populados, novo fmt=0x%x (p0=%p)\n",
                           nf, (void *)p64[0]); z++; }
      if (nf) { real_genverts(self, nf); return; }
    }
  }
  /* 🌍 FIX MUNDO BRANCO: surfaces do chão chegam com fmt=0 mas TÊM streams
   * (pos/cor/uv/normal). Computa o formato real dos 5 ponteiros (mesma lógica
   * de GetVertexComponentFlagsAkaVertexFormat): bit0=pos bit3=cor bit1=uv
   * bit2=normal bit4=tangent. Sem isso createvb(0) falha → chão não desenha. */
  if (fmt == 0 && !getenv("DYSMANTLE_GENV_NOFIX")) {
    /* DYSMANTLE_GENV_FMT força um formato fixo (ex 7 = pos+cor+uv, 24B = o que
     * os atributos do render esperam). Sem env, computa dos streams. */
    const char *fe = getenv("DYSMANTLE_GENV_FMT");
    if (fe) { uint32_t ff = (uint32_t)atoi(fe); if (ff) { real_genverts(self, ff); return; } }
    uint32_t f = 0;
    if (p64[0]) f |= 0x1;  /* position */
    if (p64[1]) f |= 0x8;  /* color */
    if (p64[2]) f |= 0x2;  /* texcoord */
    if (p64[3]) f |= 0x4;  /* normal */
    if (p64[4]) f |= 0x10; /* tangent */
    if (f) fmt = f;
    else { /* streams TODOS nulos — não dá p/ computar formato */
      static int zn = 0;
      if (zn < 6) {
        int32_t count = *(int32_t *)(s + 56);
        fprintf(stderr, "[GENV-NULLSTREAMS] fmt 0 count=%d this+232=%d (sem streams!)\n",
                count, *(int32_t *)(s + 232)); zn++;
      }
    }
  }
  real_genverts(self, fmt);
}
/* helper: computa formato real de uma ModelSurface a partir dos 5 stream ptrs */
static uint32_t surface_format(uint8_t *s) {
  uint64_t *p = (uint64_t *)(s + 64);
  uint32_t f = 0;
  if (p[0]) f |= 0x1; if (p[1]) f |= 0x8; if (p[2]) f |= 0x2;
  if (p[3]) f |= 0x4; if (p[4]) f |= 0x10;
  return f;
}
/* detour InitializeVertexAndIndexBuffers: corrige o campo formato (entry+0) das
 * entradas com formato=0 ANTES da função rodar (computa dos streams da surface). */
static void (*real_initbufs)(void *);
static void my_initbufs(void *self) {
  if (getenv("DYSMANTLE_INITBUF_DUMPALL")) {
    uint8_t *s = (uint8_t *)self;
    uint8_t *arr = *(uint8_t **)(s + 224);
    int32_t cnt = *(int32_t *)(s + 232);
    static int dn = 0;
    if (arr && dn < 8) {
      fprintf(stderr, "[ENTRIES] surface=%p nEntries=%d streams: %p %p %p %p %p\n",
              self, cnt, *(void**)(s+64),*(void**)(s+72),*(void**)(s+80),
              *(void**)(s+88),*(void**)(s+96));
      for (int i = 0; i < cnt && i < 6; i++) {
        uint8_t *e = arr + (size_t)i * 0x20;
        uint64_t v8 = *(uint64_t *)(e + 8);
        uint64_t first = v8 ? *(uint64_t *)v8 : 0;
        fprintf(stderr, "  entry%d: fmt@0=0x%x @4=0x%x verts@8=0x%lx buf@16=0x%lx [v8]:0x%lx\n",
                i, *(uint32_t*)e, *(uint32_t*)(e+4), (unsigned long)v8,
                (unsigned long)*(uint64_t*)(e+16), (unsigned long)first);
      }
      dn++;
    }
  }
  if (!getenv("DYSMANTLE_INITBUF_NOFIX")) {
    uint8_t *s = (uint8_t *)self;
    uint8_t *arr = *(uint8_t **)(s + 224);
    int32_t cnt = *(int32_t *)(s + 232);
    uint32_t real = surface_format(s);
    /* DIAG mundo-branco: p/ surface com entry fmt=0, loga o SHADER do material
     * (surface+48=mat, mat+168=effect, effect+48=shader, shader+0=nome,
     * shader+44=fmt). Hipótese: shader zerado (nunca carregado) -> fmt 0. */
    if (arr && cnt > 0 && *(uint32_t *)arr == 0) {
      static void *seensh[32]; static int nsh = 0;
      uint8_t *mat = *(uint8_t **)(s + 48);
      uint8_t *eff = mat ? *(uint8_t **)(mat + 168) : NULL;
      uint8_t *sh = eff ? *(uint8_t **)(eff + 48) : NULL;
      int known = 0;
      for (int i = 0; i < nsh; i++) if (seensh[i] == (void *)sh) { known = 1; break; }
      if (!known && nsh < 32) {
        seensh[nsh++] = (void *)sh;
        fprintf(stderr, "[BROKENSURF] mat=%p eff=%p sh=%p name='%s' shfmt=0x%x\n",
                (void *)mat, (void *)eff, (void *)sh,
                sh && *(char **)sh ? *(char **)sh : "?",
                sh ? *(uint32_t *)(sh + 44) : 0xdead);
      }
    }
    if (arr) {
      for (int i = 0; i < cnt && i < 64; i++) {
        uint32_t *e = (uint32_t *)(arr + (size_t)i * 0x20);
        /* e[0]=formato usado pelo createvb (=0, bug); e[1]@offset4=formato REAL
         * (ex 0x7F). Corrige e[0] := e[1] (ou streams, fallback). */
        if (e[0] == 0) {
          uint32_t fix = e[1] ? e[1] : real;
          static int z = 0;
          if (z < 3) {
            uint64_t *e64 = (uint64_t *)(arr + (size_t)i * 0x20);
            const float *v = (const float *)e64[1];
            fprintf(stderr, "[INITBUF-FIX] i=%d e[0]->0x%x (e[1]=0x%x) cnt56=%d verts:\n",
                    i, fix, e[1], *(int32_t *)(s + 56));
            if (v) for (int r2 = 0; r2 < 6; r2++)
              fprintf(stderr, "   +%02d: %11.4f %11.4f %11.4f %11.4f | %08x %08x %08x %08x\n",
                      r2*16, v[r2*4],v[r2*4+1],v[r2*4+2],v[r2*4+3],
                      ((uint32_t*)v)[r2*4],((uint32_t*)v)[r2*4+1],((uint32_t*)v)[r2*4+2],((uint32_t*)v)[r2*4+3]);
            z++;
          }
          /* OFF por default: verts@8 é OBJETO (não float cru) → e[0]=fmt gera
           * buffer de lixo. DYSMANTLE_INITBUF_FIX=1 reativa p/ experimentar. */
          if (fix && getenv("DYSMANTLE_INITBUF_FIX")) e[0] = fix;
        }
      }
    }
  }
  real_initbufs(self);
}
static void hook_initbufs(void) {
  uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
  uintptr_t addr = lb + 0xa03a70;
  uint32_t *tr = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  tr[0] = *(uint32_t *)addr; tr[1] = 0x58000051u; tr[2] = 0xd61f0220u;
  *(uint64_t *)&tr[3] = addr + 4;
  __builtin___clear_cache((char *)tr, (char *)tr + 32);
  real_initbufs = (void (*)(void *))tr;
  so_make_text_writable(); hook_arm64(addr, (uintptr_t)my_initbufs);
  so_make_text_executable(); so_flush_caches();
  fprintf(stderr, "hook_initbufs: detour @ %p\n", (void *)addr);
}
/* detour de StageImporter::AddActorFromNode(DMNode*, const nTransform&) — o TOPO
 * da criação de um ator. Arma sigsetjmp; se QUALQUER coisa na criação do ator
 * crashar (incl. o skinned InitializeFromModel), o handler dá longjmp de volta
 * aqui → o ATOR INTEIRO é pulado (não meio-criado), o mundo carrega sem crash. */
static void *(*real_aafn)(void *, void *, void *);
static void *my_aafn(void *self, void *node, void *xform) {
  if (g_ifm_armed) return real_aafn(self, node, xform); /* reentrante */
  void *r = NULL;
  if (sigsetjmp(g_ifm_jmp, 1) == 0) {
    g_ifm_armed = 1;
    r = real_aafn(self, node, xform);
    g_ifm_armed = 0;
  } else {
    g_ifm_armed = 0; r = NULL; /* ator pulado */
  }
  return r;
}
static void hook_ifm(void) {
  uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
  uintptr_t addr = lb + 0xd30b78; /* StageImporter::AddActorFromNode */
  uint32_t *tr = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  tr[0] = *(uint32_t *)addr; tr[1] = 0x58000051u; tr[2] = 0xd61f0220u;
  *(uint64_t *)&tr[3] = addr + 4;
  __builtin___clear_cache((char *)tr, (char *)tr + 32);
  real_aafn = (void *(*)(void *, void *, void *))tr;
  so_make_text_writable(); hook_arm64(addr, (uintptr_t)my_aafn);
  so_make_text_executable(); so_flush_caches();
  fprintf(stderr, "hook_ifm: detour AddActorFromNode @ %p\n", (void *)addr);
}
static void hook_genverts(void) {
  uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
  uintptr_t addr = lb + 0xa025b8;
  uint32_t *tr = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  tr[0] = *(uint32_t *)addr; tr[1] = 0x58000051u; tr[2] = 0xd61f0220u;
  *(uint64_t *)&tr[3] = addr + 4;
  __builtin___clear_cache((char *)tr, (char *)tr + 32);
  real_genverts = (void (*)(void *, uint32_t))tr;
  gen_streams_from_interleaved = (void (*)(void *))so_find_addr_safe(
      "_ZN12ModelSurface52GenerateVertexStreamsFromInterleavedVerticesByFormatEv");
  so_make_text_writable(); hook_arm64(addr, (uintptr_t)my_genverts);
  so_make_text_executable(); so_flush_caches();
  fprintf(stderr, "hook_genverts: detour @ %p gen_streams=%p\n", (void *)addr,
          (void *)gen_streams_from_interleaved);
}
/* 🌍 CAUSA-RAIZ DO MUNDO BRANCO (sessão 2026-06-12): detour no helper interno
 * de NXI_LoadShaderWithPreCompiledCode @0x487990 (args: x0=nx_shader_t*,
 * x1=ShaderInfo*). Ele copia info+388 -> shader+44, e shader+44 é usado por
 * ModelBuilderSurfaceBased::AddBuiltModelSurfacesToModel como o VERTEX FORMAT
 * das surfaces construídas do mundo (chão/props). info+388 = mask de targets
 * do XML do shader (atributo "target"; default 0x7F quando ausente) — 0x7F
 * coincide com o formato full 40B, por isso funciona no Android. No nosso
 * ambiente chega 0 -> entries fmt=0 -> createvb falha -> mundo invisível.
 * FIX: se info+388==0, força 0x7F antes do helper rodar (o builder então
 * aloca/escreve vértices 40B corretos). DYSMANTLE_SHADERFMT_OFF desliga;
 * log dos primeiros shaders + de todos com fmt 0. */
static void (*real_shaderload)(void *, void *);
static void my_shaderload(void *shader, void *infov) {
  uint8_t *info = (uint8_t *)infov;
  uint32_t fmt = *(uint32_t *)(info + 388);
  const char *name = *(const char **)(info + 232);
  static int n = 0;
  if (fmt == 0 || n < 8) {
    fprintf(stderr, "[SHADERFMT] '%s' fmt=0x%x ver=0x%x%s\n",
            name ? name : "?", fmt, *(uint32_t *)(info + 392),
            (fmt == 0 && !getenv("DYSMANTLE_SHADERFMT_OFF")) ? " -> 0x7f" : "");
    n++;
  }
  if (fmt == 0 && !getenv("DYSMANTLE_SHADERFMT_OFF"))
    *(uint32_t *)(info + 388) = 0x7f;
  real_shaderload(shader, infov);
}
/* 🌍 FIX B (mundo branco, GLES2-friendly): os shaders *Shadows têm
 * feature_level=2 (mask 0x7E) e são PULADOS no target GL mais baixo →
 * shader+44=0 → chão/pedras/lixeiras/árvores não desenham. Alias: redireciona
 * "...Shadows.xml" pra variante sem sombra (mesmo material, sem shadow map).
 * DYSMANTLE_KEEP_SHADOW_SHADERS=1 desliga o alias. */
static void *(*real_getshader)(const char *);
/* remove a 1ª ocorrência de tok em s (in place); 1 se removeu */
static int shname_strip(char *s, const char *tok) {
  char *p = strstr(s, tok);
  if (!p) return 0;
  memmove(p, p + strlen(tok), strlen(p + strlen(tok)) + 1);
  return 1;
}
static void *my_getshader(const char *name) {
  void *sh = real_getshader(name);
  /* fallback: shader pediu features acima do target GL atual (feature_level=2,
   * ex *Shadows) -> fmt 0 -> não desenharia. Degrada o nome progressivamente
   * até uma variante que carrega. */
  if (sh && name && strlen(name) < 240 &&
      *(uint32_t *)((uint8_t *)sh + 44) == 0 &&
      !getenv("DYSMANTLE_KEEP_SHADOW_SHADERS")) {
    char cur[256];
    strcpy(cur, name);
    static const char *toks[] = {"Shadows", "Reflections", "Heights",
                                 "Specular", "Normals", "Glow"};
    void *best = NULL; const char *how = NULL;
    for (unsigned i = 0; i < sizeof(toks) / sizeof(*toks) && !best; i++) {
      if (!shname_strip(cur, toks[i])) continue;
      void *a = real_getshader(cur);
      if (a && *(uint32_t *)((uint8_t *)a + 44) != 0) { best = a; how = toks[i]; }
    }
    if (!best) { /* Fur -> Diffuse (SkinnedLitFur etc.) */
      char *f = strstr(cur, "Fur");
      if (f) {
        char tmp[256]; size_t pre = (size_t)(f - cur);
        memcpy(tmp, cur, pre);
        snprintf(tmp + pre, sizeof(tmp) - pre, "Diffuse%s", f + 3);
        void *a = real_getshader(tmp);
        if (a && *(uint32_t *)((uint8_t *)a + 44) != 0) { best = a; how = "Fur"; strcpy(cur, tmp); }
      }
    }
    if (!best && shname_strip(cur, "Lit")) { /* TagWaterLit->TagWater, BillboardLit->Billboard */
      void *a = real_getshader(cur);
      if (a && *(uint32_t *)((uint8_t *)a + 44) != 0) { best = a; how = "Lit"; }
    }
    if (best) {
      static int z = 0;
      if (z < 48) { fprintf(stderr, "[SHALIAS] '%s' -> '%s' (-%s, fmt=0x%x)\n",
                            name, cur, how, *(uint32_t *)((uint8_t *)best + 44)); z++; }
      return best;
    }
    fprintf(stderr, "[SHALIAS] '%s' SEM fallback (fica fmt=0)\n", name);
  }
  static char seen[64][96]; static int ns = 0;
  if (name && sh) {
    for (int i = 0; i < ns; i++) if (!strncmp(seen[i], name, 95)) return sh;
    if (ns < 64) { strncpy(seen[ns], name, 95); ns++; }
    uint32_t fmt = *(uint32_t *)((uint8_t *)sh + 44);
    fprintf(stderr, "[GETSHADER] '%s' -> %p fmt=0x%x\n", name, sh, fmt);
    if (fmt == 0) {
      /* erro do TranslatorDriver: global [0xdc96a8] -> +0x448b8 = driver;
       * driver+128 = char* da última mensagem de erro (ReadShaderXML/Translate) */
      uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
      uint8_t *gd = *(uint8_t **)(lb + 0xdc96a8);
      uint8_t *drv = gd ? *(uint8_t **)(gd + 0x448b8) : NULL;
      const char *err = drv ? *(const char **)(drv + 128) : NULL;
      fprintf(stderr, "[GETSHADER]   erro do translator: '%s'\n",
              err && err[0] ? err : "(vazio)");
    }
  }
  return sh;
}
/* 🌍 FIX A (alternativo, X5M/ES3): sobe o ShaderTarget do renderer
 * (renderer+1120, lido por GetShaderTarget) ANTES do translator inicializar.
 * DYSMANTLE_SHTARGET=2 = tier ES3 → shaders feature_level=2 (mask 0x7E)
 * passam a carregar. GOT-hook em ShaderUtility::InitializeTranslator. */
static int (*orig_inittrans)(void *) = NULL;
static int my_inittrans(void *self) {
  uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
  void **g = *(void ***)(lb + 0xdc9700); /* [dc9000+1792] -> obj; [obj]=renderer */
  uint8_t *ren = g ? (uint8_t *)*g : NULL;
  if (ren) {
    uint32_t cur = *(uint32_t *)(ren + 1120);
    const char *e = getenv("DYSMANTLE_SHTARGET");
    fprintf(stderr, "[SHTARGET] renderer=%p target=%u%s\n", (void *)ren, cur,
            e ? " (override)" : "");
    if (e) *(uint32_t *)(ren + 1120) = (uint32_t)atoi(e);
  }
  return orig_inittrans ? orig_inittrans(self) : 0;
}
static void hook_inittrans(void) {
  uintptr_t slot = so_find_rel_addr_safe("_ZN8Graphics13ShaderUtility20InitializeTranslatorEv");
  if (!slot) { fprintf(stderr, "hook: GOT InitializeTranslator não achado\n"); return; }
  uintptr_t *p = (uintptr_t *)slot;
  orig_inittrans = (int (*)(void *))*p;
  *p = (uintptr_t)my_inittrans;
  fprintf(stderr, "hook: InitializeTranslator GOT %p\n", (void *)slot);
}
static void hook_getshader(void) {
  uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
  uintptr_t addr = lb + 0x4846b0;
  uint32_t *tr = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  tr[0] = *(uint32_t *)addr; tr[1] = 0x58000051u; tr[2] = 0xd61f0220u;
  *(uint64_t *)&tr[3] = addr + 4;
  __builtin___clear_cache((char *)tr, (char *)tr + 32);
  real_getshader = (void *(*)(const char *))tr;
  so_make_text_writable(); hook_arm64(addr, (uintptr_t)my_getshader);
  so_make_text_executable(); so_flush_caches();
  fprintf(stderr, "hook_getshader: detour @ %p\n", (void *)addr);
}
static void hook_shaderload(void) {
  uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
  uintptr_t addr = lb + 0x487990; /* stp x29,x30,[sp,#-96]! (PIC) */
  uint32_t *tr = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  tr[0] = *(uint32_t *)addr; tr[1] = 0x58000051u; tr[2] = 0xd61f0220u;
  *(uint64_t *)&tr[3] = addr + 4;
  __builtin___clear_cache((char *)tr, (char *)tr + 32);
  real_shaderload = (void (*)(void *, void *))tr;
  so_make_text_writable(); hook_arm64(addr, (uintptr_t)my_shaderload);
  so_make_text_executable(); so_flush_caches();
  fprintf(stderr, "hook_shaderload: detour @ %p\n", (void *)addr);
}
static void hook_createvb(void) {
  uintptr_t lb = so_find_addr("android_main") - 0x4651a4;
  uintptr_t addr = lb + 0x4837d4;
  uint32_t *tr = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (tr == MAP_FAILED) { fprintf(stderr, "hook_createvb: mmap falhou\n"); return; }
  tr[0] = *(uint32_t *)addr;   /* sub sp,sp,#0x80 (PIC) */
  tr[1] = 0x58000051u;         /* ldr x17, #8 */
  tr[2] = 0xd61f0220u;         /* br x17 */
  *(uint64_t *)&tr[3] = addr + 4;
  __builtin___clear_cache((char *)tr, (char *)tr + 32);
  real_createvb = (uint64_t(*)(uint32_t, const void *, uint32_t, int))tr;
  so_make_text_writable();
  hook_arm64(addr, (uintptr_t)my_createvb);
  so_make_text_executable();
  so_flush_caches();
  fprintf(stderr, "hook_createvb: detour @ %p tramp=%p\n", (void *)addr, (void *)tr);
}

/* GOT-hooks BitmapLoader: nome do bitmap + resultado do LoadBitmapInternal
 * (caça do mundo-branco: 'tile-floor-normals.jpg' falha; queremos o passo). */
static char g_last_bmp_name[256] = "?";
static void (*orig_setbmpname)(void *, const char *, size_t) = NULL;
static void my_setbmpname(void *bmp, const char *p, size_t n) {
  if (p && n > 0 && n < sizeof(g_last_bmp_name)) {
    memcpy(g_last_bmp_name, p, n); g_last_bmp_name[n] = 0;
  }
  if (orig_setbmpname) orig_setbmpname(bmp, p, n);
}
static unsigned char (*orig_loadbmpint)(void *, int, int, unsigned) = NULL;
static unsigned char my_loadbmpint(void *self, int a, int b, unsigned c) {
  unsigned char r = orig_loadbmpint ? orig_loadbmpint(self, a, b, c) : 0;
  static int n = 0;
  if (!r || n < 12) {
    fprintf(stderr, "[BMP] LoadBitmapInternal('%s', %d, %d, %u) -> %d\n",
            g_last_bmp_name, a, b, c, r);
    n++;
  }
  return r;
}
static void hook_bitmaploader(void) {
  uintptr_t s1 = so_find_rel_addr_safe(
      "_ZN12BitmapLoader13SetBitmapNameER11nx_bitmap_tNSt6__ndk117basic_string_viewIcNS2_11char_traitsIcEEEE");
  if (s1) { uintptr_t *p = (uintptr_t *)s1; orig_setbmpname = (void *)*p; *p = (uintptr_t)my_setbmpname; }
  uintptr_t s2 = so_find_rel_addr_safe("_ZN12BitmapLoader18LoadBitmapInternalEbbj");
  if (s2) { uintptr_t *p = (uintptr_t *)s2; orig_loadbmpint = (void *)*p; *p = (uintptr_t)my_loadbmpint; }
  fprintf(stderr, "hook: BitmapLoader SetName=%p LoadInternal=%p\n", (void *)s1, (void *)s2);
}

/* GOT-hooks NX_FileSystem: rastreia open/read do tile-floor (mundo-branco) */
static void *g_watch_file = NULL;
static void *(*orig_nxopen)(const char *, const char *) = NULL;
static void *my_nxopen(const char *path, const char *mode) {
  void *f = orig_nxopen ? orig_nxopen(path, mode) : NULL;
  if (path && strstr(path, "tile-floor")) {
    fprintf(stderr, "[NXFS] OpenFile('%s','%s') -> %p\n", path, mode ? mode : "?", f);
    if (f) {
      g_watch_file = f;
      uint64_t *w = (uint64_t *)f;
      for (int i = 0; i < 10; i++)
        fprintf(stderr, "  f[%d]=0x%016lx\n", i, (unsigned long)w[i]);
    }
  }
  return f;
}
static unsigned long (*orig_nxread)(void *, size_t, void *) = NULL;
static unsigned long my_nxread(void *buf, size_t n, void *file) {
  unsigned long r = orig_nxread ? orig_nxread(buf, n, file) : 0;
  if (file && file == g_watch_file) {
    unsigned char *b = buf;
    fprintf(stderr, "[NXFS] ReadFile(n=%zu, f=%p) -> %lu  b=%02x %02x %02x %02x\n",
            n, file, r, r > 0 ? b[0] : 0, r > 1 ? b[1] : 0, r > 2 ? b[2] : 0, r > 3 ? b[3] : 0);
  }
  return r;
}
static void hook_nxfs(void) {
  uintptr_t s1 = so_find_rel_addr_safe("_Z22NX_FileSystem_OpenFilePKcS0_");
  if (s1) { uintptr_t *p = (uintptr_t *)s1; orig_nxopen = (void *)*p; *p = (uintptr_t)my_nxopen; }
  uintptr_t s2 = so_find_rel_addr_safe("_Z22NX_FileSystem_ReadFilePvmP9nx_file_t");
  if (s2) { uintptr_t *p = (uintptr_t *)s2; orig_nxread = (void *)*p; *p = (uintptr_t)my_nxread; }
  fprintf(stderr, "hook: NXFS Open=%p Read=%p\n", (void *)s1, (void *)s2);
}

/* GOT-hook Paddleboat_getControllerData: loga se/quando a engine lê o pad */
static int32_t (*orig_pb_getdata)(int32_t, void *) = NULL;
static int32_t my_pb_getdata(int32_t idx, void *data) {
  int32_t r = orig_pb_getdata ? orig_pb_getdata(idx, data) : 1;
  static int n = 0;
  if (n < 5 || (n % 120) == 0) {
    uint32_t buttons = data ? *(uint32_t *)((char *)data + 8) : 0;
    fprintf(stderr, "[PBdata] #%d idx=%d ret=%d buttons=0x%x\n", n, idx, r,
            buttons);
  }
  n++;
  /* injetor remoto: `echo MASK > /dev/shm/dys_btn` OR-a a mascara Paddleboat
   * nos botoes por ~20 leituras (navegacao de menu via ssh, estilo sonda). */
  if (data) {
    static int hold = 0; static uint32_t held_mask = 0; static int chk = 0;
    if (hold > 0) {
      *(uint32_t *)((char *)data + 8) |= held_mask;
      if (--hold == 0) fprintf(stderr, "[inj] solta 0x%x\n", held_mask);
    } else if (++chk % 10 == 0) {
      FILE *f = fopen("/dev/shm/dys_btn", "r");
      if (f) {
        unsigned m = 0;
        if (fscanf(f, "%x", &m) == 1 && m) {
          held_mask = m; hold = 20;
          fprintf(stderr, "[inj] segura 0x%x\n", m);
        }
        fclose(f); unlink("/dev/shm/dys_btn");
      }
    }
  }
  return r;
}
static void hook_pb_getdata(void) {
  uintptr_t slot = so_find_rel_addr_safe("Paddleboat_getControllerData");
  if (!slot) { fprintf(stderr, "hook: GOT Paddleboat_getControllerData não achado\n"); return; }
  uintptr_t *p = (uintptr_t *)slot;
  orig_pb_getdata = (int32_t(*)(int32_t, void *))*p;
  *p = (uintptr_t)my_pb_getdata;
  fprintf(stderr, "hook: Paddleboat_getControllerData GOT %p\n", (void *)slot);
}

/* GOT-hook de NXI_GetProductValue -> força opengl_version="2.0" (caminho ES2) */
static const char *(*orig_getprod)(const char *) = NULL;
static const char *my_getprod(const char *key) {
  const char *r = orig_getprod ? orig_getprod(key) : NULL;
  if (key && strcmp(key, "opengl_version") == 0) {
    const char *forced = getenv("DYSMANTLE_GLVER");
    if (!forced) forced = "2.0";
    if (!forced[0]) { /* DYSMANTLE_GLVER="" → deixa o valor real (null) */
      fprintf(stderr, "[cfg] opengl_version real='%s' (não forçado)\n", r ? r : "(null)");
      return r;
    }
    fprintf(stderr, "[cfg] opengl_version real='%s' -> forçando '%s'\n",
            r ? r : "(null)", forced);
    return forced;
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

/* [PERFCPU] sampler de CPU por thread: a cada 5s lê /proc/self/task/STAT e
 * imprime as 6 threads que mais usaram CPU no intervalo (% de 1 core).
 * Diagnóstico do lag — acha busy-spin/thread moedora. */
static void *perfcpu_thread(void *arg) {
  (void)arg;
  struct ent { long tid; char name[20]; unsigned long long ticks; };
  static struct ent prev[256]; int nprev = 0;
  long hz = sysconf(_SC_CLK_TCK);
  for (;;) {
    sleep(5);
    DIR *d = opendir("/proc/self/task");
    if (!d) continue;
    struct ent cur[256]; int nc = 0;
    struct dirent *de;
    while ((de = readdir(d)) && nc < 256) {
      if (de->d_name[0] == '.') continue;
      char path[64];
      snprintf(path, sizeof(path), "/proc/self/task/%s/stat", de->d_name);
      FILE *f = fopen(path, "r");
      if (!f) continue;
      long tid; char comm[20] = ""; char st;
      unsigned long ut = 0, stm = 0;
      if (fscanf(f, "%ld (%19[^)]) %c", &tid, comm, &st) == 3) {
        for (int k = 0; k < 11; k++) fscanf(f, "%*s");
        fscanf(f, "%lu %lu", &ut, &stm);
        cur[nc].tid = tid; cur[nc].ticks = (unsigned long long)ut + stm;
        strncpy(cur[nc].name, comm, 19); cur[nc].name[19] = 0; nc++;
      }
      fclose(f);
    }
    closedir(d);
    /* delta vs prev, top 6 */
    struct { double pct; char name[20]; long tid; } top[6] = {{0}};
    for (int i = 0; i < nc; i++) {
      unsigned long long old = 0;
      for (int j = 0; j < nprev; j++)
        if (prev[j].tid == cur[i].tid) { old = prev[j].ticks; break; }
      double pct = (double)(cur[i].ticks - old) * 100.0 / (hz * 5.0);
      for (int s = 0; s < 6; s++)
        if (pct > top[s].pct) {
          for (int m = 5; m > s; m--) top[m] = top[m - 1];
          top[s].pct = pct; top[s].tid = cur[i].tid;
          strncpy(top[s].name, cur[i].name, 19); top[s].name[19] = 0;
          break;
        }
    }
    char line[256]; int lp = 0; line[0] = 0;
    for (int s = 0; s < 6 && top[s].pct > 0.5 && lp < 220; s++)
      lp += snprintf(line + lp, sizeof(line) - lp, " %s(%ld)=%.0f%%",
                     top[s].name, top[s].tid, top[s].pct);
    fprintf(stderr, "[PERFCPU]%s\n", line);
    memcpy(prev, cur, sizeof(cur[0]) * nc); nprev = nc;
  }
  return NULL;
}
static void start_perfcpu(void) {
  if (getenv("DYSMANTLE_NOPERF")) return;
  pthread_t t;
  pthread_create(&t, NULL, perfcpu_thread, NULL);
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

  /* 🔑 NX_Graphics_IsTextureFormatSupported: ANTES forçávamos →1 p/ a engine
   * carregar o .ktx (ETC2) das texturas de UI cujo .jpg vinha VAZIO. PORÉM isso
   * faz a engine carregar ETC2 p/ TUDO (mundo) → Mali-450 não amostra ETC2 →
   * MUNDO BRANCO. Agora que os .jpg vazios já foram preenchidos nos paks
   * (fix_empty_textures.py), deixamos a engine ver "ETC2 não suportado" e
   * carregar JPEG/PNG (que o Mali amostra). DYSMANTLE_FORCE_ETC2=1 reativa. */
  if (getenv("DYSMANTLE_FORCE_ETC2"))
    patch_func_ret1("_Z36NX_Graphics_IsTextureFormatSupportedRK22nx_bitmap_parameters_t");

  /* GOT-hook NXI_GetProductValue: a engine lê "opengl_version" do config; se
   * pedir ES3 ela monta o APIManager ES3 (mais funções) num buffer de pilha
   * dimensionado p/ ES2 -> stack smash no nosso contexto Utgard (ES2).
   * Forçamos "2.0" p/ alinhar a engine ao caminho ES2. */
  hook_getproductvalue();
  hook_bitmaploader(); /* caça mundo-branco: nome+resultado de cada bitmap */
  hook_nxfs(); /* rastreia open/read do tile-floor no VFS */
  /* 🎮 Paddleboat MODO CONSOLE: o FrameStart da engine só chama
   * getControllerData quando o flag dirty (impl+64) está setado, e esse
   * flag depende do ciclo de eventos GameActivity (que no nosso shim não
   * casa o timing). NOP no `cbz` que pula a leitura → a engine LÊ o pad
   * TODO frame (como um console faria). Casa com onControllerConnected
   * em slot 0. (env DYSMANTLE_PB_NOFORCE desliga p/ debug.) */
  if (!getenv("DYSMANTLE_PB_NOFORCE"))
    patch_vaddr(0x46f53c, 0xd503201f); /* nop o cbz dirty-check */
  hook_pb_getdata(); /* sempre: injetor de botoes remoto + diagnostico */
  /* 🌍 fix do mundo branco: a engine pede vertex buffers com format=0 (o
   * converter Legacy não trata → "unknown vertex format" → malha não sobe).
   * Remapeamos format 0 → um layout 2D válido (default 0x7 = pos+uv+cor).
   * Ajustável s/ rebuild: DYSMANTLE_VB_FMT0=N. DYSMANTLE_VB_LOG=1 loga. */
  { const char *f = getenv("DYSMANTLE_VB_FMT0"); if (f) g_vb_fmt0 = atoi(f); }
  g_vb_log = getenv("DYSMANTLE_VB_LOG") ? 1 : 0;
  g_vb_dump = getenv("DYSMANTLE_VB_DUMP") ? 1 : 0;
  hook_genverts(); /* fix do mundo branco: fmt 0 → formato real dos streams */
  hook_initbufs();
  hook_createvb();
  hook_shaderload(); /* 🌍 fix mundo branco: shader fmt 0 -> 0x7f (causa-raiz) */
  hook_getshader();  /* 🌍 diag + FIX B: alias *Shadows.xml -> variante s/ sombra */
  hook_inittrans();  /* 🌍 diag + FIX A: log/override do ShaderTarget */
  start_perfcpu();   /* [PERFCPU] sampler de CPU por thread (diag lag) */
  /* skip do skinned-actor: NÃO é a solução real (o crash vem de config LOW;
   * reverter as configs pro default resolve). Fica como rede de segurança
   * opcional via DYSMANTLE_SKIP_BADACTORS=1 (corrompe o importer, usar só p/
   * emergência). Default OFF p/ comportamento normal. */
  if (getenv("DYSMANTLE_SKIP_BADACTORS")) hook_ifm();
  if (getenv("DYSMANTLE_PB_TRAPS")) install_brk_traps();

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
