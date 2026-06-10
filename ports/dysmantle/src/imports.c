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
#define DYS_W 1280
#define DYS_H 720
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
void *dysmantle_gl_proc_override(const char *name) {
  if (name && strcmp(name, "glGetString") == 0) return (void *)my_glGetString;
  return NULL;
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

/* __stack_chk_fail neutralizado: a stack-canary da engine (bionic) é lida de
 * tpidr_el0+0x28, que sob glibc colide com TLS vars nossas/do libc++ -> a canary
 * "muda" no meio da função = FALSO-POSITIVO. Em vez de abortar, retornamos -> a
 * função segue o ret normal. (O guard do egl_shim já foi estabilizado tirando
 * _Thread_local; isto cobre os demais paths.) */
static void my_stack_chk_fail(void) {
  static int n = 0;
  if (n++ < 3) fprintf(stderr, "[stack_chk_fail] FALSO-POSITIVO TLS ignorado\n");
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
  {"pthread_attr_setstacksize", (uintptr_t)my_attr_setstacksize},
  {"fopen", (uintptr_t)my_fopen},
  {"__stack_chk_fail", (uintptr_t)my_stack_chk_fail},
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
