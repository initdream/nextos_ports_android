/*
 * imports.c -- .so import resolution for LSWTCS ARM64
 *
 * Maps all 286 undefined symbols from libTTapp.so to real
 * libc/GL/EGL functions or our shim implementations.
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>

#include "android_shim.h"
#include "egl_shim.h"
#include "opensles_shim.h"
#include "so_util.h"
#include "util.h"

extern uintptr_t __cxa_atexit;
extern uintptr_t __cxa_finalize;

FILE *stderr_fake = (FILE *)0x1337;

static uint8_t fake_sF[3][0x100];
static uint64_t __stack_chk_guard_fake = 0x4242424242424242;

typedef struct HostMutexEntry {
  void *guest_addr;
  pthread_mutex_t mutex;
  struct HostMutexEntry *next;
} HostMutexEntry;

typedef struct HostCondEntry {
  void *guest_addr;
  pthread_cond_t cond;
  struct HostCondEntry *next;
} HostCondEntry;

static HostMutexEntry *g_mutex_entries = NULL;
static HostCondEntry *g_cond_entries = NULL;
static pthread_mutex_t g_mutex_registry_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_cond_registry_lock = PTHREAD_MUTEX_INITIALIZER;
/* Vita-style: just log and return — no abort, no loop */
static void __stack_chk_fail_stub(void) {
  uintptr_t ra = (uintptr_t)__builtin_return_address(0);
  if (text_base && ra >= (uintptr_t)text_base &&
      ra < (uintptr_t)text_base + text_size) {
    debugPrintf("__stack_chk_fail called from %p (libTTapp.so+0x%lx)\n",
                (void *)ra, (unsigned long)(ra - (uintptr_t)text_base));
  } else if (data_base && ra >= (uintptr_t)data_base &&
             ra < (uintptr_t)data_base + data_size) {
    debugPrintf("__stack_chk_fail called from %p (libTTapp.so[data]+0x%lx)\n",
                (void *)ra, (unsigned long)(ra - (uintptr_t)data_base));
  } else {
    debugPrintf("__stack_chk_fail called from %p\n", (void *)ra);
  }
}

/* errno compat */
static int *__errno_fake(void) { return &errno; }

static pthread_mutex_t *lookup_host_mutex(void *guest_addr, int create) {
  if (!guest_addr)
    return NULL;

  pthread_mutex_lock(&g_mutex_registry_lock);
  for (HostMutexEntry *entry = g_mutex_entries; entry; entry = entry->next) {
    if (entry->guest_addr == guest_addr) {
      pthread_mutex_unlock(&g_mutex_registry_lock);
      return &entry->mutex;
    }
  }

  if (!create) {
    pthread_mutex_unlock(&g_mutex_registry_lock);
    return NULL;
  }

  HostMutexEntry *entry = calloc(1, sizeof(*entry));
  if (!entry) {
    pthread_mutex_unlock(&g_mutex_registry_lock);
    return NULL;
  }

  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&entry->mutex, &attr);
  pthread_mutexattr_destroy(&attr);

  entry->guest_addr = guest_addr;
  entry->next = g_mutex_entries;
  g_mutex_entries = entry;
  pthread_mutex_unlock(&g_mutex_registry_lock);
  return &entry->mutex;
}

static int destroy_host_mutex(void *guest_addr) {
  if (!guest_addr)
    return 0;

  pthread_mutex_lock(&g_mutex_registry_lock);
  HostMutexEntry **link = &g_mutex_entries;
  while (*link) {
    HostMutexEntry *entry = *link;
    if (entry->guest_addr == guest_addr) {
      *link = entry->next;
      pthread_mutex_unlock(&g_mutex_registry_lock);
      pthread_mutex_destroy(&entry->mutex);
      free(entry);
      return 0;
    }
    link = &entry->next;
  }
  pthread_mutex_unlock(&g_mutex_registry_lock);
  return 0;
}

static pthread_cond_t *lookup_host_cond(void *guest_addr, int create) {
  if (!guest_addr)
    return NULL;

  pthread_mutex_lock(&g_cond_registry_lock);
  for (HostCondEntry *entry = g_cond_entries; entry; entry = entry->next) {
    if (entry->guest_addr == guest_addr) {
      pthread_mutex_unlock(&g_cond_registry_lock);
      return &entry->cond;
    }
  }

  if (!create) {
    pthread_mutex_unlock(&g_cond_registry_lock);
    return NULL;
  }

  HostCondEntry *entry = calloc(1, sizeof(*entry));
  if (!entry) {
    pthread_mutex_unlock(&g_cond_registry_lock);
    return NULL;
  }

  pthread_cond_init(&entry->cond, NULL);
  entry->guest_addr = guest_addr;
  entry->next = g_cond_entries;
  g_cond_entries = entry;
  pthread_mutex_unlock(&g_cond_registry_lock);
  return &entry->cond;
}

static int destroy_host_cond(void *guest_addr) {
  if (!guest_addr)
    return 0;

  pthread_mutex_lock(&g_cond_registry_lock);
  HostCondEntry **link = &g_cond_entries;
  while (*link) {
    HostCondEntry *entry = *link;
    if (entry->guest_addr == guest_addr) {
      *link = entry->next;
      pthread_mutex_unlock(&g_cond_registry_lock);
      pthread_cond_destroy(&entry->cond);
      free(entry);
      return 0;
    }
    link = &entry->next;
  }
  pthread_mutex_unlock(&g_cond_registry_lock);
  return 0;
}

/* __android_log */
int __android_log_print_fake(int prio, const char *tag, const char *fmt, ...) {
  (void)prio;
  va_list list;
  static char string[0x1000];
  va_start(list, fmt);
  vsnprintf(string, sizeof(string), fmt, list);
  va_end(list);
  debugPrintf("LOG [%s]: %s\n", tag, string);
  return 0;
}

/* fortified libc stubs */
void *__memcpy_chk(void *dst, const void *src, size_t n, size_t dst_len) {
  (void)dst_len;
  return memcpy(dst, src, n);
}

void *__memmove_chk(void *dst, const void *src, size_t n, size_t dst_len) {
  (void)dst_len;
  return memmove(dst, src, n);
}

void *__memset_chk(void *dst, int c, size_t n, size_t dst_len) {
  (void)dst_len;
  return memset(dst, c, n);
}

char *__strcat_chk(char *dst, const char *src, size_t dst_buf_size) {
  (void)dst_buf_size;
  return strcat(dst, src);
}

char *__strcpy_chk(char *dst, const char *src, size_t dst_len) {
  (void)dst_len;
  return strcpy(dst, src);
}

size_t __strlen_chk(const char *s, size_t max_len) {
  (void)max_len;
  return strlen(s);
}

char *__strrchr_chk(const char *s, int c, size_t n) {
  (void)n;
  return strrchr(s, c);
}

