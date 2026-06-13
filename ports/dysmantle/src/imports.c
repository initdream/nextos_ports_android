/* imports.c (DEVICE) -- DYSMANTLE (10tons NX engine, Android) so-loader.
 *
 * Tabela de OVERRIDES (resolvida ANTES do fallback dlsym do so_resolve do
 * so_util AArch64). Tudo que NÃO está aqui cai no dlsym(RTLD_DEFAULT) ->
 * libc/libm/libGLESv2/libEGL/SDL2 pré-carregadas RTLD_GLOBAL.
 *
 *   EGL          -> egl_shim_*   (contexto GLES2 via SDL2, Mali fbdev)
 *   ANativeWindow/AAsset/ALooper extras -> impls locais (faltam no android_shim)
 *   OpenSL ES    -> opensles_shim
 *   bionic _chk/__assert2/property/log -> wrappers
 *   pthread      -> revc_pthread_table (pthread_bridge.c)
 *   libc++       -> snapshot (mesclado no main.c, módulo A)
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "so_util.h"
#include "egl_shim.h"
#include "android_shim.h"

/* ---------------- liblog ---------------- */
static int b_log_print(int prio, const char *tag, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap);
  return 0;
}
static int b_log_write(int prio, const char *tag, const char *text) {
  fprintf(stderr, "[ALOG:%d %s] %s\n", prio, tag ? tag : "?", text ? text : "");
  return 0;
}
/* NÃO faz vfprintf dos varargs (o jogo passa %s com ponteiros que podem estar
 * ruins na falha de textura -> memcpy crash) e NÃO aborta (a engine usa
 * __android_log_assert como log não-fatal aqui). */
static void b_log_assert(const char *cond, const char *tag, const char *fmt, ...) {
  (void)fmt;
  fprintf(stderr, "[ALOG-ASSERT %s] %s\n", tag ? tag : "?", cond ? cond : "");
}

/* ---------------- bionic libc ---------------- */
static int *b_errno(void) { extern int *__errno_location(void); return __errno_location(); }
static void b_assert2(const char *f, int l, const char *fn, const char *e) {
  fprintf(stderr, "assert2: %s:%d %s: %s\n", f ? f : "?", l, fn ? fn : "?", e ? e : "?");
  abort();
}
static size_t b_strlen_chk(const char *s, size_t n) { (void)n; return strlen(s); }
static char  *b_strchr_chk(const char *s, int c, size_t n) { (void)n; return strchr(s, c); }
static mode_t b_umask_chk(mode_t m) { return umask(m); }
static int    b_sys_prop_get(const char *name, char *value) {
  /* SDK 25 (N-MR1): Oboe/OpenSLES aceita Float (>=21) e NÃO tenta AAudio
   * (>=27). Sem isso: "ErrorInvalidFormat" no open do stream de som. */
  if (name && value && strcmp(name, "ro.build.version.sdk") == 0) {
    strcpy(value, "25");
    return 2;
  }
  if (value) value[0] = '\0';
  return 0;
}
/* __emutls_get_address vem do libgcc (linkado estático no loader). */
extern void *__emutls_get_address(void *);

/* bionic __sF[3] = stdin/out/err (libc++ usa p/ std::cerr/cout). UNRESOLVED ->
 * std::cerr na cadeia de erro do bitmap -> PLT loop/crash. Provemos o array +
 * wrappers que traduzem &bionic_sF[i] -> stream real. (do bully) */
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
static void b_set_abort_message(const char *m) { fprintf(stderr, "[abort_msg] %s\n", m ? m : "?"); }

/* setjmp/longjmp: o bionic NÃO salva a sigmask por padrão; mapeamos pros
 * _setjmp/_longjmp do glibc (também sem sigmask) p/ casar a ABI. O jogo usa
 * isso no error-handling do libjpeg (ImageWriterJPEG) -> sem isso, crash. */
extern int _setjmp(void *);
extern void _longjmp(void *, int) __attribute__((noreturn));

/* ---------------- ANativeWindow (faltam no android_shim) ---------------- */
extern int dys_screen_w, dys_screen_h; /* resolucao real (egl_shim) */
#define DYS_W dys_screen_w
#define DYS_H dys_screen_h
static ANativeWindow *aw_fromSurface(void *env, void *surface) {
  (void)env; (void)surface; return android_shim_get_window();
}
static void aw_acquire(void *w) { (void)w; }
static void aw_release(void *w) { (void)w; }
static int  aw_getWidth(void *w)  { (void)w; return DYS_W; }
static int  aw_getHeight(void *w) { (void)w; return DYS_H; }

/* ---------------- AAsset / AAssetManager / AAssetDir ----------------
 * O jogo lê assets/*.pak via AAssetManager. Servimos de um diretório real
 * no device (extraído do APK). Base configurável por env, default "assets". */
typedef struct { FILE *fp; long len; char path[512]; } DysAsset;
typedef struct { DIR *d; } DysAssetDir;
#include <dirent.h>
#include <fcntl.h>

static const char *assets_base(void) {
  const char *b = getenv("DYSMANTLE_ASSETS");
  return (b && *b) ? b : "assets";
}
static void *aam_fromJava(void *env, void *obj) { (void)env; (void)obj; return (void *)1; }
static void *aam_open(void *mgr, const char *fn, int mode) {
  (void)mgr; (void)mode;
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", assets_base(), fn);
  FILE *fp = fopen(path, "rb");
  if (!fp) { fprintf(stderr, "[AAsset] MISS %s\n", path); return NULL; }
  DysAsset *a = calloc(1, sizeof(DysAsset));
  a->fp = fp; fseek(fp, 0, SEEK_END); a->len = ftell(fp); fseek(fp, 0, SEEK_SET);
  snprintf(a->path, sizeof(a->path), "%s", path);
  fprintf(stderr, "[AAsset] open %s len=%ld\n", fn, a->len);
  return a;
}
static int g_ar = 0;
static int    aa_read(void *h, void *buf, size_t n) {
  DysAsset *a = h; if (!a) return -1;
  long p = ftell(a->fp); int r = (int)fread(buf, 1, n, a->fp);
  if (a->len > 100000000L && g_ar < 30) {
    unsigned char *b = buf;
    fprintf(stderr, "[pak read] pos=%ld n=%zu got=%d b=%02x%02x%02x%02x\n",
            p, n, r, r>0?b[0]:0, r>1?b[1]:0, r>2?b[2]:0, r>3?b[3]:0);
    g_ar++;
  }
  return r;
}
static long   aa_seek(void *h, long off, int wh) {
  DysAsset *a = h; if (!a) return -1; fseek(a->fp, off, wh);
  if (a->len > 100000000L && g_ar < 30)
    fprintf(stderr, "[pak seek] off=%ld wh=%d -> %ld\n", off, wh, ftell(a->fp));
  return ftell(a->fp);
}
static long   aa_seek64(void *h, long off, int wh)   { return aa_seek(h, off, wh); }
static long   aa_getLength(void *h)   { DysAsset *a = h; return a ? a->len : 0; }
static long   aa_getRemaining(void *h){ DysAsset *a = h; return a ? a->len - ftell(a->fp) : 0; }
static void   aa_close(void *h)       { DysAsset *a = h; if (a) { fclose(a->fp); free(a); } }
static int    aa_openFd(void *h, long *start, long *len) {
  DysAsset *a = h; if (!a) return -1;
  if (start) *start = 0; if (len) *len = a->len;
  fflush(a->fp);
  int fd = dup(fileno(a->fp));
  fprintf(stderr, "[AAsset] openFd %s len=%ld -> fd=%d\n", a->path, a->len, fd);
  return fd;
}
/* override read() p/ diagnóstico: loga os primeiros reads de fd alto (jogo). */
static long my_read(int fd, void *buf, size_t n) {
  static long (*real)(int, void *, size_t) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "read");
  off_t pos = (fd >= 10) ? lseek(fd, 0, SEEK_CUR) : -1;
  long r = real(fd, buf, n);
  static int c = 0;
  if (fd >= 17 && c < 120) {
    unsigned char *b = buf;
    fprintf(stderr, "[read] fd=%d pos=%ld n=%zu got=%ld b=%02x%02x%02x%02x\n",
            fd, (long)pos, n, r, r>0?b[0]:0, r>1?b[1]:0, r>2?b[2]:0, r>3?b[3]:0);
    c++;
  }
  return r;
}
/* std::ifstream (basic_filebuf::open): o jogo abre o pak de texturas via
 * ifstream com path relativo -> não acha no CWD -> lê lixo (0xffd9). Logamos e
 * redirecionamos pra assets/. g_real_filebuf_open vem do snapshot (main.c). */
void *g_real_filebuf_open = NULL;
static void *my_filebuf_open(void *self, const char *path, unsigned mode) {
  void *(*real)(void *, const char *, unsigned) = g_real_filebuf_open;
  void *r = real ? real(self, path, mode) : NULL;
  static int c = 0;
  if (c < 120) { fprintf(stderr, "[ifstream] '%s' mode=%u -> %s\n", path, mode, r?"OK":"FAIL"); c++; }
  if (!r && path && path[0] != '/') {
    char alt[1024]; snprintf(alt, sizeof(alt), "%s/%s", assets_base(), path);
    r = real ? real(self, alt, mode) : NULL;
    if (r && c < 130) fprintf(stderr, "[ifstream] -> redirect assets/%s OK\n", path);
  }
  return r;
}

