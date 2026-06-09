// re4_imports.c -- GERADO. egl->egl_shim_*, resto->dlsym(RTLD_DEFAULT) em runtime + shims bionic.
#include "imports.h"
#include "so_util.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
extern void egl_shim_ChooseConfig(void);
extern void egl_shim_CreateContext(void);
extern void egl_shim_CreatePbufferSurface(void);
extern void egl_shim_CreateWindowSurface(void);
extern void egl_shim_DestroyContext(void);
extern void egl_shim_DestroySurface(void);
extern void egl_shim_GetConfigAttrib(void);
extern void egl_shim_GetCurrentContext(void);
extern void egl_shim_GetCurrentSurface(void);
extern void egl_shim_GetDisplay(void);
extern void egl_shim_GetError(void);
extern void egl_shim_GetProcAddress(void);
extern void egl_shim_Initialize(void);
extern void egl_shim_MakeCurrent(void);
extern void egl_shim_QueryString(void);
extern void egl_shim_QuerySurface(void);
extern void egl_shim_SwapBuffers(void);
extern void egl_shim_SwapInterval(void);
extern void egl_shim_Terminate(void);

static long stub_generic(void){ return 0; }
/* shims bionic */
static unsigned char g_ctype[384];
static short g_tolower[384], g_toupper[384];
static int g_ctype_init=0;
#include <ctype.h>
static void ctype_build(void){
  if(g_ctype_init) return; g_ctype_init=1;
  for(int c=0;c<256;c++){ unsigned char b=0;
    if(isupper(c))b|=0x01; if(islower(c))b|=0x02; if(isdigit(c))b|=0x04;
    if(isspace(c))b|=0x08; if(ispunct(c))b|=0x10; if(iscntrl(c))b|=0x20;
    if(isxdigit(c))b|=0x40; if(c==' ')b|=0x80;
    g_ctype[c+1]=b; g_tolower[c+1]=tolower(c); g_toupper[c+1]=toupper(c);
  }
}
/* pthread_cond_timedwait_relative_np (bionic): relativo -> absoluto */
static int cond_timedwait_rel(pthread_cond_t*c,pthread_mutex_t*m,const struct timespec*rel){
  if(!rel) return pthread_cond_wait(c,m);
  struct timespec abs; clock_gettime(CLOCK_REALTIME,&abs);
  abs.tv_sec+=rel->tv_sec; abs.tv_nsec+=rel->tv_nsec;
  if(abs.tv_nsec>=1000000000L){abs.tv_sec++;abs.tv_nsec-=1000000000L;}
  return pthread_cond_timedwait(c,m,&abs);
}
static long noop(void){ return 0; }
/* resolve um simbolo: bionic-shim > alias > dlsym > stub */
static long ig_sysconf(int name){ long r=sysconf(name);
  if((name==_SC_PHYS_PAGES||name==_SC_AVPHYS_PAGES)&&r<=0){ long ps=sysconf(_SC_PAGESIZE); if(ps<=0)ps=4096;
    r=(512L*1024*1024)/ps; fprintf(stderr,"[SYSCONF] %d 0->%ld\n",name,r);} return r; }
void *re4_resolve(const char *nm){
  if(!strcmp(nm,"sysconf")) return (void*)&ig_sysconf;
  ctype_build();
  if(!strcmp(nm,"_ctype_")) return &g_ctype[1];
  if(!strcmp(nm,"_tolower_tab_")) return &g_tolower[1];
  if(!strcmp(nm,"_toupper_tab_")) return &g_toupper[1];
  if(!strcmp(nm,"__errno")) return dlsym(RTLD_DEFAULT,"__errno_location");
  if(!strcmp(nm,"setjmp")) return dlsym(RTLD_DEFAULT,"_setjmp");
  if(!strcmp(nm,"longjmp")) return dlsym(RTLD_DEFAULT,"_longjmp");
  if(!strcmp(nm,"__assert2")) return dlsym(RTLD_DEFAULT,"__assert_fail");
  if(!strcmp(nm,"pthread_cond_timedwait_relative_np")) return (void*)&cond_timedwait_rel;
  if(!strncmp(nm,"__google_potentially",20)) return (void*)&noop;
  if(!strcmp(nm,"ptrace")) return (void*)&noop;
  if(!strcmp(nm,"__sF")) return dlsym(RTLD_DEFAULT,"stdout");
  if(!strcmp(nm,"bsd_signal")||!strcmp(nm,"sysv_signal")) return dlsym(RTLD_DEFAULT,"signal");
  if(!strcmp(nm,"sigignore")) return (void*)&noop;
  void *p=dlsym(RTLD_DEFAULT,nm);
  if(p) return p;
  /* Android-specific / nao-achado -> stub */
  return (void*)&stub_generic;
}

