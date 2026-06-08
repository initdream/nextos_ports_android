#ifndef JNI_SHIM_H
#define JNI_SHIM_H
void jni_load(void);
void jni_init_input(void);
void *NVThreadGetCurrentJNIEnv(void);
#endif