/* fread/fseek override p/ diagnóstico: captura leituras de arquivo (texturas). */
static size_t my_fread(void *buf, size_t sz, size_t nm, void *fp) {
  static size_t (*real)(void *, size_t, size_t, void *) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "fread");
  long pos = ftell(fp);
  size_t r = real(buf, sz, nm, fp);
  static int c = 0;
  unsigned char *b = buf;
  /* loga reads GRANDES (JPEG/textura, não entradas de índice) + marcadores JPEG */
  if (c < 60 && (sz*nm >= 1000 || (r>=2 && b[0]==0xff && (b[1]==0xd8||b[1]==0xd9)))) {
    fprintf(stderr, "[fread] fp=%p pos=%ld sz=%zu got=%zu b=%02x%02x%02x%02x\n",
            fp, pos, sz*nm, r, r>0?b[0]:0, b[1], b[2], b[3]);
    c++;
  }
  return r;
}
static int my_fseek(void *fp, long off, int wh) {
  static int (*real)(void *, long, int) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "fseek");
  static int c = 0;
  if (c < 80) { fprintf(stderr, "[fseek] fp=%p off=%ld wh=%d\n", fp, off, wh); c++; }
  return real(fp, off, wh);
}

/* fopen override: loga path; se for arquivo do jogo não-achado no CWD,
 * tenta em assets/ (o jogo pode usar paths relativos/diferentes). */
static void *my_fopen(const char *path, const char *mode) {
  static void *(*real)(const char *, const char *) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "fopen");
  void *fp = real(path, mode);
  static int c = 0;
  if (c < 200) { fprintf(stderr, "[fopen] '%s' %s -> %s\n", path, mode, fp?"OK":"FAIL"); c++; }
  if (!fp && path && path[0] != '/') {
    char alt[1024]; snprintf(alt, sizeof(alt), "%s/%s", assets_base(), path);
    fp = real(alt, mode);
    if (fp && c < 70) { fprintf(stderr, "[fopen] -> redirect assets/%s OK\n", path); }
  }
  return fp;
}
/* força stack grande nas threads do jogo: a worker de loading tem cadeia de
 * parsing profunda; a stack pedida (bionic ~1MB) pode estourar sob glibc. */
static int my_attr_setstacksize(void *attr, size_t sz) {
  static int (*real)(void *, size_t) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "pthread_attr_setstacksize");
  if (sz < 8u * 1024 * 1024) sz = 8u * 1024 * 1024;
  return real(attr, sz);
}
static long my_read_chk(int fd, void *buf, size_t n, size_t buflen) {
  static long (*real)(int, void *, size_t) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "read");
  off_t pos = lseek(fd, 0, SEEK_CUR);
  long r = real(fd, buf, n > buflen ? buflen : n);
  static int c = 0;
  if (c < 50) {
    unsigned char *b = buf;
    fprintf(stderr, "[read_chk] fd=%d pos=%ld n=%zu got=%ld b=%02x%02x%02x%02x\n",
            fd, (long)pos, n, r, r>0?b[0]:0, r>1?b[1]:0, r>2?b[2]:0, r>3?b[3]:0);
    c++;
  }
  return r;
}
static void *aam_openDir(void *mgr, const char *dirn) {
  (void)mgr; char path[1024];
  snprintf(path, sizeof(path), "%s/%s", assets_base(), dirn && *dirn ? dirn : ".");
  DIR *d = opendir(path); if (!d) return NULL;
  DysAssetDir *ad = calloc(1, sizeof(DysAssetDir)); ad->d = d; return ad;
}
static const char *aad_getNext(void *h) {
  DysAssetDir *ad = h; if (!ad) return NULL;
  struct dirent *e;
  while ((e = readdir(ad->d))) { if (e->d_name[0] != '.') return e->d_name; }
  return NULL;
}
static void aad_close(void *h) { DysAssetDir *ad = h; if (ad) { closedir(ad->d); free(ad); } }

/* ---------------- ALooper extras (faltam no android_shim) ---------------- */
extern int ALooper_pollAll(int, int *, int *, void **);
static int  al_pollOnce(int t, int *fd, int *ev, void **data) { return ALooper_pollAll(t, fd, ev, data); }
static void *al_forThread(void) { return (void *)1; }
static void al_acquire(void *l) { (void)l; }
static void al_release(void *l) { (void)l; }
static void al_removeFd(void *l, int fd) { (void)l; (void)fd; }
static void al_wake(void *l) { (void)l; }

/* AInput extras */
static int   aie_getDeviceId(void *e) { (void)e; return 0; }
static int   ame_getButtonState(void *e) { (void)e; return 0; }

/* ---------------- OpenSL ES interface IDs ----------------
 * IDENTIDADES DO SHIM (receita Sonic Mania): o opensles_shim compara iid por
 * ponteiro com os sl_IID_* DELE. Os SL_IID_* expostos ao jogo (tabela+dlsym)
 * têm que conter esses valores; ANDROIDSIMPLEBUFFERQUEUE -> BUFFERQUEUE. */
extern uint32_t slCreateEngine_shim(void **, uint32_t, const void *, uint32_t,
                                    const void *, const void *);
extern const void *sl_IID_ENGINE, *sl_IID_PLAY, *sl_IID_VOLUME,
    *sl_IID_BUFFERQUEUE;
static const void *SL_IID_ENGINE_v, *SL_IID_PLAY_v, *SL_IID_RECORD_v,
    *SL_IID_BUFFERQUEUE_v, *SL_IID_ANDROIDSIMPLEBUFFERQUEUE_v,
    *SL_IID_ANDROIDCONFIGURATION_v = "ACFG";
__attribute__((constructor)) static void sl_iid_init(void) {
  SL_IID_ENGINE_v = sl_IID_ENGINE;
  SL_IID_PLAY_v = sl_IID_PLAY;
  SL_IID_RECORD_v = "RECORD"; /* sem suporte no shim */
  SL_IID_BUFFERQUEUE_v = sl_IID_BUFFERQUEUE;
  SL_IID_ANDROIDSIMPLEBUFFERQUEUE_v = sl_IID_BUFFERQUEUE;
}

/* EGL shim funcs já declaradas em egl_shim.h (assinaturas reais) */
static unsigned egl_releasethread_stub(void) { return 1u; }

/* glGetString: o renderer NX copia GL_EXTENSIONS/GL_VERSION em buffers de pilha
 * fixos (engine feita p/ ES3). A lista REAL do Mali Utgard estoura -> stack smash.
 * Retornamos strings curtas/controladas (ES 2.0 + extensões mínimas). */
static const unsigned char *my_glGetString(unsigned name) {
  fprintf(stderr, "[my_glGetString] name=0x%x\n", name);
  switch (name) {
  case 0x1F00: return (const unsigned char *)"NextOS";                 /* GL_VENDOR */
  case 0x1F01: return (const unsigned char *)"Mali-450 (GLES2)";       /* GL_RENDERER */
  case 0x1F02: return (const unsigned char *)"OpenGL ES 2.0";          /* GL_VERSION */
  case 0x8B8C: return (const unsigned char *)"OpenGL ES GLSL ES 1.00"; /* GL_SHADING_LANGUAGE_VERSION */
  case 0x1F03: return (const unsigned char *)                          /* GL_EXTENSIONS */
      "GL_OES_texture_npot GL_OES_depth_texture GL_OES_packed_depth_stencil "
      "GL_OES_rgb8_rgba8 GL_OES_element_index_uint GL_OES_vertex_array_object "
      "GL_EXT_texture_format_BGRA8888";
  default: {
    static const unsigned char *(*real)(unsigned) = NULL;
    if (!real) real = dlsym(RTLD_DEFAULT, "glGetString");
    const unsigned char *r = real ? real(name) : NULL;
    return r ? r : (const unsigned char *)"";
  }
  }
}

/* Roteador p/ funções GL que precisamos controlar (anti stack-smash). */
static void rgl(const char *n, void **slot);
static void store_shader_src(unsigned sh, const char *s, int len);
static void my_glShaderSource(unsigned, int, const char *const *, const int *);
static void my_glCompileShader(unsigned);
static void my_glLinkProgram(unsigned);
static void my_glTexImage3D(unsigned, int, int, int, int, int, int, unsigned,
                            unsigned, const void *);
static void my_glTexStorage3D(unsigned, int, unsigned, int, int, int);
static void my_glCompressedTexImage3D(unsigned, int, unsigned, int, int, int,
                                      int, int, const void *);
static void my_glCompressedTexImage2D(unsigned, int, unsigned, int, int, int,
                                      int, const void *);
