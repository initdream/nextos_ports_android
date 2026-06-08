/* crash.c — handler de crash armhf: loga PC/LR/fault + offset no libGTAVC.so */
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include "so_util.h"

static volatile int ignored_count = 0;

static void log_model_ptr_matches(FILE *f, const char *where, unsigned long value,
                                  unsigned long *model_ptrs) {
  if (!value || !model_ptrs) return;
  for (int id = 0; id < 6500; id++) {
    if (model_ptrs[id] == value) {
      unsigned long vtable = 0;
      if (value > 0x10000)
        vtable = *(unsigned long *)value;
      fprintf(f, "    model-match %-8s value=0x%lx id=%d info=0x%lx vtable=0x%lx\n",
              where, value, id, model_ptrs[id], vtable);
      return;
    }
  }
}

static void handler(int sig, siginfo_t *si, void *uc_) {
  ucontext_t *uc = (ucontext_t *)uc_;
  /* si_code <= 0 => sinal ENVIADO (kill/tgkill/raise), nao falha de memoria real.
   * O jogo manda SIGSEGV em si mesmo (soft-assert). Ignora e continua. */
  if (si->si_code <= 0 && sig != SIGABRT) {
    if (ignored_count++ < 50) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[sig-ignore] sinal %d enviado (si_code=%d) — ignorado, continua\n", sig, si->si_code); fclose(f); }
    }
    return; /* resume — raise() retorna pro jogo */
  }
  unsigned long pc = uc->uc_mcontext.arm_pc;
  unsigned long lr = uc->uc_mcontext.arm_lr;
  unsigned long fa = (unsigned long)si->si_addr;
  unsigned long tb = (unsigned long)text_base;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    fprintf(f, "\n=== CRASH sig=%d fault=0x%lx PC=0x%lx LR=0x%lx ===\n", sig, fa, pc, lr);
    if (pc >= tb && pc < tb + text_size)
      fprintf(f, "  PC = libGTAVC.so + 0x%lx\n", pc - tb);
    else
      fprintf(f, "  PC fora do .so (loader/syslib)\n");
    if (lr >= tb && lr < tb + text_size)
      fprintf(f, "  LR = libGTAVC.so + 0x%lx\n", lr - tb);
    fprintf(f, "  r0=%lx r1=%lx r2=%lx r3=%lx sp=%lx\n",
            uc->uc_mcontext.arm_r0, uc->uc_mcontext.arm_r1,
            uc->uc_mcontext.arm_r2, uc->uc_mcontext.arm_r3,
            uc->uc_mcontext.arm_sp);
    fprintf(f, "  r4=%lx r5=%lx r6=%lx r7=%lx r8=%lx r9=%lx r10=%lx r11=%lx r12=%lx\n",
            uc->uc_mcontext.arm_r4, uc->uc_mcontext.arm_r5,
            uc->uc_mcontext.arm_r6, uc->uc_mcontext.arm_r7,
            uc->uc_mcontext.arm_r8, uc->uc_mcontext.arm_r9,
            uc->uc_mcontext.arm_r10, uc->uc_mcontext.arm_fp,
            uc->uc_mcontext.arm_ip);
    char *ms_line = (char *)so_find_addr_safe("_ZN11CFileLoader7ms_lineE");
    if (ms_line && ms_line[0])
      fprintf(f, "  CFileLoader::ms_line='%.*s'\n", 180, ms_line);
    unsigned long *model_ptrs =
        (unsigned long *)so_find_addr_safe("_ZN10CModelInfo16ms_modelInfoPtrsE");
    if (model_ptrs) {
      fprintf(f, "  model-info matches:\n");
      log_model_ptr_matches(f, "r0", uc->uc_mcontext.arm_r0, model_ptrs);
      log_model_ptr_matches(f, "r1", uc->uc_mcontext.arm_r1, model_ptrs);
      log_model_ptr_matches(f, "r2", uc->uc_mcontext.arm_r2, model_ptrs);
      log_model_ptr_matches(f, "r3", uc->uc_mcontext.arm_r3, model_ptrs);
      log_model_ptr_matches(f, "r4", uc->uc_mcontext.arm_r4, model_ptrs);
      log_model_ptr_matches(f, "r5", uc->uc_mcontext.arm_r5, model_ptrs);
      log_model_ptr_matches(f, "r6", uc->uc_mcontext.arm_r6, model_ptrs);
      log_model_ptr_matches(f, "r7", uc->uc_mcontext.arm_r7, model_ptrs);
      log_model_ptr_matches(f, "r8", uc->uc_mcontext.arm_r8, model_ptrs);
      log_model_ptr_matches(f, "r9", uc->uc_mcontext.arm_r9, model_ptrs);
      log_model_ptr_matches(f, "r10", uc->uc_mcontext.arm_r10, model_ptrs);
      log_model_ptr_matches(f, "r11", uc->uc_mcontext.arm_fp, model_ptrs);
      log_model_ptr_matches(f, "r12", uc->uc_mcontext.arm_ip, model_ptrs);
      log_model_ptr_matches(f, "lr", lr, model_ptrs);
    }
    /* qual lib contém o PC? */
    FILE *m = fopen("/proc/self/maps", "r");
    if (m) {
      char line[512];
      while (fgets(line, sizeof(line), m)) {
        unsigned long a, b;
        if (sscanf(line, "%lx-%lx", &a, &b) == 2 && pc >= a && pc < b) {
          fprintf(f, "  PC lib: %s", line);
          break;
        }
      }
      fclose(m);
    }
    /* scan da pilha por endereços de retorno no .so (cadeia de chamada) */
    fprintf(f, "  call-chain no .so (stack scan):\n");
    unsigned long *sp = (unsigned long *)uc->uc_mcontext.arm_sp;
    int found = 0;
    for (int i = 0; i < 512 && found < 12; i++) {
      unsigned long v = sp[i];
      if (v >= tb && v < tb + text_size) {
        fprintf(f, "    libGTAVC.so+0x%lx\n", v - tb);
        found++;
      }
    }
    if (model_ptrs) {
      fprintf(f, "  model-info stack matches:\n");
      int model_found = 0;
      for (int i = 0; i < 512 && model_found < 24; i++) {
        unsigned long v = sp[i];
        for (int id = 0; id < 6500; id++) {
          if (model_ptrs[id] == v) {
            unsigned long vtable = 0;
            if (v > 0x10000)
              vtable = *(unsigned long *)v;
            fprintf(f, "    sp[%03d]=0x%lx id=%d vtable=0x%lx\n", i, v, id, vtable);
            model_found++;
            break;
          }
        }
      }
    }
    fclose(f);
  }
  _exit(1);
}

void install_crash_handler(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
}
