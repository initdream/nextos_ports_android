/* egl_shim.c -- EGL pbuffer offscreen (objetos EGL reais, sem atrito de janela X11/NVIDIA).
 * Bring-up: render vai pro pbuffer (nao visivel); prova o pipeline + handoff cross-thread.
 * (Depois: trocar pbuffer por window surface real p/ ver; no device Mali e fbdev EGL.) */
#define _GNU_SOURCE
#include <EGL/egl.h>
#include <stdio.h>
#include <stdint.h>

#ifndef DISPLAY_W
#define DISPLAY_W 1280
#endif
#ifndef DISPLAY_H
#define DISPLAY_H 720
#endif

static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLSurface g_surf = EGL_NO_SURFACE;
static EGLContext g_ctx = EGL_NO_CONTEXT;

int bully_init_gl(void) {
  if (g_ctx != EGL_NO_CONTEXT) return 1;
  g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (g_dpy == EGL_NO_DISPLAY) { fprintf(stderr, "[egl] GetDisplay falhou\n"); return 0; }
  EGLint maj, min;
  if (!eglInitialize(g_dpy, &maj, &min)) { fprintf(stderr, "[egl] Initialize 0x%x\n", eglGetError()); return 0; }
  eglBindAPI(EGL_OPENGL_ES_API);

  EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                        EGL_DEPTH_SIZE, 24, EGL_NONE };
  EGLConfig cfg; EGLint n;
  if (!eglChooseConfig(g_dpy, cfg_attr, &cfg, 1, &n) || n < 1) { fprintf(stderr, "[egl] ChooseConfig 0x%x\n", eglGetError()); return 0; }

  EGLint pb[] = { EGL_WIDTH, DISPLAY_W, EGL_HEIGHT, DISPLAY_H, EGL_NONE };
  g_surf = eglCreatePbufferSurface(g_dpy, cfg, pb);
  if (g_surf == EGL_NO_SURFACE) { fprintf(stderr, "[egl] CreatePbuffer 0x%x\n", eglGetError()); return 0; }

  EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
  g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
  if (g_ctx == EGL_NO_CONTEXT) { ctx_attr[1] = 2; g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctx_attr); }
  if (g_ctx == EGL_NO_CONTEXT) { fprintf(stderr, "[egl] CreateContext 0x%x\n", eglGetError()); return 0; }

  eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx);
  const char *(*gs)(unsigned) = (void*)eglGetProcAddress("glGetString");
  fprintf(stderr, "[egl] EGL %d.%d pbuffer %dx%d | dpy=%p surf=%p ctx=%p | GL=%s\n",
          maj, min, DISPLAY_W, DISPLAY_H, (void*)g_dpy, (void*)g_surf, (void*)g_ctx, gs ? gs(0x1F02) : "?");
  return 1;
}

void bully_egl_objects(uintptr_t *d, uintptr_t *s, uintptr_t *c) {
  if (d) *d = (uintptr_t)g_dpy; if (s) *s = (uintptr_t)g_surf; if (c) *c = (uintptr_t)g_ctx;
}
int  bully_make_current(void) { return eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx) ? 1 : 0; }
void bully_release_current(void) { eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT); }
void bully_swap_buffers(void) { if (g_dpy != EGL_NO_DISPLAY) eglSwapBuffers(g_dpy, g_surf); }
