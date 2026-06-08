#include <unistd.h>
/*
 * recon_egl.c — liga o EGL/ANativeWindow do Unity ao nosso egl_shim (SDL2/Mali).
 * Override das entradas da import table (chamar ANTES de so_resolve).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include "egl_shim.h"
#include "so_util.h"
#include "imports.h"

/* bridge dlopen/dlsym -> modulos do so-loader (libil2cpp/libunity) */
#include <stdlib.h>
extern so_module *g_m_il2cpp, *g_m_unity;

static void *my_dlopen(const char *name, int flag) {
  fprintf(stderr, "[dlopen] \"%s\"\n", name ? name : "(self)");
  if (name && strstr(name, "libil2cpp")) return (void *)g_m_il2cpp;
  if (name && strstr(name, "libunity"))  return (void *)g_m_unity;
  /* dlopen("")/NULL = escopo GLOBAL: no Android il2cpp esta no global (RTLD_GLOBAL).
     Unity faz dlopen("") p/ achar os simbolos il2cpp_* -> devolver g_m_il2cpp. */
  if (!name || name[0] == '\0') return (void *)g_m_il2cpp;
  return dlopen(name, flag); /* real p/ libdl/libc/etc */
}
static void *my_dlsym(void *h, const char *sym) {
  /* egl* -> NOSSOS shims (Unity faz dlopen(libEGL)+dlsym; o Mali real falha com
     nossos handles fake -> eglMakeCurrent FALSE -> "[EGL] Unable to acquire" smash) */
  if (sym && sym[0] == 'e' && sym[1] == 'g' && sym[2] == 'l') {
    void *p = egl_shim_GetProcAddress(sym);
    if (p) { fprintf(stderr, "[dlsym egl] %s -> shim %p\n", sym, p); return p; }
  }
  if ((h == (void *)g_m_il2cpp || h == (void *)g_m_unity) && g_m_il2cpp) {
    so_module *save = so_save();      /* salva modulo ativo */
    so_use((so_module *)h);
    uintptr_t a = so_find_addr(sym);
    so_use(save);                     /* restaura */
    free(save);
    if (!a) fprintf(stderr, "[dlsym modulo] %s -> NULL\n", sym ? sym : "?");
    return (void *)a;
  }
  return dlsym(h, sym);
}

/* ---- __android_log_* REAL (revela mensagens de erro do il2cpp/unity) ---- */
#include <stdarg.h>
static int my_alog_print(int prio, const char *tag, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  return 0;
}
static int my_alog_write(int prio, const char *tag, const char *text) {
  fprintf(stderr, "[ALOG:%d %s] %s\n", prio, tag ? tag : "?", text ? text : "");
  return 0;
}
static int my_alog_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
  fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  return 0;
}
static void my_abort_msg(const char *msg) {
  fprintf(stderr, "[ABORT_MSG] %s\n", msg ? msg : "(null)");
}

/* Intercepta __stack_chk_fail p/ revelar a funcao que estourou (return addr). */
static void my_stack_chk_fail(void) {
  void *ra = __builtin_return_address(0);
  uintptr_t r = (uintptr_t)ra;
  fprintf(stderr, "[STACK_CHK_FAIL] smash em ra=%p unity+0x%lx il2+0x%lx\n",
          ra, (unsigned long)(r - 0x540000000UL), (unsigned long)(r - 0x500000000UL));
  /* dump da pilha do frame que estourou (ASCII) -> revela a string/dado */
  unsigned char *fp = (unsigned char *)__builtin_frame_address(0);
  fprintf(stderr, "[STACK_CHK_FAIL] frame dump (ASCII):\n");
  for (int row = 0; row < 24; row++) {
    char line[80]; int p = 0;
    p += snprintf(line + p, sizeof line - p, "  +0x%03x: ", row * 32);
    for (int i = 0; i < 32; i++) {
      unsigned char c = fp[row * 32 + i];
      line[p++] = (c >= 32 && c < 127) ? c : '.';
    }
    line[p++] = '\n';
    if (write(2, line, p) < 0) {}
  }
  _exit(66);
}

