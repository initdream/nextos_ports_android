/* imports.c (DEVICE) -- shims bionic/NDK do libGame.so como tabela DynLibFunction.
 * Igual aos shims validados no PC; aqui expostos como bully_stub_table[] p/ o
 * so_resolve do so_util AArch64 (fallback dlsym pega libc/GLES/EGL/openal/mpg123
 * do device). Ponte pthread bionic->glibc vem do pthread_bridge.c. */
#define _GNU_SOURCE
#include <ctype.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "so_util.h"
#include "jni_shim.h"
#include "zip_fs.h"

/* ---- bionic libc bridges ---- */
static int *bionic___errno(void) { extern int *__errno_location(void); return __errno_location(); }
static size_t b_strlen_chk(const char *s, size_t n) { (void)n; return strlen(s); }
static char *b_strrchr_chk(const char *s, int c, size_t n) { (void)n; return strrchr(s, c); }
static char *b_strchr_chk(const char *s, int c, size_t n) { (void)n; return strchr(s, c); }
static char *b_strncpy_chk2(char *d, const char *s, size_t n, size_t dn, size_t sn) { (void)dn; (void)sn; return strncpy(d, s, n); }
static void b_assert2(const char *f, int l, const char *fn, const char *e) {
  fprintf(stderr, "assert: %s:%d %s: %s\n", f, l, fn, e); abort();
}
static int b_android_log(int prio, const char *tag, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap);
  return 0;
}

/* bionic __sF[3] = stdin/out/err. Wrappers traduzem p/ stream real. */
static char bionic_sF[3][512];
static FILE *map_sF(void *fp) {
  if (fp == (void *)&bionic_sF[0]) return stdin;
  if (fp == (void *)&bionic_sF[1]) return stdout;
  if (fp == (void *)&bionic_sF[2]) return stderr;
  return (FILE *)fp;
}
static int w_fprintf(void *fp, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); int r = vfprintf(map_sF(fp), fmt, ap); va_end(ap); return r;
}
static int w_vfprintf(void *fp, const char *fmt, va_list ap) { return vfprintf(map_sF(fp), fmt, ap); }
static size_t w_fwrite(const void *p, size_t s, size_t n, void *fp) { return fwrite(p, s, n, map_sF(fp)); }
static int w_fputs(const char *str, void *fp) { return fputs(str, map_sF(fp)); }
static int w_fputc(int c, void *fp) { return fputc(c, map_sF(fp)); }
static int w_fflush(void *fp) { return fflush(fp ? map_sF(fp) : NULL); }

/* _ctype_ legado (BSD): tabela 1+256, indexada de -1. */
static unsigned char ctype_tab[1 + 256];
#define _CT_U 0x01
#define _CT_L 0x02
#define _CT_N 0x04
#define _CT_S 0x08
#define _CT_P 0x10
#define _CT_C 0x20
#define _CT_X 0x40
#define _CT_B 0x80
static void ctype_init(void) {
  for (int c = 0; c < 256; c++) {
    unsigned char f = 0;
    if (isupper(c)) f |= _CT_U; if (islower(c)) f |= _CT_L;
    if (isdigit(c)) f |= _CT_N; if (isspace(c)) f |= _CT_S;
    if (ispunct(c)) f |= _CT_P; if (iscntrl(c)) f |= _CT_C;
    if (isxdigit(c)) f |= _CT_X; if (c == ' ') f |= _CT_B;
    ctype_tab[1 + c] = f;
  }
}

/* ---- NDK ANativeWindow (a janela é do egl_shim Mali fbdev) ---- */
static void *aw_fromSurface(void *env, void *surface) { (void)env; (void)surface; return (void *)0xAA11; }
static int aw_setBuffersGeometry(void *w, int x, int y, int f) { (void)w;(void)x;(void)y;(void)f; return 0; }
extern int bully_screen_w(void); extern int bully_screen_h(void);
static int aw_getWidth(void *w) { (void)w; return bully_screen_w(); }
static int aw_getHeight(void *w) { (void)w; return bully_screen_h(); }
static void aw_release(void *w) { (void)w; }

