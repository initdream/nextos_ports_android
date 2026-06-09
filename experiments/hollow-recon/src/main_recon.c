#define _GNU_SOURCE
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
#include <dlfcn.h>

#include "so_util.h"
#include "imports.h"
#include "jni_shim.h"
#include "util.h"
#include "egl_shim.h"
#include <SDL2/SDL.h>

void recon_wire_egl(void);

#define MEMORY_MB 128            /* il2cpp~36MB + unity~25MB; 384 estourava RAM (OOM) */
#define SO_NAME "libunity.so"

FILE *stderr_fake;               /* exigido por imports.h */

/* modulos multi-modulo (compartilhados com recon_egl.c p/ dlopen/dlsym bridge) */
so_module *g_m_il2cpp = NULL;
so_module *g_m_unity = NULL;
void *g_il2_text = NULL;
size_t g_il2_size = 0;

typedef int jint;
typedef jint (*JNI_OnLoad_t)(void *vm, void *reserved);

extern void *g_il2_text; extern size_t g_il2_size;  /* def em main p/ ranges */
/* injeção de input (def em jni_shim.c) */
struct hk_inject_s { int action, keycode, source, deviceId, metaState, repeat,
                     scancode, flags, unicode; long eventTime, downTime; };
extern struct hk_inject_s g_hk_inject;
extern void *hk_keyevent_object(void);
uintptr_t g_got_lo = 0, g_got_hi = 0;  /* faixa do GOT protegido (RO) */
uintptr_t g_egl_slots[8];              /* enderecos absolutos dos slots egl a PROTEGER */
int g_egl_nslots = 0;
static unsigned long g_got_skips = 0, g_got_emul = 0;
static unsigned long g_null_recov = 0;
static unsigned long g_gc_recov = 0;
static unsigned long g_nullcall_recov = 0;
/* tracepoints brk: addr patchado -> insn original (restaura+continua no SIGTRAP).
   kind 0 = restaura+continua (1-shot). kind 1 = GUARDADO: so loga (x0) quando
   g_in_inject (nossa injecao); nas outras chamadas EMULA `cbz x0,arg` e mantem
   armado (arg = endereco alvo do cbz). */
static struct { uintptr_t addr; unsigned int orig; const char *label; unsigned long hits;
                int kind; uintptr_t arg; } g_tp[8];
