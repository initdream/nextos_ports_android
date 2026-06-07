/*
 * stubs.c -- símbolos bionic que NÃO existem no glibc nem no libc++ do NDK.
 *
 * A grande maioria dos imports do libreVC.so resolve sozinha:
 *   - libc/libm/libdl/SDL2/GLESv2/OpenAL/mpg123  -> dlsym(RTLD_DEFAULT) das libs
 *     do device pré-carregadas (ver main.c).
 *   - std::__ndk1::* / operator new|delete / __cxa_* / _Unwind_*  -> módulo
 *     libc++_shared.so so-loadado (ver main.c, so_snapshot_symbols).
 *
 * Aqui ficam só os bionic-only que sobram. A tabela revc_stub_table[] tem
 * PRIORIDADE sobre o dlsym (é consultada antes no so_resolve via combined table).
 */

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "so_util.h"

// ===========================================================================
// FALLBACK GLES2 (Mali-450 Utgard é GLES2-only).
// A librw (gl3) testa perfis [ES3.x, ES2.0] por SDL_CreateWindow e trava
// gl3Caps.glversion no valor do 1º perfil cujo window cria. Se um pedido ES3
// cria window (Mali aceita), grava glversion>=30 -> shader "#version 310 es"
// -> NÃO compila no Mali-450 -> a engine loga erro (std::cerr) e raise(SIGILL).
// FIX: rejeitar SDL_CreateWindow(SDL_WINDOW_OPENGL) quando o major pedido >=3,
// forçando a librw a cair no perfil ES 2.0 -> glversion=20 -> "#version 100".
// ===========================================================================
#define SDL_GL_CONTEXT_MAJOR_VERSION 17
#define SDL_WINDOW_OPENGL 0x00000002u

static int g_req_gl_major = 0;

int my_SDL_GL_SetAttribute(int attr, int value) {
  if (attr == SDL_GL_CONTEXT_MAJOR_VERSION)
    g_req_gl_major = value;
  fprintf(stderr, "[GLDIAG] SDL_GL_SetAttribute(%d, %d)\n", attr, value);
  fflush(stderr);
  static int (*real)(int, int) = NULL;
  if (!real)
    real = (int (*)(int, int))dlsym(RTLD_DEFAULT, "SDL_GL_SetAttribute");
  return real ? real(attr, value) : -1;
}

// ---- instrumentação de shaders (via SDL_GL_GetProcAddress / GLAD) ----
#define GL_COMPILE_STATUS 0x8B81
static void (*real_glCompileShader)(unsigned) = NULL;
static void (*real_glGetShaderiv)(unsigned, unsigned, int *) = NULL;
static void (*real_glGetShaderInfoLog)(unsigned, int, int *, char *) = NULL;
static void (*real_glShaderSource)(unsigned, int, const char *const *,
                                   const int *) = NULL;

// substitui TODAS as ocorrências de 'find' por 'repl' (malloc novo buffer)
static char *str_replace_all(const char *src, const char *find,
                             const char *repl) {
  size_t fl = strlen(find), rl = strlen(repl), n = 0;
  for (const char *p = src; (p = strstr(p, find)); p += fl)
    n++;
  char *out = (char *)malloc(strlen(src) + n * (rl > fl ? rl - fl : 0) + 1);
  char *o = out;
  const char *p = src, *q;
  while ((q = strstr(p, find))) {
    memcpy(o, p, q - p);
    o += q - p;
    memcpy(o, repl, rl);
    o += rl;
    p = q + fl;
  }
  strcpy(o, p);
  return out;
}

static void my_glShaderSource(unsigned sh, int count, const char *const *str,
                              const int *len) {
  size_t total = 1;
  for (int i = 0; i < count; i++)
    if (str && str[i])
      total += strlen(str[i]);
  char *cat = (char *)malloc(total);
  cat[0] = 0;
  for (int i = 0; i < count; i++)
    if (str && str[i])
      strcat(cat, str[i]);

  // FIXES Mali-450 (replicam os librw-patches do port nativo que FUNCIONA):
  //  011: MAX_LIGHTS 8 -> 4   (8 luzes = 316 vec4 > 304 do Mali-400 GP)
  //  ---: highp -> mediump    (GP do Mali não suporta highp)
  //  014: im2d z-fix          (gl_Position.xyz*=w joga o Z fora do clip no
  //       Mali -> 2D do menu some; corrigir p/ só xy*=w e z=0)
  char *s1 = str_replace_all(cat, "MAX_LIGHTS 8", "MAX_LIGHTS 4");
  char *s2 = str_replace_all(s1, "highp", "mediump");
  char *s3 =
      str_replace_all(s2, "gl_Position.xyz *= gl_Position.w;",
                      "gl_Position.xy *= gl_Position.w; gl_Position.z = 0.0;");
  free(cat);
  free(s1);
  free(s2);

  const char *one = s3;
  if (real_glShaderSource)
    real_glShaderSource(sh, 1, &one, NULL);
  free(s3);
}