/* ---- NDK AAssetManager / AAsset (lê dos arquivos reais) ---- */
#ifndef ASSET_DIR
#define ASSET_DIR "assets"
#endif
typedef struct { FILE *fp; long len; } AAsset;
static void *am_fromJava(void *env, void *obj) { (void)env; (void)obj; return (void *)0xA55E7; }
static void *aa_open(void *mgr, const char *path, int mode) {
  (void)mgr; (void)mode;
  char full[1024]; snprintf(full, sizeof(full), "%s/%s", ASSET_DIR, path);
  FILE *fp = fopen(full, "rb");
  if (!fp) { fprintf(stderr, "[asset] FALTA %s\n", full); return NULL; }
  AAsset *a = calloc(1, sizeof(AAsset)); a->fp = fp;
  fseek(fp, 0, SEEK_END); a->len = ftell(fp); fseek(fp, 0, SEEK_SET);
  return a;
}
static int aa_read(void *h, void *buf, size_t n) { AAsset *a = h; return a ? fread(buf, 1, n, a->fp) : -1; }
static long aa_seek64(void *h, long off, int wh) { AAsset *a = h; if (!a) return -1; fseek(a->fp, off, wh); return ftell(a->fp); }
static long aa_getLength64(void *h) { AAsset *a = h; return a ? a->len : 0; }
static long aa_getRemainingLength64(void *h) { AAsset *a = h; return a ? a->len - ftell(a->fp) : 0; }
static void aa_close(void *h) { AAsset *a = h; if (a) { fclose(a->fp); free(a); } }

/* ---- fopen: disco; se falhar (leitura), serve de DENTRO dos data_*.zip ---- */
static FILE *w_fopen(const char *path, const char *mode) {
  static FILE *(*real)(const char *, const char *) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "fopen");
  FILE *f = real ? real(path, mode) : NULL;
  /* device: os dados ficam em assets/ (vfat sem symlink); o jogo fopena
   * "data_N.zip" no cwd -> redireciona p/ "assets/data_N.zip". */
  if (!f && real && path && mode && mode[0] == 'r' && strncmp(path, "assets/", 7) != 0) {
    char alt[1024]; snprintf(alt, sizeof(alt), "assets/%s", path);
    f = real(alt, mode);
  }
  return f;
}

/* ---- glGetString nunca-NULL ---- */
static const unsigned char *w_glGetString(unsigned name) {
  static const unsigned char *(*real)(unsigned) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "glGetString");
  const unsigned char *r = real ? real(name) : NULL;
  return r ? r : (const unsigned char *)"";
}

/* ---- bionic-only (não existem na glibc; libc++/libGame usam) ---- */
static void b_set_abort_message(const char *m) { fprintf(stderr, "[abort_msg] %s\n", m ? m : "?"); }
static int b_system_property_get(const char *name, char *value) { (void)name; if (value) value[0] = 0; return 0; }

/* ---- C++ thread-local init helpers (_ZTH*): no-op ---- */
static void tl_noop(void) {}

/* ================= GLES2 fixes Mali-450 Utgard (receitas do reVC) ============
 * O game importa glShaderSource/glTexImage2D/glTexParameteri direto -> a tabela
 * resolve p/ estes wrappers, que corrigem p/ o Utgard e chamam o real (Mali). */
