/*
 * main.c — Cuphead (Unity 2017.4.40f1 IL2CPP) so-loader → NextOS/Mali-450 (arm64, GLES2).
 *
 * Receita Unity baseada no port re4 (Unity 2018 Mono), adaptada p/ arm64 + IL2CPP:
 *   - dlopen libz/libGLESv2/libEGL RTLD_GLOBAL (Unity resolve via dlsym RTLD_DEFAULT)
 *   - so_load libunity.so (engine) -> imports overrides -> init_array
 *   - so_load libil2cpp.so (lógica do jogo C#) + global-metadata.dat   [fase seguinte]
 *   - JNI_OnLoad -> janela GLES2 -> lifecycle (initJni/nativeRender)    [fase seguinte]
 * Alvo GLES2: passar -force-gles20 ao Unity (args via initJni/command line).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include "so_util.h"
#include "imports.h"
#include "jni_shim.h"
#include "util.h"

#define HEAP_MB 96

/* ---------- crash handler (arm64) ---------- */
static void on_crash(int sig, siginfo_t *si, void *uc_) {
  ucontext_t *uc = (ucontext_t *)uc_;
  uintptr_t pc = uc->uc_mcontext.pc, lr = uc->uc_mcontext.regs[30];
  uintptr_t tb = (uintptr_t)text_base;
  fprintf(stderr, "\n=== CRASH sig=%d fault=%p pc=0x%lx", sig, si->si_addr,
          (unsigned long)pc);
  if (pc >= tb && pc < tb + text_size) fprintf(stderr, " (libunity+0x%lx)", pc - tb);
  fprintf(stderr, " lr=0x%lx", (unsigned long)lr);
  if (lr >= tb && lr < tb + text_size) fprintf(stderr, " (lr unity+0x%lx)", lr - tb);
  fprintf(stderr, " ===\n");
  for (int i = 0; i < 31; i++) {
    fprintf(stderr, " x%-2d=0x%016lx", i, (unsigned long)uc->uc_mcontext.regs[i]);
    if (i % 3 == 2) fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n[stack scan]\n");
  uintptr_t sp = uc->uc_mcontext.sp;
  for (int k = 0, hits = 0; k < 400 && hits < 24; k++) {
    uintptr_t v = *(uintptr_t *)(sp + k * 8);
    if (v >= tb && v < tb + text_size) { fprintf(stderr, "  libunity+0x%lx\n", v - tb); hits++; }
  }
  _exit(128 + sig);
}

/* ---------- overrides bionic->glibc (do re4) ---------- */
/* sysconf: Unity lê _SC_* com constantes BIONIC (≠ glibc) → page/nproc/phys errados. */
static long my_sysconf(int name) {
  switch (name) {
    case 39: case 40: return 4096;                 /* _SC_PAGE_SIZE/_SC_PAGESIZE bionic */
    case 6: return 100;                            /* _SC_CLK_TCK */
    case 96: case 97: return 4;                    /* _SC_NPROCESSORS_CONF/_ONLN -> 4 cores */
    case 98: return (512L*1024*1024)/4096;         /* _SC_PHYS_PAGES -> 512MB */
    case 99: return (256L*1024*1024)/4096;         /* _SC_AVPHYS_PAGES -> 256MB */
  }
  long r = sysconf(name);
  if ((name == _SC_PHYS_PAGES || name == _SC_AVPHYS_PAGES) && r <= 0)
    r = (512L*1024*1024)/4096;
  return r;
}
/* /proc/cpuinfo + /sys/.../cpu: Unity conta cores p/ dimensionar job workers. */
static FILE *my_fopen(const char *p, const char *m) {
  if (p && !strcmp(p, "/proc/meminfo")) {
    FILE *t = tmpfile(); if (t) { fputs("MemTotal:      524288 kB\nMemFree:       262144 kB\nMemAvailable:  262144 kB\n", t); rewind(t); return t; }
  }
  if (p && (!strcmp(p, "/sys/devices/system/cpu/possible") || !strcmp(p, "/sys/devices/system/cpu/present") || !strcmp(p, "/sys/devices/system/cpu/online"))) {
    FILE *t = tmpfile(); if (t) { fputs("0-3\n", t); rewind(t); return t; }
  }
  return fopen(p, m);
}
static int my_open(const char *p, int fl, ...) {
  if (p && !strcmp(p, "/proc/cpuinfo")) {
    FILE *t = tmpfile();
    if (t) { for (int i = 0; i < 4; i++) fprintf(t, "processor\t: %d\nCPU implementer\t: 0x41\nCPU architecture: 8\n\n", i);
      fflush(t); int fd = dup(fileno(t)); fclose(t); lseek(fd, 0, SEEK_SET); return fd; }
  }
  va_list ap; va_start(ap, fl); int mo = va_arg(ap, int); va_end(ap);
  return open(p, fl, mo);
}
/* __system_property_get: zera value (Unity lê como string null-terminated) */
static int my_sysprop(const char *name, char *value) { (void)name; if (value) value[0] = 0; return 0; }
/* __android_log -> stderr */
static int my_alog_print(int prio, const char *tag, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap); return 0;
}
static int my_alog_write(int prio, const char *tag, const char *msg) {
  fprintf(stderr, "[ALOG:%d %s] %s\n", prio, tag ? tag : "?", msg ? msg : ""); return 0;
}
/* ANativeWindow: Unity espera window !=NULL (nativeRecreateGfxState) senão trava p/ sempre. */
static int g_anw = 0xA11;
static void *my_aw_fromSurface(void *e, void *s) { (void)e; (void)s; return &g_anw; }
static int my_aw_setgeom(void *w, int a, int b, int c) { (void)w; (void)a; (void)b; (void)c; return 0; }
static int my_aw_getWidth(void *w) { (void)w; return 1280; }
static int my_aw_getHeight(void *w) { (void)w; return 720; }
static int my_aw_getFormat(void *w) { (void)w; return 1; }
static void my_aw_noop(void *w) { (void)w; }
/* dlopen/dlsym: Unity dlopen libGLESv2/EGL/OpenSLES + dlsym em runtime */
static char g_dl_self;
static void *my_dlopen(const char *nm, int flag) {
  if (!nm || !nm[0] || strstr(nm, "libc") || strstr(nm, "libunity") || strstr(nm, "libmain") || strstr(nm, "libil2cpp"))
    return &g_dl_self;
  void *h = dlopen(nm, flag); return h ? h : &g_dl_self;
}
static void *my_dlsym(void *h, const char *nm) {
  if (!nm) return NULL;
  if (h == &g_dl_self) { void *p = (void *)so_find_addr_safe(nm); if (!p) p = dlsym(RTLD_DEFAULT, nm); return p; }
  return dlsym(h, nm);
}
static const char *my_dlerror(void) { return NULL; }
static int my_dladdr(const void *a, void *i) { (void)a; (void)i; return 0; }
static int my_dlclose(void *h) { (void)h; return 0; }

