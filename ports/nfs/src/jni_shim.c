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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
  MID_GET_OBB_PATH,
  MID_GET_ABS_PATH,
  MID_GET_FILES_DIR,
  MID_GET_PACK_NAME,
  MID_SET_ACTIVITY,
  MID_ERROR_DIALOG,
  MID_GET_CLASS_LOADER,
  MID_LOAD_CLASS,
  MID_IS_OBB,
  MID_USE_ASSETS_FS,
  MID_IS_FULL_APK,
  MID_GET_ASSET_SIZE,
  MID_APP_VERSION,
  MID_DEVICE_NAME,
  MID_LOCALE,
  MID_LANGUAGE,
  MID_OS_VERSION,
  MID_GENERIC,
  FID_OBB_VERSIONCODE,
  FID_WIDTH,
  FID_HEIGHT,
  FID_DENSITY,
  FID_DENSITY_DPI,
  FID_GENERIC,
};

static int g_method_tags[32]; /* unique addresses used as method IDs */

/* ---- Configurable package/OBB ---- */
static const char *g_package_name = "com.microids.syberia";
static int g_obb_version = 12;

void jni_shim_set_package(const char *package_name, int obb_version) {
  g_package_name = package_name;
  g_obb_version = obb_version;
}

/* ---- Fake jstring tracking ---- */
/* We return tagged pointers as jstrings and map them to C strings */
#define MAX_JSTRINGS 32
static struct {
  void *handle;
  const char *value;
} g_jstrings[MAX_JSTRINGS];
static int g_jstring_count = 0;

static void *make_jstring(const char *value) {
  static char jstring_storage[MAX_JSTRINGS];
  if (g_jstring_count >= MAX_JSTRINGS)
    g_jstring_count = 0; /* wrap around */
  int idx = g_jstring_count++;
  g_jstrings[idx].handle = &jstring_storage[idx];
  g_jstrings[idx].value = value;
  return g_jstrings[idx].handle;
}

static const char *resolve_jstring(void *jstr) {
  for (int i = 0; i < g_jstring_count; i++) {
    if (g_jstrings[i].handle == jstr)
      return g_jstrings[i].value;
  }
  return "";
}

/* ---- Generic stub ---- */
static intptr_t jni_stub(void) { return 0; }

/* ---- JNIEnv functions ---- */

static jint jni_GetVersion(void *env) {
  (void)env;
  return 0x00010006;
}

static void *jni_FindClass(void *env, const char *name) {
  (void)env;
  debugPrintf("jni_shim: FindClass(%s)\n", name);
  static int fake_class;
  return &fake_class;
}

/* config strings (versão/device/locale): retornar VAZIO faz a engine tokenizar
 * "" e criar objeto garbage → assert. Damos valores válidos. */
static void *jni_match_config(const char *name) {
  if (strstr(name, "ApplicationVersion") || strstr(name, "VersionName"))
    return &g_method_tags[MID_APP_VERSION];
  if (strstr(name, "DeviceName")) return &g_method_tags[MID_DEVICE_NAME];
  if (strstr(name, "Locale")) return &g_method_tags[MID_LOCALE];
  if (strstr(name, "Language")) return &g_method_tags[MID_LANGUAGE];
  if (strstr(name, "OsVersion") || strstr(name, "OSVersion")) return &g_method_tags[MID_OS_VERSION];
  return 0;
}

static void *jni_GetMethodID(void *env, void *clazz, const char *name,
                             const char *sig) {
  (void)env;
  (void)clazz;
  debugPrintf("jni_shim: GetMethodID(%s, %s)\n", name, sig);
  if (strcmp(name, "getClassLoader") == 0)
    return &g_method_tags[MID_GET_CLASS_LOADER];
  if (strcmp(name, "loadClass") == 0)
    return &g_method_tags[MID_LOAD_CLASS];
  /* NFS: cadeia de path storage/OBB. getExternalStorageDirectory()/getFilesDir()
   * retornam um File; getAbsolutePath() nele → a String do path. */
  if (strstr(name, "ObbFullPath") || strstr(name, "ObbPath"))
    return &g_method_tags[MID_GET_OBB_PATH];
  if (strstr(name, "getAbsolutePath") || strstr(name, "getPath") || strstr(name, "getCanonicalPath"))
    return &g_method_tags[MID_GET_ABS_PATH];
  if (strstr(name, "getFilesDir"))
    return &g_method_tags[MID_GET_FILES_DIR];
  if (strcmp(name, "getPackageName") == 0 || strcmp(name, "getPackName") == 0)
    return &g_method_tags[MID_GET_PACK_NAME];
  if (strcmp(name, "isObbAssets") == 0) return &g_method_tags[MID_IS_OBB];
  if (strcmp(name, "useAssetsFileSystem") == 0) return &g_method_tags[MID_USE_ASSETS_FS];
  if (strcmp(name, "isFullApkAssets") == 0) return &g_method_tags[MID_IS_FULL_APK];
  if (strcmp(name, "getAssetSize") == 0) return &g_method_tags[MID_GET_ASSET_SIZE];
  void *cfg = jni_match_config(name);
  if (cfg) return cfg;
  if (strstr(name, "ExternalStorageDirectory") || strstr(name, "StorageDirectory") ||
      strstr(name, "StorageDir"))
    return &g_method_tags[MID_GET_STORAGE_DIR];
  return &g_method_tags[MID_GENERIC];
}