/* sigaction bionic-safe. RAIZ DO CRASH: Unity (bionic) passa oldact = buffer de
   32 bytes (struct sigaction bionic). O sigaction do glibc escreve 152 bytes ->
   ESTOURA a pilha e corrompe o x30 salvo -> ret p/ lixo. Aqui escrevemos SO 32
   bytes (SIG_DFL) e nao instalamos o handler do Unity (evita mascarar crashes). */
#include <string.h>
#include <signal.h>
/* struct sigaction BIONIC (aarch64): flags@0, handler@8, mask@16(8B), restorer@24 (32B).
   GLIBC: handler@0, mask@8(128B), flags@136, restorer@144 (~152B). */
extern int sigaction(int, const struct sigaction *, struct sigaction *);
static int my_sigaction(int sig, const void *act, void *oldact) {
  fprintf(stderr, "[sigaction] sig=%d act=%p\n", sig, act);
  /* sinais de CRASH: mantemos NOSSO handler (nao deixa Unity instalar o dele) */
  if (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGABRT ||
      sig == SIGFPE) {
    if (oldact) memset(oldact, 0, 32);
    return 0;
  }
  /* demais sinais (GC do il2cpp: suspensao de thread etc.): TRADUZ bionic->glibc */
  struct sigaction g; memset(&g, 0, sizeof g);
  struct sigaction og; memset(&og, 0, sizeof og);
  if (act) {
    const unsigned char *b = (const unsigned char *)act;
    unsigned int bflags = *(const unsigned int *)(b + 0);
    void *bhandler = *(void *const *)(b + 8);
    unsigned long bmask = *(const unsigned long *)(b + 16);
    g.sa_flags = (int)(bflags & ~0x04000000u); /* tira SA_RESTORER (glibc usa o seu) */
    if (bflags & 0x00000004u /*SA_SIGINFO*/) g.sa_sigaction = (void (*)(int, siginfo_t *, void *))bhandler;
    else g.sa_handler = (void (*)(int))bhandler;
    memcpy(&g.sa_mask, &bmask, sizeof bmask); /* expande 8B->128B (resto 0) */
  }
  int r = sigaction(sig, act ? &g : NULL, oldact ? &og : NULL);
  if (oldact) { /* glibc->bionic */
    unsigned char *b = (unsigned char *)oldact;
    memset(b, 0, 32);
    *(unsigned int *)(b + 0) = (unsigned int)og.sa_flags;
    *(void **)(b + 8) = (og.sa_flags & 0x00000004) ? (void *)og.sa_sigaction : (void *)og.sa_handler;
    memcpy(b + 16, &og.sa_mask, 8);
  }
  return r;
}
static void *my_signal(int sig, void *handler) {
  (void)sig; (void)handler;
  return (void *)0; /* SIG_DFL */
}

/* ---- ANativeWindow shim (Unity usa no Surface) ---- */
static void *ANW_fromSurface(void *env, void *surface) {
  (void)env; (void)surface;
  void *w = egl_shim_get_window();
  return w ? w : (void *)0x57; /* nao-nulo */
}
extern int egl_shim_width(void), egl_shim_height(void);
static int ANW_getWidth(void *w)  { (void)w; return egl_shim_width(); }
static int ANW_getHeight(void *w) { (void)w; return egl_shim_height(); }
static int ANW_setBuffersGeometry(void *w, int a, int b, int c) {
  (void)w; (void)a; (void)b; (void)c; return 0;
}
static void ANW_acquire(void *w) { (void)w; }
static void ANW_release(void *w) { (void)w; }

static void set_import(const char *name, void *fn) {
  for (size_t i = 0; i < dynlib_numfunctions; i++)
    if (strcmp(dynlib_functions[i].symbol, name) == 0) {
      dynlib_functions[i].func = (uintptr_t)fn;
      return;
    }
}

