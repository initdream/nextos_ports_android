/* F0: stubs das funcs SDL de áudio (não linkamos libSDL2 do device — seção corrompida).
 * Áudio real é F4 (rotear OpenSLES->SDL3 do jogo OU SDL2 device via dlsym). Por ora no-op. */
#include <stdint.h>
typedef uint32_t SDL_AudioDeviceID;
const char *SDL_GetError(void) { return ""; }
SDL_AudioDeviceID SDL_OpenAudioDevice(const char*a,int b,const void*c,void*d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
void SDL_PauseAudioDevice(SDL_AudioDeviceID d,int p){(void)d;(void)p;}
void SDL_LockAudioDevice(SDL_AudioDeviceID d){(void)d;}
void SDL_UnlockAudioDevice(SDL_AudioDeviceID d){(void)d;}
void SDL_CloseAudioDevice(SDL_AudioDeviceID d){(void)d;}
int SDL_GetNumAudioDevices(int c){(void)c;return 0;}

/* OpenSLES interface IDs (F0: dummies distintos; o opensles_shim compara por ponteiro) */
const void *SL_IID_ENGINE_shim = (void*)0x510E;
const void *SL_IID_PLAY_shim = (void*)0x510A;
const void *SL_IID_RECORD_shim = (void*)0x510D;
const void *SL_IID_VOLUME_shim = (void*)0x510F;
const void *SL_IID_ANDROIDSIMPLEBUFFERQUEUE_shim = (void*)0x510B;