static char *str_replace_all(const char *src, const char *find, const char *repl) {
  size_t fl = strlen(find), rl = strlen(repl), n = 0;
  for (const char *p = src; (p = strstr(p, find)); p += fl) n++;
  char *out = malloc(strlen(src) + n * (rl > fl ? rl - fl : 0) + 1);
  char *o = out; const char *p = src, *q;
  while ((q = strstr(p, find))) { memcpy(o, p, q - p); o += q - p; memcpy(o, repl, rl); o += rl; p = q + fl; }
  strcpy(o, p);
  return out;
}
static void (*real_glShaderSource)(unsigned, int, const char *const *, const int *) = NULL;
static void my_glShaderSource(unsigned sh, int count, const char *const *str, const int *len) {
  (void)len;
  if (!real_glShaderSource) real_glShaderSource = dlsym(RTLD_DEFAULT, "glShaderSource");
  size_t total = 1;
  for (int i = 0; i < count; i++) if (str && str[i]) total += strlen(str[i]);
  char *cat = malloc(total); cat[0] = 0;
  for (int i = 0; i < count; i++) if (str && str[i]) strcat(cat, str[i]);
  /* Utgard (Mali-400/450 GP) não suporta highp -> mediump (cores lavadas) */
  /* Mali-450: a GP (vertex) É FP32 e SUPORTA highp; só a PP (fragment) não tem.
   * Forçar mediump no VERTEX quebra a precisão do skinning -> braços/corpo do
   * Jimmy (muita deformação) colapsam/NaN -> invisíveis. Então só troca
   * highp->mediump nos shaders de FRAGMENTO (mantém highp no vertex). */
  /* só fragment: highp->mediump (Utgard PP não tem highp). Vertex mantém highp
   * (skinning). NÃO mexer no threshold do alpha-test (0.04 dava contorno preto
   * na folhagem) — fica o 0.7 original. */
  int is_vertex = strstr(cat, "gl_Position") != NULL;
  char *s1 = is_vertex ? strdup(cat) : str_replace_all(cat, "highp", "mediump");
  free(cat);
  const char *one = s1;
  if (real_glShaderSource) real_glShaderSource(sh, 1, &one, NULL);
  free(s1);
}
static void (*real_glTexParameteri)(unsigned, unsigned, int) = NULL;
static void my_glTexParameteri(unsigned target, unsigned pname, int param) {
  if (!real_glTexParameteri) real_glTexParameteri = dlsym(RTLD_DEFAULT, "glTexParameteri");
  if (pname == 0x813D) return;                                  /* GL_TEXTURE_MAX_LEVEL: inexistente em GLES2 */
  if ((pname == 0x2801 || pname == 0x2800) && param >= 0x2700 && param <= 0x2703)
    param = 0x2601;                                             /* *_MIPMAP_* -> GL_LINEAR (sem mipmap completo = preto) */
  if (real_glTexParameteri) real_glTexParameteri(target, pname, param);
}
/* log de erros de compile/link de shader (achar o shader do Jimmy que falha) */
static void (*real_glCompileShader)(unsigned) = NULL;
static void my_glCompileShader(unsigned sh) {
  if (!real_glCompileShader) real_glCompileShader = dlsym(RTLD_DEFAULT, "glCompileShader");
  if (real_glCompileShader) real_glCompileShader(sh);
  void (*giv)(unsigned, unsigned, int *) = dlsym(RTLD_DEFAULT, "glGetShaderiv");
  void (*gil)(unsigned, int, int *, char *) = dlsym(RTLD_DEFAULT, "glGetShaderInfoLog");
  int ok = 1; if (giv) giv(sh, 0x8B81, &ok); /* GL_COMPILE_STATUS */
  if (!ok) { char log[1500] = {0}; if (gil) gil(sh, 1500, NULL, log); fprintf(stderr, "[shader] COMPILE FAIL sh=%u: %s\n", sh, log); }
}
static void (*real_glLinkProgram)(unsigned) = NULL;
static void my_glLinkProgram(unsigned p) {
  if (!real_glLinkProgram) real_glLinkProgram = dlsym(RTLD_DEFAULT, "glLinkProgram");
  if (real_glLinkProgram) real_glLinkProgram(p);
  void (*giv)(unsigned, unsigned, int *) = dlsym(RTLD_DEFAULT, "glGetProgramiv");
  void (*gil)(unsigned, int, int *, char *) = dlsym(RTLD_DEFAULT, "glGetProgramInfoLog");
  int ok = 1; if (giv) giv(p, 0x8B82, &ok); /* GL_LINK_STATUS */
  if (!ok) { char log[1500] = {0}; if (gil) gil(p, 1500, NULL, log); fprintf(stderr, "[shader] LINK FAIL p=%u: %s\n", p, log); }
}
/* texturas comprimidas: Mali-450 só faz ETC1 (0x8D64). Loga formatos p/ achar
 * se a camisa/skin do Jimmy usa um formato que o Mali rejeita -> transparente. */
