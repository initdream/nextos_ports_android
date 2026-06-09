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
static void b_log_assert(const char *cond, const char *tag, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  fprintf(stderr, "[ALOG-ASSERT %s] cond=%s ", tag ? tag : "?", cond ? cond : "?");
  if (fmt) vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n"); va_end(ap);
  abort();
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
  (void)name; if (value) value[0] = '\0'; return 0;
}
/* __emutls_get_address vem do libgcc (linkado estático no loader). */
extern void *__emutls_get_address(void *);

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
typedef struct { FILE *fp; long len; } DysAsset;
typedef struct { DIR *d; } DysAssetDir;
#include <dirent.h>

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
  return a;
}
static int    aa_read(void *h, void *buf, size_t n) { DysAsset *a = h; return a ? (int)fread(buf, 1, n, a->fp) : -1; }
static long   aa_seek(void *h, long off, int wh)     { DysAsset *a = h; if (!a) return -1; fseek(a->fp, off, wh); return ftell(a->fp); }
static long   aa_seek64(void *h, long off, int wh)   { return aa_seek(h, off, wh); }
static long   aa_getLength(void *h)   { DysAsset *a = h; return a ? a->len : 0; }
static long   aa_getRemaining(void *h){ DysAsset *a = h; return a ? a->len - ftell(a->fp) : 0; }
static void   aa_close(void *h)       { DysAsset *a = h; if (a) { fclose(a->fp); free(a); } }
static int    aa_openFd(void *h, long *start, long *len) {
  DysAsset *a = h; if (!a) return -1;
  if (start) *start = 0; if (len) *len = a->len;
  return fileno(a->fp);
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

/* ---------------- OpenSL ES interface IDs (dados dummy) ---------------- */
extern uint32_t slCreateEngine_shim(void **, uint32_t, const void *, uint32_t,
                                    const void *, const void *);
static const void *SL_IID_ENGINE_v = "ENGINE";
static const void *SL_IID_PLAY_v = "PLAY";
static const void *SL_IID_RECORD_v = "RECORD";
static const void *SL_IID_BUFFERQUEUE_v = "BUFQ";
static const void *SL_IID_ANDROIDSIMPLEBUFFERQUEUE_v = "ASBQ";
static const void *SL_IID_ANDROIDCONFIGURATION_v = "ACFG";

/* EGL shim funcs já declaradas em egl_shim.h (assinaturas reais) */
static unsigned egl_releasethread_stub(void) { return 1u; }

DynLibFunction dysmantle_overrides[] = {
  /* liblog */
  {"__android_log_print", (uintptr_t)b_log_print},
  {"__android_log_write", (uintptr_t)b_log_write},
  {"__android_log_assert", (uintptr_t)b_log_assert},
  /* bionic libc */
  {"__errno", (uintptr_t)b_errno},
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
