#include <stdlib.h>
/* sonic_jni.c — JNI driver estático do Sonic Mania (RSDKv5).
 * Fabrica um JNIEnv/JavaVM falsos e chama o fluxo nativo:
 *   JNI_OnLoad -> OpenGLNativeCalls_startEngine -> loop OpenGLNativeCalls_step.
 * Molde: ports/bully/src/jni_shim.c. Os métodos do JNIEnv começam em ret0;
 * a gente adiciona os que o engine chamar (RE iterativo: run->crash->add). */
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include "so_util.h"
#include "util.h" /* ret0 */
#include "opensles_shim.h"
#include <string.h>

static char fake_env[0x1000];
static char fake_vm[0x100];
static SDL_Window *g_win;
static SDL_GLContext g_ctx;
static char fake_thiz[8192];
static const char *g_datapath = "/storage/roms/ports/sonicmania";
static const char *g_methods[512];
static int g_nmethods;
static int method_id(const char *n) {
  for (int i = 0; i < g_nmethods; i++) if (strcmp(g_methods[i], n) == 0) return i + 1;
  g_methods[g_nmethods] = strdup(n); return ++g_nmethods;
}
static const char *method_name(int id) { return (id >= 1 && id <= g_nmethods) ? g_methods[id - 1] : "?"; }

/* ---- métodos JNIEnv que o engine chama de volta (stubs; logam p/ a RE) ---- */
static void *j_FindClass(void *e, const char *n) {
  (void)e; fprintf(stderr, "[jni] FindClass %s\n", n ? n : "?");
  return (void *)(uintptr_t)(0x1000 + method_id(n ? n : "?"));
}
static int j_GetMethodID(void *e, void *c, const char *n, const char *s) {
  (void)e; (void)c; (void)s; return method_id(n ? n : "?");
}
static int j_GetStaticMethodID(void *e, void *c, const char *n, const char *s) {
  (void)e; (void)c; fprintf(stderr, "[jni] GetStaticMethodID %s %s\n", n ? n : "?", s ? s : "?"); return 1;
}
static void j_CallVoidMethod(void *e, void *o, int id, ...) {
  (void)e; (void)o; fprintf(stderr, "[jni] CallVoid %s\n", method_name(id));
}
static int j_CallIntMethod(void *e, void *o, int id, ...) {
  (void)e; (void)o; const char *n = method_name(id);
  fprintf(stderr, "[jni] CallInt %s\n", n);
  if (strstr(n, "Width")) return 1280;
  if (strstr(n, "Height")) return 720;
  return 0;
}
static int j_CallBooleanMethod(void *e, void *o, int id, ...) {
  (void)e; (void)o; const char *n = method_name(id);
  int r = 0;
  if (strstr(n, "isRestartRequired")) r = 0;       /* NAO reiniciar */
  else if (strstr(n, "supported") || strstr(n, "Available") || strstr(n, "Enabled")) r = 0;
  fprintf(stderr, "[jni] CallBool %s -> %d\n", n, r); return r;
}
static void *j_CallObjectMethod(void *e, void *o, int id, ...) {
  (void)e; (void)o; const char *n = method_name(id);
  fprintf(stderr, "[jni] CallObject %s\n", n);
  if (strcmp(n, "getClassLoader") == 0) return (void *)0x100;
  if (strcmp(n, "loadClass") == 0 || strcmp(n, "findClass") == 0) return (void *)0x200;
  return (void *)0x1;
}
static const char *j_GetStringUTFChars(void *e, void *str, void *isCopy) {
  (void)e; if (isCopy) *(char *)isCopy = 0;
  /* jstrings falsos do CallObject (0x1/0x100/0x200/0x1000+) NAO sao char* validos;
   * GetCurrentNetflixProfileId faz strlen() no retorno -> crash. Devolve "" p/ eles. */
  if ((uintptr_t)str < 0x100000) return "";
  return (const char *)str;
}
static void *j_NewStringUTF(void *e, const char *s) { (void)e; return (void *)s; }
static int j_GetEnv(void *vm, void **env, int v) { (void)vm; (void)v; *env = fake_env; return 0; }
static int j_AttachCurrentThread(void *vm, void **env, void *a) { (void)vm; (void)a; *env = fake_env; return 0; }

static void *j_NewRef(void *e, void *obj) { (void)e; return obj; }
static void *j_GetObjectClass(void *e, void *obj) { (void)e; (void)obj; return (void *)0x1; }

