/* jni_shim.c -- fake JNI 64-bit p/ Bully (porta do bully_vita/jni_patch.c).
 * Offsets do JNINativeInterface = indice_spec * 8 (64-bit) = offset_vita * 2.
 * Input via SDL_GameController. */
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "so_util_x64.h"
#include "jni_shim.h"
#include "util.h"   /* ret0 */
#include "zip_fs.h"

extern Module mod_game;
extern void bully_swap_buffers(void);  /* egl_shim */
extern int  bully_init_gl(void);       /* egl_shim */
extern int  bully_make_current(void);
extern void bully_release_current(void);
extern void bully_egl_objects(uintptr_t *d, uintptr_t *s, uintptr_t *c);

#define DATA_PATH "."   /* STORAGE_ROOT (ajustar p/ dir dos assets/OBB) */

enum {
  UNKNOWN = 0, INIT_EGL_AND_GLES2, SWAP_BUFFERS, MAKE_CURRENT, UN_MAKE_CURRENT,
  SHARE_TEXT, SHARE_IMAGE,
  HAS_APP_LOCAL_VALUE, GET_APP_LOCAL_VALUE, SET_APP_LOCAL_VALUE, GET_PARAMETER,
  FILE_GET_ARCHIVE_NAME, DELETE_FILE,
  GET_DEVICE_INFO, GET_DEVICE_TYPE, GET_DEVICE_LOCALE,
  GET_GAMEPAD_TYPE, GET_GAMEPAD_BUTTONS, GET_GAMEPAD_AXIS,
  ROCKSTAR_SHOW_INITIAL, ROCKSTAR_SHOW_GATE,
};
static struct { const char *name; int id; } method_ids[] = {
  {"rockstarShowInitial", ROCKSTAR_SHOW_INITIAL}, {"rockstarShowGate", ROCKSTAR_SHOW_GATE},
  {"InitEGLAndGLES2", INIT_EGL_AND_GLES2}, {"swapBuffers", SWAP_BUFFERS},
  {"makeCurrent", MAKE_CURRENT}, {"unMakeCurrent", UN_MAKE_CURRENT},
  {"ShareText", SHARE_TEXT}, {"ShareImage", SHARE_IMAGE},
  {"hasAppLocalValue", HAS_APP_LOCAL_VALUE}, {"getAppLocalValue", GET_APP_LOCAL_VALUE},
  {"setAppLocalValue", SET_APP_LOCAL_VALUE}, {"getParameter", GET_PARAMETER},
  {"FileGetArchiveName", FILE_GET_ARCHIVE_NAME}, {"DeleteFile", DELETE_FILE},
  {"GetDeviceInfo", GET_DEVICE_INFO}, {"GetDeviceType", GET_DEVICE_TYPE},
  {"GetDeviceLocale", GET_DEVICE_LOCALE},
  {"GetGamepadType", GET_GAMEPAD_TYPE}, {"GetGamepadButtons", GET_GAMEPAD_BUTTONS},
  {"GetGamepadAxis", GET_GAMEPAD_AXIS},
};

static char fake_vm[0x1000];
static char fake_env[0x1000];
static void *natives;
static SDL_GameController *g_pad;

/* ---- métodos "Java" que o jogo chama de volta ---- */
static int GetDeviceType(void) { return (2048 << 6) | (3 << 2) | 0x1; } /* mem|tegra3|phone */
static int swapBuffers(void) { bully_swap_buffers(); return 1; }
static int InitEGLAndGLES2(void) { return bully_init_gl(); }
static char *getAppLocalValue(char *key) {
  if (key && strcmp(key, "STORAGE_ROOT") == 0) return (char *)DATA_PATH;
  return NULL;
}
static int hasAppLocalValue(char *key) { return (key && strcmp(key, "STORAGE_ROOT") == 0) ? 1 : 0; }
static void setAppLocalValue(char *k, char *v) { fprintf(stderr, "[jni] setAppLocalValue %s=%s\n", k?k:"?", v?v:"?"); }
static char *getParameter(char *key) { return NULL; }
static char *FileGetArchiveName(int type) {
  if (type == 1) return (char *)"main.obb";
  if (type == 2) return (char *)"patch.obb";
  return NULL;
}
static int GetGamepadType(int port) { return port == 0 ? 8 : -1; } /* PS3 */
static int GetGamepadButtons(int port) {
  if (port != 0 || !g_pad) return 0;
  SDL_GameControllerUpdate();
  int m = 0;
  struct { int b; int mask; } map[] = {
    {SDL_CONTROLLER_BUTTON_A,0x1},{SDL_CONTROLLER_BUTTON_B,0x2},
    {SDL_CONTROLLER_BUTTON_X,0x4},{SDL_CONTROLLER_BUTTON_Y,0x8},
    {SDL_CONTROLLER_BUTTON_START,0x10},{SDL_CONTROLLER_BUTTON_BACK,0x20},
    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,0x40},{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0x80},
    {SDL_CONTROLLER_BUTTON_DPAD_UP,0x100},{SDL_CONTROLLER_BUTTON_DPAD_DOWN,0x200},
    {SDL_CONTROLLER_BUTTON_DPAD_LEFT,0x400},{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,0x800},
    {SDL_CONTROLLER_BUTTON_LEFTSTICK,0x1000},{SDL_CONTROLLER_BUTTON_RIGHTSTICK,0x2000},
  };
  for (unsigned i = 0; i < sizeof(map)/sizeof(map[0]); i++)
    if (SDL_GameControllerGetButton(g_pad, map[i].b)) m |= map[i].mask;
  static int last_m = 0, calls = 0;
  if (calls < 3) { fprintf(stderr, "[pad] GetGamepadButtons CHAMADO (poll #%d) m=0x%x\n", calls, m); calls++; }
  if (m != last_m) { fprintf(stderr, "[pad] buttons=0x%x\n", m); last_m = m; }
  return m;
}
static float GetGamepadAxis(int port, int axis) {
  if (port != 0 || !g_pad) return 0.0f;
  SDL_GameControllerAxis ax[] = {SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_AXIS_LEFTY,
    SDL_CONTROLLER_AXIS_RIGHTX,SDL_CONTROLLER_AXIS_RIGHTY,
    SDL_CONTROLLER_AXIS_TRIGGERLEFT,SDL_CONTROLLER_AXIS_TRIGGERRIGHT};
  if (axis < 0 || axis > 5) return 0.0f;
  float v = SDL_GameControllerGetAxis(g_pad, ax[axis]) / 32768.0f;
  return fabsf(v) > 0.25f ? v : 0.0f;
}