static void my_glCompileShader(unsigned sh) {
  if (real_glCompileShader)
    real_glCompileShader(sh);
  if (!real_glGetShaderiv)
    real_glGetShaderiv =
        (void (*)(unsigned, unsigned, int *))dlsym(RTLD_DEFAULT, "glGetShaderiv");
  if (!real_glGetShaderInfoLog)
    real_glGetShaderInfoLog = (void (*)(unsigned, int, int *, char *))dlsym(
        RTLD_DEFAULT, "glGetShaderInfoLog");
  int status = -1;
  if (real_glGetShaderiv)
    real_glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
  if (status != 1) {
    char log[1024] = {0};
    if (real_glGetShaderInfoLog)
      real_glGetShaderInfoLog(sh, sizeof(log) - 1, NULL, log);
    fprintf(stderr, "[GLDIAG] *** SHADER %u COMPILE FAIL (status=%d): %s\n", sh,
            status, log);
  } else {
    fprintf(stderr, "[GLDIAG] shader %u compilou OK\n", sh);
  }
  fflush(stderr);
}

static void (*real_glClear)(unsigned) = NULL;
static void (*real_glClearColor)(float, float, float, float) = NULL;
static unsigned (*real_glGetError)(void) = NULL;
static int g_clear_n = 0;
static void my_glClear(unsigned mask) {
  if (real_glClear)
    real_glClear(mask | 0x4000); // garante limpar cor (clears do reVC = só Z/stencil)
  g_clear_n++;
}

// contadores de draw + swap
static long g_draws = 0, g_swaps = 0;
static void (*real_glDrawArrays)(unsigned, int, int) = NULL;
static void (*real_glDrawElements)(unsigned, int, unsigned, const void *) = NULL;
static void my_glDrawArrays(unsigned m, int f, int c) {
  g_draws++;
  if (real_glDrawArrays)
    real_glDrawArrays(m, f, c);
}
static void my_glDrawElements(unsigned m, int c, unsigned t, const void *i) {
  g_draws++;
  if (real_glDrawElements)
    real_glDrawElements(m, c, t, i);
}
static int g_cur_fbo = -999;
static void (*real_glBindFramebuffer)(unsigned, unsigned) = NULL;
static void my_glBindFramebuffer(unsigned target, unsigned fb) {
  g_cur_fbo = (int)fb;
  if (real_glBindFramebuffer)
    real_glBindFramebuffer(target, fb);
}
static void (*real_glReadPixels)(int, int, int, int, unsigned, unsigned,
                                 void *) = NULL;
int my_SDL_GL_SwapWindow(void *w) {
  g_swaps++;
  if (g_swaps == 900) {
    // dump do backbuffer GL (1280x720 RGBA) in-game p/ ver o mundo 3D
    if (!real_glReadPixels)
      real_glReadPixels = (void (*)(int, int, int, int, unsigned, unsigned,
                                    void *))dlsym(RTLD_DEFAULT, "glReadPixels");
    static unsigned char buf[1280 * 720 * 4];
    if (real_glReadPixels) {
      real_glReadPixels(0, 0, 1280, 720, 0x1908, 0x1401, buf);
      FILE *f = fopen("/storage/roms/ports/revc/gl_dump.raw", "wb");
      if (f) {
        fwrite(buf, 1, sizeof(buf), f);
        fclose(f);
        fprintf(stderr, "[GLDIAG] backbuffer dumped (gl_dump.raw)\n");
      }
    }
  }
  if (g_swaps % 300 == 1) {
    // lê pixels do backbuffer GL p/ saber se o RENDER produziu cor
    if (!real_glReadPixels)
      real_glReadPixels = (void (*)(int, int, int, int, unsigned, unsigned,
                                    void *))dlsym(RTLD_DEFAULT, "glReadPixels");
    unsigned char px[4 * 5] = {0};
    int maxv = 0;
    if (real_glReadPixels) {
      int pts[5][2] = {{640, 360}, {200, 200}, {1000, 600}, {640, 100},
                       {100, 600}};
      for (int k = 0; k < 5; k++) {
        unsigned char p4[4] = {0};
        real_glReadPixels(pts[k][0], pts[k][1], 1, 1, 0x1908 /*RGBA*/,
                          0x1401 /*UBYTE*/, p4);
        for (int c = 0; c < 3; c++)
          if (p4[c] > maxv)
            maxv = p4[c];
      }
    }
    fprintf(stderr,
            "[GLDIAG] frame swaps=%ld draws=%ld fbo=%d backbuffer_maxRGB=%d\n",
            g_swaps, g_draws, g_cur_fbo, maxv);
  }
  fflush(stderr);
  static void (*real)(void *) = NULL;
  if (!real)
    real = (void (*)(void *))dlsym(RTLD_DEFAULT, "SDL_GL_SwapWindow");
  if (real)
    real(w);
  return 0;
}