static void *jni_GetStaticMethodID(void *env, void *clazz, const char *name,
                                   const char *sig) {
  (void)env;
  (void)clazz;
  debugPrintf("jni_shim: GetStaticMethodID(%s, %s)\n", name, sig);
  { void *cfg = jni_match_config(name); if (cfg) return cfg; }
  if (strcmp(name, "getStorageDir") == 0)
    return &g_method_tags[MID_GET_STORAGE_DIR];
  if (strstr(name, "ObbFullPath") || strstr(name, "ObbPath"))
    return &g_method_tags[MID_GET_OBB_PATH];
  if (strstr(name, "ExternalStorageDirectory") || strstr(name, "StorageDirectory"))
    return &g_method_tags[MID_GET_STORAGE_DIR];
  if (strcmp(name, "getPackName") == 0)
    return &g_method_tags[MID_GET_PACK_NAME];
  if (strcmp(name, "setActivity") == 0)
    return &g_method_tags[MID_SET_ACTIVITY];
  if (strcmp(name, "errorDialog") == 0)
    return &g_method_tags[MID_ERROR_DIALOG];
  return &g_method_tags[MID_GENERIC];
}

static void *jni_GetFieldID(void *env, void *clazz, const char *name,
                            const char *sig) {
  (void)env;
  (void)clazz;
  debugPrintf("jni_shim: GetFieldID(%s, %s)\n", name, sig);
  if (strcmp(name, "widthPixels") == 0) return &g_method_tags[FID_WIDTH];
  if (strcmp(name, "heightPixels") == 0) return &g_method_tags[FID_HEIGHT];
  if (strcmp(name, "density") == 0) return &g_method_tags[FID_DENSITY];
  if (strcmp(name, "densityDpi") == 0) return &g_method_tags[FID_DENSITY_DPI];
  return &g_method_tags[FID_GENERIC];
}

