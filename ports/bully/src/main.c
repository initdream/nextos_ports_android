/*
 * main.c -- Bully: Anniversary Edition (build Android) so-loader p/ NextOS
 * aarch64 + Mali-450 fbdev. Loader de DOIS módulos (libc++_shared + libGame),
 * igual ao reVC, mas entry = JNI estático (jni_load), não SDL_main.
 *
 * Módulo A = libc++_shared.so (ABI __ndk1) -> std C++ runtime.
 * Módulo B = libGame.so (engine). Resolve via: bully_stub_table + pthread_bridge
 *            + símbolos do libc++ (snapshot) + dlsym(RTLD_DEFAULT) das libs do
 *            device (SDL2/GLESv2/EGL/OpenAL/mpg123/glibc) pré-carregadas GLOBAL.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "so_util.h"

#define CXX_SO  "libc++_shared.so"
#define GAME_SO "libGame.so"
#define CXX_HEAP_MB  48
#define GAME_HEAP_MB 128

/* dummy Module p/ os externs do jni_shim (compat so_util_x64.h) */
int mod_game, mod_cxx;

extern DynLibFunction bully_stub_table[];
extern const int bully_stub_count;
extern DynLibFunction revc_pthread_table[];
extern const int revc_pthread_count;
extern void bully_imports_init(void);
extern void jni_load(void); /* driver JNI estático (jni_shim.c) */

static DynLibFunction *g_base;
static int g_base_n;

static void build_base_table(void) {
  g_base_n = bully_stub_count + revc_pthread_count;
  g_base = malloc(sizeof(DynLibFunction) * g_base_n);
  memcpy(g_base, bully_stub_table, sizeof(DynLibFunction) * bully_stub_count);
  memcpy(g_base + bully_stub_count, revc_pthread_table,
         sizeof(DynLibFunction) * revc_pthread_count);
}

/* crash handler: imprime sinal + endereço + offset relativo ao libGame
 * (offset = fault - text_base -> mapear via `nm -DC libGame.so`, igual gdb PC). */
static void crash_handler(int sig, siginfo_t *info, void *uc) {
  (void)uc;
  uintptr_t fault = (uintptr_t)info->si_addr;
  uintptr_t tb = (uintptr_t)text_base;
  fprintf(stderr, "\n=== CRASH sig=%d addr=%p ===\n", sig, info->si_addr);
  if (tb && fault >= tb && fault < tb + text_size)
    fprintf(stderr, "  libGame+0x%lx (text_base=%p)\n", (unsigned long)(fault - tb), text_base);
  else
    fprintf(stderr, "  fora do libGame text (text_base=%p size=%zu)\n", text_base, text_size);
#if defined(__aarch64__)
  ucontext_t *u = (ucontext_t *)uc;
  uintptr_t pc = u->uc_mcontext.pc;
  fprintf(stderr, "  PC=%p", (void *)pc);
  if (tb && pc >= tb && pc < tb + text_size)
    fprintf(stderr, " = libGame+0x%lx", (unsigned long)(pc - tb));
  fprintf(stderr, "\n");
#endif
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
  sigaction(SIGILL, &sa, NULL);
}

static void preload_device_libs(void) {
  static const char *libs[] = {
      "libSDL2-2.0.so.0", "libGLESv2.so",   "libEGL.so",
      "libopenal.so.1",   "libmpg123.so.0", "libm.so.6", NULL,
  };
  for (int i = 0; libs[i]; i++) {
    void *h = dlopen(libs[i], RTLD_NOW | RTLD_GLOBAL);
    fprintf(stderr, "preload: %s %s\n", libs[i], h ? "OK" : dlerror());
  }
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
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  install_crash_handler();
  fprintf(stderr, "=== Bully (Android) so-loader / NextOS aarch64 Mali-450 ===\n");

  bully_imports_init();
  preload_device_libs();
  build_base_table();

  /* Módulo A: libc++_shared.so */
  load_module(CXX_SO, CXX_HEAP_MB, g_base, g_base_n);
  int cxx_n = 0;
  DynLibFunction *cxx_tbl = so_snapshot_symbols(&cxx_n);
  if (!cxx_tbl || cxx_n <= 0) { fprintf(stderr, "snapshot %s vazio\n", CXX_SO); exit(1); }
  fprintf(stderr, "libc++: %d símbolos exportados\n", cxx_n);

  /* tabela combinada p/ o módulo B: base(stubs+pthread) + libc++ */
  int comb_n = g_base_n + cxx_n;
  DynLibFunction *comb = malloc(sizeof(DynLibFunction) * comb_n);
  memcpy(comb, g_base, sizeof(DynLibFunction) * g_base_n);
  memcpy(comb + g_base_n, cxx_tbl, sizeof(DynLibFunction) * cxx_n);

  /* Módulo B: libGame.so */
  load_module(GAME_SO, GAME_HEAP_MB, comb, comb_n);

  /* driver JNI estático (build_env -> hooks -> impl* -> frame loop) */
  fprintf(stderr, "=== rodando jni_load (driver) ===\n");
  jni_load();

  fprintf(stderr, "=== jni_load retornou ===\n");
  return 0;
}