static int g_ntp = 0;
static volatile int g_in_inject = 0;
static int g_main_tid = 0;
#include <sys/syscall.h>
static inline int cur_tid(void) { return (int)syscall(SYS_gettid); }
static void on_segv(int sig, siginfo_t *si, void *uc_) {
  ucontext_t *uc = (ucontext_t *)uc_;
  uintptr_t tb0 = (uintptr_t)text_base;
  /* CHAMADA NULL/wild: o pc aterrissou em endereco nulo/baixo -- ex: a Unity chama
     uma funcao GLES3 ausente via ponteiro NULL (Mali-450 = GLES2). Nao da pra ler a
     insn (pc=0) -> retorna pra lr com x0=0 (a funcao vira no-op que "retorna 0") e segue.
     CHECAR ANTES do null-deref (que tenta ler a insn em pc e re-crasharia). */
  if (sig == 11 && uc->uc_mcontext.pc < 0x10000) {
    g_nullcall_recov++;
    if (g_nullcall_recov > 200000) { /* trava de seguranca: nunca girar pra sempre */
      const char *m = "[NULLCALL] excesso -> abort\n";
      if (write(2, m, 27) < 0) {}
      _exit(68);
    }
    if (g_nullcall_recov <= 40 || g_nullcall_recov % 100000 == 0) {
      char lb[112];
      int ln = snprintf(lb, sizeof lb,
                        "[NULLCALL-RECOV] #%lu pc=0x%lx -> ret lr=il2+0x%lx (x0=0)\n",
                        g_nullcall_recov, (unsigned long)uc->uc_mcontext.pc,
                        (unsigned long)(uc->uc_mcontext.regs[30] - 0x500000000UL));
      if (write(2, lb, ln) < 0) {}
    }
    uc->uc_mcontext.pc = uc->uc_mcontext.regs[30]; /* volta pro caller (lr) */
    uc->uc_mcontext.regs[0] = 0;                    /* valor de retorno = 0/NULL */
    return;
  }
  /* RECUPERACAO GENERICA de NULL-deref: tipos C# stripados/ausentes viram NULL e
     sao lidos por acessores il2cpp (ldr/ldrb de [NULL+off]). Fault em endereco
     baixo -> zera o reg destino (Rt) e avanca pc. Limite evita loop infinito. */
  if (sig == 11 && (uintptr_t)si->si_addr < 0x4000 && g_null_recov < 4000000) {
    g_null_recov++;
    uintptr_t fpc = uc->uc_mcontext.pc;
    if (g_null_recov <= 20 || g_null_recov % 100000 == 0) {  /* amostra a fonte dominante */
      char lb[80];
      int ln = snprintf(lb, sizeof lb, "[NULLR] pc=il2+0x%lx unity+0x%lx fault=0x%lx\n",
                        (unsigned long)(fpc - 0x500000000UL),
                        (unsigned long)(fpc - 0x540000000UL),
                        (unsigned long)(uintptr_t)si->si_addr);
      if (write(2, lb, ln) < 0) {}
    }
    unsigned int insn = *(unsigned int *)fpc;
    int Rt = insn & 0x1F;
    if (Rt != 31) uc->uc_mcontext.regs[Rt] = 0; /* x31=xzr: nao escrever */
    /* writeback pre/post-index (bit24=0): atualiza Rn = endereco acessado */
    if ((insn & 0x3B000000u) == 0x38000000u && (insn & 0x00000400u)) {
      int Rn = (insn >> 5) & 0x1F;
      if (Rn != 31) uc->uc_mcontext.regs[Rn] = (uintptr_t)si->si_addr;
    }
    uc->uc_mcontext.pc += 4;
    return;
  }
  /* RECUPERACAO GC CONSERVATIVO: o scan de marcacao do GC do il2cpp
     (il2cpp+0x6d0000..0x6e8000) dereferencia CANDIDATOS a ponteiro lidos da
     pilha/heap. Lixo (dados UTF-16, bools 0x0101..) aponta p/ memoria nao
     mapeada (qualquer valor) -> SIGSEGV no LOAD. Tratar o load como 0
     (=nao-e-ponteiro) e exatamente o que um GC conservativo robusto faz. */
  {
    uintptr_t fpc0 = uc->uc_mcontext.pc;
    unsigned long il2off = fpc0 - 0x500000000UL;
    int in_gc = (il2off >= 0x6d0000UL && il2off < 0x6e8000UL);
    if (sig == 11 && in_gc && (uintptr_t)si->si_addr >= 0x1000 &&
        g_gc_recov < 20000000) {
    uintptr_t fpc = uc->uc_mcontext.pc;
    unsigned int insn = *(unsigned int *)fpc;
    /* so LOADS (bit22=L=1 na familia LDR/STR e LDP-load 0x29400000/0xA9400000) */
    int is_ldst = ((insn & 0x3B000000u) == 0x39000000u) ||  /* LDR/STR imm */
                  ((insn & 0x3B000000u) == 0x38000000u);     /* LDR/STR reg/uns */
    int is_ldp  = ((insn & 0x7E000000u) == 0x28000000u);     /* LDP/STP */
    int is_load = (is_ldst && (insn & 0x00400000u)) ||
                  (is_ldp && (insn & 0x00400000u));
    if (is_load) {
      g_gc_recov++;
      if (g_gc_recov <= 10 || g_gc_recov % 1000000 == 0) {
        char lb[96];
        int ln = snprintf(lb, sizeof lb, "[GCR] pc=il2+0x%lx fault=0x%lx (#%lu)\n",
                          (unsigned long)(fpc - 0x500000000UL),
                          (unsigned long)(uintptr_t)si->si_addr, g_gc_recov);
        if (write(2, lb, ln) < 0) {}
      }
      int Rt = insn & 0x1F;
      if (Rt != 31) uc->uc_mcontext.regs[Rt] = 0;
      if (is_ldp) { int Rt2 = (insn >> 10) & 0x1F; if (Rt2 != 31) uc->uc_mcontext.regs[Rt2] = 0; }
      uc->uc_mcontext.pc += 4;
      return;
    }
    }
  }
  /* SONDA driver: recupera acessos na LACUNA nao-mapeada [24GB, 540GB) — entre os
     modulos (<21.6GB) e stack/mmap (>=547GB). Init do driver tem objetos com id
     lixo -> store/load em slot wild. Skip store / zera load. Gated HK_WILDREC. */
  if (sig == 11 && getenv("HK_WILDREC") &&
      (uintptr_t)si->si_addr >= 0x600000000UL && (uintptr_t)si->si_addr < 0x7e00000000UL) {
    static unsigned long g_wild = 0; g_wild++;
    uintptr_t fpc = uc->uc_mcontext.pc;
    unsigned int insn = *(unsigned int *)fpc;
    int is_load = ((insn & 0x3B000000u) == 0x39000000u || (insn & 0x3B000000u) == 0x38000000u)
                  && (insn & 0x00400000u);
    if (g_wild <= 10) { char lb[80]; int n = snprintf(lb, sizeof lb,
        "[WILD] pc=unity+0x%lx fault=0x%lx %s (#%lu)\n", (unsigned long)(fpc - 0x540000000UL),
        (unsigned long)(uintptr_t)si->si_addr, is_load ? "load" : "store", g_wild);
        if (write(2, lb, n) < 0) {} }
    if (is_load) { int Rt = insn & 0x1F; if (Rt != 31) uc->uc_mcontext.regs[Rt] = 0; }
    uc->uc_mcontext.pc += 4;
    return;
  }
  /* GOT protegido RO. Unity sobrescreve slots com Mali real. Para os 7 slots EGL:
     pulamos o store (mantem nosso shim). Para os demais (GL): EMULAMOS o store
     (decodifica str/stp) p/ o Unity carregar as funcoes GL reais normalmente. */
  if (sig == 11 && g_got_lo && (uintptr_t)si->si_addr >= g_got_lo &&
      (uintptr_t)si->si_addr < g_got_hi) {
    uintptr_t addr = (uintptr_t)si->si_addr;
    int is_egl = 0;
    for (int i = 0; i < g_egl_nslots; i++)
      if (addr == g_egl_slots[i] || addr == g_egl_slots[i] - 8) { is_egl = 1; break; }
    if (!is_egl) {
      /* emula o store p/ os slots GL */
      unsigned int insn = *(unsigned int *)uc->uc_mcontext.pc;
      unsigned long *R = (unsigned long *)uc->uc_mcontext.regs;
      mprotect((void *)g_got_lo, g_got_hi - g_got_lo, PROT_READ | PROT_WRITE);
      if ((insn & 0xFFC00000u) == 0xF9000000u) {        /* STR Xt,[Xn,#imm] */
        *(unsigned long *)addr = R[insn & 0x1F];
      } else if ((insn & 0xFFC00000u) == 0xA9000000u || /* STP Xt,Xt2,[Xn,#imm] */
                 (insn & 0xFFC00000u) == 0xA8800000u || (insn & 0xFFC00000u) == 0xA9800000u) {
        *(unsigned long *)addr = R[insn & 0x1F];
        *(unsigned long *)(addr + 8) = R[(insn >> 10) & 0x1F];
      }
      mprotect((void *)g_got_lo, g_got_hi - g_got_lo, PROT_READ);
      g_got_emul++;
    } else {
      g_got_skips++;
    }
    uc->uc_mcontext.pc += 4;  /* avanca depois do store */
    return;
  }
  /* SIGTRAP (brk-tracepoint): se eh um tracepoint nosso, LOGA + RESTAURA a insn
     original + CONTINUA (re-executa). Senao, comportamento antigo (_exit). */
  if (sig == 5) {
    uintptr_t p = uc->uc_mcontext.pc;
    for (int i = 0; i < g_ntp; i++) {
      if (g_tp[i].addr == p) {
        unsigned long *R = (unsigned long *)uc->uc_mcontext.regs;
        if (g_tp[i].kind == 1 && !(g_in_inject && cur_tid() == g_main_tid)) {
          /* nao eh nossa injecao: EMULA o cbz x0,arg e mantem o brk armado */
          if (R[0] == 0) uc->uc_mcontext.pc = g_tp[i].arg; else uc->uc_mcontext.pc += 4;
          return;
        }
        g_tp[i].hits++;
        char tb[200];
        int tn = snprintf(tb, sizeof tb,
            "\n[TRACE] '%s' (unity+0x%lx) hit#%lu x0=%lx x8=%lx(unity+0x%lx) x20=%lx x21=%lx\n",
            g_tp[i].label, (unsigned long)(p - tb0), g_tp[i].hits,
            R[0], R[8], (unsigned long)(R[8] - tb0), R[20], R[21]);
        if (write(2, tb, tn) < 0) {}
        /* handler (x0): le vtable + metodo de processamento (vtable+256) crus */
        if (R[0] > 0x10000) {
          unsigned long vt = *(unsigned long *)R[0];
          unsigned long meth = (vt > 0x10000) ? *(unsigned long *)(vt + 256) : 0;
          char hb[160];
          int hn = snprintf(hb, sizeof hb,
            "[HANDLER] obj=0x%lx vtable=0x%lx(unity+0x%lx) metodo[+256]=0x%lx(unity+0x%lx il2+0x%lx)\n",
            R[0], vt, vt - tb0, meth, meth - 0x540000000UL, meth - 0x500000000UL);
          if (write(2, hb, hn) < 0) {}
        }
        *(unsigned int *)p = g_tp[i].orig;          /* restaura insn original */
        __builtin___clear_cache((char *)p, (char *)p + 4);
        return;                                      /* re-executa a insn restaurada */
      }
    }
    char tb[160];
    void *got_mc = *(void **)(0x540000000UL + 0x16f5698);
    int tn = snprintf(tb, sizeof tb,
                      "\n[TRAP] ALCANCOU unity+0x%lx | GOT[eglMakeCurrent]=%p (shim=%p)\n",
                      (unsigned long)(p - tb0), got_mc, (void *)egl_shim_MakeCurrent);
    if (write(2, tb, tn) < 0) {}
    _exit(5);
  }
  /* DUMP MINIMO IMEDIATO (antes de qualquer coisa que possa double-fault) —
     pc/lr em offsets unity E il2cpp, p/ achar o crash em qualquer thread. */
  {
    uintptr_t p = uc->uc_mcontext.pc, l = uc->uc_mcontext.regs[30];
    uintptr_t U = 0x540000000UL, I = 0x500000000UL;
    char sb[256];
    int sn = snprintf(sb, sizeof sb,
      "\n[SIG %d] pc=0x%lx (unity+0x%lx il2+0x%lx) lr=0x%lx (unity+0x%lx il2+0x%lx)\n",
      sig, (unsigned long)p, (unsigned long)(p-U), (unsigned long)(p-I),
      (unsigned long)l, (unsigned long)(l-U), (unsigned long)(l-I));
    if (write(2, sb, sn) < 0) {}
    char cb[96];
    int cn = snprintf(cb, sizeof cb, "[GOT] egl_skips=%lu gl_emul=%lu null_recov=%lu\n",
                      g_got_skips, g_got_emul, g_null_recov);
    if (write(2, cb, cn) < 0) {}
    char rb[160]; unsigned long *R = (unsigned long *)uc->uc_mcontext.regs;
    int rn = snprintf(rb, sizeof rb,
      "[REGS] fault=0x%lx x0=%lx x13=%lx x19=%lx x20=%lx x5=%lx\n",
      (unsigned long)(uintptr_t)si->si_addr, R[0], R[13], R[19], R[20], R[5]);
    if (write(2, rb, rn) < 0) {}
  }
  uintptr_t pc = uc->uc_mcontext.pc;
  uintptr_t lr = uc->uc_mcontext.regs[30];
  uintptr_t tb = (uintptr_t)text_base;
  char buf[512];
  const char *where = "system/stack";
  unsigned long off = pc;
  if (pc >= tb && pc < tb + text_size) { where = "libunity"; off = pc - tb; }
  else if (g_il2_text && pc >= (uintptr_t)g_il2_text &&
           pc < (uintptr_t)g_il2_text + g_il2_size) {
    where = "libil2cpp"; off = pc - (uintptr_t)g_il2_text;
  }
  Dl_info info; const char *fn = "?"; const char *lib = "?";
  if (dladdr((void *)pc, &info)) { fn = info.dli_sname ? info.dli_sname : "?"; lib = info.dli_fname ? info.dli_fname : "?"; }
  uintptr_t sp = uc->uc_mcontext.sp;
  uintptr_t x29 = uc->uc_mcontext.regs[29];
  uintptr_t il2b = (uintptr_t)g_il2_text, il2e = il2b + g_il2_size;
  int n = snprintf(buf, sizeof(buf),
      "\n*** SEGV fault=%p pc=%s+0x%lx lr=%p ***\n    dladdr: %s in %s\n"
      "    sp=%p x29=%p x19=%p x20=%p\n",
      si->si_addr, where, off, (void *)lr, fn, lib,
      (void *)sp, (void *)x29, (void *)uc->uc_mcontext.regs[19],
      (void *)uc->uc_mcontext.regs[20]);
  if (write(2, buf, n) < 0) { /* ignore */ }
  /* varre a pilha (sp..sp+8KB) por enderecos de retorno na unity/il2cpp */
  int m = snprintf(buf, sizeof(buf), "    --- stack scan (return addrs) ---\n");
  if (write(2, buf, m) < 0) {}
  uintptr_t *s = (uintptr_t *)sp;
  int found = 0;
  for (int i = 0; i < 4096 && found < 40; i++) {
    uintptr_t v = s[i];
    if (v >= tb && v < tb + text_size) {
      m = snprintf(buf, sizeof(buf), "    [sp+0x%x] UNITY+0x%lx\n",
                   i * 8, v - tb);
      if (write(2, buf, m) < 0) {}
      found++;
    } else if (il2b && v >= il2b && v < il2e) {
      m = snprintf(buf, sizeof(buf), "    [sp+0x%x] IL2CPP+0x%lx\n",
                   i * 8, v - il2b);
      if (write(2, buf, m) < 0) {}
      found++;
    }
  }
  _exit(139);
}

