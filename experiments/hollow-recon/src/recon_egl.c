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

/* fwd: wrappers GL (HK Mali-450), definidos antes de recon_wire_egl */
static const unsigned char *my_glGetString(unsigned);
static void my_glGetIntegerv(unsigned, int *);
static void my_glShaderSource(unsigned, int, const char *const *, const int *);

/* stubs p/ funcoes GLES3 ausentes no Mali-450 (devolver STUB != NULL evita crash/loop) */
static long stub_gl_noop(void) { return 0; }
static void stub_glGetInternalformativ(unsigned t, unsigned f, unsigned pn, int n, int *p) {
  (void)t; (void)f; (void)pn;
  if (p) for (int i = 0; i < n; i++) p[i] = 0;  /* 0 samples = sem MSAA -> quebra o loop */
}
/* glTexStorage2D (GLES3, storage imutavel) -> aloca via glTexImage2D (GLES2). SEM isso a
   textura nao existe -> glTexSubImage2D falha -> UI amostra vazio -> TELA PRETA. */
static void (*real_glTexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *) = 0;
static void stub_glTexStorage2D(unsigned target, int levels, unsigned ifmt, int w, int h) {
  if (!real_glTexImage2D)
    real_glTexImage2D = (void (*)(unsigned, int, int, int, int, int, unsigned, unsigned,
                                  const void *))dlsym(RTLD_DEFAULT, "glTexImage2D");
  unsigned fmt = 0x1908, type = 0x1401;  /* GL_RGBA, GL_UNSIGNED_BYTE */
  switch (ifmt) {
    case 0x8051: case 0x1907: fmt = 0x1907; break;             /* RGB8/RGB -> GL_RGB */
    case 0x8D62: fmt = 0x1907; type = 0x8363; break;           /* RGB565 -> GL_RGB + 5_6_5 */
    case 0x8056: fmt = 0x1908; type = 0x8033; break;           /* RGBA4 -> GL_RGBA + 4_4_4_4 */
    case 0x8057: fmt = 0x1908; type = 0x8034; break;           /* RGB5_A1 -> GL_RGBA + 5_5_5_1 */
    case 0x8229: fmt = 0x1909; break;                          /* R8 -> GL_LUMINANCE */
    case 0x822B: fmt = 0x190A; break;                          /* RG8 -> GL_LUMINANCE_ALPHA */
    default: break;                                            /* RGBA8/RGBA -> GL_RGBA */
  }
  static int tn = 0;
  if (tn < 25) { fprintf(stderr, "[TEXSTOR] %dx%d ifmt=0x%x -> fmt=0x%x lv=%d\n", w, h, ifmt, fmt, levels); tn++; }
  if (levels < 1) levels = 1;
  if (!real_glTexImage2D) return;
  for (int i = 0; i < levels; i++) {
    int lw = w >> i, lh = h >> i; if (lw < 1) lw = 1; if (lh < 1) lh = 1;
    real_glTexImage2D(target, i, (int)fmt, lw, lh, 0, fmt, type, 0);
  }
}
/* SYNC objects (GLES3) ausentes no Mali-450: fingir "ja sinalizado" p/ a engine NAO
   esperar pra sempre (senao trava ANTES do eglSwapBuffers -> tela preta). */
