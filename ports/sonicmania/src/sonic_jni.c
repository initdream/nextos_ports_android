/* sonic_jni.c — JNI driver estático do Sonic Mania (RSDKv5).
 * Fabrica um JNIEnv/JavaVM falsos e chama o fluxo nativo:
 *   JNI_OnLoad -> OpenGLNativeCalls_startEngine -> loop OpenGLNativeCalls_step.
 * Molde: ports/bully/src/jni_shim.c. Os métodos do JNIEnv começam em ret0;
 * a gente adiciona os que o engine chamar (RE iterativo: run->crash->add). */
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include "so_util.h"
#include "util.h" /* ret0 */
#include <string.h>

static char fake_env[0x1000];
static char fake_vm[0x100];
static SDL_Window *g_win;
static SDL_GLContext g_ctx;
static char fake_thiz[8192];
static const char *g_datapath = "/storage/roms/ports/sonicmania";
static const char *g_methods[512];
static int g_nmethods;
static int method_id(const char *n) {
  for (int i = 0; i < g_nmethods; i++) if (strcmp(g_methods[i], n) == 0) return i + 1;
  g_methods[g_nmethods] = strdup(n); return ++g_nmethods;
}
static const char *method_name(int id) { return (id >= 1 && id <= g_nmethods) ? g_methods[id - 1] : "?"; }

/* ---- métodos JNIEnv que o engine chama de volta (stubs; logam p/ a RE) ---- */
static void *j_FindClass(void *e, const char *n) {
  (void)e; fprintf(stderr, "[jni] FindClass %s\n", n ? n : "?");
  return (void *)(uintptr_t)(0x1000 + method_id(n ? n : "?"));
}
static int j_GetMethodID(void *e, void *c, const char *n, const char *s) {
  (void)e; (void)c; (void)s; return method_id(n ? n : "?");
}
static int j_GetStaticMethodID(void *e, void *c, const char *n, const char *s) {
  (void)e; (void)c; fprintf(stderr, "[jni] GetStaticMethodID %s %s\n", n ? n : "?", s ? s : "?"); return 1;
}
static void j_CallVoidMethod(void *e, void *o, int id, ...) {
  (void)e; (void)o; fprintf(stderr, "[jni] CallVoid %s\n", method_name(id));
}
static int j_CallIntMethod(void *e, void *o, int id, ...) {
  (void)e; (void)o; const char *n = method_name(id);
  fprintf(stderr, "[jni] CallInt %s\n", n);
  if (strstr(n, "Width")) return 1280;
  if (strstr(n, "Height")) return 720;
  return 0;
}
static int j_CallBooleanMethod(void *e, void *o, int id, ...) {
  (void)e; (void)o; const char *n = method_name(id);
  int r = 0;
  if (strstr(n, "isRestartRequired")) r = 0;       /* NAO reiniciar */
  else if (strstr(n, "supported") || strstr(n, "Available") || strstr(n, "Enabled")) r = 0;
  fprintf(stderr, "[jni] CallBool %s -> %d\n", n, r); return r;
}
static void *j_CallObjectMethod(void *e, void *o, int id, ...) {
  (void)e; (void)o; const char *n = method_name(id);
  fprintf(stderr, "[jni] CallObject %s\n", n);
  if (strcmp(n, "getClassLoader") == 0) return (void *)0x100;
  if (strcmp(n, "loadClass") == 0 || strcmp(n, "findClass") == 0) return (void *)0x200;
  return (void *)0x1;
}
static const char *j_GetStringUTFChars(void *e, void *str, void *isCopy) {
  (void)e; if (isCopy) *(char *)isCopy = 0; return (const char *)str;
}
static void *j_NewStringUTF(void *e, const char *s) { (void)e; return (void *)s; }
static int j_GetEnv(void *vm, void **env, int v) { (void)vm; (void)v; *env = fake_env; return 0; }
static int j_AttachCurrentThread(void *vm, void **env, void *a) { (void)vm; (void)a; *env = fake_env; return 0; }

static void *j_NewRef(void *e, void *obj) { (void)e; return obj; }
static void *j_GetObjectClass(void *e, void *obj) { (void)e; (void)obj; return (void *)0x1; }