static void my_glTexStorage2D(unsigned, int, unsigned, int, int);
void *dysmantle_gl_proc_override(const char *name) {
  if (name && strcmp(name, "glGetString") == 0) return (void *)my_glGetString;
  if (name && strcmp(name, "glShaderSource") == 0) return (void *)my_glShaderSource;
  if (name && strcmp(name, "glCompileShader") == 0) return (void *)my_glCompileShader;
  if (name && strcmp(name, "glLinkProgram") == 0) return (void *)my_glLinkProgram;
  /* GLES3-only (texture arrays/3D): logamos p/ flagrar o uso no Mali GLES2 */
  if (name && strcmp(name, "glTexImage3D") == 0) return (void *)my_glTexImage3D;
  if (name && strcmp(name, "glTexStorage3D") == 0) return (void *)my_glTexStorage3D;
  if (name && strcmp(name, "glCompressedTexImage3D") == 0) return (void *)my_glCompressedTexImage3D;
  if (name && strcmp(name, "glCompressedTexImage2D") == 0) return (void *)my_glCompressedTexImage2D;
  if (name && strcmp(name, "glTexStorage2D") == 0) return (void *)my_glTexStorage2D;
  return NULL;
}
/* loga textura COMPRIMIDA + erro GL pós-upload (ETC2/ASTC que o Mali rejeita
 * → textura branca). Mali-450 só aceita ETC1 (0x8d64). */
static void my_glCompressedTexImage2D(unsigned tgt, int lvl, unsigned ifmt,
                                      int w, int h, int border, int sz,
                                      const void *px) {
  static void (*real)(unsigned, int, unsigned, int, int, int, int,
                      const void *) = NULL;
  rgl("glCompressedTexImage2D", (void **)&real);
  static unsigned (*gerr)(void) = NULL; rgl("glGetError", (void **)&gerr);
  if (gerr) while (gerr()) {}
  if (real) real(tgt, lvl, ifmt, w, h, border, sz, px);
  unsigned e = gerr ? gerr() : 0;
  static int n = 0;
  if (n < 40 || e) {
    fprintf(stderr, "[CTEX] glCompressedTexImage2D ifmt=0x%x %dx%d sz=%d lvl=%d -> err=0x%x\n",
            ifmt, w, h, sz, lvl, e);
    n++;
  }
}
static void my_glTexStorage2D(unsigned tgt, int lvls, unsigned ifmt, int w,
                              int h) {
  static void (*real)(unsigned, int, unsigned, int, int) = NULL;
  rgl("glTexStorage2D", (void **)&real);
  static int n = 0;
  if (n < 40) { fprintf(stderr, "[TEXSTOR] glTexStorage2D ifmt=0x%x %dx%d lvls=%d\n", ifmt, w, h, lvls); n++; }
  if (real) real(tgt, lvls, ifmt, w, h);
}
/* glTexImage2D: loga ifmt/fmt/type/dim + erro GL. ifmt GLES3 (ex: GL_RGBA8
 * 0x8058, GL_SRGB8 0x8C41, sized) NÃO existe no GLES2 → INVALID_ENUM →
 * textura branca. GLES2 quer ifmt = base (GL_RGBA 0x1908) sem sized. */
/* ===== AUTO-FIX do mismatch stride buffer(0x7F=40) × atributos(0x7=24) =====
 * O render às vezes liga um buffer de formato MAIOR (variante 0x7F/40B, pq a
 * variante pedida falhou de criar) mas configura atributos com stride do
 * formato pedido (0x7/24B). A GPU lê vértices errados → branco. Rastreamos o
 * stride REAL de cada GL buffer (via formato do createvb) e, no
 * glVertexAttribPointer, se o buffer ligado tem stride maior, usamos o dele
 * (os offsets pos@0/cor@12/uv@16 valem nos dois formatos). */
static unsigned g_cur_array_buf = 0;
static int g_buf_stride[4096];        /* id -> stride real do buffer */
static long g_buf_size[4096];         /* id -> tamanho do buffer (p/ divisibilidade) */
/* tabela (tamanho do buffer de vértice -> stride), preenchida pelo createvb
 * (sabe formato→stride e count). O upload glBufferData casa por tamanho. */
static struct { long size; int stride; } g_szstride[512];
static int g_szs_n = 0;
void dysmantle_record_vbsize(long size, int stride) {
  if (size <= 0 || stride <= 0) return;
  g_szstride[g_szs_n % 512].size = size;
  g_szstride[g_szs_n % 512].stride = stride;
  g_szs_n++;
}
static int lookup_stride_by_size(long size) {
  int lim = g_szs_n < 512 ? g_szs_n : 512;
  for (int i = 0; i < lim; i++)
    if (g_szstride[i].size == size) return g_szstride[i].stride;
  return 0;
}
static int g_stridefix = -1;
static void my_glBindBuffer(unsigned tgt, unsigned id) {
  static void (*real)(unsigned, unsigned) = NULL; rgl("glBindBuffer", (void **)&real);
  if (tgt == 0x8892) g_cur_array_buf = id; /* GL_ARRAY_BUFFER */
  if (real) real(tgt, id);
}

/* trace de binding de textura no momento do draw */
static unsigned g_active_unit = 0, g_bound_tex[8] = {0};
static void my_glActiveTexture(unsigned tex) {
  static void (*real)(unsigned) = NULL; rgl("glActiveTexture", (void **)&real);
  g_active_unit = tex - 0x84C0; /* GL_TEXTURE0 */
  if (g_active_unit >= 8) g_active_unit = 0;
  if (real) real(tex);
}
static void my_glBindTexture(unsigned tgt, unsigned tex) {
  static void (*real)(unsigned, unsigned) = NULL; rgl("glBindTexture", (void **)&real);
  if (g_active_unit < 8) g_bound_tex[g_active_unit] = tex;
  if (real) real(tgt, tex);
}
static void my_glUniform1i(int loc, int v) {
  static void (*real)(int, int) = NULL; rgl("glUniform1i", (void **)&real);
  static int n = 0;
  if (getenv("DYSMANTLE_UNIF_LOG") && n < 50) {
    fprintf(stderr, "[UNIF1i] loc=%d val=%d\n", loc, v); n++;
  }
  if (real) real(loc, v);
}
static int my_glGetUniformLocation(unsigned prog, const char *nm) {
  static int (*real)(unsigned, const char *) = NULL;
  rgl("glGetUniformLocation", (void **)&real);
  int r = real ? real(prog, nm) : -1;
  static int n = 0;
  if (getenv("DYSMANTLE_UNIF_LOG") && n < 50 && nm && strstr(nm, "tex")) {
    fprintf(stderr, "[UNIFLOC] %s -> loc %d (prog %u)\n", nm, r, prog); n++;
  }
  return r;
}
static unsigned g_cur_prog = 0;
static void my_glUseProgram(unsigned p) {
  static void (*real)(unsigned) = NULL; rgl("glUseProgram", (void **)&real);
  g_cur_prog = p;
  if (real) real(p);
}
static void my_glBindAttribLocation(unsigned prog, unsigned idx, const char *nm) {
  static void (*real)(unsigned, unsigned, const char *) = NULL;
  rgl("glBindAttribLocation", (void **)&real);
  static int n = 0;
  if (getenv("DYSMANTLE_ATTR_LOG") && n < 30) {
    fprintf(stderr, "[BINDATTR] %s -> loc %u\n", nm ? nm : "?", idx); n++;
  }
  if (real) real(prog, idx, nm);
}

/* trace de atributos de vértice (UVs erradas → textura amostra canto = branco) */
static void my_glVertexAttribPointer(unsigned idx, int sz, unsigned typ,
                                     unsigned char norm, int stride, const void *ptr) {
  static void (*real)(unsigned, int, unsigned, unsigned char, int, const void *) = NULL;
  rgl("glVertexAttribPointer", (void **)&real);
  static int n = 0;
  if (getenv("DYSMANTLE_ATTR_LOG") && n < 40) {
    fprintf(stderr, "[ATTR] idx=%u size=%d type=0x%x norm=%d stride=%d off=%ld\n",
            idx, sz, typ, norm, stride, (long)(uintptr_t)ptr); n++;
  }
  /* loga o atributo de COR (ubyte normalizado) p/ cada stride distinto */
  if (getenv("DYSMANTLE_ATTR_LOG") && typ == 0x1401 && norm) {
    static int seen[128]; static int sn = 0; int known = 0;
    for (int i = 0; i < sn; i++) if (seen[i] == stride) { known = 1; break; }
    if (!known && sn < 128) { seen[sn++] = stride;
      fprintf(stderr, "[ATTRCOL] stride=%d cor_off=%ld size=%d\n", stride, (long)(uintptr_t)ptr, sz); }
  }
  /* AUTO-FIX por DIVISIBILIDADE: o stride real SEMPRE divide o tamanho do
   * buffer. Se o stride do atributo (ex 24) NÃO divide o tamanho mas existe um
   * stride de vértice conhecido MAIOR que divide (ex 40), usa esse. Os offsets
   * pos@0/cor@12/uv@16 batem (24 primeiros bytes do 0x7F == 0x7). */
  /* OFF por default: o chão usa stride 40 CORRETO (red herring); o fix só
   * arriscava quebrar sprites. DYSMANTLE_STRIDEFIX=1 p/ reativar/experimentar. */
  if (g_stridefix < 0) g_stridefix = getenv("DYSMANTLE_STRIDEFIX") ? 1 : 0;
  if (getenv("DYSMANTLE_STRIDE_DBG")) {
    static int d = 0;
    if (d < 30 && stride >= 12 && g_cur_array_buf < 4096 && g_buf_size[g_cur_array_buf] > 50000) {
      long sb = g_cur_array_buf < 4096 ? g_buf_size[g_cur_array_buf] : -1;
      fprintf(stderr, "[STRIDEDBG] buf=%u size=%ld attr_stride=%d size%%stride=%ld\n",
              g_cur_array_buf, sb, stride, sb > 0 ? sb % stride : -1); d++;
    }
  }
  if (g_stridefix && stride > 0 && g_cur_array_buf < 4096) {
    long sz_buf = g_buf_size[g_cur_array_buf];
    if (sz_buf > 0 && (sz_buf % stride) != 0) {
      const int cands[] = {28,32,36,40,44,48,52,56,60};
      for (unsigned c = 0; c < sizeof(cands)/sizeof(cands[0]); c++) {
        int S = cands[c];
        if (S > stride && (sz_buf % S) == 0 && (long)(uintptr_t)ptr + sz*4 <= S) {
          static int fl = 0;
          if (fl < 10) { fprintf(stderr, "[STRIDEFIX] buf=%u size=%ld attr=%d -> %d (idx=%u off=%ld)\n",
                                 g_cur_array_buf, sz_buf, stride, S, idx, (long)(uintptr_t)ptr); fl++; }
          stride = S; break;
        }
      }
    }
  }
  if (real) real(idx, sz, typ, norm, stride, ptr);
}
static int my_glGetAttribLocation(unsigned prog, const char *name) {
  static int (*real)(unsigned, const char *) = NULL;
  rgl("glGetAttribLocation", (void **)&real);
  int r = real ? real(prog, name) : -1;
  static int n = 0;
  if (getenv("DYSMANTLE_ATTR_LOG") && n < 40) {
    fprintf(stderr, "[ATTRLOC] %s -> %d\n", name ? name : "?", r); n++;
  }
  return r;
}

