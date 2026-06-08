/* jni_patch.h — JNIEnv fake + bring-up NVEvent (GTA VC armhf, Linux/SDL2) */
#ifndef JNI_PATCH_H
#define JNI_PATCH_H

/* configura package + versão do OBB (usado por STORAGE_ROOT / FileGetArchiveName) */
void jni_set_package(const char *package, int obb_version);

/* monta o JNIEnv fake, chama JNI_OnLoad e dispara o init do jogo (NVEvent) */
void jni_load(void);

#endif
