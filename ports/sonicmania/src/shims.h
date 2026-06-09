#ifndef SONIC_SHIMS_H
#define SONIC_SHIMS_H
#include <pthread.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
int pthread_create_fake(pthread_t *, const void *, void *(*)(void *), void *);
int pthread_rwlock_rdlock_fake(pthread_rwlock_t *);
int pthread_rwlock_wrlock_fake(pthread_rwlock_t *);
int pthread_rwlock_unlock_fake(pthread_rwlock_t *);
int pthread_setspecific_fake(pthread_key_t, const void *);
void *_Znwm(unsigned long);
void *_Znam(unsigned long);
void _ZdlPv(void *);
void _ZdaPv(void *);
int __cxa_atexit(void (*)(void *), void *, void *);
void __cxa_finalize(void *);
int __cxa_guard_acquire(uint64_t *);
void __cxa_guard_release(uint64_t *);
void __cxa_guard_abort(uint64_t *);
void *__cxa_allocate_exception(unsigned long);
void __cxa_free_exception(void *);
void __cxa_throw(void *, void *, void *);
void *__cxa_begin_catch(void *);
void __cxa_end_catch(void);
void __cxa_pure_virtual(void);
int __gxx_personality_v0(int, int, unsigned long, void *, void *);
void __stack_chk_fail(void);
int __android_log_print(int, const char *, const char *, ...);
extern void *SL_IID_ENGINE_shim, *SL_IID_PLAY_shim, *SL_IID_RECORD_shim,
    *SL_IID_BUFFERQUEUE_shim, *SL_IID_ANDROIDSIMPLEBUFFERQUEUE_shim,
    *SL_IID_ANDROIDCONFIGURATION_shim;
void *AAssetManager_fromJava(void *, void *);
void *AAssetManager_open(void *, const char *, int);
long AAsset_getLength(void *);
long AAsset_getLength64(void *);
int AAsset_read(void *, void *, size_t);
long AAsset_seek(void *, long, int);
const void *AAsset_getBuffer(void *);
void AAsset_close(void *);
int __vsprintf_chk(char *, int, size_t, const char *, va_list);
int __vsnprintf_chk(char *, size_t, int, size_t, const char *, va_list);
char *__strcpy_chk(char *, const char *, size_t);
char *__strcat_chk(char *, const char *, size_t);
char *__strncpy_chk(char *, const char *, size_t, size_t);
void *__memcpy_chk(void *, const void *, size_t, size_t);
void *__memset_chk(void *, int, size_t, size_t);
void *__memmove_chk(void *, const void *, size_t, size_t);
int my_register_atfork(void (*)(void), void (*)(void), void (*)(void), void *);
void my_glDrawArrays(); void my_glDrawElements(); void my_glBindFramebuffer(); void my_glViewport(); unsigned my_glCreateProgram(); void my_glLinkProgram(); void my_glCompileShader(); void my_glClear();
int *__errno(void); size_t __strlen_chk(const char *, size_t); int __read_chk(int, void *, size_t, size_t);
int __system_property_get(const char *, char *); void android_set_abort_message(const char *);
int AInputEvent_getDeviceId(void *); int AMotionEvent_getButtonState(void *); extern void *__sF;
void my_glGenTextures(); void my_glBindTexture(); void my_glTexImage2D(); void my_glGenFramebuffers(); void my_glGenBuffers();
unsigned my_glCreateShader(); void my_glActiveTexture(); void my_glBufferData();
void my_glTexSubImage2D();
#endif