/* ---- pump de eventos de controle (o jogo NÃO faz polling; usa eventos JNI,
 * igual bully-NX). GamepadButton enum do libGame: 0=A 1=B 2=X 3=Y 4=START
 * 5=BACK 6=L3 7=R3 8-11=NAV(menu) 12-15=DPAD 16=LB 17=LT 18=RB 19=RT. ---- */
static const struct { int sdl; int game; } g_btnmap[] = {
  {SDL_CONTROLLER_BUTTON_A,0},{SDL_CONTROLLER_BUTTON_B,1},
  {SDL_CONTROLLER_BUTTON_X,2},{SDL_CONTROLLER_BUTTON_Y,3},
  {SDL_CONTROLLER_BUTTON_START,4},{SDL_CONTROLLER_BUTTON_BACK,5},
  {SDL_CONTROLLER_BUTTON_LEFTSTICK,6},{SDL_CONTROLLER_BUTTON_RIGHTSTICK,7},
  {SDL_CONTROLLER_BUTTON_DPAD_UP,8},{SDL_CONTROLLER_BUTTON_DPAD_DOWN,9},
  {SDL_CONTROLLER_BUTTON_DPAD_LEFT,10},{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,11},
  {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,16},{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,18},
};
static void pump_gamepad(void) {
  static void (*down)(void*,void*,int,int) = NULL;
  static void (*up)(void*,void*,int,int) = NULL;
  static void (*axesfn)(void*,void*,int,float,float,float,float,float,float) = NULL;
  static void (*countfn)(void*,void*,int) = NULL;
  static int inited = 0, last[20] = {0};
  static float la[6] = {0};
  if (!g_pad) return;
  if (!inited) {
#define GP(n) (void*)so_symbol(&mod_game, "Java_com_rockstargames_oswrapper_GameNative_" n)
    down = GP("implOnGamepadButtonDown"); up = GP("implOnGamepadButtonUp");
    axesfn = GP("implOnGamepadAxesChanged"); countfn = GP("implOnGamepadCountChanged");
#undef GP
    if (countfn) countfn(fake_env, NULL, 1); /* avisa: 1 controle conectado */
    inited = 1;
  }
  SDL_GameControllerUpdate();
  /* botões */
  for (unsigned i = 0; i < sizeof(g_btnmap)/sizeof(g_btnmap[0]); i++) {
    int g = g_btnmap[i].game;
    int p = SDL_GameControllerGetButton(g_pad, g_btnmap[i].sdl) ? 1 : 0;
    if (p != last[g]) {
      if (p) { if (down) down(fake_env, NULL, 0, g); }
      else   { if (up)   up(fake_env, NULL, 0, g); }
      last[g] = p;
    }
  }
  /* gatilhos como botões 17(LT)/19(RT) */
  int lt = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT)  > 12000 ? 1 : 0;
  int rt = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000 ? 1 : 0;
  if (lt != last[17]) { if (lt) { if(down) down(fake_env,NULL,0,17);} else if(up) up(fake_env,NULL,0,17); last[17]=lt; }
  if (rt != last[19]) { if (rt) { if(down) down(fake_env,NULL,0,19);} else if(up) up(fake_env,NULL,0,19); last[19]=rt; }
  /* eixos (sticks) */
  float a[6];
  a[0] = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX)/32768.0f;
  a[1] = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY)/32768.0f;
  a[2] = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTX)/32768.0f;
  a[3] = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTY)/32768.0f;
  a[4] = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT)/32768.0f;
  a[5] = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT)/32768.0f;
  int ch = 0;
  for (int i = 0; i < 6; i++) if (fabsf(a[i]-la[i]) > 0.02f) { ch = 1; break; }
  if (ch && axesfn) { axesfn(fake_env, NULL, 0, a[0],a[1],a[2],a[3],a[4],a[5]); for(int i=0;i<6;i++) la[i]=a[i]; }
}

