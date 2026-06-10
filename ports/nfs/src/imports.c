/*
 * imports.c — shims bionic→glibc do NFS (os 18 imports que o dlsym fallback
 * não cobre: __sF/__errno/__assert2/__android_log/_ctype_/_tolower_tab_/
 * __cxa_type_match/__dso_handle/sigsetjmp/AndroidBitmap_*).
 * Exporta nfs_shims[] — main.c usa como base da tabela combinada.
 */
#define _LARGEFILE64_SOURCE
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <dlfcn.h>
#include <string.h>

#include "so_util.h"
#include "egl_shim.h"

/* ---- bionic __sF[3] = stdin/out/err (libc++ usa p/ std::cerr/cout) ---- */
static char bionic_sF[3][512];
static FILE *map_sF(void *fp) {
  if (fp == (void *)&bionic_sF[0]) return stdin;
  if (fp == (void *)&bionic_sF[1]) return stdout;
  if (fp == (void *)&bionic_sF[2]) return stderr;
  return (FILE *)fp;
}
static int w_fprintf(void *fp, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); int r = vfprintf(map_sF(fp), fmt, ap); va_end(ap); return r;
}
static int w_vfprintf(void *fp, const char *fmt, va_list ap) { return vfprintf(map_sF(fp), fmt, ap); }
static size_t w_fwrite(const void *p, size_t s, size_t n, void *fp) { return fwrite(p, s, n, map_sF(fp)); }
static int w_fputs(const char *str, void *fp) { return fputs(str, map_sF(fp)); }
static int w_fputc(int c, void *fp) { return fputc(c, map_sF(fp)); }
static int w_fflush(void *fp) { return fflush(fp ? map_sF(fp) : NULL); }

/* ---- errno / assert / log ---- */
static int *b_errno(void) { extern int *__errno_location(void); return __errno_location(); }
static void b_assert2(const char *f, int l, const char *fn, const char *m) {
  fprintf(stderr, "[assert] %s:%d %s: %s\n", f ? f : "?", l, fn ? fn : "?", m ? m : "?");
  abort();
}
static int b_log_print(int prio, const char *tag, const char *fmt, ...) {
  (void)prio; va_list ap; va_start(ap, fmt);
  fprintf(stderr, "[%s] ", tag ? tag : "nfs"); vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n"); va_end(ap); return 0;
}
static int b_log_write(int prio, const char *tag, const char *msg) {
  (void)prio; fprintf(stderr, "[%s] %s\n", tag ? tag : "nfs", msg ? msg : ""); return 0;
}
static void b_log_assert(const char *cond, const char *tag, const char *fmt, ...) {
  (void)cond; va_list ap; va_start(ap, fmt);
  fprintf(stderr, "[%s ASSERT] ", tag ? tag : "nfs"); vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n"); va_end(ap); abort();
}

/* ---- ctype tables bionic (_ctype_[c+1], _tolower_tab_[c+1]) ----
 * bionic flags: _U=1 _L=2 _N=4 _S=8 _P=0x10 _C=0x20 _X=0x40 _B=0x80.
 * Preenchidas da classificação glibc no constructor. */
static unsigned char b_ctype[1 + 256];
static unsigned char b_tolower[1 + 256];
static unsigned char b_toupper[1 + 256];
__attribute__((constructor)) static void init_ctype(void) {
  for (int c = 0; c < 256; c++) {
    unsigned char f = 0;
    if (isupper(c)) f |= 0x01;
    if (islower(c)) f |= 0x02;
    if (isdigit(c)) f |= 0x04;
    if (isspace(c)) f |= 0x08;
    if (ispunct(c)) f |= 0x10;
    if (iscntrl(c)) f |= 0x20;
    if (isxdigit(c)) f |= 0x40;
    if (c == ' ')   f |= 0x80;
    b_ctype[c + 1] = f;
    b_tolower[c + 1] = (unsigned char)tolower(c);
    b_toupper[c + 1] = (unsigned char)toupper(c);
  }
}