// FIX 2D Mali (lição do package.mk do port nativo): o im2d desenha com Z-test
// ligado e no Mali-450 os elementos 2D do menu falham o teste -> nada renderiza.
// Forçamos GL_DEPTH_TEST sempre OFF (o menu é 100% 2D). [rever p/ mundo 3D]
#define GL_DEPTH_TEST 0x0B71
static void (*real_glEnable)(unsigned) = NULL;
static int g_en_log = 0;
static void my_glEnable(unsigned cap) {
  if (g_en_log < 30) {
    fprintf(stderr, "[GLDIAG] glEnable(0x%x)\n", cap);
    g_en_log++;
  }
  // NÃO desligar mais o GL_DEPTH_TEST globalmente: o 3D precisa de depth.
  // O 2D do menu funciona via o im2d z-fix (gl_Position.z=0 passa o LEQUAL).
  if (real_glEnable)
    real_glEnable(cap);
}
static void (*real_glColorMask)(unsigned char, unsigned char, unsigned char,
                                unsigned char) = NULL;
static int g_cm_log = 0;
static void my_glColorMask(unsigned char r, unsigned char g, unsigned char b,
                           unsigned char a) {
  if (g_cm_log < 10) {
    fprintf(stderr, "[GLDIAG] glColorMask(%d,%d,%d,%d) -> forçando 1,1,1,1\n", r,
            g, b, a);
    g_cm_log++;
  }
  if (real_glColorMask)
    real_glColorMask(1, 1, 1, 1); // força writes de cor sempre ON
}
static void (*real_glViewport)(int, int, int, int) = NULL;
static int g_vp_log = 0;
static void my_glViewport(int x, int y, int w, int h) {
  if (g_vp_log < 8) {
    fprintf(stderr, "[GLDIAG] glViewport(%d,%d,%d,%d)\n", x, y, w, h);
    g_vp_log++;
  }
  if (real_glViewport)
    real_glViewport(x, y, w, h);
}

static void (*real_glVAP)(unsigned, int, unsigned, unsigned char, int,
                          const void *) = NULL;
static int g_vap_log = 0;
static void my_glVertexAttribPointer(unsigned idx, int size, unsigned type,
                                     unsigned char norm, int stride,
                                     const void *ptr) {
  if (g_vap_log < 16) {
    fprintf(stderr,
            "[GLDIAG] glVertexAttribPointer idx=%u size=%d type=0x%x norm=%d "
            "stride=%d ptr=%p\n",
            idx, size, type, norm, stride, ptr);
    g_vap_log++;
  }
  if (real_glVAP)
    real_glVAP(idx, size, type, norm, stride, ptr);
}
static void (*real_glLinkProgram)(unsigned) = NULL;
static void (*real_glGetProgramiv)(unsigned, unsigned, int *) = NULL;
static void my_glLinkProgram(unsigned prog) {
  if (real_glLinkProgram)
    real_glLinkProgram(prog);
  if (!real_glGetProgramiv)
    real_glGetProgramiv = (void (*)(unsigned, unsigned, int *))dlsym(
        RTLD_DEFAULT, "glGetProgramiv");
  int st = -1;
  if (real_glGetProgramiv)
    real_glGetProgramiv(prog, 0x8B82 /*LINK_STATUS*/, &st);
  static int n = 0;
  if (n++ < 8)
    fprintf(stderr, "[GLDIAG] glLinkProgram %u -> link_status=%d\n", prog, st);
}

// patch 012: GLES2 não aceita internalformat "sized" (GL_RGBA8 etc.) no
// glTexImage2D — precisa ser unsized (== format). Sem isso as texturas não
// sobem (menu sem fundo/fontes).
static void (*real_glTexImage2D)(unsigned, int, int, int, int, int, unsigned,
                                 unsigned, const void *) = NULL;
static void my_glTexImage2D(unsigned tgt, int lvl, int ifmt, int w, int h,
                            int bord, unsigned fmt, unsigned type,
                            const void *px) {
  switch (ifmt) {
  case 0x8058: /*GL_RGBA8*/
  case 0x8057: /*GL_RGB5_A1*/
  case 0x8056: /*GL_RGBA4*/
    ifmt = 0x1908; /*GL_RGBA*/
    break;
  case 0x8051: /*GL_RGB8*/
  case 0x8D62: /*GL_RGB565*/
    ifmt = 0x1907; /*GL_RGB*/
    break;
  case 0x8229: /*GL_R8*/
  case 0x822B: /*GL_RG8*/
    ifmt = (int)fmt;
    break;
  }
  if (real_glTexImage2D)
    real_glTexImage2D(tgt, lvl, ifmt, w, h, bord, fmt, type, px);
}

