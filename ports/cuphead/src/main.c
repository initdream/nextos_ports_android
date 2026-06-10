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
#include <sys/stat.h>
#include <sys/syscall.h>
#include <SDL2/SDL.h>

#include "so_util.h"
#include "imports.h"
#include "jni_shim.h"
#include "opensles_shim.h"
#include "util.h"

#define HEAP_MB 96

/* canary bionic: libunity lê o stack-guard de tpidr_el0+0x28 (TLS_SLOT_STACK_GUARD
 * do bionic); sob glibc esse offset cai no TLS de outra lib e MUDA em runtime →
 * __stack_chk_fail espúrio (e o "SEGV após neutralizar" era o no-op retornando em
 * código adjacente — noreturn). Pad TLS no exe (1º bloco após o TCB de 16B) cobre
 * offset 16..272 e NUNCA é escrito → slot estável. (causa-raiz achada no Dysmantle) */
/* = {1} → .tdata: fica ANTES das TLS .tbss do egl_shim (link order) no template,
 * senão o pad desliza p/ +0x30 e o slot +0x28 cai fora (visto no device). */
__attribute__((aligned(16))) static _Thread_local char g_bionic_guard_pad[256] = {1};

/* fsync(stderr→debug.log): garante que o log sobrevive a hang/power-cycle */
static void dbg_sync(void) { fsync(2); }

/* sem_shim: semáforos próprios (bionic sem_t 4B vs glibc 32B) — ver sem_shim.c.
   CAUSA-RAIZ do deadlock no boot: sem_post do glibc não acordava o sem_wait da
   thread pool do Unity. CUP_NOSEMSHIM=1 desliga (volta ao glibc cru). */
extern int sh_sem_init(void *, int, unsigned);
extern int sh_sem_wait(void *);
extern int sh_sem_trywait(void *);
extern int sh_sem_timedwait(void *, const struct timespec *);
extern int sh_sem_post(void *);
extern int sh_sem_getvalue(void *, int *);
extern int sh_sem_destroy(void *);
extern int g_main_tid;
extern void sh_tick_preload(void);
extern void sh_sem_set_poll(int ms);
static void set_import(const char *name, void *fn);
static int patch_got(const char *name, void *fn);
static void install_sem_shim(void) {
  if (getenv("CUP_NOSEMSHIM")) return;
  set_import("sem_init", (void *)sh_sem_init);
  set_import("sem_wait", (void *)sh_sem_wait);
  set_import("sem_trywait", (void *)sh_sem_trywait);
  set_import("sem_timedwait", (void *)sh_sem_timedwait);
  set_import("sem_post", (void *)sh_sem_post);
  set_import("sem_getvalue", (void *)sh_sem_getvalue);
  set_import("sem_destroy", (void *)sh_sem_destroy);
}
static void patch_sem_shim(void) {
  if (getenv("CUP_NOSEMSHIM")) return;
  patch_got("sem_init", (void *)sh_sem_init);
  patch_got("sem_wait", (void *)sh_sem_wait);
  patch_got("sem_trywait", (void *)sh_sem_trywait);
  patch_got("sem_timedwait", (void *)sh_sem_timedwait);
  patch_got("sem_post", (void *)sh_sem_post);
  patch_got("sem_getvalue", (void *)sh_sem_getvalue);
  patch_got("sem_destroy", (void *)sh_sem_destroy);
}

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
  /* qual lib contém o pc? */
  FILE *mp = fopen("/proc/self/maps", "r"); char ml[400];
  while (mp && fgets(ml, sizeof ml, mp)) { unsigned long a, b;
    if (sscanf(ml, "%lx-%lx", &a, &b) == 2 && pc >= a && pc < b) { fprintf(stderr, "[PC-LIB] %s", ml); break; } }
  if (mp) fclose(mp);
  fprintf(stderr, "[stack scan]\n");
  uintptr_t sp = uc->uc_mcontext.sp;
  for (int k = 0, hits = 0; k < 400 && hits < 24; k++) {
    uintptr_t v = *(uintptr_t *)(sp + k * 8);
    if (v >= tb && v < tb + text_size) { fprintf(stderr, "  libunity+0x%lx\n", v - tb); hits++; }
  }
  dbg_sync();
  _exit(128 + sig);
}

/* ---------- overrides bionic->glibc (do re4) ---------- */
/* sysconf: Unity lê _SC_* com constantes BIONIC (≠ glibc) → page/nproc/phys errados. */
static long my_sysconf(int name) {
  int ncpu = getenv("CUP_1CORE") ? 1 : 4;
  switch (name) {
    case 39: case 40: return 4096;                 /* _SC_PAGE_SIZE/_SC_PAGESIZE bionic */
    case 6: return 100;                            /* _SC_CLK_TCK */
    case 96: case 97: return ncpu;                 /* _SC_NPROCESSORS_CONF/_ONLN (1 core => Unity desliga MT rendering) */
    case 98: return (512L*1024*1024)/4096;         /* _SC_PHYS_PAGES -> 512MB */
    case 99: return (256L*1024*1024)/4096;         /* _SC_AVPHYS_PAGES -> 256MB */
  }
  long r = sysconf(name);
  if ((name == _SC_PHYS_PAGES || name == _SC_AVPHYS_PAGES) && r <= 0)
    r = (512L*1024*1024)/4096;
  return r;
}
/* /proc/cpuinfo + /sys/.../cpu: Unity conta cores p/ dimensionar job workers. */
static int g_dllog;
static const char *asset_redirect(const char *p, char *buf, size_t bufsz);
static FILE *my_fopen(const char *p, const char *m) {
  if (p && !strcmp(p, "/proc/meminfo")) {
    FILE *t = tmpfile(); if (t) { fputs("MemTotal:      524288 kB\nMemFree:       262144 kB\nMemAvailable:  262144 kB\n", t); rewind(t); return t; }
  }
  if (p && (!strcmp(p, "/sys/devices/system/cpu/possible") || !strcmp(p, "/sys/devices/system/cpu/present") || !strcmp(p, "/sys/devices/system/cpu/online"))) {
    FILE *t = tmpfile(); if (t) { fputs(getenv("CUP_1CORE") ? "0\n" : "0-3\n", t); rewind(t); return t; }
  }
  char rb[512]; const char *r = asset_redirect(p, rb, sizeof rb);
  if (r) {
    if (g_dllog) fprintf(stderr, "[fopen-redir] %s -> %s\n", p, r);
    return fopen(r, m);
  }
  return fopen(p, m);
}
#define ASSET_BASE_M "/storage/roms/cuphead-recon/"
/* redirect genérico de assets: o engine monta paths de dados com bases erradas
   (APK inexistente, filesdir). Mapeia qualquer tentativa p/ os arquivos REAIS
   deployados em bin/Data (mesma receita do global-metadata.dat, generalizada:
   pega o sufixo após "bin/Data/", senão o basename de arquivos conhecidos do
   engine — globalgamemanagers, level*, sharedassets*, *.assets/.resS/.resource). */
static const char *asset_redirect(const char *p, char *buf, size_t bufsz) {
  /* anti-loop: só pula o que JÁ aponta pro alvo (bin/Data real); paths de
     userdata/ sob a base ainda precisam de redirect (il2cpp/Metadata) */
  if (!p || !strncmp(p, ASSET_BASE_M "bin/Data/", sizeof(ASSET_BASE_M "bin/Data/") - 1)) return NULL;
  const char *sub = strstr(p, "bin/Data/");
  if (sub) {
    snprintf(buf, bufsz, ASSET_BASE_M "bin/Data/%s", sub + 9);
    if (access(buf, F_OK) == 0) return buf;
  }
  const char *base = strrchr(p, '/'); base = base ? base + 1 : p;
  if (!strcmp(base, "global-metadata.dat")) {
    snprintf(buf, bufsz, ASSET_BASE_M "bin/Data/Managed/Metadata/global-metadata.dat");
    return buf;
  }
  /* il2cpp procura <userdata>/il2cpp/Resources/*-resources.dat */
  if (strstr(base, "-resources.dat")) {
    snprintf(buf, bufsz, ASSET_BASE_M "bin/Data/Managed/Resources/%s", base);
    if (access(buf, F_OK) == 0) return buf;
  }
  if (!strncmp(base, "level", 5) || !strncmp(base, "sharedassets", 12) ||
      !strncmp(base, "globalgamemanagers", 18) || strstr(base, ".assets") ||
      strstr(base, ".resS") || strstr(base, ".resource") ||
      !strcmp(base, "data.unity3d") || !strcmp(base, "boot.config") ||
      !strcmp(base, "unity default resources") || !strcmp(base, "unity_builtin_extra")) {
    snprintf(buf, bufsz, ASSET_BASE_M "bin/Data/%s", base);
    if (access(buf, F_OK) == 0) return buf;
    snprintf(buf, bufsz, ASSET_BASE_M "bin/Data/Resources/%s", base);
    if (access(buf, F_OK) == 0) return buf;
  }
  return NULL;
}
/* command line do Unity: lido de /proc/<pid>/cmdline (args separados por \0).
   Injeta -force-gfx-st (single-threaded GFX) p/ matar o GfxDeviceWorker e o
   deadlock main<->worker no boot. CUP_GFXARGS sobrescreve. */
