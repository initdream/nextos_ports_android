#define _GNU_SOURCE
#include <GLES2/gl2.h>
#include <EGL/egl.h>
extern int __cxa_atexit(void(*)(void*),void*,void*);
extern void __cxa_finalize(void*);
extern int __cxa_thread_atexit_impl(void(*)(void*),void*,void*);

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <time.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <iconv.h>
#include <sys/ioctl.h>
#include <malloc.h>
static int *bionic_errno(void){return __errno_location();}
#include <locale.h>
#include <signal.h>
#include <setjmp.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netdb.h>
#include <pthread.h>
#include <zlib.h>
/* liblog (Android) não existe no glibc -> stubs p/ stderr */
static int __android_log_print(int p,const char*t,const char*f,...){(void)p;(void)t;(void)f;return 0;}
static int __android_log_write(int p,const char*t,const char*m){(void)p;fprintf(stderr,"[ALOG:%s] %s\n",t?t:"",m?m:"");return 0;}
static void android_set_abort_message(const char*m){fprintf(stderr,"[abort] %s\n",m?m:"");}
// imports.gen.c — GERADO por new-port.sh para 'dusklight' (libmain.so)
// 525 simbolos. Resolva os UNKNOWN no fim do arquivo.
#include "imports.h"
#include "so_util.h"
#include "opensles_shim.h"
extern void __stack_chk_fail(void);
/* SL_IID_*_shim: interface IDs (objetos) do opensles_shim */
extern const void *SL_IID_ENGINE_shim, *SL_IID_PLAY_shim, *SL_IID_RECORD_shim,
  *SL_IID_VOLUME_shim, *SL_IID_ANDROIDSIMPLEBUFFERQUEUE_shim;

#include <stdio.h>

// === passthrough/pthread/shim: ligados automaticamente ===

/* extern decls dos _fake (def em pthread_fake.c) */
extern long pthread_attr_destroy_fake();
extern long pthread_attr_init_fake();
extern long pthread_attr_setdetachstate_fake();
extern long pthread_attr_setstacksize_fake();
extern long pthread_cond_broadcast_fake();
extern long pthread_cond_destroy_fake();
extern long pthread_cond_init_fake();
extern long pthread_cond_signal_fake();
extern long pthread_cond_timedwait_fake();
extern long pthread_cond_wait_fake();
extern long pthread_create_fake();
extern long pthread_detach_fake();
extern long pthread_getschedparam_fake();
extern long pthread_getspecific_fake();
extern long pthread_join_fake();
extern long pthread_key_create_fake();
extern long pthread_key_delete_fake();
extern long pthread_mutexattr_destroy_fake();
extern long pthread_mutexattr_init_fake();
extern long pthread_mutexattr_settype_fake();
extern long pthread_mutex_destroy_fake();
extern long pthread_mutex_init_fake();
extern long pthread_mutex_lock_fake();
extern long pthread_mutex_trylock_fake();
extern long pthread_mutex_unlock_fake();
extern long pthread_once_fake();
extern long pthread_rwlock_destroy_fake();
extern long pthread_rwlock_init_fake();
extern long pthread_rwlock_rdlock_fake();
extern long pthread_rwlock_tryrdlock_fake();
extern long pthread_rwlock_trywrlock_fake();
extern long pthread_rwlock_unlock_fake();
extern long pthread_rwlock_wrlock_fake();
extern long pthread_self_fake();
extern long pthread_setschedparam_fake();
extern long pthread_setspecific_fake();
extern long pthread_sigmask_fake();
extern long sem_destroy_fake();
extern long sem_getvalue_fake();
extern long sem_init_fake();
extern long sem_post_fake();
extern long sem_timedwait_fake();
extern long sem_trywait_fake();
extern long sem_wait_fake();