static void *stub_glFenceSync(unsigned cond, unsigned flags) { (void)cond; (void)flags; return (void *)1; }
static unsigned stub_glClientWaitSync(void *s, unsigned f, unsigned long long t) {
  (void)s; (void)f; (void)t; return 0x911A; /* GL_ALREADY_SIGNALED */
}
static void stub_glGetSynciv(void *s, unsigned pname, int bufSize, int *length, int *values) {
  (void)s; (void)bufSize;
  if (pname == 0x9114 && values) values[0] = 0x9119; /* GL_SYNC_STATUS -> GL_SIGNALED */
  if (length) *length = 1;
}
/* DIAGNOSTICO de textura (caca a tela preta): o upload casa com a alocacao? ha erro GL? */
static int (*real_glGetError)(void) = 0;
static void (*real_glTexSubImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *) = 0;
static void diag_glTexSubImage2D(unsigned t, int lv, int xo, int yo, int w, int h,
                                 unsigned fmt, unsigned type, const void *px) {
  if (!real_glTexSubImage2D) real_glTexSubImage2D = (void (*)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *))dlsym(RTLD_DEFAULT, "glTexSubImage2D");
  if (!real_glGetError) real_glGetError = (int (*)(void))dlsym(RTLD_DEFAULT, "glGetError");
  if (real_glTexSubImage2D) real_glTexSubImage2D(t, lv, xo, yo, w, h, fmt, type, px);
  static int n = 0;
  if (n < 25) { int e = real_glGetError ? real_glGetError() : 0;
    fprintf(stderr, "[TEXSUB] %dx%d fmt=0x%x type=0x%x px=%p err=0x%x\n", w, h, fmt, type, px, e); n++; }
}
static void (*real_glCompressedTexImage2D)(unsigned, int, unsigned, int, int, int, int, const void *) = 0;
static void diag_glCompressedTexImage2D(unsigned t, int lv, unsigned ifmt, int w, int h, int b, int sz, const void *d) {
  if (!real_glCompressedTexImage2D) real_glCompressedTexImage2D = (void (*)(unsigned, int, unsigned, int, int, int, int, const void *))dlsym(RTLD_DEFAULT, "glCompressedTexImage2D");
  static int n = 0;
  if (n < 25) { fprintf(stderr, "[CTEX] %dx%d ifmt=0x%x sz=%d\n", w, h, ifmt, sz); n++; }
  if (real_glCompressedTexImage2D) real_glCompressedTexImage2D(t, lv, ifmt, w, h, b, sz, d);
}
/* FBO: a render target esta COMPLETA? (incompleta = render-to-texture preto) */
static unsigned (*real_glCheckFramebufferStatus)(unsigned) = 0;
static unsigned diag_glCheckFramebufferStatus(unsigned target) {
  if (!real_glCheckFramebufferStatus) real_glCheckFramebufferStatus = (unsigned (*)(unsigned))dlsym(RTLD_DEFAULT, "glCheckFramebufferStatus");
  unsigned s = real_glCheckFramebufferStatus ? real_glCheckFramebufferStatus(target) : 0x8CD5;
  static int n = 0;
  if (n < 20) { fprintf(stderr, "[FBO] CheckStatus -> 0x%x %s\n", s, s == 0x8CD5 ? "COMPLETE" : "INCOMPLETE!"); n++; }
  return s;
}
/* glMapBufferRange/glUnmapBuffer (GLES3) -> EMULA com malloc + glBufferSubData no unmap.
   E assim que a Unity escreve GEOMETRIA DINAMICA; no-op = buffers vazios = TELA PRETA. */