DynLibFunction dynlib_functions[] = {
  {"AInputEvent_getDeviceId", 0},
  {"AInputEvent_getType", 0},
  {"AKeyEvent_getAction", 0},
  {"AKeyEvent_getKeyCode", 0},
  {"AKeyEvent_getMetaState", 0},
  {"ALooper_forThread", 0},
  {"ALooper_prepare", 0},
  {"ANativeWindow_acquire", 0},
  {"ANativeWindow_fromSurface", 0},
  {"ANativeWindow_getHeight", 0},
  {"ANativeWindow_getWidth", 0},
  {"ANativeWindow_release", 0},
  {"ANativeWindow_setBuffersGeometry", 0},
  {"ASensorEventQueue_disableSensor", 0},
  {"ASensorEventQueue_enableSensor", 0},
  {"ASensorEventQueue_getEvents", 0},
  {"ASensorEventQueue_hasEvents", 0},
  {"ASensorEventQueue_setEventRate", 0},
  {"ASensorManager_createEventQueue", 0},
  {"ASensorManager_destroyEventQueue", 0},
  {"ASensorManager_getDefaultSensor", 0},
  {"ASensorManager_getInstance", 0},
  {"ASensorManager_getSensorList", 0},
  {"ASensor_getMinDelay", 0},
  {"ASensor_getName", 0},
  {"ASensor_getResolution", 0},
  {"ASensor_getType", 0},
  {"ASensor_getVendor", 0},
  {"_Unwind_Backtrace", 0},
  {"_Unwind_Complete", 0},
  {"_Unwind_DeleteException", 0},
  {"_Unwind_GetDataRelBase", 0},
  {"_Unwind_GetLanguageSpecificData", 0},
  {"_Unwind_GetRegionStart", 0},
  {"_Unwind_GetTextRelBase", 0},
  {"_Unwind_RaiseException", 0},
  {"_Unwind_Resume", 0},
  {"_Unwind_Resume_or_Rethrow", 0},
  {"_Unwind_VRS_Get", 0},
  {"_Unwind_VRS_Set", 0},
  {"__aeabi_atexit", 0},
  {"__aeabi_memclr", 0},
  {"__aeabi_memclr4", 0},
  {"__aeabi_memclr8", 0},
  {"__aeabi_memcpy", 0},
  {"__aeabi_memcpy4", 0},
  {"__aeabi_memcpy8", 0},
  {"__aeabi_memmove", 0},
  {"__aeabi_memmove4", 0},
  {"__aeabi_memmove8", 0},
  {"__aeabi_memset", 0},
  {"__aeabi_memset4", 0},
  {"__aeabi_memset8", 0},
  {"__aeabi_unwind_cpp_pr0", 0},
  {"__aeabi_unwind_cpp_pr1", 0},
  {"__android_log_print", 0},
  {"__android_log_vprint", 0},
  {"__android_log_write", 0},
  {"__cxa_atexit", 0},
  {"__cxa_finalize", 0},
  {"__errno", 0},
  {"__gnu_Unwind_Find_exidx", 0},
  {"__gnu_unwind_frame", 0},
  {"__google_potentially_blocking_region_begin", 0},
  {"__google_potentially_blocking_region_end", 0},
  {"__sF", 0},
  {"__stack_chk_fail", 0},
  {"__stack_chk_guard", 0},
  {"__system_property_get", 0},
  {"_ctype_", 0},
  {"_tolower_tab_", 0},
  {"_toupper_tab_", 0},
  {"accept", 0},
  {"access", 0},
  {"acos", 0},
  {"acosf", 0},
  {"asctime", 0},
  {"asin", 0},
  {"asinf", 0},
  {"atan", 0},
  {"atan2", 0},
  {"atan2f", 0},
  {"atanf", 0},
  {"atoi", 0},
  {"bind", 0},
  {"bsearch", 0},
  {"btowc", 0},
  {"calloc", 0},
  {"ceil", 0},
  {"ceilf", 0},
  {"chmod", 0},
  {"clearerr", 0},
  {"clock_gettime", 0},
  {"close", 0},
  {"closedir", 0},
  {"connect", 0},
  {"cos", 0},
  {"cosf", 0},
  {"difftime", 0},
  {"div", 0},
  {"dladdr", 0},
  {"dlclose", 0},
  {"dlerror", 0},
  {"dlopen", 0},
  {"dlsym", 0},
  {"dup2", 0},
  {"eglChooseConfig", (uintptr_t)&egl_shim_ChooseConfig},
  {"eglCreateContext", (uintptr_t)&egl_shim_CreateContext},
  {"eglCreatePbufferSurface", (uintptr_t)&egl_shim_CreatePbufferSurface},
  {"eglCreateWindowSurface", (uintptr_t)&egl_shim_CreateWindowSurface},
  {"eglDestroyContext", (uintptr_t)&egl_shim_DestroyContext},
  {"eglDestroySurface", (uintptr_t)&egl_shim_DestroySurface},
  {"eglGetConfigAttrib", (uintptr_t)&egl_shim_GetConfigAttrib},
  {"eglGetCurrentContext", (uintptr_t)&egl_shim_GetCurrentContext},
  {"eglGetCurrentSurface", (uintptr_t)&egl_shim_GetCurrentSurface},
  {"eglGetDisplay", (uintptr_t)&egl_shim_GetDisplay},
  {"eglGetError", (uintptr_t)&egl_shim_GetError},
  {"eglGetProcAddress", (uintptr_t)&egl_shim_GetProcAddress},
  {"eglInitialize", (uintptr_t)&egl_shim_Initialize},
  {"eglMakeCurrent", (uintptr_t)&egl_shim_MakeCurrent},
  {"eglQueryString", (uintptr_t)&egl_shim_QueryString},
  {"eglQuerySurface", (uintptr_t)&egl_shim_QuerySurface},
  {"eglSwapBuffers", (uintptr_t)&egl_shim_SwapBuffers},
  {"eglSwapInterval", (uintptr_t)&egl_shim_SwapInterval},
  {"eglTerminate", (uintptr_t)&egl_shim_Terminate},
  {"execl", 0},
  {"exit", 0},
  {"exp", 0},
  {"exp2", 0},
  {"expf", 0},
  {"fabsf", 0},
  {"fclose", 0},
  {"fcntl", 0},
  {"fdopen", 0},
  {"fflush", 0},
  {"fgets", 0},
  {"flock", 0},
  {"floor", 0},
  {"floorf", 0},
  {"fmaxf", 0},
  {"fminf", 0},
  {"fmod", 0},
  {"fmodf", 0},
  {"fopen", 0},
  {"fork", 0},
  {"fprintf", 0},
  {"fputc", 0},
  {"fputs", 0},
  {"fread", 0},
  {"free", 0},
  {"freeaddrinfo", 0},
  {"frexp", 0},
  {"fscanf", 0},
  {"fseek", 0},
  {"fstat", 0},
  {"ftell", 0},
  {"fwrite", 0},
  {"gai_strerror", 0},
  {"getaddrinfo", 0},
  {"getc", 0},
  {"getenv", 0},
  {"gethostbyname", 0},
  {"getnameinfo", 0},
  {"getpid", 0},
  {"getpwuid", 0},
  {"getsockname", 0},
  {"getsockopt", 0},
  {"gettid", 0},
  {"gettimeofday", 0},
  {"getuid", 0},
  {"gmtime", 0},
  {"inet_addr", 0},
  {"inet_aton", 0},
  {"inet_ntoa", 0},
  {"inet_ntop", 0},
  {"inflate", 0},
  {"inflateEnd", 0},
  {"inflateInit2_", 0},
  {"ioctl", 0},
  {"iswctype", 0},
  {"kill", 0},
  {"ldexp", 0},
  {"listen", 0},
  {"localtime", 0},
  {"log", 0},
  {"log10", 0},
  {"log10f", 0},
  {"logf", 0},
  {"longjmp", 0},
  {"lrand48", 0},
  {"lroundf", 0},
  {"lseek", 0},
  {"lseek64", 0},
  {"lstat", 0},
  {"malloc", 0},
  {"mbrtowc", 0},
  {"memalign", 0},
  {"memchr", 0},
  {"memcmp", 0},
  {"memcpy", 0},
  {"memmem", 0},
  {"memmove", 0},
  {"memset", 0},
  {"mkdir", 0},
  {"mktime", 0},
  {"mmap", 0},
  {"modf", 0},
  {"modff", 0},
  {"munmap", 0},
  {"nanosleep", 0},
  {"open", 0},
  {"opendir", 0},
  {"perror", 0},
  {"pipe", 0},
  {"pow", 0},
  {"powf", 0},
  {"prctl", 0},
  {"printf", 0},
  {"pthread_attr_destroy", 0},
  {"pthread_attr_init", 0},
  {"pthread_attr_setdetachstate", 0},
  {"pthread_attr_setschedparam", 0},
  {"pthread_attr_setstacksize", 0},
  {"pthread_cond_broadcast", 0},
  {"pthread_cond_destroy", 0},
  {"pthread_cond_init", 0},
  {"pthread_cond_signal", 0},
  {"pthread_cond_timedwait", 0},
  {"pthread_cond_timedwait_relative_np", 0},
  {"pthread_cond_wait", 0},
  {"pthread_create", 0},
  {"pthread_equal", 0},
  {"pthread_exit", 0},
  {"pthread_getcpuclockid", 0},
  {"pthread_getschedparam", 0},
  {"pthread_getspecific", 0},
  {"pthread_join", 0},
  {"pthread_key_create", 0},
  {"pthread_key_delete", 0},
  {"pthread_kill", 0},
  {"pthread_mutex_destroy", 0},
  {"pthread_mutex_init", 0},
  {"pthread_mutex_lock", 0},
  {"pthread_mutex_trylock", 0},
  {"pthread_mutex_unlock", 0},
  {"pthread_mutexattr_destroy", 0},
  {"pthread_mutexattr_init", 0},
  {"pthread_mutexattr_settype", 0},
  {"pthread_once", 0},
  {"pthread_self", 0},
  {"pthread_setname_np", 0},
  {"pthread_setschedparam", 0},
  {"pthread_setspecific", 0},
  {"ptrace", 0},
  {"putchar", 0},
  {"puts", 0},
  {"qsort", 0},
  {"raise", 0},
  {"read", 0},
  {"readdir", 0},
  {"realloc", 0},
  {"realpath", 0},
  {"recv", 0},
  {"recvfrom", 0},
  {"remainder", 0},
  {"remove", 0},
  {"rename", 0},
  {"rintf", 0},
  {"rmdir", 0},
  {"roundf", 0},
  {"sched_get_priority_max", 0},
  {"sched_get_priority_min", 0},
  {"sched_yield", 0},
  {"select", 0},
  {"sem_destroy", 0},
  {"sem_init", 0},
  {"sem_open", 0},
  {"sem_post", 0},
  {"sem_timedwait", 0},
  {"sem_wait", 0},
  {"send", 0},
  {"sendto", 0},
  {"setjmp", 0},
  {"setlocale", 0},
  {"setpriority", 0},
  {"setsockopt", 0},
  {"shutdown", 0},
  {"sigaction", 0},
  {"sin", 0},
  {"sinf", 0},
  {"sleep", 0},
  {"snprintf", 0},
  {"socket", 0},
  {"sprintf", 0},
  {"sqrt", 0},
  {"sqrtf", 0},
  {"srand48", 0},
  {"sscanf", 0},
  {"stat", 0},
  {"statfs", 0},
  {"strcasecmp", 0},
  {"strcat", 0},
  {"strchr", 0},
  {"strcmp", 0},
  {"strcoll", 0},
  {"strcpy", 0},
  {"strdup", 0},
  {"strerror", 0},
  {"strftime", 0},
  {"strlen", 0},
  {"strncasecmp", 0},
  {"strncat", 0},
  {"strncmp", 0},
  {"strncpy", 0},
  {"strnlen", 0},
  {"strpbrk", 0},
  {"strrchr", 0},
  {"strsep", 0},
  {"strstr", 0},
  {"strtod", 0},
  {"strtok", 0},
  {"strtol", 0},
  {"strtoul", 0},
  {"strtoull", 0},
  {"strxfrm", 0},
  {"syscall", 0},
  {"sysconf", 0},
  {"tan", 0},
  {"tanf", 0},
  {"time", 0},
  {"towlower", 0},
  {"towupper", 0},
  {"truncate", 0},
  {"truncf", 0},
  {"unlink", 0},
  {"usleep", 0},
  {"utime", 0},
  {"vfprintf", 0},
  {"vprintf", 0},
  {"vsnprintf", 0},
  {"vsprintf", 0},
  {"waitpid", 0},
  {"wcrtomb", 0},
  {"wcscoll", 0},
  {"wcsftime", 0},
  {"wcslen", 0},
  {"wcsxfrm", 0},
  {"wctob", 0},
  {"wctype", 0},
  {"wmemchr", 0},
  {"wmemcmp", 0},
  {"wmemcpy", 0},
  {"wmemmove", 0},
  {"wmemset", 0},
  {"write", 0},
};
size_t dynlib_numfunctions = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);

/* preenche os func=0 via re4_resolve. Chamar ANTES de so_resolve. */
void re4_fill(void){
  int real=0,stub=0;
  for(size_t i=0;i<dynlib_numfunctions;i++){
    if(dynlib_functions[i].func) continue; /* egl ja setado */
    void *p=re4_resolve(dynlib_functions[i].symbol);
    dynlib_functions[i].func=(uintptr_t)p;
    if(p==(void*)&stub_generic){stub++; if(stub<=60) fprintf(stderr,"[STUB] %s\n",dynlib_functions[i].symbol);}
    else real++;
  }
  fprintf(stderr,"[re4_fill] %d resolvidos + %d stubs (de %zu)\n",real,stub,dynlib_numfunctions);
}