static void install_segv_handler(void) {
  static char altstack[256 * 1024];
  stack_t ss = {.ss_sp = altstack, .ss_size = sizeof(altstack), .ss_flags = 0};
  sigaltstack(&ss, NULL);
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = on_segv;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGTRAP, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
}

/* Patcha um BRK (trap) no codigo carregado p/ confirmar que a execucao chega ali
   (debug de codigo na heap sem breakpoint do gdb). Dispara SIGTRAP -> on_segv. */
static void patch_brk(void *addr) {
  *(unsigned int *)addr = 0xd4200000u; /* brk #0 */
  __builtin___clear_cache((char *)addr, (char *)addr + 4);
}

/* registra+patcha um tracepoint (salva insn original p/ restaurar no SIGTRAP) */
static void add_tracepoint_k(void *addr, const char *label, int kind, void *arg) {
  if (g_ntp >= 8) return;
  unsigned int orig = *(unsigned int *)addr;
  g_tp[g_ntp].addr = (uintptr_t)addr;
  g_tp[g_ntp].orig = orig;
  g_tp[g_ntp].label = label;
  g_tp[g_ntp].hits = 0;
  g_tp[g_ntp].kind = kind;
  g_tp[g_ntp].arg = (uintptr_t)arg;
  g_ntp++;
  patch_brk(addr);
  fprintf(stderr, "[trace] tracepoint '%s' @ %p kind=%d (orig=%08x)\n", label, addr, kind, orig);
}
static void add_tracepoint(void *addr, const char *label) { add_tracepoint_k(addr, label, 0, 0); }