/* ---- dispatchers JNI ---- */
static int GetMethodID(void *e, void *c, const char *name, const char *sig) {
  for (unsigned i = 0; i < sizeof(method_ids)/sizeof(method_ids[0]); i++)
    if (strcmp(name, method_ids[i].name) == 0) return method_ids[i].id;
  /* desconhecido: retorna ID NAO-ZERO (o jogo faz `if(methodID)` antes de chamar;
     se 0, pula a chamada e usa lixo/NULL -> crash, ex: OS_GetAppVersion). */
  return 0x7777;
}
static int CallBooleanMethodV(void *e, void *o, int id, va_list a) {
  switch (id) {
    case INIT_EGL_AND_GLES2: return InitEGLAndGLES2();
    case SWAP_BUFFERS: return swapBuffers();
    case MAKE_CURRENT: return bully_make_current();
    case UN_MAKE_CURRENT: bully_release_current(); return 1;
    case HAS_APP_LOCAL_VALUE: return hasAppLocalValue(va_arg(a, char *));
    case DELETE_FILE: return 0;
  }
  return 0;
}
static float CallFloatMethodV(void *e, void *o, int id, va_list a) {
  if (id == GET_GAMEPAD_AXIS) { int p = va_arg(a, int); int ax = va_arg(a, int); return GetGamepadAxis(p, ax); }
  return 0.0f;
}
static int CallIntMethodV(void *e, void *o, int id, va_list a) {
  switch (id) {
    case GET_GAMEPAD_TYPE: return GetGamepadType(va_arg(a, int));
    case GET_GAMEPAD_BUTTONS: return GetGamepadButtons(va_arg(a, int));
    case GET_DEVICE_TYPE: return GetDeviceType();
    case GET_DEVICE_INFO: case GET_DEVICE_LOCALE: return 0;
  }
  return 0;
}
static void *CallObjectMethodV(void *e, void *o, int id, va_list a) {
  switch (id) {
    case GET_APP_LOCAL_VALUE: { char *r = getAppLocalValue(va_arg(a, char *)); return r ? r : (void*)""; }
    case GET_PARAMETER: { char *r = getParameter(va_arg(a, char *)); return r ? r : (void*)""; }
    case FILE_GET_ARCHIVE_NAME: { char *r = FileGetArchiveName(va_arg(a, int)); return r ? r : (void*)""; }
  }
  return (void*)"";  /* string vazia em vez de NULL: evita strlen(NULL) no jogo */
}
volatile int g_rk_pending_initial = 0, g_rk_pending_gate = 0, g_rk_pending_gate_type = 0;
static void CallVoidMethodV(void *e, void *o, int id, va_list a) {
  if (id == SET_APP_LOCAL_VALUE) { char *k = va_arg(a, char *); char *v = va_arg(a, char *); setAppLocalValue(k, v); }
  else if (id == ROCKSTAR_SHOW_INITIAL) { g_rk_pending_initial = 1; fprintf(stderr, "[jni] rockstarShowInitial -> pending\n"); }
  else if (id == ROCKSTAR_SHOW_GATE) { g_rk_pending_gate_type = va_arg(a, int); g_rk_pending_gate = 1; fprintf(stderr, "[jni] rockstarShowGate -> pending\n"); }
}
static void *FindClass(void *e, const char *n) { return (void *)0x41414141; }
static void *NewGlobalRef(void *e, void *o) { return o ? o : (void *)0x42424242; }
static char *NewStringUTF(void *e, char *b) { return b ? b : (char *)""; }
static char *GetStringUTFChars(void *e, char *s, int *c) { if (c) *c = 0; return s ? s : (char *)""; }
static void RegisterNatives(void *e, void *cls, void *methods, int n) {
  natives = methods;
  fprintf(stderr, "[jni] RegisterNatives: %d metodos\n", n);
  struct JNM { const char *name; const char *sig; void *fn; } *m = methods;
  for (int i = 0; i < n && i < 8; i++)
    fprintf(stderr, "   [%d] %s %s -> %p\n", i, m[i].name, m[i].sig, m[i].fn);
}
void *NVThreadGetCurrentJNIEnv(void) { return fake_env; }

/* variantes varargs (...Method) — o jogo usa AMBAS; delegam pras ...MethodV */
static void *CallObjectMethod(void *e, void *o, int id, ...) { va_list a; va_start(a, id); void *r = CallObjectMethodV(e, o, id, a); va_end(a); return r; }
static int CallBooleanMethod(void *e, void *o, int id, ...) { va_list a; va_start(a, id); int r = CallBooleanMethodV(e, o, id, a); va_end(a); return r; }
static int CallIntMethod(void *e, void *o, int id, ...) { va_list a; va_start(a, id); int r = CallIntMethodV(e, o, id, a); va_end(a); return r; }
static float CallFloatMethod(void *e, void *o, int id, ...) { va_list a; va_start(a, id); float r = CallFloatMethodV(e, o, id, a); va_end(a); return r; }
static void CallVoidMethod(void *e, void *o, int id, ...) { va_list a; va_start(a, id); CallVoidMethodV(e, o, id, a); va_end(a); }

static int GetEnv(void *vm, void **env, int v) { *env = fake_env; return 0; }
static int AttachCurrentThread(void *vm, void **env, void *args) { *env = fake_env; return 0; }