void recon_wire_egl(void) {
  set_import("eglGetDisplay", (void *)egl_shim_GetDisplay);
  set_import("eglInitialize", (void *)egl_shim_Initialize);
  set_import("eglTerminate", (void *)egl_shim_Terminate);
  set_import("eglChooseConfig", (void *)egl_shim_ChooseConfig);
  set_import("eglCreateWindowSurface", (void *)egl_shim_CreateWindowSurface);
  set_import("eglCreatePbufferSurface", (void *)egl_shim_CreatePbufferSurface);
  set_import("eglCreateContext", (void *)egl_shim_CreateContext);
  set_import("eglMakeCurrent", (void *)egl_shim_MakeCurrent);
  set_import("eglSwapBuffers", (void *)egl_shim_SwapBuffers);
  set_import("eglDestroySurface", (void *)egl_shim_DestroySurface);
  set_import("eglDestroyContext", (void *)egl_shim_DestroyContext);
  set_import("eglQuerySurface", (void *)egl_shim_QuerySurface);
  set_import("eglGetConfigAttrib", (void *)egl_shim_GetConfigAttrib);
  set_import("eglGetError", (void *)egl_shim_GetError);
  set_import("eglGetProcAddress", (void *)egl_shim_GetProcAddress);
  set_import("eglQueryString", (void *)egl_shim_QueryString);
  set_import("eglSwapInterval", (void *)egl_shim_SwapInterval);
  set_import("eglGetCurrentContext", (void *)egl_shim_GetCurrentContext);
  set_import("eglGetCurrentSurface", (void *)egl_shim_GetCurrentSurface);
  set_import("eglSurfaceAttrib", (void *)egl_shim_SurfaceAttrib);
  /* ANativeWindow */
  set_import("ANativeWindow_fromSurface", (void *)ANW_fromSurface);
  set_import("ANativeWindow_getWidth", (void *)ANW_getWidth);
  set_import("ANativeWindow_getHeight", (void *)ANW_getHeight);
  set_import("ANativeWindow_setBuffersGeometry", (void *)ANW_setBuffersGeometry);
  set_import("ANativeWindow_acquire", (void *)ANW_acquire);
  set_import("ANativeWindow_release", (void *)ANW_release);
  /* __android_log_* REAL — revela erros do il2cpp_init */
  set_import("__android_log_print", (void *)my_alog_print);
  set_import("__android_log_write", (void *)my_alog_write);
  set_import("__android_log_vprint", (void *)my_alog_vprint);
  set_import("android_set_abort_message", (void *)my_abort_msg);
  /* bloqueia o crash handler do Unity (revela o crash real no nosso on_segv) */
  set_import("sigaction", (void *)my_sigaction);
  set_import("signal", (void *)my_signal);
  set_import("__stack_chk_fail", (void *)my_stack_chk_fail);
  /* dlopen/dlsym logados (ver libil2cpp carregar) */
  set_import("dlopen", (void *)my_dlopen);
  set_import("dlsym", (void *)my_dlsym);
  /* shim pthread bionic<->glibc (SIGBUS no glibc novo do Amlogic-no) */
  void recon_wire_pthread(void (*)(const char *, void *));
  recon_wire_pthread(set_import);
  fprintf(stderr, "[egl] wired: 20 EGL + 6 ANativeWindow + dlopen/dlsym -> egl_shim\n");
  /* DEBUG: confirmar func de eglMakeCurrent vs eglCreateWindowSurface na tabela */
  for (size_t i = 0; i < dynlib_numfunctions; i++) {
    if (!strcmp(dynlib_functions[i].symbol, "eglMakeCurrent") ||
        !strcmp(dynlib_functions[i].symbol, "eglCreateWindowSurface"))
      fprintf(stderr, "[egl-dbg] %s -> %p (shim_MC=%p shim_WS=%p)\n",
              dynlib_functions[i].symbol, (void *)dynlib_functions[i].func,
              (void *)egl_shim_MakeCurrent, (void *)egl_shim_CreateWindowSurface);
  }
}