/* ---- FBO diag (render-to-texture do mundo: incompleto no Mali → branco) ---- */
static unsigned g_cur_fbo = 0;
static unsigned long g_draws_fbo = 0, g_draws_screen = 0;
static void my_glBindFramebuffer(unsigned tgt, unsigned fb) {
  static void (*real)(unsigned, unsigned) = NULL;
  rgl("glBindFramebuffer", (void **)&real);
  /* DIAG: ao SAIR do FBO da cena (fbo!=0 -> 0), lê o conteúdo do FBO p/ ver a
   * cena SEM o composite. Dump 1x após gameplay (g_draws_fbo já alto). */
  if (getenv("DYSMANTLE_FBO_DUMP") && g_cur_fbo != 0 && fb == 0 &&
      g_draws_fbo > 2000) {
    static int done = 0;
    if (!done) {
      done = 1;
      static void (*rp)(int,int,int,int,unsigned,unsigned,void*) = NULL;
      rgl("glReadPixels", (void **)&rp);
      int W = 1280, H = 720;
      unsigned char *buf = malloc((size_t)W * H * 4);
      if (rp && buf) {
        rp(0, 0, W, H, 0x1908, 0x1401, buf); /* RGBA, UBYTE */
        FILE *f = fopen("fbo_scene.raw", "wb");
        if (f) { fwrite(buf, 1, (size_t)W * H * 4, f); fclose(f); }
        fprintf(stderr, "[FBO_DUMP] cena do FBO salva (fbo_scene.raw) draws_fbo=%lu\n", g_draws_fbo);
      }
      free(buf);
    }
  }
  g_cur_fbo = fb;
  if (real) real(tgt, fb);
}
static void my_glDrawElements(unsigned mode, int cnt, unsigned typ, const void *idx) {
  static void (*real)(unsigned, int, unsigned, const void *) = NULL;
  rgl("glDrawElements", (void **)&real);
  if (g_cur_fbo) g_draws_fbo++; else g_draws_screen++;
  static unsigned long t = 0;
  if ((++t % 3000) == 0)
    fprintf(stderr, "[DRAWSTATS] fbo=%lu screen=%lu (cur_fbo=%u)\n",
            g_draws_fbo, g_draws_screen, g_cur_fbo);
  static int drawlog = -1;
  if (drawlog < 0) drawlog = getenv("DYSMANTLE_DRAW_LOG") ? 1 : 0;
  if (drawlog) {
    static int dn = 0;
    /* draws GRANDES (chão/terreno) dentro do FBO: loga programa + texturas */
    if (g_cur_fbo && cnt > 3000 && dn < 30) {
      fprintf(stderr, "[BIGDRAW] prog=%u cnt=%d unit=%u tex[0]=%u tex[1]=%u\n",
              g_cur_prog, cnt, g_active_unit, g_bound_tex[0], g_bound_tex[1]);
      dn++;
    }
  }
  if (real) real(mode, cnt, typ, idx);
}
static void my_glDrawArrays(unsigned mode, int first, int cnt) {
  static void (*real)(unsigned, int, int) = NULL;
  rgl("glDrawArrays", (void **)&real);
  if (g_cur_fbo) g_draws_fbo++; else g_draws_screen++;
  if (real) real(mode, first, cnt);
}
void dysmantle_draw_stats(void) {
  fprintf(stderr, "[DRAWSTATS] fbo=%lu screen=%lu (cur_fbo=%u)\n",
          g_draws_fbo, g_draws_screen, g_cur_fbo);
}

static void my_glFramebufferTexture2D(unsigned tgt, unsigned att, unsigned ttgt,
                                      unsigned tex, int lvl) {
  static void (*real)(unsigned, unsigned, unsigned, unsigned, int) = NULL;
  rgl("glFramebufferTexture2D", (void **)&real);
  if (real) real(tgt, att, ttgt, tex, lvl);
  static int n = 0;
  if (n < 30) { fprintf(stderr, "[FBO] FramebufferTexture2D att=0x%x tex=%u\n", att, tex); n++; }
}
static void my_glRenderbufferStorage(unsigned tgt, unsigned ifmt, int w, int h) {
  static void (*real)(unsigned, unsigned, int, int) = NULL;
  rgl("glRenderbufferStorage", (void **)&real);
  if (real) real(tgt, ifmt, w, h);
  static int n = 0;
  if (n < 30) { fprintf(stderr, "[FBO] RenderbufferStorage ifmt=0x%x %dx%d\n", ifmt, w, h); n++; }
}
static unsigned my_glCheckFramebufferStatus(unsigned tgt) {
  static unsigned (*real)(unsigned) = NULL;
  rgl("glCheckFramebufferStatus", (void **)&real);
  unsigned s = real ? real(tgt) : 0x8CD5;
  static int n = 0;
  if (s != 0x8CD5 /*COMPLETE*/ || n < 30) {
    fprintf(stderr, "[FBO] CheckFramebufferStatus -> 0x%x %s\n", s,
            s == 0x8CD5 ? "COMPLETE" : "INCOMPLETO!!!");
    n++;
  }
  return s;
}

/* trace de criação de buffer + thread (worker thread sem contexto GL?) */
static void my_glBufferData(unsigned tgt, long size, const void *data, unsigned usage) {
  static void (*real)(unsigned, long, const void *, unsigned) = NULL;
  rgl("glBufferData", (void **)&real);
  static unsigned (*gerr)(void) = NULL; rgl("glGetError", (void **)&gerr);
  if (gerr) while (gerr()) {}
  if (real) real(tgt, size, data, usage);
  unsigned e = gerr ? gerr() : 0;
  /* DETECTA o stride real do buffer de vértice pela estrutura: o stride certo
   * deixa as UVs (offset 16, 2 floats) consistentemente em ~[0,1] p/ vários
   * vértices. Escolhe o MENOR stride com >85% de UVs válidas. */
  /* registra o TAMANHO do buffer (p/ deduzir stride por divisibilidade no draw) */
  if (tgt == 0x8892 && g_cur_array_buf < 4096) g_buf_size[g_cur_array_buf] = size;
  /* TESTE: tinge a cor-de-vértice (rgba8 @12) dos buffers GRANDES (terreno,
   * stride 40) de verde — se o chão ficar verde, vary_color branco é o muro. */
  if (tgt == 0x8892 && data && getenv("DYSMANTLE_TINT_GREEN") && size > 2000 &&
      (size % 40) == 0) {
    unsigned char *b = (unsigned char *)data; /* nota: data é const; copiamos */
    static unsigned char *cp = NULL; static long cpsz = 0;
    if (cpsz < size) { free(cp); cp = malloc(size); cpsz = size; }
    if (cp) {
      memcpy(cp, b, size);
      for (long v = 0; v + 40 <= size; v += 40) { cp[v+12]=40; cp[v+13]=180; cp[v+14]=60; }
      if (real) real(tgt, size, cp, usage); /* re-upload tingido (sobrescreve) */
      static int tn = 0; if (tn < 3) { fprintf(stderr, "[TINT] buffer %ld verde\n", size); tn++; }
    }
  }
  static int n = 0;
  if (getenv("DYSMANTLE_BUF_LOG") && (n < 60 || e)) {
    fprintf(stderr, "[BUFDATA] tid=%d tgt=0x%x size=%ld data=%s usage=0x%x -> err=0x%x\n",
            (int)syscall(178), tgt, size, data ? "ptr" : "NULL", usage, e); n++;
  }
  /* dump dos vértices do buffer GIGANTE (chão): pos vec3@0 + cor rgba8@12 +
   * uv vec2@16, stride 24. Vê se UVs/cor estão válidas. */
  if (getenv("DYSMANTLE_VERT_DUMP") && data && size > 200000 && tgt == 0x8892) {
    static int vd = 0;
    if (vd < 2) {
      const unsigned char *b = (const unsigned char *)data;
      fprintf(stderr, "[VERTDUMP] buffer size=%ld (~%ld verts stride24)\n", size, size/24);
      for (int v = 0; v < 6; v++) {
        const float *pos = (const float *)(b + v*24);
        const unsigned char *col = b + v*24 + 12;
        const float *uv = (const float *)(b + v*24 + 16);
        fprintf(stderr, "  v%d: pos(%.1f,%.1f,%.1f) col(%d,%d,%d,%d) uv(%.3f,%.3f)\n",
                v, pos[0],pos[1],pos[2], col[0],col[1],col[2],col[3], uv[0],uv[1]);
      }
      vd++;
    }
  }
}

