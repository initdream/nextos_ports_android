// imports.gen.c — GERADO por new-port-arm.sh para 'nfs' (libapp.so)
// 540 simbolos. Resolva os UNKNOWN no fim do arquivo.
#include "imports.h"
#include "so_util.h"
#include <stdio.h>

DynLibFunction dynlib_functions[] = {
  {"abort", (uintptr_t)&abort},  // pass
  {"accept", (uintptr_t)&accept},  // pass
  {"access", (uintptr_t)&access},  // pass
  // TODO {"acosf", (uintptr_t)&stub_acosf},  // <<< IMPLEMENTAR
  {"__aeabi_d2lz", (uintptr_t)&__aeabi_d2lz},  // abi
  {"__aeabi_d2uiz", (uintptr_t)&__aeabi_d2uiz},  // abi
  {"__aeabi_d2ulz", (uintptr_t)&__aeabi_d2ulz},  // abi
  {"__aeabi_f2lz", (uintptr_t)&__aeabi_f2lz},  // abi
  {"__aeabi_f2ulz", (uintptr_t)&__aeabi_f2ulz},  // abi
  {"__aeabi_l2f", (uintptr_t)&__aeabi_l2f},  // abi
  {"__aeabi_memclr", (uintptr_t)&__aeabi_memclr},  // abi
  {"__aeabi_memclr4", (uintptr_t)&__aeabi_memclr4},  // abi
  {"__aeabi_memclr8", (uintptr_t)&__aeabi_memclr8},  // abi
  {"__aeabi_memcpy", (uintptr_t)&__aeabi_memcpy},  // abi
  {"__aeabi_memcpy4", (uintptr_t)&__aeabi_memcpy4},  // abi
  {"__aeabi_memcpy8", (uintptr_t)&__aeabi_memcpy8},  // abi
  {"__aeabi_memmove", (uintptr_t)&__aeabi_memmove},  // abi
  {"__aeabi_memmove4", (uintptr_t)&__aeabi_memmove4},  // abi
  {"__aeabi_memset", (uintptr_t)&__aeabi_memset},  // abi
  {"__aeabi_memset4", (uintptr_t)&__aeabi_memset4},  // abi
  {"__aeabi_memset8", (uintptr_t)&__aeabi_memset8},  // abi
  {"__aeabi_ul2f", (uintptr_t)&__aeabi_ul2f},  // abi
  // TODO {"AndroidBitmap_getInfo", (uintptr_t)&stub_AndroidBitmap_getInfo},  // <<< IMPLEMENTAR
  // TODO {"AndroidBitmap_lockPixels", (uintptr_t)&stub_AndroidBitmap_lockPixels},  // <<< IMPLEMENTAR
  // TODO {"AndroidBitmap_unlockPixels", (uintptr_t)&stub_AndroidBitmap_unlockPixels},  // <<< IMPLEMENTAR
  {"__android_log_assert", (uintptr_t)&__android_log_assert},  // liblog
  {"__android_log_print", (uintptr_t)&__android_log_print},  // liblog
  {"__android_log_write", (uintptr_t)&__android_log_write},  // liblog
  // TODO {"asinf", (uintptr_t)&stub_asinf},  // <<< IMPLEMENTAR
  {"atan2f", (uintptr_t)&atan2f},  // pass
  {"atoi", (uintptr_t)&atoi},  // pass
  // TODO {"basename", (uintptr_t)&stub_basename},  // <<< IMPLEMENTAR
  {"bind", (uintptr_t)&bind},  // pass
  {"calloc", (uintptr_t)&calloc},  // pass
  {"ceil", (uintptr_t)&ceil},  // pass
  {"ceilf", (uintptr_t)&ceilf},  // pass
  {"chdir", (uintptr_t)&chdir},  // pass
  // TODO {"chmod", (uintptr_t)&stub_chmod},  // <<< IMPLEMENTAR
  // TODO {"clock", (uintptr_t)&stub_clock},  // <<< IMPLEMENTAR
  {"clock_gettime", (uintptr_t)&clock_gettime},  // pass
  {"close", (uintptr_t)&close},  // pass
  {"closedir", (uintptr_t)&closedir},  // pass
  {"connect", (uintptr_t)&connect},  // pass
  {"cosf", (uintptr_t)&cosf},  // pass
  // TODO {"_ctype_", (uintptr_t)&stub__ctype_},  // <<< IMPLEMENTAR
  {"__cxa_atexit", (uintptr_t)&__cxa_atexit},  // cxx
  {"__cxa_bad_typeid", (uintptr_t)&__cxa_bad_typeid},  // cxx
  {"__cxa_finalize", (uintptr_t)&__cxa_finalize},  // cxx
  {"__cxa_guard_acquire", (uintptr_t)&__cxa_guard_acquire},  // cxx
  {"__cxa_guard_release", (uintptr_t)&__cxa_guard_release},  // cxx
  {"__cxa_pure_virtual", (uintptr_t)&__cxa_pure_virtual},  // cxx
  // TODO {"dladdr", (uintptr_t)&stub_dladdr},  // <<< IMPLEMENTAR
  // TODO {"dlclose", (uintptr_t)&stub_dlclose},  // <<< IMPLEMENTAR
  // TODO {"dlerror", (uintptr_t)&stub_dlerror},  // <<< IMPLEMENTAR
  // TODO {"dlopen", (uintptr_t)&stub_dlopen},  // <<< IMPLEMENTAR
  // TODO {"dlsym", (uintptr_t)&stub_dlsym},  // <<< IMPLEMENTAR
  {"dup2", (uintptr_t)&dup2},  // pass
  // TODO {"__dynamic_cast", (uintptr_t)&stub___dynamic_cast},  // <<< IMPLEMENTAR
  {"eglGetProcAddress", (uintptr_t)&eglGetProcAddress_shim},  // egl_shim
  {"__errno", (uintptr_t)&__errno},  // pass
  // TODO {"execl", (uintptr_t)&stub_execl},  // <<< IMPLEMENTAR
  // TODO {"execv", (uintptr_t)&stub_execv},  // <<< IMPLEMENTAR
  {"exit", (uintptr_t)&exit},  // pass
  {"expf", (uintptr_t)&expf},  // pass
  {"fclose", (uintptr_t)&fclose},  // pass
  {"fcntl", (uintptr_t)&fcntl},  // pass
  {"fdopen", (uintptr_t)&fdopen},  // pass
  // TODO {"feof", (uintptr_t)&stub_feof},  // <<< IMPLEMENTAR
  // TODO {"ferror", (uintptr_t)&stub_ferror},  // <<< IMPLEMENTAR
  {"fflush", (uintptr_t)&fflush},  // pass
  {"fgetc", (uintptr_t)&fgetc},  // pass
  {"fgets", (uintptr_t)&fgets},  // pass
  {"fileno", (uintptr_t)&fileno},  // pass
  {"floor", (uintptr_t)&floor},  // pass
  {"floorf", (uintptr_t)&floorf},  // pass
  {"fmod", (uintptr_t)&fmod},  // pass
  // TODO {"FMOD_EventSystem_Create", (uintptr_t)&stub_FMOD_EventSystem_Create},  // <<< IMPLEMENTAR
  {"fmodf", (uintptr_t)&fmodf},  // pass
  // TODO {"FMOD_Memory_Initialize", (uintptr_t)&stub_FMOD_Memory_Initialize},  // <<< IMPLEMENTAR
  {"fopen", (uintptr_t)&fopen},  // pass
  // TODO {"fork", (uintptr_t)&stub_fork},  // <<< IMPLEMENTAR
  {"fprintf", (uintptr_t)&fprintf},  // pass
  {"fputc", (uintptr_t)&fputc},  // pass
  {"fread", (uintptr_t)&fread},  // pass
  {"free", (uintptr_t)&free},  // pass
  // TODO {"freeaddrinfo", (uintptr_t)&stub_freeaddrinfo},  // <<< IMPLEMENTAR
  {"fseek", (uintptr_t)&fseek},  // pass
  {"fstat", (uintptr_t)&fstat},  // pass
  // TODO {"fsync", (uintptr_t)&stub_fsync},  // <<< IMPLEMENTAR
  {"ftell", (uintptr_t)&ftell},  // pass
  // TODO {"ftruncate", (uintptr_t)&stub_ftruncate},  // <<< IMPLEMENTAR
  // TODO {"fwide", (uintptr_t)&stub_fwide},  // <<< IMPLEMENTAR
  {"fwrite", (uintptr_t)&fwrite},  // pass
  // TODO {"gai_strerror", (uintptr_t)&stub_gai_strerror},  // <<< IMPLEMENTAR
  // TODO {"getaddrinfo", (uintptr_t)&stub_getaddrinfo},  // <<< IMPLEMENTAR
  // TODO {"getauxval", (uintptr_t)&stub_getauxval},  // <<< IMPLEMENTAR
  {"getc", (uintptr_t)&getc},  // pass
  {"getcwd", (uintptr_t)&getcwd},  // pass
  // TODO {"getegid", (uintptr_t)&stub_getegid},  // <<< IMPLEMENTAR
  {"getenv", (uintptr_t)&getenv},  // pass
  // TODO {"geteuid", (uintptr_t)&stub_geteuid},  // <<< IMPLEMENTAR
  // TODO {"getgid", (uintptr_t)&stub_getgid},  // <<< IMPLEMENTAR
  {"gethostbyname", (uintptr_t)&gethostbyname},  // pass
  // TODO {"gethostname", (uintptr_t)&stub_gethostname},  // <<< IMPLEMENTAR
  // TODO {"getnameinfo", (uintptr_t)&stub_getnameinfo},  // <<< IMPLEMENTAR
  // TODO {"getpeername", (uintptr_t)&stub_getpeername},  // <<< IMPLEMENTAR
  {"getpid", (uintptr_t)&getpid},  // pass
  // TODO {"getpwuid_r", (uintptr_t)&stub_getpwuid_r},  // <<< IMPLEMENTAR
  {"getsockname", (uintptr_t)&getsockname},  // pass
  {"getsockopt", (uintptr_t)&getsockopt},  // pass
  {"gettimeofday", (uintptr_t)&gettimeofday},  // pass
  // TODO {"getuid", (uintptr_t)&stub_getuid},  // <<< IMPLEMENTAR
  {"glActiveTexture", (uintptr_t)&glActiveTexture},  // gles
  {"glAttachShader", (uintptr_t)&glAttachShader},  // gles
  {"glBindAttribLocation", (uintptr_t)&glBindAttribLocation},  // gles
  {"glBindBuffer", (uintptr_t)&glBindBuffer},  // gles
  {"glBindFramebuffer", (uintptr_t)&glBindFramebuffer},  // gles
  {"glBindRenderbuffer", (uintptr_t)&glBindRenderbuffer},  // gles
  {"glBindTexture", (uintptr_t)&glBindTexture},  // gles
  {"glBlendColor", (uintptr_t)&glBlendColor},  // gles
  {"glBlendEquation", (uintptr_t)&glBlendEquation},  // gles
  {"glBlendEquationSeparate", (uintptr_t)&glBlendEquationSeparate},  // gles
  {"glBlendFunc", (uintptr_t)&glBlendFunc},  // gles
  {"glBlendFuncSeparate", (uintptr_t)&glBlendFuncSeparate},  // gles
  {"glBufferData", (uintptr_t)&glBufferData},  // gles
  {"glBufferSubData", (uintptr_t)&glBufferSubData},  // gles
  {"glCheckFramebufferStatus", (uintptr_t)&glCheckFramebufferStatus},  // gles
  {"glClear", (uintptr_t)&glClear},  // gles
  {"glClearColor", (uintptr_t)&glClearColor},  // gles
  {"glClearDepthf", (uintptr_t)&glClearDepthf},  // gles
  {"glClearStencil", (uintptr_t)&glClearStencil},  // gles
  {"glColorMask", (uintptr_t)&glColorMask},  // gles
  {"glCompileShader", (uintptr_t)&glCompileShader},  // gles
  {"glCompressedTexImage2D", (uintptr_t)&glCompressedTexImage2D},  // gles
  {"glCompressedTexSubImage2D", (uintptr_t)&glCompressedTexSubImage2D},  // gles
  {"glCopyTexImage2D", (uintptr_t)&glCopyTexImage2D},  // gles
  {"glCopyTexSubImage2D", (uintptr_t)&glCopyTexSubImage2D},  // gles
  {"glCreateProgram", (uintptr_t)&glCreateProgram},  // gles
  {"glCreateShader", (uintptr_t)&glCreateShader},  // gles
  {"glCullFace", (uintptr_t)&glCullFace},  // gles
  {"glDeleteBuffers", (uintptr_t)&glDeleteBuffers},  // gles
  {"glDeleteFramebuffers", (uintptr_t)&glDeleteFramebuffers},  // gles
  {"glDeleteProgram", (uintptr_t)&glDeleteProgram},  // gles
  {"glDeleteRenderbuffers", (uintptr_t)&glDeleteRenderbuffers},  // gles
  {"glDeleteShader", (uintptr_t)&glDeleteShader},  // gles
  {"glDeleteTextures", (uintptr_t)&glDeleteTextures},  // gles
  {"glDepthFunc", (uintptr_t)&glDepthFunc},  // gles
  {"glDepthMask", (uintptr_t)&glDepthMask},  // gles
  {"glDepthRangef", (uintptr_t)&glDepthRangef},  // gles
  {"glDetachShader", (uintptr_t)&glDetachShader},  // gles
  {"glDisable", (uintptr_t)&glDisable},  // gles
  {"glDisableVertexAttribArray", (uintptr_t)&glDisableVertexAttribArray},  // gles
  {"glDrawArrays", (uintptr_t)&glDrawArrays},  // gles
  {"glDrawElements", (uintptr_t)&glDrawElements},  // gles
  {"glEnable", (uintptr_t)&glEnable},  // gles
  {"glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray},  // gles
  {"glFinish", (uintptr_t)&glFinish},  // gles
  {"glFlush", (uintptr_t)&glFlush},  // gles
  {"glFramebufferRenderbuffer", (uintptr_t)&glFramebufferRenderbuffer},  // gles
  {"glFramebufferTexture2D", (uintptr_t)&glFramebufferTexture2D},  // gles
  {"glFrontFace", (uintptr_t)&glFrontFace},  // gles
  {"glGenBuffers", (uintptr_t)&glGenBuffers},  // gles
  {"glGenerateMipmap", (uintptr_t)&glGenerateMipmap},  // gles
  {"glGenFramebuffers", (uintptr_t)&glGenFramebuffers},  // gles
  {"glGenRenderbuffers", (uintptr_t)&glGenRenderbuffers},  // gles
  {"glGenTextures", (uintptr_t)&glGenTextures},  // gles
  {"glGetActiveAttrib", (uintptr_t)&glGetActiveAttrib},  // gles
  {"glGetActiveUniform", (uintptr_t)&glGetActiveUniform},  // gles
  {"glGetAttachedShaders", (uintptr_t)&glGetAttachedShaders},  // gles
  {"glGetAttribLocation", (uintptr_t)&glGetAttribLocation},  // gles
  {"glGetBooleanv", (uintptr_t)&glGetBooleanv},  // gles
  {"glGetBufferParameteriv", (uintptr_t)&glGetBufferParameteriv},  // gles
  {"glGetBufferPointervOES", (uintptr_t)&glGetBufferPointervOES},  // gles
  {"glGetError", (uintptr_t)&glGetError},  // gles
  {"glGetFloatv", (uintptr_t)&glGetFloatv},  // gles
  {"glGetFramebufferAttachmentParameteriv", (uintptr_t)&glGetFramebufferAttachmentParameteriv},  // gles
  {"glGetIntegerv", (uintptr_t)&glGetIntegerv},  // gles
  {"glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog},  // gles
  {"glGetProgramiv", (uintptr_t)&glGetProgramiv},  // gles
  {"glGetRenderbufferParameteriv", (uintptr_t)&glGetRenderbufferParameteriv},  // gles
  {"glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog},  // gles
  {"glGetShaderiv", (uintptr_t)&glGetShaderiv},  // gles
  {"glGetShaderPrecisionFormat", (uintptr_t)&glGetShaderPrecisionFormat},  // gles
  {"glGetShaderSource", (uintptr_t)&glGetShaderSource},  // gles
  {"glGetString", (uintptr_t)&glGetString},  // gles
  {"glGetTexParameterfv", (uintptr_t)&glGetTexParameterfv},  // gles
  {"glGetTexParameteriv", (uintptr_t)&glGetTexParameteriv},  // gles
  {"glGetUniformfv", (uintptr_t)&glGetUniformfv},  // gles
  {"glGetUniformiv", (uintptr_t)&glGetUniformiv},  // gles
  {"glGetUniformLocation", (uintptr_t)&glGetUniformLocation},  // gles
  {"glGetVertexAttribfv", (uintptr_t)&glGetVertexAttribfv},  // gles
  {"glGetVertexAttribiv", (uintptr_t)&glGetVertexAttribiv},  // gles
  {"glGetVertexAttribPointerv", (uintptr_t)&glGetVertexAttribPointerv},  // gles
  {"glHint", (uintptr_t)&glHint},  // gles
  {"glIsBuffer", (uintptr_t)&glIsBuffer},  // gles
  {"glIsEnabled", (uintptr_t)&glIsEnabled},  // gles
  {"glIsFramebuffer", (uintptr_t)&glIsFramebuffer},  // gles
  {"glIsProgram", (uintptr_t)&glIsProgram},  // gles
  {"glIsRenderbuffer", (uintptr_t)&glIsRenderbuffer},  // gles
  {"glIsShader", (uintptr_t)&glIsShader},  // gles
  {"glIsTexture", (uintptr_t)&glIsTexture},  // gles
  {"glLineWidth", (uintptr_t)&glLineWidth},  // gles
  {"glLinkProgram", (uintptr_t)&glLinkProgram},  // gles
  {"glMapBufferOES", (uintptr_t)&glMapBufferOES},  // gles
  {"glPixelStorei", (uintptr_t)&glPixelStorei},  // gles
  {"glPolygonOffset", (uintptr_t)&glPolygonOffset},  // gles
  {"glReadPixels", (uintptr_t)&glReadPixels},  // gles
  {"glReleaseShaderCompiler", (uintptr_t)&glReleaseShaderCompiler},  // gles
  {"glRenderbufferStorage", (uintptr_t)&glRenderbufferStorage},  // gles
  {"glSampleCoverage", (uintptr_t)&glSampleCoverage},  // gles
  {"glScissor", (uintptr_t)&glScissor},  // gles
  {"glShaderBinary", (uintptr_t)&glShaderBinary},  // gles
  {"glShaderSource", (uintptr_t)&glShaderSource},  // gles
  {"glStencilFunc", (uintptr_t)&glStencilFunc},  // gles
  {"glStencilFuncSeparate", (uintptr_t)&glStencilFuncSeparate},  // gles
  {"glStencilMask", (uintptr_t)&glStencilMask},  // gles
  {"glStencilMaskSeparate", (uintptr_t)&glStencilMaskSeparate},  // gles
  {"glStencilOp", (uintptr_t)&glStencilOp},  // gles
  {"glStencilOpSeparate", (uintptr_t)&glStencilOpSeparate},  // gles
  {"glTexImage2D", (uintptr_t)&glTexImage2D},  // gles
  {"glTexParameterf", (uintptr_t)&glTexParameterf},  // gles
  {"glTexParameterfv", (uintptr_t)&glTexParameterfv},  // gles
  {"glTexParameteri", (uintptr_t)&glTexParameteri},  // gles
  {"glTexParameteriv", (uintptr_t)&glTexParameteriv},  // gles
  {"glTexSubImage2D", (uintptr_t)&glTexSubImage2D},  // gles
  {"glUniform1f", (uintptr_t)&glUniform1f},  // gles
  {"glUniform1fv", (uintptr_t)&glUniform1fv},  // gles
  {"glUniform1i", (uintptr_t)&glUniform1i},  // gles
  {"glUniform1iv", (uintptr_t)&glUniform1iv},  // gles
  {"glUniform2f", (uintptr_t)&glUniform2f},  // gles
  {"glUniform2fv", (uintptr_t)&glUniform2fv},  // gles
  {"glUniform2i", (uintptr_t)&glUniform2i},  // gles
  {"glUniform2iv", (uintptr_t)&glUniform2iv},  // gles
  {"glUniform3f", (uintptr_t)&glUniform3f},  // gles
  {"glUniform3fv", (uintptr_t)&glUniform3fv},  // gles
  {"glUniform3i", (uintptr_t)&glUniform3i},  // gles
  {"glUniform3iv", (uintptr_t)&glUniform3iv},  // gles
  {"glUniform4f", (uintptr_t)&glUniform4f},  // gles
  {"glUniform4fv", (uintptr_t)&glUniform4fv},  // gles
  {"glUniform4i", (uintptr_t)&glUniform4i},  // gles
  {"glUniform4iv", (uintptr_t)&glUniform4iv},  // gles
  {"glUniformMatrix2fv", (uintptr_t)&glUniformMatrix2fv},  // gles
  {"glUniformMatrix3fv", (uintptr_t)&glUniformMatrix3fv},  // gles
  {"glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv},  // gles
  {"glUnmapBufferOES", (uintptr_t)&glUnmapBufferOES},  // gles
  {"glUseProgram", (uintptr_t)&glUseProgram},  // gles
  {"glValidateProgram", (uintptr_t)&glValidateProgram},  // gles
  {"glVertexAttrib1f", (uintptr_t)&glVertexAttrib1f},  // gles
  {"glVertexAttrib1fv", (uintptr_t)&glVertexAttrib1fv},  // gles
  {"glVertexAttrib2f", (uintptr_t)&glVertexAttrib2f},  // gles
  {"glVertexAttrib2fv", (uintptr_t)&glVertexAttrib2fv},  // gles
  {"glVertexAttrib3f", (uintptr_t)&glVertexAttrib3f},  // gles
  {"glVertexAttrib3fv", (uintptr_t)&glVertexAttrib3fv},  // gles
  {"glVertexAttrib4f", (uintptr_t)&glVertexAttrib4f},  // gles
  {"glVertexAttrib4fv", (uintptr_t)&glVertexAttrib4fv},  // gles
  {"glVertexAttribPointer", (uintptr_t)&glVertexAttribPointer},  // gles
  {"glViewport", (uintptr_t)&glViewport},  // gles
  {"gmtime", (uintptr_t)&gmtime},  // pass
  // TODO {"gmtime_r", (uintptr_t)&stub_gmtime_r},  // <<< IMPLEMENTAR
  {"__gnu_Unwind_Find_exidx", (uintptr_t)&__gnu_Unwind_Find_exidx},  // abi
  // TODO {"if_nametoindex", (uintptr_t)&stub_if_nametoindex},  // <<< IMPLEMENTAR
  {"inet_ntop", (uintptr_t)&inet_ntop},  // pass
  {"inet_pton", (uintptr_t)&inet_pton},  // pass
  {"ioctl", (uintptr_t)&ioctl},  // pass
  {"isalnum", (uintptr_t)&isalnum},  // pass
  {"isalpha", (uintptr_t)&isalpha},  // pass
  // TODO {"isgraph", (uintptr_t)&stub_isgraph},  // <<< IMPLEMENTAR
  {"islower", (uintptr_t)&islower},  // pass
  // TODO {"isprint", (uintptr_t)&stub_isprint},  // <<< IMPLEMENTAR
  {"isspace", (uintptr_t)&isspace},  // pass
  {"isupper", (uintptr_t)&isupper},  // pass
  // TODO {"isxdigit", (uintptr_t)&stub_isxdigit},  // <<< IMPLEMENTAR
  // TODO {"kill", (uintptr_t)&stub_kill},  // <<< IMPLEMENTAR
  {"listen", (uintptr_t)&listen},  // pass
  {"localtime", (uintptr_t)&localtime},  // pass
  // TODO {"longjmp", (uintptr_t)&stub_longjmp},  // <<< IMPLEMENTAR
  {"lrand48", (uintptr_t)&lrand48},  // pass
  {"lseek", (uintptr_t)&lseek},  // pass
  // TODO {"lseek64", (uintptr_t)&stub_lseek64},  // <<< IMPLEMENTAR
  {"malloc", (uintptr_t)&malloc},  // pass
  {"memchr", (uintptr_t)&memchr},  // pass
  {"memcmp", (uintptr_t)&memcmp},  // pass
  {"memcpy", (uintptr_t)&memcpy},  // pass
  {"memmove", (uintptr_t)&memmove},  // pass
  // TODO {"memrchr", (uintptr_t)&stub_memrchr},  // <<< IMPLEMENTAR
  {"memset", (uintptr_t)&memset},  // pass
  {"mkdir", (uintptr_t)&mkdir},  // pass
  // TODO {"mlock", (uintptr_t)&stub_mlock},  // <<< IMPLEMENTAR
  {"mmap", (uintptr_t)&mmap},  // pass
  {"modf", (uintptr_t)&modf},  // pass
  {"mprotect", (uintptr_t)&mprotect},  // pass
  {"munmap", (uintptr_t)&munmap},  // pass
  {"nanosleep", (uintptr_t)&nanosleep},  // pass
  {"open", (uintptr_t)&open},  // pass
  {"opendir", (uintptr_t)&opendir},  // pass
  {"perror", (uintptr_t)&perror},  // pass
  {"poll", (uintptr_t)&poll},  // pass
  {"pow", (uintptr_t)&pow},  // pass
  {"powf", (uintptr_t)&powf},  // pass
  // TODO {"prctl", (uintptr_t)&stub_prctl},  // <<< IMPLEMENTAR
  {"printf", (uintptr_t)&printf},  // pass
  {"pthread_attr_destroy", (uintptr_t)&pthread_attr_destroy_fake},  // pthread wrapper (core)
  {"pthread_attr_getstack", (uintptr_t)&pthread_attr_getstack_fake},  // pthread wrapper (core)
  {"pthread_attr_init", (uintptr_t)&pthread_attr_init_fake},  // pthread wrapper (core)
  {"pthread_attr_setschedparam", (uintptr_t)&pthread_attr_setschedparam_fake},  // pthread wrapper (core)
  {"pthread_attr_setschedpolicy", (uintptr_t)&pthread_attr_setschedpolicy_fake},  // pthread wrapper (core)
  {"pthread_attr_setstack", (uintptr_t)&pthread_attr_setstack_fake},  // pthread wrapper (core)
  {"pthread_attr_setstacksize", (uintptr_t)&pthread_attr_setstacksize_fake},  // pthread wrapper (core)
  {"pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake},  // pthread wrapper (core)
  {"pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_fake},  // pthread wrapper (core)
  {"pthread_cond_init", (uintptr_t)&pthread_cond_init_fake},  // pthread wrapper (core)
  {"pthread_cond_signal", (uintptr_t)&pthread_cond_signal_fake},  // pthread wrapper (core)
  {"pthread_cond_timedwait", (uintptr_t)&pthread_cond_timedwait_fake},  // pthread wrapper (core)
  {"pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake},  // pthread wrapper (core)
  {"pthread_create", (uintptr_t)&pthread_create_fake},  // pthread wrapper (core)
  {"pthread_detach", (uintptr_t)&pthread_detach_fake},  // pthread wrapper (core)
  {"pthread_equal", (uintptr_t)&pthread_equal_fake},  // pthread wrapper (core)
  {"pthread_exit", (uintptr_t)&pthread_exit_fake},  // pthread wrapper (core)
  {"pthread_getattr_np", (uintptr_t)&pthread_getattr_np_fake},  // pthread wrapper (core)
  {"pthread_getschedparam", (uintptr_t)&pthread_getschedparam_fake},  // pthread wrapper (core)
  {"pthread_getspecific", (uintptr_t)&pthread_getspecific_fake},  // pthread wrapper (core)
  {"pthread_join", (uintptr_t)&pthread_join_fake},  // pthread wrapper (core)
  {"pthread_key_create", (uintptr_t)&pthread_key_create_fake},  // pthread wrapper (core)
  {"pthread_key_delete", (uintptr_t)&pthread_key_delete_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_destroy", (uintptr_t)&pthread_mutexattr_destroy_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_init", (uintptr_t)&pthread_mutexattr_init_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_setpshared", (uintptr_t)&pthread_mutexattr_setpshared_fake},  // pthread wrapper (core)
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
  {"pthread_rwlock_unlock", (uintptr_t)&pthread_rwlock_unlock_fake},  // pthread wrapper (core)
  {"pthread_rwlock_wrlock", (uintptr_t)&pthread_rwlock_wrlock_fake},  // pthread wrapper (core)
  {"pthread_self", (uintptr_t)&pthread_self_fake},  // pthread wrapper (core)
  {"pthread_setschedparam", (uintptr_t)&pthread_setschedparam_fake},  // pthread wrapper (core)
  {"pthread_setspecific", (uintptr_t)&pthread_setspecific_fake},  // pthread wrapper (core)
  {"putc", (uintptr_t)&putc},  // pass
  // TODO {"putchar", (uintptr_t)&stub_putchar},  // <<< IMPLEMENTAR
  {"puts", (uintptr_t)&puts},  // pass
  {"qsort", (uintptr_t)&qsort},  // pass
  // TODO {"raise", (uintptr_t)&stub_raise},  // <<< IMPLEMENTAR
  {"read", (uintptr_t)&read},  // pass
  {"readdir", (uintptr_t)&readdir},  // pass
  // TODO {"readdir_r", (uintptr_t)&stub_readdir_r},  // <<< IMPLEMENTAR
  // TODO {"readlink", (uintptr_t)&stub_readlink},  // <<< IMPLEMENTAR
  {"realloc", (uintptr_t)&realloc},  // pass
  {"recv", (uintptr_t)&recv},  // pass
  {"recvfrom", (uintptr_t)&recvfrom},  // pass
  {"rename", (uintptr_t)&rename},  // pass
  {"rmdir", (uintptr_t)&rmdir},  // pass
  {"roundf", (uintptr_t)&roundf},  // pass
  // TODO {"sbrk", (uintptr_t)&stub_sbrk},  // <<< IMPLEMENTAR
  {"sched_yield", (uintptr_t)&sched_yield},  // pass
  {"sem_destroy", (uintptr_t)&sem_destroy_fake},  // pthread wrapper (core)
  {"sem_getvalue", (uintptr_t)&sem_getvalue_fake},  // pthread wrapper (core)
  {"sem_init", (uintptr_t)&sem_init_fake},  // pthread wrapper (core)
  {"sem_post", (uintptr_t)&sem_post_fake},  // pthread wrapper (core)
  {"sem_timedwait", (uintptr_t)&sem_timedwait_fake},  // pthread wrapper (core)
  {"sem_trywait", (uintptr_t)&sem_trywait_fake},  // pthread wrapper (core)
  {"sem_wait", (uintptr_t)&sem_wait_fake},  // pthread wrapper (core)
  {"send", (uintptr_t)&send},  // pass
  {"sendto", (uintptr_t)&sendto},  // pass
  // TODO {"setbuf", (uintptr_t)&stub_setbuf},  // <<< IMPLEMENTAR
  {"setenv", (uintptr_t)&setenv},  // pass
  // TODO {"setjmp", (uintptr_t)&stub_setjmp},  // <<< IMPLEMENTAR
  {"setsockopt", (uintptr_t)&setsockopt},  // pass
  // TODO {"__sF", (uintptr_t)&stub___sF},  // <<< IMPLEMENTAR
  {"shutdown", (uintptr_t)&shutdown},  // pass
  // TODO {"sigaction", (uintptr_t)&stub_sigaction},  // <<< IMPLEMENTAR
  // TODO {"siglongjmp", (uintptr_t)&stub_siglongjmp},  // <<< IMPLEMENTAR
  // TODO {"sigprocmask", (uintptr_t)&stub_sigprocmask},  // <<< IMPLEMENTAR
  // TODO {"sigsetjmp", (uintptr_t)&stub_sigsetjmp},  // <<< IMPLEMENTAR
  {"sinf", (uintptr_t)&sinf},  // pass
  {"snprintf", (uintptr_t)&snprintf},  // pass
  {"socket", (uintptr_t)&socket},  // pass
  // TODO {"socketpair", (uintptr_t)&stub_socketpair},  // <<< IMPLEMENTAR
  {"sprintf", (uintptr_t)&sprintf},  // pass
  {"sqrtf", (uintptr_t)&sqrtf},  // pass
  {"srand48", (uintptr_t)&srand48},  // pass
  {"sscanf", (uintptr_t)&sscanf},  // pass
  {"__stack_chk_fail", (uintptr_t)&__stack_chk_fail},  // abi
  {"__stack_chk_guard", (uintptr_t)&__stack_chk_guard},  // abi
  {"stat", (uintptr_t)&stat},  // pass
  // TODO {"statfs", (uintptr_t)&stub_statfs},  // <<< IMPLEMENTAR
  {"strcasecmp", (uintptr_t)&strcasecmp},  // pass
  {"strcat", (uintptr_t)&strcat},  // pass
  {"strchr", (uintptr_t)&strchr},  // pass
  {"strcmp", (uintptr_t)&strcmp},  // pass
  {"strcpy", (uintptr_t)&strcpy},  // pass
  // TODO {"strcspn", (uintptr_t)&stub_strcspn},  // <<< IMPLEMENTAR
  {"strdup", (uintptr_t)&strdup},  // pass
  {"strerror", (uintptr_t)&strerror},  // pass
  // TODO {"strerror_r", (uintptr_t)&stub_strerror_r},  // <<< IMPLEMENTAR
  {"strlen", (uintptr_t)&strlen},  // pass
  {"strncasecmp", (uintptr_t)&strncasecmp},  // pass
  {"strncmp", (uintptr_t)&strncmp},  // pass
  {"strncpy", (uintptr_t)&strncpy},  // pass
  // TODO {"strpbrk", (uintptr_t)&stub_strpbrk},  // <<< IMPLEMENTAR
  {"strrchr", (uintptr_t)&strrchr},  // pass
  // TODO {"strspn", (uintptr_t)&stub_strspn},  // <<< IMPLEMENTAR
  {"strstr", (uintptr_t)&strstr},  // pass
  {"strtod", (uintptr_t)&strtod},  // pass
  // TODO {"strtoimax", (uintptr_t)&stub_strtoimax},  // <<< IMPLEMENTAR
  // TODO {"strtok_r", (uintptr_t)&stub_strtok_r},  // <<< IMPLEMENTAR
  {"strtol", (uintptr_t)&strtol},  // pass
  // TODO {"strtoll", (uintptr_t)&stub_strtoll},  // <<< IMPLEMENTAR
  {"strtoul", (uintptr_t)&strtoul},  // pass
  // TODO {"strtoull", (uintptr_t)&stub_strtoull},  // <<< IMPLEMENTAR
  // TODO {"strtoumax", (uintptr_t)&stub_strtoumax},  // <<< IMPLEMENTAR
  // TODO {"syscall", (uintptr_t)&stub_syscall},  // <<< IMPLEMENTAR
  {"sysconf", (uintptr_t)&sysconf},  // pass
  // TODO {"system", (uintptr_t)&stub_system},  // <<< IMPLEMENTAR
  {"tanf", (uintptr_t)&tanf},  // pass
  {"time", (uintptr_t)&time},  // pass
  // TODO {"timezone", (uintptr_t)&stub_timezone},  // <<< IMPLEMENTAR
  {"tolower", (uintptr_t)&tolower},  // pass
  // TODO {"_tolower_tab_", (uintptr_t)&stub__tolower_tab_},  // <<< IMPLEMENTAR
  {"toupper", (uintptr_t)&toupper},  // pass
  // TODO {"tzname", (uintptr_t)&stub_tzname},  // <<< IMPLEMENTAR
  // TODO {"tzset", (uintptr_t)&stub_tzset},  // <<< IMPLEMENTAR
  // TODO {"ungetc", (uintptr_t)&stub_ungetc},  // <<< IMPLEMENTAR
  {"unlink", (uintptr_t)&unlink},  // pass
  // TODO {"unsetenv", (uintptr_t)&stub_unsetenv},  // <<< IMPLEMENTAR
  // TODO {"utime", (uintptr_t)&stub_utime},  // <<< IMPLEMENTAR
  // TODO {"vfprintf", (uintptr_t)&stub_vfprintf},  // <<< IMPLEMENTAR
  {"vsnprintf", (uintptr_t)&vsnprintf},  // pass
  // TODO {"waitpid", (uintptr_t)&stub_waitpid},  // <<< IMPLEMENTAR
  {"write", (uintptr_t)&write},  // pass
  // TODO {"_ZN2EA6Nimble6getEnvEv", (uintptr_t)&stub__ZN2EA6Nimble6getEnvEv},  // <<< IMPLEMENTAR
  // TODO {"_ZN2EA6Nimble9findClassEPKc", (uintptr_t)&stub__ZN2EA6Nimble9findClassEPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem11getCategoryEPKcPPNS_13EventCategoryE", (uintptr_t)&stub__ZN4FMOD11EventSystem11getCategoryEPKcPPNS_13EventCategoryE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem14getMusicSystemEPPNS_11MusicSystemE", (uintptr_t)&stub__ZN4FMOD11EventSystem14getMusicSystemEPPNS_11MusicSystemE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem15getReverbPresetEPKcP22FMOD_REVERB_PROPERTIESPi", (uintptr_t)&stub__ZN4FMOD11EventSystem15getReverbPresetEPKcP22FMOD_REVERB_PROPERTIESPi},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem15getSystemObjectEPPNS_6SystemE", (uintptr_t)&stub__ZN4FMOD11EventSystem15getSystemObjectEPPNS_6SystemE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem19setReverbPropertiesEPK22FMOD_REVERB_PROPERTIES", (uintptr_t)&stub__ZN4FMOD11EventSystem19setReverbPropertiesEPK22FMOD_REVERB_PROPERTIES},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem23set3DListenerAttributesEiPK11FMOD_VECTORS3_S3_S3_", (uintptr_t)&stub__ZN4FMOD11EventSystem23set3DListenerAttributesEiPK11FMOD_VECTORS3_S3_S3_},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem4initEijPvj", (uintptr_t)&stub__ZN4FMOD11EventSystem4initEijPvj},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem4loadEPKcP19FMOD_EVENT_LOADINFOPPNS_12EventProjectE", (uintptr_t)&stub__ZN4FMOD11EventSystem4loadEPKcP19FMOD_EVENT_LOADINFOPPNS_12EventProjectE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem6updateEv", (uintptr_t)&stub__ZN4FMOD11EventSystem6updateEv},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem7releaseEv", (uintptr_t)&stub__ZN4FMOD11EventSystem7releaseEv},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem8getEventEPKcjPPNS_5EventE", (uintptr_t)&stub__ZN4FMOD11EventSystem8getEventEPKcjPPNS_5EventE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD11EventSystem8getGroupEPKcbPPNS_10EventGroupE", (uintptr_t)&stub__ZN4FMOD11EventSystem8getGroupEPKcbPPNS_10EventGroupE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD12ChannelGroup15getSystemObjectEPPNS_6SystemE", (uintptr_t)&stub__ZN4FMOD12ChannelGroup15getSystemObjectEPPNS_6SystemE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD12ChannelGroup6addDSPEPNS_3DSPEPPNS_13DSPConnectionE", (uintptr_t)&stub__ZN4FMOD12ChannelGroup6addDSPEPNS_3DSPEPPNS_13DSPConnectionE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD14EventParameter6keyOffEv", (uintptr_t)&stub__ZN4FMOD14EventParameter6keyOffEv},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD14EventParameter8getValueEPf", (uintptr_t)&stub__ZN4FMOD14EventParameter8getValueEPf},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD14EventParameter8setValueEf", (uintptr_t)&stub__ZN4FMOD14EventParameter8setValueEf},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD3DSP12setParameterEif", (uintptr_t)&stub__ZN4FMOD3DSP12setParameterEif},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD3DSP7releaseEv", (uintptr_t)&stub__ZN4FMOD3DSP7releaseEv},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event11getCategoryEPPNS_13EventCategoryE", (uintptr_t)&stub__ZN4FMOD5Event11getCategoryEPPNS_13EventCategoryE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event11setCallbackEPF11FMOD_RESULTP10FMOD_EVENT23FMOD_EVENT_CALLBACKTYPEPvS5_S5_ES5_", (uintptr_t)&stub__ZN4FMOD5Event11setCallbackEPF11FMOD_RESULTP10FMOD_EVENT23FMOD_EVENT_CALLBACKTYPEPvS5_S5_ES5_},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event12getParameterEPKcPPNS_14EventParameterE", (uintptr_t)&stub__ZN4FMOD5Event12getParameterEPKcPPNS_14EventParameterE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event14get3DOcclusionEPfS1_", (uintptr_t)&stub__ZN4FMOD5Event14get3DOcclusionEPfS1_},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event14set3DOcclusionEff", (uintptr_t)&stub__ZN4FMOD5Event14set3DOcclusionEff},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event15get3DAttributesEP11FMOD_VECTORS2_S2_", (uintptr_t)&stub__ZN4FMOD5Event15get3DAttributesEP11FMOD_VECTORS2_S2_},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event15getChannelGroupEPPNS_12ChannelGroupE", (uintptr_t)&stub__ZN4FMOD5Event15getChannelGroupEPPNS_12ChannelGroupE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event15set3DAttributesEPK11FMOD_VECTORS3_S3_", (uintptr_t)&stub__ZN4FMOD5Event15set3DAttributesEPK11FMOD_VECTORS3_S3_},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event18setPropertyByIndexEiPvb", (uintptr_t)&stub__ZN4FMOD5Event18setPropertyByIndexEiPvb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event4stopEb", (uintptr_t)&stub__ZN4FMOD5Event4stopEb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event5startEv", (uintptr_t)&stub__ZN4FMOD5Event5startEv},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event7getInfoEPiPPcP15FMOD_EVENT_INFO", (uintptr_t)&stub__ZN4FMOD5Event7getInfoEPiPPcP15FMOD_EVENT_INFO},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event7getMuteEPb", (uintptr_t)&stub__ZN4FMOD5Event7getMuteEPb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event7releaseEbb", (uintptr_t)&stub__ZN4FMOD5Event7releaseEbb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event7setMuteEb", (uintptr_t)&stub__ZN4FMOD5Event7setMuteEb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event8getPitchEPf21FMOD_EVENT_PITCHUNITS", (uintptr_t)&stub__ZN4FMOD5Event8getPitchEPf21FMOD_EVENT_PITCHUNITS},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event8getStateEPj", (uintptr_t)&stub__ZN4FMOD5Event8getStateEPj},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event8setPitchEf21FMOD_EVENT_PITCHUNITS", (uintptr_t)&stub__ZN4FMOD5Event8setPitchEf21FMOD_EVENT_PITCHUNITS},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event9getPausedEPb", (uintptr_t)&stub__ZN4FMOD5Event9getPausedEPb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event9getVolumeEPf", (uintptr_t)&stub__ZN4FMOD5Event9getVolumeEPf},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event9setPausedEb", (uintptr_t)&stub__ZN4FMOD5Event9setPausedEb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Event9setVolumeEf", (uintptr_t)&stub__ZN4FMOD5Event9setVolumeEf},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD5Sound7releaseEv", (uintptr_t)&stub__ZN4FMOD5Sound7releaseEv},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System11createSoundEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE", (uintptr_t)&stub__ZN4FMOD6System11createSoundEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System11getCPUUsageEPfS1_S1_S1_S1_", (uintptr_t)&stub__ZN4FMOD6System11getCPUUsageEPfS1_S1_S1_S1_},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKciPjPPvS6_EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESA_i", (uintptr_t)&stub__ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKciPjPPvS6_EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESA_i},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System14setSpeakerModeE16FMOD_SPEAKERMODE", (uintptr_t)&stub__ZN4FMOD6System14setSpeakerModeE16FMOD_SPEAKERMODE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System15createDSPByTypeE13FMOD_DSP_TYPEPPNS_3DSPE", (uintptr_t)&stub__ZN4FMOD6System15createDSPByTypeE13FMOD_DSP_TYPEPPNS_3DSPE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System16setDSPBufferSizeEji", (uintptr_t)&stub__ZN4FMOD6System16setDSPBufferSizeEji},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System17getSoftwareFormatEPiP17FMOD_SOUND_FORMATS1_S1_P18FMOD_DSP_RESAMPLERS1_", (uintptr_t)&stub__ZN4FMOD6System17getSoftwareFormatEPiP17FMOD_SOUND_FORMATS1_S1_P18FMOD_DSP_RESAMPLERS1_},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System17setSoftwareFormatEi17FMOD_SOUND_FORMATii18FMOD_DSP_RESAMPLER", (uintptr_t)&stub__ZN4FMOD6System17setSoftwareFormatEi17FMOD_SOUND_FORMATii18FMOD_DSP_RESAMPLER},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System20set3DSpeakerPositionE12FMOD_SPEAKERffb", (uintptr_t)&stub__ZN4FMOD6System20set3DSpeakerPositionE12FMOD_SPEAKERffb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System9playSoundE17FMOD_CHANNELINDEXPNS_5SoundEbPPNS_7ChannelE", (uintptr_t)&stub__ZN4FMOD6System9playSoundE17FMOD_CHANNELINDEXPNS_5SoundEbPPNS_7ChannelE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD6System9setOutputE15FMOD_OUTPUTTYPE", (uintptr_t)&stub__ZN4FMOD6System9setOutputE15FMOD_OUTPUTTYPE},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD7Channel11setCallbackEPF11FMOD_RESULTP12FMOD_CHANNEL25FMOD_CHANNEL_CALLBACKTYPEPvS5_E", (uintptr_t)&stub__ZN4FMOD7Channel11setCallbackEPF11FMOD_RESULTP12FMOD_CHANNEL25FMOD_CHANNEL_CALLBACKTYPEPvS5_E},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD7Channel11setPriorityEi", (uintptr_t)&stub__ZN4FMOD7Channel11setPriorityEi},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD7Channel4stopEv", (uintptr_t)&stub__ZN4FMOD7Channel4stopEv},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD7Channel7setMuteEb", (uintptr_t)&stub__ZN4FMOD7Channel7setMuteEb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD7Channel9getPausedEPb", (uintptr_t)&stub__ZN4FMOD7Channel9getPausedEPb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD7Channel9isPlayingEPb", (uintptr_t)&stub__ZN4FMOD7Channel9isPlayingEPb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD7Channel9setPausedEb", (uintptr_t)&stub__ZN4FMOD7Channel9setPausedEb},  // <<< IMPLEMENTAR
  // TODO {"_ZN4FMOD7Channel9setVolumeEf", (uintptr_t)&stub__ZN4FMOD7Channel9setVolumeEf},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt11logic_error4whatEv", (uintptr_t)&stub__ZNKSt11logic_error4whatEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt13runtime_error4whatEv", (uintptr_t)&stub__ZNKSt13runtime_error4whatEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk119__shared_weak_count13__get_deleterERKSt9type_info", (uintptr_t)&stub__ZNKSt6__ndk119__shared_weak_count13__get_deleterERKSt9type_info},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk16locale9use_facetERNS0_2idE", (uintptr_t)&stub__ZNKSt6__ndk16locale9use_facetERNS0_2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNKSt6__ndk18ios_base6getlocEv", (uintptr_t)&stub__ZNKSt6__ndk18ios_base6getlocEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt11logic_errorC2EPKc", (uintptr_t)&stub__ZNSt11logic_errorC2EPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt13runtime_errorC2EPKc", (uintptr_t)&stub__ZNSt13runtime_errorC2EPKc},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt13runtime_errorD2Ev", (uintptr_t)&stub__ZNSt13runtime_errorD2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_weak_count12__add_sharedEv", (uintptr_t)&stub__ZNSt6__ndk119__shared_weak_count12__add_sharedEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_weak_count14__release_weakEv", (uintptr_t)&stub__ZNSt6__ndk119__shared_weak_count14__release_weakEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_weak_count16__release_sharedEv", (uintptr_t)&stub__ZNSt6__ndk119__shared_weak_count16__release_sharedEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk119__shared_weak_countD2Ev", (uintptr_t)&stub__ZNSt6__ndk119__shared_weak_countD2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk14cerrE", (uintptr_t)&stub__ZNSt6__ndk14cerrE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15ctypeIcE2idE", (uintptr_t)&stub__ZNSt6__ndk15ctypeIcE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutex4lockEv", (uintptr_t)&stub__ZNSt6__ndk15mutex4lockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutex6unlockEv", (uintptr_t)&stub__ZNSt6__ndk15mutex6unlockEv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk15mutexD1Ev", (uintptr_t)&stub__ZNSt6__ndk15mutexD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeC1Ev", (uintptr_t)&stub__ZNSt6__ndk16localeC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk16localeD1Ev", (uintptr_t)&stub__ZNSt6__ndk16localeD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE", (uintptr_t)&stub__ZNSt6__ndk17num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk17num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE", (uintptr_t)&stub__ZNSt6__ndk17num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base4initEPv", (uintptr_t)&stub__ZNSt6__ndk18ios_base4initEPv},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_base5clearEj", (uintptr_t)&stub__ZNSt6__ndk18ios_base5clearEj},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt6__ndk18ios_baseD2Ev", (uintptr_t)&stub__ZNSt6__ndk18ios_baseD2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt9bad_allocC1Ev", (uintptr_t)&stub__ZNSt9bad_allocC1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt9bad_allocD1Ev", (uintptr_t)&stub__ZNSt9bad_allocD1Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZNSt9exceptionD2Ev", (uintptr_t)&stub__ZNSt9exceptionD2Ev},  // <<< IMPLEMENTAR
  // TODO {"_ZSt18uncaught_exceptionv", (uintptr_t)&stub__ZSt18uncaught_exceptionv},  // <<< IMPLEMENTAR
  // TODO {"_ZTIa", (uintptr_t)&stub__ZTIa},  // <<< IMPLEMENTAR
  // TODO {"_ZTIf", (uintptr_t)&stub__ZTIf},  // <<< IMPLEMENTAR
  // TODO {"_ZTIi", (uintptr_t)&stub__ZTIi},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk119__shared_weak_countE", (uintptr_t)&stub__ZTINSt6__ndk119__shared_weak_countE},  // <<< IMPLEMENTAR
  // TODO {"_ZTINSt6__ndk18ios_baseE", (uintptr_t)&stub__ZTINSt6__ndk18ios_baseE},  // <<< IMPLEMENTAR
  // TODO {"_ZTIs", (uintptr_t)&stub__ZTIs},  // <<< IMPLEMENTAR
  // TODO {"_ZTISt13runtime_error", (uintptr_t)&stub__ZTISt13runtime_error},  // <<< IMPLEMENTAR
  // TODO {"_ZTISt9exception", (uintptr_t)&stub__ZTISt9exception},  // <<< IMPLEMENTAR
  // TODO {"_ZTIt", (uintptr_t)&stub__ZTIt},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv117__class_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv117__class_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv119__pointer_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv119__pointer_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv120__function_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv120__function_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv120__si_class_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv120__si_class_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVN10__cxxabiv121__vmi_class_type_infoE", (uintptr_t)&stub__ZTVN10__cxxabiv121__vmi_class_type_infoE},  // <<< IMPLEMENTAR
  // TODO {"_ZTVSt12length_error", (uintptr_t)&stub__ZTVSt12length_error},  // <<< IMPLEMENTAR
};
const int dynlib_functions_count = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);

// ===================== SIMBOLOS A IMPLEMENTAR (UNKNOWN) =====================
//   acosf
//   AndroidBitmap_getInfo
//   AndroidBitmap_lockPixels
//   AndroidBitmap_unlockPixels
//   asinf
//   basename
//   chmod
//   clock
//   _ctype_
//   dladdr
//   dlclose
//   dlerror
//   dlopen
//   dlsym
//   __dynamic_cast
//   execl
//   execv
//   feof
//   ferror
//   FMOD_EventSystem_Create
//   FMOD_Memory_Initialize
//   fork
//   freeaddrinfo
//   fsync
//   ftruncate
//   fwide
//   gai_strerror
//   getaddrinfo
//   getauxval
//   getegid
//   geteuid
//   getgid
//   gethostname
//   getnameinfo
//   getpeername
//   getpwuid_r
//   getuid
//   gmtime_r
//   if_nametoindex
//   isgraph
//   isprint
//   isxdigit
//   kill
//   longjmp
//   lseek64
//   memrchr
//   mlock
//   prctl
//   putchar
//   raise
//   readdir_r
//   readlink
//   sbrk
//   setbuf
//   setjmp
//   __sF
//   sigaction
//   siglongjmp
//   sigprocmask
//   sigsetjmp
//   socketpair
//   statfs
//   strcspn
//   strerror_r
//   strpbrk
//   strspn
//   strtoimax
//   strtok_r
//   strtoll
//   strtoull
//   strtoumax
//   syscall
//   system
//   timezone
//   _tolower_tab_
//   tzname
//   tzset
//   ungetc
//   unsetenv
//   utime
//   vfprintf
//   waitpid
//   _ZN2EA6Nimble6getEnvEv
//   _ZN2EA6Nimble9findClassEPKc
//   _ZN4FMOD11EventSystem11getCategoryEPKcPPNS_13EventCategoryE
//   _ZN4FMOD11EventSystem14getMusicSystemEPPNS_11MusicSystemE
//   _ZN4FMOD11EventSystem15getReverbPresetEPKcP22FMOD_REVERB_PROPERTIESPi
//   _ZN4FMOD11EventSystem15getSystemObjectEPPNS_6SystemE
//   _ZN4FMOD11EventSystem19setReverbPropertiesEPK22FMOD_REVERB_PROPERTIES
//   _ZN4FMOD11EventSystem23set3DListenerAttributesEiPK11FMOD_VECTORS3_S3_S3_
//   _ZN4FMOD11EventSystem4initEijPvj
//   _ZN4FMOD11EventSystem4loadEPKcP19FMOD_EVENT_LOADINFOPPNS_12EventProjectE
//   _ZN4FMOD11EventSystem6updateEv
//   _ZN4FMOD11EventSystem7releaseEv
//   _ZN4FMOD11EventSystem8getEventEPKcjPPNS_5EventE
//   _ZN4FMOD11EventSystem8getGroupEPKcbPPNS_10EventGroupE
//   _ZN4FMOD12ChannelGroup15getSystemObjectEPPNS_6SystemE
//   _ZN4FMOD12ChannelGroup6addDSPEPNS_3DSPEPPNS_13DSPConnectionE
//   _ZN4FMOD14EventParameter6keyOffEv
//   _ZN4FMOD14EventParameter8getValueEPf
//   _ZN4FMOD14EventParameter8setValueEf
//   _ZN4FMOD3DSP12setParameterEif
//   _ZN4FMOD3DSP7releaseEv
//   _ZN4FMOD5Event11getCategoryEPPNS_13EventCategoryE
//   _ZN4FMOD5Event11setCallbackEPF11FMOD_RESULTP10FMOD_EVENT23FMOD_EVENT_CALLBACKTYPEPvS5_S5_ES5_
//   _ZN4FMOD5Event12getParameterEPKcPPNS_14EventParameterE
//   _ZN4FMOD5Event14get3DOcclusionEPfS1_
//   _ZN4FMOD5Event14set3DOcclusionEff
//   _ZN4FMOD5Event15get3DAttributesEP11FMOD_VECTORS2_S2_
//   _ZN4FMOD5Event15getChannelGroupEPPNS_12ChannelGroupE
//   _ZN4FMOD5Event15set3DAttributesEPK11FMOD_VECTORS3_S3_
//   _ZN4FMOD5Event18setPropertyByIndexEiPvb
//   _ZN4FMOD5Event4stopEb
//   _ZN4FMOD5Event5startEv
//   _ZN4FMOD5Event7getInfoEPiPPcP15FMOD_EVENT_INFO
//   _ZN4FMOD5Event7getMuteEPb
//   _ZN4FMOD5Event7releaseEbb
//   _ZN4FMOD5Event7setMuteEb
//   _ZN4FMOD5Event8getPitchEPf21FMOD_EVENT_PITCHUNITS
//   _ZN4FMOD5Event8getStateEPj
//   _ZN4FMOD5Event8setPitchEf21FMOD_EVENT_PITCHUNITS
//   _ZN4FMOD5Event9getPausedEPb
//   _ZN4FMOD5Event9getVolumeEPf
//   _ZN4FMOD5Event9setPausedEb
//   _ZN4FMOD5Event9setVolumeEf
//   _ZN4FMOD5Sound7releaseEv
//   _ZN4FMOD6System11createSoundEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE
//   _ZN4FMOD6System11getCPUUsageEPfS1_S1_S1_S1_
//   _ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKciPjPPvS6_EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESA_i
//   _ZN4FMOD6System14setSpeakerModeE16FMOD_SPEAKERMODE
//   _ZN4FMOD6System15createDSPByTypeE13FMOD_DSP_TYPEPPNS_3DSPE
//   _ZN4FMOD6System16setDSPBufferSizeEji
//   _ZN4FMOD6System17getSoftwareFormatEPiP17FMOD_SOUND_FORMATS1_S1_P18FMOD_DSP_RESAMPLERS1_
//   _ZN4FMOD6System17setSoftwareFormatEi17FMOD_SOUND_FORMATii18FMOD_DSP_RESAMPLER
//   _ZN4FMOD6System20set3DSpeakerPositionE12FMOD_SPEAKERffb
//   _ZN4FMOD6System9playSoundE17FMOD_CHANNELINDEXPNS_5SoundEbPPNS_7ChannelE
//   _ZN4FMOD6System9setOutputE15FMOD_OUTPUTTYPE
//   _ZN4FMOD7Channel11setCallbackEPF11FMOD_RESULTP12FMOD_CHANNEL25FMOD_CHANNEL_CALLBACKTYPEPvS5_E
//   _ZN4FMOD7Channel11setPriorityEi
//   _ZN4FMOD7Channel4stopEv
//   _ZN4FMOD7Channel7setMuteEb
//   _ZN4FMOD7Channel9getPausedEPb
//   _ZN4FMOD7Channel9isPlayingEPb
//   _ZN4FMOD7Channel9setPausedEb
//   _ZN4FMOD7Channel9setVolumeEf
//   _ZNKSt11logic_error4whatEv
//   _ZNKSt13runtime_error4whatEv
//   _ZNKSt6__ndk119__shared_weak_count13__get_deleterERKSt9type_info
//   _ZNKSt6__ndk16locale9use_facetERNS0_2idE
//   _ZNKSt6__ndk18ios_base6getlocEv
//   _ZNSt11logic_errorC2EPKc
//   _ZNSt13runtime_errorC2EPKc
//   _ZNSt13runtime_errorD2Ev
//   _ZNSt6__ndk119__shared_weak_count12__add_sharedEv
//   _ZNSt6__ndk119__shared_weak_count14__release_weakEv
//   _ZNSt6__ndk119__shared_weak_count16__release_sharedEv
//   _ZNSt6__ndk119__shared_weak_countD2Ev
//   _ZNSt6__ndk14cerrE
//   _ZNSt6__ndk15ctypeIcE2idE
//   _ZNSt6__ndk15mutex4lockEv
//   _ZNSt6__ndk15mutex6unlockEv
//   _ZNSt6__ndk15mutexD1Ev
//   _ZNSt6__ndk16localeC1Ev
//   _ZNSt6__ndk16localeD1Ev
//   _ZNSt6__ndk17num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE
//   _ZNSt6__ndk17num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE
//   _ZNSt6__ndk18ios_base4initEPv
//   _ZNSt6__ndk18ios_base5clearEj
//   _ZNSt6__ndk18ios_baseD2Ev
//   _ZNSt9bad_allocC1Ev
//   _ZNSt9bad_allocD1Ev
//   _ZNSt9exceptionD2Ev
//   _ZSt18uncaught_exceptionv
//   _ZTIa
//   _ZTIf
//   _ZTIi
//   _ZTINSt6__ndk119__shared_weak_countE
//   _ZTINSt6__ndk18ios_baseE
//   _ZTIs
//   _ZTISt13runtime_error
//   _ZTISt9exception
//   _ZTIt
//   _ZTVN10__cxxabiv117__class_type_infoE
//   _ZTVN10__cxxabiv119__pointer_type_infoE
//   _ZTVN10__cxxabiv120__function_type_infoE
//   _ZTVN10__cxxabiv120__si_class_type_infoE
//   _ZTVN10__cxxabiv121__vmi_class_type_infoE
//   _ZTVSt12length_error