/* seta os 7 shims EGL no GOT do Unity + protege (GOTPROT). Usado pelo NODRIVER
   (FASE 2) E pelo driver (precisa antes do driver(1) p/ nao crashar no gfx). */
static void setup_egl_got(char *utbg, int gotprot) {
  struct { unsigned long off; void *fn; } gp[] = {
    {0x16f5698, (void *)egl_shim_MakeCurrent},
    {0x16f5740, (void *)egl_shim_GetError},
    {0x16f5308, (void *)egl_shim_GetCurrentContext},
    {0x16f59c8, (void *)egl_shim_GetCurrentSurface},
    {0x16f52f0, (void *)egl_shim_SwapBuffers},
    {0x16f5620, (void *)egl_shim_GetDisplay},
    {0x16f50d8, (void *)egl_shim_QuerySurface},
  };
  for (size_t gi = 0; gi < sizeof(gp) / sizeof(gp[0]); gi++) {
    *(void **)(utbg + gp[gi].off) = gp[gi].fn;
    g_egl_slots[g_egl_nslots++] = (uintptr_t)(utbg + gp[gi].off);
  }
  uintptr_t gp_page = ((uintptr_t)utbg + 0x16f4000) & ~0xFFFUL;
  if (gotprot) {
    g_got_lo = gp_page; g_got_hi = gp_page + 0x2000;
    if (mprotect((void *)gp_page, 0x2000, PROT_READ) != 0) perror("mprotect GOT");
  }
  fprintf(stderr, "[GOT] egl shims setados + GOT RO [%lx-%lx]\n", g_got_lo, g_got_hi);
}
/* aplica os 2 patches do Swappy (vsync waits que travam o loop). */
static void apply_swappy_patches(char *utb) {
  unsigned int *pw = (unsigned int *)(utb + 0x690f2c);
  if (*pw == 0xb5000108) { *pw = 0x14000008; __builtin___clear_cache((char *)pw, (char *)pw + 4); }
  unsigned int *pw2 = (unsigned int *)(utb + 0x6927c4);
  if (*pw2 == 0xb4ffff80) { *pw2 = 0xd503201f; __builtin___clear_cache((char *)pw2, (char *)pw2 + 4); }
  fprintf(stderr, "[boot] swappy patches aplicados\n");
}

/* Patcha a ENTRADA de uma funcao unity p/ retornar 0 cedo se x0(tipo)==NULL.
   Trampolim na heap (g_tramp_next): cbz x0,.ret ; <orig> ; b func+4 ; .ret mov w0,0;ret.
   So serve p/ funcoes cujo 1o opcode NAO e' PC-relative (stp/sub sp -> ok). */
static void patch_nullsafe_entry(char *func, char **tramp) {
  unsigned int *acc = (unsigned int *)func;
  unsigned int orig = acc[0];
  unsigned int *tr = (unsigned int *)*tramp;
  *tramp += 32;
  tr[0] = 0xB4000060;  /* cbz x0, +12 (.ret = tr[3]) */
  tr[1] = orig;        /* opcode original */
  long bo = (func + 4) - (char *)&tr[2];
  tr[2] = 0x14000000u | ((unsigned long)(bo / 4) & 0x03FFFFFFu); /* b func+4 */
  tr[3] = 0x52800000;  /* mov w0, #0 */
  tr[4] = 0xD65F03C0;  /* ret */
  long ao = (char *)tr - func;
  acc[0] = 0x14000000u | ((unsigned long)(ao / 4) & 0x03FFFFFFu); /* b tr */
  __builtin___clear_cache(func, func + 4);
  __builtin___clear_cache((char *)tr, (char *)tr + 20);
}