/* GetIntField/GetFloatField: tela 1280x720, densidade 2.0/320dpi */
static jint jni_GetIntField(void *env, void *obj, void *fid) {
  (void)env; (void)obj;
  if (fid == &g_method_tags[FID_WIDTH]) return 1280;
  if (fid == &g_method_tags[FID_HEIGHT]) return 720;
  if (fid == &g_method_tags[FID_DENSITY_DPI]) return 320;
  return 0;
}
static float jni_GetFloatField(void *env, void *obj, void *fid) {
  (void)env; (void)obj;
  if (fid == &g_method_tags[FID_DENSITY]) return 2.0f;
  return 0.0f;
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

/* path base dos dados (env NFS_DATA) + path completo do OBB */
static const char *nfs_data_dir(void) {
  const char *d = getenv("NFS_DATA");
  return (d && *d) ? d : "/storage/roms/nfs/data";
}
static const char *nfs_obb_path(void) {
  static char p[512];
  snprintf(p, sizeof(p), "%s/Android/obb/%s/main.%d.%s.obb",
           nfs_data_dir(), g_package_name, 1003128, g_package_name);
  return p;
}

/* retorna o jstring da config (ou NULL se methodID não é config) */
static void *jni_config_jstr(void *methodID) {
  if (methodID == &g_method_tags[MID_APP_VERSION]) return make_jstring("1.3.128");
  if (methodID == &g_method_tags[MID_DEVICE_NAME]) return make_jstring("NextOS");
  if (methodID == &g_method_tags[MID_LOCALE]) return make_jstring("en_US");
  if (methodID == &g_method_tags[MID_LANGUAGE]) return make_jstring("en");
  if (methodID == &g_method_tags[MID_OS_VERSION]) return make_jstring("9");
  return 0;
}

/* CallObjectMethod (index 36) - variadic */
static void *jni_CallObjectMethod(void *env, void *obj, void *methodID, ...) {
  (void)env;
  (void)obj;
  { void *cv = jni_config_jstr(methodID); if (cv) return cv; }
  if (methodID == &g_method_tags[MID_GET_STORAGE_DIR]) {
    debugPrintf("jni_shim: CallObjectMethod -> storageDir = %s\n", nfs_data_dir());
    return make_jstring(nfs_data_dir());
  }
  if (methodID == &g_method_tags[MID_GET_OBB_PATH]) {
    debugPrintf("jni_shim: CallObjectMethod -> obbPath = %s\n", nfs_obb_path());
    return make_jstring(nfs_obb_path());
  }
  if (methodID == &g_method_tags[MID_GET_FILES_DIR]) {
    static char fd[512]; snprintf(fd, sizeof(fd), "%s/files", nfs_data_dir());
    debugPrintf("jni_shim: CallObjectMethod -> filesDir = %s\n", fd);
    return make_jstring(fd);
  }
  if (methodID == &g_method_tags[MID_GET_ABS_PATH]) {
    /* getAbsolutePath() do File: o 'obj' (File) já é nosso jstring do path */
    debugPrintf("jni_shim: CallObjectMethod -> getAbsolutePath = %s\n", resolve_jstring(obj));
    return obj;
  }
  if (methodID == &g_method_tags[MID_GET_PACK_NAME]) {
    debugPrintf("jni_shim: CallObjectMethod -> packageName = %s\n", g_package_name);
    return make_jstring(g_package_name);
  }
  debugPrintf("jni_shim: CallObjectMethod(mid=%p)\n", methodID);
  static int fake_obj;
  return &fake_obj;
}

/* CallBooleanMethod (index 49) */
static unsigned char jni_CallBooleanMethod(void *env, void *obj,
                                           void *methodID, ...) {
  (void)env;
  (void)obj;
  if (methodID == &g_method_tags[MID_IS_OBB]) {
    debugPrintf("jni_shim: isObbAssets -> 1\n");
    return 1; /* assets estão no OBB */
  }
  return 0; /* useAssetsFileSystem/isFullApkAssets -> 0 */
}

/* CallIntMethod (index 61) */
static jint jni_CallIntMethod(void *env, void *obj, void *methodID, ...) {
  (void)env;
  (void)obj;
  (void)methodID;
  return 0;
}

/* CallVoidMethod (index 94) */
static void jni_CallVoidMethod(void *env, void *obj, void *methodID, ...) {
  (void)env;
  (void)obj;
  (void)methodID;
}

/* CallStaticObjectMethod (index 113) */
static void *jni_CallStaticObjectMethod(void *env, void *clazz,
                                        void *methodID, ...) {
  (void)env;
  (void)clazz;
  { void *cv = jni_config_jstr(methodID); if (cv) return cv; }

  if (methodID == &g_method_tags[MID_GET_STORAGE_DIR]) {
    debugPrintf("jni_shim: CallStaticObjectMethod -> storageDir = %s\n", nfs_data_dir());
    return make_jstring(nfs_data_dir());
  }
  if (methodID == &g_method_tags[MID_GET_OBB_PATH]) {
    debugPrintf("jni_shim: CallStaticObjectMethod -> obbPath = %s\n", nfs_obb_path());
    return make_jstring(nfs_obb_path());
  }
  if (methodID == &g_method_tags[MID_GET_PACK_NAME]) {
    debugPrintf(
        "jni_shim: CallStaticObjectMethod -> getPackName = \"%s\"\n",
        g_package_name);
    return make_jstring(g_package_name);
  }

  debugPrintf("jni_shim: CallStaticObjectMethod(mid=%p) -> NULL\n", methodID);
  static int fake_result;
  return &fake_result;
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
  (void)obj;
  static int fake_obj_class;
  return &fake_obj_class;
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
  debugPrintf("jni_shim: GetEnv(version=0x%x)\n", version);
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

/* ---- Init ---- */

void jni_shim_init(void **out_vm, void **out_env) {
  for (int i = 0; i < JNI_VTABLE_SIZE; i++) {
    jni_env_vtable[i] = (uintptr_t)jni_stub;
    java_vm_vtable[i] = (uintptr_t)jni_stub;
  }

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
  jni_env_vtable[15] = (uintptr_t)jni_ExceptionOccurred;
  jni_env_vtable[17] = (uintptr_t)jni_ExceptionClear;
  jni_env_vtable[21] = (uintptr_t)jni_NewGlobalRef;
  jni_env_vtable[22] = (uintptr_t)jni_DeleteGlobalRef;
  jni_env_vtable[23] = (uintptr_t)jni_DeleteLocalRef;
  jni_env_vtable[25] = (uintptr_t)jni_NewLocalRef;
  jni_env_vtable[31] = (uintptr_t)jni_GetObjectClass;
  jni_env_vtable[33] = (uintptr_t)jni_GetMethodID;
  jni_env_vtable[34] = (uintptr_t)jni_CallObjectMethod;
  jni_env_vtable[35] = (uintptr_t)jni_CallObjectMethod;    /* V variant */
  jni_env_vtable[37] = (uintptr_t)jni_CallBooleanMethod;
  jni_env_vtable[38] = (uintptr_t)jni_CallBooleanMethod;   /* V */
  jni_env_vtable[49] = (uintptr_t)jni_CallIntMethod;
  jni_env_vtable[50] = (uintptr_t)jni_CallIntMethod;       /* V */
  jni_env_vtable[61] = (uintptr_t)jni_CallVoidMethod;
  jni_env_vtable[62] = (uintptr_t)jni_CallVoidMethod;      /* V */
  jni_env_vtable[94] = (uintptr_t)jni_GetFieldID;
  jni_env_vtable[100] = (uintptr_t)jni_GetIntField;
  jni_env_vtable[102] = (uintptr_t)jni_GetFloatField;
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