static void (*real_glCompressedTexImage2D)(unsigned,int,unsigned,int,int,int,int,const void*) = NULL;
static void my_glCompressedTexImage2D(unsigned t,int l,unsigned ifmt,int w,int h,int b,int sz,const void*d) {
  if (!real_glCompressedTexImage2D) real_glCompressedTexImage2D = dlsym(RTLD_DEFAULT, "glCompressedTexImage2D");
  static int n = 0;
  if (n < 40) { fprintf(stderr, "[tex] compressed fmt=0x%x %dx%d sz=%d\n", ifmt, w, h, sz); n++; }
  if (real_glCompressedTexImage2D) real_glCompressedTexImage2D(t,l,ifmt,w,h,b,sz,d);
}
/* TESTE: ignora glEnable(GL_BLEND) -> se a camisa do Jimmy aparecer OPACA,
 * confirma que ela some por alpha-blend (alpha~0). (Quebra transparências
 * legítimas; é só p/ diagnóstico.) */
static void (*real_glEnable)(unsigned) = NULL;
static void my_glEnable(unsigned cap) {
  if (!real_glEnable) real_glEnable = dlsym(RTLD_DEFAULT, "glEnable");
  if (real_glEnable) real_glEnable(cap); /* (skip de GL_BLEND revertido: não era blend) */
}
static void (*real_glClear)(unsigned) = NULL;
static int g_cleardbg = 0;
static void my_glClear(unsigned mask) {
  if (!real_glClear) real_glClear = dlsym(RTLD_DEFAULT, "glClear");
  if (g_cleardbg < 8) { fprintf(stderr, "[gl] glClear mask=0x%x -> 0x%x\n", mask, mask | 0x4000); g_cleardbg++; }
  if (real_glClear) real_glClear(mask | 0x4000); /* força limpar COR (GL_COLOR_BUFFER_BIT) */
}
/* glTexStorage2D (GLES3) — a camisa do Jimmy pode vir por aqui (não pelo
 * glTexImage2D). Loga formato/níveis. */
static void (*real_glTexStorage2D)(unsigned, int, unsigned, int, int) = NULL;
static void my_glTexStorage2D(unsigned t, int levels, unsigned ifmt, int w, int h) {
  if (!real_glTexStorage2D) real_glTexStorage2D = dlsym(RTLD_DEFAULT, "glTexStorage2D");
  static int n = 0;
  if (n < 30) { fprintf(stderr, "[tex] STORAGE levels=%d ifmt=0x%x %dx%d\n", levels, ifmt, w, h); n++; }
  if (real_glTexStorage2D) real_glTexStorage2D(t, levels, ifmt, w, h);
}
static void (*real_glTexSubImage2D)(unsigned,int,int,int,int,int,unsigned,unsigned,const void*) = NULL;
static void my_glTexSubImage2D(unsigned t,int l,int xo,int yo,int w,int h,unsigned fmt,unsigned type,const void*px) {
  if (!real_glTexSubImage2D) real_glTexSubImage2D = dlsym(RTLD_DEFAULT, "glTexSubImage2D");
  static int n = 0;
  if (n < 30 && l == 0) { fprintf(stderr, "[tex] SUB fmt=0x%x type=0x%x %dx%d\n", fmt, type, w, h); n++; }
  if (real_glTexSubImage2D) real_glTexSubImage2D(t,l,xo,yo,w,h,fmt,type,px);
}
/* status do FBO (render-to-texture da roupa) — se INCOMPLETO no Mali, a textura
 * do corpo fica vazia -> camisa preta+discard. */
