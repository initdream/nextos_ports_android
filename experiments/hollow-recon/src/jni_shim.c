/*
 * jni_shim.c -- fake JNI environment for Syberia
 *
 * Android JNI works through double-indirection:
 *   JavaVM *vm;   vm->GetEnv(vm, &env, version)
 *   JNIEnv *env;  env->FindClass(env, "com/foo/Bar")
 *
 * Both vm and env are pointers to a pointer to a function table.
 * We create large stub vtables that return 0/NULL for everything,
 * with specific overrides for methods Syberia actually uses.
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "jni_shim.h"
#include "util.h"

#define JNI_VTABLE_SIZE 512

typedef int jint;

static uintptr_t jni_env_vtable[JNI_VTABLE_SIZE];
static void *jni_env_ptr;

static uintptr_t java_vm_vtable[JNI_VTABLE_SIZE];
static void *java_vm_ptr;

/* ---- Tagged method/field IDs ---- */
enum {
  MID_UNKNOWN = 0,
  MID_GET_STORAGE_DIR,
  MID_GET_PACK_NAME,
  MID_SET_ACTIVITY,
  MID_ERROR_DIALOG,
  MID_GET_CLASS_LOADER,
  MID_LOAD_CLASS,
  MID_GENERIC,
  FID_OBB_VERSIONCODE,
  FID_GENERIC,
};

static int g_method_tags[16]; /* unique addresses used as method IDs */

/* ---- Configurable package/OBB ---- */
static const char *g_package_name = "com.microids.syberia";
static int g_obb_version = 12;

void jni_shim_set_package(const char *package_name, int obb_version) {
  g_package_name = package_name;
  g_obb_version = obb_version;
}

/* ---- Fake jstring tracking ---- */
/* We return tagged pointers as jstrings and map them to C strings */
#define MAX_JSTRINGS 1024
static struct {
  void *handle;
  char *value; /* copia propria (strdup) */
} g_jstrings[MAX_JSTRINGS];
static int g_jstring_count = 0;

static void *make_jstring(const char *value) {
  static char jstring_storage[MAX_JSTRINGS];
  int idx = g_jstring_count++ % MAX_JSTRINGS;
  if (g_jstrings[idx].value) free(g_jstrings[idx].value);
  g_jstrings[idx].handle = &jstring_storage[idx];
  g_jstrings[idx].value = strdup(value ? value : "");
  return g_jstrings[idx].handle;
}

static const char *resolve_jstring(void *jstr) {
  for (int i = 0; i < MAX_JSTRINGS; i++) {
    if (g_jstrings[i].handle == jstr && g_jstrings[i].value)
      return g_jstrings[i].value;
  }
  return "";
}

/* ---- Registry de method/field IDs por NOME (recon Unity) ---- */
struct mid_entry { const char *name; const char *sig; };
static struct mid_entry g_midreg[1024];
static int g_midreg_count = 0;

static void *reg_mid(const char *name, const char *sig) {
  for (int i = 0; i < g_midreg_count; i++)
    if (g_midreg[i].name == name ||
        (name && g_midreg[i].name && strcmp(g_midreg[i].name, name) == 0))
      return &g_midreg[i];
  if (g_midreg_count >= 1024) g_midreg_count = 0;
  int i = g_midreg_count++;
  g_midreg[i].name = name;
  g_midreg[i].sig = sig;
  return &g_midreg[i];
}
static const char *mid_name(void *tag) {
  if ((char *)tag >= (char *)g_midreg &&
      (char *)tag < (char *)(g_midreg + 1024))
    return ((struct mid_entry *)tag)->name;
  return NULL;
}

/* ===================================================================
 * AssetManager bridge — le de /storage/hollow-recon/assets/<path>
 * (Unity: getAssets() + AssetManager.open(path) + InputStream.read/close)
 * =================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#define ASSET_BASE "/storage/roms/hollow-recon/assets/"

static int g_assetmgr;   /* tag do objeto AssetManager */
static int g_empty_list; /* tag de uma java.util.List vazia */
static int g_iterator;   /* tag de um Iterator vazio */
static int g_appinfo;    /* tag de ApplicationInfo */

/* --- byte[] tracking (backing real) --- */
#define MAX_BARR 128
struct barr { unsigned char *buf; int len; };
static struct barr g_barr[MAX_BARR];
static int g_barr_n = 0;
static void *barr_new(int len) {
  int i = g_barr_n++ % MAX_BARR;
  if (g_barr[i].buf) free(g_barr[i].buf);
  g_barr[i].buf = (unsigned char *)malloc(len > 0 ? len : 1);
  g_barr[i].len = len;
  return &g_barr[i];
}
static struct barr *barr_find(void *h) {
  if ((char *)h >= (char *)g_barr && (char *)h < (char *)(g_barr + MAX_BARR))
    return (struct barr *)h;
  return NULL;
}