static struct { unsigned target; long offset; long length; void *ptr; } g_mapbuf[16];
static void (*real_glBufferSubData)(unsigned, long, long, const void *) = 0;
static void (*real_glBufferData)(unsigned, long, const void *, unsigned) = 0;
static void *emul_glMapBufferRange(unsigned target, long offset, long length, unsigned access) {
  (void)access;
  void *p = malloc(length > 0 ? (size_t)length : 1);
  for (int i = 0; i < 16; i++)
    if (!g_mapbuf[i].ptr) { g_mapbuf[i].target = target; g_mapbuf[i].offset = offset;
                            g_mapbuf[i].length = length; g_mapbuf[i].ptr = p; break; }
  static int n = 0; if (n < 8) { fprintf(stderr, "[MAPBUF] map t=0x%x off=%ld len=%ld\n", target, offset, length); n++; }
  return p;
}
static unsigned char emul_glUnmapBuffer(unsigned target) {
  if (!real_glBufferSubData) real_glBufferSubData = (void (*)(unsigned, long, long, const void *))dlsym(RTLD_DEFAULT, "glBufferSubData");
  for (int i = 0; i < 16; i++)
    if (g_mapbuf[i].ptr && g_mapbuf[i].target == target) {
      static int dn = 0;
      if (target == 0x8892 && dn < 4 && g_mapbuf[i].length >= 256) { /* ARRAY_BUFFER: dump vertices */
        float *fp = (float *)g_mapbuf[i].ptr;
        int nf = g_mapbuf[i].length / 4, nz = 0;
        for (int k = 0; k < nf; k++) if (fp[k] != 0.0f) nz++;
        int m = nf / 2;
        fprintf(stderr, "[VTX] len=%ld nao-zero=%d/%d | f0-5: %.2f %.2f %.2f %.2f %.2f %.2f | meio[%d]: %.2f %.2f %.2f %.2f\n",
                g_mapbuf[i].length, nz, nf, fp[0], fp[1], fp[2], fp[3], fp[4], fp[5], m, fp[m], fp[m+1], fp[m+2], fp[m+3]);
        dn++;
      }
      if (!real_glBufferData) real_glBufferData = (void (*)(unsigned, long, const void *, unsigned))dlsym(RTLD_DEFAULT, "glBufferData");
      if (!real_glGetError) real_glGetError = (int (*)(void))dlsym(RTLD_DEFAULT, "glGetError");
      if (real_glGetError) real_glGetError(); /* limpa */
      /* glBufferData (re-aloca + sobe) p/ offset 0 -- robusto se a Unity nao pre-alocou via glBufferData */
      if (g_mapbuf[i].offset == 0 && real_glBufferData)
        real_glBufferData(target, g_mapbuf[i].length, g_mapbuf[i].ptr, 0x88E8); /* GL_DYNAMIC_DRAW */
      else if (real_glBufferSubData)
        real_glBufferSubData(target, g_mapbuf[i].offset, g_mapbuf[i].length, g_mapbuf[i].ptr);
      static int ue = 0;
      if (ue < 6 && real_glGetError) { int e = real_glGetError();
        fprintf(stderr, "[UNMAP] t=0x%x off=%ld len=%ld upload_err=0x%x\n", target, g_mapbuf[i].offset, g_mapbuf[i].length, e); ue++; }
      free(g_mapbuf[i].ptr); g_mapbuf[i].ptr = 0;
      return 1; /* GL_TRUE */
    }
  return 1;
}
/* DIAG: os draws acontecem e passam? (render preto = ou nao desenha ou erra) */
static void (*real_glDrawElements)(unsigned, int, unsigned, const void *) = 0;
static void diag_glDrawElements(unsigned mode, int count, unsigned type, const void *idx) {
  if (!real_glDrawElements) real_glDrawElements = (void (*)(unsigned, int, unsigned, const void *))dlsym(RTLD_DEFAULT, "glDrawElements");
  if (!real_glGetError) real_glGetError = (int (*)(void))dlsym(RTLD_DEFAULT, "glGetError");
  unsigned pre = real_glGetError ? (unsigned)real_glGetError() : 0; /* limpa + captura acumulado */
  if (real_glDrawElements) real_glDrawElements(mode, count, type, idx);
  static int n = 0;
  if (n < 25) { unsigned post = real_glGetError ? (unsigned)real_glGetError() : 0;
    fprintf(stderr, "[DRAW] mode=0x%x count=%d type=0x%x pre=0x%x draw_err=0x%x\n", mode, count, type, pre, post); n++; }
}
/* DIAG: as matrizes (projecao/modelview) sao setadas? (nao setadas -> gl_Position degenerado) */
static void (*real_glUniform4fv)(int, int, const float *) = 0;
static void diag_glUniform4fv(int loc, int count, const float *v) {
  if (!real_glUniform4fv) real_glUniform4fv = (void (*)(int, int, const float *))dlsym(RTLD_DEFAULT, "glUniform4fv");
  static int n = 0;
  if (n < 20 && v) {
    if (count == 4)
      fprintf(stderr, "[U4FV] loc=%d MAT [%.2f %.2f %.2f %.2f][%.2f %.2f %.2f %.2f][%.2f %.2f %.2f %.2f][%.2f %.2f %.2f %.2f]\n",
              loc, v[0],v[1],v[2],v[3], v[4],v[5],v[6],v[7], v[8],v[9],v[10],v[11], v[12],v[13],v[14],v[15]);
    else fprintf(stderr, "[U4FV] loc=%d count=%d v=%.2f %.2f %.2f %.2f\n", loc, count, v[0], v[1], v[2], v[3]);
    n++;
  }
  if (real_glUniform4fv) real_glUniform4fv(loc, count, v);
}
static void (*real_glUniformMatrix4fv)(int, int, unsigned char, const float *) = 0;
static void diag_glUniformMatrix4fv(int loc, int count, unsigned char tr, const float *v) {
  if (!real_glUniformMatrix4fv) real_glUniformMatrix4fv = (void (*)(int, int, unsigned char, const float *))dlsym(RTLD_DEFAULT, "glUniformMatrix4fv");
  static int n = 0;
  if (n < 12 && v) { fprintf(stderr, "[UMTX] loc=%d count=%d m0-3=%.2f %.2f %.2f %.2f\n", loc, count, v[0], v[1], v[2], v[3]); n++; }
  if (real_glUniformMatrix4fv) real_glUniformMatrix4fv(loc, count, tr, v);
}
/* DIAG: viewport/scissor pequeno = recorta os draws -> render esparso */
static void (*real_glViewport)(int, int, int, int) = 0;
static void diag_glViewport(int x, int y, int w, int h) {
  if (!real_glViewport) real_glViewport = (void (*)(int, int, int, int))dlsym(RTLD_DEFAULT, "glViewport");
  static int n = 0; if (n < 15) { fprintf(stderr, "[VIEWPORT] %d %d %d %d\n", x, y, w, h); n++; }
  if (real_glViewport) real_glViewport(x, y, w, h);
}
static void (*real_glScissor)(int, int, int, int) = 0;
static void diag_glScissor(int x, int y, int w, int h) {
  if (!real_glScissor) real_glScissor = (void (*)(int, int, int, int))dlsym(RTLD_DEFAULT, "glScissor");
  static int n = 0; if (n < 15) { fprintf(stderr, "[SCISSOR] %d %d %d %d\n", x, y, w, h); n++; }
  if (real_glScissor) real_glScissor(x, y, w, h);
}

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
  /* HK Mali-450: a Unity pega o GL via dlsym -> intercepta AQUI.
     glGetString mente "GLES 3.0" (Unity carrega shaders GLES3); glShaderSource captura. */
  if (sym) {
    if (!getenv("HK_NOSPOOF") && strcmp(sym, "glGetString") == 0)   return (void *)my_glGetString;
    if (!getenv("HK_NOSPOOF") && strcmp(sym, "glGetIntegerv") == 0) return (void *)my_glGetIntegerv;
    if (strcmp(sym, "glShaderSource") == 0)                          return (void *)my_glShaderSource;
    if (strcmp(sym, "glTexSubImage2D") == 0)                         return (void *)diag_glTexSubImage2D;
    if (strcmp(sym, "glCompressedTexImage2D") == 0)                  return (void *)diag_glCompressedTexImage2D;
    if (strcmp(sym, "glCheckFramebufferStatus") == 0)               return (void *)diag_glCheckFramebufferStatus;
    if (strcmp(sym, "glDrawElements") == 0)                          return (void *)diag_glDrawElements;
    if (strcmp(sym, "glUniform4fv") == 0)                            return (void *)diag_glUniform4fv;
    if (strcmp(sym, "glUniformMatrix4fv") == 0)                      return (void *)diag_glUniformMatrix4fv;
    if (strcmp(sym, "glViewport") == 0)                              return (void *)diag_glViewport;
    if (strcmp(sym, "glScissor") == 0)                               return (void *)diag_glScissor;
  }
  void *r = dlsym(h, sym);
  /* HK Mali-450: funcao GLES3 ausente -> devolve STUB (nao NULL) p/ evitar crash/loop.
     glGetInternalformativ zera params (sem MSAA, quebra o loop); demais = no-op retorna 0. */
  if (!r && sym && sym[0] == 'g' && sym[1] == 'l') {
    if (strcmp(sym, "glGetInternalformativ") == 0)   r = (void *)stub_glGetInternalformativ;
    else if (strcmp(sym, "glTexStorage2D") == 0)     r = (void *)stub_glTexStorage2D;
    else if (strcmp(sym, "glFenceSync") == 0)        r = (void *)stub_glFenceSync;
    else if (strcmp(sym, "glClientWaitSync") == 0)   r = (void *)stub_glClientWaitSync;
    else if (strcmp(sym, "glGetSynciv") == 0)        r = (void *)stub_glGetSynciv;
    else if (strcmp(sym, "glMapBufferRange") == 0)   r = (void *)emul_glMapBufferRange;
    else if (strcmp(sym, "glUnmapBuffer") == 0)      r = (void *)emul_glUnmapBuffer;
    else                                             r = (void *)stub_gl_noop;
    static int n = 0;
    if (n++ < 60) fprintf(stderr, "[gl stub] %s\n", sym);
  }
  return r;
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

