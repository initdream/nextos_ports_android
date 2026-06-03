/* imports.c -- shims bionic/NDK p/ os 23 UNRESOLVED do libGame.so.
 * Resolver: lookup_global (companion) -> bully_shim (aqui) -> dlsym host. */
#define _GNU_SOURCE
#include <ctype.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jni_shim.h"

/* ---- bionic libc bridges ---- */
static int *bionic___errno(void) { extern int *__errno_location(void); return __errno_location(); }
static size_t b_strlen_chk(const char *s, size_t n) { return strlen(s); }
static char *b_strrchr_chk(const char *s, int c, size_t n) { return strrchr(s, c); }
static char *b_strchr_chk(const char *s, int c, size_t n) { return strchr(s, c); }
static char *b_strncpy_chk2(char *d, const char *s, size_t n, size_t dn, size_t sn) { return strncpy(d, s, n); }
static void b_assert2(const char *f, int l, const char *fn, const char *e) {
  fprintf(stderr, "assert: %s:%d %s: %s\n", f, l, fn, e); abort();
}
static int b_android_log(int prio, const char *tag, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap);
  return 0;
}

/* bionic __sF[3] = stdin/out/err (FILE structs). Mapeamos via wrappers de stdio. */
static char bionic_sF[3][512];  /* &__sF[i] usado pelo jogo; wrappers traduzem p/ stream real */
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

/* _ctype_ legado (BSD): tabela 1+256, indexada de -1. Preenche classificacao basica. */
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

/* ---- NDK ANativeWindow (a janela e do SDL/egl_shim) ---- */
static void *aw_fromSurface(void *env, void *surface) { return (void *)0xAA11; }
static int aw_setBuffersGeometry(void *w, int x, int y, int f) { return 0; }
static int aw_getWidth(void *w) { return 1280; }
static int aw_getHeight(void *w) { return 720; }
static void aw_release(void *w) {}

/* ---- NDK AAssetManager / AAsset (le dos arquivos reais) ----
 * Bring-up: abre <ASSET_DIR>/<path> via fopen. ASSET_DIR aponta p/ assets extraidos. */
#ifndef ASSET_DIR
#define ASSET_DIR "assets"
#endif
typedef struct { FILE *fp; long len; } AAsset;
static void *am_fromJava(void *env, void *obj) { return (void *)0xA55E7; }
static void *aa_open(void *mgr, const char *path, int mode) {
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

/* ---- glGetString nunca-NULL (GameMain chama sem contexto -> NULL -> strlen crash) ---- */
static const unsigned char *w_glGetString(unsigned name) {
  static const unsigned char *(*real)(unsigned) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "glGetString");
  const unsigned char *r = real ? real(name) : NULL;
  return r ? r : (const unsigned char *)"";
}

/* ---- C++ thread-local init helpers (_ZTH*): no bring-up viram no-op ---- */
static void tl_noop(void) {}

typedef struct { const char *sym; void *fn; } Shim;
static Shim S[] = {
  {"__errno", bionic___errno}, {"__assert2", b_assert2},
  {"__strlen_chk", b_strlen_chk}, {"__strrchr_chk", b_strrchr_chk},
  {"__strchr_chk", b_strchr_chk}, {"__strncpy_chk2", b_strncpy_chk2},
  {"__android_log_print", b_android_log},
  {"__sF", bionic_sF},
  {"fprintf", w_fprintf}, {"vfprintf", w_vfprintf}, {"fwrite", w_fwrite},
  {"fputs", w_fputs}, {"fputc", w_fputc}, {"fflush", w_fflush},
  {"_ctype_", ctype_tab + 1},
  {"ANativeWindow_fromSurface", aw_fromSurface},
  {"ANativeWindow_setBuffersGeometry", aw_setBuffersGeometry},
  {"ANativeWindow_getWidth", aw_getWidth}, {"ANativeWindow_getHeight", aw_getHeight},
  {"ANativeWindow_release", aw_release},
  {"AAssetManager_fromJava", am_fromJava}, {"AAssetManager_open", aa_open},
  {"AAsset_read", aa_read}, {"AAsset_seek64", aa_seek64},
  {"AAsset_getLength64", aa_getLength64}, {"AAsset_getRemainingLength64", aa_getRemainingLength64},
  {"AAsset_close", aa_close},
  {"glGetString", w_glGetString},
  {"_ZTH7gString", tl_noop}, {"_ZTH8gString2", tl_noop},
  {"_ZTHN10ALCcontext13sLocalContextE", tl_noop},
  {"_Z24NVThreadGetCurrentJNIEnvv", NVThreadGetCurrentJNIEnv},
  {NULL, NULL}
};

void bully_imports_init(void) { ctype_init(); }

uintptr_t bully_shim(const char *name) {
  for (int i = 0; S[i].sym; i++)
    if (strcmp(S[i].sym, name) == 0) return (uintptr_t)S[i].fn;
  return 0;
}
