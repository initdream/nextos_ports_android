#include <unistd.h>
#ifndef PORT_WINDOW_TITLE
#define PORT_WINDOW_TITLE "nextos_port"
#endif
/*
 * egl_shim.c -- EGL wrapper backed by SDL2 (OpenGL ES 2.0)
 *
 * Each fake EGL context gets a real SDL GL context. We keep a bootstrap
 * context around as the share root so all contexts can share resources.
 */

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "egl_shim.h"
#include "util.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

/* resolucao REAL da window (nativa do display) — Unity renderiza nessa res
   p/ preencher a tela toda. ANativeWindow_get{Width,Height} usam esses. */
static int g_win_w = SCREEN_WIDTH;
static int g_win_h = SCREEN_HEIGHT;
int egl_shim_width(void)  { return g_win_w; }
int egl_shim_height(void) { return g_win_h; }

typedef struct {
  SDL_GLContext sdl_context;
  EGLBoolean is_pbuffer;
  int id;
} _egl_context;

static SDL_Window *egl_window = NULL;
static SDL_GLContext egl_share_root = NULL;
static pthread_mutex_t egl_context_create_mutex = PTHREAD_MUTEX_INITIALIZER;
static int frame_count = 0;
static int next_context_id = 1;

static _Thread_local _egl_context *current_context = NULL;
static _Thread_local _egl_context *last_context = NULL;
static _Thread_local void *current_draw_surface = NULL; /* surface real do MakeCurrent */
static _Thread_local int has_real_gl = 0;

SDL_Window *egl_shim_get_window(void) { return egl_window; }

void egl_shim_create_window(void) {
  /* GLES3 no Mali-G310 (Amlogic-no) -> shaders GLES3 do HK rodam. Env HK_GLES2=1
     volta p/ GLES2 (Mali-450). */
  int gles_major = getenv("HK_GLES2") ? 2 : 3;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gles_major);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  /* resolucao nativa do display (TV) -> tela cheia real. Override: HK_W/HK_H. */
  SDL_DisplayMode dm;
  if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
    g_win_w = dm.w; g_win_h = dm.h;
  }
  if (getenv("HK_W")) g_win_w = atoi(getenv("HK_W"));
  if (getenv("HK_H")) g_win_h = atoi(getenv("HK_H"));

  egl_window = SDL_CreateWindow(
      PORT_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      g_win_w, g_win_h,
      SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
  if (!egl_window) {
    debugPrintf("egl_shim: SDL_CreateWindow FAILED: %s\n", SDL_GetError());
    return;
  }
  /* tamanho real do drawable (pode diferir do pedido no KMSDRM) */
  int dw = g_win_w, dh = g_win_h;
  SDL_GL_GetDrawableSize(egl_window, &dw, &dh);
  if (dw > 0 && dh > 0) { g_win_w = dw; g_win_h = dh; }
  debugPrintf("egl_shim: Window created %dx%d (nativo)\n", g_win_w, g_win_h);

  egl_share_root = SDL_GL_CreateContext(egl_window);
  if (!egl_share_root) {
    debugPrintf("egl_shim: SDL_GL_CreateContext FAILED: %s\n", SDL_GetError());
    return;
  }
  debugPrintf("egl_shim: GL share-root context created\n");

  SDL_GL_MakeCurrent(egl_window, NULL);
  debugPrintf("egl_shim: Context released, ready for game\n");
}

/* --- Mutex hooks (called from imports.c pthread wrappers) --- */

void egl_shim_on_mutex_post_lock(void *mutex_id) {
  (void)mutex_id;
}

void egl_shim_on_mutex_pre_unlock(void *mutex_id) {
  (void)mutex_id;
}

int egl_shim_ensure_current(void) {
  if (has_real_gl)
    return 1;
  _egl_context *ctx = current_context ? current_context : last_context;
  if (!egl_window || !ctx || !ctx->sdl_context)
    return 0;

  int ret = SDL_GL_MakeCurrent(egl_window, ctx->sdl_context);
  if (ret == 0) {
    has_real_gl = 1;
    current_context = ctx;
    debugPrintf("egl_shim: restored current context [tid=%lx] [ctx_id=%d]\n",
                (unsigned long)pthread_self(), ctx->id);
    return 1;
  }

  debugPrintf("egl_shim: failed to restore current context [tid=%lx] [ctx_id=%d]: %s\n",
              (unsigned long)pthread_self(), ctx->id, SDL_GetError());
  return 0;
}

