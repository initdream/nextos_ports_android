// imports_unity.gen.c — GERADO. passthrough via dlsym; resto = stub log.
#include "so_util.h"
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>

// stubs (logam nome, 1as 2 vezes, retornam 0)
long stub_accept(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] accept\\n"); return 0; }
long stub_ALooper_acquire(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ALooper_acquire\\n"); return 0; }
long stub_ALooper_forThread(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ALooper_forThread\\n"); return 0; }
long stub_ALooper_pollAll(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ALooper_pollAll\\n"); return 0; }
long stub_ALooper_prepare(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ALooper_prepare\\n"); return 0; }
long stub_ALooper_release(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ALooper_release\\n"); return 0; }
long stub_ALooper_wake(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ALooper_wake\\n"); return 0; }
long stub_ANativeWindow_acquire(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ANativeWindow_acquire\\n"); return 0; }
long stub_ANativeWindow_fromSurface(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ANativeWindow_fromSurface\\n"); return 0; }
long stub_ANativeWindow_getHeight(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ANativeWindow_getHeight\\n"); return 0; }
long stub_ANativeWindow_getWidth(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ANativeWindow_getWidth\\n"); return 0; }
long stub_ANativeWindow_release(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ANativeWindow_release\\n"); return 0; }
long stub_ANativeWindow_setBuffersGeometry(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ANativeWindow_setBuffersGeometry\\n"); return 0; }
long stub___android_log_print(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __android_log_print\\n"); return 0; }
long stub___android_log_vprint(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __android_log_vprint\\n"); return 0; }
long stub___android_log_write(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __android_log_write\\n"); return 0; }
long stub_android_set_abort_message(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] android_set_abort_message\\n"); return 0; }
long stub_ASensorEventQueue_disableSensor(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensorEventQueue_disableSensor\\n"); return 0; }
long stub_ASensorEventQueue_enableSensor(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensorEventQueue_enableSensor\\n"); return 0; }
long stub_ASensorEventQueue_getEvents(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensorEventQueue_getEvents\\n"); return 0; }
long stub_ASensorEventQueue_hasEvents(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensorEventQueue_hasEvents\\n"); return 0; }
long stub_ASensorEventQueue_setEventRate(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensorEventQueue_setEventRate\\n"); return 0; }
long stub_ASensor_getMinDelay(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensor_getMinDelay\\n"); return 0; }
long stub_ASensor_getName(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensor_getName\\n"); return 0; }
long stub_ASensor_getResolution(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensor_getResolution\\n"); return 0; }
long stub_ASensor_getType(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensor_getType\\n"); return 0; }
long stub_ASensor_getVendor(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensor_getVendor\\n"); return 0; }
long stub_ASensorManager_createEventQueue(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensorManager_createEventQueue\\n"); return 0; }
long stub_ASensorManager_destroyEventQueue(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensorManager_destroyEventQueue\\n"); return 0; }
long stub_ASensorManager_getDefaultSensor(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensorManager_getDefaultSensor\\n"); return 0; }
long stub_ASensorManager_getInstance(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensorManager_getInstance\\n"); return 0; }
long stub_ASensorManager_getSensorList(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ASensorManager_getSensorList\\n"); return 0; }
long stub_basename(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] basename\\n"); return 0; }
long stub_bind(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] bind\\n"); return 0; }
long stub_chmod(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] chmod\\n"); return 0; }
long stub_closelog(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] closelog\\n"); return 0; }
long stub_connect(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] connect\\n"); return 0; }
long stub___ctype_get_mb_cur_max(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __ctype_get_mb_cur_max\\n"); return 0; }
long stub___cxa_atexit(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __cxa_atexit\\n"); return 0; }
long stub___cxa_finalize(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __cxa_finalize\\n"); return 0; }
long stub_dladdr(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] dladdr\\n"); return 0; }
long stub_dl_iterate_phdr(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] dl_iterate_phdr\\n"); return 0; }
long stub_eglChooseConfig(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglChooseConfig\\n"); return 0; }
long stub_eglCreateContext(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglCreateContext\\n"); return 0; }
long stub_eglCreatePbufferSurface(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglCreatePbufferSurface\\n"); return 0; }
long stub_eglCreateWindowSurface(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglCreateWindowSurface\\n"); return 0; }
long stub_eglDestroyContext(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglDestroyContext\\n"); return 0; }
long stub_eglDestroySurface(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglDestroySurface\\n"); return 0; }
long stub_eglGetConfigAttrib(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglGetConfigAttrib\\n"); return 0; }
long stub_eglGetCurrentContext(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglGetCurrentContext\\n"); return 0; }
long stub_eglGetCurrentSurface(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglGetCurrentSurface\\n"); return 0; }
long stub_eglGetDisplay(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglGetDisplay\\n"); return 0; }
long stub_eglGetError(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglGetError\\n"); return 0; }
long stub_eglGetProcAddress(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglGetProcAddress\\n"); return 0; }
long stub_eglInitialize(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglInitialize\\n"); return 0; }
long stub_eglMakeCurrent(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglMakeCurrent\\n"); return 0; }
long stub_eglQueryString(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglQueryString\\n"); return 0; }
long stub_eglQuerySurface(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglQuerySurface\\n"); return 0; }
long stub_eglSurfaceAttrib(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglSurfaceAttrib\\n"); return 0; }
long stub_eglSwapBuffers(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglSwapBuffers\\n"); return 0; }
long stub_eglSwapInterval(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglSwapInterval\\n"); return 0; }
long stub_eglTerminate(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] eglTerminate\\n"); return 0; }
long stub___errno(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __errno\\n"); return 0; }
long stub_execl(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] execl\\n"); return 0; }
long stub_exp2f(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] exp2f\\n"); return 0; }
long stub___FD_ISSET_chk(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __FD_ISSET_chk\\n"); return 0; }
long stub___FD_SET_chk(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __FD_SET_chk\\n"); return 0; }
long stub_flock(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] flock\\n"); return 0; }
long stub_fork(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] fork\\n"); return 0; }
long stub_freeaddrinfo(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] freeaddrinfo\\n"); return 0; }
long stub_freelocale(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] freelocale\\n"); return 0; }
long stub_frexpf(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] frexpf\\n"); return 0; }
long stub_fscanf(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] fscanf\\n"); return 0; }
long stub_gai_strerror(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] gai_strerror\\n"); return 0; }
long stub_getaddrinfo(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] getaddrinfo\\n"); return 0; }
long stub_geteuid(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] geteuid\\n"); return 0; }
long stub_gethostbyaddr(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] gethostbyaddr\\n"); return 0; }
long stub_gethostbyname(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] gethostbyname\\n"); return 0; }
long stub_getnameinfo(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] getnameinfo\\n"); return 0; }
long stub_getpeername(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] getpeername\\n"); return 0; }
long stub_getpriority(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] getpriority\\n"); return 0; }
long stub_getpwuid(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] getpwuid\\n"); return 0; }
long stub_getpwuid_r(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] getpwuid_r\\n"); return 0; }
long stub_getsockname(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] getsockname\\n"); return 0; }
long stub_getsockopt(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] getsockopt\\n"); return 0; }
long stub_gettid(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] gettid\\n"); return 0; }
long stub_if_nametoindex(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] if_nametoindex\\n"); return 0; }
long stub_inet_addr(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] inet_addr\\n"); return 0; }
long stub_inet_ntop(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] inet_ntop\\n"); return 0; }
long stub_inet_pton(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] inet_pton\\n"); return 0; }
long stub_inflate(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] inflate\\n"); return 0; }
long stub_inflateEnd(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] inflateEnd\\n"); return 0; }
long stub_inflateInit_(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] inflateInit_\\n"); return 0; }
long stub_inflateInit2_(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] inflateInit2_\\n"); return 0; }
long stub_iswalpha(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] iswalpha\\n"); return 0; }
long stub_iswblank(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] iswblank\\n"); return 0; }
long stub_iswcntrl(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] iswcntrl\\n"); return 0; }
long stub_iswdigit(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] iswdigit\\n"); return 0; }
long stub_iswlower(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] iswlower\\n"); return 0; }
long stub_iswprint(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] iswprint\\n"); return 0; }
long stub_iswpunct(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] iswpunct\\n"); return 0; }
long stub_iswspace(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] iswspace\\n"); return 0; }
long stub_iswupper(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] iswupper\\n"); return 0; }
long stub_iswxdigit(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] iswxdigit\\n"); return 0; }
long stub_kill(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] kill\\n"); return 0; }
long stub_ldexpf(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] ldexpf\\n"); return 0; }
long stub_listen(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] listen\\n"); return 0; }
long stub_lldiv(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] lldiv\\n"); return 0; }
long stub_longjmp(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] longjmp\\n"); return 0; }
long stub_mbrlen(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] mbrlen\\n"); return 0; }
long stub_mbrtowc(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] mbrtowc\\n"); return 0; }
long stub_mbsnrtowcs(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] mbsnrtowcs\\n"); return 0; }
long stub_mbsrtowcs(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] mbsrtowcs\\n"); return 0; }
long stub_mbtowc(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] mbtowc\\n"); return 0; }
long stub_memalign(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] memalign\\n"); return 0; }
long stub_modff(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] modff\\n"); return 0; }
long stub_nearbyintf(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] nearbyintf\\n"); return 0; }
long stub_newlocale(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] newlocale\\n"); return 0; }
long stub_openlog(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] openlog\\n"); return 0; }
long stub_prctl(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] prctl\\n"); return 0; }
long stub_raise(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] raise\\n"); return 0; }
long stub_recv(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] recv\\n"); return 0; }
long stub_recvfrom(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] recvfrom\\n"); return 0; }
long stub_remainderf(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] remainderf\\n"); return 0; }
long stub_sched_getaffinity(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] sched_getaffinity\\n"); return 0; }
long stub_sched_setaffinity(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] sched_setaffinity\\n"); return 0; }
long stub_send(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] send\\n"); return 0; }
long stub_sendto(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] sendto\\n"); return 0; }
long stub_setjmp(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] setjmp\\n"); return 0; }
long stub_setpriority(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] setpriority\\n"); return 0; }
long stub_setsockopt(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] setsockopt\\n"); return 0; }
long stub___sF(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __sF\\n"); return 0; }
long stub_shutdown(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] shutdown\\n"); return 0; }
long stub_sigaction(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] sigaction\\n"); return 0; }
long stub_sigaltstack(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] sigaltstack\\n"); return 0; }
long stub_sigemptyset(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] sigemptyset\\n"); return 0; }
long stub_socket(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] socket\\n"); return 0; }
long stub___stack_chk_fail(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __stack_chk_fail\\n"); return 0; }
long stub_statfs(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] statfs\\n"); return 0; }
long stub_strerror_r(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] strerror_r\\n"); return 0; }
long stub_strnlen(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] strnlen\\n"); return 0; }
long stub_strtok_r(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] strtok_r\\n"); return 0; }
long stub_strtold_l(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] strtold_l\\n"); return 0; }
long stub_strtoll_l(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] strtoll_l\\n"); return 0; }
long stub_strtoull_l(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] strtoull_l\\n"); return 0; }
long stub_strxfrm(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] strxfrm\\n"); return 0; }
long stub_syscall(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] syscall\\n"); return 0; }
long stub_syslog(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] syslog\\n"); return 0; }
long stub___system_property_find(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __system_property_find\\n"); return 0; }
long stub___system_property_get(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __system_property_get\\n"); return 0; }
long stub___system_property_read(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] __system_property_read\\n"); return 0; }
long stub_towlower(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] towlower\\n"); return 0; }
long stub_towupper(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] towupper\\n"); return 0; }
long stub_uselocale(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] uselocale\\n"); return 0; }
long stub_utime(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] utime\\n"); return 0; }
long stub_vasprintf(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] vasprintf\\n"); return 0; }
long stub_vsscanf(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] vsscanf\\n"); return 0; }
long stub_waitpid(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] waitpid\\n"); return 0; }
long stub_wcrtomb(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] wcrtomb\\n"); return 0; }
long stub_wcscoll(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] wcscoll\\n"); return 0; }
long stub_wcslen(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] wcslen\\n"); return 0; }
long stub_wcsnrtombs(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] wcsnrtombs\\n"); return 0; }
long stub_wcsxfrm(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] wcsxfrm\\n"); return 0; }
long stub_wmemcpy(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] wmemcpy\\n"); return 0; }
long stub_wmemmove(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] wmemmove\\n"); return 0; }
long stub_wmemset(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] wmemset\\n"); return 0; }
long stub_zlibVersion(void){ static int n=0; if(n++<2) fprintf(stderr,"[STUB] zlibVersion\\n"); return 0; }

// flag: 1 = passthrough (resolver via dlsym), 0 = stub ja setado
static const char *passthrough_names[] = {
  "access",
  "acos",
  "acosf",
  "asinf",
  "atan2",
  "atan2f",
  "atanf",
  "atoi",
  "atol",
  "calloc",
  "clearerr",
  "clock",
  "clock_gettime",
  "close",
  "closedir",
  "cos",
  "cosf",
  "difftime",
  "dlclose",
  "dlerror",
  "dlopen",
  "dlsym",
  "dup",
  "dup2",
  "_exit",
  "exit",
  "exp",
  "expf",
  "fclose",
  "fcntl",
  "feof",
  "ferror",
  "fflush",
  "fgets",
  "fmod",
  "fmodf",
  "fopen",
  "fprintf",
  "fputc",
  "fputs",
  "fread",
  "free",
  "fseek",
  "fseeko",
  "fstat",
  "ftell",
  "ftello",
  "fwrite",
  "getenv",
  "getpagesize",
  "getpid",
  "gettimeofday",
  "getuid",
  "gmtime",
  "gmtime_r",
  "ioctl",
  "isalnum",
  "isalpha",
  "isgraph",
  "islower",
  "isprint",
  "isspace",
  "isupper",
  "isxdigit",
  "ldexp",
  "localtime",
  "localtime_r",
  "log",
  "log10f",
  "log2f",
  "logf",
  "lseek",
  "lseek64",
  "lstat",
  "madvise",
  "malloc",
  "memchr",
  "memcmp",
  "memcpy",
  "memmove",
  "memrchr",
  "memset",
  "mkdir",
  "mktime",
  "mmap",
  "modf",
  "mprotect",
  "munmap",
  "nanosleep",
  "open",
  "opendir",
  "perror",
  "pipe",
  "poll",
  "pow",
  "powf",
  "printf",
  "pthread_attr_destroy",
  "pthread_attr_init",
  "pthread_attr_setdetachstate",
  "pthread_attr_setstacksize",
  "pthread_condattr_destroy",
  "pthread_condattr_init",
  "pthread_condattr_setclock",
  "pthread_cond_broadcast",
  "pthread_cond_destroy",
  "pthread_cond_init",
  "pthread_cond_signal",
  "pthread_cond_timedwait",
  "pthread_cond_wait",
  "pthread_create",
  "pthread_detach",
  "pthread_equal",
  "pthread_exit",
  "pthread_getspecific",
  "pthread_join",
  "pthread_key_create",
  "pthread_key_delete",
  "pthread_mutexattr_destroy",
  "pthread_mutexattr_init",
  "pthread_mutexattr_settype",
  "pthread_mutex_destroy",
  "pthread_mutex_init",
  "pthread_mutex_lock",
  "pthread_mutex_trylock",
  "pthread_mutex_unlock",
  "pthread_once",
  "pthread_self",
  "pthread_setname_np",
  "pthread_setspecific",
  "puts",
  "qsort",
  "read",
  "readdir",
  "realloc",
  "realpath",
  "remove",
  "rename",
  "rmdir",
  "sched_yield",
  "select",
  "sem_destroy",
  "sem_init",
  "sem_post",
  "sem_wait",
  "setvbuf",
  "sin",
  "sinf",
  "snprintf",
  "sprintf",
  "sqrt",
  "sqrtf",
  "sscanf",
  "stat",
  "strcasecmp",
  "strcat",
  "strchr",
  "strcmp",
  "strcoll",
  "strcpy",
  "strdup",
  "strerror",
  "strftime",
  "strlen",
  "strncmp",
  "strncpy",
  "strpbrk",
  "strrchr",
  "strspn",
  "strstr",
  "strtod",
  "strtof",
  "strtol",
  "strtoul",
  "strtoull",
  "sysconf",
  "tan",
  "tanf",
  "time",
  "tolower",
  "toupper",
  "truncate",
  "unlink",
  "usleep",
  "vfprintf",
  "vprintf",
  "vsnprintf",
  "write",
  0 };

DynLibFunction dynlib_functions[] = {
  {"accept", (uintptr_t)&stub_accept},
  {"access", 0},
  {"acos", 0},
  {"acosf", 0},
  {"ALooper_acquire", (uintptr_t)&stub_ALooper_acquire},
  {"ALooper_forThread", (uintptr_t)&stub_ALooper_forThread},
  {"ALooper_pollAll", (uintptr_t)&stub_ALooper_pollAll},
  {"ALooper_prepare", (uintptr_t)&stub_ALooper_prepare},
  {"ALooper_release", (uintptr_t)&stub_ALooper_release},
  {"ALooper_wake", (uintptr_t)&stub_ALooper_wake},
  {"ANativeWindow_acquire", (uintptr_t)&stub_ANativeWindow_acquire},
  {"ANativeWindow_fromSurface", (uintptr_t)&stub_ANativeWindow_fromSurface},
  {"ANativeWindow_getHeight", (uintptr_t)&stub_ANativeWindow_getHeight},
  {"ANativeWindow_getWidth", (uintptr_t)&stub_ANativeWindow_getWidth},
  {"ANativeWindow_release", (uintptr_t)&stub_ANativeWindow_release},
  {"ANativeWindow_setBuffersGeometry", (uintptr_t)&stub_ANativeWindow_setBuffersGeometry},
  {"__android_log_print", (uintptr_t)&stub___android_log_print},
  {"__android_log_vprint", (uintptr_t)&stub___android_log_vprint},
  {"__android_log_write", (uintptr_t)&stub___android_log_write},
  {"android_set_abort_message", (uintptr_t)&stub_android_set_abort_message},
  {"ASensorEventQueue_disableSensor", (uintptr_t)&stub_ASensorEventQueue_disableSensor},
  {"ASensorEventQueue_enableSensor", (uintptr_t)&stub_ASensorEventQueue_enableSensor},
  {"ASensorEventQueue_getEvents", (uintptr_t)&stub_ASensorEventQueue_getEvents},
  {"ASensorEventQueue_hasEvents", (uintptr_t)&stub_ASensorEventQueue_hasEvents},
  {"ASensorEventQueue_setEventRate", (uintptr_t)&stub_ASensorEventQueue_setEventRate},
  {"ASensor_getMinDelay", (uintptr_t)&stub_ASensor_getMinDelay},
  {"ASensor_getName", (uintptr_t)&stub_ASensor_getName},
  {"ASensor_getResolution", (uintptr_t)&stub_ASensor_getResolution},
  {"ASensor_getType", (uintptr_t)&stub_ASensor_getType},
  {"ASensor_getVendor", (uintptr_t)&stub_ASensor_getVendor},
  {"ASensorManager_createEventQueue", (uintptr_t)&stub_ASensorManager_createEventQueue},
  {"ASensorManager_destroyEventQueue", (uintptr_t)&stub_ASensorManager_destroyEventQueue},
  {"ASensorManager_getDefaultSensor", (uintptr_t)&stub_ASensorManager_getDefaultSensor},
  {"ASensorManager_getInstance", (uintptr_t)&stub_ASensorManager_getInstance},
  {"ASensorManager_getSensorList", (uintptr_t)&stub_ASensorManager_getSensorList},
  {"asinf", 0},
  {"atan2", 0},
  {"atan2f", 0},
  {"atanf", 0},
  {"atoi", 0},
  {"atol", 0},
  {"basename", (uintptr_t)&stub_basename},
  {"bind", (uintptr_t)&stub_bind},
  {"calloc", 0},
  {"chmod", (uintptr_t)&stub_chmod},
  {"clearerr", 0},
  {"clock", 0},
  {"clock_gettime", 0},
  {"close", 0},
  {"closedir", 0},
  {"closelog", (uintptr_t)&stub_closelog},
  {"connect", (uintptr_t)&stub_connect},
  {"cos", 0},
  {"cosf", 0},
  {"__ctype_get_mb_cur_max", (uintptr_t)&stub___ctype_get_mb_cur_max},
  {"__cxa_atexit", (uintptr_t)&stub___cxa_atexit},
  {"__cxa_finalize", (uintptr_t)&stub___cxa_finalize},
  {"difftime", 0},
  {"dladdr", (uintptr_t)&stub_dladdr},
  {"dlclose", 0},
  {"dlerror", 0},
  {"dl_iterate_phdr", (uintptr_t)&stub_dl_iterate_phdr},
  {"dlopen", 0},
  {"dlsym", 0},
  {"dup", 0},
  {"dup2", 0},
  {"eglChooseConfig", (uintptr_t)&stub_eglChooseConfig},
  {"eglCreateContext", (uintptr_t)&stub_eglCreateContext},
  {"eglCreatePbufferSurface", (uintptr_t)&stub_eglCreatePbufferSurface},
  {"eglCreateWindowSurface", (uintptr_t)&stub_eglCreateWindowSurface},
  {"eglDestroyContext", (uintptr_t)&stub_eglDestroyContext},
  {"eglDestroySurface", (uintptr_t)&stub_eglDestroySurface},
  {"eglGetConfigAttrib", (uintptr_t)&stub_eglGetConfigAttrib},
  {"eglGetCurrentContext", (uintptr_t)&stub_eglGetCurrentContext},
  {"eglGetCurrentSurface", (uintptr_t)&stub_eglGetCurrentSurface},
  {"eglGetDisplay", (uintptr_t)&stub_eglGetDisplay},
  {"eglGetError", (uintptr_t)&stub_eglGetError},
  {"eglGetProcAddress", (uintptr_t)&stub_eglGetProcAddress},
  {"eglInitialize", (uintptr_t)&stub_eglInitialize},
  {"eglMakeCurrent", (uintptr_t)&stub_eglMakeCurrent},
  {"eglQueryString", (uintptr_t)&stub_eglQueryString},
  {"eglQuerySurface", (uintptr_t)&stub_eglQuerySurface},
  {"eglSurfaceAttrib", (uintptr_t)&stub_eglSurfaceAttrib},
  {"eglSwapBuffers", (uintptr_t)&stub_eglSwapBuffers},
  {"eglSwapInterval", (uintptr_t)&stub_eglSwapInterval},
  {"eglTerminate", (uintptr_t)&stub_eglTerminate},
  {"__errno", (uintptr_t)&stub___errno},
  {"execl", (uintptr_t)&stub_execl},
  {"_exit", 0},
  {"exit", 0},
  {"exp", 0},
  {"exp2f", (uintptr_t)&stub_exp2f},
  {"expf", 0},
  {"fclose", 0},
  {"fcntl", 0},
  {"__FD_ISSET_chk", (uintptr_t)&stub___FD_ISSET_chk},
  {"__FD_SET_chk", (uintptr_t)&stub___FD_SET_chk},
  {"feof", 0},
  {"ferror", 0},
  {"fflush", 0},
  {"fgets", 0},
  {"flock", (uintptr_t)&stub_flock},
  {"fmod", 0},
  {"fmodf", 0},
  {"fopen", 0},
  {"fork", (uintptr_t)&stub_fork},
  {"fprintf", 0},
  {"fputc", 0},
  {"fputs", 0},
  {"fread", 0},
  {"free", 0},
  {"freeaddrinfo", (uintptr_t)&stub_freeaddrinfo},
  {"freelocale", (uintptr_t)&stub_freelocale},
  {"frexpf", (uintptr_t)&stub_frexpf},
  {"fscanf", (uintptr_t)&stub_fscanf},
  {"fseek", 0},
  {"fseeko", 0},
  {"fstat", 0},
  {"ftell", 0},
  {"ftello", 0},
  {"fwrite", 0},
  {"gai_strerror", (uintptr_t)&stub_gai_strerror},
  {"getaddrinfo", (uintptr_t)&stub_getaddrinfo},
  {"getenv", 0},
  {"geteuid", (uintptr_t)&stub_geteuid},
  {"gethostbyaddr", (uintptr_t)&stub_gethostbyaddr},
  {"gethostbyname", (uintptr_t)&stub_gethostbyname},
  {"getnameinfo", (uintptr_t)&stub_getnameinfo},
  {"getpagesize", 0},
  {"getpeername", (uintptr_t)&stub_getpeername},
  {"getpid", 0},
  {"getpriority", (uintptr_t)&stub_getpriority},
  {"getpwuid", (uintptr_t)&stub_getpwuid},
  {"getpwuid_r", (uintptr_t)&stub_getpwuid_r},
  {"getsockname", (uintptr_t)&stub_getsockname},
  {"getsockopt", (uintptr_t)&stub_getsockopt},
  {"gettid", (uintptr_t)&stub_gettid},
  {"gettimeofday", 0},
  {"getuid", 0},
  {"gmtime", 0},
  {"gmtime_r", 0},
  {"if_nametoindex", (uintptr_t)&stub_if_nametoindex},
  {"inet_addr", (uintptr_t)&stub_inet_addr},
  {"inet_ntop", (uintptr_t)&stub_inet_ntop},
  {"inet_pton", (uintptr_t)&stub_inet_pton},
  {"inflate", (uintptr_t)&stub_inflate},
  {"inflateEnd", (uintptr_t)&stub_inflateEnd},
  {"inflateInit_", (uintptr_t)&stub_inflateInit_},
  {"inflateInit2_", (uintptr_t)&stub_inflateInit2_},
  {"ioctl", 0},
  {"isalnum", 0},
  {"isalpha", 0},
  {"isgraph", 0},
  {"islower", 0},
  {"isprint", 0},
  {"isspace", 0},
  {"isupper", 0},
  {"iswalpha", (uintptr_t)&stub_iswalpha},
  {"iswblank", (uintptr_t)&stub_iswblank},
  {"iswcntrl", (uintptr_t)&stub_iswcntrl},
  {"iswdigit", (uintptr_t)&stub_iswdigit},
  {"iswlower", (uintptr_t)&stub_iswlower},
  {"iswprint", (uintptr_t)&stub_iswprint},
  {"iswpunct", (uintptr_t)&stub_iswpunct},
  {"iswspace", (uintptr_t)&stub_iswspace},
  {"iswupper", (uintptr_t)&stub_iswupper},
  {"iswxdigit", (uintptr_t)&stub_iswxdigit},
  {"isxdigit", 0},
  {"kill", (uintptr_t)&stub_kill},
  {"ldexp", 0},
  {"ldexpf", (uintptr_t)&stub_ldexpf},
  {"listen", (uintptr_t)&stub_listen},
  {"lldiv", (uintptr_t)&stub_lldiv},
  {"localtime", 0},
  {"localtime_r", 0},
  {"log", 0},
  {"log10f", 0},
  {"log2f", 0},
  {"logf", 0},
  {"longjmp", (uintptr_t)&stub_longjmp},
  {"lseek", 0},
  {"lseek64", 0},
  {"lstat", 0},
  {"madvise", 0},
  {"malloc", 0},
  {"mbrlen", (uintptr_t)&stub_mbrlen},
  {"mbrtowc", (uintptr_t)&stub_mbrtowc},
  {"mbsnrtowcs", (uintptr_t)&stub_mbsnrtowcs},
  {"mbsrtowcs", (uintptr_t)&stub_mbsrtowcs},
  {"mbtowc", (uintptr_t)&stub_mbtowc},
  {"memalign", (uintptr_t)&stub_memalign},
  {"memchr", 0},
  {"memcmp", 0},
  {"memcpy", 0},
  {"memmove", 0},
  {"memrchr", 0},
  {"memset", 0},
  {"mkdir", 0},
  {"mktime", 0},
  {"mmap", 0},
  {"modf", 0},
  {"modff", (uintptr_t)&stub_modff},
  {"mprotect", 0},
  {"munmap", 0},
  {"nanosleep", 0},
  {"nearbyintf", (uintptr_t)&stub_nearbyintf},
  {"newlocale", (uintptr_t)&stub_newlocale},
  {"open", 0},
  {"opendir", 0},
  {"openlog", (uintptr_t)&stub_openlog},
  {"perror", 0},
  {"pipe", 0},
  {"poll", 0},
  {"pow", 0},
  {"powf", 0},
  {"prctl", (uintptr_t)&stub_prctl},
  {"printf", 0},
  {"pthread_attr_destroy", 0},
  {"pthread_attr_init", 0},
  {"pthread_attr_setdetachstate", 0},
  {"pthread_attr_setstacksize", 0},
  {"pthread_condattr_destroy", 0},
  {"pthread_condattr_init", 0},
  {"pthread_condattr_setclock", 0},
  {"pthread_cond_broadcast", 0},
  {"pthread_cond_destroy", 0},
  {"pthread_cond_init", 0},
  {"pthread_cond_signal", 0},
  {"pthread_cond_timedwait", 0},
  {"pthread_cond_wait", 0},
  {"pthread_create", 0},
  {"pthread_detach", 0},
  {"pthread_equal", 0},
  {"pthread_exit", 0},
  {"pthread_getspecific", 0},
  {"pthread_join", 0},
  {"pthread_key_create", 0},
  {"pthread_key_delete", 0},
  {"pthread_mutexattr_destroy", 0},
  {"pthread_mutexattr_init", 0},
  {"pthread_mutexattr_settype", 0},
  {"pthread_mutex_destroy", 0},
  {"pthread_mutex_init", 0},
  {"pthread_mutex_lock", 0},
  {"pthread_mutex_trylock", 0},
  {"pthread_mutex_unlock", 0},
  {"pthread_once", 0},
  {"pthread_self", 0},
  {"pthread_setname_np", 0},
  {"pthread_setspecific", 0},
  {"puts", 0},
  {"qsort", 0},
  {"raise", (uintptr_t)&stub_raise},
  {"read", 0},
  {"readdir", 0},
  {"realloc", 0},
  {"realpath", 0},
  {"recv", (uintptr_t)&stub_recv},
  {"recvfrom", (uintptr_t)&stub_recvfrom},
  {"remainderf", (uintptr_t)&stub_remainderf},
  {"remove", 0},
  {"rename", 0},
  {"rmdir", 0},
  {"sched_getaffinity", (uintptr_t)&stub_sched_getaffinity},
  {"sched_setaffinity", (uintptr_t)&stub_sched_setaffinity},
  {"sched_yield", 0},
  {"select", 0},
  {"sem_destroy", 0},
  {"sem_init", 0},
  {"sem_post", 0},
  {"sem_wait", 0},
  {"send", (uintptr_t)&stub_send},
  {"sendto", (uintptr_t)&stub_sendto},
  {"setjmp", (uintptr_t)&stub_setjmp},
  {"setpriority", (uintptr_t)&stub_setpriority},
  {"setsockopt", (uintptr_t)&stub_setsockopt},
  {"setvbuf", 0},
  {"__sF", (uintptr_t)&stub___sF},
  {"shutdown", (uintptr_t)&stub_shutdown},
  {"sigaction", (uintptr_t)&stub_sigaction},
  {"sigaltstack", (uintptr_t)&stub_sigaltstack},
  {"sigemptyset", (uintptr_t)&stub_sigemptyset},
  {"sin", 0},
  {"sinf", 0},
  {"snprintf", 0},
  {"socket", (uintptr_t)&stub_socket},
  {"sprintf", 0},
  {"sqrt", 0},
  {"sqrtf", 0},
  {"sscanf", 0},
  {"__stack_chk_fail", (uintptr_t)&stub___stack_chk_fail},
  {"stat", 0},
  {"statfs", (uintptr_t)&stub_statfs},
  {"strcasecmp", 0},
  {"strcat", 0},
  {"strchr", 0},
  {"strcmp", 0},
  {"strcoll", 0},
  {"strcpy", 0},
  {"strdup", 0},
  {"strerror", 0},
  {"strerror_r", (uintptr_t)&stub_strerror_r},
  {"strftime", 0},
  {"strlen", 0},
  {"strncmp", 0},
  {"strncpy", 0},
  {"strnlen", (uintptr_t)&stub_strnlen},
  {"strpbrk", 0},
  {"strrchr", 0},
  {"strspn", 0},
  {"strstr", 0},
  {"strtod", 0},
  {"strtof", 0},
  {"strtok_r", (uintptr_t)&stub_strtok_r},
  {"strtol", 0},
  {"strtold_l", (uintptr_t)&stub_strtold_l},
  {"strtoll_l", (uintptr_t)&stub_strtoll_l},
  {"strtoul", 0},
  {"strtoull", 0},
  {"strtoull_l", (uintptr_t)&stub_strtoull_l},
  {"strxfrm", (uintptr_t)&stub_strxfrm},
  {"syscall", (uintptr_t)&stub_syscall},
  {"sysconf", 0},
  {"syslog", (uintptr_t)&stub_syslog},
  {"__system_property_find", (uintptr_t)&stub___system_property_find},
  {"__system_property_get", (uintptr_t)&stub___system_property_get},
  {"__system_property_read", (uintptr_t)&stub___system_property_read},
  {"tan", 0},
  {"tanf", 0},
  {"time", 0},
  {"tolower", 0},
  {"toupper", 0},
  {"towlower", (uintptr_t)&stub_towlower},
  {"towupper", (uintptr_t)&stub_towupper},
  {"truncate", 0},
  {"unlink", 0},
  {"uselocale", (uintptr_t)&stub_uselocale},
  {"usleep", 0},
  {"utime", (uintptr_t)&stub_utime},
  {"vasprintf", (uintptr_t)&stub_vasprintf},
  {"vfprintf", 0},
  {"vprintf", 0},
  {"vsnprintf", 0},
  {"vsscanf", (uintptr_t)&stub_vsscanf},
  {"waitpid", (uintptr_t)&stub_waitpid},
  {"wcrtomb", (uintptr_t)&stub_wcrtomb},
  {"wcscoll", (uintptr_t)&stub_wcscoll},
  {"wcslen", (uintptr_t)&stub_wcslen},
  {"wcsnrtombs", (uintptr_t)&stub_wcsnrtombs},
  {"wcsxfrm", (uintptr_t)&stub_wcsxfrm},
  {"wmemcpy", (uintptr_t)&stub_wmemcpy},
  {"wmemmove", (uintptr_t)&stub_wmemmove},
  {"wmemset", (uintptr_t)&stub_wmemset},
  {"write", 0},
  {"zlibVersion", (uintptr_t)&stub_zlibVersion},
};
size_t dynlib_numfunctions = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);

// resolve os passthrough via dlsym(RTLD_DEFAULT) em runtime
void recon_fill_passthrough(void){
  for(size_t i=0;i<dynlib_numfunctions;i++){
    if(dynlib_functions[i].func==0){
      void *p = dlsym(RTLD_DEFAULT, dynlib_functions[i].symbol);
      if(p) dynlib_functions[i].func=(uintptr_t)p;
      else fprintf(stderr,"[passthrough FALHOU dlsym] %s\\n", dynlib_functions[i].symbol);
    }
  }
}