void *my_SDL_GL_GetProcAddress(const char *name) {
  static void *(*real_gpa)(const char *) = NULL;
  if (!real_gpa)
    real_gpa = (void *(*)(const char *))dlsym(RTLD_DEFAULT,
                                              "SDL_GL_GetProcAddress");
  void *p = real_gpa ? real_gpa(name) : NULL;
  if (name && !p)
    fprintf(stderr, "[GLDIAG] GetProcAddress('%s') = NULL (ausente no Mali!)\n",
            name);
  if (name) {
    if (!strcmp(name, "glClear")) {
      real_glClear = (void (*)(unsigned))p;
      return (void *)&my_glClear;
    }
    if (!strcmp(name, "glEnable")) {
      real_glEnable = (void (*)(unsigned))p;
      return (void *)&my_glEnable;
    }
    if (!strcmp(name, "glViewport")) {
      real_glViewport = (void (*)(int, int, int, int))p;
      return (void *)&my_glViewport;
    }
    if (!strcmp(name, "glColorMask")) {
      real_glColorMask = (void (*)(unsigned char, unsigned char, unsigned char,
                                   unsigned char))p;
      return (void *)&my_glColorMask;
    }
    if (!strcmp(name, "glTexImage2D")) {
      real_glTexImage2D = (void (*)(unsigned, int, int, int, int, int, unsigned,
                                    unsigned, const void *))p;
      return (void *)&my_glTexImage2D;
    }
    if (!strcmp(name, "glVertexAttribPointer")) {
      real_glVAP = (void (*)(unsigned, int, unsigned, unsigned char, int,
                             const void *))p;
      return (void *)&my_glVertexAttribPointer;
    }
    if (!strcmp(name, "glLinkProgram")) {
      real_glLinkProgram = (void (*)(unsigned))p;
      return (void *)&my_glLinkProgram;
    }
    if (!strcmp(name, "glBindFramebuffer")) {
      real_glBindFramebuffer = (void (*)(unsigned, unsigned))p;
      return (void *)&my_glBindFramebuffer;
    }
    if (!strcmp(name, "glDrawArrays")) {
      real_glDrawArrays = (void (*)(unsigned, int, int))p;
      return (void *)&my_glDrawArrays;
    }
    if (!strcmp(name, "glDrawElements")) {
      real_glDrawElements = (void (*)(unsigned, int, unsigned, const void *))p;
      return (void *)&my_glDrawElements;
    }
    if (!strcmp(name, "glGetShaderiv"))
      real_glGetShaderiv = (void (*)(unsigned, unsigned, int *))p;
    else if (!strcmp(name, "glGetShaderInfoLog"))
      real_glGetShaderInfoLog = (void (*)(unsigned, int, int *, char *))p;
    else if (!strcmp(name, "glCompileShader")) {
      real_glCompileShader = (void (*)(unsigned))p;
      return (void *)&my_glCompileShader;
    } else if (!strcmp(name, "glShaderSource")) {
      real_glShaderSource =
          (void (*)(unsigned, int, const char *const *, const int *))p;
      return (void *)&my_glShaderSource;
    }
  }
  return p;
}

// Diagnóstico do enumerador de display modes (crash acontece logo após).
int my_SDL_GetNumVideoDisplays(void) {
  static int (*real)(void) = NULL;
  if (!real)
    real = (int (*)(void))dlsym(RTLD_DEFAULT, "SDL_GetNumVideoDisplays");
  int r = real ? real() : -1;
  fprintf(stderr, "[GLDIAG] SDL_GetNumVideoDisplays() = %d\n", r);
  fflush(stderr);
  return r;
}
extern void *text_base; // base do módulo carregado por último (libreVC)
int my_SDL_GetNumDisplayModes(int disp) {
  static int (*real)(int) = NULL;
  if (!real)
    real = (int (*)(int))dlsym(RTLD_DEFAULT, "SDL_GetNumDisplayModes");
  int r = real ? real(disp) : -1;
  fprintf(stderr, "[GLDIAG] SDL_GetNumDisplayModes(%d) = %d\n", disp, r);
  // ler o ponteiro x9 = *(libreVC+0x83d138) que o crash chama logo após
  uintptr_t b = (uintptr_t)text_base;
  uintptr_t obj = *(uintptr_t *)(b + 0x3b1708); // GOT slot -> memfuncs
  fprintf(stderr, "[GLDIAG] memfuncs@0x%lx:\n", (unsigned long)(obj - b));
  if (obj) {
    const char *nm[] = {"rwmalloc", "rwrealloc", "rwfree", "rwmustmalloc",
                        "rwmustrealloc"};
    for (int i = 0; i < 5; i++) {
      uintptr_t fp = *(uintptr_t *)(obj + i * 8);
      fprintf(stderr, "   +%d %-13s = %p (libreVC+0x%lx)\n", i * 8, nm[i],
              (void *)fp, (unsigned long)(fp - b));
    }
  }
  fflush(stderr);
  return r;
}

