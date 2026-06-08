// imports.c — tabela DynLibFunction (GTA VC armhf). GERADO por gen_imports.py
#define _GNU_SOURCE
#include "imports.h"
#include "so_util.h"
// headers que declaram os simbolos passthrough:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <locale.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <syscall.h>
#include <errno.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>

// wrappers (pthread_fake.c / vc_impl.c / egl_shim.c)
extern int pthread_attr_destroy_fake();
extern int pthread_attr_init_fake();
extern int pthread_attr_setdetachstate_fake();
extern int pthread_attr_setschedparam_fake();
extern int pthread_attr_setstacksize_fake();
extern int pthread_cond_broadcast_fake();
extern int pthread_cond_destroy_fake();
extern int pthread_cond_init_fake();
extern int pthread_cond_signal_fake();
extern int pthread_cond_timedwait_fake();
extern int pthread_cond_timeout_np_fake();
extern int pthread_cond_wait_fake();
extern int pthread_create_fake();
extern int pthread_mutex_destroy_fake();
extern int pthread_mutex_init_fake();
extern int pthread_mutex_lock_fake();
extern int pthread_mutex_trylock_fake();
extern int pthread_mutex_unlock_fake();
extern int pthread_setname_np_fake();
extern int pthread_setschedparam_fake();
extern int sem_destroy_fake();
extern int sem_init_fake();
extern int sem_post_fake();
extern int sem_trywait_fake();
extern int sem_wait_fake();
extern int ImmVibeCloseDevice();
extern int ImmVibeGetEffectState();
extern int ImmVibeGetIVTEffectIndexFromName();
extern int ImmVibeInitialize2();
extern int ImmVibeOpenDevice();
extern int ImmVibePlayUHLEffect();
extern int ImmVibeStopPlayingEffect();
extern int ImmVibeTerminate();
extern const unsigned char *_ctype_;
extern FILE __sF[];
static const char *eglQueryString_stub(void *d, int n){(void)d;(void)n;return "";}
#include <stdarg.h>
extern void *text_base;
static int __android_log_print_stub(int prio,const char*tag,const char*fmt,...){
  void*ra=__builtin_return_address(0); unsigned long off=(unsigned long)ra-(unsigned long)text_base;
  va_list ap; va_start(ap,fmt); FILE*f=fopen("/storage/roms/ports/gtavc/debug.log","a");
  if(f){fprintf(f,"[LOG %d/%s ra=.so+0x%lx] ",prio,tag?tag:"",off);vfprintf(f,fmt,ap);fprintf(f,"\n");fclose(f);}
  va_end(ap); return 0;}
extern int raise_stub(int); extern void abort_stub(void);
extern const unsigned char *my_glGetString(unsigned int);
extern void my_glViewport(int,int,int,int);
extern void my_glClear(unsigned);
extern void my_glClearColor(float,float,float,float);
extern void my_glClearDepthf(float);
extern void my_glTexParameterf(unsigned,unsigned,float);
extern void my_glBlendFunc(unsigned,unsigned);
extern void my_glBindFramebuffer(unsigned,unsigned);
extern void my_glFramebufferTexture2D(unsigned,unsigned,unsigned,unsigned,int);
extern void my_glCompileShader(unsigned);
extern GLenum my_glCheckFramebufferStatus(unsigned);
extern void my_glShaderSource(unsigned,int,const char*const*,const int*);
extern void my_glLinkProgram(unsigned);
extern void my_glUseProgram(unsigned);
extern void my_glDrawArrays(unsigned,int,int);
extern void my_glDrawElements(unsigned,int,unsigned,const void*);
extern float vc_acosf(float);
extern float vc_asinf(float);
extern float vc_atanf(float);
extern float vc_atan2f(float,float);
extern float vc_ceilf(float);
extern float vc_cosf(float);
extern float vc_floorf(float);
extern float vc_fmodf(float,float);
extern float vc_log10f(float);
extern float vc_powf(float,float);
extern float vc_sinf(float);
extern float vc_sqrtf(float);
extern float vc_tanf(float);
extern int my_open(const char*,int,...); extern FILE *my_fopen(const char*,const char*);
extern long my_syscall(long,long,long,long,long,long,long);
extern int sigaction_stub(int,const void*,void*);
extern void *signal_stub(int,void*);
// decls de simbolos da glibc nao expostos por headers padrao:
extern int __cxa_atexit(void(*)(void*),void*,void*);
extern void __cxa_finalize(void*);
extern void __cxa_pure_virtual(void);
extern int __cxa_guard_acquire(void*);
extern void __cxa_guard_release(void*);
extern void __cxa_guard_abort(void*);
extern int __finitef(float);
extern int __gnu_Unwind_Find_exidx(void);
extern const short *_toupper_tab_;

