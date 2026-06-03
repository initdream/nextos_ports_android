/*
 * jni_shim.c -- Fake JNI for LSWTCS
 *
 * Minimal JavaVM/JNIEnv with 512-entry vtables.
 * JNI methods: FlurryEvent (stub), IsMusicActive (0),
 * OpenPrivacyPolicy (stub), OpenTermsOfServices (stub),
 * getCountryCode ("US"). Fields: WINDOW_SERVICE, SDK_INT=21.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jni_shim.h"
#include "util.h"

#define VTABLE_SIZE 512

static void *jni_vtable[VTABLE_SIZE];
static void *jvm_vtable[VTABLE_SIZE];

JNIEnv_fake g_jni_env;
JavaVM_fake g_jni_vm;

/* Tracked method/field IDs */
typedef struct {
  int id;
  const char *name;
  const char *sig;
} FakeMethod;

#define MAX_METHODS 32
static FakeMethod g_methods[MAX_METHODS];
static int g_num_methods = 0;

#define MAX_FIELDS 16
static FakeMethod g_fields[MAX_FIELDS];
static int g_num_fields = 0;

static char g_country_code[] = "US";
static char g_window_service[] = "window";
static int g_sdk_int = 21;

typedef struct {
  jint length;
  void **elements;
} FakeObjectArray;

/* Default stub */
static void *jni_stub(void) { return NULL; }
static int jni_stub_int(void) { return 0; }

/* JNI functions */
static jclass FindClass(void *env, const char *name) {
  debugPrintf("JNI FindClass: %s\n", name);
  return (jclass)0x42424242;
}

static jmethodID GetMethodID(void *env, jclass cls, const char *name, const char *sig) {
  debugPrintf("JNI GetMethodID: %s %s\n", name, sig);
  if (g_num_methods < MAX_METHODS) {
    int id = 100 + g_num_methods;
    g_methods[g_num_methods].id = id;
    g_methods[g_num_methods].name = name;
    g_methods[g_num_methods].sig = sig;
    g_num_methods++;
    return (jmethodID)(intptr_t)id;
  }
  return (jmethodID)0;
}

static jmethodID GetStaticMethodID(void *env, jclass cls, const char *name, const char *sig) {
  return GetMethodID(env, cls, name, sig);
}

static jfieldID GetFieldID(void *env, jclass cls, const char *name, const char *sig) {
  debugPrintf("JNI GetFieldID: %s %s\n", name, sig);
  if (g_num_fields < MAX_FIELDS) {
    int id = 200 + g_num_fields;
    g_fields[g_num_fields].id = id;
    g_fields[g_num_fields].name = name;
    g_fields[g_num_fields].sig = sig;
    g_num_fields++;
    return (jfieldID)(intptr_t)id;
  }
  return (jfieldID)0;
}

static jfieldID GetStaticFieldID(void *env, jclass cls, const char *name, const char *sig) {
  return GetFieldID(env, cls, name, sig);
}

static const char *find_method_name(jmethodID mid) {
  int id = (int)(intptr_t)mid;
  for (int i = 0; i < g_num_methods; i++) {
    if (g_methods[i].id == id) return g_methods[i].name;
  }
  return "unknown";
}

static const char *find_field_name(jfieldID fid) {
  int id = (int)(intptr_t)fid;
  for (int i = 0; i < g_num_fields; i++) {
    if (g_fields[i].id == id) return g_fields[i].name;
  }
  return "unknown";
}

/* CallVoidMethod */
static void CallVoidMethod(void *env, jobject obj, jmethodID mid, ...) {
  const char *name = find_method_name(mid);
  debugPrintf("JNI CallVoidMethod: %s\n", name);
}

static void CallStaticVoidMethod(void *env, jclass cls, jmethodID mid, ...) {
  const char *name = find_method_name(mid);
  debugPrintf("JNI CallStaticVoidMethod: %s\n", name);
}

/* CallBooleanMethod */
static jboolean CallBooleanMethod(void *env, jobject obj, jmethodID mid, ...) {
  const char *name = find_method_name(mid);
  debugPrintf("JNI CallBooleanMethod: %s -> 0\n", name);
  return 0;
}

static jboolean CallStaticBooleanMethod(void *env, jclass cls, jmethodID mid, ...) {
  return CallBooleanMethod(env, cls, mid);
}

/* CallObjectMethod */
static jobject CallObjectMethod(void *env, jobject obj, jmethodID mid, ...) {
  const char *name = find_method_name(mid);
  debugPrintf("JNI CallObjectMethod: %s\n", name);
  if (strcmp(name, "getCountryCode") == 0) return g_country_code;
  return NULL;
}

