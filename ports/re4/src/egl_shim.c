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

typedef struct {
  SDL_GLContext sdl_context;
  EGLBoolean is_pbuffer;
  int id;
  unsigned long owner_tid; /* thread dona do SDL context (SDL-mali amarra a thread criadora) */
} _egl_context;

static SDL_Window *egl_window = NULL;
static SDL_GLContext egl_share_root = NULL;
static pthread_mutex_t egl_context_create_mutex = PTHREAD_MUTEX_INITIALIZER;
static int frame_count = 0;
static int next_context_id = 1;

static _Thread_local _egl_context *current_context = NULL;
static _Thread_local _egl_context *last_context = NULL;
static _Thread_local int has_real_gl = 0;

SDL_Window *egl_shim_get_window(void) { return egl_window; }

void egl_shim_create_window(void) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  egl_window = SDL_CreateWindow(
      PORT_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      SCREEN_WIDTH, SCREEN_HEIGHT,
      SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
  if (!egl_window) {
    debugPrintf("egl_shim: SDL_CreateWindow FAILED: %s\n", SDL_GetError());
    return;
  }
  debugPrintf("egl_shim: Window created %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);

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
  debugPrintf("egl_shim: eglGetDisplay() [tid=%lx]\n",(unsigned long)pthread_self());
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
  /* Unity 2018 valida o config por MATCH EXATO de cor (R/G/B/A/LUM) contra o pedido (de
     PlayerSettings, NAO no attrib_list). Em vez de adivinhar, devolvemos VARIOS configs com
     formatos comuns; o ponteiro codifica o indice (0xC0F00+i) e GetConfigAttrib devolve a cor
     daquele formato -> Unity acha o que casa. depth/stencil sao ">=" (24/8 cobrem). */
  int total = 6;
  if (num_config) *num_config = total;
  if (configs && config_size > 0) {
    int n = config_size < total ? config_size : total;
    for (int i = 0; i < n; i++) configs[i] = (EGLConfig)(uintptr_t)(0xC0F00 + i);
    if (num_config) *num_config = n;
  }
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
  /* NAO cria o SDL/EGL context aqui: o Unity cria contextos na MAIN thread mas RENDERIZA numa
     worker thread -> contexto preso a main -> eglMakeCurrent na worker = EGL_BAD_ACCESS. Adiamos
     a criacao do SDL context p/ o 1o MakeCurrent, na PROPRIA thread de render (sdl_context=NULL). */
  c->sdl_context = NULL;
  c->id = next_context_id++;
  debugPrintf("egl_shim: eglCreateContext -> ctx_id=%d [tid=%lx] (lazy)\n",
              c->id, (unsigned long)pthread_self());
  return (EGLContext)c;
}

EGLBoolean egl_shim_MakeCurrent(EGLDisplay dpy, EGLSurface draw,
                                 EGLSurface read, EGLContext ctx) {
  (void)dpy; (void)read;

  _egl_context *context = (_egl_context *)ctx;
  static _Thread_local int mc_count = 0;
  int mc = ++mc_count;

  /* === UNBIND === */
  if (context == NULL || draw == NULL) {
    current_context = NULL;
    if (egl_window) {
      SDL_GL_MakeCurrent(egl_window, NULL);
      /* debugPrintf("egl_shim: GL released [tid=%lx] reason=eglMakeCurrent(NULL)\n",
                    (unsigned long)pthread_self()); */
    }
    has_real_gl = 0;
    return EGL_TRUE;
  }

  int is_window = (((char *)draw)[0] == 'w');
  context->is_pbuffer = is_window ? EGL_FALSE : EGL_TRUE;
  current_context = context;
  last_context = context;

  if (!egl_window)
    return EGL_TRUE;

  /* SDL-mali amarra o SDL context a thread que o criou. O Unity cria o contexto na MAIN e
     RENDERIZA numa thread de gfx -> precisa MIGRAR: se o contexto nao existe OU pertence a outra
     thread, (re)cria nesta thread. A thread de render sustentado acaba dona dele. */
  unsigned long me = (unsigned long)pthread_self();
  if (!context->sdl_context || context->owner_tid != me) {
    pthread_mutex_lock(&egl_context_create_mutex);
    if (context->sdl_context) { SDL_GL_DeleteContext(context->sdl_context); context->sdl_context = NULL; }
    SDL_GL_MakeCurrent(egl_window, NULL);
    context->sdl_context = SDL_GL_CreateContext(egl_window);
    context->owner_tid = me;
    pthread_mutex_unlock(&egl_context_create_mutex);
    debugPrintf("egl_shim: (re)criou SDL context p/ ctx_id=%d [tid=%lx] -> %p\n",
                context->id, me, context->sdl_context);
    if (!context->sdl_context) { debugPrintf("egl_shim: SDL_GL_CreateContext FALHOU: %s\n", SDL_GetError()); return EGL_TRUE; }
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
  if (attribute == 0x3057 && value) *value = SCREEN_WIDTH;
  else if (attribute == 0x3056 && value) *value = SCREEN_HEIGHT;
  return EGL_TRUE;
}

EGLBoolean egl_shim_GetConfigAttrib(EGLDisplay dpy, EGLConfig config,
                                     EGLint attribute, EGLint *value) {
  (void)dpy;
  if (!value) return EGL_TRUE;
  int _logn=0; { static int n=0; _logn=(n++<120); }
  /* cor por-config: o ponteiro codifica o indice (0xC0F00+i). r/g/b/a do formato i. */
  static const int FMT[6][4] = {
    {8,8,8,8}, {8,8,8,0}, {5,6,5,0}, {4,4,4,4}, {5,5,5,1}, {8,8,8,0},
  };
  int idx = ((uintptr_t)config >= 0xC0F00 && (uintptr_t)config < 0xC0F00+6) ? (int)((uintptr_t)config - 0xC0F00) : 0;
  switch (attribute) {
  case 0x3020: *value = FMT[idx][0]; break; /* EGL_RED_SIZE */
  case 0x3021: *value = FMT[idx][1]; break; /* EGL_GREEN_SIZE */
  case 0x3022: *value = FMT[idx][2]; break; /* EGL_BLUE_SIZE */
  case 0x3023: *value = FMT[idx][3]; break; /* EGL_ALPHA_SIZE */
  case 0x3025: *value = 24; break;         /* EGL_DEPTH_SIZE */
  case 0x3026: *value = 8; break;          /* EGL_STENCIL_SIZE */
  case 0x3024: *value = 8; break;          /* EGL_LUMINANCE_SIZE -- Unity pede match exato
                                              (LUM,ALPHA,BLUE,GREEN)=(8,8,8,8); idx0=RGBA8888 +
                                              este LUM=8 fecha o match -> config aceito */
  case 0x3027: *value = 0x3038; break;     /* EGL_CONFIG_CAVEAT = EGL_NONE */
  case 0x3028: *value = idx + 1; break;    /* EGL_CONFIG_ID (unico por config) */
  case 0x3031: *value = 0; break;          /* EGL_SAMPLE_BUFFERS */
  case 0x3032: *value = 0; break;          /* EGL_SAMPLES */
  case 0x3033: *value = 0x0004; break;     /* EGL_SURFACE_TYPE = EGL_WINDOW_BIT */
  case 0x3040: *value = 0x0004; break;     /* EGL_RENDERABLE_TYPE = ES2 SO (4). Unity tenta
                                              ES3.2/3.1/3.0/ES2 em sequencia; anunciar so ES2 faz
                                              ele REJEITAR as ES3 e ACEITAR a ES2 -> usa contexto
                                              ES2 (Mali-450 e ES2-only; ES3 crashava). */
  case 0x3042: *value = 1; break;          /* EGL_BIND_TO_TEXTURE_RGB */
  case 0x3039: *value = 0x0004; break;     /* EGL_CONFORMANT = ES2 */
  case 0x302E: *value = 0; break;          /* EGL_NATIVE_VISUAL_ID */
  case 0x3034: *value = 0; break;          /* EGL_TRANSPARENT_TYPE = EGL_NONE */
  case 0x30e2: *value = 0x30e3; break;     /* Unity 2018 config-match: aceita se 0x30e2==0x30e3 */
  case 0x30e1: *value = 0; break;
  default: *value = 0; break;
  }
  if(_logn) debugPrintf("egl_shim: GetConfigAttrib(0x%x) = %d\n", attribute, *value);
  return EGL_TRUE;
}

EGLint egl_shim_GetError(void) { return EGL_SUCCESS; }

void *egl_shim_GetProcAddress(const char *procname) {
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
  return (EGLSurface)"window";
}

EGLBoolean egl_shim_SurfaceAttrib(EGLDisplay dpy, EGLSurface s, EGLint a,
                                  EGLint v) {
  (void)dpy; (void)s; (void)a; (void)v;
  return EGL_TRUE;
}