/* Patcha um acessor TINY de Type il2cpp (`<load de x0>; ret`) p/ NULL-safe:
   trampolim cbz x0,+8 (pula o load -> x0 fica 0) ; <orig load> ; ret. */
static void patch_nullsafe_accessor(char *func, char **tramp) {
  unsigned int *acc = (unsigned int *)func;
  unsigned int orig = acc[0];
  unsigned int *tr = (unsigned int *)*tramp;
  *tramp += 16;
  tr[0] = 0xB4000040;  /* cbz x0, +8 (pula o load) */
  tr[1] = orig;        /* load original (x0!=0) */
  tr[2] = 0xD65F03C0;  /* ret */
  long ao = (char *)tr - func;
  acc[0] = 0x14000000u | ((unsigned long)(ao / 4) & 0x03FFFFFFu); /* b tr */
  __builtin___clear_cache(func, func + 4);
  __builtin___clear_cache((char *)tr, (char *)tr + 12);
}

/* alvo de breakpoint p/ gdb (nao-inline); utb num global lido por simbolo */
volatile unsigned long g_recon_utb;
void recon_bp(void *utb) { g_recon_utb = (unsigned long)utb; __asm__ __volatile__("" ::: "memory"); }

#include <pthread.h>
/* WATCHDOG: thread independente que auto-encerra o processo apos N segundos, p/ NUNCA
   deixar o device travado num bloqueio/spin de userspace. (GPU-hang D-state nao salva.) */