#define SET(off, fn) *(uintptr_t *)(fake_env + (off)) = (uintptr_t)(fn)
static void build_env(void) {
  /* preenche TUDO com ret0 (qualquer slot JNI nao-tratado retorna 0, sem crash) */
  for (unsigned i = 0; i < sizeof(fake_env)/sizeof(uintptr_t); i++)
    ((uintptr_t *)fake_env)[i] = (uintptr_t)ret0;
  *(uintptr_t *)(fake_env + 0x00) = (uintptr_t)fake_env;
  SET(0x30, FindClass);            /* idx 6 */
  SET(0x88, ret0);                 /* idx 17 ExceptionClear */
  SET(0xA8, NewGlobalRef);         /* idx 21 */
  SET(0xB0, ret0);                 /* idx 22 DeleteGlobalRef */
  SET(0xB8, ret0);                 /* idx 23 DeleteLocalRef */
  SET(0x108, GetMethodID);         /* idx 33 */
  SET(0x110, CallObjectMethod);    /* idx 34 (varargs) */
  SET(0x118, CallObjectMethodV);   /* idx 35 */
  SET(0x128, CallBooleanMethod);   /* idx 37 (varargs) */
  SET(0x130, CallBooleanMethodV);  /* idx 38 */
  SET(0x188, CallIntMethod);       /* idx 49 (varargs) */
  SET(0x190, CallIntMethodV);      /* idx 50 */
  SET(0x1B8, CallFloatMethod);     /* idx 55 (varargs) */
  SET(0x1C0, CallFloatMethodV);    /* idx 56 */
  SET(0x1E8, CallVoidMethod);      /* idx 61 (varargs) */
  SET(0x1F0, CallVoidMethodV);     /* idx 62 */
  SET(0x538, NewStringUTF);        /* idx 167 */
  SET(0x548, GetStringUTFChars);   /* idx 169 */
  SET(0x550, ret0);                /* idx 170 ReleaseStringUTFChars */
  SET(0x6B8, RegisterNatives);     /* idx 215 */
}

void jni_init_input(void) {
  int n = SDL_NumJoysticks();
  fprintf(stderr, "[pad] SDL_NumJoysticks=%d\n", n);
  for (int i = 0; i < n; i++) {
    fprintf(stderr, "[pad]  js%d: \"%s\" isGameController=%d\n",
            i, SDL_JoystickNameForIndex(i), SDL_IsGameController(i));
    if (SDL_IsGameController(i) && !g_pad) {
      g_pad = SDL_GameControllerOpen(i);
      fprintf(stderr, "[pad]  -> abriu como GameController: %s\n", g_pad ? "OK" : SDL_GetError());
    }
  }
  /* fallback: se nenhum tem mapeamento, abre o 1º joystick como pad genérico */
  if (!g_pad && n > 0) {
    SDL_GameControllerAddMapping(
      "03000000000000000000000000000000,USB Gamepad,"
      "a:b2,b:b1,x:b3,y:b0,start:b9,back:b8,"
      "leftshoulder:b4,rightshoulder:b5,"
      "dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,"
      "leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,");
    g_pad = SDL_GameControllerOpen(0);
    fprintf(stderr, "[pad]  fallback genérico: %s\n", g_pad ? "OK" : SDL_GetError());
  }
}

/* ---- NvAPK hooks -> asset_archive (le dos data_*.zip reais) ---- */
extern int   asset_archive_init(void);
extern void *asset_open(const char *path);
extern void  asset_close(void *h);
extern size_t asset_read(void *buf, size_t s, size_t n, void *h);
extern int   asset_seek(void *h, long off, int wh);
extern long  asset_tell(void *h);
extern long  asset_size(void *h);
extern int   asset_eof(void *h);
extern int   asset_getc(void *h);
extern char *asset_gets(char *b, int m, void *h);

static int   nv_init(void *a, void *b, void *c) { asset_archive_init(); return 0; }
static void *nv_open(const char *p) {
  void *h = asset_open(p);
  fprintf(stderr, "[nvapk] open \"%s\" -> %s\n", p ? p : "(null)", h ? "OK" : "MISS");
  return h;
}
static int g_nvdbg = 0; /* loga as primeiras N chamadas p/ ver o padrão do loop */
static size_t nv_read(void *buf, size_t s, size_t n, void *h) {
  size_t r = h ? asset_read(buf, s, n, h) : 0;
  if (g_nvdbg < 90) { fprintf(stderr, "[nv] read h=%p s=%zu n=%zu -> %zu\n", h, s, n, r); g_nvdbg++; }
  return r;
}
static int   nv_seek(void *h, long o, int w) {
  int r = h ? asset_seek(h, o, w) : -1;
  if (g_nvdbg < 90) { fprintf(stderr, "[nv] seek h=%p o=%ld w=%d -> %d\n", h, o, w, r); g_nvdbg++; }
  return r;
}
static void  nv_close(void *h) { asset_close(h); }
static long  nv_tell(void *h) {
  long r = h ? asset_tell(h) : -1;
  if (g_nvdbg < 90) { fprintf(stderr, "[nv] tell h=%p -> %ld\n", h, r); g_nvdbg++; }
  return r;
}
static long  nv_size(void *h) {
  long r = h ? asset_size(h) : 0;
  if (g_nvdbg < 90) { fprintf(stderr, "[nv] size h=%p -> %ld\n", h, r); g_nvdbg++; }
  return r;
}
static int   nv_eof(void *h) { return h ? asset_eof(h) : 1; }
static int   nv_getc(void *h) { return h ? asset_getc(h) : -1; }
static char *nv_gets(char *b, int m, void *h) { return h ? asset_gets(b, m, h) : NULL; }