// força a resolução do fb0 (1280x720) — reVC pega 1920x1080 do display mode
// (SDL3-mali reporta o connector, não o fb console) e o present quebra.
#define FB_W 1280
#define FB_H 720
void *my_SDL_CreateWindow(const char *title, int x, int y, int w, int h,
                          uint32_t flags) {
  if ((flags & SDL_WINDOW_OPENGL) && g_req_gl_major >= 3) {
    return NULL; // força fallback ES2 (próximo perfil)
  }
  fprintf(stderr, "[GLDIAG] SDL_CreateWindow %dx%d->%dx%d flags=0x%x\n", w, h,
          FB_W, FB_H, flags);
  fflush(stderr);
  static void *(*real)(const char *, int, int, int, int, uint32_t) = NULL;
  if (!real)
    real = (void *(*)(const char *, int, int, int, int, uint32_t))dlsym(
        RTLD_DEFAULT, "SDL_CreateWindow");
  return real ? real(title, x, y, FB_W, FB_H, flags) : NULL;
}
int my_SDL_GetCurrentDisplayMode(int disp, void *mode) {
  static int (*real)(int, void *) = NULL;
  if (!real)
    real = (int (*)(int, void *))dlsym(RTLD_DEFAULT,
                                       "SDL_GetCurrentDisplayMode");
  int r = real ? real(disp, mode) : -1;
  if (mode) {
    *(int *)((char *)mode + 4) = FB_W;
    *(int *)((char *)mode + 8) = FB_H;
  }
  return r;
}
int my_SDL_GetDisplayMode(int disp, int idx, void *mode) {
  static int (*real)(int, int, void *) = NULL;
  if (!real)
    real = (int (*)(int, int, void *))dlsym(RTLD_DEFAULT, "SDL_GetDisplayMode");
  int r = real ? real(disp, idx, mode) : -1;
  if (mode) {
    *(int *)((char *)mode + 4) = FB_W;
    *(int *)((char *)mode + 8) = FB_H;
  }
  return r;
}
void my_SDL_GetWindowSize(void *win, int *ww, int *wh) {
  if (ww)
    *ww = FB_W;
  if (wh)
    *wh = FB_H;
}

// --- input/controle ---
// força o subsistema de joystick+gamecontroller (senão não vêm eventos do pad)
int my_SDL_InitSubSystem(unsigned flags) {
  fprintf(stderr, "[PAD] SDL_InitSubSystem(0x%x) -> +JOYSTICK+GAMECONTROLLER\n",
          flags);
  flags |= 0x200 | 0x2000; // SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER
  static int (*real)(unsigned) = NULL;
  if (!real)
    real = (int (*)(unsigned))dlsym(RTLD_DEFAULT, "SDL_InitSubSystem");
  fflush(stderr);
  return real ? real(flags) : -1;
}
// Remap de botões (layout do o autor). SDL: A=0(Cross/X) B=1(Circle) X=2(Square) Y=3(Triangle).
// reVC(carro): acelera lê SDL_B(1), freia lê SDL_Y(3), rouba lê SDL_X(2).
// Desejado: X(Cross) acelera, Circle freia, Triangle rouba.
//   -> quando reVC pede 1(acelera) devolve Cross(0); pede 3(freia) devolve Circle(1);
//      pede 2(rouba) devolve Triangle(3).
unsigned char my_SDL_GameControllerGetButton(void *gc, int btn) {
  static unsigned char (*real)(void *, int) = NULL;
  if (!real)
    real = (unsigned char (*)(void *, int))dlsym(RTLD_DEFAULT,
                                                 "SDL_GameControllerGetButton");
  if (!real)
    return 0;
  int src = btn;
  if (btn == 1)
    src = 0; // acelera <- Cross/X
  else if (btn == 3)
    src = 1; // freia <- Circle
  else if (btn == 2)
    src = 3; // rouba/entra <- Triangle
  return real(gc, src);
}
void *my_SDL_GameControllerOpen(int idx) {
  static void *(*real)(int) = NULL;
  if (!real)
    real = (void *(*)(int))dlsym(RTLD_DEFAULT, "SDL_GameControllerOpen");
  void *r = real ? real(idx) : NULL;
  fprintf(stderr, "[PAD] SDL_GameControllerOpen(%d) = %p\n", idx, r);
  fflush(stderr);
  return r;
}
int my_SDL_GameControllerAddMappingsFromRW(void *rw, int freerw) {
  static int (*real)(void *, int) = NULL;
  if (!real)
    real = (int (*)(void *, int))dlsym(RTLD_DEFAULT,
                                       "SDL_GameControllerAddMappingsFromRW");
  int r = real ? real(rw, freerw) : -1;
  fprintf(stderr, "[PAD] AddMappingsFromRW = %d mapeamentos\n", r);
  fflush(stderr);
  return r;
}
int my_SDL_PollEvent(void *e) {
  static int (*real)(void *) = NULL;
  if (!real)
    real = (int (*)(void *))dlsym(RTLD_DEFAULT, "SDL_PollEvent");
  int r = real ? real(e) : 0;
  if (r && e) {
    unsigned t = *(unsigned *)e;
    static int n = 0;
    if (t >= 0x600 && t <= 0x659 && n < 40) {
      fprintf(stderr, "[PAD] evento SDL type=0x%x\n", t);
      fflush(stderr);
      n++;
    }
  }
  return r;
}