int __vsprintf_chk(char *dst, int flags, size_t dst_len_from_compiler,
                    const char *fmt, va_list ap) {
  (void)flags;
  (void)dst_len_from_compiler;
  return vsprintf(dst, fmt, ap);
}

int __vsnprintf_chk(char *dst, size_t supplied_size, int flags,
                     size_t dst_len_from_compiler, const char *fmt,
                     va_list ap) {
  (void)flags;
  (void)dst_len_from_compiler;
  return vsnprintf(dst, supplied_size, fmt, ap);
}

ssize_t __read_chk(int fd, void *buf, size_t count, size_t buf_size) {
  (void)buf_size;
  return read(fd, buf, count);
}

int __open_2(const char *pathname, int flags) {
  const char *resolved = resolve_android_path(pathname);
  int fd = open(resolved, flags);
  if (strncmp(pathname, "/dev/", 5) != 0) {
    debugPrintf("open(\"%s\" -> \"%s\", 0x%x) = %d\n", pathname, resolved, flags, fd);
  }
  return fd;
}

/* open() wrapper for debugging — skip /dev/ spam */
int open_fake(const char *pathname, int flags, ...) {
  const char *resolved = resolve_android_path(pathname);
  int fd;
  mode_t mode = 0;
  int has_mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = (mode_t)va_arg(ap, int);
    va_end(ap);
    has_mode = 1;
    fd = open(resolved, flags, mode);
  } else {
    fd = open(resolved, flags);
  }
  if (strncmp(pathname, "/dev/", 5) != 0) {
    if (fd >= 0) {
      if (has_mode)
        debugPrintf("open(\"%s\" -> \"%s\", 0x%x, 0%o) = %d\n",
                    pathname, resolved, flags, (unsigned)mode, fd);
      else
        debugPrintf("open(\"%s\" -> \"%s\", 0x%x) = %d\n",
                    pathname, resolved, flags, fd);
    } else {
      if (has_mode)
        debugPrintf("open(\"%s\" -> \"%s\", 0x%x, 0%o) = %d (errno=%d: %s)\n",
                    pathname, resolved, flags, (unsigned)mode, fd, errno, strerror(errno));
      else
        debugPrintf("open(\"%s\" -> \"%s\", 0x%x) = %d (errno=%d: %s)\n",
                    pathname, resolved, flags, fd, errno, strerror(errno));
    }
  }
  return fd;
}

static int mkdir_fake(const char *pathname, mode_t mode) {
  const char *resolved = resolve_android_path(pathname);
  int ret = mkdir(resolved, mode);
  if (ret == 0)
    debugPrintf("mkdir(\"%s\" -> \"%s\", 0%o) = 0\n",
                pathname, resolved, (unsigned)mode);
  else
    debugPrintf("mkdir(\"%s\" -> \"%s\", 0%o) = -1 (errno=%d: %s)\n",
                pathname, resolved, (unsigned)mode, errno, strerror(errno));
  return ret;
}

static int remove_fake(const char *pathname) {
  const char *resolved = resolve_android_path(pathname);
  int ret = remove(resolved);
  if (ret == 0)
    debugPrintf("remove(\"%s\" -> \"%s\") = 0\n", pathname, resolved);
  else
    debugPrintf("remove(\"%s\" -> \"%s\") = -1 (errno=%d: %s)\n",
                pathname, resolved, errno, strerror(errno));
  return ret;
}

static int rename_fake(const char *oldpath, const char *newpath) {
  char resolved_old[2048];
  char resolved_new[2048];
  const char *resolved_old_src = resolve_android_path(oldpath);
  const char *resolved_new_src;
  int ret;

  SDL_strlcpy(resolved_old, resolved_old_src, sizeof(resolved_old));
  resolved_new_src = resolve_android_path(newpath);
  SDL_strlcpy(resolved_new, resolved_new_src, sizeof(resolved_new));
  ret = rename(resolved_old, resolved_new);
  if (ret == 0) {
    debugPrintf("rename(\"%s\" -> \"%s\", \"%s\" -> \"%s\") = 0\n",
                oldpath, resolved_old, newpath, resolved_new);
  } else {
    debugPrintf("rename(\"%s\" -> \"%s\", \"%s\" -> \"%s\") = -1 (errno=%d: %s)\n",
                oldpath, resolved_old, newpath, resolved_new, errno, strerror(errno));
  }
  return ret;
}

/* ctype compat */
size_t __ctype_get_mb_cur_max_fake(void) { return 4; }

/* dl_iterate_phdr stub */
int dl_iterate_phdr_fake(void *callback, void *data) {
  (void)callback;
  (void)data;
  return 0;
}

/* android_set_abort_message stub */
void android_set_abort_message_fake(const char *msg) {
  debugPrintf("android_set_abort_message: %s\n", msg ? msg : "(null)");
}

/* dlopen/dlsym stubs — game may dynamically load libGLESv2.so etc. */
void *dlopen_fake(const char *filename, int flags) {
  debugPrintf("dlopen(\"%s\", %d)\n", filename ? filename : "(null)", flags);
  return (void *)0xDEAD0001; /* non-NULL dummy handle */
}

void *dlsym_fake(void *handle, const char *symbol) {
  debugPrintf("dlsym(%p, \"%s\")\n", handle, symbol);
  /* Try SDL GL proc address first (covers GL/EGL extensions) */
  void *ptr = SDL_GL_GetProcAddress(symbol);
  if (ptr) return ptr;
  debugPrintf("dlsym: NOT FOUND: %s\n", symbol);
  return NULL;
}

int dlclose_fake(void *handle) { (void)handle; return 0; }
char *dlerror_fake(void) { return NULL; }
int dladdr_fake(void *addr, void *info) { (void)addr; (void)info; return 0; }

/* getenv/setenv stubs */
char *getenv_fake(const char *name) {
  debugPrintf("getenv(\"%s\") -> NULL\n", name);
  return NULL;
}

int setenv_fake(const char *name, const char *value, int overwrite) {
  (void)name; (void)value; (void)overwrite;
  return 0;
}

/* __system_property_get stub */
int __system_property_get_fake(const char *name, char *value) {
  debugPrintf("__system_property_get(\"%s\")\n", name);
  value[0] = '\0';
  return 0;
}

/* Vita-style: log then call real function */
void abort_fake(void) {
  debugPrintf("abort() called from %p\n", __builtin_return_address(0));
  abort();
}

void exit_fake(int status) {
  debugPrintf("exit(%d) called from %p\n", status, __builtin_return_address(0));
  _exit(status);
}

/* Vita-style: stub sigaction — game shouldn't install signal handlers */
int sigaction_fake(int signum, const void *act, void *oldact) {
  (void)signum; (void)act; (void)oldact;
  return 0;
}

