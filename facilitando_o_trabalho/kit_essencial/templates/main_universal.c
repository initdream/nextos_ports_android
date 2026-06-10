/*
 * main.c (UNIVERSAL) -- Template mestre para Ports de Android (ARM64) no NextOS.
 * Consolida o conhecimento de Bully, Vice City e Dysmantle.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <ucontext.h>

#include "core/so_util.h"
#include "core/egl_shim.h"
#include "jni_shim.h"

// Memória Heap reservada para o(s) módulo(s) .so
#define HEAP_MB 512
static void *g_heap = NULL;

/* ---------------- CRASH HANDLER (do Dysmantle) ---------------- */
static void resolve_addr(uintptr_t a, char *out, int outsz) {
    Dl_info info;
    if (dladdr((void *)a, &info) && info.dli_sname) {
        snprintf(out, outsz, "%s+0x%lx", info.dli_sname, (unsigned long)(a - (uintptr_t)info.dli_saddr));
    } else {
        snprintf(out, outsz, "0x%lx", (unsigned long)a);
    }
}

static void crash_handler(int sig, siginfo_t *info, void *uc) {
  fprintf(stderr, "\n=== CRASH sig=%d addr=%p ===\n", sig, info->si_addr);
  ucontext_t *u = (ucontext_t *)uc;
  uintptr_t pc = u->uc_mcontext.pc;
  char r[300];
  resolve_addr(pc, r, sizeof(r));
  fprintf(stderr, "  PC=%p %s\n", (void *)pc, r);
  
  // Imprime registradores básicos
  fprintf(stderr, "  x30(LR)=%lx\n", (unsigned long)u->uc_mcontext.regs[30]);
  
  // Stack scan simplificado
  fprintf(stderr, "  --- stack scan ---\n");
  uintptr_t sp = u->uc_mcontext.sp;
  for (uintptr_t a = sp; a < sp + 0x1000; a += 8) {
      uintptr_t v = *(uintptr_t *)a;
      if (v > (uintptr_t)text_base && v < (uintptr_t)text_base + text_size) {
          fprintf(stderr, "    [sp+0x%lx] game.so+0x%lx\n", (unsigned long)(a - sp), (unsigned long)(v - (uintptr_t)text_base));
      }
  }
  fflush(stderr);
  _exit(1);
}

void setup_crash_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
}

/* ---------------- MAIN LOOP ---------------- */
int main(int argc, char **argv) {
  setup_crash_handler();
  
  // 1. Reservar memória
  g_heap = mmap(NULL, HEAP_MB * 1024 * 1024, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (g_heap == MAP_FAILED) {
      fprintf(stderr, "FALHA ao alocar %dMB de heap\n", HEAP_MB);
      return 1;
  }

  // 2. Carregar libs do device (opcional, dependendo do port)
  // dlopen("libSDL2-2.0.so.0", RTLD_GLOBAL | RTLD_NOW);

  // 3. Carregar o .so do jogo
  fprintf(stderr, "[Loader] Carregando libgame.so...\n");
  if (so_load("libgame.so", g_heap, HEAP_MB * 1024 * 1024) < 0) {
      fprintf(stderr, "FALHA no so_load\n");
      return 1;
  }

  // 4. Relocalizar e Resolver Símbolos
  so_relocate();
  // so_resolve(minha_tabela_de_imports, num_imports, 1);

  // 5. Inicializar Shims (EGL, JNI, etc.)
  // egl_shim_init();
  // jni_shim_init();

  // 6. Rodar os construtores do .so
  fprintf(stderr, "[Loader] Executando init_array...\n");
  so_execute_init_array();

  // 7. Chamar o Entry Point do jogo (ex: JNI_OnLoad ou main)
  // uintptr_t entry = so_find_addr("JNI_OnLoad");
  // ... chama entry ...

  fprintf(stderr, "[Loader] Pronto. Entrando no loop de eventos.\n");
  
  // Aqui entra o loop de eventos (geralmente SDL2)
  return 0;
}