/* ---- malloc com PADDING (+64B) ----
 * A engine NFS faz pequenos overflows que o malloc do bionic tolera (layout de
 * chunk diferente) mas o glibc detecta ("malloc(): invalid size"). Over-alocar
 * absorve o overflow na folga, sem corromper a metadata do glibc. free/realloc
 * usam o mesmo ponteiro base → consistentes. */
static int nfs_pad_bytes(void) {
  static int v = -1;
  if (v < 0) { const char *e = getenv("NFS_PAD"); v = e ? atoi(e) : 64; if (v < 0) v = 0; }
  return v;
}
static void *pad_malloc(size_t n) { return malloc(n + nfs_pad_bytes()); }
static void *pad_calloc(size_t a, size_t b) {
  size_t t = a * b, pad = nfs_pad_bytes(); void *p = malloc(t + pad); if (p) memset(p, 0, t + pad); return p;
}
static void *pad_realloc(void *p, size_t n) { return realloc(p, n + nfs_pad_bytes()); }
static void *pad_memalign(size_t al, size_t n) { return memalign(al, n + nfs_pad_bytes()); }

/* ---- pthread_create hook: loga a função de entrada de cada thread (p/
 * identificar a worker thread que crasha no init). NFS_PTLOG=1 liga. ---- */
static int (*real_pthread_create)(void *, const void *, void *(*)(void *), void *);
static int my_pthread_create(void *th, const void *attr, void *(*fn)(void *), void *arg) {
  if (!real_pthread_create) {
    real_pthread_create = (int (*)(void *, const void *, void *(*)(void *), void *))
        dlsym(RTLD_DEFAULT, "pthread_create");
  }
  if (getenv("NFS_PTLOG")) {
    static int n = 0;
    if (n < 40) { fprintf(stderr, "[pthread_create] #%d fn=%p arg=%p attr=%p\n", n, (void *)fn, arg, (void *)attr); n++; }
  }
  /* IGNORA o attr bionic (layout ≠ glibc → glibc lê lixo de stack-size). Passa
   * NULL = thread glibc default (8MB). NFS_PT_KEEPATTR=1 mantém o attr original. */
  if (!getenv("NFS_PT_KEEPATTR")) attr = NULL;
  return real_pthread_create(th, attr, fn, arg);
}

/* ---- fopen/open hook: loga acessos a arquivo (ver se a engine abre o OBB) ---- */
static void *(*real_fopen)(const char *, const char *);
static void *my_fopen(const char *path, const char *mode) {
  if (!real_fopen) real_fopen = (void *(*)(const char *, const char *))dlsym(RTLD_DEFAULT, "fopen");
  void *fp = real_fopen(path, mode);
  if (getenv("NFS_FOPENLOG")) {
    static int n = 0;
    if (n < 60) { fprintf(stderr, "[fopen] '%s' (%s) -> %s\n", path ? path : "?", mode ? mode : "?", fp ? "OK" : "MISS"); n++; }
  }
  return fp;
}
static size_t (*real_fread)(void *, size_t, size_t, void *);
static size_t my_fread(void *p, size_t sz, size_t n, void *fp) {
  if (!real_fread) real_fread = (size_t(*)(void*,size_t,size_t,void*))dlsym(RTLD_DEFAULT, "fread");
  size_t r = real_fread(p, sz, n, fp);
  { extern long nfs_io_read_bytes; nfs_io_read_bytes += (long)(r * sz); }
  if (getenv("NFS_FOPENLOG")) { static int c = 0; static size_t tot = 0; tot += r * sz;
    if (c < 30) { fprintf(stderr, "[fread] %zu*%zu -> %zu (fp=%p, total=%zu)\n", sz, n, r, fp, tot); c++; } }
  return r;
}
/* ---- stat/fstat/statfs: o struct stat do bionic ARM32 = kernel stat64 =
 * glibc stat64 (st_size 64-bit @ offset 48). O stat/fstat DEFAULT da glibc é o
 * layout ANTIGO (st_size 32-bit @ 44) → a engine lia st_blksize/st_blocks como
 * tamanho do OBB (lixo) → seek/parse do índice garbage. Rotear p/ as *64. ---- */