/* fopen wrapper for debugging */
FILE *fopen_fake(const char *filename, const char *mode) {
  const char *resolved = resolve_android_path(filename);
  FILE *f;

  f = fopen(resolved, mode);
  if (!f) {
    debugPrintf("fopen(\"%s\" -> \"%s\", \"%s\") = NULL (errno=%d: %s)\n",
                filename, resolved, mode, errno, strerror(errno));
  } else {
    debugPrintf("fopen(\"%s\" -> \"%s\", \"%s\") = %p\n",
                filename, resolved, mode, f);
  }

  return f;
}

/* pthread wrappers: guest code passes inline bionic objects by address. */
int pthread_mutex_init_fake(pthread_mutex_t *uid, const int *mutexattr) {
  (void)mutexattr;
  return lookup_host_mutex(uid, 1) ? 0 : -1;
}

int pthread_mutex_destroy_fake(pthread_mutex_t *uid) {
  return destroy_host_mutex(uid);
}

int pthread_mutex_lock_fake(pthread_mutex_t *uid) {
  pthread_mutex_t *host = lookup_host_mutex(uid, 1);
  if (!host)
    return -1;
  int ret = pthread_mutex_lock(host);
  if (ret == 0) egl_shim_on_mutex_post_lock(uid);
  return ret;
}

int pthread_mutex_trylock_fake(pthread_mutex_t *uid) {
  pthread_mutex_t *host = lookup_host_mutex(uid, 1);
  if (!host)
    return -1;
  int ret = pthread_mutex_trylock(host);
  if (ret == 0) egl_shim_on_mutex_post_lock(uid);
  return ret;
}

int pthread_mutex_unlock_fake(pthread_mutex_t *uid) {
  pthread_mutex_t *host = lookup_host_mutex(uid, 1);
  if (!host)
    return -1;
  egl_shim_on_mutex_pre_unlock(uid);
  return pthread_mutex_unlock(host);
}

int pthread_cond_init_fake(pthread_cond_t *cnd, const int *condattr) {
  (void)condattr;
  return lookup_host_cond(cnd, 1) ? 0 : -1;
}

int pthread_cond_destroy_fake(pthread_cond_t *cnd) {
  return destroy_host_cond(cnd);
}

int pthread_cond_wait_fake(pthread_cond_t *cnd, pthread_mutex_t *mtx) {
  pthread_cond_t *host_cnd = lookup_host_cond(cnd, 1);
  pthread_mutex_t *host_mtx = lookup_host_mutex(mtx, 1);
  if (!host_cnd || !host_mtx)
    return -1;
  return pthread_cond_wait(host_cnd, host_mtx);
}

int pthread_cond_timedwait_fake(pthread_cond_t *cnd, pthread_mutex_t *mtx,
                                 const struct timespec *t) {
  pthread_cond_t *host_cnd = lookup_host_cond(cnd, 1);
  pthread_mutex_t *host_mtx = lookup_host_mutex(mtx, 1);
  if (!host_cnd || !host_mtx)
    return -1;
  return pthread_cond_timedwait(host_cnd, host_mtx, t);
}

int pthread_cond_signal_fake(pthread_cond_t *cnd) {
  pthread_cond_t *host_cnd = lookup_host_cond(cnd, 1);
  if (!host_cnd)
    return -1;
  return pthread_cond_signal(host_cnd);
}

int pthread_cond_broadcast_fake(pthread_cond_t *cnd) {
  pthread_cond_t *host_cnd = lookup_host_cond(cnd, 1);
  if (!host_cnd)
    return -1;
  return pthread_cond_broadcast(host_cnd);
}

typedef struct {
  void *(*entry)(void *);
  void *arg;
} ThreadWrapper;

static void *thread_wrapper_func(void *data) {
  ThreadWrapper *w = (ThreadWrapper *)data;
  void *(*entry)(void *) = w->entry;
  void *arg = w->arg;
  free(w);
  debugPrintf("[thread %lx] starting entry=%p arg=%p\n",
              (unsigned long)pthread_self(), (void *)entry, arg);
  void *ret = entry(arg);
  debugPrintf("[thread %lx] entry returned %p\n",
              (unsigned long)pthread_self(), ret);
  return ret;
}

int pthread_create_fake(pthread_t *thread, const void *attr, void *entry,
                         void *arg) {
  debugPrintf("pthread_create_fake(entry=%p, arg=%p)\n", entry, arg);
  ThreadWrapper *w = malloc(sizeof(ThreadWrapper));
  w->entry = entry;
  w->arg = arg;
  pthread_attr_t real_attr;
  pthread_attr_init(&real_attr);
  pthread_attr_setstacksize(&real_attr, 2 * 1024 * 1024); // 2MB stack
  int ret = pthread_create(thread, &real_attr, thread_wrapper_func, w);
  pthread_attr_destroy(&real_attr);
  if (ret != 0) free(w);
  return ret;
}

static void *pthread_getspecific_fake(pthread_key_t key) {
  (void)key;
  return pthread_getspecific(key);
}

static int pthread_setspecific_fake(pthread_key_t key, const void *value) {
  return pthread_setspecific(key, value);
}

int pthread_once_fake(volatile int *once_control, void (*init_routine)(void)) {
  return pthread_once((pthread_once_t *)once_control, init_routine);
}

/* GL logging wrappers — diagnose if game makes any GL calls after MakeCurrent */
typedef const GLubyte *(*PFNGLGETSTRINGIPROC)(GLenum name, GLuint index);