/* EGL surface lifecycle: nos gerenciamos o pbuffer; neutraliza o create/destroy do jogo
 * (no PC o pbuffer nao pode ser destruido/recriado como window surface -> abortava) */
static void and_create_egl(void) { bully_make_current(); }
static void and_destroy_egl(void) { /* no-op */ }
static void os_thread_makecurrent(void) {
  int ok = bully_make_current();
  fprintf(stderr, "[gl] OS_ThreadMakeCurrent tid=%lu -> eglMakeCurrent ok=%d\n",
          (unsigned long)pthread_self(), ok);
}
/* SOLTA o NOSSO contexto na thread chamadora — pareia com makecurrent. Sem isso
 * o GameMain segura o ctx (EGL é single-thread) e a render thread falha (ok=0). */
static void os_thread_unmakecurrent(void) {
  bully_release_current();
  fprintf(stderr, "[gl] OS_ThreadUnmakeCurrent tid=%lu -> released\n",
          (unsigned long)pthread_self());
}

static void hook_egl(void) {
  hook_x64(so_symbol(&mod_game, "_Z20AND_CreateEglSurfacev"), (uintptr_t)and_create_egl);
  hook_x64(so_symbol(&mod_game, "_Z21AND_DestroyEglSurfacev"), (uintptr_t)and_destroy_egl);
  hook_x64(so_symbol(&mod_game, "_Z20OS_ThreadMakeCurrentv"), (uintptr_t)os_thread_makecurrent);
  hook_x64(so_symbol(&mod_game, "_Z22OS_ThreadUnmakeCurrentv"), (uintptr_t)os_thread_unmakecurrent);
}

/* ---- hooks de tela/render como FUNÇÃO (bully-NX hooka; nós só setávamos flags
 * srp). GameRenderer::Setup pode dimensionar a textura/fbo pela tela -> se
 * Width/Height retornam 0, a whitetexture sai 0x0 e falha (NULL). ---- */
static int os_screen_w(void) { return 1280; }
static int os_screen_h(void) { return 720; }
static int os_can_render(void) { return 1; }
static int os_is_suspended(void) { return 0; }
static void hook_screen(void) {
  hook_x64(so_symbol(&mod_game, "_Z17OS_ScreenGetWidthv"), (uintptr_t)os_screen_w);
  hook_x64(so_symbol(&mod_game, "_Z18OS_ScreenGetHeightv"), (uintptr_t)os_screen_h);
  hook_x64(so_symbol(&mod_game, "_Z16OS_CanGameRenderv"), (uintptr_t)os_can_render);
  hook_x64(so_symbol(&mod_game, "_Z18OS_IsGameSuspendedv"), (uintptr_t)os_is_suspended);
}

/* ---- __cxa_guard: o guard de static do jogo (NDK) pode travar/falhar no
 * ambiente so-loader (futex/pthread) -> statics C++ não inicializam (ex: o
 * registro dos recursos default / whitetexture). Substituímos por uma versão
 * simples correta (Itanium ABI: byte 0 = inicializado), igual bully-NX. ---- */
static int my_cxa_guard_acquire(char *g) { return g && *g == 0; }
static void my_cxa_guard_release(char *g) { if (g) *g = 1; }
static void my_cxa_guard_abort(char *g) { (void)g; }
static void hook_cxa(void) {
  hook_x64(so_symbol(&mod_game, "__cxa_guard_acquire"), (uintptr_t)my_cxa_guard_acquire);
  hook_x64(so_symbol(&mod_game, "__cxa_guard_release"), (uintptr_t)my_cxa_guard_release);
  hook_x64(so_symbol(&mod_game, "__cxa_guard_abort"), (uintptr_t)my_cxa_guard_abort);
}

/* ---- ASYNC FILE WORKER (porta do bully-NX) — A PEÇA QUE FALTAVA ----
 * No Android os arquivos carregam ASSÍNCRONO: uma fila (AndroidFile::
 * firstAsyncFile) é avançada por AND_FileUpdated(delta) a cada frame por um
 * worker. Sem ele, os loads de recurso (incl. a whitetexture / default
 * resources) NUNCA completam -> GameRenderer::Setup pega NULL -> crash. */
static void (*g_AND_FileUpdated)(double) = NULL;
static volatile uintptr_t *g_first_async = NULL;
static void *async_file_worker(void *a) {
  (void)a;
  for (;;) {
    if (g_AND_FileUpdated && g_first_async &&
        __atomic_load_n(g_first_async, __ATOMIC_ACQUIRE))
      g_AND_FileUpdated(0.002);
    else
      usleep(2000);
  }
  return NULL;
}
static void start_async_file_worker(void) {
  g_AND_FileUpdated = (void (*)(double))so_symbol(&mod_game, "_Z14AND_FileUpdated");
  g_first_async =
      (volatile uintptr_t *)so_symbol(&mod_game, "_ZN11AndroidFile14firstAsyncFileE");
  fprintf(stderr, "[async] AND_FileUpdated=%p firstAsyncFile=%p\n",
          (void *)g_AND_FileUpdated, (void *)g_first_async);
  if (g_AND_FileUpdated && g_first_async) {
    pthread_t t;
    if (pthread_create(&t, NULL, async_file_worker, NULL) == 0) {
      pthread_detach(t);
      fprintf(stderr, "[async] worker started\n");
    }
  }
}

