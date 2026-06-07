/*
 * main.c -- reVC (GTA Vice City, build Android) so-loader para NextOS aarch64.
 *
 * Loader de DOIS módulos:
 *   Módulo A = libc++_shared.so (NDK, ABI __ndk1) -> fornece std::__ndk1::*,
 *              operator new/delete, __cxa_*, _Unwind_* com a ABI exata que o
 *              libreVC.so foi compilado.
 *   Módulo B = libreVC.so (a engine). Resolve seus imports a partir de:
 *              tabela(A) + stubs bionic + dlsym(RTLD_DEFAULT) das libs do device
 *              (SDL2/GLESv2/EGL/OpenAL/mpg123/glibc) pré-carregadas com RTLD_GLOBAL.
 *
 * Entrada = SDL_main (a engine é SDL2, não NativeActivity/NVEvent).
 */

#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "error.h"
#include "so_util.h"
#include "util.h"

#define CXX_SO  "libc++_shared.so"
#define GAME_SO "libreVC.so"
#define CXX_HEAP_MB  48
#define GAME_HEAP_MB 96

// stubs.c
extern DynLibFunction revc_stub_table[];
extern const int revc_stub_count;
// pthread_bridge.c (ponte ABI bionic->glibc)
extern DynLibFunction revc_pthread_table[];
extern const int revc_pthread_count;

// tabela "base" de overrides (stubs + pthread) usada pelos DOIS módulos
static DynLibFunction *g_base = NULL;
static int g_base_n = 0;
static void build_base_table(void) {
  g_base_n = revc_stub_count + revc_pthread_count;
  g_base = malloc(sizeof(DynLibFunction) * g_base_n);
  memcpy(g_base, revc_stub_table, sizeof(DynLibFunction) * revc_stub_count);
  memcpy(g_base + revc_stub_count, revc_pthread_table,
         sizeof(DynLibFunction) * revc_pthread_count);
}

// ---------------------------------------------------------------------------
// Crash handler — reporta PC/offset relativo ao módulo (B = libreVC, último
// carregado, é o que text_base/text_size apontam).
// ---------------------------------------------------------------------------
static volatile uintptr_t g_blr_trap = 0; // addr do blr x9 instrumentado
static volatile int g_blr_hits = 0;

