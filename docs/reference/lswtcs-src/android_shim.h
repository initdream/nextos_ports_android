/*
 * android_shim.h -- Fake Android NDK environment for LSWTCS
 *
 * Provides AAssetManager and ANativeWindow shims.
 */

#ifndef __ANDROID_SHIM_H__
#define __ANDROID_SHIM_H__

#include <stdint.h>

/* AAssetManager */
void *AAssetManager_open_fake(void *mgr, const char *filename, int mode);
void AAsset_close_fake(void *asset);
int AAsset_read_fake(void *asset, void *buf, size_t count);
long AAsset_getLength_fake(void *asset);
long AAsset_seek_fake(void *asset, long offset, int whence);

/* ANativeWindow */
void *ANativeWindow_fromSurface_fake(void *env, void *surface);
int ANativeWindow_getWidth_fake(void *window);
int ANativeWindow_getHeight_fake(void *window);
int ANativeWindow_setBuffersGeometry_fake(void *window, int w, int h, int fmt);

/* Set the base data path (e.g. directory containing the .dat files) */
void android_shim_set_data_path(const char *path);

#endif
