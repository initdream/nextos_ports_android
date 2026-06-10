// imports.gen.c — GERADO por new-port.sh para 'cuphead' (libil2cpp.so)
// 201 simbolos. Resolva os UNKNOWN no fim do arquivo.
#define _GNU_SOURCE
#include "imports.h"
#include "so_util.h"
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
#include <sched.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <locale.h>
#include <signal.h>
#include <setjmp.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netdb.h>
#include <sys/time.h>
#include <poll.h>
#include <pthread.h>
#include <zlib.h>
static int *bionic_errno(void){return __errno_location();}
extern int __cxa_atexit(void(*)(void*),void*,void*);
extern void __cxa_finalize(void*);
extern int __cxa_thread_atexit_impl(void(*)(void*),void*,void*);
/* liblog (Android) não existe no glibc -> stubs p/ stderr */
static int __android_log_print(int p,const char*t,const char*f,...){(void)p;(void)t;(void)f;return 0;}
static int __android_log_write(int p,const char*t,const char*m){(void)p;fprintf(stderr,"[ALOG:%s] %s\n",t?t:"",m?m:"");return 0;}
FILE *stderr_fake;

// extern decls dos _fake (def em pthread_fake.c)
extern long pthread_attr_destroy_fake(); extern long pthread_attr_getstack_fake();
extern long pthread_attr_init_fake(); extern long pthread_condattr_destroy_fake();
extern long pthread_condattr_init_fake(); extern long pthread_condattr_setclock_fake();
extern long pthread_cond_broadcast_fake(); extern long pthread_cond_destroy_fake();
extern long pthread_cond_init_fake(); extern long pthread_cond_signal_fake();
extern long pthread_cond_timedwait_fake(); extern long pthread_cond_wait_fake();
extern long pthread_create_fake(); extern long pthread_detach_fake();
extern long pthread_getattr_np_fake(); extern long pthread_getspecific_fake();
extern long pthread_key_create_fake(); extern long pthread_key_delete_fake();
extern long pthread_mutexattr_destroy_fake(); extern long pthread_mutexattr_init_fake();
extern long pthread_mutexattr_settype_fake(); extern long pthread_mutex_destroy_fake();
extern long pthread_mutex_init_fake(); extern long pthread_mutex_lock_fake();
extern long pthread_mutex_unlock_fake(); extern long pthread_once_fake();
extern long pthread_self_fake(); extern long pthread_setspecific_fake();
extern long sem_getvalue_fake(); extern long sem_init_fake();
extern long sem_post_fake(); extern long sem_wait_fake();