/* ---------- TLS bridge (bionic keys -> slots nossos; 1 glibc key) ---------- */
#define NSLOT 1024
static pthread_key_t g_tls_base; static int g_tls_init = 0;
static int g_slot_next = 1; static pthread_mutex_t g_slot_mtx = PTHREAD_MUTEX_INITIALIZER;
static void tls_dtor(void *p) { free(p); }
static void **tls_slots(void) {
  if (!g_tls_init) { pthread_key_create(&g_tls_base, tls_dtor); g_tls_init = 1; }
  void **s = (void **)pthread_getspecific(g_tls_base);
  if (!s) { s = (void **)calloc(NSLOT, sizeof(void *)); pthread_setspecific(g_tls_base, s); }
  return s;
}
static int sh_key_create(unsigned *k, void (*d)(void *)) { (void)d; pthread_mutex_lock(&g_slot_mtx);
  int n = g_slot_next++; pthread_mutex_unlock(&g_slot_mtx); if (n >= NSLOT) return 11; *k = (unsigned)n; return 0; }
static int sh_key_delete(unsigned k) { (void)k; return 0; }
static void *sh_getspecific(unsigned k) { if ((int)k <= 0 || (int)k >= NSLOT) return NULL; return tls_slots()[(int)k]; }
static int sh_setspecific(unsigned k, const void *v) { if ((int)k <= 0 || (int)k >= NSLOT) return 22; tls_slots()[(int)k] = (void *)v; return 0; }

/* ---------- helper: override import na tabela ---------- */
static void set_import(const char *name, void *fn) {
  for (size_t i = 0; i < dynlib_numfunctions; i++)
    if (!strcmp(dynlib_functions[i].symbol, name)) { dynlib_functions[i].func = (uintptr_t)fn; return; }
}