/* --- InputStream (FILE*) tracking --- */
#define MAX_ASTREAMS 32
struct astream { FILE *fp; long size; };
static struct astream g_astreams[MAX_ASTREAMS];
static int g_astream_n = 0;
static void *asset_open(const char *path) {
  char full[1200];
  snprintf(full, sizeof(full), ASSET_BASE "%s", path ? path : "");
  FILE *fp = fopen(full, "rb");
  debugPrintf("asset: open(%s) -> %s\n", path ? path : "?",
              fp ? "OK" : "FALHOU (sem arquivo)");
  if (!fp) return NULL;
  int i = g_astream_n++ % MAX_ASTREAMS;
  if (g_astreams[i].fp) fclose(g_astreams[i].fp);
  g_astreams[i].fp = fp;
  fseek(fp, 0, SEEK_END);
  g_astreams[i].size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  return &g_astreams[i];
}
static struct astream *astream_find(void *h) {
  if ((char *)h >= (char *)g_astreams &&
      (char *)h < (char *)(g_astreams + MAX_ASTREAMS))
    return (struct astream *)h;
  return NULL;
}

/* --- JNI byte-array functions --- */
static void *jni_NewByteArray(void *env, int len) {
  (void)env;
  return barr_new(len);
}
static int jni_GetArrayLength_real(void *env, void *arr) {
  (void)env;
  struct barr *b = barr_find(arr);
  return b ? b->len : 0;
}
static void *jni_GetByteArrayElements(void *env, void *arr, void *isCopy) {
  (void)env;
  if (isCopy) *(unsigned char *)isCopy = 0;
  struct barr *b = barr_find(arr);
  return b ? b->buf : NULL;
}
static void jni_ReleaseByteArrayElements(void *env, void *arr, void *elems,
                                         int mode) {
  (void)env; (void)arr; (void)elems; (void)mode;
}
static void jni_GetByteArrayRegion(void *env, void *arr, int start, int len,
                                   void *buf) {
  (void)env;
  struct barr *b = barr_find(arr);
  if (b && start >= 0 && len >= 0 && start + len <= b->len)
    memcpy(buf, b->buf + start, len);
}
static void jni_SetByteArrayRegion(void *env, void *arr, int start, int len,
                                   const void *buf) {
  (void)env;
  struct barr *b = barr_find(arr);
  if (b && start >= 0 && len >= 0 && start + len <= b->len)
    memcpy(b->buf + start, buf, len);
}

/* ---- Generic stub ---- */
static intptr_t jni_stub(void) { return 0; }

/* ---- JNIEnv functions ---- */

static jint jni_GetVersion(void *env) {
  (void)env;
  return 0x00010006;
}

/* ===== Injeção de input p/ nativeInjectEvent (KeyEvent) =====
   nativeInjectEvent lê o evento via JNI (getAction/getKeyCode/...). Setamos
   g_hk_inject ANTES de chamar nativeInjectEvent e os métodos retornam daqui. */
struct hk_inject_s { int action, keycode, source, deviceId, metaState, repeat,
                     scancode, flags, unicode; long eventTime, downTime; };
struct hk_inject_s g_hk_inject;       /* exportado p/ main_recon */
static int g_obj_keyevent;            /* sentinela do objeto KeyEvent */
void *hk_keyevent_object(void) { return &g_obj_keyevent; }

/* classes distintas por nome (Unity compara KeyEvent.class vs MotionEvent.class) */
static struct { const char *name; int tag; } g_classreg[128];
static int g_classreg_n = 0;
static void *class_for(const char *name) {
  if (!name) name = "?";
  for (int i = 0; i < g_classreg_n; i++)
    if (g_classreg[i].name == name ||
        (g_classreg[i].name && strcmp(g_classreg[i].name, name) == 0))
      return &g_classreg[i].tag;
  if (g_classreg_n >= 128) g_classreg_n = 0;
  int i = g_classreg_n++;
  g_classreg[i].name = name;
  return &g_classreg[i].tag;
}
static const char *class_name_of(void *clazz) {
  for (int i = 0; i < g_classreg_n; i++)
    if (clazz == &g_classreg[i].tag) return g_classreg[i].name;
  return NULL;
}
static void *jni_FindClass(void *env, const char *name) {
  (void)env;
  debugPrintf("jni_shim: FindClass(%s)\n", name);
  return class_for(name);
}

/* ===== NewObject (28/29/30) =====
   O Swappy do Unity cria o ChoreographerCallback JAVA via NewObject passando
   (jlong)this = o OBJETO C++ que o nOnChoreographer (thunk virtual-call em x2)
   PRECISA. Capturamos o cookie aqui -> main_recon dirige o vsync de verdade. */