/* ---- HK Mali-450: spoof de versao GLES + captura de shaders ----
   O Mali-450 e GLES2; o blob do HK so tem shaders GLES3 (platform 5 = GLES2
   "not available in shader blob"). Mentindo GL_VERSION="OpenGL ES 3.0", a Unity
   carrega os shaders GLES3 e os manda pro glShaderSource -> capturamos e (depois)
   traduzimos GLES3->GLES2. Desliga com HK_NOSPOOF=1. */
static const unsigned char *(*real_glGetString)(unsigned) = 0;
static const unsigned char *my_glGetString(unsigned name) {
  if (!real_glGetString)
    real_glGetString = (const unsigned char *(*)(unsigned))dlsym(RTLD_DEFAULT, "glGetString");
  if (!getenv("HK_NOSPOOF")) {
    if (name == 0x1F02) return (const unsigned char *)"OpenGL ES 3.0";           /* GL_VERSION */
    if (name == 0x8B8C) return (const unsigned char *)"OpenGL ES GLSL ES 3.00";  /* GL_SHADING_LANGUAGE_VERSION */
  }
  return real_glGetString ? real_glGetString(name) : (const unsigned char *)"";
}
static void (*real_glGetIntegerv)(unsigned, int *) = 0;
static void my_glGetIntegerv(unsigned pname, int *p) {
  if (!real_glGetIntegerv)
    real_glGetIntegerv = (void (*)(unsigned, int *))dlsym(RTLD_DEFAULT, "glGetIntegerv");
  if (!getenv("HK_NOSPOOF")) {
    if (pname == 0x821B) { if (p) p[0] = 3; return; }  /* GL_MAJOR_VERSION */
    if (pname == 0x821C) { if (p) p[0] = 0; return; }  /* GL_MINOR_VERSION */
  }
  if (real_glGetIntegerv) real_glGetIntegerv(pname, p);
}
static void (*real_glShaderSource)(unsigned, int, const char *const *, const int *) = 0;
static int g_shdump = 0;
/* substitui TODAS as ocorrencias de `from` por `to` -> novo buffer malloc'd */
static char *str_rep(const char *s, const char *from, const char *to) {
  size_t fl = strlen(from), tl = strlen(to), cnt = 0;
  for (const char *p = s; (p = strstr(p, from)) != NULL; p += fl) cnt++;
  char *out = (char *)malloc(strlen(s) + (tl > fl ? (tl - fl) * cnt : 0) + 1);
  char *o = out;
  for (const char *p = s; *p;) {
    if (!strncmp(p, from, fl)) { memcpy(o, to, tl); o += tl; p += fl; }
    else *o++ = *p++;
  }
  *o = 0;
  return out;
}

