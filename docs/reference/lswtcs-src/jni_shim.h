/*
 * jni_shim.h -- Fake JNI for LSWTCS
 *
 * Provides minimal JavaVM and JNIEnv with fake vtables.
 */

#ifndef __JNI_SHIM_H__
#define __JNI_SHIM_H__

#include <stdint.h>

typedef void *jobject;
typedef void *jclass;
typedef void *jstring;
typedef void *jmethodID;
typedef void *jfieldID;
typedef void *jobjectArray;
typedef int jint;
typedef int jboolean;
typedef float jfloat;

typedef struct {
  void **functions;
} JNIEnv_fake;

typedef struct {
  void **functions;
} JavaVM_fake;

extern JNIEnv_fake g_jni_env;
extern JavaVM_fake g_jni_vm;

void jni_shim_init(void);

#endif