static const GLubyte *glGetString_wrap(GLenum name) {
  switch (name) {
  case 0x1f00: /* GL_VENDOR */
  case 0x1f01: /* GL_RENDERER */
  case 0x1f02: /* GL_VERSION */
  case 0x8b8c: /* GL_SHADING_LANGUAGE_VERSION */ {
    const GLubyte *s = glGetString(name);
    /* debugPrintf("GL: glGetString(0x%x) = \"%s\"\n", name,
                s ? (const char *)s : "(null)")); */
    if (s)
      return s;

    switch (name) {
    case 0x1f00:
      return (const GLubyte *)"Imagination Technologies";
    case 0x1f01:
      return (const GLubyte *)"PowerVR Rogue GE8300";
    case 0x1f02:
      return (const GLubyte *)"OpenGL ES 2.0";
    default:
      return (const GLubyte *)"OpenGL ES GLSL ES 1.00";
    }
  }
  case 0x1f03: { /* GL_EXTENSIONS */
    static const GLubyte fallback_ext[] =
        "GL_OES_depth_texture "
        "GL_OES_depth24 "
        "GL_OES_packed_depth_stencil "
        "GL_OES_element_index_uint "
        "GL_OES_texture_npot "
        "GL_OES_rgb8_rgba8 "
        "GL_OES_vertex_array_object "
        "GL_OES_mapbuffer "
        "GL_EXT_texture_format_BGRA8888 "
        "GL_IMG_texture_compression_pvrtc "
        "GL_OES_compressed_ETC1_RGB8_texture";
    static GLubyte *ext_cache = NULL;
    static size_t ext_cache_size = 0;

    const GLubyte *ext = glGetString(name);
    if (ext && ext[0] != '\0') {
      /* debugPrintf("GL: glGetString(GL_EXTENSIONS) -> driver string (%zu bytes)\n",
                  strlen((const char *)ext)); */
      return ext;
    }

    PFNGLGETSTRINGIPROC glGetStringiProc =
        (PFNGLGETSTRINGIPROC)SDL_GL_GetProcAddress("glGetStringi");
    if (glGetStringiProc) {
      GLint ext_count = 0;
      glGetIntegerv(0x821D, &ext_count); /* GL_NUM_EXTENSIONS */
      if (ext_count > 0) {
        size_t needed = 1;
        for (GLint i = 0; i < ext_count; i++) {
          const GLubyte *item = glGetStringiProc(name, (GLuint)i);
          if (item && item[0] != '\0')
            needed += strlen((const char *)item) + 1;
        }
        if (needed > 1) {
          GLubyte *buf = realloc(ext_cache, needed);
          if (buf) {
            ext_cache = buf;
            ext_cache_size = needed;
            size_t pos = 0;
            ext_cache[0] = '\0';
            for (GLint i = 0; i < ext_count; i++) {
              const GLubyte *item = glGetStringiProc(name, (GLuint)i);
              if (!item || item[0] == '\0')
                continue;
              size_t len = strlen((const char *)item);
              if (pos + len + 1 >= ext_cache_size)
                break;
              memcpy(ext_cache + pos, item, len);
              pos += len;
              ext_cache[pos++] = ' ';
            }
            if (pos > 0)
              pos--;
            ext_cache[pos] = '\0';
            debugPrintf(
                "GL: glGetString(GL_EXTENSIONS) -> rebuilt from glGetStringi (%d entries, %zu bytes)\n",
                ext_count, pos);
            return ext_cache;
          }
        }
      }
    }

    /* debugPrintf("GL: glGetString(GL_EXTENSIONS) -> fallback list (%zu bytes)\n",
                sizeof(fallback_ext) - 1); */
    return fallback_ext;
  }
  default: {
    const GLubyte *s = glGetString(name);
    /* debugPrintf("GL: glGetString(0x%x) = \"%s\"\n", name, s ? (const char *)s : "(null)"); */
    return s;
  }
  }
}

static void glGetIntegerv_wrap(GLenum pname, GLint *data) {
  glGetIntegerv(pname, data);
  debugPrintf("GL: glGetIntegerv(0x%x) = %d\n", pname, data ? *data : -1);
}

static void glFrontFace_wrap(GLenum mode) {
  egl_shim_ensure_current();
  /* debugPrintf("GL: glFrontFace(0x%x)\n", mode); */
  glFrontFace(mode);
}

static GLuint glCreateShader_wrap(GLenum type) {
  egl_shim_ensure_current();
  GLuint s = glCreateShader(type);
  /* debugPrintf("GL: glCreateShader(0x%x) = %u\n", type, s); */
  return s;
}

static GLuint glCreateProgram_wrap(void) {
  egl_shim_ensure_current();
  GLuint p = glCreateProgram();
  /* debugPrintf("GL: glCreateProgram() = %u\n", p); */
  return p;
}

static void glGenTextures_wrap(GLsizei n, GLuint *textures) {
  egl_shim_ensure_current();
  glGenTextures(n, textures);
  /* debugPrintf("GL: glGenTextures(%d) = %u\n", n, textures ? textures[0] : 0); */
}

static void glGenFramebuffers_wrap(GLsizei n, GLuint *framebuffers) {
  egl_shim_ensure_current();
  glGenFramebuffers(n, framebuffers);
  debugPrintf("GL: glGenFramebuffers(%d) = %u\n", n, framebuffers ? framebuffers[0] : 0);
}

static void glGenBuffers_wrap(GLsizei n, GLuint *buffers) {
  egl_shim_ensure_current();
  glGenBuffers(n, buffers);
  /* debugPrintf("GL: glGenBuffers(%d) = %u\n", n, buffers ? buffers[0] : 0); */
}

static void glBindFramebuffer_wrap(GLenum target, GLuint framebuffer) {
  egl_shim_ensure_current();
  /* debugPrintf("GL: glBindFramebuffer(0x%x, %u)\n", target, framebuffer); */
  glBindFramebuffer(target, framebuffer);
}

static void glShaderSource_wrap(GLuint shader, GLsizei count,
                                const GLchar *const *string,
                                const GLint *length) {
  egl_shim_ensure_current();
  glShaderSource(shader, count, string, length);
}

static void glCompileShader_wrap(GLuint shader) {
  egl_shim_ensure_current();
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLchar log[1024];
    GLsizei len = 0;
    glGetShaderInfoLog(shader, sizeof(log), &len, log);
    debugPrintf("GL: glCompileShader(%u) FAILED: %.*s\n", shader, (int)len, log);
  }
}

static void glAttachShader_wrap(GLuint program, GLuint shader) {
  egl_shim_ensure_current();
  glAttachShader(program, shader);
}

static void glLinkProgram_wrap(GLuint program) {
  egl_shim_ensure_current();
  glLinkProgram(program);
  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLchar log[1024];
    GLsizei len = 0;
    glGetProgramInfoLog(program, sizeof(log), &len, log);
    debugPrintf("GL: glLinkProgram(%u) FAILED: %.*s\n", program, (int)len, log);
  }
}

static void glBindBuffer_wrap(GLenum target, GLuint buffer) {
  egl_shim_ensure_current();
  glBindBuffer(target, buffer);
}

static void glBufferData_wrap(GLenum target, GLsizeiptr size, const void *data,
                              GLenum usage) {
  egl_shim_ensure_current();
  glBufferData(target, size, data, usage);
}

static void glBufferSubData_wrap(GLenum target, GLintptr offset, GLsizeiptr size,
                                 const void *data) {
  egl_shim_ensure_current();
  glBufferSubData(target, offset, size, data);
}

static void glBindTexture_wrap(GLenum target, GLuint texture) {
  egl_shim_ensure_current();
  glBindTexture(target, texture);
}

static void glTexImage2D_wrap(GLenum target, GLint level, GLint internalformat,
                              GLsizei width, GLsizei height, GLint border,
                              GLenum format, GLenum type, const void *pixels) {
  egl_shim_ensure_current();
  glTexImage2D(target, level, internalformat, width, height, border, format, type,
               pixels);
}

static void glTexParameteri_wrap(GLenum target, GLenum pname, GLint param) {
  egl_shim_ensure_current();
  glTexParameteri(target, pname, param);
}