/* DIAG luz/sol: loga blends usados + permite forçar/desligar passes aditivos
 * (luz do player/sol estourada lavando o terreno de branco). */
static void my_glBlendFunc(unsigned sf, unsigned df) {
  static void (*real)(unsigned, unsigned) = NULL; rgl("glBlendFunc", (void **)&real);
  static int seen[64]; static int sn = 0;
  if (getenv("DYSMANTLE_BLEND_LOG")) {
    int key = (sf<<16)|df, known=0;
    for (int i=0;i<sn;i++) if(seen[i]==key){known=1;break;}
    if(!known && sn<64){seen[sn++]=key; fprintf(stderr,"[BLEND] src=0x%x dst=0x%x\n",sf,df);}
  }
  /* =1: troca aditivo (ONE,ONE / SRC_ALPHA,ONE) por alpha normal (teste luz) */
  if (getenv("DYSMANTLE_NO_ADD")) {
    if ((sf==1 && df==1) || (sf==0x302 && df==1)) { sf=0x302; df=0x303; } /* SRC_ALPHA,1-SRC_ALPHA */
  }
  if (real) real(sf, df);
}
static void my_glBlendFuncSeparate(unsigned sf, unsigned df, unsigned sa, unsigned da) {
  static void (*real)(unsigned, unsigned, unsigned, unsigned) = NULL;
  rgl("glBlendFuncSeparate", (void **)&real);
  static int seen[64]; static int sn = 0;
  if (getenv("DYSMANTLE_BLEND_LOG")) {
    int key = (sf<<16)|df, known=0;
    for (int i=0;i<sn;i++) if(seen[i]==key){known=1;break;}
    if(!known && sn<64){seen[sn++]=key; fprintf(stderr,"[BLENDSEP] src=0x%x dst=0x%x srcA=0x%x dstA=0x%x\n",sf,df,sa,da);}
  }
  if (getenv("DYSMANTLE_NO_ADD")) {
    if ((sf==1 && df==1) || (sf==0x302 && df==1)) { sf=0x302; df=0x303; }
  }
  if (real) real(sf, df, sa, da);
}
/* DIAG depth: força glDepthFunc=ALWAYS (depth-texture FBO quebrado no Utgard?
 * → mundo culado). DYSMANTLE_DEPTH_ALWAYS=1. =2 desliga depth test inteiro. */
static void my_glDepthFunc(unsigned f) {
  static void (*real)(unsigned) = NULL; rgl("glDepthFunc", (void **)&real);
  if (getenv("DYSMANTLE_DEPTH_ALWAYS")) f = 0x0207; /* GL_ALWAYS */
  if (real) real(f);
}
static void my_glEnable(unsigned cap) {
  static void (*real)(unsigned) = NULL; rgl("glEnable", (void **)&real);
  const char *d = getenv("DYSMANTLE_DEPTH_ALWAYS");
  if (d && atoi(d) == 2 && cap == 0x0B71 /*DEPTH_TEST*/) return; /* não habilita */
  if (real) real(cap);
}

/* DIAG: força clear color (magenta) p/ distinguir branco-geometria de fundo */
static void my_glClearColor(float r, float g, float b, float a) {
  static void (*real)(float, float, float, float) = NULL;
  rgl("glClearColor", (void **)&real);
  /* =1: tudo magenta. =2: só TELA (fbo 0) magenta. =3: só FBO magenta. */
  const char *t = getenv("DYSMANTLE_CLEAR_TEST");
  if (t) {
    int m = atoi(t);
    int hit = (m == 1) || (m == 2 && g_cur_fbo == 0) || (m == 3 && g_cur_fbo != 0);
    if (hit) { r = 1.0f; g = 0.0f; b = 1.0f; a = 1.0f; }
  }
  if (real) real(r, g, b, a);
}

/* glTexParameteri: Mali-450 GLES2 NÃO completa textura NPOT com GL_REPEAT nem
 * com min-filter mipmap → amostra branco. Forçamos CLAMP_TO_EDGE + filtro
 * não-mipmap (NPOT-safe). DYSMANTLE_NPOT_OFF desliga. */