// === passthrough/pthread/shim: ligados automaticamente ===
DynLibFunction dynlib_functions[] = {
  {"abort", (uintptr_t)&abort},  // pass
  // TODO {"accept", (uintptr_t)&stub_accept},  // <<< IMPLEMENTAR
  {"access", (uintptr_t)&access},  // pass
  {"acos", (uintptr_t)&acos},  // pass
  // TODO {"acosf", (uintptr_t)&stub_acosf},  // <<< IMPLEMENTAR
  {"asin", (uintptr_t)&asin},  // pass
  {"atan", (uintptr_t)&atan},  // pass
  {"atan2", (uintptr_t)&atan2},  // pass
  {"atan2f", (uintptr_t)&atan2f},  // pass
  {"atoi", (uintptr_t)&atoi},  // pass
  {"atol", (uintptr_t)&atol},  // pass
  // TODO {"bind", (uintptr_t)&stub_bind},  // <<< IMPLEMENTAR
  {"bsearch", (uintptr_t)&bsearch},  // pass
  // TODO {"btowc", (uintptr_t)&stub_btowc},  // <<< IMPLEMENTAR
  {"calloc", (uintptr_t)&calloc},  // pass
  // TODO {"clock", (uintptr_t)&stub_clock},  // <<< IMPLEMENTAR
  // TODO {"clock_getres", (uintptr_t)&stub_clock_getres},  // <<< IMPLEMENTAR
  {"clock_gettime", (uintptr_t)&clock_gettime},  // pass
  {"close", (uintptr_t)&close},  // pass
  {"closedir", (uintptr_t)&closedir},  // pass
  // TODO {"connect", (uintptr_t)&stub_connect},  // <<< IMPLEMENTAR
  {"cos", (uintptr_t)&cos},  // pass
  {"cosf", (uintptr_t)&cosf},  // pass
  // TODO {"_ctype_", (uintptr_t)&stub__ctype_},  // <<< IMPLEMENTAR
  // TODO {"__ctype_get_mb_cur_max", (uintptr_t)&stub___ctype_get_mb_cur_max},  // <<< IMPLEMENTAR
  {"__cxa_atexit", (uintptr_t)&__cxa_atexit},  // cxx
  {"__cxa_finalize", (uintptr_t)&__cxa_finalize},  // cxx
  // TODO {"difftime", (uintptr_t)&stub_difftime},  // <<< IMPLEMENTAR
  // TODO {"dladdr", (uintptr_t)&stub_dladdr},  // <<< IMPLEMENTAR
  // TODO {"dlclose", (uintptr_t)&stub_dlclose},  // <<< IMPLEMENTAR
  // TODO {"dl_iterate_phdr", (uintptr_t)&stub_dl_iterate_phdr},  // <<< IMPLEMENTAR
  // TODO {"dlopen", (uintptr_t)&stub_dlopen},  // <<< IMPLEMENTAR
  // TODO {"dlsym", (uintptr_t)&stub_dlsym},  // <<< IMPLEMENTAR
  {"__errno", (uintptr_t)&bionic_errno},  // pass
  {"exit", (uintptr_t)&exit},  // pass
  {"exp", (uintptr_t)&exp},  // pass
  // TODO {"exp2f", (uintptr_t)&stub_exp2f},  // <<< IMPLEMENTAR
  {"expf", (uintptr_t)&expf},  // pass
  {"fclose", (uintptr_t)&fclose},  // pass
  {"fcntl", (uintptr_t)&fcntl},  // pass
  {"fileno", (uintptr_t)&fileno},  // pass
  {"fmod", (uintptr_t)&fmod},  // pass
  {"fmodf", (uintptr_t)&fmodf},  // pass
  {"fopen", (uintptr_t)&fopen},  // pass
  {"fputc", (uintptr_t)&fputc},  // pass
  {"fputs", (uintptr_t)&fputs},  // pass
  {"free", (uintptr_t)&free},  // pass
  // TODO {"freeaddrinfo", (uintptr_t)&stub_freeaddrinfo},  // <<< IMPLEMENTAR
  // TODO {"fscanf", (uintptr_t)&stub_fscanf},  // <<< IMPLEMENTAR
  {"fstat", (uintptr_t)&fstat},  // pass
  // TODO {"ftruncate", (uintptr_t)&stub_ftruncate},  // <<< IMPLEMENTAR
  {"fwrite", (uintptr_t)&fwrite},  // pass
  // TODO {"getaddrinfo", (uintptr_t)&stub_getaddrinfo},  // <<< IMPLEMENTAR
  {"getcwd", (uintptr_t)&getcwd},  // pass
  {"getenv", (uintptr_t)&getenv},  // pass
  // TODO {"gethostname", (uintptr_t)&stub_gethostname},  // <<< IMPLEMENTAR
  {"getpid", (uintptr_t)&getpid},  // pass
  // TODO {"getsockname", (uintptr_t)&stub_getsockname},  // <<< IMPLEMENTAR
  // TODO {"getsockopt", (uintptr_t)&stub_getsockopt},  // <<< IMPLEMENTAR
  {"gettid", (uintptr_t)&gettid},  // pass
  {"gettimeofday", (uintptr_t)&gettimeofday},  // pass
  {"gmtime", (uintptr_t)&gmtime},  // pass
  // TODO {"__google_potentially_blocking_region_begin", (uintptr_t)&stub___google_potentially_blocking_region_begin},  // <<< IMPLEMENTAR
  // TODO {"__google_potentially_blocking_region_end", (uintptr_t)&stub___google_potentially_blocking_region_end},  // <<< IMPLEMENTAR
  // TODO {"inet_ntop", (uintptr_t)&stub_inet_ntop},  // <<< IMPLEMENTAR
  {"ioctl", (uintptr_t)&ioctl},  // pass
  {"isalpha", (uintptr_t)&isalpha},  // pass
  {"isspace", (uintptr_t)&isspace},  // pass
  // TODO {"iswctype", (uintptr_t)&stub_iswctype},  // <<< IMPLEMENTAR
  // TODO {"ldexpf", (uintptr_t)&stub_ldexpf},  // <<< IMPLEMENTAR
  // TODO {"listen", (uintptr_t)&stub_listen},  // <<< IMPLEMENTAR
  {"localtime", (uintptr_t)&localtime},  // pass
  {"log", (uintptr_t)&log},  // pass
  {"log10", (uintptr_t)&log10},  // pass
  {"logf", (uintptr_t)&logf},  // pass
  {"lseek", (uintptr_t)&lseek},  // pass
  {"lstat", (uintptr_t)&lstat},  // pass
  {"malloc", (uintptr_t)&malloc},  // pass
  // TODO {"mbrtowc", (uintptr_t)&stub_mbrtowc},  // <<< IMPLEMENTAR
  {"memalign", (uintptr_t)&memalign},  // pass
  {"memchr", (uintptr_t)&memchr},  // pass
  {"memcmp", (uintptr_t)&memcmp},  // pass
  {"memcpy", (uintptr_t)&memcpy},  // pass
  {"memmove", (uintptr_t)&memmove},  // pass
  {"memset", (uintptr_t)&memset},  // pass
  {"mkdir", (uintptr_t)&mkdir},  // pass
  {"mktime", (uintptr_t)&mktime},  // pass
  {"mmap", (uintptr_t)&mmap},  // pass
  {"modf", (uintptr_t)&modf},  // pass
  {"mprotect", (uintptr_t)&mprotect},  // pass
  {"munmap", (uintptr_t)&munmap},  // pass
  {"nanosleep", (uintptr_t)&nanosleep},  // pass
  {"open", (uintptr_t)&open},  // pass
  {"opendir", (uintptr_t)&opendir},  // pass
  {"pipe", (uintptr_t)&pipe},  // pass
  // TODO {"poll", (uintptr_t)&stub_poll},  // <<< IMPLEMENTAR
  {"pow", (uintptr_t)&pow},  // pass
  {"powf", (uintptr_t)&powf},  // pass
  {"pthread_attr_destroy", (uintptr_t)&pthread_attr_destroy_fake},  // pthread wrapper (core)
  {"pthread_attr_getstack", (uintptr_t)&pthread_attr_getstack_fake},  // pthread wrapper (core)
  {"pthread_attr_init", (uintptr_t)&pthread_attr_init_fake},  // pthread wrapper (core)
  {"pthread_condattr_destroy", (uintptr_t)&pthread_condattr_destroy_fake},  // pthread wrapper (core)
  {"pthread_condattr_init", (uintptr_t)&pthread_condattr_init_fake},  // pthread wrapper (core)
  {"pthread_condattr_setclock", (uintptr_t)&pthread_condattr_setclock_fake},  // pthread wrapper (core)
  {"pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake},  // pthread wrapper (core)
  {"pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_fake},  // pthread wrapper (core)
  {"pthread_cond_init", (uintptr_t)&pthread_cond_init_fake},  // pthread wrapper (core)
  {"pthread_cond_signal", (uintptr_t)&pthread_cond_signal_fake},  // pthread wrapper (core)
  {"pthread_cond_timedwait", (uintptr_t)&pthread_cond_timedwait_fake},  // pthread wrapper (core)
  {"pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake},  // pthread wrapper (core)
  {"pthread_create", (uintptr_t)&pthread_create_fake},  // pthread wrapper (core)
  {"pthread_detach", (uintptr_t)&pthread_detach_fake},  // pthread wrapper (core)
  {"pthread_getattr_np", (uintptr_t)&pthread_getattr_np_fake},  // pthread wrapper (core)
  {"pthread_getspecific", (uintptr_t)&pthread_getspecific_fake},  // pthread wrapper (core)
  {"pthread_key_create", (uintptr_t)&pthread_key_create_fake},  // pthread wrapper (core)
  {"pthread_key_delete", (uintptr_t)&pthread_key_delete_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_destroy", (uintptr_t)&pthread_mutexattr_destroy_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_init", (uintptr_t)&pthread_mutexattr_init_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_settype", (uintptr_t)&pthread_mutexattr_settype_fake},  // pthread wrapper (core)
  {"pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake},  // pthread wrapper (core)
  {"pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake},  // pthread wrapper (core)
  {"pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake},  // pthread wrapper (core)
  {"pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake},  // pthread wrapper (core)
  {"pthread_once", (uintptr_t)&pthread_once_fake},  // pthread wrapper (core)
  {"pthread_self", (uintptr_t)&pthread_self_fake},  // pthread wrapper (core)
  {"pthread_setspecific", (uintptr_t)&pthread_setspecific_fake},  // pthread wrapper (core)
  {"read", (uintptr_t)&read},  // pass
  {"readdir", (uintptr_t)&readdir},  // pass
  // TODO {"readlink", (uintptr_t)&stub_readlink},  // <<< IMPLEMENTAR
  {"realloc", (uintptr_t)&realloc},  // pass
  // TODO {"recvfrom", (uintptr_t)&stub_recvfrom},  // <<< IMPLEMENTAR
  {"sched_yield", (uintptr_t)&sched_yield},  // pass
  {"sem_getvalue", (uintptr_t)&sem_getvalue_fake},  // pthread wrapper (core)
  {"sem_init", (uintptr_t)&sem_init_fake},  // pthread wrapper (core)
  {"sem_post", (uintptr_t)&sem_post_fake},  // pthread wrapper (core)
  {"sem_wait", (uintptr_t)&sem_wait_fake},  // pthread wrapper (core)
  // TODO {"send", (uintptr_t)&stub_send},  // <<< IMPLEMENTAR
  // TODO {"sendto", (uintptr_t)&stub_sendto},  // <<< IMPLEMENTAR
  {"setenv", (uintptr_t)&setenv},  // pass
  // TODO {"setjmp", (uintptr_t)&stub_setjmp},  // <<< IMPLEMENTAR
  {"setlocale", (uintptr_t)&setlocale},  // pass
  // TODO {"setsockopt", (uintptr_t)&stub_setsockopt},  // <<< IMPLEMENTAR
  // TODO {"__sF", (uintptr_t)&stub___sF},  // <<< IMPLEMENTAR
  // TODO {"shutdown", (uintptr_t)&stub_shutdown},  // <<< IMPLEMENTAR
  // TODO {"sigaction", (uintptr_t)&stub_sigaction},  // <<< IMPLEMENTAR
  // TODO {"sigdelset", (uintptr_t)&stub_sigdelset},  // <<< IMPLEMENTAR
  // TODO {"sigfillset", (uintptr_t)&stub_sigfillset},  // <<< IMPLEMENTAR
  // TODO {"signal", (uintptr_t)&stub_signal},  // <<< IMPLEMENTAR
  // TODO {"sigsuspend", (uintptr_t)&stub_sigsuspend},  // <<< IMPLEMENTAR
  {"sin", (uintptr_t)&sin},  // pass
  {"sinf", (uintptr_t)&sinf},  // pass
  // TODO {"socket", (uintptr_t)&stub_socket},  // <<< IMPLEMENTAR
  {"sprintf", (uintptr_t)&sprintf},  // pass
  {"sqrt", (uintptr_t)&sqrt},  // pass
  {"sqrtf", (uintptr_t)&sqrtf},  // pass
  {"stat", (uintptr_t)&stat},  // pass
  {"strchr", (uintptr_t)&strchr},  // pass
  {"strcmp", (uintptr_t)&strcmp},  // pass
  {"strcoll", (uintptr_t)&strcoll},  // pass
  {"strcpy", (uintptr_t)&strcpy},  // pass
  {"strerror", (uintptr_t)&strerror},  // pass
  {"strftime", (uintptr_t)&strftime},  // pass
  // TODO {"strlcpy", (uintptr_t)&stub_strlcpy},  // <<< IMPLEMENTAR
  {"strlen", (uintptr_t)&strlen},  // pass
  {"strncmp", (uintptr_t)&strncmp},  // pass
  {"strncpy", (uintptr_t)&strncpy},  // pass
  {"strrchr", (uintptr_t)&strrchr},  // pass
  {"strtod", (uintptr_t)&strtod},  // pass
  {"strtof", (uintptr_t)&strtof},  // pass
  {"strtol", (uintptr_t)&strtol},  // pass
  // TODO {"strtold", (uintptr_t)&stub_strtold},  // <<< IMPLEMENTAR
  {"strtoul", (uintptr_t)&strtoul},  // pass
  // TODO {"strxfrm", (uintptr_t)&stub_strxfrm},  // <<< IMPLEMENTAR
  // TODO {"syscall", (uintptr_t)&stub_syscall},  // <<< IMPLEMENTAR
  {"sysconf", (uintptr_t)&sysconf},  // pass
  {"tan", (uintptr_t)&tan},  // pass
  {"tanf", (uintptr_t)&tanf},  // pass
  // TODO {"tgkill", (uintptr_t)&stub_tgkill},  // <<< IMPLEMENTAR
  {"time", (uintptr_t)&time},  // pass
  {"tolower", (uintptr_t)&tolower},  // pass
  // TODO {"towlower", (uintptr_t)&stub_towlower},  // <<< IMPLEMENTAR
  // TODO {"towupper", (uintptr_t)&stub_towupper},  // <<< IMPLEMENTAR
  // TODO {"uname", (uintptr_t)&stub_uname},  // <<< IMPLEMENTAR
  {"unlink", (uintptr_t)&unlink},  // pass
  // TODO {"unsetenv", (uintptr_t)&stub_unsetenv},  // <<< IMPLEMENTAR
  {"usleep", (uintptr_t)&usleep},  // pass
  {"vsnprintf", (uintptr_t)&vsnprintf},  // pass
  {"vsprintf", (uintptr_t)&vsprintf},  // pass
  // TODO {"wcrtomb", (uintptr_t)&stub_wcrtomb},  // <<< IMPLEMENTAR
  // TODO {"wcscoll", (uintptr_t)&stub_wcscoll},  // <<< IMPLEMENTAR
  // TODO {"wcsftime", (uintptr_t)&stub_wcsftime},  // <<< IMPLEMENTAR
  // TODO {"wcslen", (uintptr_t)&stub_wcslen},  // <<< IMPLEMENTAR
  // TODO {"wcsxfrm", (uintptr_t)&stub_wcsxfrm},  // <<< IMPLEMENTAR
  // TODO {"wctob", (uintptr_t)&stub_wctob},  // <<< IMPLEMENTAR
  // TODO {"wctype", (uintptr_t)&stub_wctype},  // <<< IMPLEMENTAR
  // TODO {"wmemchr", (uintptr_t)&stub_wmemchr},  // <<< IMPLEMENTAR
  // TODO {"wmemcpy", (uintptr_t)&stub_wmemcpy},  // <<< IMPLEMENTAR
  // TODO {"wmemmove", (uintptr_t)&stub_wmemmove},  // <<< IMPLEMENTAR
  // TODO {"wmemset", (uintptr_t)&stub_wmemset},  // <<< IMPLEMENTAR
  {"write", (uintptr_t)&write},  // pass
  // TODO {"writev", (uintptr_t)&stub_writev},  // <<< IMPLEMENTAR
};
const int dynlib_functions_count = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);
size_t dynlib_numfunctions = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);

// ===================== SIMBOLOS A IMPLEMENTAR =====================
//   accept
//   acosf
//   bind
//   btowc
//   clock
//   clock_getres
//   connect
//   _ctype_
//   __ctype_get_mb_cur_max
//   difftime
//   dladdr
//   dlclose
//   dl_iterate_phdr
//   dlopen
//   dlsym
//   exp2f
//   freeaddrinfo
//   fscanf
//   ftruncate
//   getaddrinfo
//   gethostname
//   getsockname
//   getsockopt
//   __google_potentially_blocking_region_begin
//   __google_potentially_blocking_region_end
//   inet_ntop
//   iswctype
//   ldexpf
//   listen
//   mbrtowc
//   poll
//   readlink
//   recvfrom
//   send
//   sendto
//   setjmp
//   setsockopt
//   __sF
//   shutdown
//   sigaction
//   sigdelset
//   sigfillset
//   signal
//   sigsuspend
//   socket
//   strlcpy
//   strtold
//   strxfrm
//   syscall
//   tgkill
//   towlower
//   towupper
//   uname
//   unsetenv
//   wcrtomb
//   wcscoll
//   wcsftime
//   wcslen
//   wcsxfrm
//   wctob
//   wctype
//   wmemchr
//   wmemcpy
//   wmemmove
//   wmemset
//   writev