long long g_hk_choreo_cookie = 0;  /* exportado p/ main_recon */
static int g_obj_newobject;
static void *jni_NewObjectV(void *env, void *clazz, void *methodID, va_list ap) {
  (void)env;
  const char *cn = class_name_of(clazz);
  const char *sig = NULL;
  if ((char *)methodID >= (char *)g_midreg && (char *)methodID < (char *)(g_midreg + 1024))
    sig = ((struct mid_entry *)methodID)->sig;
  debugPrintf("jni_shim: NewObject(%s, ctor %s)\n", cn ? cn : "?", sig ? sig : "?");
  /* ctor com 1o arg long (J...) -> provavel cookie de objeto nativo */
  if (sig && sig[0] == '(' && sig[1] == 'J') {
    long long cookie = va_arg(ap, long long);
    debugPrintf("jni_shim: NewObject cookie=0x%llx\n", cookie);
    if (cn && strstr(cn, "ChoreographerCallback")) {
      g_hk_choreo_cookie = cookie;
      debugPrintf("jni_shim: 🔑 CHOREO cookie CAPTURADO = 0x%llx\n", cookie);
    }
  }
  return &g_obj_newobject;
}
static void *jni_NewObject(void *env, void *clazz, void *methodID, ...) {
  va_list ap; va_start(ap, methodID);
  void *r = jni_NewObjectV(env, clazz, methodID, ap);
  va_end(ap);
  return r;
}
static void *jni_NewObjectA(void *env, void *clazz, void *methodID, const long long *args) {
  (void)env;
  const char *cn = class_name_of(clazz);
  const char *sig = NULL;
  if ((char *)methodID >= (char *)g_midreg && (char *)methodID < (char *)(g_midreg + 1024))
    sig = ((struct mid_entry *)methodID)->sig;
  debugPrintf("jni_shim: NewObjectA(%s, ctor %s)\n", cn ? cn : "?", sig ? sig : "?");
  if (sig && sig[0] == '(' && sig[1] == 'J' && args) {
    debugPrintf("jni_shim: NewObjectA cookie=0x%llx\n", args[0]);
    if (cn && strstr(cn, "ChoreographerCallback")) {
      g_hk_choreo_cookie = args[0];
      debugPrintf("jni_shim: 🔑 CHOREO cookie CAPTURADO (A) = 0x%llx\n", args[0]);
    }
  }
  return &g_obj_newobject;
}

static void *jni_GetMethodID(void *env, void *clazz, const char *name,
                             const char *sig) {
  (void)env;
  (void)clazz;
  debugPrintf("jni_shim: GetMethodID(%s, %s)\n", name, sig);
  return reg_mid(name, sig);
}

static void *jni_GetStaticMethodID(void *env, void *clazz, const char *name,
                                   const char *sig) {
  (void)env;
  (void)clazz;
  debugPrintf("jni_shim: GetStaticMethodID(%s, %s)\n", name, sig);
  return reg_mid(name, sig);
}

static void *jni_GetFieldID(void *env, void *clazz, const char *name,
                            const char *sig) {
  (void)env;
  (void)clazz;
  debugPrintf("jni_shim: GetFieldID(%s, %s)\n", name, sig);
  return reg_mid(name, sig);  /* registry com NOME -> GetIntField/FloatField respondem certo */
}

/* ===== Get<Type>Field (100/102/...) =====
   DisplayMetrics & cia: campos lidos via JNI. Stub generico devolvia 0 ->
   widthPixels/heightPixels/density = 0 -> ortho do canvas DEGENERADA (escala 0). */
extern int egl_shim_width(void), egl_shim_height(void);
static jint jni_GetIntField(void *env, void *obj, void *fieldID) {
  (void)env; (void)obj;
  const char *nm = mid_name(fieldID);
  if (nm) {
    if (strcmp(nm, "widthPixels") == 0)  { debugPrintf("[METRICS] widthPixels=%d\n", egl_shim_width()); return egl_shim_width(); }
    if (strcmp(nm, "heightPixels") == 0) { debugPrintf("[METRICS] heightPixels=%d\n", egl_shim_height()); return egl_shim_height(); }
    if (strcmp(nm, "densityDpi") == 0)   return 240;
  }
  debugPrintf("jni_shim: GetIntField(%s) -> 0\n", nm ? nm : "?");
  return 0;
}
static float jni_GetFloatField(void *env, void *obj, void *fieldID) {
  (void)env; (void)obj;
  const char *nm = mid_name(fieldID);
  if (nm) {
    if (strcmp(nm, "density") == 0 || strcmp(nm, "scaledDensity") == 0) return 1.5f;
    if (strcmp(nm, "xdpi") == 0 || strcmp(nm, "ydpi") == 0) return 240.0f;
    if (strcmp(nm, "refreshRate") == 0) return 60.0f;
  }
  debugPrintf("jni_shim: GetFloatField(%s) -> 0\n", nm ? nm : "?");
  return 0.0f;
}
static long long jni_GetLongField(void *env, void *obj, void *fieldID) {
  (void)env; (void)obj; (void)fieldID;
  return 0;
}