static int g_npot_fix = -1;
static void my_glTexParameteri(unsigned tgt, unsigned pname, int param) {
  static void (*real)(unsigned, unsigned, int) = NULL;
  rgl("glTexParameteri", (void **)&real);
  if (g_npot_fix < 0) g_npot_fix = getenv("DYSMANTLE_NPOT_OFF") ? 0 : 1;
  static int n = 0;
  if (getenv("DYSMANTLE_TEXPARAM_LOG") && n < 40) {
    fprintf(stderr, "[TEXPARAM] pname=0x%x param=0x%x\n", pname, param); n++;
  }
  if (g_npot_fix) {
    if (pname == 0x2802 /*WRAP_S*/ || pname == 0x2803 /*WRAP_T*/)
      param = 0x812F; /* CLAMP_TO_EDGE */
    else if (pname == 0x2801 /*MIN_FILTER*/) {
      if (param == 0x2700 || param == 0x2701 || param == 0x2702 || param == 0x2703)
        param = 0x2601; /* mipmap → LINEAR */
    }
  }
  if (real) real(tgt, pname, param);
}
static int g_tex_log = -1, g_tex_fix = -1;
static void my_glTexImage2D(unsigned tgt, int lvl, int ifmt, int w, int h,
                            int border, unsigned fmt, unsigned typ,
                            const void *px) {
  static void (*real)(unsigned, int, int, int, int, int, unsigned, unsigned,
                      const void *) = NULL;
  rgl("glTexImage2D", (void **)&real);
  static unsigned (*gerr)(void) = NULL; rgl("glGetError", (void **)&gerr);
  if (g_tex_log < 0) g_tex_log = getenv("DYSMANTLE_TEX_LOG") ? 1 : 0;
  if (g_tex_fix < 0) g_tex_fix = getenv("DYSMANTLE_TEX_NOFIX") ? 0 : 1;
  int orig = ifmt;
  if (g_tex_fix) {
    /* normaliza internalformat sized (GLES3) → base (GLES2) */
    switch (ifmt) {
      case 0x8058: /*RGBA8*/ case 0x8C43: /*SRGB8_ALPHA8*/ case 0x881A: /*RGBA16F*/
      case 0x8814: /*RGBA32F*/ ifmt = 0x1908; break; /* GL_RGBA */
      case 0x8051: /*RGB8*/ case 0x8C41: /*SRGB8*/ case 0x881B: /*RGB16F*/
      case 0x8815: /*RGB32F*/ ifmt = 0x1907; break; /* GL_RGB */
      case 0x8229: /*R8*/ case 0x822E: /*R32F*/ ifmt = 0x1909; break; /* LUMINANCE */
      case 0x822B: /*RG8*/ ifmt = 0x190A; break; /* LUMINANCE_ALPHA */
      default: break;
    }
  }
  /* 🧠 TEORIA MEMÓRIA UTGARD (Bully): muitas/grandes texturas estouram a memória
   * de textura da GPU → uploads tardios (terreno) falham/somem. DYSMANTLE_TEX_HALF
   * reduz texturas grandes pela metade (box 2x2) p/ liberar memória. */
  if (getenv("DYSMANTLE_TEX_HALF") && px && lvl == 0 && fmt == 0x1908 &&
      typ == 0x1401 && w >= 256 && h >= 256 && (w & 1) == 0 && (h & 1) == 0) {
    int hw = w / 2, hh = h / 2;
    static unsigned char *half = NULL; static long hcap = 0;
    long need = (long)hw * hh * 4;
    if (hcap < need) { free(half); half = malloc(need); hcap = need; }
    if (half) {
      const unsigned char *src = (const unsigned char *)px;
      for (int y = 0; y < hh; y++)
        for (int x = 0; x < hw; x++) {
          long s0 = ((long)(y*2)*w + x*2)*4, s1 = s0+4, s2 = s0+(long)w*4, s3 = s2+4;
          unsigned char *d = half + ((long)y*hw + x)*4;
          for (int c = 0; c < 4; c++) d[c] = (src[s0+c]+src[s1+c]+src[s2+c]+src[s3+c])/4;
        }
      if (gerr) while (gerr()) {}
      if (real) real(tgt, lvl, ifmt, hw, hh, border, fmt, typ, half);
      static int hn = 0; if (hn < 4) { fprintf(stderr, "[TEXHALF] %dx%d -> %dx%d\n", w, h, hw, hh); hn++; }
      return;
    }
  }
  if (gerr) while (gerr()) {}
  if (real) real(tgt, lvl, ifmt, w, h, border, fmt, typ, px);
  unsigned e = gerr ? gerr() : 0;
  if (g_tex_log) {
    /* histograma de (fmt,typ) distintos — pega LUMINANCE/ALPHA escondidos */
    static unsigned seen_fmt[32]; static int nf = 0;
    unsigned key = (fmt << 8) | (typ & 0xff);
    int known = 0;
    for (int i = 0; i < nf; i++) if (seen_fmt[i] == key) { known = 1; break; }
    if (!known && nf < 32) {
      seen_fmt[nf++] = key;
      const char *fn = fmt==0x1908?"RGBA":fmt==0x1907?"RGB":fmt==0x1909?"LUMINANCE":
                       fmt==0x190A?"LUMINANCE_ALPHA":fmt==0x1906?"ALPHA":"?";
      fprintf(stderr, "[TEXFMT] fmt=0x%x(%s) typ=0x%x\n", fmt, fn, typ);
    }
    static int n = 0;
    unsigned tid = g_bound_tex[g_active_unit < 8 ? g_active_unit : 0];
    /* salva a imagem inteira de texturas grandes p/ inspeção (DYSMANTLE_TEX_SAVE) */
    if (px && lvl == 0 && getenv("DYSMANTLE_TEX_SAVE") && w >= 128 && h >= 128 && fmt == 0x1908) {
      static int sv = 0;
      if (sv < 80) {
        char nm[80]; snprintf(nm, sizeof(nm), "texdump_t%u_%dx%d.raw", tid, w, h);
        FILE *tf = fopen(nm, "wb");
        if (tf) { fwrite(px, 1, (size_t)w*h*4, tf); fclose(tf);
          fprintf(stderr, "[TEXSAVE] %s\n", nm); sv++; }
      }
    }
    if (px && lvl == 0 && w >= 64 && h >= 64 && fmt == 0x1908) {
      const unsigned char *b = (const unsigned char *)px;
      char grid[160]; int gi = 0; int colorful = 0;
      for (int k = 0; k < 6; k++) {
        long o = ((long)((k*7+3)%h * 0 + (k+1)*h/8) * w + (k+1)*w/8) * 4;
        int R=b[o],G=b[o+1],B=b[o+2];
        int mx = R>G?(R>B?R:B):(G>B?G:B), mn = R<G?(R<B?R:B):(G<B?G:B);
        if (mx - mn > 24) colorful++;
        gi += snprintf(grid+gi, sizeof(grid)-gi, "%02x%02x%02x ", R,G,B);
      }
      static int gn = 0;
      if (gn < 30) { fprintf(stderr, "[TEXRGB] tex=%u %dx%d %s rgb: %s\n",
                             tid, w, h, colorful>=2?"COLORIDA":"grayscale", grid); gn++; }
    }
    if (n < 80 || e) {
      char pix[80] = "(null)";
      if (px && lvl == 0 && w >= 4 && h >= 4) {
        const unsigned char *b = (const unsigned char *)px;
        long mid = ((long)h / 2 * w + w / 2) * 4;
        snprintf(pix, sizeof(pix), "p0=%02x%02x%02x%02x mid=%02x%02x%02x%02x",
                 b[0], b[1], b[2], b[3], b[mid], b[mid+1], b[mid+2], b[mid+3]);
      }
      fprintf(stderr, "[TEX2D] tid=%d tex=%u ifmt=0x%x %dx%d fmt=0x%x typ=0x%x -> err=0x%x %s\n",
              (int)syscall(178), tid, ifmt, w, h, fmt, typ, e, pix); n++;
    }
  }
}
static void my_glTexImage3D(unsigned tgt, int lvl, int ifmt, int w, int h,
                            int d, int border, unsigned fmt, unsigned typ,
                            const void *px) {
  static void (*real)(unsigned, int, int, int, int, int, int, unsigned,
                      unsigned, const void *) = NULL;
  rgl("glTexImage3D", (void **)&real);
  fprintf(stderr, "[TEX3D] glTexImage3D tgt=0x%x ifmt=0x%x %dx%dx%d fmt=0x%x\n",
          tgt, ifmt, w, h, d, fmt);
  if (real) real(tgt, lvl, ifmt, w, h, d, border, fmt, typ, px);
}
static void my_glTexStorage3D(unsigned tgt, int lvls, unsigned ifmt, int w,
                              int h, int d) {
  static void (*real)(unsigned, int, unsigned, int, int, int) = NULL;
  rgl("glTexStorage3D", (void **)&real);
  fprintf(stderr, "[TEX3D] glTexStorage3D tgt=0x%x ifmt=0x%x %dx%dx%d\n", tgt,
          ifmt, w, h, d);
  if (real) real(tgt, lvls, ifmt, w, h, d);
}
static void my_glCompressedTexImage3D(unsigned tgt, int lvl, unsigned ifmt,
                                      int w, int h, int d, int border,
                                      int sz, const void *px) {
  static void (*real)(unsigned, int, unsigned, int, int, int, int, int,
                      const void *) = NULL;
  rgl("glCompressedTexImage3D", (void **)&real);
  fprintf(stderr, "[TEX3D] glCompressedTexImage3D tgt=0x%x ifmt=0x%x %dx%dx%d\n",
          tgt, ifmt, w, h, d);
  if (real) real(tgt, lvl, ifmt, w, h, d, border, sz, px);
}

/* A engine resolve as funções GL via dlsym DIRETO (libGLESv2 do device),
 * driblando a tabela de imports E o eglGetProcAddress. Interceptamos dlsym
 * p/ devolver NOSSO glGetString (strings curtas) e deixar o resto passar.
 * Oboe também faz dlopen("libOpenSLES.so")+dlsym em runtime (linkOpenSLES) ->
 * roteamos pro opensles_shim (receita do Sonic Mania). */
#define SL_MAGIC ((void *)0x5151ABCDul)
extern uint32_t slCreateEngine_shim(void **, uint32_t, const void *, uint32_t,
                                    const void *, const void *);
static void *sl_dlsym(const char *name) {
  if (!name) return NULL;
  if (strcmp(name, "slCreateEngine") == 0) return (void *)slCreateEngine_shim;
  if (strcmp(name, "SL_IID_ENGINE") == 0) return (void *)&SL_IID_ENGINE_v;
  if (strcmp(name, "SL_IID_PLAY") == 0) return (void *)&SL_IID_PLAY_v;
  if (strcmp(name, "SL_IID_RECORD") == 0) return (void *)&SL_IID_RECORD_v;
  if (strcmp(name, "SL_IID_BUFFERQUEUE") == 0) return (void *)&SL_IID_BUFFERQUEUE_v;
  if (strcmp(name, "SL_IID_ANDROIDSIMPLEBUFFERQUEUE") == 0)
    return (void *)&SL_IID_ANDROIDSIMPLEBUFFERQUEUE_v;
  if (strcmp(name, "SL_IID_ANDROIDCONFIGURATION") == 0)
    return (void *)&SL_IID_ANDROIDCONFIGURATION_v;
  fprintf(stderr, "[sl] dlsym %s -> NULL\n", name);
  return NULL;
}
static void *my_dlsym(void *handle, const char *name) {
  void *ov = dysmantle_gl_proc_override(name);
  if (ov) { fprintf(stderr, "[my_dlsym] override %s\n", name); return ov; }
  if (handle == SL_MAGIC) {
    void *r = sl_dlsym(name);
    fprintf(stderr, "[sl] dlsym %s -> %p\n", name ? name : "?", r);
    return r;
  }
  return dlsym(handle, name);
}
static void *my_dlopen(const char *name, int flag) {
  if (name && strstr(name, "OpenSLES")) {
    fprintf(stderr, "[sl] dlopen %s -> shim\n", name);
    return SL_MAGIC;
  }
  return dlopen(name, flag);
}
static int   my_dlclose(void *h) { return dlclose(h); }

/* ---- GL wrappers com LOG (pinpoint do stack-smash no renderer init) ---- */
static void rgl(const char *n, void **slot) {
  if (!*slot) *slot = dlsym(RTLD_DEFAULT, n);
}
static void my_glGetIntegerv(unsigned pname, int *params) {
  static void (*real)(unsigned, int *) = NULL; rgl("glGetIntegerv", (void **)&real);
  fprintf(stderr, "[GL] glGetIntegerv(0x%x)\n", pname);
  /* pnames multi-valor que estouram buffer de 1 int -> zera */
  if (pname == 0x86A3 /*COMPRESSED_TEXTURE_FORMATS*/ ||
      pname == 0x8DF8 /*SHADER_BINARY_FORMATS*/ ||
      pname == 0x87FE /*PROGRAM_BINARY_FORMATS*/) { if (params) params[0] = 0; return; }
  if (real) real(pname, params); else if (params) params[0] = 0;
}
static const unsigned char *my_glGetStringi(unsigned name, unsigned index) {
  fprintf(stderr, "[GL] glGetStringi(0x%x, %u)\n", name, index);
  return (const unsigned char *)"";
}
static unsigned my_glGetError(void) {
  static unsigned (*real)(void) = NULL; rgl("glGetError", (void **)&real);
  return real ? real() : 0;
}