static void (*real_glBindFramebuffer)(unsigned, unsigned) = NULL;
unsigned long g_fbo_binds = 0; /* contador p/ medir tempestade de RTT (escola) */
static void my_glBindFramebuffer(unsigned t, unsigned fb) {
  if (!real_glBindFramebuffer) real_glBindFramebuffer = dlsym(RTLD_DEFAULT, "glBindFramebuffer");
  if (fb != 0) {
    g_fbo_binds++;
    if (g_fbo_binds % 300 == 0)
      fprintf(stderr, "[fbo] RTT binds total=%lu\n", g_fbo_binds);
  }
  if (real_glBindFramebuffer) real_glBindFramebuffer(t, fb);
}
static void (*real_glFramebufferTexture2D)(unsigned,unsigned,unsigned,unsigned,int) = NULL;
static void my_glFramebufferTexture2D(unsigned t,unsigned att,unsigned tt,unsigned tex,int lvl) {
  if (!real_glFramebufferTexture2D) real_glFramebufferTexture2D = dlsym(RTLD_DEFAULT, "glFramebufferTexture2D");
  if (real_glFramebufferTexture2D) real_glFramebufferTexture2D(t,att,tt,tex,lvl);
  static int n = 0;
  if (n < 14) {
    unsigned (*chk)(unsigned) = dlsym(RTLD_DEFAULT, "glCheckFramebufferStatus");
    unsigned s = chk ? chk(0x8D40) : 0;
    fprintf(stderr, "[fbo] ATTACH att=0x%x tex=%u lvl=%d -> status=0x%x %s\n", att, tex, lvl, s, s == 0x8CD5 ? "OK" : "INCOMPLETO");
    n++;
  }
}
static void (*real_glReadPixels)(int,int,int,int,unsigned,unsigned,void*) = NULL;
static void my_glReadPixels(int x,int y,int w,int h,unsigned fmt,unsigned type,void*px) {
  if (!real_glReadPixels) real_glReadPixels = dlsym(RTLD_DEFAULT, "glReadPixels");
  static int n = 0;
  if (n < 12) { fprintf(stderr, "[fbo] READPIXELS %dx%d fmt=0x%x type=0x%x\n", w, h, fmt, type); n++; }
  if (real_glReadPixels) real_glReadPixels(x,y,w,h,fmt,type,px);
}
static unsigned (*real_glCheckFramebufferStatus)(unsigned) = NULL;
static unsigned my_glCheckFramebufferStatus(unsigned t) {
  if (!real_glCheckFramebufferStatus) real_glCheckFramebufferStatus = dlsym(RTLD_DEFAULT, "glCheckFramebufferStatus");
  unsigned s = real_glCheckFramebufferStatus ? real_glCheckFramebufferStatus(t) : 0;
  static int n = 0;
  if (n < 20) { fprintf(stderr, "[fbo] CheckStatus=0x%x %s\n", s, s == 0x8CD5 ? "COMPLETE" : "INCOMPLETO!"); n++; }
  return s;
}
static void (*real_glClearColor)(float, float, float, float) = NULL;
static int g_ccdbg = 0;
static void my_glClearColor(float r, float g, float b, float a) {
  if (!real_glClearColor) real_glClearColor = dlsym(RTLD_DEFAULT, "glClearColor");
  if (g_ccdbg < 8) { fprintf(stderr, "[gl] glClearColor %.2f %.2f %.2f %.2f\n", r, g, b, a); g_ccdbg++; }
  if (real_glClearColor) real_glClearColor(r, g, b, a);
}
static int bpp_of(unsigned fmt, unsigned type) {
  if (type == 0x1401) return fmt == 0x1908 ? 4 : fmt == 0x1907 ? 3 : fmt == 0x190A ? 2 : 1;
  if (type == 0x8033 || type == 0x8034 || type == 0x8363) return 2; /* 4444/5551/565 */
  return 0; /* desconhecido -> não reduz */
}
static int g_tex_half = -1;
static void (*real_glTexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *) = NULL;
static void my_glTexImage2D(unsigned tgt, int lvl, int ifmt, int w, int h, int bord, unsigned fmt, unsigned type, const void *px) {
  if (!real_glTexImage2D) real_glTexImage2D = dlsym(RTLD_DEFAULT, "glTexImage2D");
  if (g_tex_half < 0) g_tex_half = getenv("BULLY_TEX_HALF") ? 1 : 0;
  if (ifmt == 0x8058) ifmt = 0x1908;       /* GL_RGBA8 -> GL_RGBA (GLES2 não aceita sized) */
  else if (ifmt == 0x8051) ifmt = 0x1907;  /* GL_RGB8 -> GL_RGB */
  /* pula mipmaps: como forço MIN_FILTER=LINEAR, os níveis >0 nunca são usados ->
   * só desperdiçam memória de textura da GPU (o Mali Utgard trava ao estourar). */
  if (g_tex_half && lvl > 0) return;
  /* LUMINANCE vazia (px=NULL) = alvo de render-to-texture da roupa; Mali não
   * renderiza p/ LUMINANCE -> aloca RGBA (renderável). Sem reduzir (é o alvo). */
  if ((fmt == 0x1909 || fmt == 0x190A) && type == 0x1401 && !px && w > 0 && h > 0) {
    if (real_glTexImage2D) real_glTexImage2D(tgt, lvl, 0x1908, w, h, bord, 0x1908, 0x1401, NULL);
    return;
  }
  /* monta os dados finais; converte LUMINANCE->RGBA (Mali lê L como (L,L,L,L)) */
  const unsigned char *data = px;
  unsigned ufmt = fmt, utype = type;
  unsigned char *conv = NULL;
  if ((fmt == 0x1909 || fmt == 0x190A) && type == 0x1401 && px && w > 0 && h > 0) {
    int la = (fmt == 0x190A);
    const unsigned char *src = px;
    conv = malloc((size_t)w * h * 4);
    if (conv) {
      for (int i = 0; i < w * h; i++) {
        unsigned char L = src[la ? i * 2 : i];
        conv[i*4] = L; conv[i*4+1] = L; conv[i*4+2] = L; conv[i*4+3] = la ? src[i*2+1] : 255;
      }
      data = conv; ufmt = 0x1908; utype = 0x1401; ifmt = 0x1908;
    }
  }
  /* reduz pela metade as texturas grandes (>=512): 512->256 = 1/4 da memória.
   * UV é normalizado (0..1) -> reduzir não quebra coordenadas. */
  int bpp = bpp_of(ufmt, utype);
  if (g_tex_half && data && bpp > 0 && (w >= 512 || h >= 512)) {
    int nw = w / 2, nh = h / 2;
    unsigned char *sm = (nw > 0 && nh > 0) ? malloc((size_t)nw * nh * bpp) : NULL;
    if (sm) {
      for (int y = 0; y < nh; y++)
        for (int x = 0; x < nw; x++)
          memcpy(sm + ((size_t)y * nw + x) * bpp, data + ((size_t)(y*2) * w + x*2) * bpp, bpp);
      if (real_glTexImage2D) real_glTexImage2D(tgt, lvl, ifmt, nw, nh, bord, ufmt, utype, sm);
      free(sm); free(conv);
      return;
    }
  }
  if (real_glTexImage2D) real_glTexImage2D(tgt, lvl, ifmt, w, h, bord, ufmt, utype, data);
  free(conv);
}