static int cmdline_fd(void) {
  const char *extra = getenv("CUP_GFXARGS");
  char buf[256]; int n = 0;
  n += sprintf(buf + n, "cuphead") + 1;
  if (extra && *extra) {
    /* CUP_GFXARGS="-a -b" -> cada token \0-terminado */
    char tmp[200]; strncpy(tmp, extra, sizeof tmp - 1); tmp[sizeof tmp - 1] = 0;
    for (char *t = strtok(tmp, " "); t; t = strtok(NULL, " ")) n += sprintf(buf + n, "%s", t) + 1;
  } else {
    n += sprintf(buf + n, "-force-gfx-st") + 1;
    n += sprintf(buf + n, "-force-gles20") + 1;
  }
  FILE *t = tmpfile();
  if (!t) return -1;
  fwrite(buf, 1, n, t); fflush(t);
  int fd = dup(fileno(t)); fclose(t); lseek(fd, 0, SEEK_SET);
  fprintf(stderr, "[CMDLINE] injetado (%d bytes): force-gfx-st\n", n);
  return fd;
}
static int my_open(const char *p, int fl, ...) {
  if (p && !strcmp(p, "/proc/cpuinfo")) {
    int nc = getenv("CUP_1CORE") ? 1 : 4;
    FILE *t = tmpfile();
    if (t) { for (int i = 0; i < nc; i++) fprintf(t, "processor\t: %d\nCPU implementer\t: 0x41\nCPU architecture: 8\n\n", i);
      fflush(t); int fd = dup(fileno(t)); fclose(t); lseek(fd, 0, SEEK_SET); return fd; }
  }
  if (p && strstr(p, "cmdline") && !getenv("CUP_NOGFXARGS")) return cmdline_fd();
  char rb[512];
  const char *r = asset_redirect(p, rb, sizeof rb);
  if (r) {
    if (g_dllog) fprintf(stderr, "[open-redir] %s -> %s\n", p, r);
    return open(r, fl);
  }
  va_list ap; va_start(ap, fl); int mo = va_arg(ap, int); va_end(ap);
  int fd = open(p, fl, mo);
  if (g_dllog && p) fprintf(stderr, "[open%s] %s\n", fd < 0 ? "-MISS" : "", p);
  return fd;
}
/* stat/lstat/access com o mesmo redirect — o engine checa existência antes de
   abrir ("No GlobalGameManagers file" pode vir de um stat, não do open).
   Layout de struct stat arm64 = kernel em bionic E glibc → pass-through ok. */
static int my_stat(const char *p, void *st) {
  char rb[512]; const char *r = asset_redirect(p, rb, sizeof rb);
  if (r && g_dllog) fprintf(stderr, "[stat-redir] %s -> %s\n", p, r);
  int rc = stat(r ? r : p, (struct stat *)st);
  if (g_dllog && rc < 0 && p) fprintf(stderr, "[stat-MISS] %s\n", p);
  return rc;
}
static int my_lstat(const char *p, void *st) {
  char rb[512]; const char *r = asset_redirect(p, rb, sizeof rb);
  return lstat(r ? r : p, (struct stat *)st);
}
static int my_access(const char *p, int m) {
  char rb[512]; const char *r = asset_redirect(p, rb, sizeof rb);
  if (r && g_dllog) fprintf(stderr, "[access-redir] %s -> %s\n", p, r);
  return access(r ? r : p, m);
}
/* exit() do jogo: loga QUEM chamou (lr) + stack antes de morrer — a morte
   silenciosa pos-FMOD não deixava rastro. */
static uintptr_t g_unity_base, g_il2cpp_base;
static void my_exit(int code) {
  fprintf(stderr, "[EXIT] exit(%d) chamado! lr=%p\n", code, __builtin_return_address(0));
  uintptr_t tb = (uintptr_t)g_unity_base;
  uintptr_t lr = (uintptr_t)__builtin_return_address(0);
  if (tb && lr >= tb) fprintf(stderr, "[EXIT] (libunity+0x%lx)\n", lr - tb);
  fsync(2);
  _exit(code);
}
/* __system_property_get: FMOD checa ro.build.version.sdk antes de usar OpenSLES
   (vazio→SDK 0→desiste sem nem dar dlsym; receita Dysmantle: "25"). Resto vazio. */
static int my_sysprop(const char *name, char *value) {
  if (!value) return 0;
  if (name && strstr(name, "version.sdk")) { strcpy(value, "25"); return 2; }
  value[0] = 0; return 0;
}
/* __android_log -> stderr */
static int my_alog_print(int prio, const char *tag, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap); return 0;
}
static int my_alog_write(int prio, const char *tag, const char *msg) {
  fprintf(stderr, "[ALOG:%d %s] %s\n", prio, tag ? tag : "?", msg ? msg : ""); return 0;
}
/* __android_log_vprint é o canal do PLAYER LOG do Unity — jamais stubar */
static int my_alog_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
  fprintf(stderr, "[ALOG:%d %s] ", prio, tag ? tag : "?");
  vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); return 0;
}
/* ANativeWindow: Unity espera window !=NULL (nativeRecreateGfxState) senão trava p/ sempre.
   Os egl* do libunity são imports PLT que resolvem no libEGL REAL do Mali (dlopen GLOBAL);
   no fbdev a EGLNativeWindowType é só struct {u16 w, u16 h} → entrega uma DE VERDADE e o
   Unity cria a window surface direto no fb0 (sem shim). CUP_SHIMEGL=1 volta pro fake int. */
static struct { unsigned short w, h; } g_fbdev_win = {1280, 720};
static int g_anw = 0xA11;
static void *my_aw_fromSurface(void *e, void *s) { (void)e; (void)s;
  return getenv("CUP_SHIMEGL") ? (void *)&g_anw : (void *)&g_fbdev_win; }
static int my_aw_setgeom(void *w, int a, int b, int c) { (void)w; (void)a; (void)b; (void)c; return 0; }
static int my_aw_getWidth(void *w) { (void)w; return 1280; }
static int my_aw_getHeight(void *w) { (void)w; return 720; }
static int my_aw_getFormat(void *w) { (void)w; return 1; }
static void my_aw_noop(void *w) { (void)w; }
/* dlopen/dlsym: Unity dlopen libGLESv2/EGL/OpenSLES + dlsym em runtime */
/* ---------- egl_shim (janela GLES2 via SDL2, proven re4) ---------- */
extern void egl_shim_create_window(void);
extern void *egl_shim_GetDisplay(void *);
extern unsigned egl_shim_Initialize(void *, int *, int *);
extern unsigned egl_shim_Terminate(void *);
extern unsigned egl_shim_ChooseConfig(void *, const int *, void **, int, int *);
extern void *egl_shim_CreateWindowSurface(void *, void *, void *, const int *);
extern void *egl_shim_CreatePbufferSurface(void *, void *, const int *);
extern void *egl_shim_CreateContext(void *, void *, void *, const int *);
extern unsigned egl_shim_MakeCurrent(void *, void *, void *, void *);
extern unsigned egl_shim_SwapBuffers(void *, void *);
extern unsigned egl_shim_DestroySurface(void *, void *);
extern unsigned egl_shim_DestroyContext(void *, void *);
extern unsigned egl_shim_QuerySurface(void *, void *, int, int *);
extern unsigned egl_shim_GetConfigAttrib(void *, void *, int, int *);
extern int egl_shim_GetError(void);
extern void *egl_shim_GetProcAddress(const char *);
extern unsigned egl_shim_BindAPI(unsigned);
extern const char *egl_shim_QueryString(void *, int);
extern unsigned egl_shim_SwapInterval(void *, int);
extern void *egl_shim_GetCurrentContext(void);
extern void *egl_shim_GetCurrentSurface(int);
extern unsigned egl_shim_SurfaceAttrib(void *, void *, int, int);
static void *egl_route(const char *nm) {
  struct { const char *n; void *f; } m[] = {
    {"eglGetDisplay", egl_shim_GetDisplay}, {"eglInitialize", egl_shim_Initialize},
    {"eglTerminate", egl_shim_Terminate}, {"eglChooseConfig", egl_shim_ChooseConfig},
    {"eglCreateWindowSurface", egl_shim_CreateWindowSurface},
    {"eglCreatePbufferSurface", egl_shim_CreatePbufferSurface},
    {"eglCreateContext", egl_shim_CreateContext}, {"eglMakeCurrent", egl_shim_MakeCurrent},
    {"eglSwapBuffers", egl_shim_SwapBuffers}, {"eglDestroySurface", egl_shim_DestroySurface},
    {"eglDestroyContext", egl_shim_DestroyContext}, {"eglQuerySurface", egl_shim_QuerySurface},
    {"eglGetConfigAttrib", egl_shim_GetConfigAttrib}, {"eglGetError", egl_shim_GetError},
    {"eglGetProcAddress", egl_shim_GetProcAddress}, {"eglBindAPI", egl_shim_BindAPI},
    {"eglQueryString", egl_shim_QueryString}, {"eglSwapInterval", egl_shim_SwapInterval},
    {"eglGetCurrentContext", egl_shim_GetCurrentContext},
    {"eglGetCurrentSurface", egl_shim_GetCurrentSurface},
    {"eglGetCurrentDisplay", egl_shim_GetDisplay}, {"eglSurfaceAttrib", egl_shim_SurfaceAttrib},
    {0, 0}
  };
  for (int i = 0; m[i].n; i++) if (!strcmp(m[i].n, nm)) return m[i].f;
  return NULL;
}