/* TRADUTOR: shader Unity GLES3 (#version 300 es) -> GLES2 (#version 100). Mali-450. */
static char *translate_gles3_gles2(const char *src) {
  int is_frag = (strstr(src, "gl_Position") == NULL); /* sem gl_Position = fragment */
  char *s = str_rep(src, "#version 300 es", "#version 100");
  char *t = str_rep(s, "texture(", "texture2D("); free(s); s = t;
  if (is_frag) {
    /* GLES2 nao tem `out` de fragment: SV_Target0 -> gl_FragColor (built-in) */
    t = str_rep(s, "layout(location = 0) out mediump vec4 SV_Target0;", ""); free(s); s = t;
    t = str_rep(s, "layout(location = 0) out highp vec4 SV_Target0;", ""); free(s); s = t;
    t = str_rep(s, "SV_Target0", "gl_FragColor"); free(s); s = t;
    t = str_rep(s, "\nin ", "\nvarying "); free(s); s = t; /* entradas de fragment */
    if (getenv("HK_FORCERED")) { /* DEBUG: forca saida vermelha -> testa geometria vs textura */
      t = str_rep(s, "return;", "gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); return;"); free(s); s = t;
    } else if (getenv("HK_FORCETEX") && strstr(s, "_MainTex") && strstr(s, "vs_TEXCOORD0")) {
      /* DEBUG: saida = textura crua -> testa se a textura subiu (preta=upload falhou) */
      t = str_rep(s, "return;", "gl_FragColor = texture2D(_MainTex, vs_TEXCOORD0.xy); return;"); free(s); s = t;
    } else if (getenv("HK_FORCECOL") && strstr(s, "vs_COLOR0")) {
      /* DEBUG: saida = cor do vertice -> testa se o atributo COLOR chega (preto=atributo zerado) */
      t = str_rep(s, "return;", "gl_FragColor = vec4(vs_COLOR0.xyz, 1.0); return;"); free(s); s = t;
    }
  } else {
    t = str_rep(s, "\nin ", "\nattribute "); free(s); s = t;  /* entradas de vertex */
    t = str_rep(s, "\nout ", "\nvarying "); free(s); s = t;   /* saidas de vertex */
    if (getenv("HK_VTXRAW")) { /* DEBUG: bypassa as matrizes -> gl_Position = posicao crua */
      t = str_rep(s, "return;", "gl_Position = in_POSITION0; return;"); free(s); s = t;
    }
  }
  return s;
}

