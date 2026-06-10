/*
 * main.c — loader ARMHF (gerado por new-port-arm.sh) p/ nfstest.
 *
 * Multi-módulo bionic→glibc: carrega as dependências (libc++_shared.so libfmodevent.so libfmodex.so libNimble.so) cada uma
 * no seu heap + so_snapshot_symbols, acumula numa tabela combinada, e carrega o
 * módulo principal (libapp.so) resolvendo contra tudo + fallback dlsym. Acha
 * o entry (JNI_OnLoad / android_main). F2+ = boot do engine (JNI/NativeActivity).
 */
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "android_shim.h"
#include "error.h"
#include "imports.h"
#include "jni_shim.h"
#include "so_util.h"
#include "util.h"

#define MEMORY_MB 320
#define SO_NAME "libapp.so"

/* ---- crash handler ARMHF (campos arm_pc/arm_r0/arm_lr do sigcontext 32-bit) ---- */
static void crash_handler(int sig, siginfo_t *info, void *uctx) {
  ucontext_t *uc = (ucontext_t *)uctx;
  mcontext_t *m = &uc->uc_mcontext;
  uintptr_t pc = m->arm_pc, lr = m->arm_lr, fault = (uintptr_t)info->si_addr;
  uintptr_t text = (uintptr_t)text_base;

  fprintf(stderr, "\n=== CRASH sig=%d addr=%p ===\n", sig, (void *)fault);
  fprintf(stderr, "  PC=%p", (void *)pc);
  if (pc >= text && pc < text + text_size)
    fprintf(stderr, " (%s+0x%lx)", SO_NAME, (unsigned long)(pc - text));
  fprintf(stderr, "\n  LR=%p", (void *)lr);
  if (lr >= text && lr < text + text_size)
    fprintf(stderr, " (%s+0x%lx)", SO_NAME, (unsigned long)(lr - text));
  fprintf(stderr, "\n");
  fprintf(stderr, "  r0=%08lx r1=%08lx r2=%08lx r3=%08lx\n",
          (unsigned long)m->arm_r0, (unsigned long)m->arm_r1,
          (unsigned long)m->arm_r2, (unsigned long)m->arm_r3);
  fprintf(stderr, "  r4=%08lx r5=%08lx r6=%08lx r7=%08lx\n",
          (unsigned long)m->arm_r4, (unsigned long)m->arm_r5,
          (unsigned long)m->arm_r6, (unsigned long)m->arm_r7);
  fprintf(stderr, "  r8=%08lx r9=%08lx r10=%08lx fp=%08lx ip=%08lx sp=%08lx\n",
          (unsigned long)m->arm_r8, (unsigned long)m->arm_r9,
          (unsigned long)m->arm_r10, (unsigned long)m->arm_fp,
          (unsigned long)m->arm_ip, (unsigned long)m->arm_sp);

  /* backtrace simples via FP chain (ARM: [fp-4]=lr salvo, [fp-8]=fp ant.;
   * varia por -fomit-frame-pointer, então é best-effort) */
  fprintf(stderr, "  --- stack scan (retornos no .so) ---\n");
  uintptr_t sp = m->arm_sp;
  int n = 0;
  for (uintptr_t a = sp; a < sp + 0x2000 && n < 24; a += 4) {
    uintptr_t v = *(uintptr_t *)a;
    if (v >= text && v < text + text_size) {
      fprintf(stderr, "    [sp+0x%lx] %s+0x%lx\n", (unsigned long)(a - sp),
              SO_NAME, (unsigned long)(v - text));
      n++;
    }
  }
  fprintf(stderr, "=== END CRASH ===\n");
  fflush(stderr);
  _exit(128 + sig);
}

static void install_crash_handler(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = crash_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);
}

/* tabela combinada acumulada (base + snapshots dos módulos já carregados) */
static DynLibFunction *g_comb;
static int g_comb_n;
static void comb_append(DynLibFunction *tbl, int n) {
  g_comb = realloc(g_comb, sizeof(DynLibFunction) * (g_comb_n + n));
  memcpy(g_comb + g_comb_n, tbl, sizeof(DynLibFunction) * n);
  g_comb_n += n;
}

/* carrega 1 módulo no seu próprio heap, reloca, resolve contra a tabela
 * combinada (+ fallback dlsym no so_resolve) e, se snapshot!=0, acumula os
 * símbolos exportados na combinada p/ os módulos seguintes. */
static int load_module(const char *name, int heap_mb, int snapshot) {
  size_t hs = (size_t)heap_mb * 1024 * 1024;
  void *heap = mmap(NULL, hs, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap == MAP_FAILED) { fprintf(stderr, "mmap %s falhou\n", name); return -1; }
  debugPrintf("--- %s (heap %p, %d MB) ---\n", name, heap, heap_mb);
  if (so_load(name, heap, hs) < 0) { fprintf(stderr, "so_load %s falhou\n", name); return -1; }
  if (so_relocate() < 0) { fprintf(stderr, "so_relocate %s falhou\n", name); return -1; }
  if (so_resolve(g_comb ? g_comb : dynlib_functions,
                 g_comb ? g_comb_n : (int)dynlib_numfunctions, 0) < 0) {
    fprintf(stderr, "so_resolve %s falhou\n", name); return -1;
  }
  so_finalize();
  so_flush_caches();
  if (snapshot) {
    int n = 0;
    DynLibFunction *t = so_snapshot_symbols(&n);
    if (t && n > 0) { comb_append(t, n); debugPrintf("%s: +%d símbolos exportados\n", name, n); }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  install_crash_handler();
  debugPrintf("=== nfstest — loader ARMHF (Mali-450) ===\n");

  /* base = shims bionic→glibc (os 18 que o dlsym fallback não cobre) */
  extern DynLibFunction port_shims[];
  extern int port_shims_count;
  g_comb = malloc(sizeof(DynLibFunction) * port_shims_count);
  memcpy(g_comb, port_shims, sizeof(DynLibFunction) * port_shims_count);
  g_comb_n = port_shims_count;

  /* dependências primeiro — cada uma vira fonte de símbolos p/ as seguintes.
   * (gerado: libc++_shared.so SEMPRE 1º; demais .so do APK depois) */
  if (load_module("libc++_shared.so", 24, 1) < 0) return 1;
  if (load_module("libfmodevent.so", 8, 1) < 0) return 1;
  if (load_module("libfmodex.so", 8, 1) < 0) return 1;
  if (load_module("libNimble.so", 8, 1) < 0) return 1;
  /* módulo principal (resolve contra tudo acima + fallback dlsym) */
  if (load_module(SO_NAME, MEMORY_MB, 0) < 0) return 1;

  uintptr_t jni_onload = so_find_addr_safe("JNI_OnLoad");
  uintptr_t android_main = so_find_addr_safe("android_main");
  debugPrintf("entry: JNI_OnLoad=%p android_main=%p (combinada=%d símbolos)\n",
              (void *)jni_onload, (void *)android_main, g_comb_n);

  debugPrintf("=== F1 OK: multi-módulo carregado+resolvido. (F2: boot do engine) ===\n");
  return 0;
}