DynLibFunction dynlib_functions[] = {
  {"ImmVibeCloseDevice", (uintptr_t)&ImmVibeCloseDevice},   // vc_impl
  {"ImmVibeGetEffectState", (uintptr_t)&ImmVibeGetEffectState},   // vc_impl
  {"ImmVibeGetIVTEffectIndexFromName", (uintptr_t)&ImmVibeGetIVTEffectIndexFromName},   // vc_impl
  {"ImmVibeInitialize2", (uintptr_t)&ImmVibeInitialize2},   // vc_impl
  {"ImmVibeOpenDevice", (uintptr_t)&ImmVibeOpenDevice},   // vc_impl
  {"ImmVibePlayUHLEffect", (uintptr_t)&ImmVibePlayUHLEffect},   // vc_impl
  {"ImmVibeStopPlayingEffect", (uintptr_t)&ImmVibeStopPlayingEffect},   // vc_impl
  {"ImmVibeTerminate", (uintptr_t)&ImmVibeTerminate},   // vc_impl
  {"__android_log_print", (uintptr_t)&__android_log_print_stub},
  {"__cxa_atexit", (uintptr_t)&__cxa_atexit},
  {"__cxa_finalize", (uintptr_t)&__cxa_finalize},
  {"__errno", (uintptr_t)&__errno_location},
  {"__gnu_Unwind_Find_exidx", (uintptr_t)&__gnu_Unwind_Find_exidx},
  {"__isfinitef", (uintptr_t)&__finitef},
  {"__sF", (uintptr_t)&__sF},   // vc_impl
  {"_ctype_", (uintptr_t)&_ctype_},   // vc_impl
  {"_toupper_tab_", (uintptr_t)&_toupper_tab_},   // vc_impl
  {"abort", (uintptr_t)&abort_stub},
  {"accept", (uintptr_t)&accept},
  {"acos", (uintptr_t)&acos},
  {"acosf", (uintptr_t)&vc_acosf},
  {"asinf", (uintptr_t)&vc_asinf},
  {"atan2", (uintptr_t)&atan2},
  {"atan2f", (uintptr_t)&vc_atan2f},
  {"atanf", (uintptr_t)&vc_atanf},
  {"atoi", (uintptr_t)&atoi},
  {"bind", (uintptr_t)&bind},
  {"calloc", (uintptr_t)&calloc},
  {"ceilf", (uintptr_t)&vc_ceilf},
  {"clock_gettime", (uintptr_t)&clock_gettime},
  {"close", (uintptr_t)&close},
  {"closedir", (uintptr_t)&closedir},
  {"connect", (uintptr_t)&connect},
  {"cos", (uintptr_t)&cos},
  {"cosf", (uintptr_t)&vc_cosf},
  {"ctime", (uintptr_t)&ctime},
  {"eglGetDisplay", (uintptr_t)&eglGetDisplay},   // egl_shim
  {"eglGetProcAddress", (uintptr_t)&eglGetProcAddress},   // egl_shim
  {"eglQueryString", (uintptr_t)&eglQueryString_stub},
  {"exp", (uintptr_t)&exp},
  {"fclose", (uintptr_t)&fclose},
  {"fdopen", (uintptr_t)&fdopen},
  {"fflush", (uintptr_t)&fflush},
  {"fgetc", (uintptr_t)&fgetc},
  {"fgets", (uintptr_t)&fgets},
  {"floorf", (uintptr_t)&vc_floorf},
  {"fmodf", (uintptr_t)&vc_fmodf},
  {"fopen", (uintptr_t)&my_fopen},
  {"fprintf", (uintptr_t)&fprintf},
  {"fputc", (uintptr_t)&fputc},
  {"fputs", (uintptr_t)&fputs},
  {"fread", (uintptr_t)&fread},
  {"free", (uintptr_t)&free},
  {"fseek", (uintptr_t)&fseek},
  {"ftell", (uintptr_t)&ftell},
  {"fwrite", (uintptr_t)&fwrite},
  {"getenv", (uintptr_t)&getenv},
  {"gethostbyaddr", (uintptr_t)&gethostbyaddr},
  {"gethostbyname", (uintptr_t)&gethostbyname},
  {"getsockname", (uintptr_t)&getsockname},
  {"getsockopt", (uintptr_t)&getsockopt},
  {"gettid", (uintptr_t)&gettid},
  {"gettimeofday", (uintptr_t)&gettimeofday},
  {"glActiveTexture", (uintptr_t)&glActiveTexture},
  {"glAttachShader", (uintptr_t)&glAttachShader},
  {"glBindAttribLocation", (uintptr_t)&glBindAttribLocation},
  {"glBindBuffer", (uintptr_t)&glBindBuffer},
  {"glBindFramebuffer", (uintptr_t)&my_glBindFramebuffer},
  {"glBindRenderbuffer", (uintptr_t)&glBindRenderbuffer},
  {"glBindTexture", (uintptr_t)&glBindTexture},
  {"glBlendFunc", (uintptr_t)&my_glBlendFunc},
  {"glBufferData", (uintptr_t)&glBufferData},
  {"glCheckFramebufferStatus", (uintptr_t)&my_glCheckFramebufferStatus},
  {"glClear", (uintptr_t)&my_glClear},
  {"glClearColor", (uintptr_t)&my_glClearColor},
  {"glClearDepthf", (uintptr_t)&my_glClearDepthf},
  {"glClearStencil", (uintptr_t)&glClearStencil},
  {"glCompileShader", (uintptr_t)&my_glCompileShader},
  {"glCompressedTexImage2D", (uintptr_t)&glCompressedTexImage2D},
  {"glCreateProgram", (uintptr_t)&glCreateProgram},
  {"glCreateShader", (uintptr_t)&glCreateShader},
  {"glCullFace", (uintptr_t)&glCullFace},
  {"glDeleteBuffers", (uintptr_t)&glDeleteBuffers},
  {"glDeleteFramebuffers", (uintptr_t)&glDeleteFramebuffers},
  {"glDeleteProgram", (uintptr_t)&glDeleteProgram},
  {"glDeleteRenderbuffers", (uintptr_t)&glDeleteRenderbuffers},
  {"glDeleteShader", (uintptr_t)&glDeleteShader},
  {"glDeleteTextures", (uintptr_t)&glDeleteTextures},
  {"glDepthFunc", (uintptr_t)&glDepthFunc},
  {"glDepthMask", (uintptr_t)&glDepthMask},
  {"glDisable", (uintptr_t)&glDisable},
  {"glDisableVertexAttribArray", (uintptr_t)&glDisableVertexAttribArray},
  {"glDrawArrays", (uintptr_t)&my_glDrawArrays},
  {"glDrawElements", (uintptr_t)&my_glDrawElements},
  {"glEnable", (uintptr_t)&glEnable},
  {"glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray},
  {"glFramebufferRenderbuffer", (uintptr_t)&glFramebufferRenderbuffer},
  {"glFramebufferTexture2D", (uintptr_t)&my_glFramebufferTexture2D},
  {"glFrontFace", (uintptr_t)&glFrontFace},
  {"glGenBuffers", (uintptr_t)&glGenBuffers},
  {"glGenFramebuffers", (uintptr_t)&glGenFramebuffers},
  {"glGenRenderbuffers", (uintptr_t)&glGenRenderbuffers},
  {"glGenTextures", (uintptr_t)&glGenTextures},
  {"glGetAttribLocation", (uintptr_t)&glGetAttribLocation},
  {"glGetError", (uintptr_t)&glGetError},
  {"glGetIntegerv", (uintptr_t)&glGetIntegerv},
  {"glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog},
  {"glGetProgramiv", (uintptr_t)&glGetProgramiv},
  {"glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog},
  {"glGetShaderiv", (uintptr_t)&glGetShaderiv},
  {"glGetString", (uintptr_t)&my_glGetString},
  {"glGetUniformLocation", (uintptr_t)&glGetUniformLocation},
  {"glHint", (uintptr_t)&glHint},
  {"glLinkProgram", (uintptr_t)&my_glLinkProgram},
  {"glReadPixels", (uintptr_t)&glReadPixels},
  {"glRenderbufferStorage", (uintptr_t)&glRenderbufferStorage},
  {"glShaderSource", (uintptr_t)&my_glShaderSource},
  {"glTexImage2D", (uintptr_t)&glTexImage2D},
  {"glTexParameterf", (uintptr_t)&my_glTexParameterf},
  {"glTexParameteri", (uintptr_t)&glTexParameteri},
  {"glUniform1fv", (uintptr_t)&glUniform1fv},
  {"glUniform1i", (uintptr_t)&glUniform1i},
  {"glUniform2fv", (uintptr_t)&glUniform2fv},
  {"glUniform3fv", (uintptr_t)&glUniform3fv},
  {"glUniform4fv", (uintptr_t)&glUniform4fv},
  {"glUniformMatrix3fv", (uintptr_t)&glUniformMatrix3fv},
  {"glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv},
  {"glUseProgram", (uintptr_t)&my_glUseProgram},
  {"glVertexAttrib4fv", (uintptr_t)&glVertexAttrib4fv},
  {"glVertexAttribPointer", (uintptr_t)&glVertexAttribPointer},
  {"glViewport", (uintptr_t)&my_glViewport},
  {"gmtime", (uintptr_t)&gmtime},
  {"inet_aton", (uintptr_t)&inet_aton},
  {"inet_ntoa", (uintptr_t)&inet_ntoa},
  {"ioctl", (uintptr_t)&ioctl},
  {"listen", (uintptr_t)&listen},
  {"log", (uintptr_t)&log},
  {"log10f", (uintptr_t)&vc_log10f},
  {"longjmp", (uintptr_t)&_longjmp},
  {"lrand48", (uintptr_t)&lrand48},
  {"lseek", (uintptr_t)&lseek},
  {"malloc", (uintptr_t)&malloc},
  {"memchr", (uintptr_t)&memchr},
  {"memcmp", (uintptr_t)&memcmp},
  {"memcpy", (uintptr_t)&memcpy},
  {"memmove", (uintptr_t)&memmove},
  {"memset", (uintptr_t)&memset},
  {"mkdir", (uintptr_t)&mkdir},
  {"nanosleep", (uintptr_t)&nanosleep},
  {"open", (uintptr_t)&my_open},
  {"opendir", (uintptr_t)&opendir},
  {"pow", (uintptr_t)&pow},
  {"powf", (uintptr_t)&vc_powf},
  {"printf", (uintptr_t)&printf},
  {"pthread_attr_destroy", (uintptr_t)&pthread_attr_destroy_fake},
  {"pthread_attr_getschedparam", (uintptr_t)&pthread_attr_getschedparam},
  {"pthread_attr_getstacksize", (uintptr_t)&pthread_attr_getstacksize},
  {"pthread_attr_init", (uintptr_t)&pthread_attr_init_fake},
  {"pthread_attr_setschedparam", (uintptr_t)&pthread_attr_setschedparam_fake},
  {"pthread_attr_setstacksize", (uintptr_t)&pthread_attr_setstacksize_fake},
  {"pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake},
  {"pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_fake},
  {"pthread_cond_init", (uintptr_t)&pthread_cond_init_fake},
  {"pthread_cond_signal", (uintptr_t)&pthread_cond_signal_fake},
  {"pthread_cond_timedwait", (uintptr_t)&pthread_cond_timedwait_fake},
  {"pthread_cond_timeout_np", (uintptr_t)&pthread_cond_timeout_np_fake},
  {"pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake},
  {"pthread_create", (uintptr_t)&pthread_create_fake},
  {"pthread_getspecific", (uintptr_t)&pthread_getspecific},
  {"pthread_join", (uintptr_t)&pthread_join},
  {"pthread_key_create", (uintptr_t)&pthread_key_create},
  {"pthread_key_delete", (uintptr_t)&pthread_key_delete},
  {"pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake},
  {"pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake},
  {"pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake},
  {"pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake},
  {"pthread_mutexattr_destroy", (uintptr_t)&pthread_mutexattr_destroy},
  {"pthread_mutexattr_init", (uintptr_t)&pthread_mutexattr_init},
  {"pthread_mutexattr_settype", (uintptr_t)&pthread_mutexattr_settype},
  {"pthread_once", (uintptr_t)&pthread_once},
  {"pthread_self", (uintptr_t)&pthread_self},
  {"pthread_setname_np", (uintptr_t)&pthread_setname_np_fake},
  {"pthread_setschedparam", (uintptr_t)&pthread_setschedparam_fake},
  {"pthread_setspecific", (uintptr_t)&pthread_setspecific},
  {"puts", (uintptr_t)&puts},
  {"qsort", (uintptr_t)&qsort},
  {"raise", (uintptr_t)&raise_stub},
  {"read", (uintptr_t)&read},
  {"readdir", (uintptr_t)&readdir},
  {"realloc", (uintptr_t)&realloc},
  {"recvmsg", (uintptr_t)&recvmsg},
  {"rewind", (uintptr_t)&rewind},
  {"sched_get_priority_max", (uintptr_t)&sched_get_priority_max},
  {"sched_get_priority_min", (uintptr_t)&sched_get_priority_min},
  {"sched_yield", (uintptr_t)&sched_yield},
  {"select", (uintptr_t)&select},
  {"sem_destroy", (uintptr_t)&sem_destroy_fake},
  {"sem_getvalue", (uintptr_t)&sem_getvalue},
  {"sem_init", (uintptr_t)&sem_init_fake},
  {"sem_post", (uintptr_t)&sem_post_fake},
  {"sem_trywait", (uintptr_t)&sem_trywait_fake},
  {"sem_wait", (uintptr_t)&sem_wait_fake},
  {"sendmsg", (uintptr_t)&sendmsg},
  {"setjmp", (uintptr_t)&_setjmp},
  {"setsockopt", (uintptr_t)&setsockopt},
  {"shutdown", (uintptr_t)&shutdown},
  {"sigaction", (uintptr_t)&sigaction_stub},
  {"sin", (uintptr_t)&sin},
  {"sinf", (uintptr_t)&vc_sinf},
  {"snprintf", (uintptr_t)&snprintf},
  {"socket", (uintptr_t)&socket},
  {"sprintf", (uintptr_t)&sprintf},
  {"sqrtf", (uintptr_t)&vc_sqrtf},
  {"srand48", (uintptr_t)&srand48},
  {"sscanf", (uintptr_t)&sscanf},
  {"stat", (uintptr_t)&stat},
  {"strcasecmp", (uintptr_t)&strcasecmp},
  {"strcat", (uintptr_t)&strcat},
  {"strchr", (uintptr_t)&strchr},
  {"strcmp", (uintptr_t)&strcmp},
  {"strcpy", (uintptr_t)&strcpy},
  {"strerror", (uintptr_t)&strerror},
  {"strlen", (uintptr_t)&strlen},
  {"strncasecmp", (uintptr_t)&strncasecmp},
  {"strncat", (uintptr_t)&strncat},
  {"strncmp", (uintptr_t)&strncmp},
  {"strncpy", (uintptr_t)&strncpy},
  {"strpbrk", (uintptr_t)&strpbrk},
  {"strrchr", (uintptr_t)&strrchr},
  {"strstr", (uintptr_t)&strstr},
  {"strtod", (uintptr_t)&strtod},
  {"strtok", (uintptr_t)&strtok},
  {"strtol", (uintptr_t)&strtol},
  {"strtoul", (uintptr_t)&strtoul},
  {"syscall", (uintptr_t)&my_syscall},
  {"sysconf", (uintptr_t)&sysconf},
  {"tan", (uintptr_t)&tan},
  {"tanf", (uintptr_t)&vc_tanf},
  {"time", (uintptr_t)&time},
  {"usleep", (uintptr_t)&usleep},
  {"vsnprintf", (uintptr_t)&vsnprintf},
  {"vsprintf", (uintptr_t)&vsprintf},
  {"write", (uintptr_t)&write},
};
size_t dynlib_numfunctions = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);
FILE *stderr_fake;
