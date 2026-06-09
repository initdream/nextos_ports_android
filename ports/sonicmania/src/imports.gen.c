// imports.gen.c — GERADO por new-port.sh para 'sonicmania' (libsonicmania.so)
// 268 simbolos. Resolva os UNKNOWN no fim do arquivo.
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <GLES2/gl2.h>
#include "shims.h"
#include "android_shim.h"
#include "pthread_bridge.h"
#include "imports.h"
#include "so_util.h"
#include <stdio.h>

// === passthrough/pthread/shim: ligados automaticamente ===
DynLibFunction dynlib_functions[] = {
  {"glShaderSource", (uintptr_t)&my_glShaderSource},
  {"glBufferData", (uintptr_t)&my_glBufferData},
  {"glCreateShader", (uintptr_t)&my_glCreateShader},
  {"glGenBuffers", (uintptr_t)&my_glGenBuffers},
  {"glGenFramebuffers", (uintptr_t)&my_glGenFramebuffers},
  {"__errno", (uintptr_t)&__errno},
  {"__strlen_chk", (uintptr_t)&__strlen_chk},
  {"__read_chk", (uintptr_t)&__read_chk},
  {"__system_property_get", (uintptr_t)&__system_property_get},
  {"android_set_abort_message", (uintptr_t)&android_set_abort_message},
  {"AKeyEvent_getKeyCode", (uintptr_t)&AKeyEvent_getKeyCode},
  {"AKeyEvent_getAction", (uintptr_t)&AKeyEvent_getAction},
  {"AInputEvent_getDeviceId", (uintptr_t)&AInputEvent_getDeviceId},
  {"AInputEvent_getSource", (uintptr_t)&AInputEvent_getSource},
  {"AInputEvent_getType", (uintptr_t)&AInputEvent_getType},
  {"AMotionEvent_getAxisValue", (uintptr_t)&AMotionEvent_getAxisValue},
  {"AMotionEvent_getButtonState", (uintptr_t)&AMotionEvent_getButtonState},
  {"__sF", (uintptr_t)&__sF},
  {"pthread_mutex_init", (uintptr_t)&b_mutex_init},
  {"pthread_mutex_destroy", (uintptr_t)&b_mutex_destroy},
  {"pthread_mutex_lock", (uintptr_t)&b_mutex_lock},
  {"pthread_mutex_unlock", (uintptr_t)&b_mutex_unlock},
  {"pthread_mutex_trylock", (uintptr_t)&b_mutex_trylock},
  {"pthread_cond_init", (uintptr_t)&b_cond_init},
  {"pthread_cond_destroy", (uintptr_t)&b_cond_destroy},
  {"pthread_cond_wait", (uintptr_t)&b_cond_wait},
  {"pthread_cond_timedwait", (uintptr_t)&b_cond_timedwait},
  {"pthread_cond_signal", (uintptr_t)&b_cond_signal},
  {"pthread_cond_broadcast", (uintptr_t)&b_cond_broadcast},
  {"pthread_mutexattr_init", (uintptr_t)&b_mutexattr_init},
  {"pthread_mutexattr_destroy", (uintptr_t)&b_mutexattr_destroy},
  {"pthread_mutexattr_settype", (uintptr_t)&b_mutexattr_settype},
  {"pthread_once", (uintptr_t)&b_once},
  {"glDrawElements", (uintptr_t)&my_glDrawElements},
  {"glBindFramebuffer", (uintptr_t)&my_glBindFramebuffer},
  {"glCreateProgram", (uintptr_t)&my_glCreateProgram},
  {"glLinkProgram", (uintptr_t)&my_glLinkProgram},
  {"glCompileShader", (uintptr_t)&my_glCompileShader},
  {"AAssetManager_fromJava", (uintptr_t)&AAssetManager_fromJava},
  {"AAssetManager_open", (uintptr_t)&AAssetManager_open},
  {"AAsset_getLength", (uintptr_t)&AAsset_getLength},
  {"AAsset_getLength64", (uintptr_t)&AAsset_getLength64},
  {"AAsset_read", (uintptr_t)&AAsset_read},
  {"AAsset_seek", (uintptr_t)&AAsset_seek},
  {"AAsset_getBuffer", (uintptr_t)&AAsset_getBuffer},
  {"AAsset_close", (uintptr_t)&AAsset_close},
  {"__vsprintf_chk", (uintptr_t)&__vsprintf_chk},
  {"__vsnprintf_chk", (uintptr_t)&__vsnprintf_chk},
  {"__strcpy_chk", (uintptr_t)&__strcpy_chk},
  {"__strcat_chk", (uintptr_t)&__strcat_chk},
  {"__strncpy_chk", (uintptr_t)&__strncpy_chk},
  {"__memcpy_chk", (uintptr_t)&__memcpy_chk},
  {"__memset_chk", (uintptr_t)&__memset_chk},
  {"__memmove_chk", (uintptr_t)&__memmove_chk},
  {"strnlen", (uintptr_t)&strnlen},
  {"__register_atfork", (uintptr_t)&my_register_atfork},
  {"abort", (uintptr_t)&abort},  // pass
  // TODO {"acosf", (uintptr_t)&stub_acosf},  // <<< IMPLEMENTAR
  {"__android_log_print", (uintptr_t)&__android_log_print},  // liblog
  // TODO {"asinf", (uintptr_t)&stub_asinf},  // <<< IMPLEMENTAR
  {"atan2f", (uintptr_t)&atan2f},  // pass
  {"atoi", (uintptr_t)&atoi},  // pass
  {"calloc", (uintptr_t)&calloc},  // pass
  {"clock_gettime", (uintptr_t)&clock_gettime},  // pass
  // TODO {"clock_nanosleep", (uintptr_t)&stub_clock_nanosleep},  // <<< IMPLEMENTAR
  // TODO {"cosh", (uintptr_t)&stub_cosh},  // <<< IMPLEMENTAR
  {"__cxa_allocate_exception", (uintptr_t)&__cxa_allocate_exception},  // cxx
  {"__cxa_atexit", (uintptr_t)&__cxa_atexit},  // cxx
  {"__cxa_begin_catch", (uintptr_t)&__cxa_begin_catch},  // cxx
  {"__cxa_end_catch", (uintptr_t)&__cxa_end_catch},  // cxx
  {"__cxa_finalize", (uintptr_t)&__cxa_finalize},  // cxx
  {"__cxa_free_exception", (uintptr_t)&__cxa_free_exception},  // cxx
  {"__cxa_guard_abort", (uintptr_t)&__cxa_guard_abort},  // cxx
  {"__cxa_guard_acquire", (uintptr_t)&__cxa_guard_acquire},  // cxx
  {"__cxa_guard_release", (uintptr_t)&__cxa_guard_release},  // cxx
  {"__cxa_pure_virtual", (uintptr_t)&__cxa_pure_virtual},  // cxx
  {"__cxa_throw", (uintptr_t)&__cxa_throw},  // cxx
  // TODO {"dl_iterate_phdr", (uintptr_t)&stub_dl_iterate_phdr},  // <<< IMPLEMENTAR
  // TODO {"dlopen", (uintptr_t)&stub_dlopen},  // <<< IMPLEMENTAR
  // TODO {"dlsym", (uintptr_t)&stub_dlsym},  // <<< IMPLEMENTAR
  {"expf", (uintptr_t)&expf},  // pass
  {"fclose", (uintptr_t)&fclose},  // pass
  // TODO {"feof", (uintptr_t)&stub_feof},  // <<< IMPLEMENTAR
  // TODO {"ferror", (uintptr_t)&stub_ferror},  // <<< IMPLEMENTAR
  {"fflush", (uintptr_t)&fflush},  // pass
  {"fgetc", (uintptr_t)&fgetc},  // pass
  {"fopen", (uintptr_t)&fopen},  // pass
  {"fprintf", (uintptr_t)&fprintf},  // pass
  {"fread", (uintptr_t)&fread},  // pass
  {"free", (uintptr_t)&free},  // pass
  {"fseek", (uintptr_t)&fseek},  // pass
  {"fstat", (uintptr_t)&fstat},  // pass
  {"ftell", (uintptr_t)&ftell},  // pass
  {"fwrite", (uintptr_t)&fwrite},  // pass
  // TODO {"getauxval", (uintptr_t)&stub_getauxval},  // <<< IMPLEMENTAR
  {"glActiveTexture", (uintptr_t)&my_glActiveTexture},  // gles
  {"glBindTexture", (uintptr_t)&my_glBindTexture},  // gles
  {"glClear", (uintptr_t)&my_glClear},  // gles
  {"glClearColor", (uintptr_t)&glClearColor},  // gles
  {"glDrawArrays", (uintptr_t)&my_glDrawArrays},  // gles
  {"glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray},  // gles
  {"glGenTextures", (uintptr_t)&my_glGenTextures},  // gles
  {"glGetError", (uintptr_t)&glGetError},  // gles
  {"glGetString", (uintptr_t)&glGetString},  // gles
  {"glGetUniformLocation", (uintptr_t)&glGetUniformLocation},  // gles
  {"glTexImage2D", (uintptr_t)&my_glTexImage2D},  // gles
  {"glTexParameteri", (uintptr_t)&glTexParameteri},  // gles
  {"glTexSubImage2D", (uintptr_t)&my_glTexSubImage2D},  // gles
  {"glUniform1i", (uintptr_t)&glUniform1i},  // gles
  {"glUniform2fv", (uintptr_t)&glUniform2fv},  // gles
  {"glUseProgram", (uintptr_t)&my_glUseProgram},  // gles
  {"glVertexAttribPointer", (uintptr_t)&my_glVertexAttribPointer},  // gles
  {"glViewport", (uintptr_t)&my_glViewport},  // gles
  {"__gxx_personality_v0", (uintptr_t)&__gxx_personality_v0},  // cxx
  // TODO {"ldexpf", (uintptr_t)&stub_ldexpf},  // <<< IMPLEMENTAR
  {"localtime", (uintptr_t)&localtime},  // pass
  {"logf", (uintptr_t)&logf},  // pass
  {"malloc", (uintptr_t)&malloc},  // pass
  {"memchr", (uintptr_t)&memchr},  // pass
  {"memcmp", (uintptr_t)&memcmp},  // pass
  {"memcpy", (uintptr_t)&memcpy},  // pass
  // TODO {"__memcpy_chk", (uintptr_t)&stub___memcpy_chk},  // <<< IMPLEMENTAR
  {"memmove", (uintptr_t)&memmove},  // pass
  {"memset", (uintptr_t)&memset},  // pass
  {"mktime", (uintptr_t)&mktime},  // pass
  {"pow", (uintptr_t)&pow},  // pass
  {"printf", (uintptr_t)&printf},  // pass
  {"pthread_create", (uintptr_t)&pthread_create_fake},  // pthread wrapper (core)
  {"pthread_rwlock_rdlock", (uintptr_t)&pthread_rwlock_rdlock_fake},  // pthread wrapper (core)
  {"pthread_rwlock_unlock", (uintptr_t)&pthread_rwlock_unlock_fake},  // pthread wrapper (core)
  {"pthread_rwlock_wrlock", (uintptr_t)&pthread_rwlock_wrlock_fake},  // pthread wrapper (core)
  {"pthread_setspecific", (uintptr_t)&pthread_setspecific_fake},  // pthread wrapper (core)
  {"puts", (uintptr_t)&puts},  // pass
  {"qsort", (uintptr_t)&qsort},  // pass
  {"rand", (uintptr_t)&rand},  // pass
  // TODO {"__read_chk", (uintptr_t)&stub___read_chk},  // <<< IMPLEMENTAR
  {"realloc", (uintptr_t)&realloc},  // pass
  // TODO {"__register_atfork", (uintptr_t)&stub___register_atfork},  // <<< IMPLEMENTAR
  // TODO {"remove", (uintptr_t)&stub_remove},  // <<< IMPLEMENTAR
  // TODO {"sched_getscheduler", (uintptr_t)&stub_sched_getscheduler},  // <<< IMPLEMENTAR
  // TODO {"__sF", (uintptr_t)&stub___sF},  // <<< IMPLEMENTAR
  {"sin", (uintptr_t)&sin},  // pass
  // TODO {"sincos", (uintptr_t)&stub_sincos},  // <<< IMPLEMENTAR
  // TODO {"sincosf", (uintptr_t)&stub_sincosf},  // <<< IMPLEMENTAR
  {"sinf", (uintptr_t)&sinf},  // pass
  {"SL_IID_ANDROIDCONFIGURATION", (uintptr_t)&SL_IID_ANDROIDCONFIGURATION_shim},  // opensles_shim
  {"SL_IID_ANDROIDSIMPLEBUFFERQUEUE", (uintptr_t)&SL_IID_ANDROIDSIMPLEBUFFERQUEUE_shim},  // opensles_shim
  {"SL_IID_BUFFERQUEUE", (uintptr_t)&SL_IID_BUFFERQUEUE_shim},  // opensles_shim
  {"SL_IID_ENGINE", (uintptr_t)&SL_IID_ENGINE_shim},  // opensles_shim
  {"SL_IID_PLAY", (uintptr_t)&SL_IID_PLAY_shim},  // opensles_shim
  {"SL_IID_RECORD", (uintptr_t)&SL_IID_RECORD_shim},  // opensles_shim
  {"__stack_chk_fail", (uintptr_t)&__stack_chk_fail},  // abi
  // TODO {"__strcat_chk", (uintptr_t)&stub___strcat_chk},  // <<< IMPLEMENTAR
  {"strcmp", (uintptr_t)&strcmp},  // pass
  {"strcpy", (uintptr_t)&strcpy},  // pass
  // TODO {"__strcpy_chk", (uintptr_t)&stub___strcpy_chk},  // <<< IMPLEMENTAR
  {"strftime", (uintptr_t)&strftime},  // pass
  {"strlen", (uintptr_t)&strlen},  // pass
  // TODO {"__strlen_chk", (uintptr_t)&stub___strlen_chk},  // <<< IMPLEMENTAR
  {"strncmp", (uintptr_t)&strncmp},  // pass
  {"strncpy", (uintptr_t)&strncpy},  // pass
  // TODO {"strnlen", (uintptr_t)&stub_strnlen},  // <<< IMPLEMENTAR
  {"strstr", (uintptr_t)&strstr},  // pass
  // TODO {"__system_property_get", (uintptr_t)&stub___system_property_get},  // <<< IMPLEMENTAR
  {"tanf", (uintptr_t)&tanf},  // pass
  {"time", (uintptr_t)&time},  // pass
  {"usleep", (uintptr_t)&usleep},  // pass
  // TODO {"__vsprintf_chk", (uintptr_t)&stub___vsprintf_chk},  // <<< IMPLEMENTAR
  // TODO {"wmemset", (uintptr_t)&stub_wmemset},  // <<< IMPLEMENTAR
  {"_ZdaPv", (uintptr_t)&_ZdaPv},  // cxx
  {"_ZdlPv", (uintptr_t)&_ZdlPv},  // cxx
  {"_Znam", (uintptr_t)&_Znam},  // cxx
  // TODO {"_ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm", (uintptr_t)&stub__ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKcm", (uintptr_t)&stub__ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk114__codecvt_utf8IwE10do_unshiftER9mbstate_tPcS4_RS4_", (uintptr_t)&stub__ZNKSt6__ndk114__codecvt_utf8IwE10do_unshiftER9mbstate_tPcS4_RS4_},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk114__codecvt_utf8IwE11do_encodingEv", (uintptr_t)&stub__ZNKSt6__ndk114__codecvt_utf8IwE11do_encodingEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk114__codecvt_utf8IwE13do_max_lengthEv", (uintptr_t)&stub__ZNKSt6__ndk114__codecvt_utf8IwE13do_max_lengthEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk114__codecvt_utf8IwE16do_always_noconvEv", (uintptr_t)&stub__ZNKSt6__ndk114__codecvt_utf8IwE16do_always_noconvEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk114__codecvt_utf8IwE5do_inER9mbstate_tPKcS5_RS5_PwS7_RS7_", (uintptr_t)&stub__ZNKSt6__ndk114__codecvt_utf8IwE5do_inER9mbstate_tPKcS5_RS5_PwS7_RS7_},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk114__codecvt_utf8IwE6do_outER9mbstate_tPKwS5_RS5_PcS7_RS7_", (uintptr_t)&stub__ZNKSt6__ndk114__codecvt_utf8IwE6do_outER9mbstate_tPKwS5_RS5_PcS7_RS7_},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk114__codecvt_utf8IwE9do_lengthER9mbstate_tPKcS5_m", (uintptr_t)&stub__ZNKSt6__ndk114__codecvt_utf8IwE9do_lengthER9mbstate_tPKcS5_m},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk120__vector_base_commonILb1EE20__throw_length_errorEv", (uintptr_t)&stub__ZNKSt6__ndk120__vector_base_commonILb1EE20__throw_length_errorEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk120__vector_base_commonILb1EE20__throw_out_of_rangeEv", (uintptr_t)&stub__ZNKSt6__ndk120__vector_base_commonILb1EE20__throw_out_of_rangeEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk121__basic_string_commonILb1EE20__throw_length_errorEv", (uintptr_t)&stub__ZNKSt6__ndk121__basic_string_commonILb1EE20__throw_length_errorEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk16locale9use_facetERNS0_2idE", (uintptr_t)&stub__ZNKSt6__ndk16locale9use_facetERNS0_2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk18ios_base6getlocEv", (uintptr_t)&stub__ZNKSt6__ndk18ios_base6getlocEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt9exception4whatEv", (uintptr_t)&stub__ZNKSt9exception4whatEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt11logic_errorC2EPKc", (uintptr_t)&stub__ZNSt11logic_errorC2EPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt11range_errorD1Ev", (uintptr_t)&stub__ZNSt11range_errorD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt12length_errorD1Ev", (uintptr_t)&stub__ZNSt12length_errorD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt13runtime_errorC2EPKc", (uintptr_t)&stub__ZNSt13runtime_errorC2EPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk111this_thread9sleep_forERKNS_6chrono8durationIxNS_5ratioILl1ELl1000000000EEEEE", (uintptr_t)&stub__ZNSt6__ndk111this_thread9sleep_forERKNS_6chrono8durationIxNS_5ratioILl1ELl1000000000EEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6resizeEmc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6resizeEmc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_mmRKS4_", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_mmRKS4_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6appendEPKwm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6appendEPKwm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6resizeEmw", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6resizeEmw},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE9__grow_byEmmmmmm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE9__grow_byEmmmmmm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE9push_backEw", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE9push_backEw},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEEC1ERKS5_", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEEC1ERKS5_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112__next_primeEm", (uintptr_t)&stub__ZNSt6__ndk112__next_primeEm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED0Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED1Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE3putEc", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE3putEc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE5flushEv", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE5flushEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryC1ERS3_", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryC1ERS3_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryD1Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEi", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEi},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEl", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEPKv", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEPKv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEE6sentryC1ERS3_", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEE6sentryC1ERS3_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEE6sentryD1Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEE6sentryD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED0Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED1Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEElsEi", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEElsEi},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEElsEPKv", (uintptr_t)&stub__ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEElsEPKv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED0Ev", (uintptr_t)&stub__ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED1Ev", (uintptr_t)&stub__ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE4syncEv", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE4syncEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE5imbueERKNS_6localeE", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE5imbueERKNS_6localeE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE5uflowEv", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE5uflowEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6setbufEPcl", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6setbufEPcl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsgetnEPcl", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsgetnEPcl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsputnEPKcl", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsputnEPKcl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE9showmanycEv", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE9showmanycEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEEC2Ev", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEEC2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE4syncEv", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE4syncEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE5imbueERKNS_6localeE", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE5imbueERKNS_6localeE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE5uflowEv", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE5uflowEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE6setbufEPwl", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE6setbufEPwl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE6xsgetnEPwl", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE6xsgetnEPwl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE6xsputnEPKwl", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE6xsputnEPKwl},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE9showmanycEv", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE9showmanycEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEEC2Ev", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEEC2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115recursive_mutex4lockEv", (uintptr_t)&stub__ZNSt6__ndk115recursive_mutex4lockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115recursive_mutex6unlockEv", (uintptr_t)&stub__ZNSt6__ndk115recursive_mutex6unlockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115recursive_mutexC1Ev", (uintptr_t)&stub__ZNSt6__ndk115recursive_mutexC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115recursive_mutexD1Ev", (uintptr_t)&stub__ZNSt6__ndk115recursive_mutexD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115__thread_structC1Ev", (uintptr_t)&stub__ZNSt6__ndk115__thread_structC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115__thread_structD1Ev", (uintptr_t)&stub__ZNSt6__ndk115__thread_structD1Ev},  // <<< IMPLEMENTAR
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
  // TODO {"_ZNSt6__ndk120__throw_system_errorEiPKc", (uintptr_t)&stub__ZNSt6__ndk120__throw_system_errorEiPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk14coutE", (uintptr_t)&stub__ZNSt6__ndk14coutE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15ctypeIcE2idE", (uintptr_t)&stub__ZNSt6__ndk15ctypeIcE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15ctypeIwE2idE", (uintptr_t)&stub__ZNSt6__ndk15ctypeIwE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutex4lockEv", (uintptr_t)&stub__ZNSt6__ndk15mutex4lockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutex6unlockEv", (uintptr_t)&stub__ZNSt6__ndk15mutex6unlockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutex8try_lockEv", (uintptr_t)&stub__ZNSt6__ndk15mutex8try_lockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutexD1Ev", (uintptr_t)&stub__ZNSt6__ndk15mutexD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16chrono12steady_clock3nowEv", (uintptr_t)&stub__ZNSt6__ndk16chrono12steady_clock3nowEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16locale5facet16__on_zero_sharedEv", (uintptr_t)&stub__ZNSt6__ndk16locale5facet16__on_zero_sharedEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeD1Ev", (uintptr_t)&stub__ZNSt6__ndk16localeD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16thread6detachEv", (uintptr_t)&stub__ZNSt6__ndk16thread6detachEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16threadD1Ev", (uintptr_t)&stub__ZNSt6__ndk16threadD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17codecvtIwc9mbstate_tEC2Em", (uintptr_t)&stub__ZNSt6__ndk17codecvtIwc9mbstate_tEC2Em},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17codecvtIwc9mbstate_tED2Ev", (uintptr_t)&stub__ZNSt6__ndk17codecvtIwc9mbstate_tED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base33__set_badbit_and_consider_rethrowEv", (uintptr_t)&stub__ZNSt6__ndk18ios_base33__set_badbit_and_consider_rethrowEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base4initEPv", (uintptr_t)&stub__ZNSt6__ndk18ios_base4initEPv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base5clearEj", (uintptr_t)&stub__ZNSt6__ndk18ios_base5clearEj},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk19basic_iosIcNS_11char_traitsIcEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk19basic_iosIcNS_11char_traitsIcEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk19basic_iosIwNS_11char_traitsIwEEED2Ev", (uintptr_t)&stub__ZNSt6__ndk19basic_iosIwNS_11char_traitsIwEEED2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt9exceptionD2Ev", (uintptr_t)&stub__ZNSt9exceptionD2Ev},  // <<< IMPLEMENTAR
  {"_Znwm", (uintptr_t)&_Znwm},  // cxx
  // TODO {"_ZSt17__throw_bad_allocv", (uintptr_t)&stub__ZSt17__throw_bad_allocv},  // <<< IMPLEMENTAR
  // TODO {"_ZSt9terminatev", (uintptr_t)&stub__ZSt9terminatev},  // <<< IMPLEMENTAR
  // TODO {"_ZThn16_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED0Ev", (uintptr_t)&stub__ZThn16_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZThn16_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED1Ev", (uintptr_t)&stub__ZThn16_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk113basic_istreamIcNS_11char_traitsIcEEEE", (uintptr_t)&stub__ZTINSt6__ndk113basic_istreamIcNS_11char_traitsIcEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEEE", (uintptr_t)&stub__ZTINSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEEE", (uintptr_t)&stub__ZTINSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEEE", (uintptr_t)&stub__ZTINSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk114__codecvt_utf8IwEE", (uintptr_t)&stub__ZTINSt6__ndk114__codecvt_utf8IwEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk115basic_streambufIcNS_11char_traitsIcEEEE", (uintptr_t)&stub__ZTINSt6__ndk115basic_streambufIcNS_11char_traitsIcEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk115basic_streambufIwNS_11char_traitsIwEEEE", (uintptr_t)&stub__ZTINSt6__ndk115basic_streambufIwNS_11char_traitsIwEEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk119__shared_weak_countE", (uintptr_t)&stub__ZTINSt6__ndk119__shared_weak_countE},  // <<< IMPLEMENTAR
  // TODO {"_ZTISt11range_error", (uintptr_t)&stub__ZTISt11range_error},  // <<< IMPLEMENTAR
  // TODO {"_ZTISt12length_error", (uintptr_t)&stub__ZTISt12length_error},  // <<< IMPLEMENTAR
  // TODO {"_ZTISt9exception", (uintptr_t)&stub__ZTISt9exception},  // <<< IMPLEMENTAR
  // TODO {"_ZTv0_n24_NSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED0Ev", (uintptr_t)&stub__ZTv0_n24_NSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTv0_n24_NSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED1Ev", (uintptr_t)&stub__ZTv0_n24_NSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev", (uintptr_t)&stub__ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev", (uintptr_t)&stub__ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTv0_n24_NSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED0Ev", (uintptr_t)&stub__ZTv0_n24_NSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTv0_n24_NSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED1Ev", (uintptr_t)&stub__ZTv0_n24_NSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTv0_n24_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED0Ev", (uintptr_t)&stub__ZTv0_n24_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED0Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTv0_n24_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED1Ev", (uintptr_t)&stub__ZTv0_n24_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv117__class_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv117__class_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv119__pointer_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv119__pointer_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv120__function_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv120__function_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv120__si_class_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv120__si_class_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv121__vmi_class_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv121__vmi_class_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVSt11range_error", (uintptr_t)&stub__ZTVSt11range_error},  // <<< IMPLEMENTAR
  // TODO {"_ZTVSt12length_error", (uintptr_t)&stub__ZTVSt12length_error},  // <<< IMPLEMENTAR
};
size_t dynlib_numfunctions = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);