/* --- EGL API --- */

EGLDisplay egl_shim_GetDisplay(EGLNativeDisplayType display_id) {
  (void)display_id;
  debugPrintf("egl_shim: eglGetDisplay()\n");
  return (EGLDisplay)strdup("display");
}

EGLBoolean egl_shim_Initialize(EGLDisplay dpy, EGLint *major, EGLint *minor) {
  (void)dpy;
  if (major) *major = 1;
  if (minor) *minor = 4;
  debugPrintf("egl_shim: eglInitialize() -> 1.4\n");
  return EGL_TRUE;
}

EGLBoolean egl_shim_Terminate(EGLDisplay dpy) {
  (void)dpy;
  debugPrintf("egl_shim: eglTerminate()\n");
  if (egl_share_root) {
    SDL_GL_DeleteContext(egl_share_root);
    egl_share_root = NULL;
  }
  if (egl_window) {
    SDL_DestroyWindow(egl_window);
    egl_window = NULL;
  }
  return EGL_TRUE;
}

EGLBoolean egl_shim_ChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                                  EGLConfig *configs, EGLint config_size,
                                  EGLint *num_config) {
  (void)dpy; (void)attrib_list;
  debugPrintf("egl_shim: eglChooseConfig()\n");
  if (configs && config_size > 0)
    configs[0] = (EGLConfig)strdup("config");
  if (num_config)
    *num_config = 1;
  return EGL_TRUE;
}

EGLSurface egl_shim_CreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                         EGLNativeWindowType win,
                                         const EGLint *attrib_list) {
  (void)dpy; (void)config; (void)win; (void)attrib_list;
  EGLSurface s = (EGLSurface)strdup("window");
  debugPrintf("egl_shim: eglCreateWindowSurface() -> %p\n", s);
  return s;
}

EGLSurface egl_shim_CreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
                                          const EGLint *attrib_list) {
  (void)dpy; (void)config; (void)attrib_list;
  EGLSurface s = (EGLSurface)strdup("pbuffer");
  debugPrintf("egl_shim: eglCreatePbufferSurface() -> %p\n", s);
  return s;
}

EGLContext egl_shim_CreateContext(EGLDisplay dpy, EGLConfig config,
                                  EGLContext share_context,
                                  const EGLint *attrib_list) {
  (void)dpy; (void)config; (void)share_context; (void)attrib_list;
  _egl_context *c = (_egl_context *)calloc(1, sizeof(_egl_context));
  if (!c)
    return EGL_NO_CONTEXT;

  pthread_mutex_lock(&egl_context_create_mutex);
  SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
  if (egl_share_root)
    SDL_GL_MakeCurrent(egl_window, egl_share_root);
  c->sdl_context = SDL_GL_CreateContext(egl_window);
  SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
  SDL_GL_MakeCurrent(egl_window, NULL);
  pthread_mutex_unlock(&egl_context_create_mutex);

  if (!c->sdl_context) {
    debugPrintf("egl_shim: eglCreateContext(share=%p) FAILED: %s\n",
                share_context, SDL_GetError());
    free(c);
    return EGL_NO_CONTEXT;
  }

  c->id = next_context_id++;
  debugPrintf("egl_shim: eglCreateContext(share=%p) -> %p [ctx_id=%d]\n",
              share_context, c, c->id);
  return (EGLContext)c;
}