static int b_stat(const char *p, void *st) {
  int rc = stat64(p, (struct stat64 *)st);
  if (getenv("NFS_SEEKLOG")) {
    static int n = 0;
    if (n < 30) { fprintf(stderr, "[stat] '%s' -> rc=%d size=%lld\n", p ? p : "?", rc, rc == 0 ? (long long)((struct stat64 *)st)->st_size : -1LL); n++; }
  }
  return rc;
}
static int b_fstat(int fd, void *st) {
  int rc = fstat64(fd, (struct stat64 *)st);
  if (getenv("NFS_SEEKLOG")) {
    static int n = 0;
    if (n < 30) { fprintf(stderr, "[fstat] fd=%d -> rc=%d size=%lld\n", fd, rc, rc == 0 ? (long long)((struct stat64 *)st)->st_size : -1LL); n++; }
  }
  return rc;
}
static int b_statfs(const char *p, void *st) { return statfs64(p, (struct statfs64 *)st); }

/* ---- seek hooks (NFS_SEEKLOG=1): ver o padrão de leitura do índice do OBB ----
 * Estado de I/O em globals p/ o crash handler imprimir onde a caminhada parou. */
long nfs_io_last_seek = -1;
long nfs_io_seeks = 0;
long nfs_io_read_bytes = 0;
static int my_fseek(void *fp, long off, int wh) {
  int rc = fseek((FILE *)fp, off, wh);
  nfs_io_last_seek = off; nfs_io_seeks++;
  if (getenv("NFS_SEEKLOG")) {
    static int n = 0;
    if (n < 60) { fprintf(stderr, "[fseek] fp=%p off=%ld wh=%d -> %d (pos=%ld)\n", fp, off, wh, rc, ftell((FILE *)fp)); n++; }
  }
  return rc;
}
static long my_ftell(void *fp) {
  long r = ftell((FILE *)fp);
  if (getenv("NFS_SEEKLOG")) {
    static int n = 0;
    if (n < 60) { fprintf(stderr, "[ftell] fp=%p -> %ld\n", fp, r); n++; }
  }
  return r;
}
static off_t my_lseek(int fd, off_t off, int wh) {
  off_t r = lseek(fd, off, wh);
  if (getenv("NFS_SEEKLOG")) {
    static int n = 0;
    if (n < 60) { fprintf(stderr, "[lseek] fd=%d off=%ld wh=%d -> %ld\n", fd, (long)off, wh, (long)r); n++; }
  }
  return r;
}
static long long my_lseek64(int fd, long long off, int wh) {
  long long r = lseek64(fd, off, wh);
  if (getenv("NFS_SEEKLOG")) {
    static int n = 0;
    if (n < 60) { fprintf(stderr, "[lseek64] fd=%d off=%lld wh=%d -> %lld\n", fd, off, wh, r); n++; }
  }
  return r;
}

static int (*real_open)(const char *, int, ...);
static int my_open(const char *path, int flags, ...) {
  if (!real_open) real_open = (int (*)(const char *, int, ...))dlsym(RTLD_DEFAULT, "open");
  int fd = real_open(path, flags, 0666);
  if (getenv("NFS_FOPENLOG")) {
    static int n = 0;
    if (n < 60) { fprintf(stderr, "[open] '%s' -> %s\n", path ? path : "?", fd >= 0 ? "OK" : "MISS"); n++; }
  }
  return fd;
}

/* ---- dlsym hook: a engine pode dlsym funções math em runtime e chamá-las
 * (softfp). Retornamos o wrapper softfp_resolve ANTES do dlsym real (senão
 * pega a versão HARDFP do glibc → crash). NFS_DLSYMLOG=1 loga. ---- */
extern void *softfp_resolve(const char *);
static void *(*real_dlsym)(void *, const char *);
static void *my_dlsym(void *handle, const char *name) {
  if (!real_dlsym) real_dlsym = (void *(*)(void *, const char *))dlsym(RTLD_DEFAULT, "dlsym");
  void *p = softfp_resolve(name);
  if (!p) p = real_dlsym(handle, name);
  if (getenv("NFS_DLSYMLOG")) {
    static int n = 0;
    if (n < 80) { fprintf(stderr, "[dlsym] '%s' -> %p%s\n", name ? name : "?", p, softfp_resolve(name) ? " (softfp)" : ""); n++; }
  }
  return p;
}