static void *jni_GetStaticFieldID(void *env, void *clazz, const char *name,
                                  const char *sig) {
  (void)env;
  (void)clazz;
  debugPrintf("jni_shim: GetStaticFieldID(%s, %s)\n", name, sig);
  if (strcmp(name, "OBB_VERSIONCODE") == 0)
    return &g_method_tags[FID_OBB_VERSIONCODE];
  return &g_method_tags[FID_GENERIC];
}

/* CallObjectMethod — Unity (C++) usa a variante V (va_list); dispatch nela. */
static void *jni_CallObjectMethodV(void *env, void *obj, void *methodID,
                                   va_list ap) {
  (void)env;
  const char *nm = mid_name(methodID);
  debugPrintf("jni_shim: CallObjectMethod(%s)\n", nm ? nm : "?");
  static int fake_obj;
  if (nm) {
    if (strcmp(nm, "getPackageName") == 0)
      return make_jstring("com.teamcherry.hollowknight");
    /* AssetManager bridge */
    if (strcmp(nm, "getAssets") == 0) return &g_assetmgr;
    /* listas vazias (queryIntentActivities, etc.) + iterator vazio */
    if (strcmp(nm, "queryIntentActivities") == 0 ||
        strcmp(nm, "queryBroadcastReceivers") == 0 ||
        strcmp(nm, "getSystemSharedLibraryNames") == 0)
      return &g_empty_list;
    if (strcmp(nm, "iterator") == 0) return &g_iterator;
    if (strcmp(nm, "getApplicationInfo") == 0) return &g_appinfo;
    if ((strcmp(nm, "open") == 0 || strcmp(nm, "openNonAsset") == 0) &&
        obj == &g_assetmgr) {
      void *pathstr = va_arg(ap, void *);
      return asset_open(resolve_jstring(pathstr)); /* NULL se nao existe */
    }
    /* builders Android (Intent.addFlags/setData/...) retornam o proprio obj */
    if (strcmp(nm, "addFlags") == 0 || strcmp(nm, "setFlags") == 0 ||
        strcmp(nm, "setData") == 0 || strcmp(nm, "setAction") == 0 ||
        strcmp(nm, "append") == 0)
      return obj;
    /* diretorios de dados -> path REAL gravavel (persistentDataPath do Unity).
       Sem isso (=""), PlayerPrefs/save quebram -> jogo trava em "first run". */
    if (strcmp(nm, "getFilesDir") == 0 || strcmp(nm, "getExternalFilesDir") == 0 ||
        strcmp(nm, "getCacheDir") == 0 || strcmp(nm, "getExternalCacheDir") == 0 ||
        strcmp(nm, "getDataDir") == 0 || strcmp(nm, "getExternalStorageDirectory") == 0 ||
        strcmp(nm, "getPath") == 0 || strcmp(nm, "getAbsolutePath") == 0 ||
        strcmp(nm, "getCanonicalPath") == 0)
      return make_jstring("/storage/roms/hollow-recon/userdata");
    /* SharedPreferences.getString(key, default) -> loga a chave + retorna default */
    if (strcmp(nm, "getString") == 0) {
      void *keystr = va_arg(ap, void *);
      void *defstr = va_arg(ap, void *);
      debugPrintf("[PREFS] getString key='%s'\n", resolve_jstring(keystr));
      return defstr ? defstr : make_jstring("");
    }
    if (strcmp(nm, "toString") == 0)
      return make_jstring("");
  }
  return &fake_obj;
}
static void *jni_CallObjectMethod(void *env, void *obj, void *methodID, ...) {
  va_list ap; va_start(ap, methodID);
  void *r = jni_CallObjectMethodV(env, obj, methodID, ap);
  va_end(ap);
  return r;
}

/* CallBooleanMethod (index 49) */
static unsigned char jni_CallBooleanMethod(void *env, void *obj,
                                           void *methodID, ...) {
  (void)env;
  (void)obj;
  const char *nm = mid_name(methodID);
  if (nm) {
    if (strcmp(nm, "isEmpty") == 0) return 1;  /* lista vazia */
    if (strcmp(nm, "hasNext") == 0) return 0;  /* iterator vazio */
  }
  return 0;
}

