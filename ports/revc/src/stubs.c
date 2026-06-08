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
  // im2d (2D) z-fix: `gl_Position.xyz *= w` estoura o Z no Mali -> 2D some (preto).
  // Em vez de z=0 (que achata TODO 2D e quebra a máscara de depth do radar ->
  // mapa quadrado vaza), escalamos o z por um fator pequeno: fica no range (menu
  // aparece) E preserva a ordem máscara<mapa do DrawRadarMask (radar recorta).
  char *s3 =
      str_replace_all(s2, "gl_Position.xyz *= gl_Position.w;",
                      "gl_Position.xy *= gl_Position.w; gl_Position.z *= 0.01;");
  // im2d (2D) NÃO deve ter fog: as nuvens do céu são sprites 2D e o Clouds.cpp
  // não desliga o fog -> ficavam pretas (cor do fog). u_xform é exclusivo do
  // im2d, então só o 2D perde o fog (o fog do mundo 3D fica intacto).
  if (strstr(s3, "u_xform")) {
    char *sf = str_replace_all(s3, "v_fog = DoFog(gl_Position.w);", "v_fog = 1.0;");
    free(s3);
    s3 = sf;
  }
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
    fprintf(stderr, "[reVC] SHADER %u COMPILE FAIL (status=%d): %s\n", sh,
            status, log);
    fflush(stderr);
  }
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
int my_SDL_GL_SwapWindow(void *w) {
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
static void my_glEnable(unsigned cap) {
  if (real_glEnable)
    real_glEnable(cap);
}
static void (*real_glColorMask)(unsigned char, unsigned char, unsigned char,
                                unsigned char) = NULL;
static void my_glColorMask(unsigned char r, unsigned char g, unsigned char b,
                           unsigned char a) {
  (void)r; (void)g; (void)b; (void)a;
  if (real_glColorMask)
    real_glColorMask(1, 1, 1, 1); // força writes de cor sempre ON
}
static void (*real_glViewport)(int, int, int, int) = NULL;
static void my_glViewport(int x, int y, int w, int h) {
  if (real_glViewport)
    real_glViewport(x, y, w, h);
}

static void (*real_glVAP)(unsigned, int, unsigned, unsigned char, int,
                          const void *) = NULL;
// FIX peds pretos/bugados: librw usa GL_TEXTURE_MAX_LEVEL (GLES3/desktop, NÃO
// existe em GLES2) p/ limitar a cadeia de mipmaps -> no GLES2 é ignorado ->
// textura com mipmap incompleto = INCOMPLETA -> renderiza PRETO (NPCs).
// (Tentamos glGenerateMipmap mas não completa no Mali -> NPCs voltavam pretos.)
// Fix que funciona: forçar o min filter de *_MIPMAP_* p/ GL_LINEAR (sem mipmap)
// -> completude não exigida -> renderiza. Efeito colateral: shimmer no reflexo
// do sol na água (aceitável).
static void (*real_glTexParameteri)(unsigned, unsigned, int) = NULL;
static void my_glTexParameteri(unsigned target, unsigned pname, int param) {
  if (pname == 0x2801) { // GL_TEXTURE_MIN_FILTER
    if (param == 0x2700 || param == 0x2701 || param == 0x2702 ||
        param == 0x2703)        // *_MIPMAP_*
      param = 0x2601;           // GL_LINEAR
  }
  if (pname == 0x813D)          // GL_TEXTURE_MAX_LEVEL (inválido em GLES2)
    return;                     // ignora (evita GL_INVALID_ENUM)
  if (real_glTexParameteri)
    real_glTexParameteri(target, pname, param);
}

static void my_glVertexAttribPointer(unsigned idx, int size, unsigned type,
                                     unsigned char norm, int stride,
                                     const void *ptr) {
  if (real_glVAP)
    real_glVAP(idx, size, type, norm, stride, ptr);
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
    if (!strcmp(name, "glTexParameteri")) {
      real_glTexParameteri = (void (*)(unsigned, unsigned, int))p;
      return (void *)&my_glTexParameteri;
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

int my_SDL_GetNumVideoDisplays(void) {
  static int (*real)(void) = NULL;
  if (!real)
    real = (int (*)(void))dlsym(RTLD_DEFAULT, "SDL_GetNumVideoDisplays");
  return real ? real() : -1;
}
int my_SDL_GetNumDisplayModes(int disp) {
  static int (*real)(int) = NULL;
  if (!real)
    real = (int (*)(int))dlsym(RTLD_DEFAULT, "SDL_GetNumDisplayModes");
  return real ? real(disp) : -1;
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
  static void *(*real)(const char *, int, int, int, int, uint32_t) = NULL;
  if (!real)
    real = (void *(*)(const char *, int, int, int, int, uint32_t))dlsym(
        RTLD_DEFAULT, "SDL_CreateWindow");
  return real ? real(title, x, y, FB_W, FB_H, flags) : NULL;
}
// SDL_DisplayMode = { Uint32 format(0); int w(4); int h(8); int refresh(12);
// void* drvdata(16) }. Forçamos TODOS os campos relevantes pra valores fixos
// (1280x720, RGBA8888=32bpp, 60Hz) — assim a librw (addVideoMode dedup por
// w/h/format) deduplica os 18 modos do device num ÚNICO modo exclusivo →
// lista de vídeo determinística (2 entradas) → menu Gráficos não estoura índice.
static void force_mode(void *mode) {
  if (!mode)
    return;
  *(uint32_t *)((char *)mode + 0) = 0x16462004u; // SDL_PIXELFORMAT_RGBA8888 (32bpp)
  *(int *)((char *)mode + 4) = FB_W;
  *(int *)((char *)mode + 8) = FB_H;
  *(int *)((char *)mode + 12) = 60;
}
int my_SDL_GetCurrentDisplayMode(int disp, void *mode) {
  static int (*real)(int, void *) = NULL;
  if (!real)
    real = (int (*)(int, void *))dlsym(RTLD_DEFAULT,
                                       "SDL_GetCurrentDisplayMode");
  int r = real ? real(disp, mode) : -1;
  force_mode(mode);
  return r;
}
int my_SDL_GetDisplayMode(int disp, int idx, void *mode) {
  static int (*real)(int, int, void *) = NULL;
  if (!real)
    real = (int (*)(int, int, void *))dlsym(RTLD_DEFAULT, "SDL_GetDisplayMode");
  int r = real ? real(disp, idx, mode) : -1;
  force_mode(mode);
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
  flags |= 0x200 | 0x2000; // SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER
  static int (*real)(unsigned) = NULL;
  if (!real)
    real = (int (*)(unsigned))dlsym(RTLD_DEFAULT, "SDL_InitSubSystem");
  return real ? real(flags) : -1;
}
// remap: índice = enum SDL que o reVC pede; valor = botão físico SDL devolvido.
// Derivado da fonte (reVC Xbox-branch: A->bater,B->entrar,X->acelera,Y->freio)
// + gamecontrollerdb do Feir (Cross->A,Square->B,Circle->X,Triangle->Y).
// Desejado: Cross=acelera(X), Circle=freio(Y), Square=bater(A), Triangle=entrar(B).
//   reVC pede A(0,bater) -> devolve fisico Square(=B=1)
//   reVC pede B(1,entrar)-> devolve fisico Triangle(=Y=3)
//   reVC pede X(2,acelera)-> devolve fisico Cross(=A=0)
//   reVC pede Y(3,freio) -> devolve fisico Circle(=X=2)
int g_btn_remap[16] = {1, 3, 0, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
unsigned char my_SDL_GameControllerGetButton(void *gc, int btn) {
  static unsigned char (*real)(void *, int) = NULL;
  if (!real)
    real = (unsigned char (*)(void *, int))dlsym(RTLD_DEFAULT,
                                                 "SDL_GameControllerGetButton");
  if (!real)
    return 0;
  int src = (btn >= 0 && btn < 16) ? g_btn_remap[btn] : btn;
  return real(gc, src);
}
void *my_SDL_GameControllerOpen(int idx) {
  static void *(*real)(int) = NULL;
  if (!real)
    real = (void *(*)(int))dlsym(RTLD_DEFAULT, "SDL_GameControllerOpen");
  return real ? real(idx) : NULL;
}
int my_SDL_GameControllerAddMappingsFromRW(void *rw, int freerw) {
  static int (*real)(void *, int) = NULL;
  if (!real)
    real = (int (*)(void *, int))dlsym(RTLD_DEFAULT,
                                       "SDL_GameControllerAddMappingsFromRW");
  return real ? real(rw, freerw) : -1;
}
int my_SDL_PollEvent(void *e) {
  static int (*real)(void *) = NULL;
  if (!real)
    real = (int (*)(void *))dlsym(RTLD_DEFAULT, "SDL_PollEvent");
  return real ? real(e) : 0;
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
  // Redireciona paths RELATIVOS das pastas de dados do jogo (librw/cutscene
  // usam ./models/, ./audio/, etc relativos ao cwd -> quebram com o chdir do
  // reVC). Os absolutos (/storage/emulated/0/reVC) já vão via symlink+casepath.
  const char *rel = path;
  while (rel[0] == '.' && (rel[1] == '/' || rel[1] == '\\'))
    rel += 2;
  static const char *dirs[] = {"models", "audio", "anim",   "data",
                               "text",   "txd",   "movies", "mp3",
                               "neo",    "skins", "user",   NULL};
  int ok = 0;
  for (int i = 0; dirs[i]; i++) {
    size_t dl = strlen(dirs[i]);
    if (strncasecmp(rel, dirs[i], dl) == 0 &&
        (rel[dl] == '/' || rel[dl] == '\\')) {
      ok = 1;
      break;
    }
  }
  if (!ok)
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
