// imports.gen.c — GERADO por new-port.sh para 'revc' (libreVC.so)
// 323 simbolos. Resolva os UNKNOWN no fim do arquivo.
#include "imports.h"
#include "so_util.h"
#include <stdio.h>

// === passthrough/pthread/shim: ligados automaticamente ===
DynLibFunction dynlib_functions[] = {
  {"access", (uintptr_t)&access},  // pass
  // TODO {"acosf", (uintptr_t)&stub_acosf},  // <<< IMPLEMENTAR
  // TODO {"alBufferData", (uintptr_t)&stub_alBufferData},  // <<< IMPLEMENTAR
  // TODO {"alBufferiv", (uintptr_t)&stub_alBufferiv},  // <<< IMPLEMENTAR
  // TODO {"alcCloseDevice", (uintptr_t)&stub_alcCloseDevice},  // <<< IMPLEMENTAR
  // TODO {"alcCreateContext", (uintptr_t)&stub_alcCreateContext},  // <<< IMPLEMENTAR
  // TODO {"alcDestroyContext", (uintptr_t)&stub_alcDestroyContext},  // <<< IMPLEMENTAR
  // TODO {"alcGetIntegerv", (uintptr_t)&stub_alcGetIntegerv},  // <<< IMPLEMENTAR
  // TODO {"alcGetString", (uintptr_t)&stub_alcGetString},  // <<< IMPLEMENTAR
  // TODO {"alcIsExtensionPresent", (uintptr_t)&stub_alcIsExtensionPresent},  // <<< IMPLEMENTAR
  // TODO {"alcMakeContextCurrent", (uintptr_t)&stub_alcMakeContextCurrent},  // <<< IMPLEMENTAR
  // TODO {"alcOpenDevice", (uintptr_t)&stub_alcOpenDevice},  // <<< IMPLEMENTAR
  // TODO {"alcSuspendContext", (uintptr_t)&stub_alcSuspendContext},  // <<< IMPLEMENTAR
  // TODO {"alDeleteBuffers", (uintptr_t)&stub_alDeleteBuffers},  // <<< IMPLEMENTAR
  // TODO {"alDeleteSources", (uintptr_t)&stub_alDeleteSources},  // <<< IMPLEMENTAR
  // TODO {"alDistanceModel", (uintptr_t)&stub_alDistanceModel},  // <<< IMPLEMENTAR
  // TODO {"alGenBuffers", (uintptr_t)&stub_alGenBuffers},  // <<< IMPLEMENTAR
  // TODO {"alGenSources", (uintptr_t)&stub_alGenSources},  // <<< IMPLEMENTAR
  // TODO {"alGetEnumValue", (uintptr_t)&stub_alGetEnumValue},  // <<< IMPLEMENTAR
  // TODO {"alGetError", (uintptr_t)&stub_alGetError},  // <<< IMPLEMENTAR
  // TODO {"alGetProcAddress", (uintptr_t)&stub_alGetProcAddress},  // <<< IMPLEMENTAR
  // TODO {"alGetSourcei", (uintptr_t)&stub_alGetSourcei},  // <<< IMPLEMENTAR
  // TODO {"alGetString", (uintptr_t)&stub_alGetString},  // <<< IMPLEMENTAR
  // TODO {"alIsExtensionPresent", (uintptr_t)&stub_alIsExtensionPresent},  // <<< IMPLEMENTAR
  // TODO {"alListener3f", (uintptr_t)&stub_alListener3f},  // <<< IMPLEMENTAR
  // TODO {"alListenerf", (uintptr_t)&stub_alListenerf},  // <<< IMPLEMENTAR
  // TODO {"alListenerfv", (uintptr_t)&stub_alListenerfv},  // <<< IMPLEMENTAR
  // TODO {"alSource3f", (uintptr_t)&stub_alSource3f},  // <<< IMPLEMENTAR
  // TODO {"alSource3i", (uintptr_t)&stub_alSource3i},  // <<< IMPLEMENTAR
  // TODO {"alSourcef", (uintptr_t)&stub_alSourcef},  // <<< IMPLEMENTAR
  // TODO {"alSourcei", (uintptr_t)&stub_alSourcei},  // <<< IMPLEMENTAR
  // TODO {"alSourcePause", (uintptr_t)&stub_alSourcePause},  // <<< IMPLEMENTAR
  // TODO {"alSourcePlay", (uintptr_t)&stub_alSourcePlay},  // <<< IMPLEMENTAR
  // TODO {"alSourceQueueBuffers", (uintptr_t)&stub_alSourceQueueBuffers},  // <<< IMPLEMENTAR
  // TODO {"alSourceStop", (uintptr_t)&stub_alSourceStop},  // <<< IMPLEMENTAR
  // TODO {"alSourceUnqueueBuffers", (uintptr_t)&stub_alSourceUnqueueBuffers},  // <<< IMPLEMENTAR
  {"__android_log_print", (uintptr_t)&__android_log_print},  // liblog
  {"__android_log_write", (uintptr_t)&__android_log_write},  // liblog
  // TODO {"asinf", (uintptr_t)&stub_asinf},  // <<< IMPLEMENTAR
  {"atan2f", (uintptr_t)&atan2f},  // pass
  // TODO {"atanf", (uintptr_t)&stub_atanf},  // <<< IMPLEMENTAR
  {"atof", (uintptr_t)&atof},  // pass
  {"atoi", (uintptr_t)&atoi},  // pass
  {"calloc", (uintptr_t)&calloc},  // pass
  {"chdir", (uintptr_t)&chdir},  // pass
  {"clock_gettime", (uintptr_t)&clock_gettime},  // pass
  {"close", (uintptr_t)&close},  // pass
  {"closedir", (uintptr_t)&closedir},  // pass
  {"cosf", (uintptr_t)&cosf},  // pass
  {"__cxa_allocate_exception", (uintptr_t)&__cxa_allocate_exception},  // cxx
  {"__cxa_atexit", (uintptr_t)&__cxa_atexit},  // cxx
  {"__cxa_begin_catch", (uintptr_t)&__cxa_begin_catch},  // cxx
  {"__cxa_call_unexpected", (uintptr_t)&__cxa_call_unexpected},  // cxx
  {"__cxa_end_catch", (uintptr_t)&__cxa_end_catch},  // cxx
  {"__cxa_finalize", (uintptr_t)&__cxa_finalize},  // cxx
  {"__cxa_free_exception", (uintptr_t)&__cxa_free_exception},  // cxx
  {"__cxa_guard_abort", (uintptr_t)&__cxa_guard_abort},  // cxx
  {"__cxa_guard_acquire", (uintptr_t)&__cxa_guard_acquire},  // cxx
  {"__cxa_guard_release", (uintptr_t)&__cxa_guard_release},  // cxx
  {"__cxa_pure_virtual", (uintptr_t)&__cxa_pure_virtual},  // cxx
  {"__cxa_rethrow", (uintptr_t)&__cxa_rethrow},  // cxx
  {"__cxa_throw", (uintptr_t)&__cxa_throw},  // cxx
  // TODO {"dladdr", (uintptr_t)&stub_dladdr},  // <<< IMPLEMENTAR
  // TODO {"dlclose", (uintptr_t)&stub_dlclose},  // <<< IMPLEMENTAR
  // TODO {"dlopen", (uintptr_t)&stub_dlopen},  // <<< IMPLEMENTAR
  // TODO {"dlsym", (uintptr_t)&stub_dlsym},  // <<< IMPLEMENTAR
  {"__errno", (uintptr_t)&__errno},  // pass
  // TODO {"execvp", (uintptr_t)&stub_execvp},  // <<< IMPLEMENTAR
  {"exit", (uintptr_t)&exit},  // pass
  // TODO {"exp2f", (uintptr_t)&stub_exp2f},  // <<< IMPLEMENTAR
  {"expf", (uintptr_t)&expf},  // pass
  {"fclose", (uintptr_t)&fclose},  // pass
  // TODO {"feof", (uintptr_t)&stub_feof},  // <<< IMPLEMENTAR
  {"fflush", (uintptr_t)&fflush},  // pass
  {"fgetc", (uintptr_t)&fgetc},  // pass
  {"fopen", (uintptr_t)&fopen},  // pass
  // TODO {"fork", (uintptr_t)&stub_fork},  // <<< IMPLEMENTAR
  {"fprintf", (uintptr_t)&fprintf},  // pass
  {"fputc", (uintptr_t)&fputc},  // pass
  {"fputs", (uintptr_t)&fputs},  // pass
  {"fread", (uintptr_t)&fread},  // pass
  {"free", (uintptr_t)&free},  // pass
  {"fseek", (uintptr_t)&fseek},  // pass
  // TODO {"fseeko", (uintptr_t)&stub_fseeko},  // <<< IMPLEMENTAR
  {"ftell", (uintptr_t)&ftell},  // pass
  // TODO {"ftello", (uintptr_t)&stub_ftello},  // <<< IMPLEMENTAR
  {"fwrite", (uintptr_t)&fwrite},  // pass
  // TODO {"getauxval", (uintptr_t)&stub_getauxval},  // <<< IMPLEMENTAR
  {"getcwd", (uintptr_t)&getcwd},  // pass
  {"getenv", (uintptr_t)&getenv},  // pass
  {"getpid", (uintptr_t)&getpid},  // pass
  // TODO {"getpriority", (uintptr_t)&stub_getpriority},  // <<< IMPLEMENTAR
  {"glDeleteBuffers", (uintptr_t)&glDeleteBuffers},  // gles
  {"glDeleteProgram", (uintptr_t)&glDeleteProgram},  // gles
  {"glDeleteTextures", (uintptr_t)&glDeleteTextures},  // gles
  {"glGetIntegerv", (uintptr_t)&glGetIntegerv},  // gles
  {"glGetString", (uintptr_t)&glGetString},  // gles
  {"gmtime", (uintptr_t)&gmtime},  // pass
  {"__gxx_personality_v0", (uintptr_t)&__gxx_personality_v0},  // cxx
  // TODO {"ldexpf", (uintptr_t)&stub_ldexpf},  // <<< IMPLEMENTAR
  {"localtime", (uintptr_t)&localtime},  // pass
  // TODO {"log10f", (uintptr_t)&stub_log10f},  // <<< IMPLEMENTAR
  {"log2", (uintptr_t)&log2},  // pass
  {"logf", (uintptr_t)&logf},  // pass
  {"lseek", (uintptr_t)&lseek},  // pass
  {"lstat", (uintptr_t)&lstat},  // pass
  {"malloc", (uintptr_t)&malloc},  // pass
  {"memchr", (uintptr_t)&memchr},  // pass
  {"memcmp", (uintptr_t)&memcmp},  // pass
  {"memcpy", (uintptr_t)&memcpy},  // pass
  // TODO {"__memcpy_chk", (uintptr_t)&stub___memcpy_chk},  // <<< IMPLEMENTAR
  {"memmove", (uintptr_t)&memmove},  // pass
  {"memset", (uintptr_t)&memset},  // pass
  // TODO {"__memset_chk", (uintptr_t)&stub___memset_chk},  // <<< IMPLEMENTAR
  {"mkdir", (uintptr_t)&mkdir},  // pass
  // TODO {"mpg123_close", (uintptr_t)&stub_mpg123_close},  // <<< IMPLEMENTAR
  // TODO {"mpg123_delete", (uintptr_t)&stub_mpg123_delete},  // <<< IMPLEMENTAR
  // TODO {"mpg123_exit", (uintptr_t)&stub_mpg123_exit},  // <<< IMPLEMENTAR
  // TODO {"mpg123_format", (uintptr_t)&stub_mpg123_format},  // <<< IMPLEMENTAR
  // TODO {"mpg123_format_none", (uintptr_t)&stub_mpg123_format_none},  // <<< IMPLEMENTAR
  // TODO {"mpg123_getformat", (uintptr_t)&stub_mpg123_getformat},  // <<< IMPLEMENTAR
  // TODO {"mpg123_init", (uintptr_t)&stub_mpg123_init},  // <<< IMPLEMENTAR
  // TODO {"mpg123_length", (uintptr_t)&stub_mpg123_length},  // <<< IMPLEMENTAR
  // TODO {"mpg123_new", (uintptr_t)&stub_mpg123_new},  // <<< IMPLEMENTAR
  // TODO {"mpg123_open", (uintptr_t)&stub_mpg123_open},  // <<< IMPLEMENTAR
  // TODO {"mpg123_open_handle", (uintptr_t)&stub_mpg123_open_handle},  // <<< IMPLEMENTAR
  // TODO {"mpg123_param", (uintptr_t)&stub_mpg123_param},  // <<< IMPLEMENTAR
  // TODO {"mpg123_read", (uintptr_t)&stub_mpg123_read},  // <<< IMPLEMENTAR
  // TODO {"mpg123_replace_reader_handle", (uintptr_t)&stub_mpg123_replace_reader_handle},  // <<< IMPLEMENTAR
  // TODO {"mpg123_seek", (uintptr_t)&stub_mpg123_seek},  // <<< IMPLEMENTAR
  // TODO {"mpg123_tell", (uintptr_t)&stub_mpg123_tell},  // <<< IMPLEMENTAR
  // TODO {"__open_2", (uintptr_t)&stub___open_2},  // <<< IMPLEMENTAR
  {"opendir", (uintptr_t)&opendir},  // pass
  {"perror", (uintptr_t)&perror},  // pass
  {"pow", (uintptr_t)&pow},  // pass
  {"powf", (uintptr_t)&powf},  // pass
  {"printf", (uintptr_t)&printf},  // pass
  {"pthread_create", (uintptr_t)&pthread_create_fake},  // pthread wrapper (core)
  {"pthread_exit", (uintptr_t)&pthread_exit_fake},  // pthread wrapper (core)
  {"pthread_join", (uintptr_t)&pthread_join_fake},  // pthread wrapper (core)
  {"pthread_kill", (uintptr_t)&pthread_kill_fake},  // pthread wrapper (core)
  {"pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake},  // pthread wrapper (core)
  {"pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake},  // pthread wrapper (core)
  {"pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake},  // pthread wrapper (core)
  {"pthread_setspecific", (uintptr_t)&pthread_setspecific_fake},  // pthread wrapper (core)
  // TODO {"putchar", (uintptr_t)&stub_putchar},  // <<< IMPLEMENTAR
  {"puts", (uintptr_t)&puts},  // pass
  {"qsort", (uintptr_t)&qsort},  // pass
  // TODO {"__read_chk", (uintptr_t)&stub___read_chk},  // <<< IMPLEMENTAR
  {"readdir", (uintptr_t)&readdir},  // pass
  // TODO {"__readlink_chk", (uintptr_t)&stub___readlink_chk},  // <<< IMPLEMENTAR
  {"realloc", (uintptr_t)&realloc},  // pass
  {"realpath", (uintptr_t)&realpath},  // pass
  // TODO {"__register_atfork", (uintptr_t)&stub___register_atfork},  // <<< IMPLEMENTAR
  {"rename", (uintptr_t)&rename},  // pass
  {"rewind", (uintptr_t)&rewind},  // pass
  // TODO {"SDL_CreateWindow", (uintptr_t)&stub_SDL_CreateWindow},  // <<< IMPLEMENTAR
  // TODO {"SDL_DestroyWindow", (uintptr_t)&stub_SDL_DestroyWindow},  // <<< IMPLEMENTAR
  // TODO {"SDL_GameControllerAddMappingsFromRW", (uintptr_t)&stub_SDL_GameControllerAddMappingsFromRW},  // <<< IMPLEMENTAR
  // TODO {"SDL_GameControllerClose", (uintptr_t)&stub_SDL_GameControllerClose},  // <<< IMPLEMENTAR
  // TODO {"SDL_GameControllerGetAxis", (uintptr_t)&stub_SDL_GameControllerGetAxis},  // <<< IMPLEMENTAR
  // TODO {"SDL_GameControllerGetButton", (uintptr_t)&stub_SDL_GameControllerGetButton},  // <<< IMPLEMENTAR
  // TODO {"SDL_GameControllerGetJoystick", (uintptr_t)&stub_SDL_GameControllerGetJoystick},  // <<< IMPLEMENTAR
  // TODO {"SDL_GameControllerOpen", (uintptr_t)&stub_SDL_GameControllerOpen},  // <<< IMPLEMENTAR
  // TODO {"SDL_GetCurrentDisplayMode", (uintptr_t)&stub_SDL_GetCurrentDisplayMode},  // <<< IMPLEMENTAR
  // TODO {"SDL_GetDisplayMode", (uintptr_t)&stub_SDL_GetDisplayMode},  // <<< IMPLEMENTAR
  // TODO {"SDL_GetDisplayName", (uintptr_t)&stub_SDL_GetDisplayName},  // <<< IMPLEMENTAR
  // TODO {"SDL_GetError", (uintptr_t)&stub_SDL_GetError},  // <<< IMPLEMENTAR
  // TODO {"SDL_GetMouseState", (uintptr_t)&stub_SDL_GetMouseState},  // <<< IMPLEMENTAR
  // TODO {"SDL_GetNumDisplayModes", (uintptr_t)&stub_SDL_GetNumDisplayModes},  // <<< IMPLEMENTAR
  // TODO {"SDL_GetNumVideoDisplays", (uintptr_t)&stub_SDL_GetNumVideoDisplays},  // <<< IMPLEMENTAR
  // TODO {"SDL_GetWindowSize", (uintptr_t)&stub_SDL_GetWindowSize},  // <<< IMPLEMENTAR
  // TODO {"SDL_GL_CreateContext", (uintptr_t)&stub_SDL_GL_CreateContext},  // <<< IMPLEMENTAR
  // TODO {"SDL_GL_DeleteContext", (uintptr_t)&stub_SDL_GL_DeleteContext},  // <<< IMPLEMENTAR
  // TODO {"SDL_GL_GetProcAddress", (uintptr_t)&stub_SDL_GL_GetProcAddress},  // <<< IMPLEMENTAR
  // TODO {"SDL_GL_SetAttribute", (uintptr_t)&stub_SDL_GL_SetAttribute},  // <<< IMPLEMENTAR
  // TODO {"SDL_GL_SetSwapInterval", (uintptr_t)&stub_SDL_GL_SetSwapInterval},  // <<< IMPLEMENTAR
  // TODO {"SDL_GL_SwapWindow", (uintptr_t)&stub_SDL_GL_SwapWindow},  // <<< IMPLEMENTAR
  // TODO {"SDL_InitSubSystem", (uintptr_t)&stub_SDL_InitSubSystem},  // <<< IMPLEMENTAR
  // TODO {"SDL_JoystickInstanceID", (uintptr_t)&stub_SDL_JoystickInstanceID},  // <<< IMPLEMENTAR
  // TODO {"SDL_JoystickNumAxes", (uintptr_t)&stub_SDL_JoystickNumAxes},  // <<< IMPLEMENTAR
  // TODO {"SDL_JoystickNumButtons", (uintptr_t)&stub_SDL_JoystickNumButtons},  // <<< IMPLEMENTAR
  // TODO {"SDL_PeepEvents", (uintptr_t)&stub_SDL_PeepEvents},  // <<< IMPLEMENTAR
  // TODO {"SDL_PollEvent", (uintptr_t)&stub_SDL_PollEvent},  // <<< IMPLEMENTAR
  // TODO {"SDL_PumpEvents", (uintptr_t)&stub_SDL_PumpEvents},  // <<< IMPLEMENTAR
  // TODO {"SDL_QuitSubSystem", (uintptr_t)&stub_SDL_QuitSubSystem},  // <<< IMPLEMENTAR
  // TODO {"SDL_RWFromFile", (uintptr_t)&stub_SDL_RWFromFile},  // <<< IMPLEMENTAR
  // TODO {"SDL_SetHint", (uintptr_t)&stub_SDL_SetHint},  // <<< IMPLEMENTAR
  // TODO {"SDL_SetWindowDisplayMode", (uintptr_t)&stub_SDL_SetWindowDisplayMode},  // <<< IMPLEMENTAR
  // TODO {"SDL_SetWindowSize", (uintptr_t)&stub_SDL_SetWindowSize},  // <<< IMPLEMENTAR
  // TODO {"SDL_ShowCursor", (uintptr_t)&stub_SDL_ShowCursor},  // <<< IMPLEMENTAR
  // TODO {"SDL_WarpMouseInWindow", (uintptr_t)&stub_SDL_WarpMouseInWindow},  // <<< IMPLEMENTAR
  {"setenv", (uintptr_t)&setenv},  // pass
  {"setlocale", (uintptr_t)&setlocale},  // pass
  // TODO {"setpriority", (uintptr_t)&stub_setpriority},  // <<< IMPLEMENTAR
  // TODO {"sigaction", (uintptr_t)&stub_sigaction},  // <<< IMPLEMENTAR
  // TODO {"sigemptyset", (uintptr_t)&stub_sigemptyset},  // <<< IMPLEMENTAR
  // TODO {"sincosf", (uintptr_t)&stub_sincosf},  // <<< IMPLEMENTAR
  {"sinf", (uintptr_t)&sinf},  // pass
  {"sscanf", (uintptr_t)&sscanf},  // pass
  {"__stack_chk_fail", (uintptr_t)&__stack_chk_fail},  // abi
  {"stat", (uintptr_t)&stat},  // pass
  // TODO {"statfs", (uintptr_t)&stub_statfs},  // <<< IMPLEMENTAR
  // TODO {"stderr", (uintptr_t)&stub_stderr},  // <<< IMPLEMENTAR
  // TODO {"stdout", (uintptr_t)&stub_stdout},  // <<< IMPLEMENTAR
  {"strcasecmp", (uintptr_t)&strcasecmp},  // pass
  {"strcat", (uintptr_t)&strcat},  // pass
  // TODO {"__strcat_chk", (uintptr_t)&stub___strcat_chk},  // <<< IMPLEMENTAR
  {"strchr", (uintptr_t)&strchr},  // pass
  // TODO {"__strchr_chk", (uintptr_t)&stub___strchr_chk},  // <<< IMPLEMENTAR
  {"strcmp", (uintptr_t)&strcmp},  // pass
  {"strcpy", (uintptr_t)&strcpy},  // pass
  // TODO {"__strcpy_chk", (uintptr_t)&stub___strcpy_chk},  // <<< IMPLEMENTAR
  {"strdup", (uintptr_t)&strdup},  // pass
  {"strftime", (uintptr_t)&strftime},  // pass
  {"strlen", (uintptr_t)&strlen},  // pass
  // TODO {"__strlen_chk", (uintptr_t)&stub___strlen_chk},  // <<< IMPLEMENTAR
  {"strncasecmp", (uintptr_t)&strncasecmp},  // pass
  // TODO {"__strncat_chk", (uintptr_t)&stub___strncat_chk},  // <<< IMPLEMENTAR
  {"strncmp", (uintptr_t)&strncmp},  // pass
  {"strncpy", (uintptr_t)&strncpy},  // pass
  // TODO {"__strncpy_chk", (uintptr_t)&stub___strncpy_chk},  // <<< IMPLEMENTAR
  // TODO {"__strncpy_chk2", (uintptr_t)&stub___strncpy_chk2},  // <<< IMPLEMENTAR
  {"strrchr", (uintptr_t)&strrchr},  // pass
  // TODO {"__strrchr_chk", (uintptr_t)&stub___strrchr_chk},  // <<< IMPLEMENTAR
  // TODO {"strsep", (uintptr_t)&stub_strsep},  // <<< IMPLEMENTAR
  {"strstr", (uintptr_t)&strstr},  // pass
  {"strtof", (uintptr_t)&strtof},  // pass
  {"strtok", (uintptr_t)&strtok},  // pass
  {"strtol", (uintptr_t)&strtol},  // pass
  {"strtoul", (uintptr_t)&strtoul},  // pass
  // TODO {"syscall", (uintptr_t)&stub_syscall},  // <<< IMPLEMENTAR
  // TODO {"sysinfo", (uintptr_t)&stub_sysinfo},  // <<< IMPLEMENTAR
  // TODO {"__system_property_get", (uintptr_t)&stub___system_property_get},  // <<< IMPLEMENTAR
  {"tanf", (uintptr_t)&tanf},  // pass
  {"time", (uintptr_t)&time},  // pass
  // TODO {"ungetc", (uintptr_t)&stub_ungetc},  // <<< IMPLEMENTAR
  {"unlink", (uintptr_t)&unlink},  // pass
  // TODO {"_Unwind_Backtrace", (uintptr_t)&stub__Unwind_Backtrace},  // <<< IMPLEMENTAR
  // TODO {"_Unwind_GetIP", (uintptr_t)&stub__Unwind_GetIP},  // <<< IMPLEMENTAR
  // TODO {"_Unwind_Resume", (uintptr_t)&stub__Unwind_Resume},  // <<< IMPLEMENTAR
  {"vsnprintf", (uintptr_t)&vsnprintf},  // pass
  // TODO {"__vsnprintf_chk", (uintptr_t)&stub___vsnprintf_chk},  // <<< IMPLEMENTAR
  // TODO {"__vsprintf_chk", (uintptr_t)&stub___vsprintf_chk},  // <<< IMPLEMENTAR
  // TODO {"waitpid", (uintptr_t)&stub_waitpid},  // <<< IMPLEMENTAR
  {"_ZdaPv", (uintptr_t)&_ZdaPv},  // cxx
  {"_ZdlPv", (uintptr_t)&_ZdlPv},  // cxx
  {"_Znam", (uintptr_t)&_Znam},  // cxx
  // TODO {"_ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4findEcm", (uintptr_t)&stub__ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4findEcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm", (uintptr_t)&stub__ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk119__shared_weak_count13__get_deleterERKSt9type_info", (uintptr_t)&stub__ZNKSt6__ndk119__shared_weak_count13__get_deleterERKSt9type_info},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk16locale9has_facetERNS0_2idE", (uintptr_t)&stub__ZNKSt6__ndk16locale9has_facetERNS0_2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk16locale9use_facetERNS0_2idE", (uintptr_t)&stub__ZNKSt6__ndk16locale9use_facetERNS0_2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk18ios_base6getlocEv", (uintptr_t)&stub__ZNKSt6__ndk18ios_base6getlocEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt11logic_errorC2EPKc", (uintptr_t)&stub__ZNSt11logic_errorC2EPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt12length_errorD1Ev", (uintptr_t)&stub__ZNSt12length_errorD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt12out_of_rangeD1Ev", (uintptr_t)&stub__ZNSt12out_of_rangeD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt13runtime_errorC2EPKc", (uintptr_t)&stub__ZNSt13runtime_errorC2EPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt14overflow_errorD1Ev", (uintptr_t)&stub__ZNSt14overflow_errorD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt20bad_array_new_lengthC1Ev", (uintptr_t)&stub__ZNSt20bad_array_new_lengthC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt20bad_array_new_lengthD1Ev", (uintptr_t)&stub__ZNSt20bad_array_new_lengthD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5eraseEmm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5eraseEmm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEmc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEmc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6resizeEmc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6resizeEmc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7replaceEmmPKcm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7replaceEmmPKcm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSERKS5_", (uintptr_t)&stub__ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSERKS5_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115__thread_structC1Ev", (uintptr_t)&stub__ZNSt6__ndk115__thread_structC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk115__thread_structD1Ev", (uintptr_t)&stub__ZNSt6__ndk115__thread_structD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk118condition_variable10notify_oneEv", (uintptr_t)&stub__ZNSt6__ndk118condition_variable10notify_oneEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk118condition_variable4waitERNS_11unique_lockINS_5mutexEEE", (uintptr_t)&stub__ZNSt6__ndk118condition_variable4waitERNS_11unique_lockINS_5mutexEEE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk118condition_variableD1Ev", (uintptr_t)&stub__ZNSt6__ndk118condition_variableD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_weak_count14__release_weakEv", (uintptr_t)&stub__ZNSt6__ndk119__shared_weak_count14__release_weakEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_weak_countD2Ev", (uintptr_t)&stub__ZNSt6__ndk119__shared_weak_countD2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__thread_local_dataEv", (uintptr_t)&stub__ZNSt6__ndk119__thread_local_dataEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk120__throw_system_errorEiPKc", (uintptr_t)&stub__ZNSt6__ndk120__throw_system_errorEiPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15ctypeIcE2idE", (uintptr_t)&stub__ZNSt6__ndk15ctypeIcE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15ctypeIwE2idE", (uintptr_t)&stub__ZNSt6__ndk15ctypeIwE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutex4lockEv", (uintptr_t)&stub__ZNSt6__ndk15mutex4lockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutex6unlockEv", (uintptr_t)&stub__ZNSt6__ndk15mutex6unlockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutexD1Ev", (uintptr_t)&stub__ZNSt6__ndk15mutexD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeaSERKS0_", (uintptr_t)&stub__ZNSt6__ndk16localeaSERKS0_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeC1ERKS0_", (uintptr_t)&stub__ZNSt6__ndk16localeC1ERKS0_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeC1Ev", (uintptr_t)&stub__ZNSt6__ndk16localeC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeD1Ev", (uintptr_t)&stub__ZNSt6__ndk16localeD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16thread4joinEv", (uintptr_t)&stub__ZNSt6__ndk16thread4joinEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16threadD1Ev", (uintptr_t)&stub__ZNSt6__ndk16threadD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17codecvtIcc9mbstate_tE2idE", (uintptr_t)&stub__ZNSt6__ndk17codecvtIcc9mbstate_tE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE", (uintptr_t)&stub__ZNSt6__ndk17num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17num_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE", (uintptr_t)&stub__ZNSt6__ndk17num_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE", (uintptr_t)&stub__ZNSt6__ndk17num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17num_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE", (uintptr_t)&stub__ZNSt6__ndk17num_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base16__call_callbacksENS0_5eventE", (uintptr_t)&stub__ZNSt6__ndk18ios_base16__call_callbacksENS0_5eventE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base33__set_badbit_and_consider_rethrowEv", (uintptr_t)&stub__ZNSt6__ndk18ios_base33__set_badbit_and_consider_rethrowEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base34__set_failbit_and_consider_rethrowEv", (uintptr_t)&stub__ZNSt6__ndk18ios_base34__set_failbit_and_consider_rethrowEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base4initEPv", (uintptr_t)&stub__ZNSt6__ndk18ios_base4initEPv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base4swapERS0_", (uintptr_t)&stub__ZNSt6__ndk18ios_base4swapERS0_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base5clearEj", (uintptr_t)&stub__ZNSt6__ndk18ios_base5clearEj},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base7copyfmtERKS0_", (uintptr_t)&stub__ZNSt6__ndk18ios_base7copyfmtERKS0_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_baseD2Ev", (uintptr_t)&stub__ZNSt6__ndk18ios_baseD2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk1plIcNS_11char_traitsIcEENS_9allocatorIcEEEENS_12basic_stringIT_T0_T1_EEPKS6_RKS9_", (uintptr_t)&stub__ZNSt6__ndk1plIcNS_11char_traitsIcEENS_9allocatorIcEEEENS_12basic_stringIT_T0_T1_EEPKS6_RKS9_},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt8bad_castC1Ev", (uintptr_t)&stub__ZNSt8bad_castC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt8bad_castD1Ev", (uintptr_t)&stub__ZNSt8bad_castD1Ev},  // <<< IMPLEMENTAR
  {"_Znwm", (uintptr_t)&_Znwm},  // cxx
  // TODO {"_ZSt18uncaught_exceptionv", (uintptr_t)&stub__ZSt18uncaught_exceptionv},  // <<< IMPLEMENTAR
  // TODO {"_ZSt9terminatev", (uintptr_t)&stub__ZSt9terminatev},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk119__shared_weak_countE", (uintptr_t)&stub__ZTINSt6__ndk119__shared_weak_countE},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk18ios_baseE", (uintptr_t)&stub__ZTINSt6__ndk18ios_baseE},  // <<< IMPLEMENTAR
  // TODO {"_ZTISt12length_error", (uintptr_t)&stub__ZTISt12length_error},  // <<< IMPLEMENTAR
  // TODO {"_ZTISt12out_of_range", (uintptr_t)&stub__ZTISt12out_of_range},  // <<< IMPLEMENTAR
  // TODO {"_ZTISt14overflow_error", (uintptr_t)&stub__ZTISt14overflow_error},  // <<< IMPLEMENTAR
  // TODO {"_ZTISt20bad_array_new_length", (uintptr_t)&stub__ZTISt20bad_array_new_length},  // <<< IMPLEMENTAR
  // TODO {"_ZTISt8bad_cast", (uintptr_t)&stub__ZTISt8bad_cast},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv117__class_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv117__class_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv120__si_class_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv120__si_class_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv121__vmi_class_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv121__vmi_class_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVSt12length_error", (uintptr_t)&stub__ZTVSt12length_error},  // <<< IMPLEMENTAR
  // TODO {"_ZTVSt12out_of_range", (uintptr_t)&stub__ZTVSt12out_of_range},  // <<< IMPLEMENTAR
  // TODO {"_ZTVSt14overflow_error", (uintptr_t)&stub__ZTVSt14overflow_error},  // <<< IMPLEMENTAR
};
const int dynlib_functions_count = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);

// ===================== SIMBOLOS A IMPLEMENTAR =====================
//   acosf
//   alBufferData
//   alBufferiv
//   alcCloseDevice
//   alcCreateContext
//   alcDestroyContext
//   alcGetIntegerv
//   alcGetString
//   alcIsExtensionPresent
//   alcMakeContextCurrent
//   alcOpenDevice
//   alcSuspendContext
//   alDeleteBuffers
//   alDeleteSources
//   alDistanceModel
//   alGenBuffers
//   alGenSources
//   alGetEnumValue
//   alGetError
//   alGetProcAddress
//   alGetSourcei
//   alGetString
//   alIsExtensionPresent
//   alListener3f
//   alListenerf
//   alListenerfv
//   alSource3f
//   alSource3i
//   alSourcef
//   alSourcei
//   alSourcePause
//   alSourcePlay
//   alSourceQueueBuffers
//   alSourceStop
//   alSourceUnqueueBuffers
//   asinf
//   atanf
//   dladdr
//   dlclose
//   dlopen
//   dlsym
//   execvp
//   exp2f
//   feof
//   fork
//   fseeko
//   ftello
//   getauxval
//   getpriority
//   ldexpf
//   log10f
//   __memcpy_chk
//   __memset_chk
//   mpg123_close
//   mpg123_delete
//   mpg123_exit
//   mpg123_format
//   mpg123_format_none
//   mpg123_getformat
//   mpg123_init
//   mpg123_length
//   mpg123_new
//   mpg123_open
//   mpg123_open_handle
//   mpg123_param
//   mpg123_read
//   mpg123_replace_reader_handle
//   mpg123_seek
//   mpg123_tell
//   __open_2
//   putchar
//   __read_chk
//   __readlink_chk
//   __register_atfork
//   SDL_CreateWindow
//   SDL_DestroyWindow
//   SDL_GameControllerAddMappingsFromRW
//   SDL_GameControllerClose
//   SDL_GameControllerGetAxis
//   SDL_GameControllerGetButton
//   SDL_GameControllerGetJoystick
//   SDL_GameControllerOpen
//   SDL_GetCurrentDisplayMode
//   SDL_GetDisplayMode
//   SDL_GetDisplayName
//   SDL_GetError
//   SDL_GetMouseState
//   SDL_GetNumDisplayModes
//   SDL_GetNumVideoDisplays
//   SDL_GetWindowSize
//   SDL_GL_CreateContext
//   SDL_GL_DeleteContext
//   SDL_GL_GetProcAddress
//   SDL_GL_SetAttribute
//   SDL_GL_SetSwapInterval
//   SDL_GL_SwapWindow
//   SDL_InitSubSystem
//   SDL_JoystickInstanceID
//   SDL_JoystickNumAxes
//   SDL_JoystickNumButtons
//   SDL_PeepEvents
//   SDL_PollEvent
//   SDL_PumpEvents
//   SDL_QuitSubSystem
//   SDL_RWFromFile
//   SDL_SetHint
//   SDL_SetWindowDisplayMode
//   SDL_SetWindowSize
//   SDL_ShowCursor
//   SDL_WarpMouseInWindow
//   setpriority
//   sigaction
//   sigemptyset
//   sincosf
//   statfs
//   stderr
//   stdout
//   __strcat_chk
//   __strchr_chk
//   __strcpy_chk
//   __strlen_chk
//   __strncat_chk
//   __strncpy_chk
//   __strncpy_chk2
//   __strrchr_chk
//   strsep
//   syscall
//   sysinfo
//   __system_property_get
//   ungetc
//   _Unwind_Backtrace
//   _Unwind_GetIP
//   _Unwind_Resume
//   __vsnprintf_chk
//   __vsprintf_chk
//   waitpid
//   _ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4findEcm
//   _ZNKSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm
//   _ZNKSt6__ndk119__shared_weak_count13__get_deleterERKSt9type_info
//   _ZNKSt6__ndk16locale9has_facetERNS0_2idE
//   _ZNKSt6__ndk16locale9use_facetERNS0_2idE
//   _ZNKSt6__ndk18ios_base6getlocEv
//   _ZNSt11logic_errorC2EPKc
//   _ZNSt12length_errorD1Ev
//   _ZNSt12out_of_rangeD1Ev
//   _ZNSt13runtime_errorC2EPKc
//   _ZNSt14overflow_errorD1Ev
//   _ZNSt20bad_array_new_lengthC1Ev
//   _ZNSt20bad_array_new_lengthD1Ev
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5eraseEmm
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEmc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6resizeEmc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7replaceEmmPKcm
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc
//   _ZNSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSERKS5_
//   _ZNSt6__ndk115__thread_structC1Ev
//   _ZNSt6__ndk115__thread_structD1Ev
//   _ZNSt6__ndk118condition_variable10notify_oneEv
//   _ZNSt6__ndk118condition_variable4waitERNS_11unique_lockINS_5mutexEEE
//   _ZNSt6__ndk118condition_variableD1Ev
//   _ZNSt6__ndk119__shared_weak_count14__release_weakEv
//   _ZNSt6__ndk119__shared_weak_countD2Ev
//   _ZNSt6__ndk119__thread_local_dataEv
//   _ZNSt6__ndk120__throw_system_errorEiPKc
//   _ZNSt6__ndk15ctypeIcE2idE
//   _ZNSt6__ndk15ctypeIwE2idE
//   _ZNSt6__ndk15mutex4lockEv
//   _ZNSt6__ndk15mutex6unlockEv
//   _ZNSt6__ndk15mutexD1Ev
//   _ZNSt6__ndk16localeaSERKS0_
//   _ZNSt6__ndk16localeC1ERKS0_
//   _ZNSt6__ndk16localeC1Ev
//   _ZNSt6__ndk16localeD1Ev
//   _ZNSt6__ndk16thread4joinEv
//   _ZNSt6__ndk16threadD1Ev
//   _ZNSt6__ndk17codecvtIcc9mbstate_tE2idE
//   _ZNSt6__ndk17num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE
//   _ZNSt6__ndk17num_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE
//   _ZNSt6__ndk17num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE
//   _ZNSt6__ndk17num_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE
//   _ZNSt6__ndk18ios_base16__call_callbacksENS0_5eventE
//   _ZNSt6__ndk18ios_base33__set_badbit_and_consider_rethrowEv
//   _ZNSt6__ndk18ios_base34__set_failbit_and_consider_rethrowEv
//   _ZNSt6__ndk18ios_base4initEPv
//   _ZNSt6__ndk18ios_base4swapERS0_
//   _ZNSt6__ndk18ios_base5clearEj
//   _ZNSt6__ndk18ios_base7copyfmtERKS0_
//   _ZNSt6__ndk18ios_baseD2Ev
//   _ZNSt6__ndk1plIcNS_11char_traitsIcEENS_9allocatorIcEEEENS_12basic_stringIT_T0_T1_EEPKS6_RKS9_
//   _ZNSt8bad_castC1Ev
//   _ZNSt8bad_castD1Ev
//   _ZSt18uncaught_exceptionv
//   _ZSt9terminatev
//   _ZTINSt6__ndk119__shared_weak_countE
//   _ZTINSt6__ndk18ios_baseE
//   _ZTISt12length_error
//   _ZTISt12out_of_range
//   _ZTISt14overflow_error
//   _ZTISt20bad_array_new_length
//   _ZTISt8bad_cast
//   _ZTVN10__cxxabiv117__class_type_infoE
//   _ZTVN10__cxxabiv120__si_class_type_infoE
//   _ZTVN10__cxxabiv121__vmi_class_type_infoE
//   _ZTVSt12length_error
//   _ZTVSt12out_of_range
//   _ZTVSt14overflow_error