static jobject CallStaticObjectMethod(void *env, jclass cls, jmethodID mid, ...) {
  return CallObjectMethod(env, cls, mid);
}

/* CallIntMethod */
static jint CallIntMethod(void *env, jobject obj, jmethodID mid, ...) {
  const char *name = find_method_name(mid);
  debugPrintf("JNI CallIntMethod: %s -> 0\n", name);
  return 0;
}

static jint CallStaticIntMethod(void *env, jclass cls, jmethodID mid, ...) {
  return CallIntMethod(env, cls, mid);
}

/* GetField */
static jobject GetObjectField(void *env, jobject obj, jfieldID fid) {
  const char *name = find_field_name(fid);
  debugPrintf("JNI GetObjectField: %s\n", name);
  if (strcmp(name, "WINDOW_SERVICE") == 0) return g_window_service;
  return NULL;
}

static jint GetStaticIntField(void *env, jclass cls, jfieldID fid) {
  const char *name = find_field_name(fid);
  debugPrintf("JNI GetStaticIntField: %s -> %d\n", name, g_sdk_int);
  if (strcmp(name, "SDK_INT") == 0) return g_sdk_int;
  return 0;
}

static jint GetIntField(void *env, jobject obj, jfieldID fid) {
  return GetStaticIntField(env, obj, fid);
}

/* String operations */
static const char *GetStringUTFChars(void *env, jstring str, void *isCopy) {
  (void)env;
  (void)isCopy;
  return (const char *)str;
}

static void ReleaseStringUTFChars(void *env, jstring str, const char *chars) {
  (void)env; (void)str; (void)chars;
}

static jstring NewStringUTF(void *env, const char *bytes) {
  (void)env;
  return (jstring)bytes;
}

static int GetStringUTFLength(void *env, jstring str) {
  (void)env;
  if (!str) return 0;
  return (int)strlen((const char *)str);
}

static jint GetArrayLength(void *env, jobjectArray array) {
  (void)env;
  FakeObjectArray *arr = (FakeObjectArray *)array;
  if (!arr)
    return 0;
  return arr->length;
}

static jobjectArray NewObjectArray(void *env, jint length, jclass elementClass,
                                   jobject initialElement) {
  (void)env;
  (void)elementClass;
  FakeObjectArray *arr = calloc(1, sizeof(*arr));
  if (!arr)
    return NULL;
  arr->length = length;
  if (length > 0) {
    arr->elements = calloc((size_t)length, sizeof(void *));
    if (!arr->elements) {
      free(arr);
      return NULL;
    }
    for (jint i = 0; i < length; i++) {
      arr->elements[i] = initialElement;
    }
  }
  return (jobjectArray)arr;
}

static jobject GetObjectArrayElement(void *env, jobjectArray array, jint index) {
  (void)env;
  FakeObjectArray *arr = (FakeObjectArray *)array;
  if (!arr || index < 0 || index >= arr->length || !arr->elements)
    return NULL;
  return (jobject)arr->elements[index];
}

static void SetObjectArrayElement(void *env, jobjectArray array, jint index,
                                  jobject value) {
  (void)env;
  FakeObjectArray *arr = (FakeObjectArray *)array;
  if (!arr || index < 0 || index >= arr->length || !arr->elements)
    return;
  arr->elements[index] = value;
}

/* GetJavaVM */
static jint GetJavaVM(void *env, void **vm) {
  if (vm) *vm = &g_jni_vm;
  return 0;
}

/* JVM functions */
static jint GetEnv(void *vm, void **env, int version) {
  (void)vm; (void)version;
  if (env) *env = &g_jni_env;
  return 0;
}

static jint AttachCurrentThread(void *vm, void **env, void *args) {
  (void)vm; (void)args;
  debugPrintf("JNI AttachCurrentThread\n");
  if (env) *env = &g_jni_env;
  return 0;
}

static jint DetachCurrentThread(void *vm) {
  (void)vm;
  return 0;
}

/* NewGlobalRef/DeleteGlobalRef - just pass through */
static jobject NewGlobalRef(void *env, jobject obj) { (void)env; return obj; }
static void DeleteGlobalRef(void *env, jobject obj) { (void)env; (void)obj; }
static jobject NewLocalRef(void *env, jobject obj) { (void)env; return obj; }
static void DeleteLocalRef(void *env, jobject obj) { (void)env; (void)obj; }