EGLBoolean egl_shim_MakeCurrent(EGLDisplay dpy, EGLSurface draw,
                                 EGLSurface read, EGLContext ctx) {
  (void)dpy; (void)read;

  /* bionic le o stack-guard de tpidr_el0+40 (TLS slot 5); no glibc esse slot e'
     um campo de pthread que SDL/pthread mexem -> canario do Unity falha (falso).
     Salvamos e restauramos esse slot em volta do shim. */
  unsigned long tp; __asm__ volatile("mrs %0, tpidr_el0" : "=r"(tp));
  unsigned long *bguard = (unsigned long *)(tp + 40);
  unsigned long bguard_saved = *bguard;

  _egl_context *context = (_egl_context *)ctx;
  static _Thread_local int mc_count = 0;
  int mc = ++mc_count;
  (void)mc;

  /* === UNBIND === */
  if (context == NULL || draw == NULL) {
    current_context = NULL;
    current_draw_surface = NULL;
    if (egl_window) {
      SDL_GL_MakeCurrent(egl_window, NULL);
      /* debugPrintf("egl_shim: GL released [tid=%lx] reason=eglMakeCurrent(NULL)\n",
                    (unsigned long)pthread_self()); */
    }
    has_real_gl = 0;
    *bguard = bguard_saved;
    return EGL_TRUE;
  }

  int is_window = (((char *)draw)[0] == 'w');
  context->is_pbuffer = is_window ? EGL_FALSE : EGL_TRUE;
  current_context = context;
  last_context = context;
  current_draw_surface = draw;   /* p/ eglGetCurrentSurface bater no compare do Unity */

  if (!egl_window || !context->sdl_context) {
    *bguard = bguard_saved;
    return EGL_TRUE;
  }

  int ret = SDL_GL_MakeCurrent(egl_window, context->sdl_context);
  if (ret == 0) {
    has_real_gl = 1;
    static _Thread_local int acq_log = 0;
    if (acq_log < 20 || mc % 500 == 0) {
      //debugPrintf("egl_shim: MakeCurrent #%d %s [tid=%lx] ACQUIRED [ctx_id=%d]\n",
      //            mc, is_window ? "WINDOW" : "PBUFFER",
      //            (unsigned long)pthread_self(), context->id);
      acq_log++;
    }
  } else {
    has_real_gl = 0;
    debugPrintf("egl_shim: MakeCurrent #%d %s [tid=%lx] SDL FAILED [ctx_id=%d]: %s\n",
                mc, is_window ? "WINDOW" : "PBUFFER",
                (unsigned long)pthread_self(), context->id, SDL_GetError());
  }

  *bguard = bguard_saved;
  return EGL_TRUE;
}

EGLBoolean egl_shim_SwapBuffers(EGLDisplay dpy, EGLSurface surface) {
  (void)dpy; (void)surface;
  if (!egl_window) return EGL_TRUE;

  if (has_real_gl && current_context && !current_context->is_pbuffer) {
    SDL_GL_SwapWindow(egl_window);
    int fc = ++frame_count;
    if (fc <= 10 || fc % 60 == 0) {
      //debugPrintf("egl_shim: SwapBuffers #%d [tid=%lx]\n",
      //            fc, (unsigned long)pthread_self());
    }
  } else {
    static int noswap_log = 0;
    if (noswap_log < 3) {
      debugPrintf("egl_shim: SwapBuffers SKIPPED (no real GL) [tid=%lx]\n",
                  (unsigned long)pthread_self());
      noswap_log++;
    }
  }
  return EGL_TRUE;
}

EGLBoolean egl_shim_DestroySurface(EGLDisplay dpy, EGLSurface surface) {
  (void)dpy;
  free(surface);
  return EGL_TRUE;
}

EGLBoolean egl_shim_DestroyContext(EGLDisplay dpy, EGLContext ctx) {
  (void)dpy;
  _egl_context *context = (_egl_context *)ctx;
  if (context) {
    if (context->sdl_context)
      SDL_GL_DeleteContext(context->sdl_context);
    free(context);
  }
  return EGL_TRUE;
}

EGLBoolean egl_shim_QuerySurface(EGLDisplay dpy, EGLSurface surface,
                                  EGLint attribute, EGLint *value) {
  (void)dpy; (void)surface;
  if (attribute == 0x3057 && value) *value = g_win_w;       /* EGL_WIDTH */
  else if (attribute == 0x3056 && value) *value = g_win_h;  /* EGL_HEIGHT */
  return EGL_TRUE;
}

EGLBoolean egl_shim_GetConfigAttrib(EGLDisplay dpy, EGLConfig config,
                                     EGLint attribute, EGLint *value) {
  (void)dpy; (void)config;
  if (!value) return EGL_TRUE;
  switch (attribute) {
  case 0x3020: *value = 32; break;   /* EGL_BUFFER_SIZE */
  case 0x3021: *value = 8;  break;   /* EGL_ALPHA_SIZE */
  case 0x3022: *value = 8;  break;   /* EGL_BLUE_SIZE */
  case 0x3023: *value = 8;  break;   /* EGL_GREEN_SIZE */
  case 0x3024: *value = 8;  break;   /* EGL_RED_SIZE */
  case 0x3025: *value = 24; break;   /* EGL_DEPTH_SIZE */
  case 0x3026: *value = 8;  break;   /* EGL_STENCIL_SIZE */
  case 0x3027: *value = 0x3038; break; /* EGL_CONFIG_CAVEAT = EGL_NONE */
  case 0x3028: *value = 1;  break;   /* EGL_CONFIG_ID */
  case 0x302E: *value = 1;  break;   /* EGL_NATIVE_VISUAL_ID */
  case 0x3031: *value = 0;  break;   /* EGL_SAMPLES */
  case 0x3032: *value = 0;  break;   /* EGL_SAMPLE_BUFFERS */
  case 0x3033: *value = 0x0007; break; /* EGL_SURFACE_TYPE = WINDOW|PBUFFER|PIXMAP */
  case 0x303F: *value = 0x308E; break; /* EGL_COLOR_BUFFER_TYPE = EGL_RGB_BUFFER */
  case 0x3040: *value = 0x0044; break; /* EGL_RENDERABLE_TYPE = ES2|ES3 (Mali-G310 GLES3) */
  case 0x3042: *value = 0x0044; break; /* EGL_CONFORMANT = ES2|ES3 */
  default: *value = 0; break;
  }
  return EGL_TRUE;
}

