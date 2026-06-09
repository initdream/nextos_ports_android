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

static DynLibFunction *g_base;
static int g_base_n;
static void build_base_table(void) {
  g_base_n = dysmantle_overrides_count + revc_pthread_count;
  g_base = malloc(sizeof(DynLibFunction) * g_base_n);
  memcpy(g_base, dysmantle_overrides, sizeof(DynLibFunction) * dysmantle_overrides_count);
  memcpy(g_base + dysmantle_overrides_count, revc_pthread_table,
         sizeof(DynLibFunction) * revc_pthread_count);
}

static void crash_handler(int sig, siginfo_t *info, void *uc) {
  uintptr_t fault = (uintptr_t)info->si_addr;
  uintptr_t tb = (uintptr_t)text_base;
  fprintf(stderr, "\n=== CRASH sig=%d addr=%p ===\n", sig, info->si_addr);
  ucontext_t *u = (ucontext_t *)uc;
  uintptr_t pc = u->uc_mcontext.pc;
  fprintf(stderr, "  PC=%p", (void *)pc);
  if (tb && pc >= tb && pc < tb + text_size)
    fprintf(stderr, " = %s+0x%lx", GAME_SO, (unsigned long)(pc - tb));
  fprintf(stderr, "\n");
  if (tb && fault >= tb && fault < tb + text_size)
    fprintf(stderr, "  fault %s+0x%lx\n", GAME_SO, (unsigned long)(fault - tb));
  uintptr_t fp = u->uc_mcontext.regs[29];
  for (int f = 0; f < 20 && fp; f++) {
    uintptr_t *p = (uintptr_t *)fp; uintptr_t next = p[0], lr = p[1];
    if (!lr) break;
    fprintf(stderr, "  #%-2d lr %p", f, (void *)lr);
    if (lr >= tb && lr < tb + text_size) fprintf(stderr, " (%s+0x%lx)", GAME_SO, (unsigned long)(lr - tb));
    fprintf(stderr, "\n");
    if (next <= fp) break; fp = next;
  }
  fprintf(stderr, "  text_base=%p size=0x%zx\n", text_base, text_size);
  fflush(stderr);
  _exit(128 + sig);
}
static void install_crash_handler(void) {
  struct sigaction sa; memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = crash_handler; sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL);
  sigaction(SIGILL, &sa, NULL); sigaction(SIGABRT, &sa, NULL);
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
  fprintf(stderr, "=== DYSMANTLE (Android) so-loader / NextOS aarch64 Mali-450 ===\n");

  jni_shim_set_package("com.dysmantle53.soco", 0);

  preload_device_libs();
  build_base_table();

  /* Módulo A: libc++_shared.so */
  load_module(CXX_SO, CXX_HEAP_MB, g_base, g_base_n);
  int cxx_n = 0;
  DynLibFunction *cxx_tbl = so_snapshot_symbols(&cxx_n);
  if (!cxx_tbl || cxx_n <= 0) { fprintf(stderr, "snapshot %s vazio\n", CXX_SO); exit(1); }
  fprintf(stderr, "libc++: %d símbolos exportados\n", cxx_n);

  int comb_n = g_base_n + cxx_n;
  DynLibFunction *comb = malloc(sizeof(DynLibFunction) * comb_n);
  memcpy(comb, g_base, sizeof(DynLibFunction) * g_base_n);
  memcpy(comb + g_base_n, cxx_tbl, sizeof(DynLibFunction) * cxx_n);

  /* Módulo B: libNativeGame.so */
  load_module(GAME_SO, GAME_HEAP_MB, comb, comb_n);

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