// ---- bionic errno: __errno() devolve o ponteiro de errno (glibc: __errno_location) ----
int *__errno(void) { return &errno; }

// Bloqueia a engine de instalar handlers nos sinais fatais (mantém o nosso,
// p/ vermos a falha ORIGINAL em vez da re-raise da engine). Outros: passa.
int my_sigaction(int sig, const void *act, void *old) {
  if (sig == 4 || sig == 6 || sig == 7 || sig == 8 || sig == 11) {
    fprintf(stderr, "[DIAG] sigaction(%d) bloqueado (mantém nosso handler)\n",
            sig);
    return 0;
  }
  static int (*real)(int, const void *, void *) = NULL;
  if (!real)
    real = (int (*)(int, const void *, void *))dlsym(RTLD_DEFAULT, "sigaction");
  return real ? real(sig, act, old) : 0;
}

// ---- bionic stdio: __sF[3] = {stdin,stdout,stderr}. Layout difere do glibc;
// fornecemos um buffer com sizeof(FILE) do glibc por slot e apontamos para os
// streams reais via fdopen, pra que fprintf(&__sF[i]) caia em FILE* válido. ----
static char sf_storage[3 * 1024];

// ---- Android logging -> stderr ----
static int log_prio_min = 0; // tudo

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
  (void)prio;
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "[reVC/%s] ", tag ? tag : "?");
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
  return 0;
}

int __android_log_write(int prio, const char *tag, const char *text) {
  (void)prio;
  fprintf(stderr, "[reVC/%s] %s\n", tag ? tag : "?", text ? text : "");
  return 0;
}

int __android_log_vprint(int prio, const char *tag, const char *fmt,
                         va_list ap) {
  (void)prio;
  fprintf(stderr, "[reVC/%s] ", tag ? tag : "?");
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  return 0;
}

// ---- bionic FORTIFY _chk que o glibc NÃO tem ----
// (os que o glibc tem — __memcpy_chk, __strcpy_chk, __vsnprintf_chk, etc. —
//  resolvem por dlsym e não precisam de stub.)
size_t __strlen_chk(const char *s, size_t maxlen) {
  (void)maxlen;
  return strlen(s);
}
char *__strchr_chk(const char *s, int c, size_t n) {
  (void)n;
  return (char *)strchr(s, c);
}
char *__strrchr_chk(const char *s, int c, size_t n) {
  (void)n;
  return (char *)strrchr(s, c);
}
char *__strncpy_chk2(char *dst, const char *src, size_t n, size_t dlen,
                     size_t slen) {
  (void)dlen;
  (void)slen;
  return strncpy(dst, src, n);
}
ssize_t __read_chk(int fd, void *buf, size_t count, size_t buflen) {
  (void)buflen;
  return read(fd, buf, count);
}
ssize_t __readlink_chk(const char *path, char *buf, size_t sz, size_t buflen) {
  (void)buflen;
  return readlink(path, buf, sz);
}
// ===========================================================================
// Redirecionamento de PATH (resolve o problema do cwd/chdir do reVC + case).
// A librw (base.cpp) abre arquivos RELATIVOS (ex "./models/NSWBTTNS.TXD")
// relativos ao cwd, mas o reVC faz chdir em runtime -> path errado -> spam.
// Redirecionamos qualquer path relativo (ou /storage/emulated/0/reVC) para
// a pasta de dados real, com resolução case-insensitive componente a componente.
// ===========================================================================
#define REVC_DATA "/storage/roms/ports/revc/gamedata"