/* CallIntMethod — variante V */
static jint jni_CallIntMethodV(void *env, void *obj, void *methodID,
                               va_list ap) {
  (void)env;
  const char *nm = mid_name(methodID);
  if (nm) {
    /* ---- KeyEvent (nativeInjectEvent) ---- */
    if (strcmp(nm, "getAction") == 0) { debugPrintf("[KEYEV] getAction->%d\n", g_hk_inject.action); return g_hk_inject.action; }
    if (strcmp(nm, "getKeyCode") == 0) { debugPrintf("[KEYEV] getKeyCode->%d\n", g_hk_inject.keycode); return g_hk_inject.keycode; }
    if (strcmp(nm, "getSource") == 0) return g_hk_inject.source;
    if (strcmp(nm, "getDeviceId") == 0) return g_hk_inject.deviceId;
    if (strcmp(nm, "getMetaState") == 0) return g_hk_inject.metaState;
    if (strcmp(nm, "getRepeatCount") == 0) return g_hk_inject.repeat;
    if (strcmp(nm, "getScanCode") == 0) return g_hk_inject.scancode;
    if (strcmp(nm, "getInt") == 0) { void *k = va_arg(ap, void *); int d = va_arg(ap, int);
      debugPrintf("[PREFS] getInt key='%s' def=%d\n", resolve_jstring(k), d); return d; }
    if (strcmp(nm, "getFlags") == 0) return g_hk_inject.flags;
    if (strcmp(nm, "getUnicodeChar") == 0) return g_hk_inject.unicode;
    if (strcmp(nm, "size") == 0) return 0; /* List/Collection vazia */
    /* ---- Display (receita do CUPHEAD que renderiza no Mali-450): a Unity pergunta
       a resolucao TAMBEM por metodos; 0 -> camera/ortho degenerada -> tela preta. */
    if (strcmp(nm, "getWidth") == 0 || strcmp(nm, "getRawWidth") == 0) {
      debugPrintf("[DISPLAY] getWidth->%d\n", egl_shim_width()); return egl_shim_width(); }
    if (strcmp(nm, "getHeight") == 0 || strcmp(nm, "getRawHeight") == 0) {
      debugPrintf("[DISPLAY] getHeight->%d\n", egl_shim_height()); return egl_shim_height(); }
    if (strcmp(nm, "getRotation") == 0) return 0;
    if (strcmp(nm, "getDisplayId") == 0) return 0;
  }
  struct astream *s = astream_find(obj);
  if (s && nm) {
    if (strcmp(nm, "read") == 0) {
      void *barr = va_arg(ap, void *);
      int off = va_arg(ap, int);
      int len = va_arg(ap, int);
      struct barr *b = barr_find(barr);
      if (!b) return -1;
      if (off < 0) off = 0;
      if (off + len > b->len) len = b->len - off;
      if (len <= 0) return -1;
      size_t n = fread(b->buf + off, 1, (size_t)len, s->fp);
      return n > 0 ? (int)n : -1; /* -1 = EOF */
    }
    if (strcmp(nm, "available") == 0) {
      long pos = ftell(s->fp);
      return (int)(s->size - pos);
    }
  }
  return 0;
}
static jint jni_CallIntMethod(void *env, void *obj, void *methodID, ...) {
  va_list ap; va_start(ap, methodID);
  jint r = jni_CallIntMethodV(env, obj, methodID, ap);
  va_end(ap);
  return r;
}

/* CallFloatMethod (55/56) — receita do Cuphead: Display.getRefreshRate() -> 60Hz
   (0 quebra o engine/frame pacing). */
static float jni_CallFloatMethodV(void *env, void *obj, void *methodID, va_list ap) {
  (void)env; (void)obj; (void)ap;
  const char *nm = mid_name(methodID);
  if (nm && strcmp(nm, "getRefreshRate") == 0) { debugPrintf("[DISPLAY] getRefreshRate->60\n"); return 60.0f; }
  return 0.0f;
}
static float jni_CallFloatMethod(void *env, void *obj, void *methodID, ...) {
  va_list ap; va_start(ap, methodID);
  float r = jni_CallFloatMethodV(env, obj, methodID, ap);
  va_end(ap);
  return r;
}

/* CallVoidMethod (index 94) */
static void jni_CallVoidMethod(void *env, void *obj, void *methodID, ...) {
  (void)env;
  const char *nm = mid_name(methodID);
  struct astream *s = astream_find(obj);
  if (s && nm && strcmp(nm, "close") == 0) {
    if (s->fp) { fclose(s->fp); s->fp = NULL; }
  }
}

/* CallStaticObjectMethod (index 113) */
static void *jni_CallStaticObjectMethod(void *env, void *clazz,
                                        void *methodID, ...) {
  (void)env;
  (void)clazz;

  const char *nm = mid_name(methodID);
  debugPrintf("jni_shim: CallStaticObjectMethod(%s)\n", nm ? nm : "?");
  static int fake_result;
  return &fake_result;  /* fake Class/objeto nao-nulo (forName etc.) */
}

/* CallStaticBooleanMethod (index 124) */
static unsigned char jni_CallStaticBooleanMethod(void *env, void *clazz,
                                                 void *methodID, ...) {
  (void)env;
  (void)clazz;
  debugPrintf("jni_shim: CallStaticBooleanMethod(mid=%p) -> 1\n", methodID);
  // Return true for hasTouchScreen — prevents game from managing
  // Shield gamepad button layouts that don't exist in the OBB.
  return 1;
}

/* CallStaticIntMethod (index 136) */
static jint jni_CallStaticIntMethod(void *env, void *clazz, void *methodID,
                                    ...) {
  (void)env;
  (void)clazz;
  (void)methodID;
  return 0;
}

/* CallStaticVoidMethod (index 145) */
static void jni_CallStaticVoidMethod(void *env, void *clazz, void *methodID,
                                     ...) {
  (void)env;
  (void)clazz;
  debugPrintf("jni_shim: CallStaticVoidMethod(mid=%p)\n", methodID);
}

/* GetStaticIntField (index 155) */
static jint jni_GetStaticIntField(void *env, void *clazz, void *fieldID) {
  (void)env;
  (void)clazz;

  if (fieldID == &g_method_tags[FID_OBB_VERSIONCODE]) {
    debugPrintf("jni_shim: GetStaticIntField -> OBB_VERSIONCODE = %d\n",
                g_obb_version);
    return g_obb_version;
  }
  debugPrintf("jni_shim: GetStaticIntField(fid=%p) -> 0\n", fieldID);
  return 0;
}

/* GetStaticObjectField (index 156) */
static void *jni_GetStaticObjectField(void *env, void *clazz, void *fieldID) {
  (void)env;
  (void)clazz;
  (void)fieldID;
  debugPrintf("jni_shim: GetStaticObjectField -> NULL\n");
  static int fake;
  return &fake;
}

/* NewStringUTF (index 167) */
static void *jni_NewStringUTF(void *env, const char *str) {
  (void)env;
  debugPrintf("jni_shim: NewStringUTF(%s)\n", str ? str : "(null)");
  return make_jstring(str ? str : "");
}

/* GetStringUTFLength (index 168) */
static jint jni_GetStringUTFLength(void *env, void *jstr) {
  (void)env;
  const char *s = resolve_jstring(jstr);
  return (jint)strlen(s);
}

/* GetStringUTFChars (index 169) */
static const char *jni_GetStringUTFChars(void *env, void *jstr,
                                         void *isCopy) {
  (void)env;
  (void)isCopy;
  const char *s = resolve_jstring(jstr);
  debugPrintf("jni_shim: GetStringUTFChars -> \"%s\"\n", s);
  return s;
}

/* ReleaseStringUTFChars (index 170) */
static void jni_ReleaseStringUTFChars(void *env, void *jstr,
                                      const char *chars) {
  (void)env;
  (void)jstr;
  (void)chars;
}

/* Ref management */
static void *jni_NewGlobalRef(void *env, void *obj) {
  (void)env;
  return obj;
}
static void *jni_NewLocalRef(void *env, void *obj) {
  (void)env;
  return obj;
}
static void jni_DeleteGlobalRef(void *env, void *obj) {
  (void)env;
  (void)obj;
}
static void jni_DeleteLocalRef(void *env, void *obj) {
  (void)env;
  (void)obj;
}
static void *jni_GetObjectClass(void *env, void *obj) {
  (void)env;
  if (obj == &g_obj_keyevent) return class_for("android/view/KeyEvent");
  static int fake_obj_class;
  return &fake_obj_class;
}
static unsigned char jni_IsInstanceOf(void *env, void *obj, void *clazz) {
  (void)env;
  if (obj == &g_obj_keyevent) return clazz == class_for("android/view/KeyEvent");
  return 1; /* permissivo p/ outros casts */
}
static unsigned char jni_IsSameObject(void *env, void *a, void *b) {
  (void)env; return a == b;
}
/* CallLongMethod V — getEventTime/getDownTime do KeyEvent (retornam long) */
static long jni_CallLongMethodV(void *env, void *obj, void *methodID, va_list ap) {
  (void)env; (void)obj; (void)ap;
  const char *nm = mid_name(methodID);
  if (nm) {
    if (strcmp(nm, "getEventTime") == 0) return g_hk_inject.eventTime;
    if (strcmp(nm, "getDownTime") == 0) return g_hk_inject.downTime;
  }
  return 0;
}
static long jni_CallLongMethod(void *env, void *obj, void *methodID, ...) {
  va_list ap; va_start(ap, methodID);
  long r = jni_CallLongMethodV(env, obj, methodID, ap);
  va_end(ap);
  return r;
}

/* Exception handling */
static unsigned char jni_ExceptionCheck(void *env) {
  (void)env;
  return 0;
}
static void jni_ExceptionClear(void *env) { (void)env; }
static void *jni_ExceptionOccurred(void *env) {
  (void)env;
  return 0;
}

/* Array */
static jint jni_GetArrayLength(void *env, void *array) {
  (void)env;
  (void)array;
  return 0;
}

/* ---- JavaVM functions ---- */

static jint vm_DestroyJavaVM(void *vm) {
  (void)vm;
  return 0;
}

static jint vm_AttachCurrentThread(void *vm, void **penv, void *args) {
  (void)vm;
  (void)args;
  debugPrintf("jni_shim: AttachCurrentThread\n");
  if (penv)
    *penv = &jni_env_ptr;
  return 0;
}

static jint vm_DetachCurrentThread(void *vm) {
  (void)vm;
  return 0;
}

static jint vm_GetEnv(void *vm, void **penv, jint version) {
  (void)vm;
  (void)version;
  /* GetEnv e' chamado milhares de vezes (cada thread/icall) -> silenciado */
  if (penv)
    *penv = &jni_env_ptr;
  return 0;
}

static jint vm_AttachCurrentThreadAsDaemon(void *vm, void **penv, void *args) {
  (void)vm;
  (void)args;
  if (penv)
    *penv = &jni_env_ptr;
  return 0;
}

/* ---- recon: RegisterNatives com log + STORAGE dos ponteiros ---- */
struct native_method { const char *name; const char *sig; void *fn; };
static struct native_method g_natives[512];
static int g_natives_count = 0;

void *jni_find_native(const char *name) {
  for (int i = 0; i < g_natives_count; i++)
    if (strcmp(g_natives[i].name, name) == 0) return g_natives[i].fn;
  return 0;
}

static int jni_RegisterNatives(void *env, void *clazz, const void *methods, int n) {
  (void)env; (void)clazz;
  debugPrintf("jni_shim: >> RegisterNatives(%d metodos)\n", n);
  const uintptr_t *m = (const uintptr_t *)methods;  /* {name, sig, fnPtr} x n */
  for (int i = 0; i < n && i < 128; i++) {
    const char *nm = (const char *)m[i * 3];
    const char *sg = (const char *)m[i * 3 + 1];
    void *fn = (void *)m[i * 3 + 2];
    debugPrintf("     [%d] %s %s  -> %p\n", i, nm ? nm : "?", sg ? sg : "?", fn);
    if (nm && g_natives_count < 512) {
      g_natives[g_natives_count].name = nm;
      g_natives[g_natives_count].sig = sg;
      g_natives[g_natives_count].fn = fn;
      g_natives_count++;
    }
  }
  return 0;
}

/* GetJavaVM (index 219) — initJni chama isso */
static jint jni_GetJavaVM(void *env, void **vm) {
  (void)env;
  debugPrintf("jni_shim: GetJavaVM -> nossa VM\n");
  *vm = &java_vm_ptr;   /* mesma JavaVM passada no out_vm */
  return 0;
}

/* ---- Init ---- */

void jni_install_indexed(uintptr_t *vt, int n);

