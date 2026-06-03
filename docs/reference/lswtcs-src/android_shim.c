/*
 * android_shim.c -- Fake Android NDK environment for LSWTCS
 *
 * AAssetManager: maps asset opens to filesystem reads.
 * ANativeWindow: returns screen dimensions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "android_shim.h"
#include "util.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

static char g_data_path[512] = ".";

void android_shim_set_data_path(const char *path) {
  strncpy(g_data_path, path, sizeof(g_data_path) - 1);
  g_data_path[sizeof(g_data_path) - 1] = '\0';
}

/* AAssetManager — wraps FILE* */
typedef struct {
  FILE *fp;
  long size;
} FakeAsset;

void *AAssetManager_open_fake(void *mgr, const char *filename, int mode) {
  (void)mgr;
  (void)mode;

  char path[1024];
  if (filename && filename[0] == '/') {
    snprintf(path, sizeof(path), "%s", filename);
  } else {
    snprintf(path, sizeof(path), "%s/%s", g_data_path, filename);
  }
  const char *resolved = resolve_android_path(path);

  debugPrintf("AAssetManager_open: %s -> %s\n", path, resolved);

  FILE *fp = fopen(resolved, "rb");
  if (!fp) {
    debugPrintf("AAssetManager_open: FAILED to open %s\n", resolved);
    return NULL;
  }

  FakeAsset *a = malloc(sizeof(FakeAsset));
  a->fp = fp;
  fseek(fp, 0, SEEK_END);
  a->size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  return a;
}

void AAsset_close_fake(void *asset) {
  if (!asset) return;
  FakeAsset *a = (FakeAsset *)asset;
  if (a->fp) fclose(a->fp);
  free(a);
}

int AAsset_read_fake(void *asset, void *buf, size_t count) {
  if (!asset) return -1;
  FakeAsset *a = (FakeAsset *)asset;
  return (int)fread(buf, 1, count, a->fp);
}

long AAsset_getLength_fake(void *asset) {
  if (!asset) return 0;
  FakeAsset *a = (FakeAsset *)asset;
  return a->size;
}

long AAsset_seek_fake(void *asset, long offset, int whence) {
  if (!asset) return -1;
  FakeAsset *a = (FakeAsset *)asset;
  fseek(a->fp, offset, whence);
  return ftell(a->fp);
}

/* ANativeWindow — allocate a large enough fake struct so field accesses don't fault */
static char fake_native_window[8192] __attribute__((aligned(64)));

void *ANativeWindow_fromSurface_fake(void *env, void *surface) {
  (void)env;
  (void)surface;
  debugPrintf("ANativeWindow_fromSurface(%p) -> %p\n", surface, fake_native_window);
  return fake_native_window;
}

int ANativeWindow_getWidth_fake(void *window) {
  (void)window;
  return SCREEN_WIDTH;
}

int ANativeWindow_getHeight_fake(void *window) {
  (void)window;
  return SCREEN_HEIGHT;
}

int ANativeWindow_setBuffersGeometry_fake(void *window, int w, int h, int fmt) {
  (void)window;
  (void)w;
  (void)h;
  (void)fmt;
  return 0;
}
