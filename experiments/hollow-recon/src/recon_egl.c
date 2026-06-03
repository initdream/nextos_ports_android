/*
 * recon_egl.c — liga o EGL/ANativeWindow do Unity ao nosso egl_shim (SDL2/Mali).
 * Override das entradas da import table (chamar ANTES de so_resolve).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include "egl_shim.h"
#include "so_util.h"
#include "imports.h"

/* wrapper de dlopen/dlsym p/ ver o Unity carregar libil2cpp */
static void *my_dlopen(const char *name, int flag) {
  fprintf(stderr, "[dlopen] \"%s\" flag=%d\n", name ? name : "(self)", flag);
  void *h = dlopen(name, flag);
  fprintf(stderr, "[dlopen] -> %p %s\n", h, h ? "OK" : dlerror());
  return h;
}
static void *my_dlsym(void *h, const char *sym) {
  void *p = dlsym(h, sym);
  if (!p) fprintf(stderr, "[dlsym] %s -> NULL\n", sym ? sym : "?");
  return p;
}

/* ---- ANativeWindow shim (Unity usa no Surface) ---- */
static void *ANW_fromSurface(void *env, void *surface) {
  (void)env; (void)surface;
  void *w = egl_shim_get_window();
  return w ? w : (void *)0x57; /* nao-nulo */
}
static int ANW_getWidth(void *w)  { (void)w; return 1280; }
static int ANW_getHeight(void *w) { (void)w; return 720; }
static int ANW_setBuffersGeometry(void *w, int a, int b, int c) {
  (void)w; (void)a; (void)b; (void)c; return 0;
}
static void ANW_acquire(void *w) { (void)w; }
static void ANW_release(void *w) { (void)w; }

static void set_import(const char *name, void *fn) {
  for (size_t i = 0; i < dynlib_numfunctions; i++)
    if (strcmp(dynlib_functions[i].symbol, name) == 0) {
      dynlib_functions[i].func = (uintptr_t)fn;
      return;
    }
}

void recon_wire_egl(void) {
  set_import("eglGetDisplay", (void *)egl_shim_GetDisplay);
  set_import("eglInitialize", (void *)egl_shim_Initialize);
  set_import("eglTerminate", (void *)egl_shim_Terminate);
  set_import("eglChooseConfig", (void *)egl_shim_ChooseConfig);
  set_import("eglCreateWindowSurface", (void *)egl_shim_CreateWindowSurface);
  set_import("eglCreatePbufferSurface", (void *)egl_shim_CreatePbufferSurface);
  set_import("eglCreateContext", (void *)egl_shim_CreateContext);
  set_import("eglMakeCurrent", (void *)egl_shim_MakeCurrent);
  set_import("eglSwapBuffers", (void *)egl_shim_SwapBuffers);
  set_import("eglDestroySurface", (void *)egl_shim_DestroySurface);
  set_import("eglDestroyContext", (void *)egl_shim_DestroyContext);
  set_import("eglQuerySurface", (void *)egl_shim_QuerySurface);
  set_import("eglGetConfigAttrib", (void *)egl_shim_GetConfigAttrib);
  set_import("eglGetError", (void *)egl_shim_GetError);
  set_import("eglGetProcAddress", (void *)egl_shim_GetProcAddress);
  set_import("eglQueryString", (void *)egl_shim_QueryString);
  set_import("eglSwapInterval", (void *)egl_shim_SwapInterval);
  set_import("eglGetCurrentContext", (void *)egl_shim_GetCurrentContext);
  set_import("eglGetCurrentSurface", (void *)egl_shim_GetCurrentSurface);
  set_import("eglSurfaceAttrib", (void *)egl_shim_SurfaceAttrib);
  /* ANativeWindow */
  set_import("ANativeWindow_fromSurface", (void *)ANW_fromSurface);
  set_import("ANativeWindow_getWidth", (void *)ANW_getWidth);
  set_import("ANativeWindow_getHeight", (void *)ANW_getHeight);
  set_import("ANativeWindow_setBuffersGeometry", (void *)ANW_setBuffersGeometry);
  set_import("ANativeWindow_acquire", (void *)ANW_acquire);
  set_import("ANativeWindow_release", (void *)ANW_release);
  /* dlopen/dlsym logados (ver libil2cpp carregar) */
  set_import("dlopen", (void *)my_dlopen);
  set_import("dlsym", (void *)my_dlsym);
  fprintf(stderr, "[egl] wired: 20 EGL + 6 ANativeWindow + dlopen/dlsym -> egl_shim\n");
}
