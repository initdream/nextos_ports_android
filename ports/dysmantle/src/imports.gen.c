// imports.gen.c — GERADO por new-port.sh para 'dysmantle' (libNativeGame.so)
// 404 simbolos. Resolva os UNKNOWN no fim do arquivo.
#include "imports.h"
#include "so_util.h"
#include <stdio.h>

// === passthrough/pthread/shim: ligados automaticamente ===
DynLibFunction dynlib_functions[] = {
  {"abort", (uintptr_t)&abort},  // pass
  {"access", (uintptr_t)&access},  // pass
  {"acos", (uintptr_t)&acos},  // pass
  // TODO {"acosf", (uintptr_t)&stub_acosf},  // <<< IMPLEMENTAR
  {"__android_log_assert", (uintptr_t)&__android_log_assert},  // liblog
  {"__android_log_print", (uintptr_t)&__android_log_print},  // liblog
  {"__android_log_write", (uintptr_t)&__android_log_write},  // liblog
  // TODO {"asinf", (uintptr_t)&stub_asinf},  // <<< IMPLEMENTAR
  // TODO {"__assert2", (uintptr_t)&stub___assert2},  // <<< IMPLEMENTAR
  {"atan", (uintptr_t)&atan},  // pass
  {"atan2", (uintptr_t)&atan2},  // pass
  {"atan2f", (uintptr_t)&atan2f},  // pass
  // TODO {"atanf", (uintptr_t)&stub_atanf},  // <<< IMPLEMENTAR
  {"atof", (uintptr_t)&atof},  // pass
  {"atoi", (uintptr_t)&atoi},  // pass
  {"atol", (uintptr_t)&atol},  // pass
  {"calloc", (uintptr_t)&calloc},  // pass
  {"clock_gettime", (uintptr_t)&clock_gettime},  // pass
  // TODO {"clock_nanosleep", (uintptr_t)&stub_clock_nanosleep},  // <<< IMPLEMENTAR
  {"close", (uintptr_t)&close},  // pass
  {"closedir", (uintptr_t)&closedir},  // pass
  {"cos", (uintptr_t)&cos},  // pass
  {"cosf", (uintptr_t)&cosf},  // pass
  // TODO {"cosh", (uintptr_t)&stub_cosh},  // <<< IMPLEMENTAR
  {"__cxa_atexit", (uintptr_t)&__cxa_atexit},  // cxx
  {"__cxa_finalize", (uintptr_t)&__cxa_finalize},  // cxx
  {"__cxa_guard_acquire", (uintptr_t)&__cxa_guard_acquire},  // cxx
  {"__cxa_guard_release", (uintptr_t)&__cxa_guard_release},  // cxx
  {"__cxa_pure_virtual", (uintptr_t)&__cxa_pure_virtual},  // cxx
  {"__cxa_thread_atexit", (uintptr_t)&__cxa_thread_atexit},  // cxx
  // TODO {"dlclose", (uintptr_t)&stub_dlclose},  // <<< IMPLEMENTAR
  // TODO {"dlopen", (uintptr_t)&stub_dlopen},  // <<< IMPLEMENTAR
  // TODO {"dlsym", (uintptr_t)&stub_dlsym},  // <<< IMPLEMENTAR
  {"eglBindAPI", (uintptr_t)&eglBindAPI_shim},  // egl_shim
  {"eglChooseConfig", (uintptr_t)&eglChooseConfig_shim},  // egl_shim
  {"eglCreateContext", (uintptr_t)&eglCreateContext_shim},  // egl_shim
  {"eglCreateWindowSurface", (uintptr_t)&eglCreateWindowSurface_shim},  // egl_shim
  {"eglDestroyContext", (uintptr_t)&eglDestroyContext_shim},  // egl_shim
  {"eglDestroySurface", (uintptr_t)&eglDestroySurface_shim},  // egl_shim
  {"eglGetConfigAttrib", (uintptr_t)&eglGetConfigAttrib_shim},  // egl_shim
  {"eglGetDisplay", (uintptr_t)&eglGetDisplay_shim},  // egl_shim
  {"eglGetError", (uintptr_t)&eglGetError_shim},  // egl_shim
  {"eglGetProcAddress", (uintptr_t)&eglGetProcAddress_shim},  // egl_shim
  {"eglInitialize", (uintptr_t)&eglInitialize_shim},  // egl_shim
  {"eglMakeCurrent", (uintptr_t)&eglMakeCurrent_shim},  // egl_shim
  {"eglReleaseThread", (uintptr_t)&eglReleaseThread_shim},  // egl_shim
  {"eglSwapBuffers", (uintptr_t)&eglSwapBuffers_shim},  // egl_shim
  {"eglSwapInterval", (uintptr_t)&eglSwapInterval_shim},  // egl_shim
  {"eglTerminate", (uintptr_t)&eglTerminate_shim},  // egl_shim
  // TODO {"__emutls_get_address", (uintptr_t)&stub___emutls_get_address},  // <<< IMPLEMENTAR
  {"__errno", (uintptr_t)&__errno},  // pass
  {"exit", (uintptr_t)&exit},  // pass
  {"exp", (uintptr_t)&exp},  // pass
  {"expf", (uintptr_t)&expf},  // pass
  {"fclose", (uintptr_t)&fclose},  // pass
  {"fcntl", (uintptr_t)&fcntl},  // pass
  // TODO {"ferror", (uintptr_t)&stub_ferror},  // <<< IMPLEMENTAR
  {"fflush", (uintptr_t)&fflush},  // pass
  {"fgets", (uintptr_t)&fgets},  // pass
  {"fileno", (uintptr_t)&fileno},  // pass
  {"fmod", (uintptr_t)&fmod},  // pass
  {"fmodf", (uintptr_t)&fmodf},  // pass
  {"fopen", (uintptr_t)&fopen},  // pass
  {"fprintf", (uintptr_t)&fprintf},  // pass
  {"fputc", (uintptr_t)&fputc},  // pass
  {"fread", (uintptr_t)&fread},  // pass
  {"free", (uintptr_t)&free},  // pass
  {"frexp", (uintptr_t)&frexp},  // pass
  {"fseek", (uintptr_t)&fseek},  // pass
  // TODO {"fseeko", (uintptr_t)&stub_fseeko},  // <<< IMPLEMENTAR
  {"fstat", (uintptr_t)&fstat},  // pass
  // TODO {"fsync", (uintptr_t)&stub_fsync},  // <<< IMPLEMENTAR
  {"ftell", (uintptr_t)&ftell},  // pass
  // TODO {"ftello", (uintptr_t)&stub_ftello},  // <<< IMPLEMENTAR
  {"fwrite", (uintptr_t)&fwrite},  // pass
  // TODO {"getauxval", (uintptr_t)&stub_getauxval},  // <<< IMPLEMENTAR
  {"getenv", (uintptr_t)&getenv},  // pass
  {"getpid", (uintptr_t)&getpid},  // pass
  {"gettid", (uintptr_t)&gettid},  // pass
  {"gettimeofday", (uintptr_t)&gettimeofday},  // pass
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
  {"glClearDepthf", (uintptr_t)&glClearDepthf},  // gles
  {"glColorMask", (uintptr_t)&glColorMask},  // gles
  {"glCompileShader", (uintptr_t)&glCompileShader},  // gles
  {"glCopyTexSubImage2D", (uintptr_t)&glCopyTexSubImage2D},  // gles
  {"glCreateProgram", (uintptr_t)&glCreateProgram},  // gles
  {"glCreateShader", (uintptr_t)&glCreateShader},  // gles
  {"glCullFace", (uintptr_t)&glCullFace},  // gles
  {"glDeleteBuffers", (uintptr_t)&glDeleteBuffers},  // gles
  {"glDeleteFramebuffers", (uintptr_t)&glDeleteFramebuffers},  // gles
  {"glDeleteProgram", (uintptr_t)&glDeleteProgram},  // gles
  {"glDeleteShader", (uintptr_t)&glDeleteShader},  // gles
  {"glDeleteTextures", (uintptr_t)&glDeleteTextures},  // gles
  {"glDepthFunc", (uintptr_t)&glDepthFunc},  // gles
  {"glDepthMask", (uintptr_t)&glDepthMask},  // gles
  {"glDepthRangef", (uintptr_t)&glDepthRangef},  // gles
  {"glDetachShader", (uintptr_t)&glDetachShader},  // gles
  {"glDisable", (uintptr_t)&glDisable},  // gles
  {"glDrawArrays", (uintptr_t)&glDrawArrays},  // gles
  {"glDrawElements", (uintptr_t)&glDrawElements},  // gles
  {"glEnable", (uintptr_t)&glEnable},  // gles
  {"glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray},  // gles
  {"glFramebufferTexture2D", (uintptr_t)&glFramebufferTexture2D},  // gles
  {"glFrontFace", (uintptr_t)&glFrontFace},  // gles
  {"glGenBuffers", (uintptr_t)&glGenBuffers},  // gles
  {"glGenerateMipmap", (uintptr_t)&glGenerateMipmap},  // gles
  {"glGenFramebuffers", (uintptr_t)&glGenFramebuffers},  // gles
  {"glGenTextures", (uintptr_t)&glGenTextures},  // gles
  {"glGetAttachedShaders", (uintptr_t)&glGetAttachedShaders},  // gles
  {"glGetError", (uintptr_t)&glGetError},  // gles
  {"glGetIntegerv", (uintptr_t)&glGetIntegerv},  // gles
  {"glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog},  // gles
  {"glGetProgramiv", (uintptr_t)&glGetProgramiv},  // gles
  {"glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog},  // gles
  {"glGetShaderiv", (uintptr_t)&glGetShaderiv},  // gles
  {"glGetString", (uintptr_t)&glGetString},  // gles
  {"glGetUniformLocation", (uintptr_t)&glGetUniformLocation},  // gles
  {"glLinkProgram", (uintptr_t)&glLinkProgram},  // gles
  {"glReadPixels", (uintptr_t)&glReadPixels},  // gles
  {"glScissor", (uintptr_t)&glScissor},  // gles
  {"glShaderSource", (uintptr_t)&glShaderSource},  // gles
  {"glTexImage2D", (uintptr_t)&glTexImage2D},  // gles
  {"glTexParameteri", (uintptr_t)&glTexParameteri},  // gles
  {"glTexSubImage2D", (uintptr_t)&glTexSubImage2D},  // gles
  {"glUniform1i", (uintptr_t)&glUniform1i},  // gles
  {"glUniform4fv", (uintptr_t)&glUniform4fv},  // gles
  {"glUniform4iv", (uintptr_t)&glUniform4iv},  // gles
  {"glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv},  // gles
  {"glUseProgram", (uintptr_t)&glUseProgram},  // gles
  {"glVertexAttribPointer", (uintptr_t)&glVertexAttribPointer},  // gles
  {"glViewport", (uintptr_t)&glViewport},  // gles
  {"gmtime", (uintptr_t)&gmtime},  // pass
  // TODO {"kill", (uintptr_t)&stub_kill},  // <<< IMPLEMENTAR
  {"ldexp", (uintptr_t)&ldexp},  // pass
  // TODO {"lldiv", (uintptr_t)&stub_lldiv},  // <<< IMPLEMENTAR
  {"localtime", (uintptr_t)&localtime},  // pass
  {"log", (uintptr_t)&log},  // pass
  {"log10", (uintptr_t)&log10},  // pass
  // TODO {"log10f", (uintptr_t)&stub_log10f},  // <<< IMPLEMENTAR
  {"logf", (uintptr_t)&logf},  // pass
  // TODO {"longjmp", (uintptr_t)&stub_longjmp},  // <<< IMPLEMENTAR
  {"malloc", (uintptr_t)&malloc},  // pass
  {"memchr", (uintptr_t)&memchr},  // pass
  {"memcmp", (uintptr_t)&memcmp},  // pass
  {"memcpy", (uintptr_t)&memcpy},  // pass
  // TODO {"__memcpy_chk", (uintptr_t)&stub___memcpy_chk},  // <<< IMPLEMENTAR
  // TODO {"memfd_create", (uintptr_t)&stub_memfd_create},  // <<< IMPLEMENTAR
  {"memmove", (uintptr_t)&memmove},  // pass
  // TODO {"__memmove_chk", (uintptr_t)&stub___memmove_chk},  // <<< IMPLEMENTAR
  {"memset", (uintptr_t)&memset},  // pass
  // TODO {"__memset_chk", (uintptr_t)&stub___memset_chk},  // <<< IMPLEMENTAR
  {"mkdir", (uintptr_t)&mkdir},  // pass
  {"modf", (uintptr_t)&modf},  // pass
  {"opendir", (uintptr_t)&opendir},  // pass
  {"pipe", (uintptr_t)&pipe},  // pass
  {"posix_memalign", (uintptr_t)&posix_memalign},  // pass
  {"pow", (uintptr_t)&pow},  // pass
  {"powf", (uintptr_t)&powf},  // pass
  {"printf", (uintptr_t)&printf},  // pass
  {"pthread_attr_destroy", (uintptr_t)&pthread_attr_destroy_fake},  // pthread wrapper (core)
  {"pthread_attr_init", (uintptr_t)&pthread_attr_init_fake},  // pthread wrapper (core)
  {"pthread_attr_setdetachstate", (uintptr_t)&pthread_attr_setdetachstate_fake},  // pthread wrapper (core)
  {"pthread_attr_setstacksize", (uintptr_t)&pthread_attr_setstacksize_fake},  // pthread wrapper (core)
  {"pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake},  // pthread wrapper (core)
  {"pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_fake},  // pthread wrapper (core)
  {"pthread_cond_init", (uintptr_t)&pthread_cond_init_fake},  // pthread wrapper (core)
  {"pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake},  // pthread wrapper (core)
  {"pthread_create", (uintptr_t)&pthread_create_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_init", (uintptr_t)&pthread_mutexattr_init_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_settype", (uintptr_t)&pthread_mutexattr_settype_fake},  // pthread wrapper (core)
  {"pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake},  // pthread wrapper (core)
  {"pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake},  // pthread wrapper (core)
  {"pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake},  // pthread wrapper (core)
  {"pthread_mutex_trylock", (uintptr_t)&pthread_mutex_trylock_fake},  // pthread wrapper (core)
  {"pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake},  // pthread wrapper (core)
  {"pthread_self", (uintptr_t)&pthread_self_fake},  // pthread wrapper (core)
  {"pthread_setname_np", (uintptr_t)&pthread_setname_np_fake},  // pthread wrapper (core)
  {"pthread_setspecific", (uintptr_t)&pthread_setspecific_fake},  // pthread wrapper (core)
  {"puts", (uintptr_t)&puts},  // pass
  {"qsort", (uintptr_t)&qsort},  // pass
  {"rand", (uintptr_t)&rand},  // pass
  {"read", (uintptr_t)&read},  // pass
  // TODO {"__read_chk", (uintptr_t)&stub___read_chk},  // <<< IMPLEMENTAR
  {"readdir", (uintptr_t)&readdir},  // pass
  // TODO {"readlink", (uintptr_t)&stub_readlink},  // <<< IMPLEMENTAR
  {"realloc", (uintptr_t)&realloc},  // pass
  // TODO {"__register_atfork", (uintptr_t)&stub___register_atfork},  // <<< IMPLEMENTAR
  // TODO {"remove", (uintptr_t)&stub_remove},  // <<< IMPLEMENTAR
  {"rename", (uintptr_t)&rename},  // pass
  {"rmdir", (uintptr_t)&rmdir},  // pass
  // TODO {"sched_getaffinity", (uintptr_t)&stub_sched_getaffinity},  // <<< IMPLEMENTAR
  // TODO {"sched_getscheduler", (uintptr_t)&stub_sched_getscheduler},  // <<< IMPLEMENTAR
  // TODO {"sched_setaffinity", (uintptr_t)&stub_sched_setaffinity},  // <<< IMPLEMENTAR
  {"sched_yield", (uintptr_t)&sched_yield},  // pass
  // TODO {"setjmp", (uintptr_t)&stub_setjmp},  // <<< IMPLEMENTAR
  {"sin", (uintptr_t)&sin},  // pass
  // TODO {"sincos", (uintptr_t)&stub_sincos},  // <<< IMPLEMENTAR
  // TODO {"sincosf", (uintptr_t)&stub_sincosf},  // <<< IMPLEMENTAR
  {"sinf", (uintptr_t)&sinf},  // pass
  {"sleep", (uintptr_t)&sleep_shim},  // opensles_shim
  {"SL_IID_ANDROIDCONFIGURATION", (uintptr_t)&SL_IID_ANDROIDCONFIGURATION_shim},  // opensles_shim
  {"SL_IID_ANDROIDSIMPLEBUFFERQUEUE", (uintptr_t)&SL_IID_ANDROIDSIMPLEBUFFERQUEUE_shim},  // opensles_shim
  {"SL_IID_BUFFERQUEUE", (uintptr_t)&SL_IID_BUFFERQUEUE_shim},  // opensles_shim
  {"SL_IID_ENGINE", (uintptr_t)&SL_IID_ENGINE_shim},  // opensles_shim
  {"SL_IID_PLAY", (uintptr_t)&SL_IID_PLAY_shim},  // opensles_shim
  {"SL_IID_RECORD", (uintptr_t)&SL_IID_RECORD_shim},  // opensles_shim
  {"srand", (uintptr_t)&srand},  // pass
  {"sscanf", (uintptr_t)&sscanf},  // pass
  {"__stack_chk_fail", (uintptr_t)&__stack_chk_fail},  // abi
  {"stat", (uintptr_t)&stat},  // pass
  // TODO {"stderr", (uintptr_t)&stub_stderr},  // <<< IMPLEMENTAR
  {"strcasecmp", (uintptr_t)&strcasecmp},  // pass
  {"strcat", (uintptr_t)&strcat},  // pass
  {"strchr", (uintptr_t)&strchr},  // pass
  // TODO {"__strchr_chk", (uintptr_t)&stub___strchr_chk},  // <<< IMPLEMENTAR
  {"strcmp", (uintptr_t)&strcmp},  // pass
  {"strcpy", (uintptr_t)&strcpy},  // pass
  {"strerror", (uintptr_t)&strerror},  // pass
  {"strlen", (uintptr_t)&strlen},  // pass
  // TODO {"__strlen_chk", (uintptr_t)&stub___strlen_chk},  // <<< IMPLEMENTAR
  {"strncmp", (uintptr_t)&strncmp},  // pass
  {"strncpy", (uintptr_t)&strncpy},  // pass
  // TODO {"strpbrk", (uintptr_t)&stub_strpbrk},  // <<< IMPLEMENTAR
  {"strrchr", (uintptr_t)&strrchr},  // pass
  {"strstr", (uintptr_t)&strstr},  // pass
  {"strtod", (uintptr_t)&strtod},  // pass
  {"strtof", (uintptr_t)&strtof},  // pass
  // TODO {"strtoll", (uintptr_t)&stub_strtoll},  // <<< IMPLEMENTAR
  {"strtoul", (uintptr_t)&strtoul},  // pass
  // TODO {"__system_property_get", (uintptr_t)&stub___system_property_get},  // <<< IMPLEMENTAR
  {"tanf", (uintptr_t)&tanf},  // pass
  {"time", (uintptr_t)&time},  // pass
  // TODO {"__umask_chk", (uintptr_t)&stub___umask_chk},  // <<< IMPLEMENTAR
  {"unlink", (uintptr_t)&unlink},  // pass
  {"usleep", (uintptr_t)&usleep},  // pass
  // TODO {"utimes", (uintptr_t)&stub_utimes},  // <<< IMPLEMENTAR
  {"vsnprintf", (uintptr_t)&vsnprintf},  // pass
  // TODO {"__vsnprintf_chk", (uintptr_t)&stub___vsnprintf_chk},  // <<< IMPLEMENTAR
  {"vsprintf", (uintptr_t)&vsprintf},  // pass
  // TODO {"__vsprintf_chk", (uintptr_t)&stub___vsprintf_chk},  // <<< IMPLEMENTAR
  // TODO {"wmemchr", (uintptr_t)&stub_wmemchr},  // <<< IMPLEMENTAR
  {"write", (uintptr_t)&write},  // pass
  {"_ZdaPv", (uintptr_t)&_ZdaPv},  // cxx
  {"_ZdlPv", (uintptr_t)&_ZdlPv},  // cxx
  {"_Znam", (uintptr_t)&_Znam},  // cxx
  // TODO {"_ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4findEcm", (uintptr_t)&stub__ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4findEcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm", (uintptr_t)&stub__ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKc", (uintptr_t)&stub__ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKcm", (uintptr_t)&stub__ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEE3strEv", (uintptr_t)&stub__ZNKSt6__ndk115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEE3strEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk119__shared_weak_count13__get_deleterERKSt9type_info", (uintptr_t)&stub__ZNKSt6__ndk119__shared_weak_count13__get_deleterERKSt9type_info},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk120__vector_base_commonILb1EE20__throw_length_errorEv", (uintptr_t)&stub__ZNKSt6__ndk120__vector_base_commonILb1EE20__throw_length_errorEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk121__basic_string_commonILb1EE20__throw_length_errorEv", (uintptr_t)&stub__ZNKSt6__ndk121__basic_string_commonILb1EE20__throw_length_errorEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk123__match_any_but_newlineIcE6__execERNS_7__stateIcEE", (uintptr_t)&stub__ZNKSt6__ndk123__match_any_but_newlineIcE6__execERNS_7__stateIcEE},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk16locale4nameEv", (uintptr_t)&stub__ZNKSt6__ndk16locale4nameEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk16locale9has_facetERNS0_2idE", (uintptr_t)&stub__ZNKSt6__ndk16locale9has_facetERNS0_2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk16locale9use_facetERNS0_2idE", (uintptr_t)&stub__ZNKSt6__ndk16locale9use_facetERNS0_2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk18ios_base6getlocEv", (uintptr_t)&stub__ZNKSt6__ndk18ios_base6getlocEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk111this_thread9sleep_forERKNS_6chrono8durationIxNS_5ratioILl1ELl1000000000EEEEE", (uintptr_t)&stub__ZNSt6__ndk111this_thread9sleep_forERKNS_6chrono8durationIxNS_5ratioILl1ELl1000000000EEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5eraseEmm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5eraseEmm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKcm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmmc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmmc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKcm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7replaceEmmPKc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7replaceEmmPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSEc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSEc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSERKS5_", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSERKS5_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_mmRKS4_", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_mmRKS4_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112__next_primeEm", (uintptr_t)&stub__ZNSt6__ndk112__next_primeEm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_filebufIcNS_11char_traitsIcEEE4openEPKcj", (uintptr_t)&stub__ZNSt6__ndk113basic_filebufIcNS_11char_traitsIcEEE4openEPKcj},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_filebufIcNS_11char_traitsIcEEEC1Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_filebufIcNS_11char_traitsIcEEEC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_filebufIcNS_11char_traitsIcEEED1Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_filebufIcNS_11char_traitsIcEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEE4readEPcl", (uintptr_t)&stub__ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEE4readEPcl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE3putEc", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE3putEc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE5flushEv", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE5flushEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE5writeEPKcl", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE5writeEPKcl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryC1ERS3_", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryC1ERS3_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryD1Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEi", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEi},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEl", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEPKv", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEPKv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEPNS_15basic_streambufIcS2_EE", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEPNS_15basic_streambufIcS2_EE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113random_deviceC2ERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE", (uintptr_t)&stub__ZNSt6__ndk113random_deviceC2ERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113random_deviceclEv", (uintptr_t)&stub__ZNSt6__ndk113random_deviceclEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113random_deviceD1Ev", (uintptr_t)&stub__ZNSt6__ndk113random_deviceD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE5uflowEv", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE5uflowEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsgetnEPcl", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsgetnEPcl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsputnEPKcl", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsputnEPKcl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE9showmanycEv", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE9showmanycEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEEC2Ev", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEEC2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115__get_classnameEPKcb", (uintptr_t)&stub__ZNSt6__ndk115__get_classnameEPKcb},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115recursive_mutex4lockEv", (uintptr_t)&stub__ZNSt6__ndk115recursive_mutex4lockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115recursive_mutex6unlockEv", (uintptr_t)&stub__ZNSt6__ndk115recursive_mutex6unlockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115recursive_mutexC1Ev", (uintptr_t)&stub__ZNSt6__ndk115recursive_mutexC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115recursive_mutexD1Ev", (uintptr_t)&stub__ZNSt6__ndk115recursive_mutexD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115__thread_structC1Ev", (uintptr_t)&stub__ZNSt6__ndk115__thread_structC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115__thread_structD1Ev", (uintptr_t)&stub__ZNSt6__ndk115__thread_structD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk118condition_variable10notify_allEv", (uintptr_t)&stub__ZNSt6__ndk118condition_variable10notify_allEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk118condition_variable10notify_oneEv", (uintptr_t)&stub__ZNSt6__ndk118condition_variable10notify_oneEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk118condition_variable15__do_timed_waitERNS_11unique_lockINS_5mutexEEENS_6chrono10time_pointINS5_12system_clockENS5_8durationIxNS_5ratioILl1ELl1000000000EEEEEEE", (uintptr_t)&stub__ZNSt6__ndk118condition_variable15__do_timed_waitERNS_11unique_lockINS_5mutexEEENS_6chrono10time_pointINS5_12system_clockENS5_8durationIxNS_5ratioILl1ELl1000000000EEEEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk118condition_variable4waitERNS_11unique_lockINS_5mutexEEE", (uintptr_t)&stub__ZNSt6__ndk118condition_variable4waitERNS_11unique_lockINS_5mutexEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk118condition_variableD1Ev", (uintptr_t)&stub__ZNSt6__ndk118condition_variableD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_mutex_base11lock_sharedEv", (uintptr_t)&stub__ZNSt6__ndk119__shared_mutex_base11lock_sharedEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_mutex_base13unlock_sharedEv", (uintptr_t)&stub__ZNSt6__ndk119__shared_mutex_base13unlock_sharedEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_mutex_base4lockEv", (uintptr_t)&stub__ZNSt6__ndk119__shared_mutex_base4lockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_mutex_base6unlockEv", (uintptr_t)&stub__ZNSt6__ndk119__shared_mutex_base6unlockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_mutex_baseC1Ev", (uintptr_t)&stub__ZNSt6__ndk119__shared_mutex_baseC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_weak_count14__release_weakEv", (uintptr_t)&stub__ZNSt6__ndk119__shared_weak_count14__release_weakEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_weak_count4lockEv", (uintptr_t)&stub__ZNSt6__ndk119__shared_weak_count4lockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_weak_countD2Ev", (uintptr_t)&stub__ZNSt6__ndk119__shared_weak_countD2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__thread_local_dataEv", (uintptr_t)&stub__ZNSt6__ndk119__thread_local_dataEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk120__get_collation_nameEPKc", (uintptr_t)&stub__ZNSt6__ndk120__get_collation_nameEPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk120__throw_system_errorEiPKc", (uintptr_t)&stub__ZNSt6__ndk120__throw_system_errorEiPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk122__libcpp_verbose_abortEPKcz", (uintptr_t)&stub__ZNSt6__ndk122__libcpp_verbose_abortEPKcz},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk14stoiERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi", (uintptr_t)&stub__ZNSt6__ndk14stoiERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk14stolERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi", (uintptr_t)&stub__ZNSt6__ndk14stolERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15ctypeIcE2idE", (uintptr_t)&stub__ZNSt6__ndk15ctypeIcE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutex4lockEv", (uintptr_t)&stub__ZNSt6__ndk15mutex4lockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutex6unlockEv", (uintptr_t)&stub__ZNSt6__ndk15mutex6unlockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutex8try_lockEv", (uintptr_t)&stub__ZNSt6__ndk15mutex8try_lockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutexD1Ev", (uintptr_t)&stub__ZNSt6__ndk15mutexD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16chrono12steady_clock3nowEv", (uintptr_t)&stub__ZNSt6__ndk16chrono12steady_clock3nowEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16chrono12system_clock3nowEv", (uintptr_t)&stub__ZNSt6__ndk16chrono12system_clock3nowEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeaSERKS0_", (uintptr_t)&stub__ZNSt6__ndk16localeaSERKS0_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeC1ERKS0_", (uintptr_t)&stub__ZNSt6__ndk16localeC1ERKS0_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeC1Ev", (uintptr_t)&stub__ZNSt6__ndk16localeC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeD1Ev", (uintptr_t)&stub__ZNSt6__ndk16localeD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16__sortIRNS_6__lessIjjEEPjEEvT0_S5_T_", (uintptr_t)&stub__ZNSt6__ndk16__sortIRNS_6__lessIjjEEPjEEvT0_S5_T_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16thread4joinEv", (uintptr_t)&stub__ZNSt6__ndk16thread4joinEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16thread6detachEv", (uintptr_t)&stub__ZNSt6__ndk16thread6detachEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16threadD1Ev", (uintptr_t)&stub__ZNSt6__ndk16threadD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17codecvtIcc9mbstate_tE2idE", (uintptr_t)&stub__ZNSt6__ndk17codecvtIcc9mbstate_tE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17collateIcE2idE", (uintptr_t)&stub__ZNSt6__ndk17collateIcE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base4initEPv", (uintptr_t)&stub__ZNSt6__ndk18ios_base4initEPv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base5clearEj", (uintptr_t)&stub__ZNSt6__ndk18ios_base5clearEj},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk19basic_iosIcNS_11char_traitsIcEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk19basic_iosIcNS_11char_traitsIcEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk19to_stringEi", (uintptr_t)&stub__ZNSt6__ndk19to_stringEi},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk19to_stringEm", (uintptr_t)&stub__ZNSt6__ndk19to_stringEm},  // <<< IMPLEMENTAR
  {"_Znwm", (uintptr_t)&_Znwm},  // cxx
  // TODO {"_ZSt9terminatev", (uintptr_t)&stub__ZSt9terminatev},  // <<< IMPLEMENTAR
  // TODO {"_ZTTNSt6__ndk114basic_ifstreamIcNS_11char_traitsIcEEEE", (uintptr_t)&stub__ZTTNSt6__ndk114basic_ifstreamIcNS_11char_traitsIcEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTTNSt6__ndk118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE", (uintptr_t)&stub__ZTTNSt6__ndk118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev", (uintptr_t)&stub__ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev", (uintptr_t)&stub__ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTVNSt6__ndk114basic_ifstreamIcNS_11char_traitsIcEEEE", (uintptr_t)&stub__ZTVNSt6__ndk114basic_ifstreamIcNS_11char_traitsIcEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVNSt6__ndk115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEEE", (uintptr_t)&stub__ZTVNSt6__ndk115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVNSt6__ndk118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE", (uintptr_t)&stub__ZTVNSt6__ndk118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE},  // <<< IMPLEMENTAR
};
const int dynlib_functions_count = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);

// ===================== SIMBOLOS A IMPLEMENTAR =====================
//   acosf
//   asinf
//   __assert2
//   atanf
//   clock_nanosleep
//   cosh
//   dlclose
//   dlopen
//   dlsym
//   __emutls_get_address
//   ferror
//   fseeko
//   fsync
//   ftello
//   getauxval
//   kill
//   lldiv
//   log10f
//   longjmp
//   __memcpy_chk
//   memfd_create
//   __memmove_chk
//   __memset_chk
//   __read_chk
//   readlink
//   __register_atfork
//   remove
//   sched_getaffinity
//   sched_getscheduler
//   sched_setaffinity
//   setjmp
//   sincos
//   sincosf
//   stderr
//   __strchr_chk
//   __strlen_chk
//   strpbrk
//   strtoll
//   __system_property_get
//   __umask_chk
//   utimes
//   __vsnprintf_chk
//   __vsprintf_chk
//   wmemchr
//   _ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4findEcm
//   _ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm
//   _ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKc
//   _ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKcm
//   _ZNKSt6__ndk115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEE3strEv
//   _ZNKSt6__ndk119__shared_weak_count13__get_deleterERKSt9type_info
//   _ZNKSt6__ndk120__vector_base_commonILb1EE20__throw_length_errorEv
//   _ZNKSt6__ndk121__basic_string_commonILb1EE20__throw_length_errorEv
//   _ZNKSt6__ndk123__match_any_but_newlineIcE6__execERNS_7__stateIcEE
//   _ZNKSt6__ndk16locale4nameEv
//   _ZNKSt6__ndk16locale9has_facetERNS0_2idE
//   _ZNKSt6__ndk16locale9use_facetERNS0_2idE
//   _ZNKSt6__ndk18ios_base6getlocEv
//   _ZNSt6__ndk111this_thread9sleep_forERKNS_6chrono8durationIxNS_5ratioILl1ELl1000000000EEEEE
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5eraseEmm
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKcm
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmmc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKcm
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7replaceEmmPKc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSEc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSERKS5_
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_mmRKS4_
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED2Ev
//   _ZNSt6__ndk112__next_primeEm
//   _ZNSt6__ndk113basic_filebufIcNS_11char_traitsIcEEE4openEPKcj
//   _ZNSt6__ndk113basic_filebufIcNS_11char_traitsIcEEEC1Ev
//   _ZNSt6__ndk113basic_filebufIcNS_11char_traitsIcEEED1Ev
//   _ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEE4readEPcl
//   _ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED2Ev
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE3putEc
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE5flushEv
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE5writeEPKcl
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryC1ERS3_
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryD1Ev
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED2Ev
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEi
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEl
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEPKv
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEPNS_15basic_streambufIcS2_EE
//   _ZNSt6__ndk113random_deviceC2ERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE
//   _ZNSt6__ndk113random_deviceclEv
//   _ZNSt6__ndk113random_deviceD1Ev
//   _ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED2Ev
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE5uflowEv
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsgetnEPcl
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsputnEPKcl
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE9showmanycEv
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEEC2Ev
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEED2Ev
//   _ZNSt6__ndk115__get_classnameEPKcb
//   _ZNSt6__ndk115recursive_mutex4lockEv
//   _ZNSt6__ndk115recursive_mutex6unlockEv
//   _ZNSt6__ndk115recursive_mutexC1Ev
//   _ZNSt6__ndk115recursive_mutexD1Ev
//   _ZNSt6__ndk115__thread_structC1Ev
//   _ZNSt6__ndk115__thread_structD1Ev
//   _ZNSt6__ndk118condition_variable10notify_allEv
//   _ZNSt6__ndk118condition_variable10notify_oneEv
//   _ZNSt6__ndk118condition_variable15__do_timed_waitERNS_11unique_lockINS_5mutexEEENS_6chrono10time_pointINS5_12system_clockENS5_8durationIxNS_5ratioILl1ELl1000000000EEEEEEE
//   _ZNSt6__ndk118condition_variable4waitERNS_11unique_lockINS_5mutexEEE
//   _ZNSt6__ndk118condition_variableD1Ev
//   _ZNSt6__ndk119__shared_mutex_base11lock_sharedEv
//   _ZNSt6__ndk119__shared_mutex_base13unlock_sharedEv
//   _ZNSt6__ndk119__shared_mutex_base4lockEv
//   _ZNSt6__ndk119__shared_mutex_base6unlockEv
//   _ZNSt6__ndk119__shared_mutex_baseC1Ev
//   _ZNSt6__ndk119__shared_weak_count14__release_weakEv
//   _ZNSt6__ndk119__shared_weak_count4lockEv
//   _ZNSt6__ndk119__shared_weak_countD2Ev
//   _ZNSt6__ndk119__thread_local_dataEv
//   _ZNSt6__ndk120__get_collation_nameEPKc
//   _ZNSt6__ndk120__throw_system_errorEiPKc
//   _ZNSt6__ndk122__libcpp_verbose_abortEPKcz
//   _ZNSt6__ndk14stoiERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi
//   _ZNSt6__ndk14stolERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi
//   _ZNSt6__ndk15ctypeIcE2idE
//   _ZNSt6__ndk15mutex4lockEv
//   _ZNSt6__ndk15mutex6unlockEv
//   _ZNSt6__ndk15mutex8try_lockEv
//   _ZNSt6__ndk15mutexD1Ev
//   _ZNSt6__ndk16chrono12steady_clock3nowEv
//   _ZNSt6__ndk16chrono12system_clock3nowEv
//   _ZNSt6__ndk16localeaSERKS0_
//   _ZNSt6__ndk16localeC1ERKS0_
//   _ZNSt6__ndk16localeC1Ev
//   _ZNSt6__ndk16localeD1Ev
//   _ZNSt6__ndk16__sortIRNS_6__lessIjjEEPjEEvT0_S5_T_
//   _ZNSt6__ndk16thread4joinEv
//   _ZNSt6__ndk16thread6detachEv
//   _ZNSt6__ndk16threadD1Ev
//   _ZNSt6__ndk17codecvtIcc9mbstate_tE2idE
//   _ZNSt6__ndk17collateIcE2idE
//   _ZNSt6__ndk18ios_base4initEPv
//   _ZNSt6__ndk18ios_base5clearEj
//   _ZNSt6__ndk19basic_iosIcNS_11char_traitsIcEEED2Ev
//   _ZNSt6__ndk19to_stringEi
//   _ZNSt6__ndk19to_stringEm
//   _ZSt9terminatev
//   _ZTTNSt6__ndk114basic_ifstreamIcNS_11char_traitsIcEEEE
//   _ZTTNSt6__ndk118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE
//   _ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev
//   _ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev
//   _ZTVNSt6__ndk114basic_ifstreamIcNS_11char_traitsIcEEEE
//   _ZTVNSt6__ndk115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEEE
//   _ZTVNSt6__ndk118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE
