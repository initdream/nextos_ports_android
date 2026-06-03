/*
 * main_recon.c — RECON do Unity (Hollow Knight).
 *
 * Carrega libunity.so com o so-loader, resolve os imports (gen table),
 * roda init_array, e chama JNI_OnLoad com um JavaVM FALSO + jni_shim verboso.
 * O objetivo NAO e rodar o jogo — e CAPTURAR no log o contrato Java que o
 * Unity exige (FindClass / GetMethodID / RegisterNatives), pra medir o trabalho.
 *
 * Saida: stderr (rode com 2> recon.log).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>

#include "so_util.h"
#include "imports.h"
#include "jni_shim.h"
#include "util.h"
#include "egl_shim.h"
#include <SDL2/SDL.h>

void recon_wire_egl(void);

#define MEMORY_MB 384            /* libunity e grande */
#define SO_NAME "libunity.so"

FILE *stderr_fake;               /* exigido por imports.h */

/* modulos multi-modulo (compartilhados com recon_egl.c p/ dlopen/dlsym bridge) */
so_module *g_m_il2cpp = NULL;
so_module *g_m_unity = NULL;

typedef int jint;
typedef jint (*JNI_OnLoad_t)(void *vm, void *reserved);

static void on_segv(int sig, siginfo_t *si, void *uc_) {
  (void)sig;
  ucontext_t *uc = (ucontext_t *)uc_;
  uintptr_t pc = uc->uc_mcontext.pc;
  uintptr_t lr = uc->uc_mcontext.regs[30];
  uintptr_t tb = (uintptr_t)text_base;
  char buf[256];
  int n;
  if (pc >= tb && pc < tb + text_size)
    n = snprintf(buf, sizeof(buf),
                 "\n*** SEGV fault=%p pc=libunity+0x%lx lr=+0x%lx ***\n",
                 si->si_addr, (unsigned long)(pc - tb),
                 (lr >= tb && lr < tb + text_size) ? (unsigned long)(lr - tb)
                                                   : 0xffffffffUL);
  else
    n = snprintf(buf, sizeof(buf),
                 "\n*** SEGV fault=%p pc=%p [FORA libunity] ***\n", si->si_addr,
                 (void *)pc);
  if (write(2, buf, n) < 0) { /* ignore */ }
  _exit(139);
}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  stderr_fake = stderr;
  setvbuf(stderr, NULL, _IOLBF, 0);

  static char altstack[64 * 1024];
  stack_t ss = {.ss_sp = altstack, .ss_size = sizeof(altstack), .ss_flags = 0};
  sigaltstack(&ss, NULL);
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = on_segv;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);

  fprintf(stderr, "===================================================\n");
  fprintf(stderr, " HOLLOW-RECON — carregando %s\n", SO_NAME);
  fprintf(stderr, "===================================================\n");

  /* === MULTI-MODULO: replica o libmain (carrega il2cpp -> unity) === */
  size_t hs = (size_t)MEMORY_MB * 1024 * 1024;
  void *heap_il2 = mmap(NULL, hs, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  void *heap_uni = mmap(NULL, hs, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap_il2 == MAP_FAILED || heap_uni == MAP_FAILED) { perror("mmap"); return 1; }

  /* tabela de imports (uma vez) + wire egl/ANativeWindow */
  void recon_fill_passthrough(void); recon_fill_passthrough();
  recon_wire_egl();

  /* (A) libil2cpp — .init_array inicializa o runtime/memory manager */
  fprintf(stderr, "[A] carregando libil2cpp.so...\n");
  if (so_load("libil2cpp.so", heap_il2, hs) < 0) { fprintf(stderr, "FALHOU il2cpp\n"); return 1; }
  if (so_relocate() < 0) { fprintf(stderr, "FALHOU reloc il2cpp\n"); return 1; }
  so_resolve(dynlib_functions, dynlib_numfunctions, 0);
  so_execute_init_array();
  g_m_il2cpp = so_save();
  fprintf(stderr, "[A] libil2cpp OK (text=%p+0x%zx)\n", text_base, text_size);

  /* (B) libunity */
  fprintf(stderr, "[B] carregando libunity.so...\n");
  if (so_load(SO_NAME, heap_uni, hs) < 0) { fprintf(stderr, "FALHOU unity\n"); return 1; }
  if (so_relocate() < 0) { fprintf(stderr, "FALHOU reloc unity\n"); return 1; }
  so_resolve(dynlib_functions, dynlib_numfunctions, 0);
  so_execute_init_array();
  g_m_unity = so_save();
  fprintf(stderr, "[B] libunity OK (text=%p+0x%zx)\n", text_base, text_size);

  /* JavaVM/JNIEnv falso */
  void *fake_vm = NULL, *fake_env = NULL;
  jni_shim_init(&fake_vm, &fake_env);
  fprintf(stderr, "[6] jni_shim pronto\n");

  /* (C) JNI_OnLoad: il2cpp (se houver) + unity — igual o libmain */
  so_use(g_m_il2cpp);
  uintptr_t il2_onload = so_find_addr("JNI_OnLoad");
  if (il2_onload) {
    fprintf(stderr, "[C] il2cpp JNI_OnLoad @ %p...\n", (void *)il2_onload);
    ((JNI_OnLoad_t)il2_onload)(fake_vm, NULL);
  }
  so_use(g_m_unity);
  uintptr_t onload = so_find_addr("JNI_OnLoad");
  if (!onload) { fprintf(stderr, "!!! unity JNI_OnLoad nao achado\n"); return 1; }
  fprintf(stderr, "[C] unity JNI_OnLoad @ %p — CHAMANDO\n", (void *)onload);
  jint ver = ((JNI_OnLoad_t)onload)(fake_vm, NULL);
  fprintf(stderr, "[8] JNI_OnLoad RETORNOU 0x%x — multi-modulo OK!\n", ver);

  /* 9. DIRIGIR: UnityPlayer.initJni(Context) — o 1o passo do UnityPlayer.java */
  void *initJni = jni_find_native("initJni");
  fprintf(stderr, "[9] initJni nativo @ %p\n", initJni);
  if (initJni) {
    static long fake_thiz = 0xA1, fake_context = 0xC0;
    void (*initJni_fn)(void *, void *, void *) =
        (void (*)(void *, void *, void *))initJni;
    fprintf(stderr, "------------- CHAMANDO initJni(env, this, Context) -------------\n");
    initJni_fn(fake_env, &fake_thiz, &fake_context);
    fprintf(stderr, "----------------------------------------------------------------\n");
    fprintf(stderr, "[10] initJni RETORNOU — proximo contrato Java logado acima\n");
  }

  /* 10b. janela SDL/GLES (precisa ES parado p/ pegar o framebuffer) */
  fprintf(stderr, "[10b] SDL_Init(VIDEO) + egl_shim_create_window...\n");
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    fprintf(stderr, "    SDL_Init FALHOU: %s\n", SDL_GetError());
  egl_shim_create_window();
  fprintf(stderr, "[10c] window=%p (janela SDL criada?)\n", (void *)egl_shim_get_window());
  fflush(NULL);

  /* === FASE 2: sequencia de init === */
  /* 11. nativeRender PRIMEIRO — em muitas versoes do Unity o 1o nativeRender
     dispara o engine init (dlopen libil2cpp + carrega dados). */
  void *render = jni_find_native("nativeRender");
  fprintf(stderr, "[11] nativeRender (1o = engine init?) @ %p\n", render);
  if (render) {
    static long t = 0xA1;
    unsigned char (*rf)(void *, void *) = (unsigned char (*)(void *, void *))render;
    fprintf(stderr, "----------------- CHAMANDO nativeRender() #1 -----------------\n");
    unsigned char r = rf(fake_env, &t);
    fprintf(stderr, "--------------------------------------------------------------\n");
    fprintf(stderr, "[12] nativeRender #1 RETORNOU %d\n", r);
  }

  /* 13. nativeRecreateGfxState depois */
  void *gfx = jni_find_native("nativeRecreateGfxState");
  fprintf(stderr, "[13] nativeRecreateGfxState @ %p\n", gfx);
  if (gfx) {
    static long t = 0xA1, surf = 0x5F;
    void (*gf)(void *, void *, int, void *) =
        (void (*)(void *, void *, int, void *))gfx;
    fprintf(stderr, "------ CHAMANDO nativeRecreateGfxState(0, surface) ------\n");
    gf(fake_env, &t, 0, &surf);
    fprintf(stderr, "[14] nativeRecreateGfxState RETORNOU\n");
  }
  return 0;
}