static void glUseProgram_wrap(GLuint program) {
  egl_shim_ensure_current();
  static _Thread_local GLuint last_program = UINT32_MAX;
  if (program != last_program) {
    /* debugPrintf("GL: glUseProgram(%u)\n", program); */
  }
  last_program = program;
  glUseProgram(program);
}

static void glUniform1f_wrap(GLint location, GLfloat v0) {
  egl_shim_ensure_current();
  glUniform1f(location, v0);
}

static void glUniform2f_wrap(GLint location, GLfloat v0, GLfloat v1) {
  egl_shim_ensure_current();
  glUniform2f(location, v0, v1);
}

static void glUniform3f_wrap(GLint location, GLfloat v0, GLfloat v1,
                             GLfloat v2) {
  egl_shim_ensure_current();
  glUniform3f(location, v0, v1, v2);
}

static void glUniform4f_wrap(GLint location, GLfloat v0, GLfloat v1,
                             GLfloat v2, GLfloat v3) {
  egl_shim_ensure_current();
  glUniform4f(location, v0, v1, v2, v3);
}

static void glUniform1i_wrap(GLint location, GLint v0) {
  egl_shim_ensure_current();
  glUniform1i(location, v0);
}

void glUniform1fv_wrap(GLint location, GLsizei count,
                       const GLfloat *value) {
  egl_shim_ensure_current();
  glUniform1fv(location, count, value);
}

void glUniform2fv_wrap(GLint location, GLsizei count,
                       const GLfloat *value) {
  egl_shim_ensure_current();
  glUniform2fv(location, count, value);
}

void glUniform3fv_wrap(GLint location, GLsizei count,
                       const GLfloat *value) {
  egl_shim_ensure_current();
  glUniform3fv(location, count, value);
}

void glUniform4fv_wrap(GLint location, GLsizei count,
                       const GLfloat *value) {
  egl_shim_ensure_current();
  glUniform4fv(location, count, value);
}

static void glUniform1iv_wrap(GLint location, GLsizei count,
                              const GLint *value) {
  egl_shim_ensure_current();
  glUniform1iv(location, count, value);
}

static void glUniform2iv_wrap(GLint location, GLsizei count,
                              const GLint *value) {
  egl_shim_ensure_current();
  glUniform2iv(location, count, value);
}

static void glUniform3iv_wrap(GLint location, GLsizei count,
                              const GLint *value) {
  egl_shim_ensure_current();
  glUniform3iv(location, count, value);
}

static void glUniform4iv_wrap(GLint location, GLsizei count,
                              const GLint *value) {
  egl_shim_ensure_current();
  glUniform4iv(location, count, value);
}

static void glUniformMatrix2fv_wrap(GLint location, GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value) {
  egl_shim_ensure_current();
  glUniformMatrix2fv(location, count, transpose, value);
}

static void glUniformMatrix3fv_wrap(GLint location, GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value) {
  egl_shim_ensure_current();
  glUniformMatrix3fv(location, count, transpose, value);
}

static void glUniformMatrix4fv_wrap(GLint location, GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value) {
  egl_shim_ensure_current();
  glUniformMatrix4fv(location, count, transpose, value);
}

static GLenum glCheckFramebufferStatus_wrap(GLenum target) {
  egl_shim_ensure_current();
  GLenum status = glCheckFramebufferStatus(target);
  static _Thread_local int fb_log_count = 0;
  if (status != GL_FRAMEBUFFER_COMPLETE || fb_log_count < 12) {
    debugPrintf("GL: glCheckFramebufferStatus(0x%x) = 0x%x\n", target, status);
    fb_log_count++;
  }
  return status;
}

static void glClearColor_wrap(GLfloat red, GLfloat green, GLfloat blue,
                              GLfloat alpha) {
  egl_shim_ensure_current();
  static _Thread_local int clear_color_log_count = 0;
  if (clear_color_log_count < 12) {
    debugPrintf("GL: glClearColor(%.3f, %.3f, %.3f, %.3f)\n",
                red, green, blue, alpha);
    clear_color_log_count++;
  }
  glClearColor(red, green, blue, alpha);
}

static void glClear_wrap(GLbitfield mask) {
  egl_shim_ensure_current();
  /* debugPrintf("GL: glClear(0x%x)\n", mask); */
  glClear(mask);
}

static void glDrawArrays_wrap(GLenum mode, GLint first, GLsizei count) {
  egl_shim_ensure_current();
  static _Thread_local int draw_log_count = 0;
  if (draw_log_count < 40 || draw_log_count % 200 == 0) {
    //debugPrintf("GL: glDrawArrays(0x%x, first=%d, count=%d)\n",
    //            mode, first, count);
  }
  draw_log_count++;
  glDrawArrays(mode, first, count);
}

static void glDrawElements_wrap(GLenum mode, GLsizei count, GLenum type,
                                const void *indices) {
  egl_shim_ensure_current();
  static _Thread_local int draw_log_count = 0;
  if (draw_log_count < 40 || draw_log_count % 200 == 0) {
    //debugPrintf("GL: glDrawElements(0x%x, count=%d, type=0x%x, indices=%p)\n",
    //            mode, count, type, indices);
  }
  draw_log_count++;
  glDrawElements(mode, count, type, indices);
}

static void glViewport_wrap(GLint x, GLint y, GLsizei width, GLsizei height) {
  egl_shim_ensure_current();
  static _Thread_local int viewport_log_count = 0;
  if (viewport_log_count < 20) {
    /* debugPrintf("GL: glViewport(%d, %d, %d, %d)\n", x, y, width, height); */
    viewport_log_count++;
  }
  glViewport(x, y, width, height);
}