DynLibFunction dynlib_functions[] = {
  {"abort", (uintptr_t)&abort},  // pass
  {"access", (uintptr_t)&access},  // pass
  {"acos", (uintptr_t)&acos},  // pass
  // TODO {"acosf", (uintptr_t)&stub_acosf},  // <<< IMPLEMENTAR
  // TODO {"acosh", (uintptr_t)&stub_acosh},  // <<< IMPLEMENTAR
  // TODO {"acoshf", (uintptr_t)&stub_acoshf},  // <<< IMPLEMENTAR
  {"aligned_alloc", (uintptr_t)&aligned_alloc},  // pass
  {"__android_log_print", (uintptr_t)&__android_log_print},  // liblog
  {"__android_log_write", (uintptr_t)&__android_log_write},  // liblog
  // TODO {"android_set_abort_message", (uintptr_t)&stub_android_set_abort_message},  // <<< IMPLEMENTAR
  {"asin", (uintptr_t)&asin},  // pass
  // TODO {"asinf", (uintptr_t)&stub_asinf},  // <<< IMPLEMENTAR
  // TODO {"asinh", (uintptr_t)&stub_asinh},  // <<< IMPLEMENTAR
  // TODO {"asinhf", (uintptr_t)&stub_asinhf},  // <<< IMPLEMENTAR
  {"atan", (uintptr_t)&atan},  // pass
  {"atan2", (uintptr_t)&atan2},  // pass
  {"atan2f", (uintptr_t)&atan2f},  // pass
  // TODO {"atanf", (uintptr_t)&stub_atanf},  // <<< IMPLEMENTAR
  // TODO {"atanh", (uintptr_t)&stub_atanh},  // <<< IMPLEMENTAR
  // TODO {"atanhf", (uintptr_t)&stub_atanhf},  // <<< IMPLEMENTAR
  {"atof", (uintptr_t)&atof},  // pass
  {"atoi", (uintptr_t)&atoi},  // pass
  // TODO {"bind", (uintptr_t)&stub_bind},  // <<< IMPLEMENTAR
  // TODO {"btowc", (uintptr_t)&stub_btowc},  // <<< IMPLEMENTAR
  {"calloc", (uintptr_t)&calloc},  // pass
  {"chdir", (uintptr_t)&chdir},  // pass
  // TODO {"chmod", (uintptr_t)&stub_chmod},  // <<< IMPLEMENTAR
  // TODO {"chown", (uintptr_t)&stub_chown},  // <<< IMPLEMENTAR
  // TODO {"chroot", (uintptr_t)&stub_chroot},  // <<< IMPLEMENTAR
  // TODO {"clearerr", (uintptr_t)&stub_clearerr},  // <<< IMPLEMENTAR
  {"clock_gettime", (uintptr_t)&clock_gettime},  // pass
  // TODO {"clock_nanosleep", (uintptr_t)&stub_clock_nanosleep},  // <<< IMPLEMENTAR
  {"close", (uintptr_t)&close},  // pass
  {"closedir", (uintptr_t)&closedir},  // pass
  // TODO {"closelog", (uintptr_t)&stub_closelog},  // <<< IMPLEMENTAR
  // [zlib runtime] {"compress2", (uintptr_t)&compress2},  // pass
  // TODO {"connect", (uintptr_t)&stub_connect},  // <<< IMPLEMENTAR
  // TODO {"copy_file_range", (uintptr_t)&stub_copy_file_range},  // <<< IMPLEMENTAR
  {"cos", (uintptr_t)&cos},  // pass
  {"cosf", (uintptr_t)&cosf},  // pass
  // TODO {"cosh", (uintptr_t)&stub_cosh},  // <<< IMPLEMENTAR
  // TODO {"coshf", (uintptr_t)&stub_coshf},  // <<< IMPLEMENTAR
  // [zlib runtime] {"crc32", (uintptr_t)&crc32},  // pass
  // TODO {"__ctype_get_mb_cur_max", (uintptr_t)&stub___ctype_get_mb_cur_max},  // <<< IMPLEMENTAR
  {"__cxa_atexit", (uintptr_t)&__cxa_atexit},  // cxx
  {"__cxa_finalize", (uintptr_t)&__cxa_finalize},  // cxx
  {"__cxa_thread_atexit_impl", (uintptr_t)&__cxa_thread_atexit_impl},  // cxx
  // [zlib runtime] {"deflate", (uintptr_t)&deflate},  // pass
  // [zlib runtime] {"deflateEnd", (uintptr_t)&deflateEnd},  // pass
  // [zlib runtime] {"deflateInit2_", (uintptr_t)&deflateInit2_},  // pass
  // [zlib runtime] // TODO {"deflateReset", (uintptr_t)&stub_deflateReset},  // <<< IMPLEMENTAR
  // TODO {"dirfd", (uintptr_t)&stub_dirfd},  // <<< IMPLEMENTAR
  // TODO {"div", (uintptr_t)&stub_div},  // <<< IMPLEMENTAR
  // TODO {"dladdr", (uintptr_t)&stub_dladdr},  // <<< IMPLEMENTAR
  // TODO {"dlclose", (uintptr_t)&stub_dlclose},  // <<< IMPLEMENTAR
  // TODO {"dlerror", (uintptr_t)&stub_dlerror},  // <<< IMPLEMENTAR
  // TODO {"dl_iterate_phdr", (uintptr_t)&stub_dl_iterate_phdr},  // <<< IMPLEMENTAR
  // TODO {"dlopen", (uintptr_t)&stub_dlopen},  // <<< IMPLEMENTAR
  // TODO {"dlsym", (uintptr_t)&stub_dlsym},  // <<< IMPLEMENTAR
  {"dup", (uintptr_t)&dup},  // pass
  {"dup2", (uintptr_t)&dup2},  // pass
  // TODO {"environ", (uintptr_t)&stub_environ},  // <<< IMPLEMENTAR
  {"__errno", (uintptr_t)&bionic_errno},  // pass
  // TODO {"execvp", (uintptr_t)&stub_execvp},  // <<< IMPLEMENTAR
  // TODO {"_exit", (uintptr_t)&stub__exit},  // <<< IMPLEMENTAR
  {"exit", (uintptr_t)&exit},  // pass
  // TODO {"_Exit", (uintptr_t)&stub__Exit},  // <<< IMPLEMENTAR
  {"exp", (uintptr_t)&exp},  // pass
  {"exp2", (uintptr_t)&exp2},  // pass
  // TODO {"exp2f", (uintptr_t)&stub_exp2f},  // <<< IMPLEMENTAR
  {"expf", (uintptr_t)&expf},  // pass
  // TODO {"fchmod", (uintptr_t)&stub_fchmod},  // <<< IMPLEMENTAR
  // TODO {"fchmodat", (uintptr_t)&stub_fchmodat},  // <<< IMPLEMENTAR
  // TODO {"fchown", (uintptr_t)&stub_fchown},  // <<< IMPLEMENTAR
  {"fclose", (uintptr_t)&fclose},  // pass
  {"fcntl", (uintptr_t)&fcntl},  // pass
  // TODO {"fdatasync", (uintptr_t)&stub_fdatasync},  // <<< IMPLEMENTAR
  // TODO {"__FD_ISSET_chk", (uintptr_t)&stub___FD_ISSET_chk},  // <<< IMPLEMENTAR
  {"fdopen", (uintptr_t)&fdopen},  // pass
  // TODO {"fdopendir", (uintptr_t)&stub_fdopendir},  // <<< IMPLEMENTAR
  // TODO {"__FD_SET_chk", (uintptr_t)&stub___FD_SET_chk},  // <<< IMPLEMENTAR
  // TODO {"feholdexcept", (uintptr_t)&stub_feholdexcept},  // <<< IMPLEMENTAR
  // TODO {"feof", (uintptr_t)&stub_feof},  // <<< IMPLEMENTAR
  // TODO {"ferror", (uintptr_t)&stub_ferror},  // <<< IMPLEMENTAR
  // TODO {"fesetenv", (uintptr_t)&stub_fesetenv},  // <<< IMPLEMENTAR
  {"fflush", (uintptr_t)&fflush},  // pass
  {"fgets", (uintptr_t)&fgets},  // pass
  {"fileno", (uintptr_t)&fileno},  // pass
  {"fopen", (uintptr_t)&fopen},  // pass
  // TODO {"fopen64", (uintptr_t)&stub_fopen64},  // <<< IMPLEMENTAR
  // TODO {"fork", (uintptr_t)&stub_fork},  // <<< IMPLEMENTAR
  {"fprintf", (uintptr_t)&fprintf},  // pass
  {"fputc", (uintptr_t)&fputc},  // pass
  // TODO {"fputwc", (uintptr_t)&stub_fputwc},  // <<< IMPLEMENTAR
  {"fread", (uintptr_t)&fread},  // pass
  // TODO {"__fread_chk", (uintptr_t)&stub___fread_chk},  // <<< IMPLEMENTAR
  {"free", (uintptr_t)&free},  // pass
  // TODO {"freeaddrinfo", (uintptr_t)&stub_freeaddrinfo},  // <<< IMPLEMENTAR
  // TODO {"freelocale", (uintptr_t)&stub_freelocale},  // <<< IMPLEMENTAR
  {"frexp", (uintptr_t)&frexp},  // pass
  // TODO {"frexpl", (uintptr_t)&stub_frexpl},  // <<< IMPLEMENTAR
  {"fseek", (uintptr_t)&fseek},  // pass
  // TODO {"fseeko", (uintptr_t)&stub_fseeko},  // <<< IMPLEMENTAR
  // TODO {"fseeko64", (uintptr_t)&stub_fseeko64},  // <<< IMPLEMENTAR
  {"fstat", (uintptr_t)&fstat},  // pass
  // TODO {"fstatat", (uintptr_t)&stub_fstatat},  // <<< IMPLEMENTAR
  // TODO {"fsync", (uintptr_t)&stub_fsync},  // <<< IMPLEMENTAR
  {"ftell", (uintptr_t)&ftell},  // pass
  // TODO {"ftello", (uintptr_t)&stub_ftello},  // <<< IMPLEMENTAR
  // TODO {"ftello64", (uintptr_t)&stub_ftello64},  // <<< IMPLEMENTAR
  // TODO {"ftruncate", (uintptr_t)&stub_ftruncate},  // <<< IMPLEMENTAR
  // TODO {"ftruncate64", (uintptr_t)&stub_ftruncate64},  // <<< IMPLEMENTAR
  // TODO {"futimens", (uintptr_t)&stub_futimens},  // <<< IMPLEMENTAR
  {"fwrite", (uintptr_t)&fwrite},  // pass
  // TODO {"gai_strerror", (uintptr_t)&stub_gai_strerror},  // <<< IMPLEMENTAR
  // TODO {"getaddrinfo", (uintptr_t)&stub_getaddrinfo},  // <<< IMPLEMENTAR
  // TODO {"getauxval", (uintptr_t)&stub_getauxval},  // <<< IMPLEMENTAR
  {"getc", (uintptr_t)&getc},  // pass
  {"getcwd", (uintptr_t)&getcwd},  // pass
  {"getenv", (uintptr_t)&getenv},  // pass
  // TODO {"geteuid", (uintptr_t)&stub_geteuid},  // <<< IMPLEMENTAR
  // TODO {"gethostname", (uintptr_t)&stub_gethostname},  // <<< IMPLEMENTAR
  {"getpagesize", (uintptr_t)&getpagesize},  // pass
  // TODO {"getpeername", (uintptr_t)&stub_getpeername},  // <<< IMPLEMENTAR
  {"getpid", (uintptr_t)&getpid},  // pass
  // TODO {"getppid", (uintptr_t)&stub_getppid},  // <<< IMPLEMENTAR
  // TODO {"getrandom", (uintptr_t)&stub_getrandom},  // <<< IMPLEMENTAR
  // TODO {"getsockname", (uintptr_t)&stub_getsockname},  // <<< IMPLEMENTAR
  // TODO {"getsockopt", (uintptr_t)&stub_getsockopt},  // <<< IMPLEMENTAR
  {"gettid", (uintptr_t)&gettid},  // pass
  {"gettimeofday", (uintptr_t)&gettimeofday},  // pass
  // TODO {"getwc", (uintptr_t)&stub_getwc},  // <<< IMPLEMENTAR
  {"glActiveTexture", (uintptr_t)&glActiveTexture},  // gles
  {"glAttachShader", (uintptr_t)&glAttachShader},  // gles
  {"glBindAttribLocation", (uintptr_t)&glBindAttribLocation},  // gles
  {"glBindBuffer", (uintptr_t)&glBindBuffer},  // gles
  {"glBindFramebuffer", (uintptr_t)&glBindFramebuffer},  // gles
  {"glBindTexture", (uintptr_t)&glBindTexture},  // gles
  {"glBlendEquationSeparate", (uintptr_t)&glBlendEquationSeparate},  // gles
  {"glBlendFuncSeparate", (uintptr_t)&glBlendFuncSeparate},  // gles
  {"glBufferData", (uintptr_t)&glBufferData},  // gles
  {"glBufferSubData", (uintptr_t)&glBufferSubData},  // gles
  {"glCheckFramebufferStatus", (uintptr_t)&glCheckFramebufferStatus},  // gles
  {"glClear", (uintptr_t)&glClear},  // gles
  {"glClearColor", (uintptr_t)&glClearColor},  // gles
  {"glCompileShader", (uintptr_t)&glCompileShader},  // gles
  {"glCreateProgram", (uintptr_t)&glCreateProgram},  // gles
  {"glCreateShader", (uintptr_t)&glCreateShader},  // gles
  {"glDeleteBuffers", (uintptr_t)&glDeleteBuffers},  // gles
  {"glDeleteFramebuffers", (uintptr_t)&glDeleteFramebuffers},  // gles
  {"glDeleteProgram", (uintptr_t)&glDeleteProgram},  // gles
  {"glDeleteShader", (uintptr_t)&glDeleteShader},  // gles
  {"glDeleteTextures", (uintptr_t)&glDeleteTextures},  // gles
  {"glDisable", (uintptr_t)&glDisable},  // gles
  {"glDisableVertexAttribArray", (uintptr_t)&glDisableVertexAttribArray},  // gles
  {"glDrawArrays", (uintptr_t)&glDrawArrays},  // gles
  {"glEnable", (uintptr_t)&glEnable},  // gles
  {"glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray},  // gles
  {"glFinish", (uintptr_t)&glFinish},  // gles
  {"glFramebufferTexture2D", (uintptr_t)&glFramebufferTexture2D},  // gles
  {"glGenBuffers", (uintptr_t)&glGenBuffers},  // gles
  {"glGenFramebuffers", (uintptr_t)&glGenFramebuffers},  // gles
  {"glGenTextures", (uintptr_t)&glGenTextures},  // gles
  {"glGetAttribLocation", (uintptr_t)&glGetAttribLocation},  // gles
  {"glGetError", (uintptr_t)&glGetError},  // gles
  {"glGetIntegerv", (uintptr_t)&glGetIntegerv},  // gles
  {"glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog},  // gles
  {"glGetProgramiv", (uintptr_t)&glGetProgramiv},  // gles
  {"glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog},  // gles
  {"glGetShaderiv", (uintptr_t)&glGetShaderiv},  // gles
  {"glGetString", (uintptr_t)&glGetString},  // gles
  {"glGetUniformLocation", (uintptr_t)&glGetUniformLocation},  // gles
  {"glLinkProgram", (uintptr_t)&glLinkProgram},  // gles
  {"glPixelStorei", (uintptr_t)&glPixelStorei},  // gles
  {"glReadPixels", (uintptr_t)&glReadPixels},  // gles
  {"glScissor", (uintptr_t)&glScissor},  // gles
  {"glShaderBinary", (uintptr_t)&glShaderBinary},  // gles
  {"glShaderSource", (uintptr_t)&glShaderSource},  // gles
  {"glTexImage2D", (uintptr_t)&glTexImage2D},  // gles
  {"glTexParameteri", (uintptr_t)&glTexParameteri},  // gles
  {"glTexSubImage2D", (uintptr_t)&glTexSubImage2D},  // gles
  {"glUniform1i", (uintptr_t)&glUniform1i},  // gles
  {"glUniform3f", (uintptr_t)&glUniform3f},  // gles
  {"glUniform4f", (uintptr_t)&glUniform4f},  // gles
  {"glUniformMatrix3fv", (uintptr_t)&glUniformMatrix3fv},  // gles
  {"glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv},  // gles
  {"glUseProgram", (uintptr_t)&glUseProgram},  // gles
  {"glVertexAttribPointer", (uintptr_t)&glVertexAttribPointer},  // gles
  {"glViewport", (uintptr_t)&glViewport},  // gles
  {"gmtime", (uintptr_t)&gmtime},  // pass
  // TODO {"gmtime_r", (uintptr_t)&stub_gmtime_r},  // <<< IMPLEMENTAR
  {"iconv", (uintptr_t)&iconv},  // pass
  {"iconv_close", (uintptr_t)&iconv_close},  // pass
  {"iconv_open", (uintptr_t)&iconv_open},  // pass
  // TODO {"inet_pton", (uintptr_t)&stub_inet_pton},  // <<< IMPLEMENTAR
  // [zlib runtime] {"inflate", (uintptr_t)&inflate},  // pass
  // [zlib runtime] {"inflateEnd", (uintptr_t)&inflateEnd},  // pass
  // [zlib runtime] {"inflateInit2_", (uintptr_t)&inflateInit2_},  // pass
  // [zlib runtime] // TODO {"inflateReset", (uintptr_t)&stub_inflateReset},  // <<< IMPLEMENTAR
  // [zlib runtime] // TODO {"inflateReset2", (uintptr_t)&stub_inflateReset2},  // <<< IMPLEMENTAR
  {"ioctl", (uintptr_t)&ioctl},  // pass
  // TODO {"isatty", (uintptr_t)&stub_isatty},  // <<< IMPLEMENTAR
  // TODO {"iswalpha_l", (uintptr_t)&stub_iswalpha_l},  // <<< IMPLEMENTAR
  // TODO {"iswblank_l", (uintptr_t)&stub_iswblank_l},  // <<< IMPLEMENTAR
  // TODO {"iswcntrl_l", (uintptr_t)&stub_iswcntrl_l},  // <<< IMPLEMENTAR
  // TODO {"iswdigit_l", (uintptr_t)&stub_iswdigit_l},  // <<< IMPLEMENTAR
  // TODO {"iswlower_l", (uintptr_t)&stub_iswlower_l},  // <<< IMPLEMENTAR
  // TODO {"iswprint_l", (uintptr_t)&stub_iswprint_l},  // <<< IMPLEMENTAR
  // TODO {"iswpunct_l", (uintptr_t)&stub_iswpunct_l},  // <<< IMPLEMENTAR
  // TODO {"iswspace_l", (uintptr_t)&stub_iswspace_l},  // <<< IMPLEMENTAR
  // TODO {"iswupper_l", (uintptr_t)&stub_iswupper_l},  // <<< IMPLEMENTAR
  // TODO {"iswxdigit_l", (uintptr_t)&stub_iswxdigit_l},  // <<< IMPLEMENTAR
  // TODO {"kill", (uintptr_t)&stub_kill},  // <<< IMPLEMENTAR
  // TODO {"lchown", (uintptr_t)&stub_lchown},  // <<< IMPLEMENTAR
  {"ldexp", (uintptr_t)&ldexp},  // pass
  // TODO {"ldexpf", (uintptr_t)&stub_ldexpf},  // <<< IMPLEMENTAR
  // TODO {"ldexpl", (uintptr_t)&stub_ldexpl},  // <<< IMPLEMENTAR
  // TODO {"link", (uintptr_t)&stub_link},  // <<< IMPLEMENTAR
  // TODO {"listen", (uintptr_t)&stub_listen},  // <<< IMPLEMENTAR
  {"localeconv", (uintptr_t)&localeconv},  // pass
  {"localtime", (uintptr_t)&localtime},  // pass
  // TODO {"localtime_r", (uintptr_t)&stub_localtime_r},  // <<< IMPLEMENTAR
  {"log", (uintptr_t)&log},  // pass
  {"log10", (uintptr_t)&log10},  // pass
  // TODO {"log10f", (uintptr_t)&stub_log10f},  // <<< IMPLEMENTAR
  {"log2", (uintptr_t)&log2},  // pass
  // TODO {"log2f", (uintptr_t)&stub_log2f},  // <<< IMPLEMENTAR
  {"logf", (uintptr_t)&logf},  // pass
  // TODO {"longjmp", (uintptr_t)&stub_longjmp},  // <<< IMPLEMENTAR
  {"lseek", (uintptr_t)&lseek},  // pass
  // TODO {"lseek64", (uintptr_t)&stub_lseek64},  // <<< IMPLEMENTAR
  {"lstat", (uintptr_t)&lstat},  // pass
  {"malloc", (uintptr_t)&malloc},  // pass
  // TODO {"mbrlen", (uintptr_t)&stub_mbrlen},  // <<< IMPLEMENTAR
  // TODO {"mbrtowc", (uintptr_t)&stub_mbrtowc},  // <<< IMPLEMENTAR
  // TODO {"mbsnrtowcs", (uintptr_t)&stub_mbsnrtowcs},  // <<< IMPLEMENTAR
  // TODO {"mbsrtowcs", (uintptr_t)&stub_mbsrtowcs},  // <<< IMPLEMENTAR
  // TODO {"mbtowc", (uintptr_t)&stub_mbtowc},  // <<< IMPLEMENTAR
  {"memalign", (uintptr_t)&memalign},  // pass
  {"memchr", (uintptr_t)&memchr},  // pass
  {"memcmp", (uintptr_t)&memcmp},  // pass
  {"memcpy", (uintptr_t)&memcpy},  // pass
  // TODO {"__memcpy_chk", (uintptr_t)&stub___memcpy_chk},  // <<< IMPLEMENTAR
  {"memmove", (uintptr_t)&memmove},  // pass
  // TODO {"__memmove_chk", (uintptr_t)&stub___memmove_chk},  // <<< IMPLEMENTAR
  {"memset", (uintptr_t)&memset},  // pass
  // TODO {"__memset_chk", (uintptr_t)&stub___memset_chk},  // <<< IMPLEMENTAR
  {"mkdir", (uintptr_t)&mkdir},  // pass
  // TODO {"mkfifo", (uintptr_t)&stub_mkfifo},  // <<< IMPLEMENTAR
  {"mktime", (uintptr_t)&mktime},  // pass
  {"mmap", (uintptr_t)&mmap},  // pass
  {"modf", (uintptr_t)&modf},  // pass
  // TODO {"modff", (uintptr_t)&stub_modff},  // <<< IMPLEMENTAR
  // TODO {"mremap", (uintptr_t)&stub_mremap},  // <<< IMPLEMENTAR
  {"munmap", (uintptr_t)&munmap},  // pass
  {"nan", (uintptr_t)&nan},  // pass
  {"nanf", (uintptr_t)&nanf},  // pass
  {"nanosleep", (uintptr_t)&nanosleep},  // pass
  // TODO {"newlocale", (uintptr_t)&stub_newlocale},  // <<< IMPLEMENTAR
  {"open", (uintptr_t)&open},  // pass
  // TODO {"__open_2", (uintptr_t)&stub___open_2},  // <<< IMPLEMENTAR
  // TODO {"openat", (uintptr_t)&stub_openat},  // <<< IMPLEMENTAR
  {"opendir", (uintptr_t)&opendir},  // pass
  // TODO {"openlog", (uintptr_t)&stub_openlog},  // <<< IMPLEMENTAR
  // TODO {"pathconf", (uintptr_t)&stub_pathconf},  // <<< IMPLEMENTAR
  {"pipe", (uintptr_t)&pipe},  // pass
  // TODO {"pipe2", (uintptr_t)&stub_pipe2},  // <<< IMPLEMENTAR
  // TODO {"poll", (uintptr_t)&stub_poll},  // <<< IMPLEMENTAR
  {"posix_memalign", (uintptr_t)&posix_memalign},  // pass
  {"pow", (uintptr_t)&pow},  // pass
  {"powf", (uintptr_t)&powf},  // pass
  // TODO {"prctl", (uintptr_t)&stub_prctl},  // <<< IMPLEMENTAR
  // TODO {"pread", (uintptr_t)&stub_pread},  // <<< IMPLEMENTAR
  // TODO {"pread64", (uintptr_t)&stub_pread64},  // <<< IMPLEMENTAR
  // TODO {"preadv", (uintptr_t)&stub_preadv},  // <<< IMPLEMENTAR
  {"printf", (uintptr_t)&printf},  // pass
  {"pthread_attr_destroy", (uintptr_t)&pthread_attr_destroy_fake},  // pthread wrapper (core)
  {"pthread_attr_init", (uintptr_t)&pthread_attr_init_fake},  // pthread wrapper (core)
  {"pthread_attr_setdetachstate", (uintptr_t)&pthread_attr_setdetachstate_fake},  // pthread wrapper (core)
  {"pthread_attr_setstacksize", (uintptr_t)&pthread_attr_setstacksize_fake},  // pthread wrapper (core)
  {"pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake},  // pthread wrapper (core)
  {"pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_fake},  // pthread wrapper (core)
  {"pthread_cond_init", (uintptr_t)&pthread_cond_init_fake},  // pthread wrapper (core)
  {"pthread_cond_signal", (uintptr_t)&pthread_cond_signal_fake},  // pthread wrapper (core)
  {"pthread_cond_timedwait", (uintptr_t)&pthread_cond_timedwait_fake},  // pthread wrapper (core)
  {"pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake},  // pthread wrapper (core)
  {"pthread_create", (uintptr_t)&pthread_create_fake},  // pthread wrapper (core)
  {"pthread_detach", (uintptr_t)&pthread_detach_fake},  // pthread wrapper (core)
  {"pthread_getschedparam", (uintptr_t)&pthread_getschedparam_fake},  // pthread wrapper (core)
  {"pthread_getspecific", (uintptr_t)&pthread_getspecific_fake},  // pthread wrapper (core)
  {"pthread_join", (uintptr_t)&pthread_join_fake},  // pthread wrapper (core)
  {"pthread_key_create", (uintptr_t)&pthread_key_create_fake},  // pthread wrapper (core)
  {"pthread_key_delete", (uintptr_t)&pthread_key_delete_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_destroy", (uintptr_t)&pthread_mutexattr_destroy_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_init", (uintptr_t)&pthread_mutexattr_init_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_settype", (uintptr_t)&pthread_mutexattr_settype_fake},  // pthread wrapper (core)
  {"pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake},  // pthread wrapper (core)
  {"pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake},  // pthread wrapper (core)
  {"pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake},  // pthread wrapper (core)
  {"pthread_mutex_trylock", (uintptr_t)&pthread_mutex_trylock_fake},  // pthread wrapper (core)
  {"pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake},  // pthread wrapper (core)
  {"pthread_once", (uintptr_t)&pthread_once_fake},  // pthread wrapper (core)
  {"pthread_rwlock_destroy", (uintptr_t)&pthread_rwlock_destroy_fake},  // pthread wrapper (core)
  {"pthread_rwlock_init", (uintptr_t)&pthread_rwlock_init_fake},  // pthread wrapper (core)
  {"pthread_rwlock_rdlock", (uintptr_t)&pthread_rwlock_rdlock_fake},  // pthread wrapper (core)
  {"pthread_rwlock_tryrdlock", (uintptr_t)&pthread_rwlock_tryrdlock_fake},  // pthread wrapper (core)
  {"pthread_rwlock_trywrlock", (uintptr_t)&pthread_rwlock_trywrlock_fake},  // pthread wrapper (core)
  {"pthread_rwlock_unlock", (uintptr_t)&pthread_rwlock_unlock_fake},  // pthread wrapper (core)
  {"pthread_rwlock_wrlock", (uintptr_t)&pthread_rwlock_wrlock_fake},  // pthread wrapper (core)
  {"pthread_self", (uintptr_t)&pthread_self_fake},  // pthread wrapper (core)
  {"pthread_setschedparam", (uintptr_t)&pthread_setschedparam_fake},  // pthread wrapper (core)
  {"pthread_setspecific", (uintptr_t)&pthread_setspecific_fake},  // pthread wrapper (core)
  {"pthread_sigmask", (uintptr_t)&pthread_sigmask_fake},  // pthread wrapper (core)
  {"putc", (uintptr_t)&putc},  // pass
  {"puts", (uintptr_t)&puts},  // pass
  // TODO {"pwrite", (uintptr_t)&stub_pwrite},  // <<< IMPLEMENTAR
  // TODO {"pwrite64", (uintptr_t)&stub_pwrite64},  // <<< IMPLEMENTAR
  // TODO {"pwritev", (uintptr_t)&stub_pwritev},  // <<< IMPLEMENTAR
  {"qsort", (uintptr_t)&qsort},  // pass
  {"rand", (uintptr_t)&rand},  // pass
  {"read", (uintptr_t)&read},  // pass
  // TODO {"__read_chk", (uintptr_t)&stub___read_chk},  // <<< IMPLEMENTAR
  {"readdir", (uintptr_t)&readdir},  // pass
  // TODO {"readlink", (uintptr_t)&stub_readlink},  // <<< IMPLEMENTAR
  // TODO {"readv", (uintptr_t)&stub_readv},  // <<< IMPLEMENTAR
  {"realloc", (uintptr_t)&realloc},  // pass
  {"realpath", (uintptr_t)&realpath},  // pass
  // TODO {"recv", (uintptr_t)&stub_recv},  // <<< IMPLEMENTAR
  // TODO {"recvfrom", (uintptr_t)&stub_recvfrom},  // <<< IMPLEMENTAR
  // TODO {"recvmsg", (uintptr_t)&stub_recvmsg},  // <<< IMPLEMENTAR
  // TODO {"__register_atfork", (uintptr_t)&stub___register_atfork},  // <<< IMPLEMENTAR
  // TODO {"remainderf", (uintptr_t)&stub_remainderf},  // <<< IMPLEMENTAR
  // TODO {"remove", (uintptr_t)&stub_remove},  // <<< IMPLEMENTAR
  {"rename", (uintptr_t)&rename},  // pass
  {"rmdir", (uintptr_t)&rmdir},  // pass
  // TODO {"scalbn", (uintptr_t)&stub_scalbn},  // <<< IMPLEMENTAR
  // TODO {"scalbnf", (uintptr_t)&stub_scalbnf},  // <<< IMPLEMENTAR
  // TODO {"sched_getaffinity", (uintptr_t)&stub_sched_getaffinity},  // <<< IMPLEMENTAR
  // TODO {"sched_get_priority_max", (uintptr_t)&stub_sched_get_priority_max},  // <<< IMPLEMENTAR
  // TODO {"sched_get_priority_min", (uintptr_t)&stub_sched_get_priority_min},  // <<< IMPLEMENTAR
  {"sched_yield", (uintptr_t)&sched_yield},  // pass
  // TODO {"select", (uintptr_t)&stub_select},  // <<< IMPLEMENTAR
  {"sem_destroy", (uintptr_t)&sem_destroy_fake},  // pthread wrapper (core)
  {"sem_getvalue", (uintptr_t)&sem_getvalue_fake},  // pthread wrapper (core)
  {"sem_init", (uintptr_t)&sem_init_fake},  // pthread wrapper (core)
  {"sem_post", (uintptr_t)&sem_post_fake},  // pthread wrapper (core)
  {"sem_timedwait", (uintptr_t)&sem_timedwait_fake},  // pthread wrapper (core)
  {"sem_trywait", (uintptr_t)&sem_trywait_fake},  // pthread wrapper (core)
  {"sem_wait", (uintptr_t)&sem_wait_fake},  // pthread wrapper (core)
  // TODO {"send", (uintptr_t)&stub_send},  // <<< IMPLEMENTAR
  // TODO {"sendfile", (uintptr_t)&stub_sendfile},  // <<< IMPLEMENTAR
  // TODO {"sendmsg", (uintptr_t)&stub_sendmsg},  // <<< IMPLEMENTAR
  // TODO {"sendto", (uintptr_t)&stub_sendto},  // <<< IMPLEMENTAR
  // TODO {"__sendto_chk", (uintptr_t)&stub___sendto_chk},  // <<< IMPLEMENTAR
  // TODO {"setbuf", (uintptr_t)&stub_setbuf},  // <<< IMPLEMENTAR
  {"setenv", (uintptr_t)&setenv},  // pass
  // TODO {"setgid", (uintptr_t)&stub_setgid},  // <<< IMPLEMENTAR
  // TODO {"setgroups", (uintptr_t)&stub_setgroups},  // <<< IMPLEMENTAR
  // TODO {"setjmp", (uintptr_t)&stub_setjmp},  // <<< IMPLEMENTAR
  {"setlocale", (uintptr_t)&setlocale},  // pass
  // TODO {"setpgid", (uintptr_t)&stub_setpgid},  // <<< IMPLEMENTAR
  // TODO {"setsid", (uintptr_t)&stub_setsid},  // <<< IMPLEMENTAR
  // TODO {"setsockopt", (uintptr_t)&stub_setsockopt},  // <<< IMPLEMENTAR
  // TODO {"setuid", (uintptr_t)&stub_setuid},  // <<< IMPLEMENTAR
  // TODO {"__sF", (uintptr_t)&stub___sF},  // <<< IMPLEMENTAR
  // TODO {"shutdown", (uintptr_t)&stub_shutdown},  // <<< IMPLEMENTAR
  // TODO {"sigaction", (uintptr_t)&stub_sigaction},  // <<< IMPLEMENTAR
  // TODO {"sigaddset", (uintptr_t)&stub_sigaddset},  // <<< IMPLEMENTAR
  // TODO {"sigemptyset", (uintptr_t)&stub_sigemptyset},  // <<< IMPLEMENTAR
  // TODO {"sigfillset", (uintptr_t)&stub_sigfillset},  // <<< IMPLEMENTAR
  // TODO {"signal", (uintptr_t)&stub_signal},  // <<< IMPLEMENTAR
  {"sin", (uintptr_t)&sin},  // pass
  // TODO {"sincos", (uintptr_t)&stub_sincos},  // <<< IMPLEMENTAR
  // TODO {"sincosf", (uintptr_t)&stub_sincosf},  // <<< IMPLEMENTAR
  {"sinf", (uintptr_t)&sinf},  // pass
  // TODO {"sinh", (uintptr_t)&stub_sinh},  // <<< IMPLEMENTAR
  // TODO {"sinhf", (uintptr_t)&stub_sinhf},  // <<< IMPLEMENTAR
  {"slCreateEngine", (uintptr_t)&slCreateEngine_shim},  // opensles_shim
  {"SL_IID_ANDROIDSIMPLEBUFFERQUEUE", (uintptr_t)&SL_IID_ANDROIDSIMPLEBUFFERQUEUE_shim},  // opensles_shim
  {"SL_IID_ENGINE", (uintptr_t)&SL_IID_ENGINE_shim},  // opensles_shim
  {"SL_IID_PLAY", (uintptr_t)&SL_IID_PLAY_shim},  // opensles_shim
  {"SL_IID_RECORD", (uintptr_t)&SL_IID_RECORD_shim},  // opensles_shim
  {"SL_IID_VOLUME", (uintptr_t)&SL_IID_VOLUME_shim},  // opensles_shim
  {"snprintf", (uintptr_t)&snprintf},  // pass
  // TODO {"socket", (uintptr_t)&stub_socket},  // <<< IMPLEMENTAR
  // TODO {"socketpair", (uintptr_t)&stub_socketpair},  // <<< IMPLEMENTAR
  // TODO {"splice", (uintptr_t)&stub_splice},  // <<< IMPLEMENTAR
  {"sqrt", (uintptr_t)&sqrt},  // pass
  {"sscanf", (uintptr_t)&sscanf},  // pass
  {"__stack_chk_fail", (uintptr_t)&__stack_chk_fail},  // abi
  {"stat", (uintptr_t)&stat},  // pass
  // TODO {"statvfs", (uintptr_t)&stub_statvfs},  // <<< IMPLEMENTAR
  // TODO {"stderr", (uintptr_t)&stub_stderr},  // <<< IMPLEMENTAR
  // TODO {"stdin", (uintptr_t)&stub_stdin},  // <<< IMPLEMENTAR
  // TODO {"stdout", (uintptr_t)&stub_stdout},  // <<< IMPLEMENTAR
  {"strcasecmp", (uintptr_t)&strcasecmp},  // pass
  {"strcat", (uintptr_t)&strcat},  // pass
  // TODO {"__strcat_chk", (uintptr_t)&stub___strcat_chk},  // <<< IMPLEMENTAR
  {"strchr", (uintptr_t)&strchr},  // pass
  // TODO {"__strchr_chk", (uintptr_t)&stub___strchr_chk},  // <<< IMPLEMENTAR
  {"strcmp", (uintptr_t)&strcmp},  // pass
  // TODO {"strcoll_l", (uintptr_t)&stub_strcoll_l},  // <<< IMPLEMENTAR
  {"strcpy", (uintptr_t)&strcpy},  // pass
  // TODO {"__strcpy_chk", (uintptr_t)&stub___strcpy_chk},  // <<< IMPLEMENTAR
  // TODO {"strcspn", (uintptr_t)&stub_strcspn},  // <<< IMPLEMENTAR
  {"strdup", (uintptr_t)&strdup},  // pass
  {"strerror", (uintptr_t)&strerror},  // pass
  // TODO {"strerror_r", (uintptr_t)&stub_strerror_r},  // <<< IMPLEMENTAR
  {"strftime", (uintptr_t)&strftime},  // pass
  // TODO {"strftime_l", (uintptr_t)&stub_strftime_l},  // <<< IMPLEMENTAR
  // TODO {"strlcat", (uintptr_t)&stub_strlcat},  // <<< IMPLEMENTAR
  // TODO {"strlcpy", (uintptr_t)&stub_strlcpy},  // <<< IMPLEMENTAR
  {"strlen", (uintptr_t)&strlen},  // pass
  // TODO {"__strlen_chk", (uintptr_t)&stub___strlen_chk},  // <<< IMPLEMENTAR
  {"strncmp", (uintptr_t)&strncmp},  // pass
  {"strncpy", (uintptr_t)&strncpy},  // pass
  // TODO {"__strncpy_chk", (uintptr_t)&stub___strncpy_chk},  // <<< IMPLEMENTAR
  // TODO {"__strncpy_chk2", (uintptr_t)&stub___strncpy_chk2},  // <<< IMPLEMENTAR
  // TODO {"strnlen", (uintptr_t)&stub_strnlen},  // <<< IMPLEMENTAR
  // TODO {"strpbrk", (uintptr_t)&stub_strpbrk},  // <<< IMPLEMENTAR
  {"strrchr", (uintptr_t)&strrchr},  // pass
  // TODO {"strspn", (uintptr_t)&stub_strspn},  // <<< IMPLEMENTAR
  {"strstr", (uintptr_t)&strstr},  // pass
  {"strtod", (uintptr_t)&strtod},  // pass
  {"strtof", (uintptr_t)&strtof},  // pass
  {"strtol", (uintptr_t)&strtol},  // pass
  // TODO {"strtold", (uintptr_t)&stub_strtold},  // <<< IMPLEMENTAR
  // TODO {"strtold_l", (uintptr_t)&stub_strtold_l},  // <<< IMPLEMENTAR
  // TODO {"strtoll", (uintptr_t)&stub_strtoll},  // <<< IMPLEMENTAR
  // TODO {"strtoll_l", (uintptr_t)&stub_strtoll_l},  // <<< IMPLEMENTAR
  {"strtoul", (uintptr_t)&strtoul},  // pass
  // TODO {"strtoull", (uintptr_t)&stub_strtoull},  // <<< IMPLEMENTAR
  // TODO {"strtoull_l", (uintptr_t)&stub_strtoull_l},  // <<< IMPLEMENTAR
  // TODO {"strxfrm_l", (uintptr_t)&stub_strxfrm_l},  // <<< IMPLEMENTAR
  // TODO {"swprintf", (uintptr_t)&stub_swprintf},  // <<< IMPLEMENTAR
  // TODO {"symlink", (uintptr_t)&stub_symlink},  // <<< IMPLEMENTAR
  // TODO {"syscall", (uintptr_t)&stub_syscall},  // <<< IMPLEMENTAR
  {"sysconf", (uintptr_t)&sysconf},  // pass
  // TODO {"sysinfo", (uintptr_t)&stub_sysinfo},  // <<< IMPLEMENTAR
  // TODO {"syslog", (uintptr_t)&stub_syslog},  // <<< IMPLEMENTAR
  // TODO {"__system_property_get", (uintptr_t)&stub___system_property_get},  // <<< IMPLEMENTAR
  {"tan", (uintptr_t)&tan},  // pass
  {"tanf", (uintptr_t)&tanf},  // pass
  // TODO {"tanh", (uintptr_t)&stub_tanh},  // <<< IMPLEMENTAR
  // TODO {"tanhf", (uintptr_t)&stub_tanhf},  // <<< IMPLEMENTAR
  // TODO {"tcgetattr", (uintptr_t)&stub_tcgetattr},  // <<< IMPLEMENTAR
  // TODO {"tcsetattr", (uintptr_t)&stub_tcsetattr},  // <<< IMPLEMENTAR
  {"time", (uintptr_t)&time},  // pass
  // TODO {"towlower_l", (uintptr_t)&stub_towlower_l},  // <<< IMPLEMENTAR
  // TODO {"towupper_l", (uintptr_t)&stub_towupper_l},  // <<< IMPLEMENTAR
  // TODO {"truncate", (uintptr_t)&stub_truncate},  // <<< IMPLEMENTAR
  // [zlib runtime] {"uncompress", (uintptr_t)&uncompress},  // pass
  // TODO {"ungetc", (uintptr_t)&stub_ungetc},  // <<< IMPLEMENTAR
  // TODO {"ungetwc", (uintptr_t)&stub_ungetwc},  // <<< IMPLEMENTAR
  {"unlink", (uintptr_t)&unlink},  // pass
  // TODO {"unlinkat", (uintptr_t)&stub_unlinkat},  // <<< IMPLEMENTAR
  // TODO {"unsetenv", (uintptr_t)&stub_unsetenv},  // <<< IMPLEMENTAR
  // TODO {"uselocale", (uintptr_t)&stub_uselocale},  // <<< IMPLEMENTAR
  // TODO {"utimensat", (uintptr_t)&stub_utimensat},  // <<< IMPLEMENTAR
  // TODO {"utimes", (uintptr_t)&stub_utimes},  // <<< IMPLEMENTAR
  // TODO {"vasprintf", (uintptr_t)&stub_vasprintf},  // <<< IMPLEMENTAR
  // TODO {"vfprintf", (uintptr_t)&stub_vfprintf},  // <<< IMPLEMENTAR
  {"vsnprintf", (uintptr_t)&vsnprintf},  // pass
  // TODO {"__vsnprintf_chk", (uintptr_t)&stub___vsnprintf_chk},  // <<< IMPLEMENTAR
  // TODO {"__vsprintf_chk", (uintptr_t)&stub___vsprintf_chk},  // <<< IMPLEMENTAR
  // TODO {"vsscanf", (uintptr_t)&stub_vsscanf},  // <<< IMPLEMENTAR
  // TODO {"waitpid", (uintptr_t)&stub_waitpid},  // <<< IMPLEMENTAR
  // TODO {"wcrtomb", (uintptr_t)&stub_wcrtomb},  // <<< IMPLEMENTAR
  // TODO {"wcscmp", (uintptr_t)&stub_wcscmp},  // <<< IMPLEMENTAR
  // TODO {"wcscoll_l", (uintptr_t)&stub_wcscoll_l},  // <<< IMPLEMENTAR
  // TODO {"wcslcat", (uintptr_t)&stub_wcslcat},  // <<< IMPLEMENTAR
  // TODO {"wcslcpy", (uintptr_t)&stub_wcslcpy},  // <<< IMPLEMENTAR
  // TODO {"wcslen", (uintptr_t)&stub_wcslen},  // <<< IMPLEMENTAR
  // TODO {"wcsncmp", (uintptr_t)&stub_wcsncmp},  // <<< IMPLEMENTAR
  // TODO {"wcsncpy", (uintptr_t)&stub_wcsncpy},  // <<< IMPLEMENTAR
  // TODO {"wcsnlen", (uintptr_t)&stub_wcsnlen},  // <<< IMPLEMENTAR
  // TODO {"wcsnrtombs", (uintptr_t)&stub_wcsnrtombs},  // <<< IMPLEMENTAR
  // TODO {"wcsstr", (uintptr_t)&stub_wcsstr},  // <<< IMPLEMENTAR
  // TODO {"wcstod", (uintptr_t)&stub_wcstod},  // <<< IMPLEMENTAR
  // TODO {"wcstof", (uintptr_t)&stub_wcstof},  // <<< IMPLEMENTAR
  // TODO {"wcstol", (uintptr_t)&stub_wcstol},  // <<< IMPLEMENTAR
  // TODO {"wcstold", (uintptr_t)&stub_wcstold},  // <<< IMPLEMENTAR
  // TODO {"wcstoll", (uintptr_t)&stub_wcstoll},  // <<< IMPLEMENTAR
  // TODO {"wcstoul", (uintptr_t)&stub_wcstoul},  // <<< IMPLEMENTAR
  // TODO {"wcstoull", (uintptr_t)&stub_wcstoull},  // <<< IMPLEMENTAR
  // TODO {"wcsxfrm_l", (uintptr_t)&stub_wcsxfrm_l},  // <<< IMPLEMENTAR
  // TODO {"wctob", (uintptr_t)&stub_wctob},  // <<< IMPLEMENTAR
  // TODO {"wmemchr", (uintptr_t)&stub_wmemchr},  // <<< IMPLEMENTAR
  // TODO {"wmemcmp", (uintptr_t)&stub_wmemcmp},  // <<< IMPLEMENTAR
  {"write", (uintptr_t)&write},  // pass
  // TODO {"__write_chk", (uintptr_t)&stub___write_chk},  // <<< IMPLEMENTAR
  // TODO {"writev", (uintptr_t)&stub_writev},  // <<< IMPLEMENTAR
  // TODO {"ZSTD_trace_compress_begin", (uintptr_t)&stub_ZSTD_trace_compress_begin},  // <<< IMPLEMENTAR
  // TODO {"ZSTD_trace_compress_end", (uintptr_t)&stub_ZSTD_trace_compress_end},  // <<< IMPLEMENTAR
  // TODO {"ZSTD_trace_decompress_begin", (uintptr_t)&stub_ZSTD_trace_decompress_begin},  // <<< IMPLEMENTAR
  // TODO {"ZSTD_trace_decompress_end", (uintptr_t)&stub_ZSTD_trace_decompress_end},  // <<< IMPLEMENTAR
};
size_t dynlib_numfunctions = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);

// ===================== SIMBOLOS A IMPLEMENTAR =====================
//   acosf
//   acosh
//   acoshf
//   android_set_abort_message
//   asinf
//   asinh
//   asinhf
//   atanf
//   atanh
//   atanhf
//   bind
//   btowc
//   chmod
//   chown
//   chroot
//   clearerr
//   clock_nanosleep
//   closelog
//   connect
//   copy_file_range
//   cosh
//   coshf
//   __ctype_get_mb_cur_max
//   deflateReset
//   dirfd
//   div
//   dladdr
//   dlclose
//   dlerror
//   dl_iterate_phdr
//   dlopen
//   dlsym
//   environ
//   execvp
//   _exit
//   _Exit
//   exp2f
//   fchmod
//   fchmodat
//   fchown
//   fdatasync
//   __FD_ISSET_chk
//   fdopendir
//   __FD_SET_chk
//   feholdexcept
//   feof
//   ferror
//   fesetenv
//   fopen64
//   fork
//   fputwc
//   __fread_chk
//   freeaddrinfo
//   freelocale
//   frexpl
//   fseeko
//   fseeko64
//   fstatat
//   fsync
//   ftello
//   ftello64
//   ftruncate
//   ftruncate64
//   futimens
//   gai_strerror
//   getaddrinfo
//   getauxval
//   geteuid
//   gethostname
//   getpeername
//   getppid
//   getrandom
//   getsockname
//   getsockopt
//   getwc
//   gmtime_r
//   inet_pton
//   inflateReset
//   inflateReset2
//   isatty
//   iswalpha_l
//   iswblank_l
//   iswcntrl_l
//   iswdigit_l
//   iswlower_l
//   iswprint_l
//   iswpunct_l
//   iswspace_l
//   iswupper_l
//   iswxdigit_l
//   kill
//   lchown
//   ldexpf
//   ldexpl
//   link
//   listen
//   localtime_r
//   log10f
//   log2f
//   longjmp
//   lseek64
//   mbrlen
//   mbrtowc
//   mbsnrtowcs
//   mbsrtowcs
//   mbtowc
//   __memcpy_chk
//   __memmove_chk
//   __memset_chk
//   mkfifo
//   modff
//   mremap
//   newlocale
//   __open_2
//   openat
//   openlog
//   pathconf
//   pipe2
//   poll
//   prctl
//   pread
//   pread64
//   preadv
//   pwrite
//   pwrite64
//   pwritev
//   __read_chk
//   readlink
//   readv
//   recv
//   recvfrom
//   recvmsg
//   __register_atfork
//   remainderf
//   remove
//   scalbn
//   scalbnf
//   sched_getaffinity
//   sched_get_priority_max
//   sched_get_priority_min
//   select
//   send
//   sendfile
//   sendmsg
//   sendto
//   __sendto_chk
//   setbuf
//   setgid
//   setgroups
//   setjmp
//   setpgid
//   setsid
//   setsockopt
//   setuid
//   __sF
//   shutdown
//   sigaction
//   sigaddset
//   sigemptyset
//   sigfillset
//   signal
//   sincos
//   sincosf
//   sinh
//   sinhf
//   socket
//   socketpair
//   splice
//   statvfs
//   stderr
//   stdin
//   stdout
//   __strcat_chk
//   __strchr_chk
//   strcoll_l
//   __strcpy_chk
//   strcspn
//   strerror_r
//   strftime_l
//   strlcat
//   strlcpy
//   __strlen_chk
//   __strncpy_chk
//   __strncpy_chk2
//   strnlen
//   strpbrk
//   strspn
//   strtold
//   strtold_l
//   strtoll
//   strtoll_l
//   strtoull
//   strtoull_l
//   strxfrm_l
//   swprintf
//   symlink
//   syscall
//   sysinfo
//   syslog
//   __system_property_get
//   tanh
//   tanhf
//   tcgetattr
//   tcsetattr
//   towlower_l
//   towupper_l
//   truncate
//   ungetc
//   ungetwc
//   unlinkat
//   unsetenv
//   uselocale
//   utimensat
//   utimes
//   vasprintf
//   vfprintf
//   __vsnprintf_chk
//   __vsprintf_chk
//   vsscanf
//   waitpid
//   wcrtomb
//   wcscmp
//   wcscoll_l
//   wcslcat
//   wcslcpy
//   wcslen
//   wcsncmp
//   wcsncpy
//   wcsnlen
//   wcsnrtombs
//   wcsstr
//   wcstod
//   wcstof
//   wcstol
//   wcstold
//   wcstoll
//   wcstoul
//   wcstoull
//   wcsxfrm_l
//   wctob
//   wmemchr
//   wmemcmp
//   __write_chk
//   writev
//   ZSTD_trace_compress_begin
//   ZSTD_trace_compress_end
//   ZSTD_trace_decompress_begin
//   ZSTD_trace_decompress_end