void bully_imports_init(void) { ctype_init(); }

/* tabela de overrides (resolvida ANTES do fallback dlsym do so_resolve) */
DynLibFunction bully_stub_table[] = {
  {"__errno", (uintptr_t)bionic___errno}, {"__assert2", (uintptr_t)b_assert2},
  {"__strlen_chk", (uintptr_t)b_strlen_chk}, {"__strrchr_chk", (uintptr_t)b_strrchr_chk},
  {"__strchr_chk", (uintptr_t)b_strchr_chk}, {"__strncpy_chk2", (uintptr_t)b_strncpy_chk2},
  {"__android_log_print", (uintptr_t)b_android_log},
  {"android_set_abort_message", (uintptr_t)b_set_abort_message},
  {"__system_property_get", (uintptr_t)b_system_property_get},
  {"__sF", (uintptr_t)bionic_sF},
  {"fprintf", (uintptr_t)w_fprintf}, {"vfprintf", (uintptr_t)w_vfprintf}, {"fwrite", (uintptr_t)w_fwrite},
  {"fputs", (uintptr_t)w_fputs}, {"fputc", (uintptr_t)w_fputc}, {"fflush", (uintptr_t)w_fflush},
  {"_ctype_", (uintptr_t)(ctype_tab + 1)},
  {"ANativeWindow_fromSurface", (uintptr_t)aw_fromSurface},
  {"ANativeWindow_setBuffersGeometry", (uintptr_t)aw_setBuffersGeometry},
  {"ANativeWindow_getWidth", (uintptr_t)aw_getWidth}, {"ANativeWindow_getHeight", (uintptr_t)aw_getHeight},
  {"ANativeWindow_release", (uintptr_t)aw_release},
  {"AAssetManager_fromJava", (uintptr_t)am_fromJava}, {"AAssetManager_open", (uintptr_t)aa_open},
  {"AAsset_read", (uintptr_t)aa_read}, {"AAsset_seek64", (uintptr_t)aa_seek64},
  {"AAsset_getLength64", (uintptr_t)aa_getLength64}, {"AAsset_getRemainingLength64", (uintptr_t)aa_getRemainingLength64},
  {"AAsset_close", (uintptr_t)aa_close},
  {"glGetString", (uintptr_t)w_glGetString},
  {"glShaderSource", (uintptr_t)my_glShaderSource},
  {"glTexParameteri", (uintptr_t)my_glTexParameteri},
  {"glTexImage2D", (uintptr_t)my_glTexImage2D},
  {"glClear", (uintptr_t)my_glClear},
  {"glClearColor", (uintptr_t)my_glClearColor},
  {"glCompileShader", (uintptr_t)my_glCompileShader},
  {"glLinkProgram", (uintptr_t)my_glLinkProgram},
  {"glCompressedTexImage2D", (uintptr_t)my_glCompressedTexImage2D},
  {"glEnable", (uintptr_t)my_glEnable},
  {"glTexStorage2D", (uintptr_t)my_glTexStorage2D},
  {"glTexSubImage2D", (uintptr_t)my_glTexSubImage2D},
  {"glCheckFramebufferStatus", (uintptr_t)my_glCheckFramebufferStatus},
  {"glBindFramebuffer", (uintptr_t)my_glBindFramebuffer},
  {"glReadPixels", (uintptr_t)my_glReadPixels},
  {"glFramebufferTexture2D", (uintptr_t)my_glFramebufferTexture2D},
  {"fopen", (uintptr_t)w_fopen},
  {"_ZTH7gString", (uintptr_t)tl_noop}, {"_ZTH8gString2", (uintptr_t)tl_noop},
  {"_ZTHN10ALCcontext13sLocalContextE", (uintptr_t)tl_noop},
  {"_Z24NVThreadGetCurrentJNIEnvv", (uintptr_t)NVThreadGetCurrentJNIEnv},
};
const int bully_stub_count = sizeof(bully_stub_table) / sizeof(bully_stub_table[0]);