/* ---- __dynamic_cast SEGURO ----
 * A engine, no parse de asset, chama dynamic_cast em objetos cujo ponteiro de
 * vtable está corrompido/deslocado (vtable[-1]=typeinfo aponta p/ CÓDIGO em vez
 * de um __class_type_info). O __dynamic_cast da libc++ desreferencia esse
 * typeinfo lixo → blx num ponteiro inválido → SIGSEGV. Validamos a cadeia
 * sub→vtable→typeinfo→typeinfo_vtable[6] ANTES de delegar; se algo não for
 * legível, devolvemos NULL (= "cast falhou", resultado C++ válido). */
void *g_real_dynamic_cast;  /* setado em main.c c/ o __dynamic_cast da libc++ */

/* probe de legibilidade à prova de fault: arma g_probe_jmp, lê o 1º e o último
 * byte de [p,p+n); se faultar, o crash_handler dá siglongjmp de volta → 0. */
static int mem_readable(const void *p, size_t n) {
  if (!p || n == 0) return 0;
  if ((uintptr_t)p < 0x1000) return 0;
  volatile char sink;
  if (sigsetjmp(g_probe_jmp, 1) != 0) { g_probe_armed = 0; return 0; }
  g_probe_armed = 1;
  sink = ((const volatile char *)p)[0];
  sink = ((const volatile char *)p)[n - 1];
  (void)sink;
  g_probe_armed = 0;
  return 1;
}
/* faixas executáveis do processo (cache do /proc/self/maps) p/ validar que um
 * ponteiro de função é código real e não dados de asset (ex: ASCII "HT=1"). */
static struct { uintptr_t s, e; } g_xr[64];
static int g_xr_n = -1;
static void load_exec_ranges(void) {
  g_xr_n = 0;
  FILE *mf = fopen("/proc/self/maps", "r");
  if (!mf) return;
  char ln[512];
  while (fgets(ln, sizeof ln, mf) && g_xr_n < 64) {
    unsigned long s, e; char perm[8];
    if (sscanf(ln, "%lx-%lx %7s", &s, &e, perm) == 3 && perm[2] == 'x') {
      g_xr[g_xr_n].s = s; g_xr[g_xr_n].e = e; g_xr_n++;
    }
  }
  fclose(mf);
}
static int ptr_executable(uintptr_t p) {
  if (g_xr_n < 0) load_exec_ranges();
  for (int i = 0; i < g_xr_n; i++) if (p >= g_xr[i].s && p < g_xr[i].e) return 1;
  /* miss: regiões podem ter mudado (mmap novo) → recarrega 1x e retenta */
  load_exec_ranges();
  for (int i = 0; i < g_xr_n; i++) if (p >= g_xr[i].s && p < g_xr[i].e) return 1;
  return 0;
}

uintptr_t g_dyncast_tramp;  /* trampolim p/ rodar o __dynamic_cast original */
static void *my_dynamic_cast(const void *, const void *, const void *, long);

/* instala inline-hook na ENTRADA do __dynamic_cast da libc++ p/ que TODA chamada
 * (inclusive a recursão interna da libc++, que não passa pela GOT da libapp)
 * valide o typeinfo antes. Constrói trampolim com os 8 bytes originais. */
