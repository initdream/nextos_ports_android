/* egl_shim.c (DEVICE) -- EGL real no Mali-450 fbdev (Utgard).
 * Igual ao egl_shim do PC, mas com WINDOW surface (fbdev_window -> /dev/fb0 =
 * TV) em vez de pbuffer. Os imports egl* do jogo (via dlsym -> libMali.m450)
 * usam ESTE mesmo contexto/display -> consistente. Render VISÍVEL na TV.
 * API bully_* idêntica ao PC -> jni_shim não muda. */
#define _GNU_SOURCE
#include <EGL/egl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* fbdev_window (Mali fbdev / Utgard) já vem de <EGL/fbdev_window.h> via egl.h */
static fbdev_window g_win;
static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLSurface g_surf = EGL_NO_SURFACE;
static EGLContext g_ctx = EGL_NO_CONTEXT;
static int g_w = 1280, g_h = 720;

/* lê a resolução do framebuffer (/sys/class/graphics/fb0/virtual_size) */
static void read_fb_res(void) {
  FILE *f = fopen("/sys/class/graphics/fb0/virtual_size", "r");
  if (f) {
    int w, h;
    if (fscanf(f, "%d,%d", &w, &h) == 2 && w > 0 && h > 0) { g_w = w; g_h = h; }
    fclose(f);
  }
  if (g_w > 1920) g_w = 1920;  /* Utgard: limite seguro */
  if (g_h > 1080) g_h = 1080;
}

int bully_screen_w(void) { return g_w; }
int bully_screen_h(void) { return g_h; }

int bully_init_gl(void) {
  if (g_ctx != EGL_NO_CONTEXT) return 1;
  read_fb_res();
  g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (g_dpy == EGL_NO_DISPLAY) { fprintf(stderr, "[egl] GetDisplay falhou\n"); return 0; }
  EGLint maj, min;
  if (!eglInitialize(g_dpy, &maj, &min)) { fprintf(stderr, "[egl] Initialize 0x%x\n", eglGetError()); return 0; }
  eglBindAPI(EGL_OPENGL_ES_API);

  EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                        EGL_DEPTH_SIZE, 24, EGL_NONE };
  EGLConfig cfg; EGLint n;
  if (!eglChooseConfig(g_dpy, cfg_attr, &cfg, 1, &n) || n < 1) { fprintf(stderr, "[egl] ChooseConfig 0x%x\n", eglGetError()); return 0; }

  g_win.width = (uint16_t)g_w; g_win.height = (uint16_t)g_h;
  g_surf = eglCreateWindowSurface(g_dpy, cfg, (EGLNativeWindowType)&g_win, NULL);
  if (g_surf == EGL_NO_SURFACE) { fprintf(stderr, "[egl] CreateWindowSurface 0x%x\n", eglGetError()); return 0; }

  EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
  if (g_ctx == EGL_NO_CONTEXT) { fprintf(stderr, "[egl] CreateContext 0x%x\n", eglGetError()); return 0; }

  eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx);
  const char *(*gs)(unsigned) = (void*)eglGetProcAddress("glGetString");
  fprintf(stderr, "[egl] EGL %d.%d Mali fbdev %dx%d | dpy=%p surf=%p ctx=%p | GL=%s\n",
          maj, min, g_w, g_h, (void*)g_dpy, (void*)g_surf, (void*)g_ctx, gs ? gs(0x1F02) : "?");
  return 1;
}

void bully_egl_objects(uintptr_t *d, uintptr_t *s, uintptr_t *c) {
  if (d) *d = (uintptr_t)g_dpy; if (s) *s = (uintptr_t)g_surf; if (c) *c = (uintptr_t)g_ctx;
}
int  bully_make_current(void) { return eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx) ? 1 : 0; }
void bully_release_current(void) { eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT); }
void bully_swap_buffers(void) { if (g_dpy != EGL_NO_DISPLAY) eglSwapBuffers(g_dpy, g_surf); }
