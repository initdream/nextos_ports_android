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

#include "so_util.h"
#include "imports.h"
#include "jni_shim.h"
#include "util.h"

#define MEMORY_MB 384            /* libunity e grande */
#define SO_NAME "libunity.so"

FILE *stderr_fake;               /* exigido por imports.h */

typedef int jint;
typedef jint (*JNI_OnLoad_t)(void *vm, void *reserved);

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  stderr_fake = stderr;
  setvbuf(stderr, NULL, _IOLBF, 0);

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
  fprintf(stderr, "    (se chegou aqui, o contrato Java do Unity foi logado acima)\n");
  return 0;
}