void nfs_install_dyncast_hook(void) {
  extern void hook_arm(uintptr_t, uintptr_t);
  if (!g_real_dynamic_cast) return;
  uintptr_t even = ((uintptr_t)g_real_dynamic_cast) & ~1u;
  uint8_t *tr = mmap(NULL, 64, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (tr == MAP_FAILED) return;
  memcpy(tr, (void *)even, 8);                  /* push{..};add r7;str.w r8 (PIC) */
  tr[8] = 0xDF; tr[9] = 0xF8; tr[10] = 0x00; tr[11] = 0xF0;  /* ldr.w pc,[pc,#0] */
  *(uint32_t *)(tr + 12) = (uint32_t)(even + 8) | 1u;        /* continua em +8 (Thumb) */
  __builtin___clear_cache((char *)tr, (char *)tr + 16);
  g_dyncast_tramp = (uintptr_t)tr | 1u;
  long pg = sysconf(_SC_PAGESIZE);
  uintptr_t pbase = even & ~((uintptr_t)pg - 1);
  mprotect((void *)pbase, pg * 2, PROT_READ | PROT_WRITE | PROT_EXEC);
  hook_arm(even | 1u, (uintptr_t)my_dynamic_cast);          /* entrada → nosso shim */
  __builtin___clear_cache((char *)even, (char *)even + 8);
  mprotect((void *)pbase, pg * 2, PROT_READ | PROT_EXEC);
  fprintf(stderr, "[dcast-hook] __dynamic_cast @%p hooked (tramp=%p)\n",
          (void *)even, (void *)g_dyncast_tramp);
}

static void *my_dynamic_cast(const void *sub, const void *src, const void *dst, long s2d) {
  int dbg = getenv("NFS_DCASTLOG") != NULL;
  static int dn = 0;
  if (!mem_readable(sub, 4)) goto fail;
  { const char *vt = *(const char *const *)sub;
    if (!mem_readable(vt - 8, 8)) goto fail;
    const void *ti = *(const void *const *)(vt - 4);
    if (!mem_readable(ti, 4)) goto fail;
    const char *ti_vt = *(const char *const *)ti;       /* vtable do próprio typeinfo */
    if (!mem_readable(ti_vt, 28)) goto fail;            /* dynamic_cast lê ti_vt[6] (+24) */
    /* o handler ti_vt[6] (offset 0x18) será chamado via blx — precisa ser CÓDIGO
     * real, não dados de asset (ex: ASCII "HT=1"=0x313d5448). */
    uintptr_t handler = *(const uintptr_t *)(ti_vt + 0x18);
    if (!ptr_executable(handler & ~1u)) {
      if (dbg && dn < 20) { fprintf(stderr, "[dcast] sub=%p ti=%p handler=%p NÃO-EXEC -> NULL\n", sub, ti, (void *)handler); dn++; }
      goto fail;
    }
  }
  if (g_dyncast_tramp) {
    void *(*f)(const void *, const void *, const void *, long) =
        (void *(*)(const void *, const void *, const void *, long))g_dyncast_tramp;
    return f(sub, src, dst, s2d);
  }
  if (g_real_dynamic_cast) {
    void *(*f)(const void *, const void *, const void *, long) =
        (void *(*)(const void *, const void *, const void *, long))g_real_dynamic_cast;
    return f(sub, src, dst, s2d);
  }
fail:
  if (getenv("NFS_DCASTLOG")) {
    static int n = 0;
    if (n < 20) { fprintf(stderr, "[dynamic_cast] sub=%p -> NULL (cadeia typeinfo inválida)\n", sub); n++; }
  }
  return 0;
}

/* ---- stubs ---- */
static int b_dso_handle;                       /* __dso_handle = endereço dummy */
static void *b_cxa_type_match(void *a, void *b, char c) { (void)a; (void)b; (void)c; return (void *)0; }
/* __sigsetjmp: declarado por <setjmp.h> (via so_util.h); bionic sigsetjmp == isso */
/* AndroidBitmap (jnigraphics) — stub: sinaliza erro p/ a engine cair no fallback */
static int abm_getInfo(void *env, void *bmp, void *info) { (void)env; (void)bmp; (void)info; return -1; }
static int abm_lock(void *env, void *bmp, void **pix) { (void)env; (void)bmp; if (pix) *pix = 0; return -1; }
static int abm_unlock(void *env, void *bmp) { (void)env; (void)bmp; return 0; }

DynLibFunction nfs_shims[] = {
    /* EGL → SDL2 (Mali fbdev): a engine cria contexto/surface via egl_shim */
    {"eglGetDisplay", (uintptr_t)egl_shim_GetDisplay},
    {"eglInitialize", (uintptr_t)egl_shim_Initialize},
    {"eglTerminate", (uintptr_t)egl_shim_Terminate},
    {"eglChooseConfig", (uintptr_t)egl_shim_ChooseConfig},
    {"eglGetConfigAttrib", (uintptr_t)egl_shim_GetConfigAttrib},
    {"eglCreateWindowSurface", (uintptr_t)egl_shim_CreateWindowSurface},
    {"eglCreatePbufferSurface", (uintptr_t)egl_shim_CreatePbufferSurface},
    {"eglDestroySurface", (uintptr_t)egl_shim_DestroySurface},
    {"eglCreateContext", (uintptr_t)egl_shim_CreateContext},
    {"eglDestroyContext", (uintptr_t)egl_shim_DestroyContext},
    {"eglMakeCurrent", (uintptr_t)egl_shim_MakeCurrent},
    {"eglSwapBuffers", (uintptr_t)egl_shim_SwapBuffers},
    {"eglSwapInterval", (uintptr_t)egl_shim_SwapInterval},
    {"eglGetCurrentContext", (uintptr_t)egl_shim_GetCurrentContext},
    {"eglGetCurrentSurface", (uintptr_t)egl_shim_GetCurrentSurface},
    {"eglGetError", (uintptr_t)egl_shim_GetError},
    {"eglQueryString", (uintptr_t)egl_shim_QueryString},
    {"eglQuerySurface", (uintptr_t)egl_shim_QuerySurface},
    {"eglBindAPI", (uintptr_t)egl_shim_BindAPI},
    {"eglSurfaceAttrib", (uintptr_t)egl_shim_SurfaceAttrib},
    {"eglGetProcAddress", (uintptr_t)egl_shim_GetProcAddress},
    {"pthread_create", (uintptr_t)my_pthread_create},
    {"dlsym", (uintptr_t)my_dlsym},
    {"fopen", (uintptr_t)my_fopen},
    {"fread", (uintptr_t)my_fread},
    {"open", (uintptr_t)my_open},
    {"stat", (uintptr_t)b_stat},
    {"fstat", (uintptr_t)b_fstat},
    {"statfs", (uintptr_t)b_statfs},
    {"fseek", (uintptr_t)my_fseek},
    {"ftell", (uintptr_t)my_ftell},
    {"lseek", (uintptr_t)my_lseek},
    {"lseek64", (uintptr_t)my_lseek64},
    {"malloc", (uintptr_t)pad_malloc},
    {"calloc", (uintptr_t)pad_calloc},
    {"realloc", (uintptr_t)pad_realloc},
    {"memalign", (uintptr_t)pad_memalign},
    {"__sF", (uintptr_t)bionic_sF},
    {"fprintf", (uintptr_t)w_fprintf}, {"vfprintf", (uintptr_t)w_vfprintf},
    {"fwrite", (uintptr_t)w_fwrite}, {"fputs", (uintptr_t)w_fputs},
    {"fputc", (uintptr_t)w_fputc}, {"fflush", (uintptr_t)w_fflush},
    {"__errno", (uintptr_t)b_errno},
    {"__assert2", (uintptr_t)b_assert2},
    {"__android_log_print", (uintptr_t)b_log_print},
    {"__android_log_write", (uintptr_t)b_log_write},
    {"__android_log_assert", (uintptr_t)b_log_assert},
    {"_ctype_", (uintptr_t)b_ctype},
    {"_tolower_tab_", (uintptr_t)b_tolower},
    {"_toupper_tab_", (uintptr_t)b_toupper},
    {"__dso_handle", (uintptr_t)&b_dso_handle},
    {"__cxa_type_match", (uintptr_t)b_cxa_type_match},
    {"sigsetjmp", (uintptr_t)__sigsetjmp},
    {"__dynamic_cast", (uintptr_t)my_dynamic_cast},
    {"AndroidBitmap_getInfo", (uintptr_t)abm_getInfo},
    {"AndroidBitmap_lockPixels", (uintptr_t)abm_lock},
    {"AndroidBitmap_unlockPixels", (uintptr_t)abm_unlock},
};
int nfs_shims_count = sizeof(nfs_shims) / sizeof(nfs_shims[0]);