// resolve 'rel' (separado por / ) sob 'base', casando entradas por strcasecmp.
// escreve o caminho real em out. Devolve 1 se resolveu até o fim, 0 senão
// (mas out fica com o melhor esforço — útil p/ criar/abrir mesmo parcial).
static int resolve_ci(const char *base, const char *rel, char *out,
                      size_t osz) {
  snprintf(out, osz, "%s", base);
  size_t ol = strlen(out);
  char comp[256];
  const char *p = rel;
  while (*p) {
    while (*p == '/' || *p == '\\')
      p++;
    if (!*p)
      break;
    int ci = 0;
    while (*p && *p != '/' && *p != '\\' && ci < 255)
      comp[ci++] = *p++;
    comp[ci] = 0;
    // tenta match exato primeiro
    char cand[1024];
    snprintf(cand, sizeof(cand), "%s/%s", out, comp);
    if (access(cand, F_OK) == 0) {
      snprintf(out + ol, osz - ol, "/%s", comp);
      ol = strlen(out);
      continue;
    }
    // varre o diretório casando case-insensitive
    DIR *d = opendir(out);
    int found = 0;
    if (d) {
      struct dirent *e;
      while ((e = readdir(d))) {
        if (strcasecmp(e->d_name, comp) == 0) {
          snprintf(out + ol, osz - ol, "/%s", e->d_name);
          ol = strlen(out);
          found = 1;
          break;
        }
      }
      closedir(d);
    }
    if (!found) {
      // não achou: anexa como veio (deixa o open falhar/criar)
      snprintf(out + ol, osz - ol, "/%s", comp);
      ol = strlen(out);
      return 0;
    }
  }
  return 1;
}

// devolve 1 e preenche 'out' se o path deve ser redirecionado p/ os dados
static int revc_redirect(const char *path, char *out, size_t osz) {
  if (!path)
    return 0;
  // CIRÚRGICO: só os paths relativos "./models/..." da librw (base.cpp) que
  // davam o spam de 641K (NSWBTTNS etc). Tudo mais (absolutos via symlink+
  // casepath, e outros relativos) passa intacto.
  const char *rel = path;
  while (rel[0] == '.' && (rel[1] == '/' || rel[1] == '\\'))
    rel += 2;
  if (strncasecmp(rel, "models/", 7) != 0 && strncasecmp(rel, "models\\", 7) != 0)
    return 0;
  resolve_ci(REVC_DATA, rel, out, osz);
  return 1;
}

FILE *my_fopen(const char *path, const char *mode) {
  static FILE *(*real)(const char *, const char *) = NULL;
  if (!real)
    real = (FILE *(*)(const char *, const char *))dlsym(RTLD_DEFAULT, "fopen");
  char buf[1024];
  if (revc_redirect(path, buf, sizeof(buf)))
    return real(buf, mode);
  return real(path, mode);
}

int __open_2(const char *path, int flags) {
  char buf[1024];
  if (revc_redirect(path, buf, sizeof(buf)))
    return open(buf, flags);
  return open(path, flags);
}
int __openat_2(int fd, const char *path, int flags) {
  return openat(fd, path, flags);
}

// ---- bionic-only sistema ----
int __system_property_get(const char *name, char *value) {
  (void)name;
  if (value)
    value[0] = '\0';
  return 0;
}
void android_set_abort_message(const char *msg) {
  if (msg)
    fprintf(stderr, "[abort-msg] %s\n", msg);
}
void __assert2(const char *file, int line, const char *func,
               const char *expr) {
  fprintf(stderr, "assert falhou: %s:%d %s: %s\n", file ? file : "?", line,
          func ? func : "?", expr ? expr : "?");
  abort();
}