/* ---- orquestração de thread (porta do bully-NX, adaptada x86_64) ----
 * O engine lê handle[0x69]=running (OS_ThreadIsRunning) e handle[0x28]=pthread_t
 * (OS_ThreadWait). Sem gerenciar isso, o sync de thread quebra e o renderer/
 * init de recursos default (whitetexture) não fica pronto -> GameRenderer::Setup
 * pega ResourceManager::Get<Texture2D>("whitetexture")=NULL -> SIGSEGV.
 * No x86_64 o pthread do host já dá TLS (não precisa do armSetTlsRw do Switch). */
volatile int g_gamemain_alive = 0; /* 0=não iniciou 1=rodando 2=retornou */
typedef struct { unsigned (*func)(void *); void *arg; char *handle; int is_gm; } OsThreadData;

static void *os_thread_entry(void *p) {
  OsThreadData *td = p;
  unsigned (*func)(void *) = td->func;
  void *arg = td->arg;
  char *h = td->handle;
  int gm = td->is_gm;
  free(td);
  if (h) h[0x69] = 1;
  if (gm) g_gamemain_alive = 1;
  int ret = func ? (int)func(arg) : 0;
  if (h) h[0x69] = 0;
  if (gm) g_gamemain_alive = 2;
  return (void *)(intptr_t)ret;
}

static void *my_OS_ThreadLaunch(unsigned (*func)(void *), void *arg, unsigned r2,
                                const char *name, int r4, int prio) {
  (void)r2; (void)r4; (void)prio;
  char *h = calloc(1, 0x400); /* handle grande p/ a struct privada do engine */
  if (!h) return NULL;
  OsThreadData *td = malloc(sizeof(*td));
  td->func = func; td->arg = arg; td->handle = h;
  td->is_gm = (name && strcmp(name, "GameMain") == 0);
  pthread_t t;
  if (pthread_create(&t, NULL, os_thread_entry, td) != 0) { free(td); free(h); return NULL; }
  h[0x69] = 1;                       /* OS_ThreadIsRunning */
  memcpy(h + 0x28, &t, sizeof(t));   /* OS_ThreadWait/join */
  fprintf(stderr, "[thr] OS_ThreadLaunch '%s' -> handle=%p\n", name ? name : "?", (void *)h);
  return h;
}

static void my_OS_ThreadWait(void *thread) {
  if (!thread) return;
  pthread_t t;
  memcpy(&t, (char *)thread + 0x28, sizeof(t));
  pthread_join(t, NULL);
}

/* bypassa o wrapper de thread JNI do jogo (crasha em pthread_getspecific null) */
static int my_NVThreadSpawnJNIThread(long *out, const void *attr, const char *name,
                                     void *(*entry)(void *), void *arg) {
  (void)attr; (void)name;
  if (!entry) return -1;
  pthread_t t;
  int rc = pthread_create(&t, NULL, entry, arg);
  if (rc == 0 && out)
    memcpy(out, &t, sizeof(*out) < sizeof(t) ? sizeof(*out) : sizeof(t));
  return rc;
}

static void hook_threads(void) {
  hook_x64(so_symbol(&mod_game, "_Z15OS_ThreadLaunchPFjPvES_jPKci16OSThreadPriority"),
           (uintptr_t)my_OS_ThreadLaunch);
  hook_x64(so_symbol(&mod_game, "_Z13OS_ThreadWaitPv"), (uintptr_t)my_OS_ThreadWait);
  hook_x64(so_symbol(&mod_game, "_Z22NVThreadSpawnJNIThreadPlPK14pthread_attr_tPKcPFPvS5_ES5_"),
           (uintptr_t)my_NVThreadSpawnJNIThread);
}

static void hook_nvapk(void) {
#define HK(sym, fn) hook_x64(so_symbol(&mod_game, sym), (uintptr_t)(fn))
  HK("_Z9NvAPKInitP8_jobjectP13_jobjectArrayS2_", nv_init);
  HK("_Z9NvAPKOpenPKc", nv_open);
  HK("_Z17NvAPKOpenFromPackPKc", nv_open);
  HK("_Z9NvAPKReadPvmmS_", nv_read);
  HK("_Z9NvAPKSeekPvli", nv_seek);
  HK("_Z10NvAPKClosePv", nv_close);
  HK("_Z9NvAPKTellPv", nv_tell);
  HK("_Z9NvAPKSizePv", nv_size);
  HK("_Z8NvAPKEOFPv", nv_eof);
  HK("_Z9NvAPKGetcPv", nv_getc);
  HK("_Z9NvAPKGetsPciPv", nv_gets);
#undef HK
}