static void crash_handler(int sig, siginfo_t *info, void *uctx) {
  ucontext_t *uc = (ucontext_t *)uctx;
  uintptr_t pc = uc->uc_mcontext.pc;
  uintptr_t fault = (uintptr_t)info->si_addr;
  uintptr_t text = (uintptr_t)text_base;

  // BRK trap no blr x9: loga x9 e emula o blr (LR=ret, PC=x9) p/ continuar.
  if (sig == SIGTRAP && g_blr_trap && pc == g_blr_trap) {
    uintptr_t x9 = uc->uc_mcontext.regs[9];
    uintptr_t x0 = uc->uc_mcontext.regs[0];
    fprintf(stderr, "[BRK] blr x9: x9=%p (libreVC+0x%lx) x0=%p [hit %d]\n",
            (void *)x9, (unsigned long)(x9 - text), (void *)x0, ++g_blr_hits);
    fflush(stderr);
    uc->uc_mcontext.regs[30] = g_blr_trap + 4; // LR = retorno
    uc->uc_mcontext.pc = x9;                    // salta p/ x9
    return;
  }

  fprintf(stderr, "\n=== CRASH === sig=%d fault=%p pc=%p\n", sig, (void *)fault,
          (void *)pc);
  if (pc >= text && pc < text + text_size)
    fprintf(stderr, "PC em %s + 0x%lx\n", GAME_SO, (unsigned long)(pc - text));
  else
    fprintf(stderr, "PC fora de %s (lib do sistema?)\n", GAME_SO);

  for (int i = 0; i < 31; i++) {
    fprintf(stderr, " x%-2d=%016lx", i, (unsigned long)uc->uc_mcontext.regs[i]);
    if (i % 3 == 2)
      fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n sp=%016lx pc=%016lx\n", (unsigned long)uc->uc_mcontext.sp,
          (unsigned long)pc);

  // Backtrace via FP/LR
  fprintf(stderr, "Backtrace:\n");
  uintptr_t fp = uc->uc_mcontext.regs[29];
  for (int f = 0; f < 24 && fp; f++) {
    uintptr_t *p = (uintptr_t *)fp;
    uintptr_t next = p[0], lr = p[1];
    if (!lr)
      break;
    fprintf(stderr, "  #%-2d lr %p", f, (void *)lr);
    if (lr >= text && lr < text + text_size)
      fprintf(stderr, " (%s+0x%lx)", GAME_SO, (unsigned long)(lr - text));
    fprintf(stderr, "\n");
    if (next <= fp)
      break;
    fp = next;
  }
  fprintf(stderr, "text_base=%p size=0x%zx\n", text_base, text_size);

  // Instrução no PC (se legível) + qual mapa contém o PC.
  if (pc > 0x1000)
    fprintf(stderr, "insn@pc = 0x%08x\n", *(uint32_t *)pc);
  FILE *maps = fopen("/proc/self/maps", "r");
  if (maps) {
    char line[512];
    while (fgets(line, sizeof(line), maps)) {
      unsigned long a, b;
      if (sscanf(line, "%lx-%lx", &a, &b) == 2 && pc >= a && pc < b) {
        fprintf(stderr, ">>> PC: %s", line);
        break;
      }
    }
    fclose(maps);
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
  sigaction(SIGTRAP, &sa, NULL);
}

// ---------------------------------------------------------------------------
// Pré-carrega as libs do device no escopo GLOBAL para o fallback dlsym do
// so_resolve enxergar SDL2/GLES/OpenAL/mpg123. Não-fatal se alguma faltar.
// ---------------------------------------------------------------------------
static void preload_device_libs(void) {
  static const char *libs[] = {
      "libSDL2-2.0.so.0", "libGLESv2.so",   "libEGL.so",
      "libopenal.so.1",   "libmpg123.so.0", "libm.so.6",
      NULL,
  };
  for (int i = 0; libs[i]; i++) {
    void *h = dlopen(libs[i], RTLD_NOW | RTLD_GLOBAL);
    if (h)
      debugPrintf("preload: %s OK\n", libs[i]);
    else
      debugPrintf("preload: %s FALHOU (%s)\n", libs[i], dlerror());
  }
}

// Carrega um módulo .so via so_util: load+relocate+resolve+finalize+init_array.
// resolve_tbl/resolve_n = tabela de símbolos explícitos (override antes do dlsym).
static void load_module(const char *name, int heap_mb, DynLibFunction *tbl,
                        int n) {
  size_t heap_size = (size_t)heap_mb * 1024 * 1024;
  void *heap = mmap(NULL, heap_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap == MAP_FAILED)
    fatal_error("mmap %d MB falhou para %s", heap_mb, name);

  debugPrintf("== carregando %s (heap %p, %d MB) ==\n", name, heap, heap_mb);
  if (so_load(name, heap, heap_size) < 0)
    fatal_error("so_load(%s) falhou", name);
  if (so_relocate() < 0)
    fatal_error("so_relocate(%s) falhou", name);
  so_resolve(tbl, n, 0);
  so_finalize();
  so_flush_caches();
  so_execute_init_array();
  debugPrintf("== %s: text=%p+%zu data=%p+%zu ==\n", name, text_base, text_size,
              data_base, data_size);
}

int main(int argc, char *argv[]) {
  install_crash_handler();
  debugPrintf("=== reVC (Android) so-loader / NextOS aarch64 ===\n");

  preload_device_libs();
  build_base_table(); // stubs + ponte pthread (p/ os dois módulos)

  // ---- Módulo A: libc++_shared.so (provedor de C++ runtime __ndk1) ----
  load_module(CXX_SO, CXX_HEAP_MB, g_base, g_base_n);
  int cxx_n = 0;
  DynLibFunction *cxx_tbl = so_snapshot_symbols(&cxx_n);
  if (!cxx_tbl || cxx_n <= 0)
    fatal_error("snapshot de %s vazio", CXX_SO);
  debugPrintf("libc++: %d símbolos exportados disponíveis para o jogo\n", cxx_n);

  // tabela combinada para o módulo B: base(stubs+pthread, prioridade) + libc++(A).
  int comb_n = g_base_n + cxx_n;
  DynLibFunction *comb = malloc(sizeof(DynLibFunction) * comb_n);
  if (!comb)
    fatal_error("malloc tabela combinada");
  memcpy(comb, g_base, sizeof(DynLibFunction) * g_base_n);
  memcpy(comb + g_base_n, cxx_tbl, sizeof(DynLibFunction) * cxx_n);

  // ---- Módulo B: libreVC.so (a engine) ----
  load_module(GAME_SO, GAME_HEAP_MB, comb, comb_n);


  // ---- Entrada SDL_main ----
  void (*sdl_set_main_ready)(void) = dlsym(RTLD_DEFAULT, "SDL_SetMainReady");
  if (sdl_set_main_ready) {
    sdl_set_main_ready();
    debugPrintf("SDL_SetMainReady() chamado\n");
  } else {
    debugPrintf("aviso: SDL_SetMainReady não encontrado (seguindo)\n");
  }

  uintptr_t sdl_main_addr = so_find_addr("SDL_main");
  debugPrintf("SDL_main em %p — chamando...\n", (void *)sdl_main_addr);
  int (*sdl_main)(int, char **) = (int (*)(int, char **))sdl_main_addr;

  char *game_argv[] = {(char *)"reVC", NULL};
  int rc = sdl_main(argc > 1 ? argc : 1, argc > 1 ? argv : game_argv);
  debugPrintf("SDL_main retornou %d\n", rc);

  _exit(0);
}