// ===================== SIMBOLOS A IMPLEMENTAR =====================
//   acosf
//   asinf
//   clock_nanosleep
//   cosh
//   dl_iterate_phdr
//   dlopen
//   dlsym
//   feof
//   ferror
//   getauxval
//   ldexpf
//   __memcpy_chk
//   __read_chk
//   __register_atfork
//   remove
//   sched_getscheduler
//   __sF
//   sincos
//   sincosf
//   __strcat_chk
//   __strcpy_chk
//   __strlen_chk
//   strnlen
//   __system_property_get
//   __vsprintf_chk
//   wmemset
//   _ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm
//   _ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKcm
//   _ZNKSt6__ndk114__codecvt_utf8IwE10do_unshiftER9mbstate_tPcS4_RS4_
//   _ZNKSt6__ndk114__codecvt_utf8IwE11do_encodingEv
//   _ZNKSt6__ndk114__codecvt_utf8IwE13do_max_lengthEv
//   _ZNKSt6__ndk114__codecvt_utf8IwE16do_always_noconvEv
//   _ZNKSt6__ndk114__codecvt_utf8IwE5do_inER9mbstate_tPKcS5_RS5_PwS7_RS7_
//   _ZNKSt6__ndk114__codecvt_utf8IwE6do_outER9mbstate_tPKwS5_RS5_PcS7_RS7_
//   _ZNKSt6__ndk114__codecvt_utf8IwE9do_lengthER9mbstate_tPKcS5_m
//   _ZNKSt6__ndk120__vector_base_commonILb1EE20__throw_length_errorEv
//   _ZNKSt6__ndk120__vector_base_commonILb1EE20__throw_out_of_rangeEv
//   _ZNKSt6__ndk121__basic_string_commonILb1EE20__throw_length_errorEv
//   _ZNKSt6__ndk16locale9use_facetERNS0_2idE
//   _ZNKSt6__ndk18ios_base6getlocEv
//   _ZNKSt9exception4whatEv
//   _ZNSt11logic_errorC2EPKc
//   _ZNSt11range_errorD1Ev
//   _ZNSt12length_errorD1Ev
//   _ZNSt13runtime_errorC2EPKc
//   _ZNSt6__ndk111this_thread9sleep_forERKNS_6chrono8durationIxNS_5ratioILl1ELl1000000000EEEEE
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6resizeEmc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_mmRKS4_
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED2Ev
//   _ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6appendEPKwm
//   _ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6resizeEmw
//   _ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE9__grow_byEmmmmmm
//   _ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE9push_backEw
//   _ZNSt6__ndk112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEEC1ERKS5_
//   _ZNSt6__ndk112__next_primeEm
//   _ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED0Ev
//   _ZNSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED1Ev
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE3putEc
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE5flushEv
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryC1ERS3_
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEE6sentryD1Ev
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEi
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEl
//   _ZNSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEElsEPKv
//   _ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEE6sentryC1ERS3_
//   _ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEE6sentryD1Ev
//   _ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED0Ev
//   _ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED1Ev
//   _ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED2Ev
//   _ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEElsEi
//   _ZNSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEElsEPKv
//   _ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED0Ev
//   _ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED1Ev
//   _ZNSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED2Ev
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE4syncEv
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE5imbueERKNS_6localeE
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE5uflowEv
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6setbufEPcl
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsgetnEPcl
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE6xsputnEPKcl
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEE9showmanycEv
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEEC2Ev
//   _ZNSt6__ndk115basic_streambufIcNS_11char_traitsIcEEED2Ev
//   _ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE4syncEv
//   _ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE5imbueERKNS_6localeE
//   _ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE5uflowEv
//   _ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE6setbufEPwl
//   _ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE6xsgetnEPwl
//   _ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE6xsputnEPKwl
//   _ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEE9showmanycEv
//   _ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEEC2Ev
//   _ZNSt6__ndk115basic_streambufIwNS_11char_traitsIwEEED2Ev
//   _ZNSt6__ndk115recursive_mutex4lockEv
//   _ZNSt6__ndk115recursive_mutex6unlockEv
//   _ZNSt6__ndk115recursive_mutexC1Ev
//   _ZNSt6__ndk115recursive_mutexD1Ev
//   _ZNSt6__ndk115__thread_structC1Ev
//   _ZNSt6__ndk115__thread_structD1Ev
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
//   _ZNSt6__ndk120__throw_system_errorEiPKc
//   _ZNSt6__ndk14coutE
//   _ZNSt6__ndk15ctypeIcE2idE
//   _ZNSt6__ndk15ctypeIwE2idE
//   _ZNSt6__ndk15mutex4lockEv
//   _ZNSt6__ndk15mutex6unlockEv
//   _ZNSt6__ndk15mutex8try_lockEv
//   _ZNSt6__ndk15mutexD1Ev
//   _ZNSt6__ndk16chrono12steady_clock3nowEv
//   _ZNSt6__ndk16locale5facet16__on_zero_sharedEv
//   _ZNSt6__ndk16localeD1Ev
//   _ZNSt6__ndk16thread6detachEv
//   _ZNSt6__ndk16threadD1Ev
//   _ZNSt6__ndk17codecvtIwc9mbstate_tEC2Em
//   _ZNSt6__ndk17codecvtIwc9mbstate_tED2Ev
//   _ZNSt6__ndk18ios_base33__set_badbit_and_consider_rethrowEv
//   _ZNSt6__ndk18ios_base4initEPv
//   _ZNSt6__ndk18ios_base5clearEj
//   _ZNSt6__ndk19basic_iosIcNS_11char_traitsIcEEED2Ev
//   _ZNSt6__ndk19basic_iosIwNS_11char_traitsIwEEED2Ev
//   _ZNSt9exceptionD2Ev
//   _ZSt17__throw_bad_allocv
//   _ZSt9terminatev
//   _ZThn16_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED0Ev
//   _ZThn16_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED1Ev
//   _ZTINSt6__ndk113basic_istreamIcNS_11char_traitsIcEEEE
//   _ZTINSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEEE
//   _ZTINSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEEE
//   _ZTINSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEEE
//   _ZTINSt6__ndk114__codecvt_utf8IwEE
//   _ZTINSt6__ndk115basic_streambufIcNS_11char_traitsIcEEEE
//   _ZTINSt6__ndk115basic_streambufIwNS_11char_traitsIwEEEE
//   _ZTINSt6__ndk119__shared_weak_countE
//   _ZTISt11range_error
//   _ZTISt12length_error
//   _ZTISt9exception
//   _ZTv0_n24_NSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED0Ev
//   _ZTv0_n24_NSt6__ndk113basic_istreamIcNS_11char_traitsIcEEED1Ev
//   _ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED0Ev
//   _ZTv0_n24_NSt6__ndk113basic_ostreamIcNS_11char_traitsIcEEED1Ev
//   _ZTv0_n24_NSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED0Ev
//   _ZTv0_n24_NSt6__ndk113basic_ostreamIwNS_11char_traitsIwEEED1Ev
//   _ZTv0_n24_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED0Ev
//   _ZTv0_n24_NSt6__ndk114basic_iostreamIcNS_11char_traitsIcEEED1Ev
//   _ZTVN10__cxxabiv117__class_type_infoE
//   _ZTVN10__cxxabiv119__pointer_type_infoE
//   _ZTVN10__cxxabiv120__function_type_infoE
//   _ZTVN10__cxxabiv120__si_class_type_infoE
//   _ZTVN10__cxxabiv121__vmi_class_type_infoE
//   _ZTVSt11range_error
//   _ZTVSt12length_error