static void *hk_watchdog(void *arg) {
  int secs = (int)(long)arg;
  for (int i = 0; i < secs; i++) { struct timespec ts = {1, 0}; nanosleep(&ts, NULL); }
  static const char m[] = "[WATCHDOG] tempo esgotado -> _exit\n";
  if (write(2, m, sizeof(m) - 1) < 0) {}
  _exit(70);
  return NULL;
}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  stderr_fake = stderr;
  setvbuf(stderr, NULL, _IOLBF, 0);
  g_main_tid = cur_tid();

  /* watchdog de seguranca (HK_WD segundos, default 30) -- iteracao sem travar o device */
  {
    int wd = getenv("HK_WD") ? atoi(getenv("HK_WD")) : 30;
    if (wd > 0) { pthread_t t; pthread_create(&t, NULL, hk_watchdog, (void *)(long)wd); pthread_detach(t); }
  }

  /* GC conservativo do il2cpp (BDWGC) escaneia pilha/heap e crasha de-referenciando
     candidatos-lixo no ambiente bionic-sobre-glibc. Desabilitar = jogo roda contInuo
     e estavel (usa +RAM, sobra nos 3.6GB do device). GC_KEEP=1 reativa o GC. */
  if (!getenv("GC_KEEP")) {
    setenv("GC_DONT_GC", "1", 1);
    setenv("GC_INITIAL_HEAP_SIZE", "536870912", 1);
  }

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

  /* === MULTI-MODULO: replica o libmain (carrega il2cpp -> unity) ===
     mmap em endereco FIXO -> utb deterministico entre runs (hw-breakpoint estavel) */
  size_t hs = (size_t)MEMORY_MB * 1024 * 1024;
  void *heap_il2 = mmap((void *)0x500000000UL, hs, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  void *heap_uni = mmap((void *)0x540000000UL, hs, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
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
  g_il2_text = text_base; g_il2_size = text_size;
  fprintf(stderr, "[A] libil2cpp OK (text=%p+0x%zx)\n", text_base, text_size);

  /* NULL-safe patch dos acessores TINY de Type il2cpp (crasham com tipo NULL =
     tipos C# stripados "Unable to find"). Trampolim na heap il2cpp (+-128MB). */
  {
    static char *il2tr;  /* cursor de trampolim na heap il2cpp */
    il2tr = (char *)g_il2_text + 0x7000000;
    unsigned long acc_offs[] = {0x6b1124, 0x6a68c0, 0x67cf44, 0x6a5e54};
    for (size_t i = 0; i < sizeof(acc_offs) / sizeof(acc_offs[0]); i++)
      patch_nullsafe_entry((char *)g_il2_text + acc_offs[i], &il2tr);
    (void)patch_nullsafe_accessor;
    fprintf(stderr, "[patch] %zu acessores Type il2cpp NULL-safe\n",
            sizeof(acc_offs) / sizeof(acc_offs[0]));
  }

  /* (B) libunity */
  fprintf(stderr, "[B] carregando libunity.so...\n");
  if (so_load(SO_NAME, heap_uni, hs) < 0) { fprintf(stderr, "FALHOU unity\n"); return 1; }
  if (so_relocate() < 0) { fprintf(stderr, "FALHOU reloc unity\n"); return 1; }
  so_resolve(dynlib_functions, dynlib_numfunctions, 0);
  so_execute_init_array();
  g_m_unity = so_save();
  fprintf(stderr, "[GOT] eglMakeCurrent GOT[+0x16f5698]=%p | eglCreateWindowSurface GOT[+0x16f54e8]=%p | shim_MC=%p\n",
          *(void **)((char *)text_base + 0x16f5698),
          *(void **)((char *)text_base + 0x16f54e8),
          (void *)egl_shim_MakeCurrent);
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
  egl_shim_open_input();  /* gamepad + teclado p/ nativeInjectEvent */
  fprintf(stderr, "[10c] window=%p (janela SDL criada?)\n", (void *)egl_shim_get_window());
  fflush(NULL);

  /* === BOOTSTRAP IL2CPP (experimento): inicializa o runtime antes dos natives.
     Em Unity IL2CPP o memory manager/runtime sobe via il2cpp_init + metadata. === */
  install_segv_handler();  /* captura SIGILL/SIGSEGV do bootstrap */
  {
    /* DRIVER do bootstrap real: unity+0x67c9dc (chama PlayerInitEngineNoGraphics
       em 0x530424). Recebe bool em w0 (tbz w0,#0 -> se 0 pula). Tenta w0=1. */
    void *render0 = jni_find_native("nativeRender");
    char *utb = (char *)render0 - 0x6949e8;
    g_recon_utb = (unsigned long)utb;  /* p/ scan no crash do bootstrap */
    void (*driver)(long) = (void (*)(long))(utb + 0x67c9dc);
    /* TRACEPOINT bisseccao: brk no endereco alvo (env BRK_OFF override) */
    unsigned long brk_off = 0;  /* 0 = sem trap; BRK_OFF env ativa bisseccao */
    const char *e = getenv("BRK_OFF");
    if (e) brk_off = strtoul(e, NULL, 0);
    if (brk_off) {
      fprintf(stderr, "[brk] patchando trap em unity+0x%lx\n", brk_off);
      patch_brk(utb + brk_off);
    }
    if (getenv("NODRIVER")) {
      fprintf(stderr, "[boot] NODRIVER set — pulando driver, usando natives diretos\n");
    } else {
      /* DRIVER: roda o loop COMPLETO (init+render+DRENA input). Precisa do setup
         da FASE 2 ANTES (EGL GOT + swappy) p/ nao crashar no gfx. A vantagem: o
         consumidor de input (que le getKeyCode) RODA no driver (≠ NODRIVER). */
      apply_swappy_patches(utb);
      setup_egl_got(utb, 1 /*GOTPROT*/);
      fprintf(stderr, "[boot] driver(PlayerInitEngine) @ %p — CHAMANDO driver(1)\n",
              (void *)driver);
      driver(1);
      fprintf(stderr, "[boot] driver RETORNOU — engine bootstrapped?!\n");
    }
  }
  fflush(NULL);

  /* === FASE 2: sequencia CORRETA do UnityPlayer ===
     initJni -> nativeResume -> nativeRecreateGfxState(surface) -> nativeRender.
     O bootstrap da engine (memory manager/gfx device) ocorre no
     nativeRecreateGfxState; chamar nativeRender antes = crash no ctx getter. */
  static long t = 0xA1, surf = 0x5F;

  /* helper de bp + reinstala handler */
  {
    void *render0 = jni_find_native("nativeRender");
    extern void recon_bp(void *);
    extern volatile unsigned long g_recon_utb;
    char *utb = (char *)render0 - 0x6949e8;
    g_recon_utb = (unsigned long)utb;
    fprintf(stderr, "[BP] utb=%p ctxget=%p alloc=%p allocinit=%p\n",
            utb, utb + 0x666a00, utb + 0x44b4e8, utb + 0x44bde8);
    install_segv_handler();

  }

  /* 11. nativeResume — coloca o player no estado resumed */
  void *resume = jni_find_native("nativeResume");
  fprintf(stderr, "[11] nativeResume @ %p\n", resume);
  if (resume) {
    void (*fn)(void *, void *) = (void (*)(void *, void *))resume;
    fprintf(stderr, "------ CHAMANDO nativeResume ------\n");
    fn(fake_env, &t);
    fprintf(stderr, "[11b] nativeResume RETORNOU\n");
  }

  /* 12. nativeRecreateGfxState(0, surface) — BOOTSTRAP da engine + gfx device */
  void *gfx = jni_find_native("nativeRecreateGfxState");
  fprintf(stderr, "[12] nativeRecreateGfxState @ %p\n", gfx);
  if (gfx) {
    void (*gf)(void *, void *, int, void *) =
        (void (*)(void *, void *, int, void *))gfx;
    fprintf(stderr, "------ CHAMANDO nativeRecreateGfxState(0, surface) ------\n");
    gf(fake_env, &t, 0, &surf);
    fprintf(stderr, "[12b] nativeRecreateGfxState RETORNOU — engine bootstrapped\n");
  }

  /* 13. nativeSendSurfaceChangedEvent */
  void *surfchg = jni_find_native("nativeSendSurfaceChangedEvent");
  if (surfchg) {
    void (*fn)(void *, void *) = (void (*)(void *, void *))surfchg;
    fprintf(stderr, "------ CHAMANDO nativeSendSurfaceChangedEvent ------\n");
    fn(fake_env, &t);
    fprintf(stderr, "[13b] surfaceChanged RETORNOU\n");
  }

  /* 13.5 nativeFocusChanged(true) — sem foco, o jogo pausa/espera no splash
     (UnityMain bloqueia em cond_wait infinito esperando o evento de foco). */
  void *focus = jni_find_native("nativeFocusChanged");
  fprintf(stderr, "[13c] nativeFocusChanged @ %p\n", focus);
  if (focus) {
    void (*fn)(void *, void *, int) = (void (*)(void *, void *, int))focus;
    fprintf(stderr, "------ CHAMANDO nativeFocusChanged(true) ------\n");
    fn(fake_env, &t, 1);
    fprintf(stderr, "[13d] nativeFocusChanged RETORNOU\n");
  }

  /* 14. LOOP nativeRender — agora a engine ja tem gfx state */
  void *render = jni_find_native("nativeRender");
  fprintf(stderr, "[14] nativeRender LOOP @ %p\n", render);
  if (render) {
    unsigned char (*rf)(void *, void *) = (unsigned char (*)(void *, void *))render;
    /* GOT egl: Unity sobrescreve os slots com Mali real (falha c/ handles fake)
       DENTRO do nativeRender. Solucao: setar nossos shims + mprotect RO -> os
       writes do GL-loader do Unity faltam e sao pulados no on_segv. */
    char *utbg = (char *)render - 0x6949e8;
    struct { unsigned long off; void *fn; } gp[] = {
      {0x16f5698, (void *)egl_shim_MakeCurrent},
      {0x16f5740, (void *)egl_shim_GetError},
      {0x16f5308, (void *)egl_shim_GetCurrentContext},
      {0x16f59c8, (void *)egl_shim_GetCurrentSurface},
      {0x16f52f0, (void *)egl_shim_SwapBuffers},
      {0x16f5620, (void *)egl_shim_GetDisplay},
      {0x16f50d8, (void *)egl_shim_QuerySurface},
    };
    extern uintptr_t g_got_lo, g_got_hi, g_egl_slots[8]; extern int g_egl_nslots;
    for (size_t gi = 0; gi < sizeof(gp) / sizeof(gp[0]); gi++) {
      *(void **)(utbg + gp[gi].off) = gp[gi].fn;
      g_egl_slots[g_egl_nslots++] = (uintptr_t)(utbg + gp[gi].off); /* proteger */
    }
    uintptr_t gp_page = ((uintptr_t)utbg + 0x16f4000) & ~0xFFFUL;
    if (getenv("GOTPROT")) {
      g_got_lo = gp_page; g_got_hi = gp_page + 0x2000;
      if (mprotect((void *)gp_page, 0x2000, PROT_READ) != 0) perror("mprotect GOT");
    }
    fprintf(stderr, "[GOT] egl shims setados + GOT RO [%lx-%lx]\n", g_got_lo, g_got_hi);

    /* === PATCH SWAPPY/CHOREOGRAPHER (unity+0x690f2c) ===
       nativeRender -> PlayerLoop -> "UnityChoreographer" espera VSync do
       Choreographer JAVA (via JNI, stubado) -> pthread_cond_wait INFINITO em
       0x690f3c. Patch: `cbnz x8,0x690f4c` (b5000108) -> `b 0x690f4c` (14000008)
       = trata vsync como SEMPRE pronto, pula a espera. Pacing = usleep no loop.
       Env SWAPPY_OFF=0 desliga o patch. */
    if (!getenv("SWAPPY_NOPATCH")) {
      unsigned int *pw = (unsigned int *)(utbg + 0x690f2c);
      if (*pw == 0xb5000108) {
        *pw = 0x14000008;
        __builtin___clear_cache((char *)pw, (char *)pw + 4);
        fprintf(stderr, "[SWAPPY] patch vsync-wait aplicado @ unity+0x690f2c\n");
      } else {
        fprintf(stderr, "[SWAPPY] AVISO: opcode inesperado @0x690f2c = %08x\n", *pw);
      }
      /* 2o vsync-wait do Choreographer @0x6927c4 (cbz x0,0x6927b4 = b4ffff80) -> NOP.
         Sem ele a engine trava no frame ~3 montando o UnityChoreographer (HandlerThread
         + getLooper + FrameCallback esperando VSync do Android que nunca vem). */
      unsigned int *pw2 = (unsigned int *)(utbg + 0x6927c4);
      if (*pw2 == 0xb4ffff80) {
        *pw2 = 0xd503201f;
        __builtin___clear_cache((char *)pw2, (char *)pw2 + 4);
        fprintf(stderr, "[SWAPPY] 2o patch vsync-wait aplicado @ unity+0x6927c4\n");
      } else {
        fprintf(stderr, "[SWAPPY] AVISO: opcode inesperado @0x6927c4 = %08x\n", *pw2);
      }
      /* 3o+4o waits do Choreographer: o skip simples REGREDIU (11->3 frames) -> a thread do
         Choreographer E NECESSARIA (nao da pra so pular). Gated atras de HK_SWAPPY34. */
      if (!getenv("HK_NOSWAPPY34")) { /* default ON: passa do Choreographer */
      unsigned int *pw3 = (unsigned int *)(utbg + 0x69159c);
      if (*pw3 == 0xb40000e8) {
        *pw3 = 0x14000007;
        __builtin___clear_cache((char *)pw3, (char *)pw3 + 4);
        fprintf(stderr, "[SWAPPY] 3o patch vsync-wait aplicado @ unity+0x69159c\n");
      } else {
        fprintf(stderr, "[SWAPPY] AVISO: opcode inesperado @0x69159c = %08x\n", *pw3);
      }
      /* 4o = O BLOQUEIO do Choreographer @0x693124: apos pthread_create, cond_wait esperando
         a thread sinalizar (Looper pronto) que nunca vem. Patch @0x693118
         (cbnz w8,0x693130 = 350000c8) -> b 0x693130 (14000006) = pula a espera da thread. */
      unsigned int *pw4 = (unsigned int *)(utbg + 0x693118);
      if (*pw4 == 0x350000c8) {
        *pw4 = 0x14000006;
        __builtin___clear_cache((char *)pw4, (char *)pw4 + 4);
        fprintf(stderr, "[SWAPPY] 4o patch (Choreographer getLooper) aplicado @ unity+0x693118\n");
      } else {
        fprintf(stderr, "[SWAPPY] AVISO: opcode inesperado @0x693118 = %08x\n", *pw4);
      }
      } /* fim HK_SWAPPY34 */
    }
    /* input: nativeInjectEvent(env, this, KeyEvent) — jni_shim responde os
       getAction/getKeyCode/etc. a partir de g_hk_inject (setado por evento SDL). */
    void *inject = jni_find_native("nativeInjectEvent");
    unsigned char (*injfn)(void *, void *, void *) =
        (unsigned char (*)(void *, void *, void *))inject;
    fprintf(stderr, "[14b] nativeInjectEvent @ %p (input ligado)\n", inject);
    /* nativeInjectEvent rejeita se alguma thread esta "trapped" (global em
       unity+0x1784A48 = ponteiro p/ thread trapped). Zeramos p/ liberar input.
       SWAPPY_NOPATCH-like: HK_NOTRAP=1 desliga. */
    void **trap_glob = (void **)(utbg + 0x1784a48);
    fprintf(stderr, "[14c] trapped-thread global @ unity+0x1784a48 = %p\n", *trap_glob);
    /* nativeInjectEvent: guard `bl is_trapped; cbnz w0,reject` em unity+0x694a64
       descarta o evento se a thread esta "trapped" (UnityMain marca [thread+264]!=0
       no nosso ambiente). Patch cbnz(0x35000080) -> NOP(0xd503201f) = sempre processa. */
    if (!getenv("HK_NOTRAP")) {
      unsigned int *pg = (unsigned int *)(utbg + 0x694a64);
      if (*pg == 0x35000080) {
        *pg = 0xd503201f;
        __builtin___clear_cache((char *)pg, (char *)pg + 4);
        fprintf(stderr, "[INPUT] patch guard trapped @ unity+0x694a64 (NOP)\n");
      } else fprintf(stderr, "[INPUT] AVISO opcode guard inesperado=%08x\n", *pg);
    }
    /* TRACEPOINTS p/ ver o caminho REAL do nativeInjectEvent: reject vs push.
       HK_TRACEINJECT=1 ativa. Cada um dispara 1x (auto-restaura no SIGTRAP). */
    if (getenv("HK_TRACEINJECT")) {
      add_tracepoint(utbg + 0x694a74, "INJECT-REJECT");
      add_tracepoint(utbg + 0x694b34, "INJECT-PUSH");
      /* GUARDADO: x0 = handler de input apos 0x116ec48; cbz x0,0x116f94c (drop) */
      add_tracepoint_k(utbg + 0x116f8ec, "HANDLER-NULL?", 1, utbg + 0x116f94c);
    }
    long evt_clock = 1;
    /* loop de render INFINITO (ate morto). HK_FRAMES limita (0=infinito). */
    long cap = getenv("HK_FRAMES") ? atol(getenv("HK_FRAMES")) : 0;
    long pace = getenv("HK_PACE_US") ? atol(getenv("HK_PACE_US")) : 14000;
    /* CHOREOGRAPHER nativo: chamar nOnChoreographer a cada frame = dirige o vsync do Swappy
       SEM Looper Java. Sem isso o jogo TRAVA no init esperando o frame-callback. HK_NOCHOREO desliga. */
    /* nOnChoreographer(env,thiz,x2) e um thunk que faz virtual-call em x2 (objeto Swappy),
       nao aceita frametime cru -> crasha. Precisa do objeto de contexto real. Gated HK_CHOREO. */
    void *choreo = 0;
    if (getenv("HK_CHOREO")) {
      so_module *sv = so_save(); so_use(g_m_unity);
      choreo = (void *)so_find_addr("Java_com_google_androidgamesdk_ChoreographerCallback_nOnChoreographer");
      so_use(sv); free(sv);
      fprintf(stderr, "[CHOREO] nOnChoreographer @ %p\n", choreo);
    }
    for (long frame = 0; cap == 0 || frame < cap; frame++) {
      if (choreo) ((void (*)(void *, void *, long long))choreo)(fake_env, &t, (long long)frame * 16666666LL);
      unsigned char r = rf(fake_env, &t);
      /* TESTE sem teclado fisico: HK_AUTOKEY=<keycode> aperta a tecla a cada ~3s
         (depois do frame 240) p/ validar a injecao por SSH (66=ENTER, 23=DPAD_CENTER) */
      static int autok = -2;
      if (autok == -2) autok = getenv("HK_AUTOKEY") ? atoi(getenv("HK_AUTOKEY")) : 0;
      if (!getenv("HK_NOTRAP")) *trap_glob = 0; /* destrava o guard de input */
      if (injfn && autok > 0 && frame > 240 && (frame % 200) == 0) {
        for (int ph = 0; ph < 2; ph++) {  /* down depois up */
          g_hk_inject.action = ph; g_hk_inject.keycode = autok; g_hk_inject.source = 0x101;
          g_hk_inject.deviceId = 0; g_hk_inject.eventTime = evt_clock;
          g_hk_inject.downTime = evt_clock; evt_clock++;
          g_hk_inject.repeat = 0; g_hk_inject.metaState = 0;
          g_in_inject = 1;
          unsigned char ir = injfn(fake_env, &t, hk_keyevent_object());
          g_in_inject = 0;
          fprintf(stderr, "[AUTOKEY] action=%d keycode=%d -> ret=%d\n", ph, autok, ir);
        }
      }
      /* bombeia eventos de teclado/gamepad -> injeta no Unity */
      int act, kc, src;
      while (injfn && egl_shim_next_key(&act, &kc, &src)) {
        g_hk_inject.action = act; g_hk_inject.keycode = kc; g_hk_inject.source = src;
        g_hk_inject.deviceId = (src == 0x401) ? 1 : 0;
        g_hk_inject.eventTime = evt_clock; g_hk_inject.downTime = evt_clock; evt_clock++;
        g_hk_inject.repeat = 0; g_hk_inject.metaState = 0;
        fprintf(stderr, "[INPUT] inject action=%d keycode=%d src=0x%x\n", act, kc, src);
        injfn(fake_env, &t, hk_keyevent_object());
      }
      if (frame == 0) fprintf(stderr, "[15] nativeRender #1 OK ret=%d!!!\n", r);
      else if (frame % 120 == 0) fprintf(stderr, "---- render frame %ld (vivo) ----\n", frame);
      if (pace > 0) usleep(pace); /* pacing manual ~60fps (sem vsync real) */
    }
    fprintf(stderr, "[16] render loop terminou (cap=%ld)\n", cap);
  }
  return 0;
}
