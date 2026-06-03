/* jni_shim.c -- fake JNI 64-bit p/ Bully (porta do bully_vita/jni_patch.c).
 * Offsets do JNINativeInterface = indice_spec * 8 (64-bit) = offset_vita * 2.
 * Input via SDL_GameController. */
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "so_util_x64.h"
#include "jni_shim.h"
#include "util.h"   /* ret0 */

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
  for (int i = 0; i < SDL_NumJoysticks(); i++)
    if (SDL_IsGameController(i)) { g_pad = SDL_GameControllerOpen(i); break; }
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
static void *nv_open(const char *p) { return asset_open(p); }
static size_t nv_read(void *buf, size_t s, size_t n, void *h) { return h ? asset_read(buf, s, n, h) : 0; }
static int   nv_seek(void *h, long o, int w) { return h ? asset_seek(h, o, w) : -1; }
static void  nv_close(void *h) { asset_close(h); }
static long  nv_tell(void *h) { return h ? asset_tell(h) : -1; }
static long  nv_size(void *h) { return h ? asset_size(h) : 0; }
static int   nv_eof(void *h) { return h ? asset_eof(h) : 1; }
static int   nv_getc(void *h) { return h ? asset_getc(h) : -1; }
static char *nv_gets(char *b, int m, void *h) { return h ? asset_gets(b, m, h) : NULL; }

/* EGL surface lifecycle: nos gerenciamos o pbuffer; neutraliza o create/destroy do jogo
 * (no PC o pbuffer nao pode ser destruido/recriado como window surface -> abortava) */
static void and_create_egl(void) { bully_make_current(); }
static void and_destroy_egl(void) { /* no-op */ }
static void os_thread_makecurrent(void) { bully_make_current(); }

static void hook_egl(void) {
  hook_x64(so_symbol(&mod_game, "_Z20AND_CreateEglSurfacev"), (uintptr_t)and_create_egl);
  hook_x64(so_symbol(&mod_game, "_Z21AND_DestroyEglSurfacev"), (uintptr_t)and_destroy_egl);
  hook_x64(so_symbol(&mod_game, "_Z20OS_ThreadMakeCurrentv"), (uintptr_t)os_thread_makecurrent);
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

  /* hooka NvAPK ANTES de qualquer init (asset reading vem dos data_*.zip) */
  hook_nvapk();
  hook_egl();
  asset_archive_init();

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
  if (OnResume) { fprintf(stderr, "[drv] implOnResume...\n"); OnResume(fake_env, NULL); }

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