static void my_glShaderSource(unsigned sh, int count, const char *const *str,
                              const int *len) {
  if (!real_glShaderSource)
    real_glShaderSource = (void (*)(unsigned, int, const char *const *,
                                    const int *))dlsym(RTLD_DEFAULT, "glShaderSource");
  /* concatena os pedacos num buffer unico */
  size_t total = 0;
  for (int i = 0; i < count; i++)
    if (str[i]) total += (len && len[i] > 0) ? (size_t)len[i] : strlen(str[i]);
  char *src = (char *)malloc(total + 1);
  size_t o = 0;
  for (int i = 0; i < count; i++)
    if (str[i]) {
      size_t l = (len && len[i] > 0) ? (size_t)len[i] : strlen(str[i]);
      memcpy(src + o, str[i], l); o += l;
    }
  src[o] = 0;
  char *tr = translate_gles3_gles2(src);
  if (g_shdump < 80) {
    char p[160]; int id = g_shdump++;
    snprintf(p, sizeof p, "/storage/hollow-recon/shdump/sh_%03d.glsl", id);
    FILE *f = fopen(p, "w"); if (f) { fputs(src, f); fclose(f); }
    snprintf(p, sizeof p, "/storage/hollow-recon/shdump/sh_%03d.gles2", id);
    f = fopen(p, "w"); if (f) { fputs(tr, f); fclose(f); }
  }
  if (real_glShaderSource) {
    const char *one = tr; int onelen = (int)strlen(tr);
    real_glShaderSource(sh, 1, &one, &onelen);
  }
  free(src); free(tr);
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
  /* HK Mali-450: mente versao GLES3 (Unity carrega shaders GLES3) + captura shaders */
  set_import("glGetString", (void *)my_glGetString);
  set_import("glGetIntegerv", (void *)my_glGetIntegerv);
  set_import("glShaderSource", (void *)my_glShaderSource);
  /* shim pthread bionic<->glibc (SIGBUS no glibc NOVO do Amlogic-no).
     Amlogic-old (.87, Mali-450) tem glibc antigo -> NAO precisa e QUEBRA o GL setup.
     So liga com HK_PTHREAD_SHIM=1 (Amlogic-no). */
  if (getenv("HK_PTHREAD_SHIM")) {
    void recon_wire_pthread(void (*)(const char *, void *));
    recon_wire_pthread(set_import);
  }
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
