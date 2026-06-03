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

#define MEMORY_MB 384            /* libunity e grande */
#define SO_NAME "libunity.so"

FILE *stderr_fake;               /* exigido por imports.h */

typedef int jint;
typedef jint (*JNI_OnLoad_t)(void *vm, void *reserved);

static void on_segv(int sig, siginfo_t *si, void *uc_) {
  (void)sig;
  ucontext_t *uc = (ucontext_t *)uc_;
  uintptr_t pc = uc->uc_mcontext.pc;
  uintptr_t lr = uc->uc_mcontext.regs[30];
  uintptr_t tb = (uintptr_t)text_base;
  fprintf(stderr, "\n*** SEGV fault=%p pc=%p lr=%p ", si->si_addr, (void *)pc,
          (void *)lr);
  if (pc >= tb && pc < tb + text_size)
    fprintf(stderr, "[em libunity +0x%lx]\n", (unsigned long)(pc - tb));
  else
    fprintf(stderr, "[FORA do libunity — nosso codigo/libc]\n");
  if (lr >= tb && lr < tb + text_size)
    fprintf(stderr, "    LR aponta libunity +0x%lx\n", (unsigned long)(lr - tb));
  fflush(stderr);
  _exit(139);
}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  stderr_fake = stderr;
  setvbuf(stderr, NULL, _IOLBF, 0);

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = on_segv;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);

  fprintf(stderr, "===================================================\n");
  fprintf(stderr, " HOLLOW-RECON — carregando %s\n", SO_NAME);
  fprintf(stderr, "===================================================\n");

  /* 1. heap RWX */
  size_t heap_size = (size_t)MEMORY_MB * 1024 * 1024;
  void *heap = mmap(NULL, heap_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap == MAP_FAILED) { perror("mmap"); return 1; }
  fprintf(stderr, "[1] heap %p (%d MB)\n", heap, MEMORY_MB);

  /* 2. carregar o ELF */
  if (so_load(SO_NAME, heap, heap_size) < 0) {
    fprintf(stderr, "FALHOU so_load(%s) — o arquivo esta no cwd?\n", SO_NAME);
    return 1;
  }
  fprintf(stderr, "[2] carregado: text=%p+0x%zx\n", text_base, text_size);

  /* 3. relocations */
  if (so_relocate() < 0) { fprintf(stderr, "FALHOU so_relocate\n"); return 1; }
  fprintf(stderr, "[3] relocate ok\n");

  /* 4. resolver imports (passthrough libc/GLES + stubs que logam) */
  fprintf(stderr, "[4] resolvendo %zu imports...\n", dynlib_numfunctions);
  void recon_fill_passthrough(void); recon_fill_passthrough();
  so_resolve(dynlib_functions, dynlib_numfunctions, 0);
  fprintf(stderr, "    (imports nao-resolvidos aparecem como '*** UNRESOLVED ***' acima)\n");

  /* 5. construtores .init_array */
  fprintf(stderr, "[5] rodando .init_array...\n");
  so_execute_init_array();
  fprintf(stderr, "    init_array ok\n");

  /* 6. JavaVM/JNIEnv falso */
  void *fake_vm = NULL, *fake_env = NULL;
  jni_shim_init(&fake_vm, &fake_env);
  fprintf(stderr, "[6] jni_shim pronto (vm=%p)\n", fake_vm);

  /* 7. JNI_OnLoad — aqui o Unity registra natives + chama Java (logado) */
  uintptr_t onload = so_find_addr("JNI_OnLoad");
  if (!onload) {
    fprintf(stderr, "!!! JNI_OnLoad NAO encontrado em %s\n", SO_NAME);
    return 1;
  }
  fprintf(stderr, "[7] JNI_OnLoad @ %p — CHAMANDO (o contrato Java vem agora)\n",
          (void *)onload);
  fprintf(stderr, "-------------------- CONTRATO UNITY --------------------\n");
  JNI_OnLoad_t jni_onload = (JNI_OnLoad_t)onload;
  jint ver = jni_onload(fake_vm, NULL);
  fprintf(stderr, "--------------------------------------------------------\n");
  fprintf(stderr, "[8] JNI_OnLoad RETORNOU 0x%x — recon ate aqui OK!\n", ver);

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

  /* 11. nativeRecreateGfxState(api, Surface) — setup grafico (EGL ainda stub) */
  void *gfx = jni_find_native("nativeRecreateGfxState");
  fprintf(stderr, "[11] nativeRecreateGfxState @ %p\n", gfx);
  if (gfx) {
    static long fake_thiz = 0xA1, fake_surface = 0x5F;
    void (*gfx_fn)(void *, void *, int, void *) =
        (void (*)(void *, void *, int, void *))gfx;
    fprintf(stderr, "------------- CHAMANDO nativeRecreateGfxState(0, surface) -------------\n");
    gfx_fn(fake_env, &fake_thiz, 0, &fake_surface);
    fprintf(stderr, "-----------------------------------------------------------------------\n");
    fprintf(stderr, "[12] nativeRecreateGfxState RETORNOU\n");
  }

  /* 13. nativeRender() — o coracao. Aqui Unity inicializa engine/il2cpp/dados */
  void *render = jni_find_native("nativeRender");
  fprintf(stderr, "[13] nativeRender @ %p\n", render);
  if (render) {
    static long fake_thiz = 0xA1;
    unsigned char (*render_fn)(void *, void *) =
        (unsigned char (*)(void *, void *))render;
    fprintf(stderr, "------------------- CHAMANDO nativeRender() -------------------\n");
    unsigned char r = render_fn(fake_env, &fake_thiz);
    fprintf(stderr, "--------------------------------------------------------------\n");
    fprintf(stderr, "[14] nativeRender RETORNOU %d\n", r);
  }
  return 0;
}