EGLint egl_shim_GetError(void) { return EGL_SUCCESS; }

/* stub no-op p/ funcoes GL ausentes no Mali-450 (zera 8 bytes em ptr args comuns) */
static long gl_noop_stub(void) { return 0; }

void *egl_shim_GetProcAddress(const char *procname) {
  /* funcoes egl* -> NOSSOS shims (Unity pode pegar via procaddress; o Mali real
     falharia com nossos handles fake -> eglMakeCurrent FALSE -> erro -> smash). */
  if (procname && procname[0] == 'e' && procname[1] == 'g' && procname[2] == 'l') {
    debugPrintf("egl_shim: GetProcAddress(%s) -> shim\n", procname);
    if (!strcmp(procname, "eglMakeCurrent"))        return (void *)egl_shim_MakeCurrent;
    if (!strcmp(procname, "eglGetError"))           return (void *)egl_shim_GetError;
    if (!strcmp(procname, "eglGetCurrentSurface"))  return (void *)egl_shim_GetCurrentSurface;
    if (!strcmp(procname, "eglGetCurrentContext"))  return (void *)egl_shim_GetCurrentContext;
    if (!strcmp(procname, "eglGetCurrentDisplay"))  return (void *)egl_shim_GetDisplay;
    if (!strcmp(procname, "eglSwapBuffers"))        return (void *)egl_shim_SwapBuffers;
    if (!strcmp(procname, "eglGetDisplay"))         return (void *)egl_shim_GetDisplay;
    if (!strcmp(procname, "eglQueryString"))        return (void *)egl_shim_QueryString;
    if (!strcmp(procname, "eglSwapInterval"))       return (void *)egl_shim_SwapInterval;
    if (!strcmp(procname, "eglMakeCurrent"))        return (void *)egl_shim_MakeCurrent;
  }
  void *ptr = SDL_GL_GetProcAddress(procname);
  if (ptr) return ptr;

  size_t len = strlen(procname);
  if (len > 3 && strcmp(procname + len - 3, "OES") == 0) {
    char stripped[256];
    if (len - 3 < sizeof(stripped)) {
      memcpy(stripped, procname, len - 3);
      stripped[len - 3] = '\0';
      ptr = SDL_GL_GetProcAddress(stripped);
      if (ptr) return ptr;
    }
  }

  /* funcao GL ausente no Mali-450 (ex: GLES3 glGetInternalformativ): devolve um
     stub no-op em vez de NULL -> Unity nao crasha ao chamar o slot. */
  if (procname && procname[0] == 'g' && procname[1] == 'l') {
    debugPrintf("egl_shim: GL %s ausente -> stub no-op\n", procname);
    return (void *)gl_noop_stub;
  }
  debugPrintf("egl_shim: eglGetProcAddress(%s) -> NOT FOUND\n", procname);
  return NULL;
}

EGLBoolean egl_shim_BindAPI(unsigned int api) {
  (void)api;
  return EGL_TRUE;
}

const char *egl_shim_QueryString(EGLDisplay dpy, EGLint name) {
  (void)dpy;
  switch (name) {
  case 0x3053: return "NextOS";      /* EGL_VENDOR */
  case 0x3054: return "1.4 NextOS";  /* EGL_VERSION */
  case 0x3055: return "";            /* EGL_EXTENSIONS */
  case 0x308D: return "OpenGL_ES";   /* EGL_CLIENT_APIS */
  default: return "";
  }
}