extern int my_sigaction();  /* bionic_shims.c (ABI sigset bionic/glibc) */

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  setvbuf(stdout, NULL, _IONBF, 0); setvbuf(stderr, NULL, _IONBF, 0);
  struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_sigaction = on_crash; sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, 0); sigaction(SIGBUS, &sa, 0); sigaction(SIGABRT, &sa, 0);
  sigaction(SIGILL, &sa, 0); sigaction(SIGFPE, &sa, 0);

  fprintf(stderr, "=== Cuphead Unity 2017.4 IL2CPP (arm64 GLES2) so-loader ===\n");

  /* GL/EGL/z visíveis p/ dlsym(RTLD_DEFAULT) do Unity */
  dlopen("libz.so.1", RTLD_NOW | RTLD_GLOBAL);
  void *g = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_GLOBAL); if (!g) dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
  void *e = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL); if (!e) dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
  fprintf(stderr, "[libs] z/GLESv2/EGL dlopen (glClear=%p)\n", dlsym(RTLD_DEFAULT, "glClear"));

  /* ---- F0: carrega libunity.so ---- */
  size_t hs = (size_t)HEAP_MB * 1024 * 1024;
  void *heap = mmap(NULL, hs, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap == MAP_FAILED) { perror("mmap"); return 1; }
  fprintf(stderr, "[F0] heap %dMB @ %p, carregando libunity.so...\n", HEAP_MB, heap);
  if (so_load("libunity.so", heap, hs) < 0) { fprintf(stderr, "so_load libunity FALHOU\n"); return 1; }
  fprintf(stderr, "[F0] libunity: text=%p+%zu data=%p+%zu\n", text_base, text_size, data_base, data_size);
  if (so_relocate() < 0) { fprintf(stderr, "relocate FALHOU\n"); return 1; }

  /* overrides */
  set_import("sysconf", (void *)my_sysconf);
  set_import("fopen", (void *)my_fopen);
  set_import("open", (void *)my_open);
  set_import("__system_property_get", (void *)my_sysprop);
  set_import("__android_log_print", (void *)my_alog_print);
  set_import("__android_log_write", (void *)my_alog_write);
  set_import("sigaction", (void *)my_sigaction);
  set_import("dlopen", (void *)my_dlopen);
  set_import("dlsym", (void *)my_dlsym);
  set_import("dlerror", (void *)my_dlerror);
  set_import("dladdr", (void *)my_dladdr);
  set_import("dlclose", (void *)my_dlclose);
  set_import("pthread_key_create", (void *)sh_key_create);
  set_import("pthread_key_delete", (void *)sh_key_delete);
  set_import("pthread_getspecific", (void *)sh_getspecific);
  set_import("pthread_setspecific", (void *)sh_setspecific);
  set_import("ANativeWindow_fromSurface", (void *)my_aw_fromSurface);
  set_import("ANativeWindow_setBuffersGeometry", (void *)my_aw_setgeom);
  set_import("ANativeWindow_getWidth", (void *)my_aw_getWidth);
  set_import("ANativeWindow_getHeight", (void *)my_aw_getHeight);
  set_import("ANativeWindow_getFormat", (void *)my_aw_getFormat);
  set_import("ANativeWindow_acquire", (void *)my_aw_noop);
  set_import("ANativeWindow_release", (void *)my_aw_noop);

  fprintf(stderr, "[F0] resolvendo %zu imports...\n", dynlib_numfunctions);
  if (so_resolve(dynlib_functions, dynlib_numfunctions, 0) < 0) { fprintf(stderr, "resolve FALHOU\n"); return 1; }
  so_finalize(); so_flush_caches();
  fprintf(stderr, "[F0] init_array...\n");
  so_execute_init_array();
  fprintf(stderr, "[F0] libunity init OK\n");

  /* ---- JNI_OnLoad da libunity ---- */
  void *vm = NULL, *env = NULL; jni_shim_init(&vm, &env);
  uintptr_t onload = so_find_addr_safe("JNI_OnLoad");
  if (onload) {
    int ver = ((int (*)(void *, void *))onload)(vm, NULL);
    fprintf(stderr, "[F0] JNI_OnLoad = 0x%x\n", ver);
  } else {
    fprintf(stderr, "[F0] JNI_OnLoad não encontrado em libunity\n");
  }

  fprintf(stderr, "[F0] === fim F0 (libunity carregada). Próximo: libil2cpp + lifecycle ===\n");
  fflush(stderr);
  _exit(0);  /* hard exit — destrutores do .so crasham no teardown normal */
}