/* ---- Interceptação de SHADERS (diag do mundo branco) ----
 * Loga source (gated DYSMANTLE_SHADER_DUMP) e SEMPRE loga erro de compile/link.
 * A engine é ES3; shaders #version 300 es podem falhar no Mali GLES2 → render
 * cai p/ fallback branco. */
static int g_shader_dump = -1;
static int shader_dump_on(void) {
  if (g_shader_dump < 0) g_shader_dump = getenv("DYSMANTLE_SHADER_DUMP") ? 1 : 0;
  return g_shader_dump;
}
static void my_glShaderSource(unsigned sh, int count, const char *const *str,
                              const int *len) {
  static void (*real)(unsigned, int, const char *const *, const int *) = NULL;
  rgl("glShaderSource", (void **)&real);
  if (shader_dump_on()) {
    fprintf(stderr, "[SHADER src #%u] count=%d:\n", sh, count);
    for (int i = 0; i < count && i < 8; i++)
      fprintf(stderr, "%.*s", len && len[i] > 0 ? len[i] : 2000, str[i]);
    fprintf(stderr, "\n[/SHADER src #%u]\n", sh);
  }
  store_shader_src(sh, str[0], len ? len[0] : -1);
  /* DIAG: reescreve o fim do fragment shader. RED=vermelho sólido (localiza
   * geometria). TEX=só a textura (sem _vary_color, p/ ver se a cor lava). */
  /* DYSMANTLE_NOCOL: troca "_vary_color * diffuse_sample" por "diffuse_sample"
   * no shader básico — testa se a cor de vértice (luz assada) lava branco. */
  const char *repl = NULL;
  if (getenv("DYSMANTLE_NOCOL")) repl = "(vec4(1.0)) * diffuse_sample"; /* 28, branco */
  else if (getenv("DYSMANTLE_GREEN")) repl = "vec4(0,1,0,1)*diffuse_sample"; /* 28, verde */
  if (count == 1 && repl && str[0] &&
      strstr(str[0], "_vary_color * diffuse_sample")) {
    static char nb[16384];
    size_t L = (len && len[0] > 0) ? (size_t)len[0] : strlen(str[0]);
    if (L < sizeof(nb) - 4) {
      memcpy(nb, str[0], L); nb[L] = 0;
      char *p = strstr(nb, "_vary_color * diffuse_sample");
      if (p) {
        memmove(p, repl, 28);
        const char *pp = nb; int nl = (int)strlen(nb);
        if (real) { real(sh, 1, &pp, &nl); return; }
      }
    }
  }
  const char *inj = NULL;
  if (getenv("DYSMANTLE_SHADER_RED")) inj = "gl_FragColor=vec4(1.0,0.0,0.0,1.0);}";
  else if (getenv("DYSMANTLE_SHADER_TEX")) inj = "gl_FragColor=texture2D(_tex_diffuse,_vary_texture_coordinate);}";
  if (count == 1 && inj) {
    const char *s = str[0];
    if (s && strstr(s, "gl_FragColor") && strstr(s, "_tex_diffuse") &&
        strstr(s, "_vary_texture_coordinate")) {
      static char nb[16384];
      size_t L = (len && len[0] > 0) ? (size_t)len[0] : strlen(s);
      if (L < sizeof(nb) - 80) {
        memcpy(nb, s, L); nb[L] = 0;
        char *last = strrchr(nb, '}');
        if (last) {
          strcpy(last, inj);
          const char *p = nb; int nl = (int)strlen(nb);
          if (real) real(sh, 1, &p, &nl);
          return;
        }
      }
    }
  }
  if (real) real(sh, count, str, len);
}
static void my_glCompileShader(unsigned sh) {
  static void (*real)(unsigned) = NULL; rgl("glCompileShader", (void **)&real);
  static void (*giv)(unsigned, unsigned, int *) = NULL;
  rgl("glGetShaderiv", (void **)&giv);
  static void (*glog)(unsigned, int, int *, char *) = NULL;
  rgl("glGetShaderInfoLog", (void **)&glog);
  if (real) real(sh);
  if (giv && glog) {
    int ok = 1; giv(sh, 0x8B81 /*COMPILE_STATUS*/, &ok);
    if (!ok) {
      char buf[1024]; int n = 0; glog(sh, sizeof(buf) - 1, &n, buf);
      buf[n > 0 ? n : 0] = 0;
      fprintf(stderr, "[SHADER #%u COMPILE FALHOU] %s\n", sh, buf);
    }
  }
}
/* armazena source de cada shader (p/ dumpar o do program do chão) */
static struct { unsigned sh; char *src; } g_shsrc[256]; static int g_nsh = 0;
static void store_shader_src(unsigned sh, const char *s, int len) {
  if (g_nsh >= 256) return;
  int L = len > 0 ? len : (int)strlen(s);
  char *c = malloc(L + 1); if (!c) return;
  memcpy(c, s, L); c[L] = 0;
  g_shsrc[g_nsh].sh = sh; g_shsrc[g_nsh].src = c; g_nsh++;
}
static void my_glLinkProgram(unsigned pr) {
  static void (*real)(unsigned) = NULL; rgl("glLinkProgram", (void **)&real);
  /* dump da fragment source do programa alvo (DYSMANTLE_DUMP_PROG=N) */
  const char *want = getenv("DYSMANTLE_DUMP_PROG");
  if (want && (unsigned)atoi(want) == pr) {
    static void (*gas)(unsigned,int,int*,unsigned*) = NULL;
    rgl("glGetAttachedShaders", (void **)&gas);
    if (gas) {
      unsigned shs[8]; int cnt = 0; gas(pr, 8, &cnt, shs);
      for (int i = 0; i < cnt; i++)
        for (int j = 0; j < g_nsh; j++)
          if (g_shsrc[j].sh == shs[i] && strstr(g_shsrc[j].src, "gl_FragColor"))
            fprintf(stderr, "[PROGSRC prog=%u sh=%u]\n%s\n[/PROGSRC]\n", pr, shs[i], g_shsrc[j].src);
    }
  }
  static void (*giv)(unsigned, unsigned, int *) = NULL;
  rgl("glGetProgramiv", (void **)&giv);
  static void (*plog)(unsigned, int, int *, char *) = NULL;
  rgl("glGetProgramInfoLog", (void **)&plog);
  if (real) real(pr);
  if (giv && plog) {
    int ok = 1; giv(pr, 0x8B82 /*LINK_STATUS*/, &ok);
    if (!ok) {
      char buf[1024]; int n = 0; plog(pr, sizeof(buf) - 1, &n, buf);
      buf[n > 0 ? n : 0] = 0;
      fprintf(stderr, "[PROGRAM #%u LINK FALHOU] %s\n", pr, buf);
    }
  }
}

/* __stack_chk_fail neutralizado: a stack-canary da engine (bionic) é lida de
 * tpidr_el0+0x28, que sob glibc colide com TLS vars nossas/do libc++ -> a canary
 * "muda" no meio da função = FALSO-POSITIVO. Em vez de abortar, retornamos -> a
 * função segue o ret normal. (O guard do egl_shim já foi estabilizado tirando
 * _Thread_local; isto cobre os demais paths.) */
static void my_stack_chk_fail(void) {
  static int n = 0;
  if (n++ < 3) fprintf(stderr, "[stack_chk_fail] FALSO-POSITIVO TLS ignorado\n");
}

/* GUARDA no memcpy: o caminho skinned-actor (StageImporter::AddActorFromNode →
 * ModelInstance::InitializeFromModel) crasha com memcpy(dst=NULL, n=11) ao
 * importar o mapa (config change). Se dst/src inválidos, loga o caller e PULA
 * (em vez de SIGSEGV) → o loading do mundo continua. */
static int copy_bad(const char *who, void *dst, const void *src, size_t n) {
  if ((uintptr_t)dst >= 0x10000 && (uintptr_t)src >= 0x10000) return 0;
  static int g = 0;
  if (g < 30) { fprintf(stderr, "[%s-GUARD] dst=%p src=%p n=%zu — PULANDO\n", who, dst, src, n); g++; }
  return 1;
}
static void *my_memcpy(void *dst, const void *src, size_t n) {
  static void *(*real)(void *, const void *, size_t) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "memcpy");
  if (copy_bad("memcpy", dst, src, n)) return dst;
  return real(dst, src, n);
}
static void *my_memmove(void *dst, const void *src, size_t n) {
  static void *(*real)(void *, const void *, size_t) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "memmove");
  if (copy_bad("memmove", dst, src, n)) return dst;
  return real(dst, src, n);
}
static void *my_memcpy_chk(void *dst, const void *src, size_t n, size_t dl) {
  static void *(*real)(void *, const void *, size_t) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "memcpy");
  (void)dl; if (copy_bad("memcpy_chk", dst, src, n)) return dst;
  return real(dst, src, n);
}
static void *my_memmove_chk(void *dst, const void *src, size_t n, size_t dl) {
  static void *(*real)(void *, const void *, size_t) = NULL;
  if (!real) real = dlsym(RTLD_DEFAULT, "memmove");
  (void)dl; if (copy_bad("memmove_chk", dst, src, n)) return dst;
  return real(dst, src, n);
}

