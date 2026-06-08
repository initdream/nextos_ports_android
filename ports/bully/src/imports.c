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
  char *s1 = str_replace_all(cat, "highp", "mediump");
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
static void (*real_glClear)(unsigned) = NULL;
static int g_cleardbg = 0;
static void my_glClear(unsigned mask) {
  if (!real_glClear) real_glClear = dlsym(RTLD_DEFAULT, "glClear");
  if (g_cleardbg < 8) { fprintf(stderr, "[gl] glClear mask=0x%x -> 0x%x\n", mask, mask | 0x4000); g_cleardbg++; }
  if (real_glClear) real_glClear(mask | 0x4000); /* força limpar COR (GL_COLOR_BUFFER_BIT) */
}
static void (*real_glClearColor)(float, float, float, float) = NULL;
static int g_ccdbg = 0;
static void my_glClearColor(float r, float g, float b, float a) {
  if (!real_glClearColor) real_glClearColor = dlsym(RTLD_DEFAULT, "glClearColor");
  if (g_ccdbg < 8) { fprintf(stderr, "[gl] glClearColor %.2f %.2f %.2f %.2f\n", r, g, b, a); g_ccdbg++; }
  if (real_glClearColor) real_glClearColor(r, g, b, a);
}
static void (*real_glTexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *) = NULL;
static void my_glTexImage2D(unsigned tgt, int lvl, int ifmt, int w, int h, int bord, unsigned fmt, unsigned type, const void *px) {
  if (!real_glTexImage2D) real_glTexImage2D = dlsym(RTLD_DEFAULT, "glTexImage2D");
  if (ifmt == 0x8058) ifmt = 0x1908;       /* GL_RGBA8 -> GL_RGBA (GLES2 não aceita sized) */
  else if (ifmt == 0x8051) ifmt = 0x1907;  /* GL_RGB8 -> GL_RGB */
  if (real_glTexImage2D) real_glTexImage2D(tgt, lvl, ifmt, w, h, bord, fmt, type, px);
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
  {"fopen", (uintptr_t)w_fopen},
  {"_ZTH7gString", (uintptr_t)tl_noop}, {"_ZTH8gString2", (uintptr_t)tl_noop},
  {"_ZTHN10ALCcontext13sLocalContextE", (uintptr_t)tl_noop},
  {"_Z24NVThreadGetCurrentJNIEnvv", (uintptr_t)NVThreadGetCurrentJNIEnv},
};
const int bully_stub_count = sizeof(bully_stub_table) / sizeof(bully_stub_table[0]);
