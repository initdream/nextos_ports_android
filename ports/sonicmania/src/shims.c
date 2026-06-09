/* shims.c — shims do so-loader Sonic Mania (RSDKv5) p/ Mali-450.
 * Resolve os simbolos que o libsonicmania.so importa do bionic/NDK/cxx que a
 * glibc nao tem com o mesmo nome. Os pthread (create/rwlock/setspecific) sao
 * tipos COMPATIVEIS LP64 (so mutex diverge, e o RSDK nao importa mutex) ->
 * passthrough direto. cxx new/delete -> malloc/free. OpenSLES -> stub (audio
 * sai por SDL depois). __android_log_print -> stderr. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "shims.h"

/* ---- pthread bionic->glibc (tipos compativeis: passthrough) ---- */
int pthread_create_fake(pthread_t *t, const void *attr, void *(*f)(void *), void *arg) {
  (void)attr;
  return pthread_create(t, NULL, f, arg);
}
int pthread_rwlock_rdlock_fake(pthread_rwlock_t *l) { return pthread_rwlock_rdlock(l); }
int pthread_rwlock_wrlock_fake(pthread_rwlock_t *l) { return pthread_rwlock_wrlock(l); }
int pthread_rwlock_unlock_fake(pthread_rwlock_t *l) { return pthread_rwlock_unlock(l); }
int pthread_setspecific_fake(pthread_key_t k, const void *v) { return pthread_setspecific(k, v); }

/* ---- C++ runtime / operators ---- */
void *_Znwm(unsigned long n) { return malloc(n ? n : 1); }     /* operator new */
void *_Znam(unsigned long n) { return malloc(n ? n : 1); }     /* operator new[] */
void _ZdlPv(void *p) { free(p); }                              /* operator delete */
void _ZdaPv(void *p) { free(p); }                              /* operator delete[] */

int __cxa_atexit(void (*f)(void *), void *arg, void *dso) { (void)f; (void)arg; (void)dso; return 0; }
void __cxa_finalize(void *dso) { (void)dso; }
int __cxa_guard_acquire(uint64_t *g) { return !*(volatile uint8_t *)g; }
void __cxa_guard_release(uint64_t *g) { *(volatile uint8_t *)g = 1; }
void __cxa_guard_abort(uint64_t *g) { (void)g; }
void *__cxa_allocate_exception(unsigned long n) { return malloc(n ? n : 1); }
void __cxa_free_exception(void *p) { free(p); }
void __cxa_throw(void *e, void *t, void *d) { (void)e; (void)t; (void)d; fprintf(stderr, "[shim] __cxa_throw -> abort\n"); abort(); }
void *__cxa_begin_catch(void *e) { return e; }
void __cxa_end_catch(void) {}
void __cxa_pure_virtual(void) { fprintf(stderr, "[shim] pure_virtual -> abort\n"); abort(); }
int __gxx_personality_v0(int a, int b, unsigned long c, void *d, void *e) { (void)a; (void)b; (void)c; (void)d; (void)e; return 0; }

void __stack_chk_fail(void) { fprintf(stderr, "[shim] stack_chk_fail\n"); abort(); }

/* ---- liblog ---- */
int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  return 0;
}

/* ---- OpenSLES interface IDs (stub NULL; audio sai por SDL depois) ---- */
void *SL_IID_ENGINE_shim = 0;
void *SL_IID_PLAY_shim = 0;
void *SL_IID_RECORD_shim = 0;
void *SL_IID_BUFFERQUEUE_shim = 0;
void *SL_IID_ANDROIDSIMPLEBUFFERQUEUE_shim = 0;
void *SL_IID_ANDROIDCONFIGURATION_shim = 0;

/* ---- AAssetManager -> abre Data.rsdk e outros via fopen do cwd ---- */
#include <string.h>
void *AAssetManager_fromJava(void *env, void *mgr) { (void)env; (void)mgr; return (void *)0x1; }
void *AAssetManager_open(void *mgr, const char *name, int mode) {
  (void)mgr; (void)mode;
  FILE *f = fopen(name, "rb");
  fprintf(stderr, "[asset] open %s -> %p\n", name ? name : "?", (void *)f);
  return f;
}
long AAsset_getLength(void *a) { FILE *f = a; long c = ftell(f); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, c, SEEK_SET); return n; }
long AAsset_getLength64(void *a) { return AAsset_getLength(a); }
int AAsset_read(void *a, void *buf, size_t n) { size_t r=fread(buf,1,n,(FILE*)a); static int c=0; if(c++<3) fprintf(stderr,"[asset] read %zu->%zu\n",n,r); return (int)r; }
long AAsset_seek(void *a, long off, int whence) { fseek((FILE *)a, off, whence); return ftell((FILE *)a); }
const void *AAsset_getBuffer(void *a) {
  FILE *f = a; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  void *buf = malloc(n); if (buf) { if(fread(buf, 1, n, f)!=(size_t)n){} }
  fprintf(stderr, "[asset] getBuffer %ld bytes -> %p\n", n, buf);
  return buf;
}
void AAsset_close(void *a) { if (a) fclose((FILE *)a); }
/* ---- bionic _chk / misc ---- */
int __vsprintf_chk(char *s, int flag, size_t slen, const char *fmt, va_list ap) { (void)flag; (void)slen; return vsprintf(s, fmt, ap); }
int __vsnprintf_chk(char *s, size_t n, int flag, size_t slen, const char *fmt, va_list ap) { (void)flag; (void)slen; return vsnprintf(s, n, fmt, ap); }
char *__strcpy_chk(char *d, const char *s, size_t dl) { (void)dl; return strcpy(d, s); }
char *__strcat_chk(char *d, const char *s, size_t dl) { (void)dl; return strcat(d, s); }
char *__strncpy_chk(char *d, const char *s, size_t n, size_t dl) { (void)dl; return strncpy(d, s, n); }
void *__memcpy_chk(void *d, const void *s, size_t n, size_t dl) { (void)dl; return memcpy(d, s, n); }
void *__memset_chk(void *d, int c, size_t n, size_t dl) { (void)dl; return memset(d, c, n); }
void *__memmove_chk(void *d, const void *s, size_t n, size_t dl) { (void)dl; return memmove(d, s, n); }
int my_register_atfork(void (*a)(void), void (*b)(void), void (*c)(void), void *d) { (void)a; (void)b; (void)c; (void)d; return 0; }

/* ---- 15 unresolved restantes (libc bionic + input) ---- */
extern int *__errno_location(void);
int *__errno(void) { return __errno_location(); }
size_t __strlen_chk(const char *s, size_t n) { (void)n; return strlen(s); }
int __read_chk(int fd, void *b, size_t n, size_t bl) { (void)bl; extern long read(int,void*,size_t); return (int)read(fd,b,n); }
int __system_property_get(const char *k, char *v) { (void)k; if (v) v[0] = 0; return 0; }
void android_set_abort_message(const char *m) { (void)m; }
char g_sF_stub[3][256];
void *__sF = g_sF_stub;
int AInputEvent_getDeviceId(void *e) { (void)e; return 0; }
int AMotionEvent_getButtonState(void *e) { (void)e; return 0; }