static void build_env(void) {
  extern long (*g_jlog[256])(void);
  for (unsigned i = 0; i < 256; i++)
    ((uintptr_t *)fake_env)[i] = (uintptr_t)g_jlog[i];
  *(uintptr_t *)(fake_env + 0x00) = (uintptr_t)fake_env;
#define SET(off, fn) *(uintptr_t *)(fake_env + (off)) = (uintptr_t)(fn)
  SET(0x30, j_FindClass);          /* idx 6 */
  SET(0xA8, j_NewRef);             /* idx 21 NewGlobalRef */
  SET(0xB0, ret0);                 /* idx 22 DeleteGlobalRef */
  SET(0xB8, ret0);                 /* idx 23 DeleteLocalRef */
  SET(0xC8, j_NewRef);             /* idx 25 NewLocalRef */
  SET(0xF8, j_GetObjectClass);     /* idx 31 GetObjectClass */
  SET(0x108, j_GetMethodID);       /* idx 33 */
  SET(0x110, j_CallObjectMethod);  /* idx 34 */
  SET(0x188, j_CallIntMethod);     /* idx 49 */
  SET(0x1C8, j_CallBooleanMethod); /* idx ~ */
  SET(0x1E8, j_CallVoidMethod);    /* idx 61 */
  SET(0x1C0, j_GetStaticMethodID); /* aprox; ajusta na RE */
  SET(0x538, j_NewStringUTF);      /* idx 167 */
  SET(0x548, j_GetStringUTFChars); /* idx 169 */
  SET(0x118, j_CallObjectMethod);  /* idx 35 CallObjectMethodV */
  SET(0x190, j_CallIntMethod);     /* idx 50 CallIntMethodV */
  SET(0x1D0, j_CallBooleanMethod); /* CallBooleanMethodV aprox */
  SET(0x1F0, j_CallVoidMethod);    /* idx 62 CallVoidMethodV */
  SET(0x388, j_GetMethodID);       /* idx 113 GetStaticMethodID */
  SET(0x390, j_CallObjectMethod);  /* idx 114 CallStaticObjectMethod */
  SET(0x398, j_CallObjectMethod);  /* idx 115 CallStaticObjectMethodV */
  SET(0x3a8, j_CallBooleanMethod); /* idx 117 CallStaticBooleanMethod */
  SET(0x3b0, j_CallBooleanMethod); /* idx 118 CallStaticBooleanMethodV */
  SET(0x418, j_CallIntMethod);     /* CallStaticIntMethodV aprox */
  SET(0x468, j_CallVoidMethod);    /* idx 141 CallStaticVoidMethod */
  SET(0x470, j_CallVoidMethod);    /* idx 142 CallStaticVoidMethodV */
#undef SET
}

static void (*g_mixtobuf)(float *, unsigned int) = NULL;
static void audio_cb(void *ud, Uint8 *stream, int len) {
  (void)ud;
  if (g_mixtobuf) { g_mixtobuf((float *)stream, (unsigned)(len / 4));
    static int c=0; if(c++%50==0){ float *f=(float*)stream; int nz=0; for(int i=0;i<len/4;i++) if(f[i]>0.001f||f[i]<-0.001f) nz++; fprintf(stderr,"[audio] cb#%d nonzero=%d/%d\n",c,nz,len/4); } }
  else memset(stream, 0, len);
}

static void (*g_onkey)(void *, void *, int, int) = NULL;
static void (*g_copyslot)(void *, unsigned) = NULL;
static uintptr_t g_ctrl_base = 0;
static int g_ctrl_probe = 0;
static int sdl_key_to_android(int sc) {
  switch (sc) {
    case SDL_SCANCODE_UP: return 19; case SDL_SCANCODE_DOWN: return 20;
    case SDL_SCANCODE_LEFT: return 21; case SDL_SCANCODE_RIGHT: return 22;
    case SDL_SCANCODE_RETURN: return 108; case SDL_SCANCODE_ESCAPE: return 109;
    case SDL_SCANCODE_Z: case SDL_SCANCODE_SPACE: return 96;
    case SDL_SCANCODE_X: return 97; case SDL_SCANCODE_C: return 99; case SDL_SCANCODE_V: return 100;
    default: return 0;
  }
}
static int sdl_btn_to_android(int b) {
  switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return 19; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return 20;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return 21; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return 22;
    case SDL_CONTROLLER_BUTTON_A: return 96; case SDL_CONTROLLER_BUTTON_B: return 97;
    case SDL_CONTROLLER_BUTTON_X: return 99; case SDL_CONTROLLER_BUTTON_Y: return 100;
    case SDL_CONTROLLER_BUTTON_START: return 108; case SDL_CONTROLLER_BUTTON_BACK: return 109;
    default: return 0;
  }
}