/* glGetString wrapper (proven re4): o preprocessador de shader do Unity chama
 * glGetString(RENDERER/VERSION/EXT) numa thread sem contexto GL current -> real
 * devolve NULL -> parse char-a-char de NULL estoura o buffer (stack smash em
 * nativeRecreateGfxState). Cache + defaults Mali; NUNCA NULL. */
static const unsigned char *(*r_glGetString)(unsigned) = NULL;
static const unsigned char *g_glcache[5] = {0,0,0,0,0};
static int glstr_idx(unsigned n){ switch(n){case 0x1F00:return 0;case 0x1F01:return 1;case 0x1F02:return 2;case 0x1F03:return 3;case 0x8B8C:return 4;} return -1; }
/* GL_EXTENSIONS curado curto: a string real do Mali-450 é longa e o parser do
 * Unity pode estourar um buffer fixo (stack smash em nativeRecreateGfxState). */
static const char *GL_EXT_SHORT =
  "GL_OES_depth24 GL_OES_element_index_uint GL_OES_texture_npot "
  "GL_OES_rgb8_rgba8 GL_OES_packed_depth_stencil GL_OES_vertex_array_object "
  "GL_EXT_texture_format_BGRA8888 GL_OES_standard_derivatives";
static const unsigned char *my_glGetString(unsigned n){
  if(n==0x1F03) return (const unsigned char*)GL_EXT_SHORT;   /* GL_EXTENSIONS curto */
  if(!r_glGetString) r_glGetString=(const unsigned char*(*)(unsigned))dlsym(RTLD_DEFAULT,"glGetString");
  const unsigned char *s = r_glGetString ? r_glGetString(n) : NULL;
  int i = glstr_idx(n);
  if(s){ if(i>=0 && !g_glcache[i]) g_glcache[i]=(const unsigned char*)strdup((const char*)s); }
  else if(i>=0 && g_glcache[i]) s=g_glcache[i];
  else if(i>=0) s=(const unsigned char*)(n==0x1F00?"ARM":n==0x1F01?"Mali-450 MP":n==0x1F02?"OpenGL ES 2.0":n==0x8B8C?"OpenGL ES GLSL ES 1.00":"");
  return s;
}

/* ---- wrappers GL de shader (diagnóstico: shader falha/trava no Mali?) ---- */
static void (*r_glCompileShader)(unsigned);
static void (*r_glGetShaderiv)(unsigned, unsigned, int *);
static void (*r_glLinkProgram)(unsigned);
static void (*r_glGetProgramiv)(unsigned, unsigned, int *);
static void (*r_glGetShaderInfoLog)(unsigned, int, int *, char *);
static int g_shN, g_prN;
static void my_glCompileShader(unsigned sh) {
  if (!r_glCompileShader) r_glCompileShader = dlsym(RTLD_DEFAULT, "glCompileShader");
  if (!r_glGetShaderiv) r_glGetShaderiv = dlsym(RTLD_DEFAULT, "glGetShaderiv");
  if (!r_glGetShaderInfoLog) r_glGetShaderInfoLog = dlsym(RTLD_DEFAULT, "glGetShaderInfoLog");
  r_glCompileShader(sh);
  int st = -1; if (r_glGetShaderiv) r_glGetShaderiv(sh, 0x8B81, &st); /* COMPILE_STATUS */
  if (st != 1 && g_shN < 20) {
    char log[512] = {0}; if (r_glGetShaderInfoLog) r_glGetShaderInfoLog(sh, sizeof log - 1, NULL, log);
    fprintf(stderr, "[SHADER] compile sh=%u status=%d LOG=%s\n", sh, st, log); dbg_sync();
  }
  g_shN++;
}
static void my_glLinkProgram(unsigned pr) {
  if (!r_glLinkProgram) r_glLinkProgram = dlsym(RTLD_DEFAULT, "glLinkProgram");
  if (!r_glGetProgramiv) r_glGetProgramiv = dlsym(RTLD_DEFAULT, "glGetProgramiv");
  r_glLinkProgram(pr);
  int st = -1; if (r_glGetProgramiv) r_glGetProgramiv(pr, 0x8B82, &st); /* LINK_STATUS */
  if (st != 1 && g_prN < 20) { fprintf(stderr, "[SHADER] link pr=%u status=%d\n", pr, st); dbg_sync(); }
  g_prN++;
}

/* my_eglGetProcAddress: o Unity resolve as funções GL/extensões via
 * eglGetProcAddress (PLT→Mali real). Se uma extensão é ANUNCIADA (glGetString
 * EXTENSIONS) mas a função NÃO resolve (NULL), o Unity guarda um ponteiro
 * inválido e CRASHA ao chamá-lo (fault 0x7f10000004). Loga TODAS as resoluções
 * (com NULL destacado) p/ achar a culpada. CUP_NOVAO força NULL p/ as funções de
 * VAO (testa a hipótese de que GL_OES_vertex_array_object é a culpada). */
static void *(*r_eglGetProcAddress)(const char *);
static unsigned g_egp_n = 0;
static void *my_eglGetProcAddress(const char *nm) {
  if (!r_eglGetProcAddress) r_eglGetProcAddress = dlsym(RTLD_DEFAULT, "eglGetProcAddress");
  void *p = r_eglGetProcAddress ? r_eglGetProcAddress(nm) : NULL;
  if (nm && getenv("CUP_NOVAO") && strstr(nm, "VertexArray")) p = NULL;  /* (vira no-op no Unity) */
  if (g_egp_n++ < 400)
    fprintf(stderr, "[EGP] %s -> %p%s\n", nm ? nm : "(null)", p, p ? "" : "  <== NULL!");
  return p;
}

static char g_dl_self, g_dl_il2cpp;
static so_module *g_m_unity = NULL, *g_m_il2cpp = NULL;

/* ---- probe MemoryManager do libunity (RE: GetMemoryManager=0x3cbe2c) ----
 * gMemoryManager (bss)  vaddr 0x1292B48; cursor da arena estatica vaddr 0x11EF4D0;
 * data segment vaddr 0x11e6000. Detecta corrupcao do singleton entre fases. */
static uintptr_t g_unity_data = 0;
static void mm_probe(const char *tag) {
  if (!g_unity_data) return;
  void *mm  = *(void **)(g_unity_data + (0x1292B48 - 0x11e6000));
  void *cur = *(void **)(g_unity_data + (0x11EF4D0 - 0x11e6000));
  fprintf(stderr, "[MM:%s] gMemoryManager=%p cursor-arena=%p\n", tag, mm, cur);
}

/* ---- spy na entrada do operator-new tagueado (vaddr 0x3cbf2c) ----
 * Na entrada: x0=mgr x1=size x2=align(0x10) x3=kind x4=flag x5=tag-string.
 * O canario estoura nesta funcao durante RecreateGfxState -> capturar a chamada
 * culpada (size/kind gigante). Loga so' qdo g_in_gfx setado (evita flood).
 * O hook clobbera 4 insns; o tramp re-executa e segue em entry+16. */