DynLibFunction dysmantle_overrides[] = {
  /* liblog */
  {"__android_log_print", (uintptr_t)b_log_print},
  {"__android_log_write", (uintptr_t)b_log_write},
  {"__android_log_assert", (uintptr_t)b_log_assert},
  /* bionic stdio __sF + wrappers (resolve UNRESOLVED do libc++ -> std::cerr) */
  {"__sF", (uintptr_t)bionic_sF},
  {"android_set_abort_message", (uintptr_t)b_set_abort_message},
  {"fprintf", (uintptr_t)w_fprintf}, {"vfprintf", (uintptr_t)w_vfprintf},
  {"fwrite", (uintptr_t)w_fwrite}, {"fputs", (uintptr_t)w_fputs},
  {"fputc", (uintptr_t)w_fputc}, {"fflush", (uintptr_t)w_fflush},
  /* bionic libc */
  {"__errno", (uintptr_t)b_errno},
  {"setjmp", (uintptr_t)_setjmp},
  {"longjmp", (uintptr_t)_longjmp},
  {"__assert2", (uintptr_t)b_assert2},
  {"__strlen_chk", (uintptr_t)b_strlen_chk},
  {"__strchr_chk", (uintptr_t)b_strchr_chk},
  {"__umask_chk", (uintptr_t)b_umask_chk},
  {"__system_property_get", (uintptr_t)b_sys_prop_get},
  {"__emutls_get_address", (uintptr_t)__emutls_get_address},
  /* EGL -> egl_shim */
  {"eglGetDisplay", (uintptr_t)egl_shim_GetDisplay},
  {"eglInitialize", (uintptr_t)egl_shim_Initialize},
  {"eglTerminate", (uintptr_t)egl_shim_Terminate},
  {"eglChooseConfig", (uintptr_t)egl_shim_ChooseConfig},
  {"eglCreateWindowSurface", (uintptr_t)egl_shim_CreateWindowSurface},
  {"eglCreateContext", (uintptr_t)egl_shim_CreateContext},
  {"eglDestroyContext", (uintptr_t)egl_shim_DestroyContext},
  {"eglDestroySurface", (uintptr_t)egl_shim_DestroySurface},
  {"eglGetConfigAttrib", (uintptr_t)egl_shim_GetConfigAttrib},
  {"eglGetError", (uintptr_t)egl_shim_GetError},
  {"eglGetProcAddress", (uintptr_t)egl_shim_GetProcAddress},
  {"eglMakeCurrent", (uintptr_t)egl_shim_MakeCurrent},
  {"eglSwapBuffers", (uintptr_t)egl_shim_SwapBuffers},
  {"eglSwapInterval", (uintptr_t)egl_shim_SwapInterval},
  {"eglBindAPI", (uintptr_t)egl_shim_BindAPI},
  {"eglReleaseThread", (uintptr_t)egl_releasethread_stub},
  /* GL string override (anti stack-smash do Utgard) */
  {"glGetString", (uintptr_t)my_glGetString},
  /* intercepta dlsym (a engine resolve GL por aqui) */
  {"dlsym", (uintptr_t)my_dlsym},
  {"dlopen", (uintptr_t)my_dlopen},
  {"dlclose", (uintptr_t)my_dlclose},
  /* GL wrappers com log p/ pinpoint do smash */
  {"glGetIntegerv", (uintptr_t)my_glGetIntegerv},
  {"glGetStringi", (uintptr_t)my_glGetStringi},
  {"glGetError", (uintptr_t)my_glGetError},
  /* GL core interceptados p/ diag/fix do mundo branco (resolvidos pela tabela,
   * NÃO via eglGetProcAddress) */
  {"glTexImage2D", (uintptr_t)my_glTexImage2D},
  {"glShaderSource", (uintptr_t)my_glShaderSource},
  {"glCompileShader", (uintptr_t)my_glCompileShader},
  {"glLinkProgram", (uintptr_t)my_glLinkProgram},
  {"glCompressedTexImage2D", (uintptr_t)my_glCompressedTexImage2D},
  {"glTexParameteri", (uintptr_t)my_glTexParameteri},
  {"glClearColor", (uintptr_t)my_glClearColor},
  {"glBlendFunc", (uintptr_t)my_glBlendFunc},
  {"glBlendFuncSeparate", (uintptr_t)my_glBlendFuncSeparate},
  {"glBindBuffer", (uintptr_t)my_glBindBuffer},
  {"glUseProgram", (uintptr_t)my_glUseProgram},
  {"glBufferData", (uintptr_t)my_glBufferData},
  {"glEnable", (uintptr_t)my_glEnable},
  {"glDepthFunc", (uintptr_t)my_glDepthFunc},
  {"glGetUniformLocation", (uintptr_t)my_glGetUniformLocation},
  {"glUniform1i", (uintptr_t)my_glUniform1i},
  {"glBindAttribLocation", (uintptr_t)my_glBindAttribLocation},
  {"glBindTexture", (uintptr_t)my_glBindTexture},
  {"glActiveTexture", (uintptr_t)my_glActiveTexture},
  {"glGetAttribLocation", (uintptr_t)my_glGetAttribLocation},
  {"glVertexAttribPointer", (uintptr_t)my_glVertexAttribPointer},
  {"glDrawArrays", (uintptr_t)my_glDrawArrays},
  {"glDrawElements", (uintptr_t)my_glDrawElements},
  {"glBindFramebuffer", (uintptr_t)my_glBindFramebuffer},
  {"glCheckFramebufferStatus", (uintptr_t)my_glCheckFramebufferStatus},
  {"glRenderbufferStorage", (uintptr_t)my_glRenderbufferStorage},
  {"glFramebufferTexture2D", (uintptr_t)my_glFramebufferTexture2D},
  {"pthread_attr_setstacksize", (uintptr_t)my_attr_setstacksize},
  {"fopen", (uintptr_t)my_fopen},
  {"__stack_chk_fail", (uintptr_t)my_stack_chk_fail},
  {"memcpy", (uintptr_t)my_memcpy},
  {"memmove", (uintptr_t)my_memmove},
  {"__memcpy_chk", (uintptr_t)my_memcpy_chk},
  {"__memmove_chk", (uintptr_t)my_memmove_chk},
  /* ANativeWindow */
  {"ANativeWindow_fromSurface", (uintptr_t)aw_fromSurface},
  {"ANativeWindow_acquire", (uintptr_t)aw_acquire},
  {"ANativeWindow_release", (uintptr_t)aw_release},
  {"ANativeWindow_getWidth", (uintptr_t)aw_getWidth},
  {"ANativeWindow_getHeight", (uintptr_t)aw_getHeight},
  /* AAsset */
  {"AAssetManager_fromJava", (uintptr_t)aam_fromJava},
  {"AAssetManager_open", (uintptr_t)aam_open},
  {"AAssetManager_openDir", (uintptr_t)aam_openDir},
  {"AAsset_read", (uintptr_t)aa_read},
  {"AAsset_seek", (uintptr_t)aa_seek},
  {"AAsset_seek64", (uintptr_t)aa_seek64},
  {"AAsset_getLength", (uintptr_t)aa_getLength},
  {"AAsset_getLength64", (uintptr_t)aa_getLength},
  {"AAsset_getRemainingLength", (uintptr_t)aa_getRemaining},
  {"AAsset_getRemainingLength64", (uintptr_t)aa_getRemaining},
  {"AAsset_close", (uintptr_t)aa_close},
  {"AAsset_openFileDescriptor", (uintptr_t)aa_openFd},
  {"AAssetDir_getNextFileName", (uintptr_t)aad_getNext},
  {"AAssetDir_close", (uintptr_t)aad_close},
  /* ALooper extras */
  {"ALooper_pollOnce", (uintptr_t)al_pollOnce},
  {"ALooper_forThread", (uintptr_t)al_forThread},
  {"ALooper_acquire", (uintptr_t)al_acquire},
  {"ALooper_release", (uintptr_t)al_release},
  {"ALooper_removeFd", (uintptr_t)al_removeFd},
  {"ALooper_wake", (uintptr_t)al_wake},
  /* AInput extras */
  {"AInputEvent_getDeviceId", (uintptr_t)aie_getDeviceId},
  {"AMotionEvent_getButtonState", (uintptr_t)ame_getButtonState},
  /* OpenSL */
  {"slCreateEngine", (uintptr_t)slCreateEngine_shim},
  {"SL_IID_ENGINE", (uintptr_t)&SL_IID_ENGINE_v},
  {"SL_IID_PLAY", (uintptr_t)&SL_IID_PLAY_v},
  {"SL_IID_RECORD", (uintptr_t)&SL_IID_RECORD_v},
  {"SL_IID_BUFFERQUEUE", (uintptr_t)&SL_IID_BUFFERQUEUE_v},
  {"SL_IID_ANDROIDSIMPLEBUFFERQUEUE", (uintptr_t)&SL_IID_ANDROIDSIMPLEBUFFERQUEUE_v},
  {"SL_IID_ANDROIDCONFIGURATION", (uintptr_t)&SL_IID_ANDROIDCONFIGURATION_v},
};
const int dysmantle_overrides_count =
    sizeof(dysmantle_overrides) / sizeof(dysmantle_overrides[0]);
