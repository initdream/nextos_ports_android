/* main_pc.c -- bring-up do Bully no PC (x86_64): load + resolve + jni_load + loop. */
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "so_util_x64.h"
#include "jni_shim.h"

#ifndef GAMEDIR
#define GAMEDIR "gamefiles"
#endif

Module mod_cxx, mod_game;   /* mod_game referenciado pelo jni_shim */

extern void bully_imports_init(void);
extern uintptr_t bully_shim(const char *name);

static uintptr_t resolver(const char *name, void *user) {
  (void)user;
  uintptr_t a = so_lookup_global(name);   /* companion (libc++) */
  if (a) return a;
  a = bully_shim(name);                    /* bionic/NDK shims */
  if (a) return a;
  void *d = dlsym(RTLD_DEFAULT, name);     /* host: libc/m/GLESv2/openal/z/pthread */
  return (uintptr_t)d;
}

static int load_mod(Module *m, const char *file) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", GAMEDIR, file);
  if (so_load(m, path) != 0) { fprintf(stderr, "FALHOU load %s\n", file); return -1; }
  so_relocate(m);
  so_register_module(m);
  return 0;
}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  fprintf(stderr, "=== bully-pc bring-up ===\n");

  setenv("SDL_VIDEODRIVER", "x11", 1);          /* X11/XWayland */
  setenv("SDL_VIDEO_X11_FORCE_EGL", "1", 1);    /* força EGL (nao GLX) p/ ter objetos EGL reais */
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    fprintf(stderr, "SDL_Init falhou: %s\n", SDL_GetError()); return 1;
  }
  bully_imports_init();
  if (chdir(GAMEDIR) != 0) fprintf(stderr, "aviso: chdir(%s) falhou\n", GAMEDIR);

  if (load_mod(&mod_cxx, "libc++_shared.so")) return 1;
  if (load_mod(&mod_game, "libGame.so")) return 1;

  so_resolve(&mod_cxx, resolver, NULL);
  int u = so_resolve(&mod_game, resolver, NULL);
  fprintf(stderr, "libGame UNRESOLVED=%d\n", u);

  fprintf(stderr, "-- init_array libc++ --\n"); so_execute_init_array(&mod_cxx);
  fprintf(stderr, "-- init_array libGame --\n"); so_execute_init_array(&mod_game);

  jni_init_input();
  fprintf(stderr, "-- jni_load (JNI_OnLoad -> RegisterNatives -> init) --\n");
  jni_load();   /* dirige o init do jogo; pode entrar no loop interno do jogo */

  /* se init() retornar (nao tem loop proprio), bombeia eventos */
  fprintf(stderr, "-- loop de eventos (init retornou) --\n");
  int run = 1;
  while (run) {
    SDL_Event e;
    while (SDL_PollEvent(&e))
      if (e.type == SDL_QUIT) run = 0;
    SDL_Delay(16);
  }
  SDL_Quit();
  return 0;
}