/* ExceptionCheck/ExceptionClear */
static jboolean ExceptionCheck(void *env) { (void)env; return 0; }
static void ExceptionClear(void *env) { (void)env; }

void jni_shim_init(void) {
  // Fill vtables with stubs
  for (int i = 0; i < VTABLE_SIZE; i++) {
    jni_vtable[i] = (void *)jni_stub;
    jvm_vtable[i] = (void *)jni_stub_int;
  }

  // JNIEnv vtable — indices from Android NDK jni.h (JNINativeInterface struct)
  jni_vtable[4]   = (void *)jni_stub_int;          // GetVersion → return 0
  jni_vtable[6]   = (void *)FindClass;              // FindClass
  jni_vtable[15]  = (void *)ExceptionCheck;         // ExceptionOccurred (returns NULL = no exception)
  jni_vtable[17]  = (void *)ExceptionClear;         // ExceptionClear
  jni_vtable[21]  = (void *)NewGlobalRef;           // NewGlobalRef
  jni_vtable[22]  = (void *)DeleteGlobalRef;        // DeleteGlobalRef
  jni_vtable[23]  = (void *)DeleteLocalRef;         // DeleteLocalRef
  jni_vtable[25]  = (void *)NewLocalRef;            // NewLocalRef
  jni_vtable[31]  = (void *)jni_stub;               // GetObjectClass → NULL
  jni_vtable[33]  = (void *)GetMethodID;            // GetMethodID
  jni_vtable[34]  = (void *)CallObjectMethod;       // CallObjectMethod
  jni_vtable[37]  = (void *)CallBooleanMethod;      // CallBooleanMethod
  jni_vtable[49]  = (void *)CallIntMethod;          // CallIntMethod
  jni_vtable[61]  = (void *)CallVoidMethod;         // CallVoidMethod
  jni_vtable[94]  = (void *)GetFieldID;             // GetFieldID
  jni_vtable[95]  = (void *)GetObjectField;         // GetObjectField
  jni_vtable[100] = (void *)GetIntField;            // GetIntField
  jni_vtable[113] = (void *)GetStaticMethodID;      // GetStaticMethodID
  jni_vtable[114] = (void *)CallStaticObjectMethod; // CallStaticObjectMethod
  jni_vtable[117] = (void *)CallStaticBooleanMethod;// CallStaticBooleanMethod
  jni_vtable[129] = (void *)CallStaticIntMethod;    // CallStaticIntMethod
  jni_vtable[141] = (void *)CallStaticVoidMethod;   // CallStaticVoidMethod
  jni_vtable[144] = (void *)GetStaticFieldID;       // GetStaticFieldID
  jni_vtable[145] = (void *)GetObjectField;         // GetStaticObjectField
  jni_vtable[150] = (void *)GetStaticIntField;      // GetStaticIntField
  jni_vtable[167] = (void *)NewStringUTF;           // NewStringUTF
  jni_vtable[168] = (void *)GetStringUTFLength;     // GetStringUTFLength
  jni_vtable[169] = (void *)GetStringUTFChars;      // GetStringUTFChars
  jni_vtable[170] = (void *)ReleaseStringUTFChars;  // ReleaseStringUTFChars
  jni_vtable[171] = (void *)GetArrayLength;         // GetArrayLength
  jni_vtable[172] = (void *)NewObjectArray;         // NewObjectArray
  jni_vtable[173] = (void *)GetObjectArrayElement;  // GetObjectArrayElement
  jni_vtable[174] = (void *)SetObjectArrayElement;  // SetObjectArrayElement
  jni_vtable[214] = (void *)jni_stub_int;           // RegisterNatives → return 0 (JNI_OK)
  jni_vtable[219] = (void *)GetJavaVM;              // GetJavaVM (offset 1752)
  jni_vtable[227] = (void *)ExceptionCheck;         // ExceptionCheck

  g_jni_env.functions = jni_vtable;

  // JavaVM vtable — indices from Android NDK jni.h (JNIInvokeInterface struct)
  // [0]-[2] are reserved (already filled with stubs)
  jvm_vtable[3] = (void *)jni_stub_int;        // DestroyJavaVM
  jvm_vtable[4] = (void *)AttachCurrentThread;  // AttachCurrentThread
  jvm_vtable[5] = (void *)DetachCurrentThread;  // DetachCurrentThread
  jvm_vtable[6] = (void *)GetEnv;               // GetEnv
  jvm_vtable[7] = (void *)AttachCurrentThread;  // AttachCurrentThreadAsDaemon

  g_jni_vm.functions = jvm_vtable;
}