// ---- DIAGNÓSTICO: flagrar quem aborta/levanta sinal ----
void __assert_fail(const char *expr, const char *file, unsigned int line,
                   const char *func) {
  fprintf(stderr, "[DIAG] __assert_fail: %s:%u %s: %s\n", file ? file : "?",
          line, func ? func : "?", expr ? expr : "?");
  fflush(stderr);
  abort();
}
int raise(int sig) {
  fprintf(stderr, "[DIAG] raise(%d) chamado! backtrace:\n", sig);
  uintptr_t b = (uintptr_t)text_base;
  uintptr_t fp = (uintptr_t)__builtin_frame_address(0);
  for (int i = 0; i < 20 && fp; i++) {
    uintptr_t next = ((uintptr_t *)fp)[0];
    uintptr_t lr = ((uintptr_t *)fp)[1];
    if (!lr)
      break;
    if (lr >= b && lr < b + 0x3a5170)
      fprintf(stderr, "   #%d libreVC+0x%lx\n", i, (unsigned long)(lr - b));
    else
      fprintf(stderr, "   #%d %p\n", i, (void *)lr);
    if (next <= fp)
      break;
    fp = next;
  }
  fflush(stderr);
  // (experimento de ignorar SIGILL revertido: é fatal genuíno; ignorar faz
  //  a execução cair no início do ELF. Precisamos prevenir a CONDIÇÃO.)
  extern int kill(int, int);
  return kill(getpid(), sig);
}
void abort(void) {
  fprintf(stderr, "[DIAG] abort() chamado!\n");
  fflush(stderr);
  _exit(134);
}
void __cxa_pure_virtual(void) {
  fprintf(stderr, "[DIAG] __cxa_pure_virtual() chamado!\n");
  fflush(stderr);
  _exit(135);
}
// Flagra exceção C++ (se dispara, o problema é unwind/eh_frame do so-loader).
void __cxa_throw(void *thrown, void *tinfo, void *dest) {
  const char *tn = "?";
  // std::type_info: vtable em [0], nome (char*) em [8]
  if (tinfo)
    tn = *(const char *const *)((char *)tinfo + 8);
  fprintf(stderr, "[DIAG] __cxa_throw! tipo='%s' (tinfo=%p)\n", tn ? tn : "?",
          tinfo);
  fflush(stderr);
  _exit(136);
}

DynLibFunction revc_stub_table[] = {
    {"SDL_GL_SetAttribute", (uintptr_t)&my_SDL_GL_SetAttribute},
    {"SDL_CreateWindow", (uintptr_t)&my_SDL_CreateWindow},
    {"SDL_GL_GetProcAddress", (uintptr_t)&my_SDL_GL_GetProcAddress},
    {"SDL_GL_SwapWindow", (uintptr_t)&my_SDL_GL_SwapWindow},
    {"SDL_GetNumVideoDisplays", (uintptr_t)&my_SDL_GetNumVideoDisplays},
    {"SDL_GetNumDisplayModes", (uintptr_t)&my_SDL_GetNumDisplayModes},
    {"SDL_GetCurrentDisplayMode", (uintptr_t)&my_SDL_GetCurrentDisplayMode},
    {"SDL_GetDisplayMode", (uintptr_t)&my_SDL_GetDisplayMode},
    {"SDL_GetWindowSize", (uintptr_t)&my_SDL_GetWindowSize},
    {"SDL_InitSubSystem", (uintptr_t)&my_SDL_InitSubSystem},
    {"SDL_GameControllerOpen", (uintptr_t)&my_SDL_GameControllerOpen},
    {"SDL_GameControllerGetButton", (uintptr_t)&my_SDL_GameControllerGetButton},
    {"SDL_GameControllerAddMappingsFromRW",
     (uintptr_t)&my_SDL_GameControllerAddMappingsFromRW},
    {"SDL_PollEvent", (uintptr_t)&my_SDL_PollEvent},
    {"__assert_fail", (uintptr_t)&__assert_fail},
    {"raise", (uintptr_t)&raise},
    {"abort", (uintptr_t)&abort},
    {"__cxa_pure_virtual", (uintptr_t)&__cxa_pure_virtual},
    {"__cxa_throw", (uintptr_t)&__cxa_throw},
    {"__errno", (uintptr_t)&__errno},
    {"fopen", (uintptr_t)&my_fopen},
    {"sigaction", (uintptr_t)&my_sigaction},
    {"__sF", (uintptr_t)sf_storage},
    {"__android_log_print", (uintptr_t)&__android_log_print},
    {"__android_log_write", (uintptr_t)&__android_log_write},
    {"__android_log_vprint", (uintptr_t)&__android_log_vprint},
    {"__strlen_chk", (uintptr_t)&__strlen_chk},
    {"__strchr_chk", (uintptr_t)&__strchr_chk},
    {"__strrchr_chk", (uintptr_t)&__strrchr_chk},
    {"__strncpy_chk2", (uintptr_t)&__strncpy_chk2},
    {"__read_chk", (uintptr_t)&__read_chk},
    {"__readlink_chk", (uintptr_t)&__readlink_chk},
    {"__open_2", (uintptr_t)&__open_2},
    {"__openat_2", (uintptr_t)&__openat_2},
    {"__system_property_get", (uintptr_t)&__system_property_get},
    {"android_set_abort_message", (uintptr_t)&android_set_abort_message},
    {"__assert2", (uintptr_t)&__assert2},
};
const int revc_stub_count = sizeof(revc_stub_table) / sizeof(revc_stub_table[0]);