void jni_shim_init(void **out_vm, void **out_env) {
  for (int i = 0; i < JNI_VTABLE_SIZE; i++) {
    jni_env_vtable[i] = (uintptr_t)jni_stub;
    java_vm_vtable[i] = (uintptr_t)jni_stub;
  }
  jni_install_indexed(jni_env_vtable, JNI_VTABLE_SIZE);

  /*
   * JNIEnv vtable indices from Android NDK jni.h.
   * C++ wrappers in the .so call the *V (va_list) variants,
   * so we must set both the variadic and V slots.
   *
   *   0-3:   reserved
   *   4:     GetVersion
   *   6:     FindClass
   *  15:     ExceptionOccurred
   *  17:     ExceptionClear
   *  21:     NewGlobalRef
   *  22:     DeleteGlobalRef
   *  23:     DeleteLocalRef
   *  25:     NewLocalRef
   *  31:     GetObjectClass
   *  33:     GetMethodID
   *  34/35:  CallObjectMethod / V
   *  37/38:  CallBooleanMethod / V
   *  49/50:  CallIntMethod / V
   *  61/62:  CallVoidMethod / V
   *  94:     GetFieldID
   * 113:     GetStaticMethodID
   * 114/115: CallStaticObjectMethod / V
   * 117/118: CallStaticBooleanMethod / V
   * 129/130: CallStaticIntMethod / V
   * 141/142: CallStaticVoidMethod / V
   * 144:     GetStaticFieldID
   * 145:     GetStaticObjectField
   * 150:     GetStaticIntField
   * 167:     NewStringUTF
   * 168:     GetStringUTFLength
   * 169:     GetStringUTFChars
   * 170:     ReleaseStringUTFChars
   * 171:     GetArrayLength
   * 205:     ExceptionCheck
   */
  jni_env_vtable[4] = (uintptr_t)jni_GetVersion;
  jni_env_vtable[6] = (uintptr_t)jni_FindClass;
  jni_env_vtable[215] = (uintptr_t)jni_RegisterNatives;  /* recon: Unity */
  jni_env_vtable[219] = (uintptr_t)jni_GetJavaVM;        /* recon: Unity initJni */
  /* AssetManager bridge: byte-array functions */
  jni_env_vtable[171] = (uintptr_t)jni_GetArrayLength_real;
  jni_env_vtable[176] = (uintptr_t)jni_NewByteArray;
  jni_env_vtable[184] = (uintptr_t)jni_GetByteArrayElements;
  jni_env_vtable[192] = (uintptr_t)jni_ReleaseByteArrayElements;
  jni_env_vtable[200] = (uintptr_t)jni_GetByteArrayRegion;
  jni_env_vtable[208] = (uintptr_t)jni_SetByteArrayRegion;
  jni_env_vtable[15] = (uintptr_t)jni_ExceptionOccurred;
  jni_env_vtable[17] = (uintptr_t)jni_ExceptionClear;
  jni_env_vtable[21] = (uintptr_t)jni_NewGlobalRef;
  jni_env_vtable[22] = (uintptr_t)jni_DeleteGlobalRef;
  jni_env_vtable[23] = (uintptr_t)jni_DeleteLocalRef;
  jni_env_vtable[25] = (uintptr_t)jni_NewLocalRef;
  jni_env_vtable[24] = (uintptr_t)jni_IsSameObject;
  jni_env_vtable[28] = (uintptr_t)jni_NewObject;
  jni_env_vtable[29] = (uintptr_t)jni_NewObjectV;
  jni_env_vtable[30] = (uintptr_t)jni_NewObjectA;
  jni_env_vtable[31] = (uintptr_t)jni_GetObjectClass;
  jni_env_vtable[32] = (uintptr_t)jni_IsInstanceOf;
  jni_env_vtable[33] = (uintptr_t)jni_GetMethodID;
  jni_env_vtable[52] = (uintptr_t)jni_CallLongMethod;
  jni_env_vtable[53] = (uintptr_t)jni_CallLongMethodV;
  jni_env_vtable[34] = (uintptr_t)jni_CallObjectMethod;
  jni_env_vtable[35] = (uintptr_t)jni_CallObjectMethodV;   /* V variant (va_list) */
  jni_env_vtable[37] = (uintptr_t)jni_CallBooleanMethod;
  jni_env_vtable[38] = (uintptr_t)jni_CallBooleanMethod;   /* V */
  jni_env_vtable[49] = (uintptr_t)jni_CallIntMethod;
  jni_env_vtable[50] = (uintptr_t)jni_CallIntMethodV;      /* V (va_list) */
  jni_env_vtable[61] = (uintptr_t)jni_CallVoidMethod;
  jni_env_vtable[62] = (uintptr_t)jni_CallVoidMethod;      /* V */
  jni_env_vtable[94] = (uintptr_t)jni_GetFieldID;
  jni_env_vtable[100] = (uintptr_t)jni_GetIntField;
  jni_env_vtable[101] = (uintptr_t)jni_GetLongField;
  jni_env_vtable[102] = (uintptr_t)jni_GetFloatField;
  jni_env_vtable[55] = (uintptr_t)jni_CallFloatMethod;     /* getRefreshRate (Cuphead) */
  jni_env_vtable[56] = (uintptr_t)jni_CallFloatMethodV;    /* V */
  jni_env_vtable[113] = (uintptr_t)jni_GetStaticMethodID;
  jni_env_vtable[114] = (uintptr_t)jni_CallStaticObjectMethod;
  jni_env_vtable[115] = (uintptr_t)jni_CallStaticObjectMethod; /* V */
  jni_env_vtable[117] = (uintptr_t)jni_CallStaticBooleanMethod;
  jni_env_vtable[118] = (uintptr_t)jni_CallStaticBooleanMethod; /* V */
  jni_env_vtable[129] = (uintptr_t)jni_CallStaticIntMethod;
  jni_env_vtable[130] = (uintptr_t)jni_CallStaticIntMethod; /* V */
  jni_env_vtable[141] = (uintptr_t)jni_CallStaticVoidMethod;
  jni_env_vtable[142] = (uintptr_t)jni_CallStaticVoidMethod; /* V */
  jni_env_vtable[144] = (uintptr_t)jni_GetStaticFieldID;
  jni_env_vtable[145] = (uintptr_t)jni_GetStaticObjectField;
  jni_env_vtable[150] = (uintptr_t)jni_GetStaticIntField;
  jni_env_vtable[167] = (uintptr_t)jni_NewStringUTF;
  jni_env_vtable[168] = (uintptr_t)jni_GetStringUTFLength;
  jni_env_vtable[169] = (uintptr_t)jni_GetStringUTFChars;
  jni_env_vtable[170] = (uintptr_t)jni_ReleaseStringUTFChars;
  jni_env_vtable[171] = (uintptr_t)jni_GetArrayLength;
  jni_env_vtable[205] = (uintptr_t)jni_ExceptionCheck;

  jni_env_ptr = jni_env_vtable;

  /* JavaVM vtable */
  java_vm_vtable[3] = (uintptr_t)vm_DestroyJavaVM;
  java_vm_vtable[4] = (uintptr_t)vm_AttachCurrentThread;
  java_vm_vtable[5] = (uintptr_t)vm_DetachCurrentThread;
  java_vm_vtable[6] = (uintptr_t)vm_GetEnv;
  java_vm_vtable[7] = (uintptr_t)vm_AttachCurrentThreadAsDaemon;

  java_vm_ptr = java_vm_vtable;

  if (out_vm)
    *out_vm = &java_vm_ptr;
  if (out_env)
    *out_env = &jni_env_ptr;

  debugPrintf("jni_shim: Initialized (vm=%p, env=%p)\n", &java_vm_ptr,
              &jni_env_ptr);
}