static void build_env(void) {
  extern long (*g_jlog[256])(void);
  for (unsigned i = 0; i < 256; i++)
    ((uintptr_t *)fake_env)[i] = (uintptr_t)g_jlog[i];
  *(uintptr_t *)(fake_env + 0x00) = (uintptr_t)fake_env;
#define SET(off, fn) *(uintptr_t *)(fake_env + (off)) = (uintptr_t)(fn)
  SET(0x30, j_FindClass);          /* idx 6 */
  SET(0xA8, j_NewRef);             /* idx 21 NewGlobalRef */
  SET(0xB0, ret0);                 /* idx 22 DeleteGlobalRef */
  SET(0xB8, ret0);                 /* idx 23 DeleteLocalRef */
  SET(0xC8, j_NewRef);             /* idx 25 NewLocalRef */
  SET(0xF8, j_GetObjectClass);     /* idx 31 GetObjectClass */
  SET(0x108, j_GetMethodID);       /* idx 33 */
  SET(0x110, j_CallObjectMethod);  /* idx 34 */
  SET(0x188, j_CallIntMethod);     /* idx 49 */
  SET(0x1C8, j_CallBooleanMethod); /* idx ~ */
  SET(0x1E8, j_CallVoidMethod);    /* idx 61 */
  SET(0x1C0, j_GetStaticMethodID); /* aprox; ajusta na RE */
  SET(0x538, j_NewStringUTF);      /* idx 167 */
  SET(0x548, j_GetStringUTFChars); /* idx 169 */
  SET(0x118, j_CallObjectMethod);  /* idx 35 CallObjectMethodV */
  SET(0x190, j_CallIntMethod);     /* idx 50 CallIntMethodV */
  SET(0x1D0, j_CallBooleanMethod); /* CallBooleanMethodV aprox */
  SET(0x1F0, j_CallVoidMethod);    /* idx 62 CallVoidMethodV */
  SET(0x388, j_GetMethodID);       /* idx 113 GetStaticMethodID */
  SET(0x390, j_CallObjectMethod);  /* idx 114 CallStaticObjectMethod */
  SET(0x398, j_CallObjectMethod);  /* idx 115 CallStaticObjectMethodV */
  SET(0x3a8, j_CallBooleanMethod); /* idx 117 CallStaticBooleanMethod */
  SET(0x3b0, j_CallBooleanMethod); /* idx 118 CallStaticBooleanMethodV */
  SET(0x418, j_CallIntMethod);     /* CallStaticIntMethodV aprox */
  SET(0x468, j_CallVoidMethod);    /* idx 141 CallStaticVoidMethod */
  SET(0x470, j_CallVoidMethod);    /* idx 142 CallStaticVoidMethodV */
#undef SET
}

void jni_run(void) {
  /* contexto GLES2 (no Android o Java cria; aqui criamos via SDL2->Mali fbdev) */
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  g_win = SDL_CreateWindow("Sonic Mania", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
  if (!g_win) fprintf(stderr, "[gl] CreateWindow FALHOU: %s\n", SDL_GetError());
  g_ctx = SDL_GL_CreateContext(g_win);
  if (!g_ctx) fprintf(stderr, "[gl] CreateContext FALHOU: %s\n", SDL_GetError());
  SDL_GL_MakeCurrent(g_win, g_ctx);
  fprintf(stderr, "[gl] contexto GLES2 win=%p ctx=%p\n", (void *)g_win, (void *)g_ctx);

  build_env();
  for (unsigned i = 0; i < sizeof(fake_vm) / sizeof(uintptr_t); i++)
    ((uintptr_t *)fake_vm)[i] = (uintptr_t)ret0;
  *(uintptr_t *)(fake_vm + 0x00) = (uintptr_t)fake_vm;
  *(uintptr_t *)(fake_vm + 0x20) = (uintptr_t)j_AttachCurrentThread; /* idx 4 */
  *(uintptr_t *)(fake_vm + 0x30) = (uintptr_t)j_GetEnv;              /* idx 6 */
  *(uintptr_t *)(fake_vm + 0x38) = (uintptr_t)j_AttachCurrentThread; /* idx 7 */

  uintptr_t onload = so_find_addr_safe("JNI_OnLoad");
  if (onload) {
    int v = ((int (*)(void *, void *))onload)(fake_vm, NULL);
    fprintf(stderr, "[drv] JNI_OnLoad => 0x%x\n", v);
  } else {
    fprintf(stderr, "[drv] JNI_OnLoad nao achado\n");
  }

  uintptr_t se = so_find_addr_safe("Java_com_netflix_NGP_SonicMania_OpenGLNativeCalls_startEngine");
  uintptr_t st = so_find_addr_safe("Java_com_netflix_NGP_SonicMania_OpenGLNativeCalls_step");
  fprintf(stderr, "[drv] startEngine=0x%lx step=0x%lx\n", (unsigned long)se, (unsigned long)st);
  if (!se) {
    fprintf(stderr, "[drv] startEngine NAO achado — abortando\n");
    return;
  }
  fprintf(stderr, "[drv] chamando startEngine...\n");
  ((void (*)(void *, void *, int, int, void *, const char *, int, void *,
              int, int, int, int, int, int, int, int))se)(
      fake_env, fake_thiz, 1280, 720, fake_thiz, g_datapath, 0, fake_thiz,
      1280, 720, 424, 240, 60, 1, 0, 0);
  fprintf(stderr, "[drv] startEngine retornou\n");
  uintptr_t sgr = so_find_addr_safe("Java_com_netflix_NGP_SonicMania_MainActivity_setGameRunning");
  if (sgr) { fprintf(stderr, "[drv] setGameRunning(1)\n"); ((void(*)(void*,void*,int))sgr)(fake_env, fake_thiz, 1); }
  fprintf(stderr, "[drv] entrando no loop step\n");
  for (long f = 0; st; f++) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
      if (ev.type == SDL_QUIT) return;
    ((void (*)(void *, void *, float))st)(fake_env, fake_thiz, 60.0f);
    { extern int g_drawcount; static int last=0;
      if (f%30==0) { fprintf(stderr, "[loop] frame %ld draws=%d (+%d) glErr=0x%x\n", f, g_drawcount, g_drawcount-last, glGetError()); last=g_drawcount; } }
    SDL_GL_SwapWindow(g_win);
  }
}
