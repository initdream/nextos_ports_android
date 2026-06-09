/* android_ndk_shims.c — stubs das funcs Android NDK (F1: passar do init_array).
 * ANativeWindow* vira janela Mali real na F2; AAsset* vira fopen(data1) na F3.
 * Por ora: ASensor/ALooper = no-op; ANativeWindow = stub; AAsset = NULL. Exportados (-rdynamic). */
#include <stdio.h>
#include <stdint.h>

/* ---- ASensorManager / ASensor (sem sensores) ---- */
void *ASensorManager_getInstance(void){ return (void*)1; }
void *ASensorManager_getSensorList(void *m, void ***list){ (void)m; if(list)*list=0; return 0; }
void *ASensorManager_createEventQueue(void *m, void *looper, int id, void *cb, void *data){ (void)m;(void)looper;(void)id;(void)cb;(void)data; return (void*)1; }
int   ASensorManager_destroyEventQueue(void *m, void *q){ (void)m;(void)q; return 0; }
const char *ASensor_getName(void *s){ (void)s; return "none"; }
int   ASensor_getType(void *s){ (void)s; return 0; }
int   ASensor_getMinDelay(void *s){ (void)s; return 0; }
int   ASensorEventQueue_enableSensor(void *q, void *s){ (void)q;(void)s; return -1; }
int   ASensorEventQueue_disableSensor(void *q, void *s){ (void)q;(void)s; return 0; }
int   ASensorEventQueue_setEventRate(void *q, void *s, int us){ (void)q;(void)s;(void)us; return 0; }
int   ASensorEventQueue_getEvents(void *q, void *ev, size_t n){ (void)q;(void)ev;(void)n; return 0; }

/* ---- ALooper (não bloqueia) ---- */
void *ALooper_prepare(int opts){ (void)opts; return (void*)1; }
int   ALooper_pollOnce(int timeoutMs, int *outFd, int *outEvents, void **outData){ (void)timeoutMs;(void)outFd;(void)outEvents;(void)outData; return -3; /* ALOOPER_POLL_TIMEOUT */ }
void  ALooper_wake(void *l){ (void)l; }

/* ---- ANativeWindow (F2: vira surface Mali fbdev. Por ora stub) ---- */
void *ANativeWindow_fromSurface(void *env, void *surface){ (void)env; (void)surface; return 0; }
int   ANativeWindow_setBuffersGeometry(void *w, int width, int height, int format){ (void)w;(void)width;(void)height;(void)format; return 0; }
int   ANativeWindow_getWidth(void *w){ (void)w; return 1280; }
int   ANativeWindow_getHeight(void *w){ (void)w; return 720; }
int   ANativeWindow_getFormat(void *w){ (void)w; return 1; /* RGBA_8888 */ }
void  ANativeWindow_release(void *w){ (void)w; }

/* ---- AAssetManager / AAsset (F3: fopen no data1. Por ora NULL) ---- */
void *AAssetManager_fromJava(void *env, void *obj){ (void)env;(void)obj; return (void*)1; }
void *AAssetManager_open(void *mgr, const char *fn, int mode){ (void)mgr;(void)mode; fprintf(stderr,"[AAsset] open %s (stub NULL)\n", fn?fn:"?"); return 0; }
int   AAsset_read(void *a, void *buf, size_t count){ (void)a;(void)buf;(void)count; return -1; }
long  AAsset_getLength64(void *a){ (void)a; return 0; }
long  AAsset_seek64(void *a, long off, int whence){ (void)a;(void)off;(void)whence; return -1; }
void  AAsset_close(void *a){ (void)a; }
void *AAssetManager_openDir(void *mgr, const char *dir){ (void)mgr;(void)dir; return 0; }
const char *AAssetDir_getNextFileName(void *d){ (void)d; return 0; }
void  AAssetDir_close(void *d){ (void)d; }