void jni_run(void) {
  /* contexto GLES2 (no Android o Java cria; aqui criamos via SDL2->Mali fbdev) */
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  g_win = SDL_CreateWindow("Sonic Mania", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
  if (!g_win) fprintf(stderr, "[gl] CreateWindow FALHOU: %s\n", SDL_GetError());
  g_ctx = SDL_GL_CreateContext(g_win);
  if (!g_ctx) fprintf(stderr, "[gl] CreateContext FALHOU: %s\n", SDL_GetError());
  SDL_GL_MakeCurrent(g_win, g_ctx);
  SDL_GL_SetSwapInterval(0);
  fprintf(stderr, "[gl] contexto GLES2 win=%p ctx=%p\n", (void *)g_win, (void *)g_ctx);

  build_env();
  for (unsigned i = 0; i < sizeof(fake_vm) / sizeof(uintptr_t); i++)
    ((uintptr_t *)fake_vm)[i] = (uintptr_t)ret0;
  *(uintptr_t *)(fake_vm + 0x00) = (uintptr_t)fake_vm;
  *(uintptr_t *)(fake_vm + 0x20) = (uintptr_t)j_AttachCurrentThread; /* idx 4 */
  *(uintptr_t *)(fake_vm + 0x30) = (uintptr_t)j_GetEnv;              /* idx 6 */
  *(uintptr_t *)(fake_vm + 0x38) = (uintptr_t)j_AttachCurrentThread; /* idx 7 */

  uintptr_t onload = so_find_addr_safe("JNI_OnLoad");
  if (onload) {
    int v = ((int (*)(void *, void *))onload)(fake_vm, NULL);
    fprintf(stderr, "[drv] JNI_OnLoad => 0x%x\n", v);
  } else {
    fprintf(stderr, "[drv] JNI_OnLoad nao achado\n");
  }

  uintptr_t se = so_find_addr_safe("Java_com_netflix_NGP_SonicMania_OpenGLNativeCalls_startEngine");
  uintptr_t st = so_find_addr_safe("Java_com_netflix_NGP_SonicMania_OpenGLNativeCalls_step");
  fprintf(stderr, "[drv] startEngine=0x%lx step=0x%lx\n", (unsigned long)se, (unsigned long)st);
  if (!se) {
    fprintf(stderr, "[drv] startEngine NAO achado — abortando\n");
    return;
  }
  { extern void *SL_IID_ENGINE_shim, *SL_IID_PLAY_shim, *SL_IID_BUFFERQUEUE_shim, *SL_IID_ANDROIDSIMPLEBUFFERQUEUE_shim;
    SL_IID_ENGINE_shim = (void *)sl_IID_ENGINE; SL_IID_PLAY_shim = (void *)sl_IID_PLAY;
    SL_IID_BUFFERQUEUE_shim = (void *)sl_IID_BUFFERQUEUE; SL_IID_ANDROIDSIMPLEBUFFERQUEUE_shim = (void *)sl_IID_BUFFERQUEUE;
    fprintf(stderr, "[sl] SL_IID wired\n"); }
  fprintf(stderr, "[drv] chamando startEngine...\n");
  ((void (*)(void *, void *, int, int, void *, const char *, int, void *,
              int, int, int, int, int, int, int, int))se)(
      fake_env, fake_thiz, 1280, 720, fake_thiz, g_datapath, 0, fake_thiz,
      1280, 720, 424, 240, 60, 1, 0, 0);
  fprintf(stderr, "[drv] startEngine retornou\n");
  uintptr_t sgr = so_find_addr_safe("Java_com_netflix_NGP_SonicMania_MainActivity_setGameRunning");
  if (sgr) { fprintf(stderr, "[drv] setGameRunning(1)\n"); ((void(*)(void*,void*,int))sgr)(fake_env, fake_thiz, 1); }
  uintptr_t spa = so_find_addr_safe("Java_com_netflix_NGP_SonicMania_MainActivity_setPauseAudio");
  if (spa) { fprintf(stderr, "[drv] setPauseAudio(0)\n"); ((void(*)(void*,void*,int))spa)(fake_env, fake_thiz, 0); }
  uintptr_t s_run = so_find_addr_safe("_ZN4RSDK4Game7runningE");
  uintptr_t s_cont = so_find_addr_safe("_ZN4RSDK10FileStream16useRSDKContainerE");
  uintptr_t s_info = so_find_addr_safe("_ZN4RSDK4Game4infoE");
  uintptr_t s_scene = so_find_addr_safe("_ZN4RSDK5Stage12currentSceneE");
  uintptr_t s_scr = so_find_addr_safe("_ZN4RSDK8Graphics10screenListE");
  uintptr_t s_sinfo = so_find_addr_safe("_ZN4RSDK5Stage4infoE");
  uintptr_t s_folder = so_find_addr_safe("_ZN4RSDK5Stage13currentFolderE");
  fprintf(stderr, "[state] syms run=%p cont=%p info=%p\n", (void*)s_run, (void*)s_cont, (void*)s_info);
  { uintptr_t sa=so_find_addr_safe("_ZN4RSDK5Audio15deviceAvailableE"); uintptr_t si=so_find_addr_safe("_ZN4RSDK5Audio11initializedE");
    if(sa){ fprintf(stderr,"[audio] deviceAvailable era %d -> 1\n", *(int*)sa); *(int*)sa=1; }
    if(si){ fprintf(stderr,"[audio] initialized era %d -> 1\n", *(int*)si); *(int*)si=1; } }
  { uintptr_t ai = so_find_addr_safe("_ZN4RSDK3SKU18AndroidInputDevice4InitEv");
    if (ai) { fprintf(stderr, "[input] AndroidInputDevice::Init()\n"); ((void(*)(void))ai)(); }
    uintptr_t ci = so_find_addr_safe("_ZN4RSDK3SKU18AndroidInputDevice10Controller4InitEv");
    if (ci) { fprintf(stderr, "[input] Controller::Init()\n"); ((void(*)(void))ci)(); } }
  g_onkey = (void (*)(void *, void *, int, int))so_find_addr_safe("Java_com_netflix_NGP_SonicMania_MainActivity_OnKeyEvent");
  g_copyslot = (void (*)(void *, unsigned))so_find_addr_safe("_ZN4RSDK3SKU18AndroidInputDevice10CopyToSlotEh");
  { uintptr_t kc = so_find_addr_safe("Java_com_google_android_games_paddleboat_GameControllerManager_onKeyboardConnected");
    if (kc) { fprintf(stderr, "[input] onKeyboardConnected()\n"); ((int(*)(void*,void*,int))kc)(fake_env, fake_thiz, 0); }
  }
  fprintf(stderr, "[input] CopyToSlot=%p\n", (void *)g_copyslot);
  if (g_copyslot) { uintptr_t tb = (uintptr_t)g_copyslot - 0x17d9bc;
    g_ctrl_base = *(uintptr_t *)(tb + 0x490e18);
    fprintf(stderr, "[input] text_base=0x%lx controller_base=0x%lx\n", (unsigned long)tb, (unsigned long)g_ctrl_base); }
  g_ctrl_probe = getenv("SONIC_FORCEINPUT") != NULL;
  fprintf(stderr, "[input] OnKeyEvent=%p\n", (void *)g_onkey);
  /* ---- PATCH: GetCloudSaveConflictState() -> sempre 0 (port offline, sem cloud).
   * Sem isso ela retorna 1 e BLOQUEIA o PressButton (gate==1 = return antes de
   * checar botao). Original: ldr w0,[x0,#60]; ret  ->  mov w0,#0; ret ---- */
  { uintptr_t gc = so_find_addr_safe("_ZN4RSDK3SKU11UserStorage25GetCloudSaveConflictStateEv");
    uintptr_t tb0 = g_copyslot ? (uintptr_t)g_copyslot - 0x17d9bc : 0;
    so_make_text_writable();
    if (gc) { *(uint32_t*)gc = 0x52800000u; /* movz w0,#0 */
      fprintf(stderr, "[patch] GetCloudSaveConflictState -> 0 @%p\n", (void*)gc); }
    /* ---- PATCH MenuSetup_PrerollChecks: completar naturalmente em vez de forçar
     * initializedAPI (que pulava UIWaitSpinner_Hide e a ativação do menu -> spinner
     * preso, botões não aparecem). Com os 4 status forçados=OK, NOP nas 2 ramificações
     * dos gates de API mobile p/ sempre seguir: dispensa o spinner + seta initializedAPI
     * + retorna done -> MenuSetup_SingleUpdate path B seta selectionDisabled=false e
     * ativa o menu. 0x20f4c4 cbz w0,20f5e4 -> NOP ; 0x20f518 cbnz w0,20f134 -> NOP ---- */
    if (tb0) { *(uint32_t*)(tb0+0x20f4c4)=0xd503201fu; *(uint32_t*)(tb0+0x20f518)=0xd503201fu;
      fprintf(stderr, "[patch] PrerollChecks gates NOP @%p\n", (void*)(tb0+0x20f4c4)); }
    /* ---- PATCH crash da fase: destruir badnik (Player_Action_Enemy) chama
     * Stats::TryTrackStat->DummyStats::TrackStat que faz std::wstring_convert::from_bytes
     * e CRASHA na libc (string ruim). Stats são inúteis offline -> no-op (ret). ---- */
    if (tb0) { *(uint32_t*)(tb0+0x4453a4)=0xd65f03c0u; /* Stats::TryTrackStat ret */
      *(uint32_t*)(tb0+0x4457f8)=0xd65f03c0u; /* DummyStats::TrackStat ret */
      fprintf(stderr, "[patch] Stats::TryTrackStat/TrackStat -> ret (no-op)\n"); }
    so_make_text_executable(); so_flush_caches(); }
  { int nj = SDL_NumJoysticks(); fprintf(stderr, "[input] %d joysticks\n", nj);
    for (int i=0;i<nj;i++){ if (SDL_IsGameController(i)) { SDL_GameControllerOpen(i); fprintf(stderr,"[input] gamecontroller %d aberto\n",i);} else { SDL_JoystickOpen(i); fprintf(stderr,"[input] joystick RAW %d aberto (%s)\n",i,SDL_JoystickNameForIndex(i)); } } }
  fprintf(stderr, "[drv] entrando no loop step\n");
  for (long f = 0; st; f++) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) return;
      else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
        int kc = sdl_key_to_android(ev.key.keysym.scancode);
        if (kc && g_onkey) g_onkey(fake_env, fake_thiz, kc, ev.type == SDL_KEYDOWN ? 1 : 0);
      } else if (ev.type == SDL_CONTROLLERBUTTONDOWN || ev.type == SDL_CONTROLLERBUTTONUP) {
        fprintf(stderr, "[ev] CBUTTON %d %s\n", ev.cbutton.button, ev.type==SDL_CONTROLLERBUTTONDOWN?"down":"up");
        int kc = sdl_btn_to_android(ev.cbutton.button);
        if (kc && g_onkey) g_onkey(fake_env, fake_thiz, kc, ev.type == SDL_CONTROLLERBUTTONDOWN ? 1 : 0);
      } else if (ev.type == SDL_JOYBUTTONDOWN || ev.type == SDL_JOYBUTTONUP) {
        fprintf(stderr, "[ev] JOYBUTTON %d %s\n", ev.jbutton.button, ev.type==SDL_JOYBUTTONDOWN?"down":"up");
        /* mapa raw: 0=A 1=B 2=X 3=Y 9=START 8=SELECT (ajusta conforme log) */
        int kc=0; switch(ev.jbutton.button){case 0:kc=96;break;case 1:kc=97;break;case 2:kc=99;break;case 3:kc=100;break;case 9:case 6:kc=108;break;case 8:case 4:kc=109;break;}
        if (kc && g_onkey) g_onkey(fake_env, fake_thiz, kc, ev.type == SDL_JOYBUTTONDOWN ? 1 : 0);
      } else if (ev.type == SDL_JOYHATMOTION) {
        fprintf(stderr, "[ev] JOYHAT %d\n", ev.jhat.value);
        int v=ev.jhat.value; if(g_onkey){ g_onkey(fake_env,fake_thiz,19,(v&SDL_HAT_UP)?1:0); g_onkey(fake_env,fake_thiz,20,(v&SDL_HAT_DOWN)?1:0); g_onkey(fake_env,fake_thiz,21,(v&SDL_HAT_LEFT)?1:0); g_onkey(fake_env,fake_thiz,22,(v&SDL_HAT_RIGHT)?1:0);}
      } else if (ev.type == SDL_JOYAXISMOTION && (ev.jaxis.value>16000||ev.jaxis.value<-16000)) {
        fprintf(stderr, "[ev] JOYAXIS %d=%d\n", ev.jaxis.axis, ev.jaxis.value);
      } else if (ev.type == SDL_CONTROLLERDEVICEADDED) SDL_GameControllerOpen(ev.cdevice.which);
    }
    /* auto-aperto START/A SÓ na Title (p/ avançar a tela-título). Em QUALQUER
     * outra cena PARA (na Menu disparava transições; na gameplay START=pausa!). */
    if (g_onkey && s_folder && memcmp((char*)s_folder,"Title",6)==0) {
      long ph = f % 60;
      if (ph == 0) { g_onkey(fake_env, fake_thiz, 108, 1); g_onkey(fake_env, fake_thiz, 96, 1); }
      else if (ph == 4) { g_onkey(fake_env, fake_thiz, 108, 0); g_onkey(fake_env, fake_thiz, 96, 0); }
    }
    /* SONIC_AUTONEW: auto-navega o menu (aperta A) p/ testar o fluxo de jogo novo
     * Mania Mode->save->personagem->iniciar, sem precisar do controle do o autor. */
    if (g_onkey && getenv("SONIC_AUTONEW") && s_folder && memcmp((char*)s_folder,"Menu",5)==0) {
      static long mf=0; mf++;
      long ph=mf%120;
      if (ph==0 && mf<1500) { g_onkey(fake_env,fake_thiz,108,1); g_onkey(fake_env,fake_thiz,96,1);
        uintptr_t tb3=(uintptr_t)g_copyslot-0x17d9bc; uintptr_t ci=*(uintptr_t*)(tb3+0x4a76c8);
        fprintf(stderr,"[autonew] START+A (mf=%ld) ci A(d%d,p%d) Start(d%d,p%d)\n",mf,
          ci?((int*)ci)[12]:-1,ci?((int*)ci)[13]:-1,ci?((int*)ci)[30]:-1,ci?((int*)ci)[31]:-1); }
      else if (ph==8) { g_onkey(fake_env,fake_thiz,108,0); g_onkey(fake_env,fake_thiz,96,0); }
    }

    { static uintptr_t sa2=0; if(!sa2) sa2=so_find_addr_safe("_ZN4RSDK5Audio15deviceAvailableE"); if(sa2)*(int*)sa2=1; }
    /* ---- FIX SAVE (cloud): no build Netflix, salvar usa JniWrapper::CloudSave que
     * registra um callback (id no contador 0x4a7088) e chama um método Java que NÃO
     * existe no port -> o callback nunca é entregue -> SaveGame_SaveFile (write) trava
     * o "novo jogo". Entregamos os callbacks void pendentes via CallCallback(id,...)
     * (a fila é drenada por ExecuteCallbacks no loop do engine) -> o save "completa"
     * e o fluxo segue p/ a cutscene+fase. ids de data-callback (CloudLoad) são no-op
     * no mapa void (seguros). ---- */
    { uintptr_t tb2=(uintptr_t)g_copyslot-0x17d9bc;
      long cur=*(long*)(tb2+0x4a7088); static long flushed=0;
      if (cur>flushed) {
        static uintptr_t ccb=0; if(!ccb) ccb=so_find_addr_safe("Java_com_netflix_NGP_SonicMania_MainActivity_CallCallback");
        if (ccb) while (flushed<cur) { ((void(*)(void*,void*,long,long,long,long))ccb)(fake_env,fake_thiz,flushed,0,0,0); flushed++; }
        else flushed=cur;
      }
    }
    /* ---- FIX (ANTES do step): título trava em WaitForConflictState (espera um
     * cloud-save conflict que nunca chega no port offline). Força o estado p/
     * PressButton ANTES do step p/ que PressButton esteja ativo quando o EDGE do
     * press chega (gate GetCloudSaveConflictState==0 libera a checagem de botão). ---- */
    { uintptr_t tb = (uintptr_t)g_copyslot - 0x17d9bc;
      static uintptr_t getent=0; if(!getent) getent=so_find_addr_safe("_ZN4RSDK12ObjectSystem9GetEntityEt");
      if (getent) { uintptr_t ent=((uintptr_t(*)(unsigned))getent)(0);
        if (ent) { uintptr_t *st_field=(uintptr_t*)(ent+96);
          if (*st_field == tb+0x31c454) { *st_field = tb+0x31c5f4; /* WaitForConflictState->PressButton */
            static int once=0; if(!once){once=1;fprintf(stderr,"[fix] forcado WaitForConflictState->PressButton\n");} } } } }
    ((void (*)(void *, void *, float))st)(fake_env, fake_thiz, 60.0f);
    /* ---- DIAG PressButton: a cada frame, se estado==PressButton, amostra
     * controller[0].press + self->timer[112] + gate, p/ ver se o press é detectado ---- */
    { uintptr_t tb = (uintptr_t)g_copyslot - 0x17d9bc;
      uintptr_t ci = *(uintptr_t*)(tb + 0x4a76c8);
      static uintptr_t getent=0; if(!getent) getent=so_find_addr_safe("_ZN4RSDK12ObjectSystem9GetEntityEt");
      static int max_press=0, in_pb=0; static int last_timer=-1;
      if (getent) { uintptr_t ent=((uintptr_t(*)(unsigned))getent)(0);
        if (ent) { uintptr_t stp=*(uintptr_t*)(ent+96);
          if (stp==tb+0x31c5f4) { in_pb=1;
            if (ci){ int *p=(int*)ci; for(int k=0;k<12;k++) if(p[k*3+1]) max_press=1; }
            int timer=*(int*)(ent+112);
            if (timer!=last_timer && (timer<6 || timer%60==0)) {
              uintptr_t gatep=*(uintptr_t*)(tb+0x4a7938);
              int gr = gatep?((int(*)(void))gatep)():-9;
              fprintf(stderr,"[pb] timer=%d max_press=%d gate=%d ci_start(d%d,p%d)\n",
                timer,max_press, gr, ci?((int*)ci)[30]:-1, ci?((int*)ci)[31]:-1);
              last_timer=timer; }
          } else if (in_pb && stp!=tb+0x31c5f4) {
            const char*nm="?"; if(stp==tb+0x31cafc)nm="FadeToScene"; else if(stp==tb+0x31cb2c)nm="FadeToVideo";
            fprintf(stderr,"[pb] SAIU de PressButton -> %s (0x%lx)\n",nm,(unsigned long)(stp-tb)); in_pb=0; }
        }
      }
    }
    /* ---- FIX MenuSetup: o port offline nao tem save/options no storage -> os
     * callbacks setam globals->saveLoaded/optionsLoaded = STATUS_ERROR(500) e a
     * MenuSetup_InitAPI fica presa na tela preta com o spinner. Força ambos p/
     * STATUS_OK(200) = save novo/vazio -> MenuSetup avança p/ o menu real.
     * globals = *(text_base+0x4a76d8); saveLoaded@0x100a0; optionsLoaded@0x414bc ---- */
    if (s_folder && memcmp((char*)s_folder,"Menu",5)==0) {
      uintptr_t tb = (uintptr_t)g_copyslot - 0x17d9bc;
      uintptr_t gp = *(uintptr_t*)(tb + 0x4a76d8);
      if (gp) { /* MenuSetup_PrerollChecks exige 4 campos == STATUS_OK(200): */
        if (*(int*)(gp+0x100a0)!=200) *(int*)(gp+0x100a0)=200; /* saveLoaded */
        if (*(int*)(gp+0x414bc)!=200) *(int*)(gp+0x414bc)=200; /* optionsLoaded */
        if (*(int*)(gp+0x441778)!=200) *(int*)(gp+0x441778)=200; /* replayTableLoaded */
        if (*(int*)(gp+0x441780)!=200) *(int*)(gp+0x441780)=200; /* taTableLoaded */ }
      /* Forçar selectionDisabled=false (offset +228) nos UIControls do menu: o force
       * de initializedAPI pulou o `mainMenu->selectionDisabled=false`, deixando o menu
       * sem processar input (isProcessingInput=0). Varre entidades; nas que parecem
       * UIControl (state @+96 aponta p/ .text) com selectionDisabled==1, zera. */
      { static uintptr_t ge2=0; if(!ge2) ge2=so_find_addr_safe("_ZN4RSDK12ObjectSystem9GetEntityEt");
        if (ge2) { uintptr_t tlo=tb, thi=tb+0x500000; /* text range aprox */
          for (int s=0;s<160;s++){ uintptr_t e=((uintptr_t(*)(unsigned))ge2)(s); if(!e)continue;
            uintptr_t st=*(uintptr_t*)(e+96);
            if (st>=tlo && st<thi && *(int*)(e+228)==1) { *(int*)(e+228)=0;
              static int once2=0; if(!once2){once2=1;fprintf(stderr,"[menufix] selectionDisabled->0 (slot %d)\n",s);} } } } }
      /* MenuSetup_PrerollChecks: se MenuSetup->initializedAPI(+16)!=0 retorna "done"
       * de cara, pulando TODOS os gates de API mobile (NotifyAutosave/Notifs/Dialogs).
       * MenuSetup = *(tb+0x4a7b20). So depois de initializedSaves(+20) setar. */
      { uintptr_t ms=*(uintptr_t*)(tb+0x4a7b20);
        /* NAO forçar initializedAPI: o patch dos gates do PrerollChecks deixa o menu
         * completar sozinho (dispensa spinner + ativa botões). Force só como fallback
         * tardio se travar >5s sem completar. */
        if (getenv("SONIC_FORCEAPI") && ms && *(int*)(ms+20)!=0 && *(int*)(ms+16)==0) {
          *(int*)(ms+16)=1; fprintf(stderr,"[menu] forcado MenuSetup->initializedAPI=1\n"); }
        if (ms && f%60==15) { uintptr_t fx=*(uintptr_t*)(ms+48);
          fprintf(stderr,"[menu] MS menuRet=%d menu=%d api=%d saves=%d | fxFade=0x%lx timer=%d\n",
            *(int*)(ms+8),*(int*)(ms+12),*(int*)(ms+16),*(int*)(ms+20),(unsigned long)fx, fx?*(int*)(fx+104):-1); } }
      /* scan entidades p/ achar UITransition (states MoveIn/Wait/MoveOut) e UIControl */
      if (f%120==45) { static uintptr_t ge=0; if(!ge) ge=so_find_addr_safe("_ZN4RSDK12ObjectSystem9GetEntityEt");
        if (ge) { struct{unsigned long o;const char*n;} S[]={{0x1aae84,"UITrans_MoveIn"},{0x1aae6c,"UITrans_Wait"},{0x1ab034,"UITrans_MoveOut"}};
          for(int s=0;s<48;s++){ uintptr_t e=((uintptr_t(*)(unsigned))ge)(s); if(!e)continue;
            uintptr_t *w=(uintptr_t*)e;
            for(int i=0;i<60;i++) for(unsigned j=0;j<3;j++) if(w[i]==tb+S[j].o)
              fprintf(stderr,"[menu] slot%d %s @field%d\n",s,S[j].n,i*8); } } }
      { uintptr_t ci=*(uintptr_t*)(tb+0x4a76c8); uintptr_t c1=g_ctrl_base+144;
        static int mpress0=0,mpress1=0;
        if(ci){int*p=(int*)ci;for(int k=0;k<12;k++)if(p[k*3+1])mpress0=1;}
        if(g_ctrl_base){int*p=(int*)c1;for(int k=0;k<12;k++)if(p[k*3+1])mpress1=1;}
        if(f%120==45){ uintptr_t uic=*(uintptr_t*)(tb+0x4a7ae8);
          fprintf(stderr,"[menuin] ctrl0_press=%d ctrl1_press=%d | UIControl proc=%d inputLocked=%d lockInput=%d\n",mpress0,mpress1,
            uic?*(int*)(uic+4):-1, uic?*(int*)(uic+8):-1, uic?*(int*)(uic+12):-1);mpress0=mpress1=0;} }
      if (f%60==15) {
        uintptr_t pv=*(uintptr_t*)(tb+0x490e08); uintptr_t us=pv?*(uintptr_t*)pv:0;
        fprintf(stderr,"[menu] us=0x%lx auth=%d storage=%d perm=%d conflict=%d | saveLd=%d optLd=%d\n",
          (unsigned long)us, us?*(int*)(us+64):-1, us?*(int*)(us+68):-1, us?*(int*)(us+72):-1, us?*(int*)(us+60):-1,
          gp?*(int*)(gp+0x100a0):-1, gp?*(int*)(gp+0x414bc):-1);
      }
    }
    /* ---- REDIRECT da cutscene de abertura: começar jogo novo no menu chama
     * SetScene("Cutscenes","Angel Island Zone")=#122 que trava (cutscene não roda
     * no port). Reescreve p/ Green Hill Zone 1 (#14, cat 'Mania Mode'=1) -> cai
     * direto na fase. (#123=AIZ Encore tb). Mantém o playerID escolhido no menu. ---- */
    if (getenv("SONIC_SKIPCUT")) { uintptr_t tb=(uintptr_t)g_copyslot-0x17d9bc; uintptr_t si=*(uintptr_t*)(tb+0x4a7098);
      if (si) { unsigned short lp=*(unsigned short*)(si+36); char *fo=s_folder?(char*)s_folder:"";
        if (lp==122 || lp==123 || strcmp(fo,"AIZ")==0) {
          *(unsigned char*)(si+64)=1; *(unsigned short*)(si+36)=14; *(unsigned char*)(si+66)=0; /* state=LOAD */
          uintptr_t gp=*(uintptr_t*)(tb+0x4a76d8); if(gp){ *(int*)(gp+0)=0; if(*(int*)(gp+4)==0)*(int*)(gp+4)=1; }
          static int once=0; if(!once){once=1;fprintf(stderr,"[redir] Angel Island cutscene -> Green Hill Zone 1 (reload)\n");} } } }
    /* ---- LOAD DIRETO de uma fase (pula o menu mobile). SONIC_ZONE=14 (GHZ1).
     * globals: gameMode(+0)=0 Mania, playerID(+4)=1 Sonic. sceneInfo: activeCategory(+64),
     * listPos(+36), state(+66)=0 ENGINESTATE_LOAD. ---- */
    { const char *zs=getenv("SONIC_ZONE");
      if (zs && f==280) { int zone=atoi(zs);
        uintptr_t tb=(uintptr_t)g_copyslot-0x17d9bc;
        uintptr_t gp=*(uintptr_t*)(tb+0x4a76d8); uintptr_t si=*(uintptr_t*)(tb+0x4a7098);
        if (gp && si) {
          *(int*)(gp+0)=0;            /* gameMode = Mania */
          *(int*)(gp+4)=1;            /* playerID = Sonic */
          *(unsigned char*)(si+64)=1; /* activeCategory = Mania Mode */
          *(unsigned short*)(si+36)=(unsigned short)zone; /* listPos */
          *(unsigned char*)(si+66)=0; /* state = ENGINESTATE_LOAD */
          fprintf(stderr,"[zone] forcando load cena %d (GHZ1=14)\n",zone);
        }
      }
    }
    /* auto-RIGHT na gameplay SÓ em modo demo (SONIC_ZONE set). No fluxo normal o
     * jogador controla tudo. */
    if (g_onkey && getenv("SONIC_ZONE") && s_folder) { char *fo=(char*)s_folder;
      int gameplay = strcmp(fo,"Logos")&&strcmp(fo,"Title")&&strcmp(fo,"Menu")&&fo[0]>='A'&&fo[0]<='Z';
      if (gameplay) { static int held=0; if(!held){held=1; g_onkey(fake_env,fake_thiz,22,1); fprintf(stderr,"[play] segurando RIGHT (folder=%s)\n",fo);} }
    }
    /* ---- DUMP scene list 1x (achar uma zona p/ carregar direto) ---- */
    if (f==180) { uintptr_t tb=(uintptr_t)g_copyslot-0x17d9bc;
      uintptr_t si=*(uintptr_t*)(tb+0x4a7098);
      if (si) { uintptr_t ld=*(uintptr_t*)(si+8); int catc=*(unsigned char*)(si+65);
        fprintf(stderr,"[scn] sceneInfo=0x%lx listData=0x%lx catCount=%d activeCat=%d listPos=%d\n",
          (unsigned long)si,(unsigned long)ld,catc,*(unsigned char*)(si+64),*(unsigned short*)(si+36));
        /* stride do SceneListEntry: hash16+name32+folder16+id8+filter1 -> tenta 76 e 80 */
        int stride=72; uintptr_t e=ld;
        for (int i=0;i<140;i++){ char*nm=(char*)(e+16); char*fo=(char*)(e+48); char*id=(char*)(e+64);
          if(nm[0]<32||nm[0]>126){e+=stride;continue;}
          fprintf(stderr,"[scn] #%d name='%.20s' folder='%.12s' id='%.6s'\n",i,nm,fo,id); e+=stride; }
        /* categorias */
        uintptr_t lc=*(uintptr_t*)(si+16);
        for (int c=0;c<catc;c++){ char*cn=(char*)(lc+c*56+16);
          fprintf(stderr,"[scn] CAT%d '%.20s' start=%d end=%d\n",c,cn,
            *(unsigned short*)(lc+c*56+48),*(unsigned short*)(lc+c*56+50)); }
      }
    }
    { extern void opensles_shim_pump_callbacks(void); static int au=-1;
      if(au<0) au=getenv("SONIC_AUDIO")?1:0; /* áudio OFF por padrão (pump crasha; em debug) */
      if(au && f>600) opensles_shim_pump_callbacks(); } /* só após init estável */
    { extern int g_drawcount; static int last=0;
      if (f%30==0) { fprintf(stderr, "[loop] frame %ld draws=%d (+%d) glErr=0x%x\n", f, g_drawcount, g_drawcount-last, glGetError()); last=g_drawcount; }
      if (f%120==1) fprintf(stderr, "[state] running=%d container=%d info=%d %d %d %d\n",
          s_run?*(int*)s_run:-9, s_cont?*(unsigned char*)s_cont:255,
          s_info?((int*)s_info)[0]:-9, s_info?((int*)s_info)[1]:-9, s_info?((int*)s_info)[2]:-9, s_scene?*(int*)s_scene:-9);
        if (s_scr) { unsigned short *fb=(unsigned short*)s_scr; long nz=0; for(int i=0;i<1280*256;i++) if(fb[i]) nz++; fprintf(stderr,"[fb] screenList nonzero=%ld/%d\n", nz, 1280*256); }
        if (s_sinfo) { int *si=(int*)s_sinfo; fprintf(stderr,"[scene] info ints:"); for(int i=0;i<14;i++) fprintf(stderr," %d",si[i]); fprintf(stderr,"\n"); }
        if (s_folder) fprintf(stderr,"[scene] folder='%.16s'\n", (char*)s_folder); }
    SDL_GL_SwapWindow(g_win);
  }
}