void jni_load(void) {
  build_env();
  for (unsigned i = 0; i < sizeof(fake_vm)/sizeof(uintptr_t); i++)
    ((uintptr_t *)fake_vm)[i] = (uintptr_t)ret0;
  *(uintptr_t *)(fake_vm + 0x00) = (uintptr_t)fake_vm;
  *(uintptr_t *)(fake_vm + 0x20) = (uintptr_t)AttachCurrentThread;  /* idx 4 */
  *(uintptr_t *)(fake_vm + 0x30) = (uintptr_t)GetEnv;               /* idx 6 */
  *(uintptr_t *)(fake_vm + 0x38) = (uintptr_t)AttachCurrentThread;  /* idx 7 daemon */

  /* hooks patcham a TEXT do libGame (que está RX após so_finalize) -> torna
   * gravável durante o hooking, depois volta p/ executável (device AArch64). */
  so_make_text_writable();
  /* hooka NvAPK ANTES de qualquer init (asset reading vem dos data_*.zip) */
  hook_nvapk();
  hook_egl();
  hook_threads(); /* gerência de thread Switch-safe -> destrava GameMain/whitetexture */
  hook_screen();  /* OS_ScreenGetWidth/Height + render gates como função */
  hook_cxa();     /* __cxa_guard simples -> statics C++ (whitetexture?) inicializam */
  so_make_text_executable();
  so_flush_caches();
  asset_archive_init();
  zip_fs_init(); /* serve recursos (whitetexture etc) de DENTRO dos data zips via fopen */

  /* resolve as funcoes nativas estaticas (JNI estatico, v1.4.311) */
#define R(n) so_symbol(&mod_game, "Java_com_rockstargames_oswrapper_GameNative_" n)
  void (*OnInitialSetup)(void*,void*,void*,void*,void*,void*) = (void*)R("implOnInitialSetup");
  void (*OnActivityCreated)(void*,void*,void*,int) = (void*)R("implOnActivityCreated");
  void (*OnSurfaceCreated)(void*,void*) = (void*)R("implOnSurfaceCreated");
  void (*OnSurfaceChanged)(void*,void*,void*,int,int) = (void*)R("implOnSurfaceChanged");
  void (*OnDrawFrame)(void*,void*,float) = (void*)R("implOnDrawFrame");
  void (*OnResume)(void*,void*) = (void*)R("implOnResume");
#undef R
  fprintf(stderr, "[drv] impl*: setup=%p act=%p surfC=%p surfCh=%p draw=%p resume=%p\n",
          OnInitialSetup, OnActivityCreated, OnSurfaceCreated, OnSurfaceChanged, OnDrawFrame, OnResume);

  /* gate flags ancorados em StorageRootPath (igual bully-NX) */
  uintptr_t srp = so_symbol(&mod_game, "StorageRootPath");
  volatile uint8_t *isInit   = srp ? (volatile uint8_t*)(srp - 0x174) : NULL;
  volatile uint8_t *suspended= srp ? (volatile uint8_t*)(srp - 0x17c) : NULL;
  volatile uint8_t *canRender= srp ? (volatile uint8_t*)(srp - 0x2e8) : NULL;
  fprintf(stderr, "[drv] StorageRootPath=%p\n", (void*)srp);
  if (suspended) *suspended = 0;

  /* JNI_OnLoad primeiro (registra a VM no jogo) */
  int (*JNI_OnLoad)(void*,void*) = (void*)so_symbol(&mod_game, "JNI_OnLoad");
  fprintf(stderr, "[drv] JNI_OnLoad => 0x%x\n", JNI_OnLoad(fake_vm, NULL));

  if (!OnInitialSetup) { fprintf(stderr, "[drv] ERRO: implOnInitialSetup nao achado\n"); return; }
  fprintf(stderr, "[drv] implOnInitialSetup...\n");
  OnInitialSetup(fake_env, NULL, NULL, NULL, NULL, NULL);
  fprintf(stderr, "[drv] implOnInitialSetup OK\n");

  /* registra os data zips (o jogo exporta OS_ZipAdd p/ o launcher chamar; libGame nao chama sozinho).
     Sem isso, OS_ZipFileOpen itera registro vazio -> ZIPFile::Find(NULL) -> crash no GameMain. */
  void (*OS_ZipAdd)(const char *) = (void *)so_symbol(&mod_game, "_Z9OS_ZipAddPKc");
  if (OS_ZipAdd) {
    fprintf(stderr, "[drv] OS_ZipAdd data_0.zip / data_1.zip ...\n");
    OS_ZipAdd("data_0.zip");
    OS_ZipAdd("data_1.zip");
    fprintf(stderr, "[drv] OS_ZipAdd OK\n");
  }

  if (isInit && *isInit != 1) *isInit = 1;
  if (suspended) *suspended = 0;
  if (canRender) *canRender = 1;
  fprintf(stderr, "[drv] gates: init=%d susp=%d render=%d\n",
          isInit?*isInit:-1, suspended?*suspended:-1, canRender?*canRender:-1);

  fprintf(stderr, "[drv] implOnActivityCreated...\n");
  if (OnActivityCreated) OnActivityCreated(fake_env, NULL, (void*)0x42424242, 1);
  fprintf(stderr, "[drv] implOnActivityCreated OK\n");

  /* contexto GL (EGL real) + sincroniza nos globais OS_EGL* do jogo */
  bully_init_gl();
  /* inicializa + abre o controle (SDL gamecontroller) — sem isso g_pad=NULL e
   * GetGamepadButtons/Axis sempre retornam 0 (jni_init_input nunca era chamado) */
  if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0)
    fprintf(stderr, "[pad] InitSubSystem: %s\n", SDL_GetError());
  jni_init_input();
  uintptr_t egl_d=0, egl_s=0, egl_c=0; bully_egl_objects(&egl_d, &egl_s, &egl_c);
  volatile uintptr_t *OS_EGLDisplay = srp ? (volatile uintptr_t*)(srp - 0x2d0) : NULL;
  volatile uintptr_t *OS_EGLSurface = srp ? (volatile uintptr_t*)(srp - 0x2c8) : NULL;
  volatile uintptr_t *OS_EGLContext = srp ? (volatile uintptr_t*)(srp - 0x2c0) : NULL;
  if (OS_EGLDisplay) *OS_EGLDisplay = egl_d;
  if (OS_EGLSurface) *OS_EGLSurface = egl_s;
  if (OS_EGLContext) *OS_EGLContext = egl_c;
  fprintf(stderr, "[drv] OS_EGL globals: d=%p s=%p c=%p\n", (void*)egl_d, (void*)egl_s, (void*)egl_c);

  /* solta o contexto do main ANTES das surfaces — a render thread do jogo pega via makeCurrent */
  bully_release_current();

  if (OnSurfaceCreated) { fprintf(stderr, "[drv] implOnSurfaceCreated...\n"); OnSurfaceCreated(fake_env, NULL); }
  if (OnSurfaceChanged) { fprintf(stderr, "[drv] implOnSurfaceChanged 1280x720...\n"); OnSurfaceChanged(fake_env, NULL, NULL, 1280, 720); }
  /* RE-SEED dos EGL globals após surface-changed (o engine reseta OS_EGLSurface
   * aqui; sem re-seed a render thread fica sem contexto -> whitetexture NULL).
   * Igual bully-NX sync_engine_egl_globals("post-surface-changed"). */
  if (OS_EGLDisplay) *OS_EGLDisplay = egl_d;
  if (OS_EGLSurface) *OS_EGLSurface = egl_s;
  if (OS_EGLContext) *OS_EGLContext = egl_c;
  fprintf(stderr, "[drv] OS_EGL globals RE-SEED pós-surface: d=%p s=%p c=%p\n",
          (void*)egl_d, (void*)egl_s, (void*)egl_c);
  if (OnResume) { fprintf(stderr, "[drv] implOnResume...\n"); OnResume(fake_env, NULL); }
  start_async_file_worker(); /* processa a fila de loads async -> recursos/whitetexture carregam */

  /* callbacks Rockstar (gate online): no Android vem async do Java; aqui disparamos no loop */
  void (*OS_StateChanged)(int) = (void*)so_symbol(&mod_game, "_Z25OS_OnRockstarStateChangedb");
  void (*OS_InitialComplete)(void) = (void*)so_symbol(&mod_game, "_Z28OS_OnRockstarInitialCompletev");
  void (*OS_GateComplete)(int,int) = (void*)so_symbol(&mod_game, "_Z25OS_OnRockstarGateCompleteib");
  void (*OS_SignInComplete)(void) = (void*)so_symbol(&mod_game, "_Z27OS_OnRockstarSignInCompletev");
  void (*OS_AppEvent)(int,void*) = (void*)so_symbol(&mod_game, "_Z19OS_ApplicationEvent11OSEventTypePv");
  void (*OnRkSetup)(void*,void*,void*,void*) = (void*)so_symbol(&mod_game, "Java_com_rockstargames_oswrapper_GameNative_implOnRockstarSetup");

  /* loop de render */
  fprintf(stderr, "[drv] -- loop implOnDrawFrame --\n");
  extern volatile int g_rk_pending_initial, g_rk_pending_gate, g_rk_pending_gate_type;
  int rk_fired = 0, rk_signin = 0;
  for (int f = 0; OnDrawFrame; f++) {
    SDL_Event e; while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) return;
    pump_gamepad(); /* empurra eventos de controle pro jogo (Down/Up/Axes) */
    if (canRender) *canRender = 1;

    /* completa o gate Rockstar (igual bully-NX) */
    if (!rk_fired && (g_rk_pending_initial || g_rk_pending_gate) && f > 30) {
      rk_fired = 1; int gt = g_rk_pending_gate ? g_rk_pending_gate_type : 0;
      fprintf(stderr, "[drv] === ROCKSTAR COMPLETE (frame %d type %d) ===\n", f, gt);
      if (OS_StateChanged) OS_StateChanged(0);
      if (OS_InitialComplete) OS_InitialComplete();
      if (OS_GateComplete) OS_GateComplete(gt, 1);
      if (OS_AppEvent) OS_AppEvent(9, NULL); /* OSET_Resume */
      if (OnRkSetup) OnRkSetup(fake_env, NULL, (void*)"pc_user", (void*)"pc_ticket");
      if (canRender) *canRender = 1; if (suspended) *suspended = 0; if (isInit) *isInit = 1;
      g_rk_pending_initial = g_rk_pending_gate = 0; rk_signin = 1;
    }
    if (rk_signin && f > 45) { rk_signin = 0; if (OS_SignInComplete) OS_SignInComplete(); }

    OnDrawFrame(fake_env, NULL, 1.0f/60.0f);  /* heartbeat; GL real ocorre na render thread do jogo */
    if (f < 5 || f % 120 == 0) fprintf(stderr, "[drv] frame %d\n", f);
    SDL_Delay(16);
  }
}