/* Import table */
DynLibFunction dynlib_functions[] = {
    /* Android stubs */
    {"__sF", (uintptr_t)&fake_sF},
    {"__errno", (uintptr_t)&__errno_fake},
    {"__stack_chk_fail", (uintptr_t)&__stack_chk_fail_stub},
    {"__stack_chk_guard", (uintptr_t)&__stack_chk_guard_fake},
    {"__cxa_atexit", (uintptr_t)&__cxa_atexit},
    {"__cxa_finalize", (uintptr_t)&__cxa_finalize},
    {"__android_log_print", (uintptr_t)&__android_log_print_fake},
    {"android_set_abort_message", (uintptr_t)&android_set_abort_message_fake},
    {"__ctype_get_mb_cur_max", (uintptr_t)&__ctype_get_mb_cur_max_fake},
    {"dl_iterate_phdr", (uintptr_t)&dl_iterate_phdr_fake},
    {"__system_property_get", (uintptr_t)&__system_property_get_fake},

    /* libdl */
    {"dlopen", (uintptr_t)&dlopen_fake},
    {"dlsym", (uintptr_t)&dlsym_fake},
    {"dlclose", (uintptr_t)&dlclose_fake},
    {"dlerror", (uintptr_t)&dlerror_fake},
    {"dladdr", (uintptr_t)&dladdr_fake},

    /* Fortified libc */
    {"__memcpy_chk", (uintptr_t)&__memcpy_chk},
    {"__memmove_chk", (uintptr_t)&__memmove_chk},
    {"__memset_chk", (uintptr_t)&__memset_chk},
    {"__strcat_chk", (uintptr_t)&__strcat_chk},
    {"__strcpy_chk", (uintptr_t)&__strcpy_chk},
    {"__strlen_chk", (uintptr_t)&__strlen_chk},
    {"__strrchr_chk", (uintptr_t)&__strrchr_chk},
    {"__vsprintf_chk", (uintptr_t)&__vsprintf_chk},
    {"__vsnprintf_chk", (uintptr_t)&__vsnprintf_chk},
    {"__read_chk", (uintptr_t)&__read_chk},
    {"__open_2", (uintptr_t)&__open_2},

    /* pthread (wrapped for bionic compat) */
    {"pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake},
    {"pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake},
    {"pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake},
    {"pthread_mutex_trylock", (uintptr_t)&pthread_mutex_trylock_fake},
    {"pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake},
    {"pthread_mutexattr_init", (uintptr_t)&ret0},
    {"pthread_mutexattr_settype", (uintptr_t)&ret0},
    {"pthread_mutexattr_destroy", (uintptr_t)&ret0},
    {"pthread_cond_init", (uintptr_t)&pthread_cond_init_fake},
    {"pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_fake},
    {"pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake},
    {"pthread_cond_timedwait", (uintptr_t)&pthread_cond_timedwait_fake},
    {"pthread_cond_signal", (uintptr_t)&pthread_cond_signal_fake},
    {"pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake},
    {"pthread_create", (uintptr_t)&pthread_create_fake},
    {"pthread_join", (uintptr_t)&pthread_join},
    {"pthread_self", (uintptr_t)&pthread_self},
    {"pthread_equal", (uintptr_t)&pthread_equal},
    {"pthread_detach", (uintptr_t)&pthread_detach},
    {"pthread_exit", (uintptr_t)&pthread_exit},
    {"pthread_once", (uintptr_t)&pthread_once_fake},
    {"pthread_attr_init", (uintptr_t)&ret0},
    {"pthread_attr_destroy", (uintptr_t)&ret0},
    {"pthread_attr_setdetachstate", (uintptr_t)&ret0},
    {"pthread_attr_setstacksize", (uintptr_t)&ret0},
    {"pthread_attr_setschedparam", (uintptr_t)&ret0},
    {"pthread_getschedparam", (uintptr_t)&ret0},
    {"pthread_key_create", (uintptr_t)&pthread_key_create},
    {"pthread_key_delete", (uintptr_t)&pthread_key_delete},
    {"pthread_getspecific", (uintptr_t)&pthread_getspecific_fake},
    {"pthread_setspecific", (uintptr_t)&pthread_setspecific_fake},
    {"sched_yield", (uintptr_t)&sched_yield},

    /* Memory */
    {"malloc", (uintptr_t)&malloc},
    {"calloc", (uintptr_t)&calloc},
    {"realloc", (uintptr_t)&realloc},
    {"free", (uintptr_t)&free},
    {"posix_memalign", (uintptr_t)&posix_memalign},

    /* String/memory */
    {"memcmp", (uintptr_t)&memcmp},
    {"memcpy", (uintptr_t)&memcpy},
    {"memmove", (uintptr_t)&memmove},
    {"memset", (uintptr_t)&memset},
    {"memchr", (uintptr_t)&memchr},
    {"strcmp", (uintptr_t)&strcmp},
    {"strncpy", (uintptr_t)&strncpy},
    {"strcat", (uintptr_t)&strcat},
    {"strchr", (uintptr_t)&strchr},
    {"strrchr", (uintptr_t)&strrchr},
    {"strlen", (uintptr_t)&strlen},
    {"strncmp", (uintptr_t)&strncmp},
    {"strerror_r", (uintptr_t)&strerror_r},
    {"strcoll", (uintptr_t)&strcoll},
    {"strxfrm", (uintptr_t)&strxfrm},
    {"strcpy", (uintptr_t)&strcpy},
    {"strtod", (uintptr_t)&strtod},
    {"strtof", (uintptr_t)&strtof},
    {"strtol", (uintptr_t)&strtol},
    {"strtoul", (uintptr_t)&strtoul},
    {"strtoll", (uintptr_t)&strtoll},
    {"strtoull", (uintptr_t)&strtoull},
    {"strtold", (uintptr_t)&strtold},
    {"strtold_l", (uintptr_t)&strtold},
    {"strtoll_l", (uintptr_t)&strtoll},
    {"strtoull_l", (uintptr_t)&strtoull},
    {"strftime", (uintptr_t)&strftime},

    /* ctype */
    {"islower", (uintptr_t)&islower},
    {"isupper", (uintptr_t)&isupper},
    {"isxdigit", (uintptr_t)&isxdigit},
    {"tolower", (uintptr_t)&tolower},
    {"toupper", (uintptr_t)&toupper},

    /* wctype / wchar */
    {"towlower", (uintptr_t)&towlower},
    {"towupper", (uintptr_t)&towupper},
    {"iswalpha", (uintptr_t)&iswalpha},
    {"iswblank", (uintptr_t)&iswblank},
    {"iswcntrl", (uintptr_t)&iswcntrl},
    {"iswdigit", (uintptr_t)&iswdigit},
    {"iswlower", (uintptr_t)&iswlower},
    {"iswprint", (uintptr_t)&iswprint},
    {"iswpunct", (uintptr_t)&iswpunct},
    {"iswspace", (uintptr_t)&iswspace},
    {"iswupper", (uintptr_t)&iswupper},
    {"iswxdigit", (uintptr_t)&iswxdigit},
    {"wctob", (uintptr_t)&wctob},
    {"btowc", (uintptr_t)&btowc},
    {"wcstol", (uintptr_t)&wcstol},
    {"wcstoul", (uintptr_t)&wcstoul},
    {"wcstoll", (uintptr_t)&wcstoll},
    {"wcstoull", (uintptr_t)&wcstoull},
    {"wcstod", (uintptr_t)&wcstod},
    {"wcstof", (uintptr_t)&wcstof},
    {"wcstold", (uintptr_t)&wcstold},
    {"wcslen", (uintptr_t)&wcslen},
    {"wcscoll", (uintptr_t)&wcscoll},
    {"wcsxfrm", (uintptr_t)&wcsxfrm},
    {"wmemcmp", (uintptr_t)&wmemcmp},
    {"wmemcpy", (uintptr_t)&wmemcpy},
    {"wmemmove", (uintptr_t)&wmemmove},
    {"wmemset", (uintptr_t)&wmemset},
    {"wmemchr", (uintptr_t)&wmemchr},
    {"mbrtowc", (uintptr_t)&mbrtowc},
    {"wcrtomb", (uintptr_t)&wcrtomb},
    {"mbrlen", (uintptr_t)&mbrlen},
    {"mbtowc", (uintptr_t)&mbtowc},
    {"mbsrtowcs", (uintptr_t)&mbsrtowcs},
    {"mbsnrtowcs", (uintptr_t)&mbsnrtowcs},
    {"wcsnrtombs", (uintptr_t)&wcsnrtombs},
    {"swprintf", (uintptr_t)&swprintf},

    /* stdio */
    {"printf", (uintptr_t)&printf},
    {"fprintf", (uintptr_t)&fprintf},
    {"snprintf", (uintptr_t)&snprintf},
    {"vfprintf", (uintptr_t)&vfprintf},
    {"vsprintf", (uintptr_t)&vsprintf},
    {"vsnprintf", (uintptr_t)&vsnprintf},
    {"vasprintf", (uintptr_t)&vasprintf},
    {"sscanf", (uintptr_t)&sscanf},
    {"vsscanf", (uintptr_t)&vsscanf},
    {"fopen", (uintptr_t)&fopen_fake},
    {"fclose", (uintptr_t)&fclose},
    {"fflush", (uintptr_t)&fflush},
    {"fread", (uintptr_t)&fread},
    {"fwrite", (uintptr_t)&fwrite},
    {"fputc", (uintptr_t)&fputc},
    {"fseek", (uintptr_t)&fseek},
    {"ftell", (uintptr_t)&ftell},
    {"getc", (uintptr_t)&getc},
    {"putc", (uintptr_t)&putc},
    {"putchar", (uintptr_t)&putchar},
    {"puts", (uintptr_t)&puts},
    {"ungetc", (uintptr_t)&ungetc},

    /* POSIX I/O */
    {"open", (uintptr_t)&open_fake},
    {"close", (uintptr_t)&close},
    {"read", (uintptr_t)&read},
    {"write", (uintptr_t)&write},
    {"mkdir", (uintptr_t)&mkdir_fake},
    {"chdir", (uintptr_t)&chdir},
    {"remove", (uintptr_t)&remove_fake},
    {"rename", (uintptr_t)&rename_fake},

    /* stdlib */
    {"abort", (uintptr_t)&abort_fake},
    {"exit", (uintptr_t)&exit_fake},
    {"getenv", (uintptr_t)&getenv_fake},
    {"setenv", (uintptr_t)&setenv_fake},
    {"qsort", (uintptr_t)&qsort},
    {"rand", (uintptr_t)&rand},
    {"srand", (uintptr_t)&srand},
    {"lrand48", (uintptr_t)&lrand48},
    {"srand48", (uintptr_t)&srand48},

    /* math */
    {"acosf", (uintptr_t)&acosf},
    {"asinf", (uintptr_t)&asinf},
    {"atanf", (uintptr_t)&atanf},
    {"atan2f", (uintptr_t)&atan2f},
    {"cosf", (uintptr_t)&cosf},
    {"sinf", (uintptr_t)&sinf},
    {"sin", (uintptr_t)&sin},
    {"sincos", (uintptr_t)&sincos},
    {"sincosf", (uintptr_t)&sincosf},
    {"exp", (uintptr_t)&exp},
    {"expf", (uintptr_t)&expf},
    {"log", (uintptr_t)&log},
    {"log10f", (uintptr_t)&log10f},
    {"logf", (uintptr_t)&logf},
    {"pow", (uintptr_t)&pow},
    {"powf", (uintptr_t)&powf},
    {"sqrtf", (uintptr_t)&sqrtf},
    {"ldexp", (uintptr_t)&ldexp},
    {"ldexpf", (uintptr_t)&ldexpf},

    /* time */
    {"clock_gettime", (uintptr_t)&clock_gettime},
    {"nanosleep", (uintptr_t)&nanosleep},

    /* locale */
    {"setlocale", (uintptr_t)&setlocale},
    {"localeconv", (uintptr_t)&localeconv},
    {"newlocale", (uintptr_t)&newlocale},
    {"uselocale", (uintptr_t)&uselocale},
    {"freelocale", (uintptr_t)&freelocale},

    /* syslog */
    {"openlog", (uintptr_t)&openlog},
    {"closelog", (uintptr_t)&closelog},
    {"syslog", (uintptr_t)&syslog},

    /* signals */
    {"raise", (uintptr_t)&ret0},
    {"sigaction", (uintptr_t)&sigaction_fake},

    /* syscall */
    {"syscall", (uintptr_t)&syscall},
    {"sysconf", (uintptr_t)&sysconf},

    /* EGL (our shim) */
    {"eglGetDisplay", (uintptr_t)&egl_shim_GetDisplay},
    {"eglInitialize", (uintptr_t)&egl_shim_Initialize},
    {"eglChooseConfig", (uintptr_t)&egl_shim_ChooseConfig},
    {"eglCreateWindowSurface", (uintptr_t)&egl_shim_CreateWindowSurface},
    {"eglCreatePbufferSurface", (uintptr_t)&egl_shim_CreatePbufferSurface},
    {"eglCreateContext", (uintptr_t)&egl_shim_CreateContext},
    {"eglMakeCurrent", (uintptr_t)&egl_shim_MakeCurrent},
    {"eglSwapBuffers", (uintptr_t)&egl_shim_SwapBuffers},
    {"eglDestroySurface", (uintptr_t)&egl_shim_DestroySurface},
    {"eglDestroyContext", (uintptr_t)&egl_shim_DestroyContext},
    {"eglTerminate", (uintptr_t)&egl_shim_Terminate},
    {"eglQuerySurface", (uintptr_t)&egl_shim_QuerySurface},
    {"eglGetConfigAttrib", (uintptr_t)&egl_shim_GetConfigAttrib},
    {"eglGetError", (uintptr_t)&egl_shim_GetError},
    {"eglGetProcAddress", (uintptr_t)&egl_shim_GetProcAddress},
    {"eglBindAPI", (uintptr_t)&egl_shim_BindAPI},

    /* OpenGL ES 2.0 (direct passthrough) */
    {"glActiveTexture", (uintptr_t)&glActiveTexture},
    {"glAttachShader", (uintptr_t)&glAttachShader_wrap},
    {"glBindAttribLocation", (uintptr_t)&glBindAttribLocation},
    {"glBindBuffer", (uintptr_t)&glBindBuffer_wrap},
    {"glBindFramebuffer", (uintptr_t)&glBindFramebuffer_wrap},
    {"glBindTexture", (uintptr_t)&glBindTexture_wrap},
    {"glBlendEquationSeparate", (uintptr_t)&glBlendEquationSeparate},
    {"glBlendFuncSeparate", (uintptr_t)&glBlendFuncSeparate},
    {"glBufferData", (uintptr_t)&glBufferData_wrap},
    {"glBufferSubData", (uintptr_t)&glBufferSubData_wrap},
    {"glCheckFramebufferStatus", (uintptr_t)&glCheckFramebufferStatus_wrap},
    {"glClear", (uintptr_t)&glClear_wrap},
    {"glClearColor", (uintptr_t)&glClearColor_wrap},
    {"glCompileShader", (uintptr_t)&glCompileShader_wrap},
    {"glCompressedTexImage2D", (uintptr_t)&glCompressedTexImage2D},
    {"glCopyTexImage2D", (uintptr_t)&glCopyTexImage2D},
    {"glCreateProgram", (uintptr_t)&glCreateProgram_wrap},
    {"glCreateShader", (uintptr_t)&glCreateShader_wrap},
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
    {"glDrawArrays", (uintptr_t)&glDrawArrays_wrap},
    {"glDrawElements", (uintptr_t)&glDrawElements_wrap},
    {"glEnable", (uintptr_t)&glEnable},
    {"glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray},
    {"glFinish", (uintptr_t)&glFinish},
    {"glFlush", (uintptr_t)&glFlush},
    {"glFrontFace", (uintptr_t)&glFrontFace_wrap},
    {"glGenBuffers", (uintptr_t)&glGenBuffers_wrap},
    {"glGenFramebuffers", (uintptr_t)&glGenFramebuffers_wrap},
    {"glGenTextures", (uintptr_t)&glGenTextures_wrap},
    {"glGenerateMipmap", (uintptr_t)&glGenerateMipmap},
    {"glGetActiveAttrib", (uintptr_t)&glGetActiveAttrib},
    {"glGetActiveUniform", (uintptr_t)&glGetActiveUniform},
    {"glGetAttachedShaders", (uintptr_t)&glGetAttachedShaders},
    {"glGetAttribLocation", (uintptr_t)&glGetAttribLocation},
    {"glGetError", (uintptr_t)&glGetError},
    {"glGetIntegerv", (uintptr_t)&glGetIntegerv_wrap},
    {"glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog},
    {"glGetProgramiv", (uintptr_t)&glGetProgramiv},
    {"glGetShaderSource", (uintptr_t)&glGetShaderSource},
    {"glGetShaderiv", (uintptr_t)&glGetShaderiv},
    {"glGetString", (uintptr_t)&glGetString_wrap},
    {"glGetUniformLocation", (uintptr_t)&glGetUniformLocation},
    {"glGetVertexAttribPointerv", (uintptr_t)&glGetVertexAttribPointerv},
    {"glGetVertexAttribiv", (uintptr_t)&glGetVertexAttribiv},
    {"glLinkProgram", (uintptr_t)&glLinkProgram_wrap},
    {"glReleaseShaderCompiler", (uintptr_t)&glReleaseShaderCompiler},
    {"glShaderSource", (uintptr_t)&glShaderSource_wrap},
    {"glTexImage2D", (uintptr_t)&glTexImage2D_wrap},
    {"glTexParameteri", (uintptr_t)&glTexParameteri_wrap},
    {"glUniform1f", (uintptr_t)&glUniform1f_wrap},
    {"glUniform1fv", (uintptr_t)&glUniform1fv_wrap},
    {"glUniform1i", (uintptr_t)&glUniform1i_wrap},
    {"glUniform1iv", (uintptr_t)&glUniform1iv_wrap},
    {"glUniform2f", (uintptr_t)&glUniform2f_wrap},
    {"glUniform2fv", (uintptr_t)&glUniform2fv_wrap},
    {"glUniform2iv", (uintptr_t)&glUniform2iv_wrap},
    {"glUniform3f", (uintptr_t)&glUniform3f_wrap},
    {"glUniform3fv", (uintptr_t)&glUniform3fv_wrap},
    {"glUniform3iv", (uintptr_t)&glUniform3iv_wrap},
    {"glUniform4f", (uintptr_t)&glUniform4f_wrap},
    {"glUniform4fv", (uintptr_t)&glUniform4fv_wrap},
    {"glUniform4iv", (uintptr_t)&glUniform4iv_wrap},
    {"glUniformMatrix2fv", (uintptr_t)&glUniformMatrix2fv_wrap},
    {"glUniformMatrix3fv", (uintptr_t)&glUniformMatrix3fv_wrap},
    {"glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv_wrap},
    {"glUseProgram", (uintptr_t)&glUseProgram_wrap},
    {"glValidateProgram", (uintptr_t)&ret0},
    {"glVertexAttribPointer", (uintptr_t)&glVertexAttribPointer},
    {"glViewport", (uintptr_t)&glViewport_wrap},

    /* OpenSL ES (our shim) */
    {"slCreateEngine", (uintptr_t)&slCreateEngine_shim},
    {"SL_IID_ENGINE", (uintptr_t)&sl_IID_ENGINE},
    {"SL_IID_PLAY", (uintptr_t)&sl_IID_PLAY},
    {"SL_IID_VOLUME", (uintptr_t)&sl_IID_VOLUME},
    {"SL_IID_ANDROIDSIMPLEBUFFERQUEUE", (uintptr_t)&sl_IID_BUFFERQUEUE},
    {"SL_IID_ENGINECAPABILITIES", (uintptr_t)&sl_IID_ENGINECAPABILITIES},
    {"SL_IID_ENVIRONMENTALREVERB", (uintptr_t)&sl_IID_ENVIRONMENTALREVERB},

    /* Android NDK (our shim) */
    {"AAssetManager_fromJava", (uintptr_t)&ret1},
    {"AAssetManager_open", (uintptr_t)&AAssetManager_open_fake},
    {"AAsset_close", (uintptr_t)&AAsset_close_fake},
    {"AAsset_read", (uintptr_t)&AAsset_read_fake},
    {"AAsset_getLength", (uintptr_t)&AAsset_getLength_fake},
    {"AAsset_seek", (uintptr_t)&AAsset_seek_fake},
    {"ANativeWindow_fromSurface", (uintptr_t)&ANativeWindow_fromSurface_fake},
    {"ANativeWindow_getWidth", (uintptr_t)&ANativeWindow_getWidth_fake},
    {"ANativeWindow_getHeight", (uintptr_t)&ANativeWindow_getHeight_fake},
    {"ANativeWindow_setBuffersGeometry", (uintptr_t)&ANativeWindow_setBuffersGeometry_fake},
};

size_t dynlib_numfunctions =
    sizeof(dynlib_functions) / sizeof(*dynlib_functions);