EGLBoolean egl_shim_SwapInterval(EGLDisplay dpy, EGLint interval) {
  (void)dpy;
  SDL_GL_SetSwapInterval(interval);
  return EGL_TRUE;
}

EGLContext egl_shim_GetCurrentContext(void) {
  return (EGLContext)current_context;
}

EGLSurface egl_shim_GetCurrentSurface(EGLint readdraw) {
  (void)readdraw;
  /* DEVE retornar a surface real do ultimo MakeCurrent (Unity compara por ponteiro;
     literal fixo != strdup da surface -> Unity acha que o acquire falhou -> erro). */
  return (EGLSurface)current_draw_surface;
}

EGLBoolean egl_shim_SurfaceAttrib(EGLDisplay dpy, EGLSurface s, EGLint a,
                                  EGLint v) {
  (void)dpy; (void)s; (void)a; (void)v;
  return EGL_TRUE;
}

/* ===== Captura de input (SDL -> Android keycode) p/ nativeInjectEvent =====
   Android keycodes: DPAD_UP=19 DOWN=20 LEFT=21 RIGHT=22 CENTER=23 BACK=4
   ENTER=66 ESCAPE=111 SPACE=62 W=51 A=29 S=47 D=32 Z=54 X=52 C=31
   BUTTON_A=96 B=97 X=99 Y=100 L1=102 R1=103 START=108 SELECT=109 */
static SDL_GameController *g_pad = NULL;
void egl_shim_open_input(void) {
  /* O HK le /dev/input/event* DIRETO (Rewired). Se o SDL abrir o gamepad
     (GAMECONTROLLER), agarra o event do pad e o HK nao le. Por padrao NAO abrimos
     o pad no SDL — deixamos o HK ler o evdev. HK_SDLPAD=1 reativa (via inject). */
  if (!getenv("HK_SDLPAD")) {
    debugPrintf("egl_shim: SDL gamepad DESLIGADO (HK le evdev direto)\n");
    return;
  }
  SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
  for (int i = 0; i < SDL_NumJoysticks(); i++)
    if (SDL_IsGameController(i)) { g_pad = SDL_GameControllerOpen(i); if (g_pad) break; }
  debugPrintf("egl_shim: input aberto (pad=%p)\n", (void *)g_pad);
}
static int map_sdl_key(int sym) {
  switch (sym) {
    case SDLK_UP: return 19;  case SDLK_DOWN: return 20;
    case SDLK_LEFT: return 21; case SDLK_RIGHT: return 22;
    case SDLK_RETURN: case SDLK_KP_ENTER: return 66;
    case SDLK_ESCAPE: return 111; case SDLK_BACKSPACE: return 4;
    case SDLK_SPACE: return 62;
    case SDLK_w: return 51; case SDLK_a: return 29; case SDLK_s: return 47; case SDLK_d: return 32;
    case SDLK_z: return 54; case SDLK_x: return 52; case SDLK_c: return 31;
    case SDLK_v: return 50; case SDLK_f: return 34; case SDLK_e: return 33;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 59;
    case SDLK_TAB: return 61;
    default: return -1;
  }
}
static int map_sdl_button(int b) {
  switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return 19;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return 20;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return 21;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return 22;
    case SDL_CONTROLLER_BUTTON_A: return 96;
    case SDL_CONTROLLER_BUTTON_B: return 97;
    case SDL_CONTROLLER_BUTTON_X: return 99;
    case SDL_CONTROLLER_BUTTON_Y: return 100;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return 102;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return 103;
    case SDL_CONTROLLER_BUTTON_START: return 108;
    case SDL_CONTROLLER_BUTTON_BACK: return 109;
    default: return -1;
  }
}
/* pega o proximo evento de tecla mapeado; *src=0x101 teclado / 0x401 gamepad.
   retorna 1 se houve tecla, 0 se a fila esvaziou. */
int egl_shim_next_key(int *action, int *keycode, int *source) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
      if (e.type == SDL_KEYDOWN && e.key.repeat) continue;
      int kc = map_sdl_key(e.key.keysym.sym);
      if (kc < 0) continue;
      *action = (e.type == SDL_KEYDOWN) ? 0 : 1; *keycode = kc; *source = 0x101;
      return 1;
    }
    if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) {
      int kc = map_sdl_button(e.cbutton.button);
      if (kc < 0) continue;
      *action = (e.type == SDL_CONTROLLERBUTTONDOWN) ? 0 : 1; *keycode = kc; *source = 0x401;
      return 1;
    }
  }
  return 0;
}