uintptr_t g_gfx_cont = 0;            /* entry+16 (usado pelo asm) */
uintptr_t g_alloc_ub = 0, g_alloc_ib = 0;
volatile int g_in_gfx = 0;
static unsigned g_ospy_n = 0;
void onew_spy_log(uintptr_t mgr, uintptr_t size, uintptr_t kind, uintptr_t tag);
void onew_spy_log(uintptr_t mgr, uintptr_t size, uintptr_t kind, uintptr_t tag) {
  if (!g_in_gfx) return;
  const char *t = "?";
  if (g_alloc_ub && tag >= g_alloc_ub && tag < g_alloc_ub + 0x11e6000)
    t = (const char *)tag;
  fprintf(stderr, "[ONEW] #%u mgr=%lx size=%lu kind=%lu tag=%s\n",
          ++g_ospy_n, mgr, size, kind, t);
  fflush(stderr);
}
__asm__(
  ".text\n"
  ".global onew_spy_tramp\n"
  "onew_spy_tramp:\n"
  "  stp x29, x30, [sp, #-112]!\n"
  "  stp x0, x1, [sp, #16]\n"
  "  stp x2, x3, [sp, #32]\n"
  "  stp x4, x5, [sp, #48]\n"
  "  stp x6, x7, [sp, #64]\n"
  "  str x8, [sp, #80]\n"
  "  mov x0, x0\n"               /* mgr */
  "  mov x2, x3\n"               /* kind */
  "  mov x3, x5\n"               /* tag */
  "  bl onew_spy_log\n"          /* (mgr,size,kind,tag) */
  "  ldr x8, [sp, #80]\n"
  "  ldp x6, x7, [sp, #64]\n"
  "  ldp x4, x5, [sp, #48]\n"
  "  ldp x2, x3, [sp, #32]\n"
  "  ldp x0, x1, [sp, #16]\n"
  "  ldp x29, x30, [sp], #112\n"
  /* prologo original clobberado (0x3cbf2c..0x3cbf38) */
  "  stp x28, x27, [sp, #-96]!\n"
  "  stp x26, x25, [sp, #16]\n"
  "  stp x24, x23, [sp, #32]\n"
  "  stp x22, x21, [sp, #48]\n"
  "  adrp x17, g_gfx_cont\n"
  "  add x17, x17, :lo12:g_gfx_cont\n"
  "  ldr x17, [x17]\n"
  "  br x17\n"
);
extern void onew_spy_tramp(void);

/* ===== CUP_WAITGATE: FORCEINTEG cirúrgico (só durante WaitForAll) =====
 * O FORCEINTEG global (NOP em 0x872774) integra ops cedo demais até FORA do
 * WaitForAll → corrompe um delegate → crash 0x7f10000004 ~60 frames depois.
 * Aqui só ignoramos o gate de budget QUANDO a main está dentro de
 * WaitForAllAsyncOperationsToComplete (0x873a90, force-complete legítimo).
 *
 * (1) hook 0x873a90 → my_waitall (C): liga g_in_waitall, chama o original
 *     (waitall_orig_tramp re-executa o prólogo clobberado e segue em +16),
 *     desliga o flag. (2) hook do gate 0x871844 → my_gate: se in_waitall=1
 *     retorna 1; senão replica a lógica original (budget 0x871884 AND
 *     (jobmgr==null OR NOT pending 0x6cdad0)). */
volatile int g_in_waitall = 0;
uintptr_t g_waitall_cont = 0;   /* 0x873a90 + 16 (usado pelo asm) */
/* gate replica — usa as bases já capturadas (g_unity_base/g_unity_data) */
int my_gate(void *op);
int my_gate(void *op) {
  if (g_in_waitall) return 1;
  int budget = ((int (*)(void *))(g_unity_base + 0x871884))((char *)op + 0x98);
  if (!budget) return 0;
  void *mgr = *(void **)(g_unity_data + 0xd3380);  /* job-scheduler 0x12b9380 */
  if (!mgr) return 1;
  int pending = ((int (*)(void *))(g_unity_base + 0x6cdad0))(mgr);
  return pending ? 0 : 1;
}
/* trampolim que re-executa o prólogo clobberado de 0x873a90 e segue em +16 */
__asm__(
  ".text\n"
  ".global waitall_orig_tramp\n"
  "waitall_orig_tramp:\n"
  "  stp x22, x21, [sp, #-48]!\n"   /* 0x873a90 */
  "  stp x20, x19, [sp, #16]\n"     /* 0x873a94 */
  "  stp x29, x30, [sp, #32]\n"     /* 0x873a98 */
  "  add x29, sp, #0x20\n"          /* 0x873a9c */
  "  adrp x17, g_waitall_cont\n"
  "  add x17, x17, :lo12:g_waitall_cont\n"
  "  ldr x17, [x17]\n"
  "  br x17\n"
);
extern long waitall_orig_tramp(void *thiz, long a1);
long my_waitall(void *thiz, long a1);
long my_waitall(void *thiz, long a1) {
  g_in_waitall++;
  long r = waitall_orig_tramp(thiz, a1);
  g_in_waitall--;
  return r;
}

static char g_dl_sl; /* sentinela do handle de libOpenSLES (FMOD → opensles_shim) */
static void *my_dlopen(const char *nm, int flag) {
  if (g_dllog) fprintf(stderr, "[dlopen] \"%s\"\n", nm ? nm : "(null)");
  /* il2cpp: nosso modulo ja' carregado (F1). Casa "il2cpp" em qualquer forma. */
  if (nm && strstr(nm, "il2cpp")) { fprintf(stderr, "[DLOPEN] %s -> il2cpp module\n", nm); return &g_dl_il2cpp; }
  /* FMOD (audio do Unity) faz dlopen("libOpenSLES.so") em runtime. CUP_NOSL=1
     desliga o shim (volta ao estado imagem-OK: FMOD cai no null output). */
  if (nm && strstr(nm, "OpenSLES") && !getenv("CUP_NOSL")) {
    fprintf(stderr, "[DLOPEN] %s -> opensles_shim\n", nm); return &g_dl_sl; }
  if (!nm || !nm[0] || strstr(nm, "libc") || strstr(nm, "libunity") || strstr(nm, "libmain"))
    return &g_dl_self;
  void *h = dlopen(nm, flag); return h ? h : &g_dl_self;
}
static void *my_dlsym(void *h, const char *nm) {
  if (!nm) return NULL;
  if (g_dllog) fprintf(stderr, "[dlsym] h=%p \"%s\"\n", h, nm);
  if (!strcmp(nm, "glGetString")) return (void *)my_glGetString;
  if (getenv("CUP_SHLOG")) {
    if (!strcmp(nm, "glCompileShader")) return (void *)my_glCompileShader;
    if (!strcmp(nm, "glLinkProgram")) return (void *)my_glLinkProgram;
  }
  if (nm[0] == 'e' && nm[1] == 'g' && nm[2] == 'l') { void *p = egl_route(nm); if (p) return p; }
  /* AUDIO: dlsym do handle de libOpenSLES -> opensles_shim (slCreateEngine + SL_IID_*
     com as identidades DO SHIM — ele compara ponteiro, receita re4/Dysmantle) */
  if (h == &g_dl_sl) {
    if (!strcmp(nm, "slCreateEngine")) return (void *)slCreateEngine_shim;
    if (!strcmp(nm, "SL_IID_ENGINE")) return (void *)&sl_IID_ENGINE;
    if (!strcmp(nm, "SL_IID_PLAY")) return (void *)&sl_IID_PLAY;
    if (!strcmp(nm, "SL_IID_VOLUME")) return (void *)&sl_IID_VOLUME;
    if (!strcmp(nm, "SL_IID_BUFFERQUEUE") || !strcmp(nm, "SL_IID_ANDROIDSIMPLEBUFFERQUEUE"))
      return (void *)&sl_IID_BUFFERQUEUE;
    if (!strcmp(nm, "SL_IID_EFFECTSEND")) return (void *)&sl_IID_EFFECTSEND;
    if (!strcmp(nm, "SL_IID_ENGINECAPABILITIES")) return (void *)&sl_IID_ENGINECAPABILITIES;
    if (!strcmp(nm, "SL_IID_ENVIRONMENTALREVERB")) return (void *)&sl_IID_ENVIRONMENTALREVERB;
    fprintf(stderr, "[DLSYM:SL] %s -> NULL\n", nm);
    return NULL;
  }
  /* qualquer simbolo il2cpp_* resolve no modulo il2cpp (qualquer handle) */
  if (!strncmp(nm, "il2cpp", 6) && g_m_il2cpp) {
    so_module *c = so_save(); so_use(g_m_il2cpp);
    void *p = (void *)so_find_addr_safe(nm);
    so_use(c); free(c);
    fprintf(stderr, "[DLSYM:il2cpp*] %s -> %p\n", nm, p);
    return p;
  }
  if (h == &g_dl_il2cpp && g_m_il2cpp) {
    so_module *c = so_save(); so_use(g_m_il2cpp);
    void *p = (void *)so_find_addr_safe(nm);
    so_use(c); free(c);
    fprintf(stderr, "[DLSYM:il2cpp] %s -> %p\n", nm, p);
    return p;
  }
  if (h == &g_dl_self) {
    void *p = (void *)so_find_addr_safe(nm);
    if (!p && g_m_il2cpp) { so_module *c = so_save(); so_use(g_m_il2cpp); p = (void *)so_find_addr_safe(nm); so_use(c); free(c); }
    if (!p) p = dlsym(RTLD_DEFAULT, nm);
    return p;
  }
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

/* ---------- abort/raise/tgkill: loga o CALLER (achar a origem do fatal) ---------- */
static void map_caller(const char *tag, uintptr_t ra) {
  if (g_unity_base && ra >= g_unity_base && ra < g_unity_base + 0x2000000)
    fprintf(stderr, "%s caller=libunity+0x%lx\n", tag, ra - g_unity_base);
  else if (g_il2cpp_base && ra >= g_il2cpp_base && ra < g_il2cpp_base + 0x3000000)
    fprintf(stderr, "%s caller=libil2cpp+0x%lx\n", tag, ra - g_il2cpp_base);
  else fprintf(stderr, "%s caller=0x%lx (?)\n", tag, ra);
  fflush(stderr);
}
static int my_raise(int sig) { map_caller("[RAISE]", (uintptr_t)__builtin_return_address(0));
  fprintf(stderr, "[RAISE] sig=%d\n", sig); if (getenv("CUP_NORAISE")) return 0; return raise(sig); }
static void my_abort(void) { map_caller("[ABORT]", (uintptr_t)__builtin_return_address(0));
  if (getenv("CUP_NORAISE")) return; abort(); }
static int my_tgkill(int tgid, int tid, int sig) { map_caller("[TGKILL]", (uintptr_t)__builtin_return_address(0));
  fprintf(stderr, "[TGKILL] sig=%d\n", sig); if (getenv("CUP_NORAISE")) return 0; return syscall(__NR_tgkill, tgid, tid, sig); }

/* __stack_chk_fail: o operator-new tagueado (0x3cbf2c) tem canario; numa chamada
 * do RecreateGfxState ele falha -> abort. Neutraliza p/ diagnosticar (loga caller). */
static int g_scf_n = 0;
static void my_stack_chk_fail(void) {
  uintptr_t ra = (uintptr_t)__builtin_return_address(0);
  if (g_scf_n++ == 0) {
    fprintf(stderr, "\n[SCF] __stack_chk_fail caller=%lx", ra);
    if (g_alloc_ub && ra >= g_alloc_ub && ra < g_alloc_ub + 0x11e6000)
      fprintf(stderr, " (libunity+0x%lx)", ra - g_alloc_ub);
    fprintf(stderr, "\n[SCF] stack scan (callers unity FORA do operator-new):\n");
    uintptr_t sp = (uintptr_t)__builtin_frame_address(0);
    for (int k = 0, hits = 0; k < 800 && hits < 30; k++) {
      uintptr_t v = *(uintptr_t *)(sp + k * 8);
      if (g_alloc_ub && v >= g_alloc_ub && v < g_alloc_ub + 0x11e6000) {
        uintptr_t off = v - g_alloc_ub;
        const char *tag = (off >= 0x3cbe90 && off <= 0x3cc1a0) ? " [op-new]" : " <==";
        fprintf(stderr, "  sp+0x%04x libunity+0x%lx%s\n", k * 8, off, tag);
        hits++;
      } else if (g_alloc_ib && v >= g_alloc_ib && v < g_alloc_ib + 0x2325000) {
        fprintf(stderr, "  sp+0x%04x libil2cpp+0x%lx\n", k * 8, v - g_alloc_ib);
        hits++;
      }
    }
    fflush(stderr);
  }
  /* retorna em vez de abort */
}

/* ---------- helper: override import na tabela ---------- */
static void set_import(const char *name, void *fn) {
  for (size_t i = 0; i < dynlib_numfunctions; i++)
    if (!strcmp(dynlib_functions[i].symbol, name)) { dynlib_functions[i].func = (uintptr_t)fn; return; }
}

/* patch_got: sobrescreve o slot da GOT DIRETO (apos so_resolve). Necessario p/
 * simbolos que NAO estao em dynlib_functions (NDK: ANativeWindow_*, __android_log_*,
 * ASensor*, ...) — p/ esses set_import e' no-op e ficam UNRESOLVED com GOT lixo. */
static int patch_got(const char *name, void *fn) {
  int n = so_patch_got(name, (uintptr_t)fn);
  if (!n) fprintf(stderr, "[GOT] %s: 0 slots (nao achado)\n", name);
  return n;
}

/* stubs NDK no-op (sensores/looper/profiler google) — devolvem 0/NULL */
static long ndk_stub0(void) { return 0; }

extern int my_sigaction();  /* bionic_shims.c (ABI sigset bionic/glibc) */

/* thread de áudio do FMOD (output AudioTrack Java): o Cuphead registra
   org.fmod.FMODAudioDevice.fmodProcess(ByteBuffer) e espera uma thread Java
   chamá-la em loop p/ o mixer avançar. Sem JVM, replicamos em C: assim que
   fmodProcess existir, chamamos com nosso ByteBuffer a cada ~10ms. Isso destrava
   o boot (a main fica presa no nativeRender esperando o áudio progredir). */
static void *g_fmod_env;
static volatile int g_fmod_run = 1;

/* ---- CUP_PSPY: espião do PreloadManager (muro frame 111) ----
 * RE (libunity, offsets confirmados no disasm):
 *   [mgr+208/+224] = fila de PRELOAD  (array/count) — consumida pela thread
 *                    UnityPreload (entry 0x8736cc): roda op->vt[+80] em bg e
 *                    seta op[72]=1; move a op p/ fila de integração (push 0x873790).
 *   [mgr+240/+256] = fila de INTEGRAÇÃO (array/count) — consumida pela main em
 *                    UpdatePreloadingSingleStep (0x8733a8): chama op->vt[+88]
 *                    (timesliced); o POP ([+256]-- em 0x873500) SÓ acontece se
 *                    vt[+88] retornar true E op[72]==1.
 *   HasPendingOperations (0x8739c4) = lock(mgr+0x78); [+224]||[+256]; unlock.
 *   WaitForAllAsyncOperationsToComplete (0x873a90) gira enquanto pendente.
 * O spy substitui 0x8739c4 por uma réplica C (mesma semântica, mesmos locks
 * 0x8f5d0c/0x8f5d14) que captura o ponteiro do mgr; uma thread despeja as filas
 * (op, vtable, flags, alvos de vt[+80/+88/+112]) p/ identificar a op presa. */
#define UN_HASPEND   0x8739c4
#define UN_MTX_LOCK  0x8f5d0c
#define UN_MTX_UNLK  0x8f5d14
static void *volatile g_preload_mgr;
static int my_haspending(void *mgr) {
  g_preload_mgr = mgr;
  void (*lk)(void *) = (void (*)(void *))(g_unity_base + UN_MTX_LOCK);
  void (*ul)(void *) = (void (*)(void *))(g_unity_base + UN_MTX_UNLK);
  lk((char *)mgr + 0x78);
  uintptr_t pq = *(uintptr_t *)((char *)mgr + 224);
  uintptr_t iq = *(uintptr_t *)((char *)mgr + 256);
  ul((char *)mgr + 0x78);
  return (pq || iq) ? 1 : 0;
}
static void pspy_dump_op(const char *qn, unsigned i, uintptr_t op) {
  uintptr_t vt = *(uintptr_t *)op;
  uintptr_t vt_off = (vt >= g_unity_base) ? vt - g_unity_base : vt;
  uintptr_t f80 = *(uintptr_t *)(vt + 80), f88 = *(uintptr_t *)(vt + 88);
  uintptr_t f112 = *(uintptr_t *)(vt + 112);
  fprintf(stderr,
          "[PSPY]  %s[%u] op=%lx vt=u+0x%lx done72=%d w64=%d w68=%d "
          "bg(+80)=u+0x%lx integ(+88)=u+0x%lx q(+112)=u+0x%lx\n",
          qn, i, op, vt_off, *(int *)(op + 72), *(int *)(op + 64),
          *(int *)(op + 68), f80 - g_unity_base, f88 - g_unity_base,
          f112 - g_unity_base);
  /* primeiros 0xC0 bytes da op (estado interno: progress/flags/ponteiros) */
  for (int k = 0; k < 24; k += 4)
    fprintf(stderr, "[PSPY]   +%02x: %016lx %016lx %016lx %016lx\n", k * 8,
            ((uintptr_t *)op)[k], ((uintptr_t *)op)[k + 1],
            ((uintptr_t *)op)[k + 2], ((uintptr_t *)op)[k + 3]);
}
static void *preload_spy_thread(void *arg) {
  (void)arg;
  fprintf(stderr, "[PSPY] thread ativa (2s)\n");
  while (g_fmod_run) {
    sleep(2);
    char *m = (char *)g_preload_mgr;
    if (!m) continue;
    /* leitura SEM lock (racy, mas nunca bloqueia/deadlocka o diagnóstico) */
    uintptr_t pq_a = *(uintptr_t *)(m + 208), pq_n = *(uintptr_t *)(m + 224);
    uintptr_t iq_a = *(uintptr_t *)(m + 240), iq_n = *(uintptr_t *)(m + 256);
    /* job-scheduler global (libunity bss 0x12b9380; o gate da integração só passa
       quando estes 3 contadores estão <=0 — 0x6cdad0). +0x70=jobs / +0x168 / +0x16c */
    void *jm = *(void **)(g_unity_data + 0xd3380);
    if (jm) fprintf(stderr, "[PSPY] jobmgr=%p +70=%d +168=%d +16c=%d\n", jm,
                    *(int *)((char *)jm + 0x70), *(int *)((char *)jm + 0x168),
                    *(int *)((char *)jm + 0x16c));
    fprintf(stderr, "[PSPY] mgr=%p preloadQ=%lu integQ=%lu\n", m, pq_n, iq_n);
    for (unsigned i = 0; i < pq_n && i < 2; i++)
      if (((uintptr_t *)pq_a)[i]) pspy_dump_op("PQ", i, ((uintptr_t *)pq_a)[i]);
    for (unsigned i = 0; i < iq_n && i < 2; i++)
      if (((uintptr_t *)iq_a)[i]) pspy_dump_op("IQ", i, ((uintptr_t *)iq_a)[i]);
    dbg_sync();
  }
  return NULL;
}
/* TESTE CUP_PRELOAD_TICK: posta periodicamente os sems em que threads não-main
   bloqueiam (acorda a UnityPreload p/ processar o item pendente da fila). */
static void *preload_tick_thread(void *arg) {
  (void)arg;
  fprintf(stderr, "[TICK] thread de preload-tick ativa (16ms)\n");
  while (g_fmod_run) { sh_tick_preload(); usleep(16000); }
  return NULL;
}
static void *fmod_audio_thread(void *arg) {
  (void)arg;
  void *fp = NULL;
  while (g_fmod_run && !(fp = jni_find_native("fmodProcess"))) usleep(20000);
  if (!fp) return NULL;
  fprintf(stderr, "[AUDIO] fmodProcess=%p; thread alimentando (10ms)\n", fp);
  static long dev = 0xFAD;            /* this (FMODAudioDevice) fake */
  void *bb = jni_fmod_bytebuffer();
  /* dá um tempo p/ o FMOD terminar o init antes de bater no mixer */
  usleep(300000);
  unsigned long n = 0;
  while (g_fmod_run) {
    int r = ((int (*)(void *, void *, void *))fp)(g_fmod_env, &dev, bb);
    if (n < 3 || n % 500 == 0) { fprintf(stderr, "[AUDIO] fmodProcess #%lu -> %d\n", n, r); dbg_sync(); }
    n++;
    usleep(10000);
  }
  return NULL;
}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  setvbuf(stdout, NULL, _IONBF, 0); setvbuf(stderr, NULL, _IONBF, 0);
  g_main_tid = (int)syscall(SYS_gettid);  /* p/ o sem_shim distinguir main de workers */
  if (getenv("CUP_SEMPOLL")) sh_sem_set_poll(atoi(getenv("CUP_SEMPOLL")));  /* polling do sem_wait */
  { extern void cond_set_poll(int);  /* polling do pthread_cond_wait (lost-wakeup futex) */
    if (getenv("CUP_CONDPOLL")) cond_set_poll(atoi(getenv("CUP_CONDPOLL"))); }

  /* log persistente: stderr -> debug.log (unbuffered + fsync nos marcos =
     sobrevive a hang/power-cycle do device). CUP_NOLOGFILE=1 desativa. */
  if (!getenv("CUP_NOLOGFILE")) {
    int lf = open(ASSET_BASE_M "debug.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (lf >= 0) {
      printf("stderr -> " ASSET_BASE_M "debug.log\n");
      dup2(lf, 2); if (lf != 2) close(lf);
    }
  }

  /* valida que tpidr_el0+0x28 (canary bionic) caiu DENTRO do nosso pad TLS */
  { uintptr_t tp; __asm__ volatile("mrs %0, tpidr_el0" : "=r"(tp));
    uintptr_t slot = tp + 0x28, lo = (uintptr_t)g_bionic_guard_pad;
    fprintf(stderr, "TLS guard: tpidr=0x%lx slot+0x28=0x%lx pad=[0x%lx..0x%lx] %s (val=0x%lx)\n",
            (unsigned long)tp, (unsigned long)slot, (unsigned long)lo,
            (unsigned long)(lo + sizeof(g_bionic_guard_pad)),
            (slot >= lo && slot + 8 <= lo + sizeof(g_bionic_guard_pad))
                ? "DENTRO ok" : "FORA (canary instavel!)",
            *(unsigned long *)slot);
  }
  /* sigaltstack: p/ o handler reportar STACK OVERFLOW (SIGSEGV na guard page →
     sem espaço na pilha normal p/ rodar o handler → morte silenciosa). */
  { static char altstk[64 * 1024]; stack_t ss = {0};
    ss.ss_sp = altstk; ss.ss_size = sizeof altstk; ss.ss_flags = 0;
    sigaltstack(&ss, NULL); }
  struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_sigaction = on_crash; sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigaction(SIGSEGV, &sa, 0); sigaction(SIGBUS, &sa, 0); sigaction(SIGABRT, &sa, 0);
  sigaction(SIGILL, &sa, 0); sigaction(SIGFPE, &sa, 0);
  sigaction(SIGTRAP, &sa, 0); sigaction(SIGSYS, &sa, 0);  /* BRK/seccomp matam calado */

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
  g_unity_data = (uintptr_t)data_base;
  if (so_relocate() < 0) { fprintf(stderr, "relocate FALHOU\n"); return 1; }

  /* overrides */
  g_unity_base = (uintptr_t)text_base;
  set_import("abort", (void *)my_abort);
  set_import("raise", (void *)my_raise);
  set_import("tgkill", (void *)my_tgkill);
  set_import("exit", (void *)my_exit);
  set_import("_exit", (void *)my_exit);
  /* __stack_chk_fail nao esta na tabela de imports -> patch direto na GOT (apos resolve) */
  set_import("glGetString", (void *)my_glGetString);
  if (getenv("CUP_EGPLOG") || getenv("CUP_NOVAO"))
    set_import("eglGetProcAddress", (void *)my_eglGetProcAddress);
  set_import("sysconf", (void *)my_sysconf);
  set_import("fopen", (void *)my_fopen);
  set_import("open", (void *)my_open);
  set_import("stat", (void *)my_stat);
  set_import("lstat", (void *)my_lstat);
  set_import("access", (void *)my_access);
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

  install_sem_shim();  /* semáforos próprios bionic→glibc (fix deadlock boot) */

  fprintf(stderr, "[F0] resolvendo %zu imports...\n", dynlib_numfunctions);
  if (so_resolve(dynlib_functions, dynlib_numfunctions, 0) < 0) { fprintf(stderr, "resolve FALHOU\n"); return 1; }
  /* PATCH-GOT: os imports NDK nao estao em dynlib_functions -> set_import foi
   * no-op e ficaram UNRESOLVED (GOT lixo). Sobrescreve os slots DIRETO. */
  patch_got("ANativeWindow_fromSurface", (void *)my_aw_fromSurface);
  patch_got("ANativeWindow_setBuffersGeometry", (void *)my_aw_setgeom);
  patch_got("ANativeWindow_getWidth", (void *)my_aw_getWidth);
  patch_got("ANativeWindow_getHeight", (void *)my_aw_getHeight);
  patch_got("ANativeWindow_getFormat", (void *)my_aw_getFormat);
  patch_got("ANativeWindow_acquire", (void *)my_aw_noop);
  patch_got("ANativeWindow_release", (void *)my_aw_noop);
  patch_got("__android_log_print", (void *)my_alog_print);
  patch_got("__android_log_write", (void *)my_alog_write);
  patch_got("__android_log_vprint", (void *)my_alog_vprint);
  if (getenv("CUP_EGPLOG") || getenv("CUP_NOVAO"))
    patch_got("eglGetProcAddress", (void *)my_eglGetProcAddress);
  /* dl* estavam COMENTADOS em imports.gen.c -> set_import foi no-op e o dlopen@plt
     caiu no glibc REAL (falha ao carregar .so Android). Sem isso o il2cpp nao carrega. */
  patch_got("dlopen", (void *)my_dlopen);
  patch_got("dlsym", (void *)my_dlsym);
  patch_got("dlerror", (void *)my_dlerror);
  patch_got("dlclose", (void *)my_dlclose);
  patch_got("dladdr", (void *)my_dladdr);
  /* engine checa existência dos arquivos de dados antes de abrir */
  patch_got("open", (void *)my_open);
  patch_got("fopen", (void *)my_fopen);
  patch_got("stat", (void *)my_stat);
  patch_got("lstat", (void *)my_lstat);
  patch_got("access", (void *)my_access);
  patch_got("exit", (void *)my_exit);
  patch_got("_exit", (void *)my_exit);
  patch_sem_shim();  /* sem_* nos slots GOT do libunity */
  /* sensores/looper/profiler google: stub no-op (nao usados no path do gfx) */
  const char *ndk_noop[] = {
    "ALooper_forThread","ALooper_prepare","ASensorManager_getInstance",
    "ASensorManager_createEventQueue","ASensorManager_getSensorList",
    "ASensorManager_getDefaultSensor","ASensorManager_destroyEventQueue",
    "ASensorEventQueue_hasEvents","ASensorEventQueue_getEvents",
    "ASensorEventQueue_enableSensor","ASensorEventQueue_disableSensor",
    "ASensorEventQueue_setEventRate","ASensor_getType","ASensor_getResolution",
    "ASensor_getMinDelay","ASensor_getName","ASensor_getVendor",
    "__google_potentially_blocking_region_begin",
    "__google_potentially_blocking_region_end", NULL };
  for (int i = 0; ndk_noop[i]; i++) patch_got(ndk_noop[i], (void *)ndk_stub0);

  /* CUP_FORCEIL2: o helper "load library by name" do Unity (0x357938) faz o
     System.load do il2cpp via JNI -> falha no nosso ambiente ("Failed to load
     Il2CPP"). Mas NOS ja' carregamos libil2cpp.so no F1. Forca retorno 1 (sucesso):
       mov w0,#1 ; ret  */
  if (getenv("CUP_FORCEIL2")) {
    *(uint32_t *)((uintptr_t)text_base + 0x357938) = 0x52800020u; /* mov w0,#1 */
    *(uint32_t *)((uintptr_t)text_base + 0x35793c) = 0xd65f03c0u; /* ret */
    fprintf(stderr, "[FORCEIL2] 0x357938 -> mov w0,#1; ret\n");
  }
  /* CUP_NOEXTRACT: a extracao de recursos do APK (0x94184c) copia de um VFS source
     (o APK) que nao temos -> falha ("Failed to extract resources"). Mas os assets
     JA estao deployados em bin/Data/. Forca a extracao reportar sucesso. */
  if (getenv("CUP_NOEXTRACT")) {
    *(uint32_t *)((uintptr_t)text_base + 0x94184c) = 0x52800020u; /* mov w0,#1 */
    *(uint32_t *)((uintptr_t)text_base + 0x941850) = 0xd65f03c0u; /* ret */
    fprintf(stderr, "[NOEXTRACT] 0x94184c -> mov w0,#1; ret\n");
  }

  so_finalize(); so_flush_caches();
  g_alloc_ub = (uintptr_t)text_base;
  if (getenv("CUP_DLLOG")) g_dllog = 1;

  /* __stack_chk_fail nao esta na tabela -> patch direto no slot da GOT */
  if (getenv("CUP_NOSCF")) {
    extern uintptr_t so_find_rel_addr_safe(const char *);
    uintptr_t got = so_find_rel_addr_safe("__stack_chk_fail");
    if (got) { *(uintptr_t *)got = (uintptr_t)my_stack_chk_fail;
      fprintf(stderr, "[SCF] GOT __stack_chk_fail @ 0x%lx patcheado\n", got); }
    else fprintf(stderr, "[SCF] __stack_chk_fail nao achado na GOT\n");
  }

  /* DIAGNÓSTICO: a main fica presa num loop (libunity 0x873a90) esperando a
     função 0x8739c4 (fila do GfxDevice [+224]/[+256]) zerar — deadlock do
     threaded rendering no Mali. CUP_NOGFXWAIT patcha 0x8739c4 p/ retornar 0
     (não espera) e ver se o jogo avança. */
  if (getenv("CUP_NOGFXWAIT")) {
    extern void so_make_text_writable(void), so_make_text_executable(void);
    so_make_text_writable();
    *(uint32_t *)((uintptr_t)text_base + 0x8739c4) = 0x52800000u; /* mov w0,#0 */
    *(uint32_t *)((uintptr_t)text_base + 0x8739c8) = 0xd65f03c0u; /* ret */
    so_make_text_executable(); so_flush_caches();
    fprintf(stderr, "[NOGFXWAIT] 0x8739c4 -> mov w0,#0; ret\n");
  }

  /* CUP_PSPY: substitui HasPendingOperations (0x8739c4) pela réplica C que
     captura o ponteiro do PreloadManager (diagnóstico do muro frame 111). */
  if (getenv("CUP_PSPY")) {
    extern void so_make_text_writable(void), so_make_text_executable(void);
    so_make_text_writable();
    hook_arm64((uintptr_t)text_base + UN_HASPEND, (uintptr_t)my_haspending);
    so_make_text_executable(); so_flush_caches();
    fprintf(stderr, "[PSPY] hook HasPendingOperations (0x%x) instalado\n", UN_HASPEND);
  }
  /* CUP_FORCEINTEG: dentro de IntegrateOp (0x872758), o gate (0x871844, budget
     time-slice) é checado em 0x872774 `tbz w0,#0, 872810` → se budget recusa,
     integração aborta retornando 0. Em WaitForAll (force-complete) o budget
     DEVERIA ser ignorado. NOP nesse branch: o gate ainda RODA (efeitos colaterais
     do predictor preservados), só o VEREDITO é ignorado → integração prossegue.
     Cirúrgico (só o path de integração; não mexe no gate compartilhado). */
  if (getenv("CUP_FORCEINTEG")) {
    extern void so_make_text_writable(void), so_make_text_executable(void);
    so_make_text_writable();
    *(uint32_t *)((uintptr_t)text_base + 0x872774) = 0xd503201fu; /* NOP */
    so_make_text_executable(); so_flush_caches();
    fprintf(stderr, "[FORCEINTEG] 0x872774 (gate-fail branch) -> NOP\n");
  }
  /* CUP_WAITGATE: FORCEINTEG cirúrgico — ignora o gate de budget SÓ dentro do
     WaitForAll (0x873a90). Hook do WaitForAll (flag in_waitall) + hook do gate
     (0x871844 → my_gate). NÃO combinar com CUP_FORCEINTEG. */
  if (getenv("CUP_WAITGATE")) {
    extern void so_make_text_writable(void), so_make_text_executable(void);
    g_waitall_cont = (uintptr_t)text_base + 0x873a90 + 16;
    so_make_text_writable();
    hook_arm64((uintptr_t)text_base + 0x873a90, (uintptr_t)my_waitall);
    hook_arm64((uintptr_t)text_base + 0x871844, (uintptr_t)my_gate);
    so_make_text_executable(); so_flush_caches();
    fprintf(stderr, "[WAITGATE] hook WaitForAll(0x873a90)+gate(0x871844); cont=0x%lx\n",
            (unsigned long)g_waitall_cont);
  }

  fprintf(stderr, "[F0] init_array...\n");
  so_execute_init_array();
  fprintf(stderr, "[F0] libunity init OK\n");
  mm_probe("pos-init_array-unity");

  /* ---- JNI_OnLoad da libunity ---- */
  void *vm = NULL, *env = NULL; jni_shim_init(&vm, &env);
  uintptr_t onload = so_find_addr_safe("JNI_OnLoad");
  if (onload) {
    int ver = ((int (*)(void *, void *))onload)(vm, NULL);
    fprintf(stderr, "[F0] JNI_OnLoad = 0x%x\n", ver);
  } else {
    fprintf(stderr, "[F0] JNI_OnLoad não encontrado em libunity\n");
  }
  fprintf(stderr, "[F0] === libunity OK ===\n");
  mm_probe("pos-JNI_OnLoad");
  dbg_sync();

  /* ---- F1: carrega libil2cpp.so (2º módulo, lógica C# do jogo) ---- */
  g_m_unity = so_save();
  size_t i2s = 96UL * 1024 * 1024;
  void *i2heap = mmap(NULL, i2s, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (i2heap != MAP_FAILED && so_load("libil2cpp.so", i2heap, i2s) >= 0) {
    g_il2cpp_base = (uintptr_t)text_base;
    g_alloc_ib = g_il2cpp_base;
    fprintf(stderr, "[F1] libil2cpp: text=%p+%zu\n", text_base, text_size);
    so_relocate();
    so_resolve(dynlib_functions, dynlib_numfunctions, 0);
    /* il2cpp abre o global-metadata.dat via open() -> intercepta p/ redirecionar.
       patch_got opera no modulo ATIVO (=il2cpp agora). Tb dlopen/dlsym/log. */
    patch_got("open", (void *)my_open);
    patch_got("fopen", (void *)my_fopen);
    patch_got("stat", (void *)my_stat);
    patch_got("lstat", (void *)my_lstat);
    patch_got("access", (void *)my_access);
    patch_got("dlopen", (void *)my_dlopen);
    patch_got("dlsym", (void *)my_dlsym);
    patch_got("exit", (void *)my_exit);
    patch_got("_exit", (void *)my_exit);
    patch_got("__android_log_print", (void *)my_alog_print);
    patch_got("__android_log_write", (void *)my_alog_write);
    patch_got("__android_log_vprint", (void *)my_alog_vprint);
    patch_sem_shim();  /* sem_* nos slots GOT do libil2cpp */
    so_finalize(); so_flush_caches();
    fprintf(stderr, "[F1] libil2cpp init_array...\n");
    so_execute_init_array();
    g_m_il2cpp = so_save();
    fprintf(stderr, "[F1] libil2cpp carregado OK\n");
    mm_probe("pos-init_array-il2cpp");
    dbg_sync();
  } else {
    fprintf(stderr, "[F1] FALHOU carregar libil2cpp (heap=%p)\n", i2heap);
  }
  /* informa o sem_shim das bases p/ o detector de livelock mapear callers */
  { extern void sh_set_bases(unsigned long, unsigned long, unsigned long, unsigned long);
    sh_set_bases(g_unity_base, 0x2000000, g_il2cpp_base, 0x3000000); }

  so_use(g_m_unity);  /* volta o contexto p/ libunity */

  /* lista os métodos nativos registrados (achar initJni/nativeRender) */
  extern void jni_dump_natives(void);
  extern void *jni_find_native(const char *);
  jni_dump_natives();

  /* ---- F2: janela GLES2 + lifecycle Unity ----
     Default = EGL REAL do Mali (Unity cria contexto/surface no fb0 via fbdev_window);
     CUP_SHIMEGL=1 = caminho antigo SDL2/egl_shim (não usado pelo Unity: egl* é PLT). */
  if (getenv("CUP_SHIMEGL")) {
    extern int egl_shim_ensure_current(void);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) fprintf(stderr, "[F2] SDL_Init: %s\n", SDL_GetError());
    egl_shim_create_window();
    egl_shim_ensure_current();   /* deixa o contexto GL current na thread do jogo */
    fprintf(stderr, "[F2] janela GLES2 criada (egl_shim/SDL2)\n");
  } else {
    /* áudio (opensles_shim usa SDL_OpenAudioDevice) */
    if (SDL_Init(SDL_INIT_AUDIO) != 0) fprintf(stderr, "[F2] SDL_Init(AUDIO): %s\n", SDL_GetError());
    fprintf(stderr, "[F2] EGL REAL Mali fbdev (fbdev_window %ux%u)\n",
            g_fbdev_win.w, g_fbdev_win.h);
  }

  static long thiz = 0xA1, ctx = 0xC0, surf = 0x5F;
  void *fn;
  if ((fn = jni_find_native("initJni"))) {
    fprintf(stderr, "[F2] initJni...\n");
    ((void (*)(void *, void *, void *))fn)(env, &thiz, &ctx);
    fprintf(stderr, "[F2] initJni OK\n");
    mm_probe("pos-initJni");
    dbg_sync();
  }
  if ((fn = jni_find_native("nativeRecreateGfxState"))) {
    mm_probe("pre-RecreateGfxState");
    /* TESTE: anula o instalador de signal-handlers do Unity (0x360af8) com RET.
       Esse caminho (sigaction QUERY -> map RB-tree de old-handlers via operator-new)
       e' onde o canario estoura. Nao precisamos dos handlers do Unity (temos on_crash). */
    if (getenv("CUP_NOSIGINST")) {
      extern void so_make_text_writable(void), so_make_text_executable(void);
      so_make_text_writable();
      *(uint32_t *)(g_alloc_ub + 0x360af8) = 0xd65f03c0u;  /* RET */
      so_make_text_executable();
      so_flush_caches();
      fprintf(stderr, "[NOSIGINST] 0x360af8 (install handlers) -> RET\n");
    }
    /* spy: hook na entrada do operator-new (0x3cbf2c) p/ capturar args da
       chamada que estoura o canario. Instala AQUI p/ pegar so' o gfx path. */
    if (getenv("CUP_ASPY")) {
      extern void so_make_text_writable(void), so_make_text_executable(void);
      g_gfx_cont = g_alloc_ub + 0x3cbf2c + 16;
      so_make_text_writable();
      hook_arm64(g_alloc_ub + 0x3cbf2c, (uintptr_t)onew_spy_tramp);
      so_make_text_executable();
      so_flush_caches();
      g_in_gfx = 1;
      fprintf(stderr, "[ONEW] hook em operator-new (0x3cbf2c) instalado\n");
    }
    fprintf(stderr, "[F2] nativeRecreateGfxState...\n");
    ((void (*)(void *, void *, int, void *))fn)(env, &thiz, 0, &surf);
    fprintf(stderr, "[F2] nativeRecreateGfxState OK\n");
    mm_probe("pos-RecreateGfxState");
    dbg_sync();
  }
  if ((fn = jni_find_native("nativeResume"))) ((void (*)(void *, void *))fn)(env, &thiz);
  if ((fn = jni_find_native("nativeFocusChanged"))) ((void (*)(void *, void *, int))fn)(env, &thiz, 1);

  /* dispara a thread de áudio do FMOD (alimenta fmodProcess em paralelo ao
     render — destrava o boot que espera o mixer). CUP_NOAUDIOTHREAD=1 desliga. */
  if (!getenv("CUP_NOAUDIOTHREAD")) {
    g_fmod_env = env;
    pthread_t at; pthread_create(&at, NULL, fmod_audio_thread, NULL);
    pthread_detach(at);
    fprintf(stderr, "[AUDIO] thread de áudio FMOD criada\n");
  }
  if (getenv("CUP_PRELOAD_TICK")) {
    pthread_t tt; pthread_create(&tt, NULL, preload_tick_thread, NULL);
    pthread_detach(tt);
  }
  if (getenv("CUP_PSPY")) {
    pthread_t st; pthread_create(&st, NULL, preload_spy_thread, NULL);
    pthread_detach(st);
  }

  void *render = jni_find_native("nativeRender");
  fprintf(stderr, "[F2] nativeRender=%p -> loop\n", render);
  int max_f = getenv("CUP_FRAMES") ? atoi(getenv("CUP_FRAMES")) : 600;
  void *fpump = jni_find_native("nativePause");  /* só p/ existência */ (void)fpump;
  for (int f = 0; render && (max_f <= 0 || f < max_f); f++) {
    if (f < 200) { fprintf(stderr, "[r%d>\n", f); dbg_sync(); }  /* ENTRA no render */
    ((unsigned char (*)(void *, void *))render)(env, &thiz);
    if (f < 200) { fprintf(stderr, "<r%d]\n", f); dbg_sync(); }  /* SAIU do render */
    opensles_shim_pump_callbacks();
    /* bombeia eventos SDL (foco/janela) p/ o input do Unity não esfomear */
    SDL_Event ev; while (SDL_PollEvent(&ev)) {}
    if (f % 60 == 0) { fprintf(stderr, "[render %d]\n", f); dbg_sync(); }
  }
  fprintf(stderr, "[F2] === render loop terminou ===\n");
  fflush(stderr); dbg_sync();
  _exit(0);  /* hard exit — destrutores do .so crasham no teardown normal */
}
