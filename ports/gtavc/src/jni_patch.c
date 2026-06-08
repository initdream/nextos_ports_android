/* jni_patch.c — JNIEnv fake + métodos NVEvent (GTA VC), Linux/SDL2/armhf.
 *
 * Portado de gtactw_vita/loader/jni_patch.c (Andy Nguyen, MIT).
 * Trocas Vita->Linux:
 *   - sceCtrl/sceTouch  -> SDL_GameController
 *   - vitaGL            -> SDL2 GL (janela + contexto criados em InitEGLAndGLES2)
 *   - sceAppUtil locale -> 0 (inglês)
 *   - sceIoRemove       -> remove()
 *   - DATA_PATH/OBB     -> pasta do port + main.<ver>.<pkg>.obb
 * A vtable JNIEnv (offsets 0x18..0x35C) é ARM 32-bit — idêntica no armhf.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>

#include "error.h"
#include "so_util.h"
#include "util.h"
#include "jni_patch.h"

#ifndef DATA_PATH
#define DATA_PATH "/storage/roms/ports/gtavc"
#endif

/* janela/GL globais (criados no 1º InitEGLAndGLES2) */
static SDL_Window *g_window = NULL;
static SDL_GLContext g_glctx = NULL;
static SDL_GameController *g_pad = NULL;
volatile unsigned g_gl_draw_arrays_frame = 0;
volatile unsigned g_gl_draw_elements_frame = 0;
volatile int g_current_framebuffer = 0;
volatile unsigned g_current_fbo_color_tex = 0;
volatile int g_swap_frame = 0;

static void present_fbo_texture(GLuint tex, int win_w, int win_h, int flip_y) {
  static GLuint prog = 0;
  static GLuint vbo = 0;
  static GLint attr_pos = -1;
  static GLint attr_uv = -1;
  static GLint uni_tex = -1;
  static int ready = 0;

  if (!ready) {
    const char *vs_src =
        "attribute vec2 aPos;\n"
        "attribute vec2 aUV;\n"
        "varying vec2 vUV;\n"
        "void main() {\n"
        "  vUV = aUV;\n"
        "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";
    const char *fs_src =
        "precision mediump float;\n"
        "varying vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(uTex, vUV);\n"
        "}\n";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    prog = glCreateProgram();
    glShaderSource(vs, 1, &vs_src, NULL);
    glShaderSource(fs, 1, &fs_src, NULL);
    glCompileShader(vs);
    glCompileShader(fs);
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "aPos");
    glBindAttribLocation(prog, 1, "aUV");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    attr_pos = 0;
    attr_uv = 1;
    uni_tex = glGetUniformLocation(prog, "uTex");
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
    ready = 1;
  }

  GLint prev_program = 0, prev_active_tex = 0, prev_tex_binding = 0;
  GLint prev_fbo = 0, prev_array_buffer = 0;
  GLint prev_vp[4] = {0, 0, 0, 0};
  GLboolean prev_depth = GL_FALSE, prev_blend = GL_FALSE;
  glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_array_buffer);
  glGetIntegerv(GL_VIEWPORT, prev_vp);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_tex);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex_binding);
  prev_depth = glIsEnabled(GL_DEPTH_TEST);
  prev_blend = glIsEnabled(GL_BLEND);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, win_w, win_h);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glUseProgram(prog);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(uni_tex, 0);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  const GLfloat v0 = flip_y ? 1.0f : 0.0f;
  const GLfloat v1 = flip_y ? 0.0f : 1.0f;
  const GLfloat quad[] = {
    -1.0f, -1.0f, 0.0f, v0,
     1.0f, -1.0f, 1.0f, v0,
    -1.0f,  1.0f, 0.0f, v1,
     1.0f,  1.0f, 1.0f, v1,
  };
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
  glEnableVertexAttribArray((GLuint)attr_pos);
  glEnableVertexAttribArray((GLuint)attr_uv);
  glVertexAttribPointer((GLuint)attr_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)0);
  glVertexAttribPointer((GLuint)attr_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDisableVertexAttribArray((GLuint)attr_pos);
  glDisableVertexAttribArray((GLuint)attr_uv);
  glBindTexture(GL_TEXTURE_2D, prev_tex_binding);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)prev_array_buffer);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
  glActiveTexture((GLenum)prev_active_tex);
  if (prev_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
  if (prev_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
  glUseProgram((GLuint)prev_program);
  glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
}

static void get_window_size_fallback(int *win_w, int *win_h) {
  *win_w = 0;
  *win_h = 0;
  if (g_window)
    SDL_GetWindowSize(g_window, win_w, win_h);
  if (*win_w <= 0 || *win_h <= 0) {
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
      *win_w = dm.w;
      *win_h = dm.h;
    }
  }
}

static GLuint g_game_backbuffer_tex = 0;
static int g_game_backbuffer_w = 0;
static int g_game_backbuffer_h = 0;
static int g_game_backbuffer_ready = 0;

static int viewport_has_world_detail_rgb(const GLint viewport[4]) {
  if (viewport[2] <= 0 || viewport[3] <= 0)
    return 0;

  const int xs[9] = {
      viewport[0] + viewport[2] / 2,
      viewport[0] + viewport[2] / 4,
      viewport[0] + (viewport[2] * 3) / 4,
      viewport[0] + viewport[2] / 4,
      viewport[0] + viewport[2] / 2,
      viewport[0] + (viewport[2] * 3) / 4,
      viewport[0] + viewport[2] / 4,
      viewport[0] + viewport[2] / 2,
      viewport[0] + (viewport[2] * 3) / 4,
  };
  const int ys[9] = {
      viewport[1] + viewport[3] / 4,
      viewport[1] + viewport[3] / 4,
      viewport[1] + viewport[3] / 4,
      viewport[1] + viewport[3] / 2,
      viewport[1] + viewport[3] / 2,
      viewport[1] + viewport[3] / 2,
      viewport[1] + (viewport[3] * 3) / 4,
      viewport[1] + (viewport[3] * 3) / 4,
      viewport[1] + (viewport[3] * 3) / 4,
  };

  int max_sum = 0;
  int min_sum = 765;
  for (int i = 0; i < 9; i++) {
    unsigned char px[4] = {0, 0, 0, 0};
    glReadPixels(xs[i], ys[i], 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    int sum = (int)px[0] + (int)px[1] + (int)px[2];
    if (sum > max_sum) max_sum = sum;
    if (sum < min_sum) min_sum = sum;
  }
  return max_sum > 24 && (max_sum - min_sum) > 18;
}

void vc_update_game_backbuffer_copy_from_viewport(const GLint viewport[4]) {
  if (viewport[2] <= 0 || viewport[3] <= 0)
    return;

  GLint prev_active_tex = 0;
  GLint prev_tex_binding = 0;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_tex);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex_binding);

  if (!g_game_backbuffer_tex)
    glGenTextures(1, &g_game_backbuffer_tex);
  glBindTexture(GL_TEXTURE_2D, g_game_backbuffer_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                   viewport[0], viewport[1], viewport[2], viewport[3], 0);
  g_game_backbuffer_w = viewport[2];
  g_game_backbuffer_h = viewport[3];
  g_game_backbuffer_ready = 1;

  glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex_binding);
  glActiveTexture((GLenum)prev_active_tex);
}

static char g_package[128] = "com.rockstargames.gtavc";
static int g_obb_ver = 11;
static char g_obb_path[256];
static char g_storage_root[256] = DATA_PATH;

void jni_set_package(const char *package, int obb_version) {
  if (package) snprintf(g_package, sizeof(g_package), "%s", package);
  g_obb_ver = obb_version;
  /* caminho do OBB no padrão Android: main.<ver>.<pkg>.obb dentro do storage */
  snprintf(g_obb_path, sizeof(g_obb_path), "/main.%d.%s.obb", g_obb_ver, g_package);
}

/* ============ method IDs (mesma tabela do gtactw_vita) ============ */
enum MethodIDs {
  M_UNKNOWN = 0,
  INIT_EGL_AND_GLES2,
  SWAP_BUFFERS,
  MAKE_CURRENT,
  UN_MAKE_CURRENT,
  GET_APP_LOCAL_VALUE,
  FILE_GET_ARCHIVE_NAME,
  DELETE_FILE,
  GET_DEVICE_INFO,
  GET_DEVICE_TYPE,
  GET_DEVICE_LOCALE,
  GET_GAMEPAD_TYPE,
  GET_GAMEPAD_BUTTONS,
  GET_GAMEPAD_AXIS,
  /* VC extras */
  GET_TOTAL_MEMORY, GET_AVAILABLE_MEMORY, GET_LOW_THRESHHOLD, GET_SCREEN_WIDTH_INCHES,
  GET_APP_ID, SCREEN_SET_WAKE_LOCK, SERVICE_APP_COMMAND, SERVICE_APP_COMMAND_VALUE,
  OPEN_FILE_ANDROID, CLOSE_FILE_ANDROID, SEEK_FILE_ANDROID, READ_FILE_ANDROID,
  HAS_APP_LOCAL_VALUE, SET_APP_LOCAL_VALUE, GET_PARAMETER, GET_SUPPORT_PAUSE_RESUME,
  SEND_STAT_EVENT, SET_NAME,
  PLAY_MOVIE, PLAY_MOVIE_IN_FILE, PLAY_MOVIE_IN_WINDOW, STOP_MOVIE,
  MOVIE_SET_SKIPPABLE, IS_MOVIE_PLAYING, MOVIE_KEEP_ASPECT_RATIO,
  MOVIE_SET_TEXT, MOVIE_DISPLAY_TEXT, MOVIE_CLEAR_TEXT, MOVIE_SET_TEXT_SCALE,
};

typedef struct { const char *name; enum MethodIDs id; } NameToMethodID;
static NameToMethodID name_to_method_ids[] = {
  { "InitEGLAndGLES2", INIT_EGL_AND_GLES2 },
  { "swapBuffers", SWAP_BUFFERS },
  { "makeCurrent", MAKE_CURRENT },
  { "unMakeCurrent", UN_MAKE_CURRENT },
  { "getAppLocalValue", GET_APP_LOCAL_VALUE },
  { "FileGetArchiveName", FILE_GET_ARCHIVE_NAME },
  { "DeleteFile", DELETE_FILE },
  { "GetDeviceInfo", GET_DEVICE_INFO },
  { "GetDeviceType", GET_DEVICE_TYPE },
  { "GetDeviceLocale", GET_DEVICE_LOCALE },
  { "GetGamepadType", GET_GAMEPAD_TYPE },
  { "GetGamepadButtons", GET_GAMEPAD_BUTTONS },
  { "GetGamepadAxis", GET_GAMEPAD_AXIS },
  { "GetTotalMemory", GET_TOTAL_MEMORY },
  { "GetAvailableMemory", GET_AVAILABLE_MEMORY },
  { "GetLowThreshhold", GET_LOW_THRESHHOLD },
  { "GetScreenWidthInches", GET_SCREEN_WIDTH_INCHES },
  { "GetAppId", GET_APP_ID },
  { "ScreenSetWakeLock", SCREEN_SET_WAKE_LOCK },
  { "ServiceAppCommand", SERVICE_APP_COMMAND },
  { "ServiceAppCommandValue", SERVICE_APP_COMMAND_VALUE },
  { "openFileAndroid", OPEN_FILE_ANDROID },
  { "closeFileAndroid", CLOSE_FILE_ANDROID },
  { "seekFileAndroid", SEEK_FILE_ANDROID },
  { "readFileAndroid", READ_FILE_ANDROID },
  { "hasAppLocalValue", HAS_APP_LOCAL_VALUE },
  { "setAppLocalValue", SET_APP_LOCAL_VALUE },
  { "getParameter", GET_PARAMETER },
  { "getSupportPauseResume", GET_SUPPORT_PAUSE_RESUME },
  { "SendStatEvent", SEND_STAT_EVENT },
  { "setName", SET_NAME },
  { "PlayMovie", PLAY_MOVIE },
  { "PlayMovieInFile", PLAY_MOVIE_IN_FILE },
  { "PlayMovieInWindow", PLAY_MOVIE_IN_WINDOW },
  { "StopMovie", STOP_MOVIE },
  { "MovieSetSkippable", MOVIE_SET_SKIPPABLE },
  { "IsMoviePlaying", IS_MOVIE_PLAYING },
  { "MovieKeepAspectRatio", MOVIE_KEEP_ASPECT_RATIO },
  { "MovieSetText", MOVIE_SET_TEXT },
  { "MovieDisplayText", MOVIE_DISPLAY_TEXT },
  { "MovieClearText", MOVIE_CLEAR_TEXT },
  { "MovieSetTextScale", MOVIE_SET_TEXT_SCALE },
};

static char fake_vm[0x1000];
static char fake_env[0x1000];
static void *natives;

static int jni_ret0(void) { return 0; }

/* ============ device info ============ */
int GetDeviceInfo(void) { return 0; }
int GetDeviceType(void) {
  /* bits: mem<<6 | cores<<2 | form. form: 0x1=phone, 0x2=TEGRA (reconhecido!) */
  return (192 << 6) | (2 << 2) | 0x2;
}
int GetDeviceLocale(void) { return 0; /* English */ }

/* ============ gamepad (SDL_GameController) ============ */
static void ensure_pad(void) {
  if (g_pad) return;
  static int logged = 0;
  for (int i = 0; i < SDL_NumJoysticks(); i++) {
    if (SDL_IsGameController(i)) { g_pad = SDL_GameControllerOpen(i); if (g_pad) break; }
  }
  if (!logged) { logged = 1;
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      int nj = SDL_NumJoysticks();
      fprintf(f, "[PAD] njoysticks=%d g_pad=%s", nj, g_pad ? "ABERTO" : "NULL");
      for (int i = 0; i < nj; i++)
        fprintf(f, " | js%d='%s' isGC=%d", i, SDL_JoystickNameForIndex(i),
                SDL_IsGameController(i));
      if (g_pad) fprintf(f, " | nome='%s'", SDL_GameControllerName(g_pad));
      fprintf(f, "\n"); fclose(f);
    }
  }
}
int GetGamepadType(int port) {
  if (port != 0) return -1;
  SDL_GameControllerUpdate();
  ensure_pad();
  return 8; /* PS3-style */
}
static int btn(SDL_GameControllerButton b) {
  return g_pad ? SDL_GameControllerGetButton(g_pad, b) : 0;
}

/* ===== hooks de OS_Gamepad* (.so) — o menu (CPad) usa esse path, não o JNI.
 * Forçamos "conectado" e alimentamos os botões pelo SDL_GameController. ===== */
int my_os_gamepad_isconnected(unsigned pad, int *out) {
  (void)pad;
  SDL_GameControllerUpdate();
  ensure_pad();
  if (out) *out = 8; /* OSGamepadType: PS3-style (qualquer != -1 = conectado) */
  return 1;
}
/* OS_GamepadButton(pad, idx): retorna o bit do botão `idx`. Mapeamento
 * convencional GTA/Android: 0=Cross/A 1=Circle/B 2=Square/X 3=Triangle/Y
 * 4=L1 5=R1 6=Select 7=Start 8=L3 9=R3 10=Up 11=Down 12=Left 13=Right. */
static const SDL_GameControllerButton GP_MAP[14] = {
  SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B,
  SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
  SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
  SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
  SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
  SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
  SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
};
int g_gp_debug_pulse = -1; /* idx p/ forçar pressionado (debug); -1 = nenhum */
static const char *GP_NAME[14] = {"A/Cross","B/Circle","X/Square","Y/Triangle",
  "L1","R1","Back/Select","Start","L3","R3","DpadUp","DpadDown","DpadLeft","DpadRight"};
int my_os_gamepad_button(unsigned pad, unsigned idx) {
  (void)pad;
  SDL_JoystickUpdate();
  SDL_GameControllerUpdate();
  ensure_pad();
  /* DIAGNÓSTICO: loga bordas de subida dos botões RAW do joystick (1x por poll,
   * quando idx==0) p/ descobrir o mapeamento real do adaptador PS2. */
  if (idx == 0 && g_pad) {
    SDL_Joystick *js = SDL_GameControllerGetJoystick(g_pad);
    if (js) {
      static int rprev[32] = {0};
      int nb = SDL_JoystickNumButtons(js);
      if (nb > 32) nb = 32;
      for (int b = 0; b < nb; b++) {
        int p = SDL_JoystickGetButton(js, b);
        if (p && !rprev[b]) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, ">>> RAW joystick button %d PRESSIONADO\n", b); fclose(f); }
        }
        rprev[b] = p;
      }
      /* loga hat (dpad) tb */
      static int hprev = 0;
      if (SDL_JoystickNumHats(js) > 0) {
        int h = SDL_JoystickGetHat(js, 0);
        if (h != hprev) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, ">>> RAW hat=0x%x\n", h); fclose(f); }
          hprev = h;
        }
      }
    }
  }
  if ((int)idx == g_gp_debug_pulse) return 1; /* debug auto-pulse */
  /* Lê RAW joystick (adaptador Twin USB PS2 — mapeamento GameController do SDL
   * é genérico/errado). game idx -> raw button. dpad via hat. */
  if (idx >= 14 || !g_pad) return 0;
  SDL_Joystick *js = SDL_GameControllerGetJoystick(g_pad);
  if (!js) return 0;
  /* game idx: 0=Cross 1=Circle 2=Square 3=Triangle 4=L1 5=R1 6=Sel 7=Start
   *           8=L3 9=R3 10=Up 11=Down 12=Left 13=Right */
  static const int RAW[10] = { 2, 1, 3, 0, 6, 7, 8, 9, 10, 11 };
  if (idx < 10) return SDL_JoystickGetButton(js, RAW[idx]) ? 1 : 0;
  int h = SDL_JoystickNumHats(js) > 0 ? SDL_JoystickGetHat(js, 0) : 0;
  switch (idx) {
    case 10: return (h & SDL_HAT_UP) ? 1 : 0;
    case 11: return (h & SDL_HAT_DOWN) ? 1 : 0;
    case 12: return (h & SDL_HAT_LEFT) ? 1 : 0;
    case 13: return (h & SDL_HAT_RIGHT) ? 1 : 0;
  }
  return 0;
}
/* Bitmask no MESMO layout dos índices do OS_GamepadButton (bit i = botão i):
 * 0=A 1=B 2=X 3=Y 4=L1 5=R1 6=Select 7=Start 8=L3 9=R3 10=Up 11=Down 12=Left
 * 13=Right. WarGamepad_GetGamepadButtons chama isto via CallIntMethod, e
 * AND_GamepadUpdate gera os eventos (LIB_InputEvent) por bit -> menu/accept. */
int GetGamepadButtons(int port) {
  (void)port;
  SDL_GameControllerUpdate();
  ensure_pad();
  static int logged = 0;
  if (!logged) { logged = 1;
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[JNI GetGamepadButtons] chamado (event path ATIVO)\n"); fclose(f); }
  }
  int m = 0;
  for (int i = 0; i < 14; i++)
    if (btn(GP_MAP[i])) m |= (1 << i);
  if (g_gp_debug_pulse >= 0 && g_gp_debug_pulse < 14) m |= (1 << g_gp_debug_pulse);
  return m;
}
float GetGamepadAxis(int port, int axis) {
  (void)port;
  if (!g_pad) return 0.0f;
  float v = 0.0f;
  switch (axis) {
    case 0: v = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX) / 32768.0f; break;
    case 1: v = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY) / 32768.0f; break;
    case 2: v = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTX) / 32768.0f; break;
    case 3: v = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTY) / 32768.0f; break;
    case 4: v = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32768.0f; break;
    case 5: v = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32768.0f; break;
    default: break;
  }
  if (fabsf(v) > 0.25f) return v;
  return 0.0f;
}

/* ============ GL (SDL2 — NÃO forçar driver; auto sobe no Mali fbdev) ============ */
int InitEGLAndGLES2(void) {
  if (g_window) return 1; /* já criado */
  SDL_DisplayMode dm;
  int w = 1280, h = 720;
  if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { w = dm.w; h = dm.h; }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
  g_window = SDL_CreateWindow("GTA Vice City", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, w, h,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
  if (!g_window) fatal_error("SDL_CreateWindow falhou: %s", SDL_GetError());
  g_glctx = SDL_GL_CreateContext(g_window);
  if (!g_glctx) fatal_error("SDL_GL_CreateContext falhou: %s", SDL_GetError());
  SDL_GL_MakeCurrent(g_window, g_glctx);
  SDL_GL_SetSwapInterval(1);
  debugPrintf("InitEGLAndGLES2: janela %dx%d ctx OK (tid=%lu, swapintvl=%d)\n",
              w, h, (unsigned long)SDL_ThreadID(), SDL_GL_GetSwapInterval());
  return 1;
}
int swapBuffers(void) {
  static int frame = 0;
  static int *visible_entities = NULL;
  static int *invisible_entities = NULL;
  static int *curr_area = NULL;
  static int *curr_level = NULL;
  static int *is_android_paused = NULL;
  static unsigned char *cutscene_running = NULL;
  static unsigned char *cutscene_processing = NULL;
  static unsigned char *menu_startup_req = NULL;
  static unsigned char *menu_shutdown_req = NULL;
  static int *menu_game_started_counter = NULL;
  static unsigned char *draw_fade_value = NULL;
  static unsigned char *draw_fade_red = NULL;
  static unsigned char *draw_fade_green = NULL;
  static unsigned char *draw_fade_blue = NULL;
  static unsigned char *still_to_fade_out = NULL;
  static unsigned char *just_loaded_dont_fade = NULL;
  static void **visible_entity_ptrs = NULL;
  static void **invisible_entity_ptrs = NULL;
  static float *renderer_camera_pos = NULL;
  extern volatile int g_current_framebuffer;
  extern volatile unsigned g_current_fbo_color_tex;
  if (!visible_entities) {
    visible_entities = (int *)so_find_addr_safe("_ZN9CRenderer23ms_nNoOfVisibleEntitiesE");
    invisible_entities = (int *)so_find_addr_safe("_ZN9CRenderer25ms_nNoOfInVisibleEntitiesE");
    curr_area = (int *)so_find_addr_safe("_ZN5CGame8currAreaE");
    curr_level = (int *)so_find_addr_safe("_ZN5CGame9currLevelE");
    is_android_paused = (int *)so_find_addr_safe("IsAndroidPaused");
    cutscene_running = (unsigned char *)so_find_addr_safe("_ZN12CCutsceneMgr10ms_runningE");
    cutscene_processing = (unsigned char *)so_find_addr_safe("_ZN12CCutsceneMgr21ms_cutsceneProcessingE");
    menu_startup_req = (unsigned char *)so_find_addr_safe("_ZN12CMenuManager27m_bStartUpFrontEndRequestedE");
    menu_shutdown_req = (unsigned char *)so_find_addr_safe("_ZN12CMenuManager28m_bShutDownFrontEndRequestedE");
    menu_game_started_counter = (int *)so_find_addr_safe("_ZN12CMenuManager20m_GameStartedCounterE");
    draw_fade_value = (unsigned char *)so_find_addr_safe("_ZN5CDraw9FadeValueE");
    draw_fade_red = (unsigned char *)so_find_addr_safe("_ZN5CDraw7FadeRedE");
    draw_fade_green = (unsigned char *)so_find_addr_safe("_ZN5CDraw9FadeGreenE");
    draw_fade_blue = (unsigned char *)so_find_addr_safe("_ZN5CDraw8FadeBlueE");
    still_to_fade_out = (unsigned char *)so_find_addr_safe("StillToFadeOut");
    just_loaded_dont_fade = (unsigned char *)so_find_addr_safe("JustLoadedDontFadeInYet");
    visible_entity_ptrs = (void **)so_find_addr_safe("_ZN9CRenderer21ms_aVisibleEntityPtrsE");
    invisible_entity_ptrs = (void **)so_find_addr_safe("_ZN9CRenderer23ms_aInVisibleEntityPtrsE");
    renderer_camera_pos = (float *)so_find_addr_safe("_ZN9CRenderer20ms_vecCameraPositionE");
  }
  if (g_window) {
    GLint bound_fbo = g_current_framebuffer;
    GLint viewport[4] = {0, 0, 0, 0};
    GLint att_type = 0;
    GLint att_name = 0;
    GLboolean mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLboolean depth_mask = GL_TRUE;
    GLfloat clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (bound_fbo < 0)
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound_fbo);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
    int in_gameplay =
        curr_level && *curr_level >= 1 &&
        (!menu_startup_req || *menu_startup_req == 0) &&
        (!menu_shutdown_req || *menu_shutdown_req == 0);
    if (curr_level && *curr_level >= 1) {
      if (curr_area) *curr_area = 0;
      if (draw_fade_value) *draw_fade_value = 0;
      if (draw_fade_red) *draw_fade_red = 0;
      if (draw_fade_green) *draw_fade_green = 0;
      if (draw_fade_blue) *draw_fade_blue = 0;
      if (still_to_fade_out) *still_to_fade_out = 0;
      if (just_loaded_dont_fade) *just_loaded_dont_fade = 0;
    }
    if (bound_fbo != 0) {
      glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                            &att_type);
      glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                            &att_name);
    }
    if (bound_fbo == 0) {
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      glColorMask(mask[0], mask[1], mask[2], mask[3]);
      glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    }
    if (in_gameplay && bound_fbo == 0) {
      int win_w = 0, win_h = 0;
      int vis_count = visible_entities ? *visible_entities : -1;
      int has_world_visibility = (vis_count > 1);
      get_window_size_fallback(&win_w, &win_h);
      int has_source_pixels = viewport_has_world_detail_rgb(viewport);
      if (viewport[2] > 0 && viewport[3] > 0 &&
          win_w > 0 && win_h > 0 &&
          (viewport[2] < win_w || viewport[3] < win_h) &&
          has_world_visibility && has_source_pixels) {
        vc_update_game_backbuffer_copy_from_viewport(viewport);
        if ((frame < 900 && frame % 30 == 0) || frame % 240 == 0)
          debugPrintf("[present-copy] updated frame=%d src=%dx%d win=%dx%d tex=%u vis=%d\n",
                      frame + 1, g_game_backbuffer_w, g_game_backbuffer_h,
                      win_w, win_h, g_game_backbuffer_tex, vis_count);
      } else if (g_game_backbuffer_ready &&
                 ((frame < 900 && frame % 30 == 0) || frame % 240 == 0)) {
        debugPrintf("[present-copy] keep-last frame=%d drawsE=%u vp=%dx%d win=%dx%d vis=%d rgb=%d\n",
                    frame + 1, g_gl_draw_elements_frame,
                    viewport[2], viewport[3], win_w, win_h, vis_count, has_source_pixels);
      }
      if (g_game_backbuffer_ready && win_w > 0 && win_h > 0) {
        present_fbo_texture(g_game_backbuffer_tex, win_w, win_h, 0);
        if ((frame < 900 && frame % 30 == 0) || frame % 240 == 0)
          debugPrintf("[present-copy] presented frame=%d src=%dx%d win=%dx%d tex=%u\n",
                      frame + 1, g_game_backbuffer_w, g_game_backbuffer_h,
                      win_w, win_h, g_game_backbuffer_tex);
      }
    }
    if (frame < 10 || frame % 120 == 0) {
      unsigned char px[4] = {0, 0, 0, 0};
      glReadPixels(640, 360, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
      debugPrintf("pre-swap frame=%d fbo=%d tex=%u att=%d:%d vp=%d,%d,%d,%d cm=%d%d%d%d dm=%d drawsA=%u drawsE=%u vis=%d invis=%d area=%d level=%d paused=%d cut=%u/%u menu=%u/%u/%d center=%u,%u,%u,%u err=0x%x\n",
                  frame + 1, bound_fbo, g_current_fbo_color_tex, att_type, att_name,
                  viewport[0], viewport[1], viewport[2], viewport[3],
                  mask[0], mask[1], mask[2], mask[3], depth_mask,
                  g_gl_draw_arrays_frame, g_gl_draw_elements_frame,
                  visible_entities ? *visible_entities : -999,
                  invisible_entities ? *invisible_entities : -999,
                  curr_area ? *curr_area : -999,
                  curr_level ? *curr_level : -999,
                  is_android_paused ? *is_android_paused : -999,
                  cutscene_running ? *cutscene_running : 255,
                  cutscene_processing ? *cutscene_processing : 255,
                  menu_startup_req ? *menu_startup_req : 255,
                  menu_shutdown_req ? *menu_shutdown_req : 255,
                  menu_game_started_counter ? *menu_game_started_counter : -999,
                  px[0], px[1], px[2], px[3], glGetError());
      if (draw_fade_value || still_to_fade_out || just_loaded_dont_fade) {
        debugPrintf("fade frame=%d val=%u rgb=%u,%u,%u still=%u just=%u\n",
                    frame + 1,
                    draw_fade_value ? *draw_fade_value : 255,
                    draw_fade_red ? *draw_fade_red : 255,
                    draw_fade_green ? *draw_fade_green : 255,
                    draw_fade_blue ? *draw_fade_blue : 255,
                    still_to_fade_out ? *still_to_fade_out : 255,
                    just_loaded_dont_fade ? *just_loaded_dont_fade : 255);
      }
      if (curr_level && *curr_level >= 1 && visible_entity_ptrs && visible_entities) {
        int vis_count = *visible_entities;
        int invis_count = invisible_entities ? *invisible_entities : -1;
        int sample_count = vis_count < 8 ? vis_count : 8;
        debugPrintf("visible-sample frame=%d count=%d invis=%d cam=%.2f,%.2f,%.2f ptrs=%p invisptrs=%p\n",
                    frame + 1, vis_count, invis_count,
                    renderer_camera_pos ? renderer_camera_pos[0] : 0.0f,
                    renderer_camera_pos ? renderer_camera_pos[1] : 0.0f,
                    renderer_camera_pos ? renderer_camera_pos[2] : 0.0f,
                    visible_entity_ptrs, invisible_entity_ptrs);
        for (int i = 0; i < sample_count; i++) {
          void *ent = visible_entity_ptrs[i];
          if ((uintptr_t)ent < 0x10000) {
            debugPrintf("visible-ent frame=%d i=%d ent=%p invalid\n", frame + 1, i, ent);
            continue;
          }
          void *vtable = *(void **)ent;
          int16_t model_id = *(int16_t *)((uint8_t *)ent + 0x60);
          unsigned char type = *((unsigned char *)ent + 0x36);
          unsigned char status = *((unsigned char *)ent + 0x37);
          float *pos = (float *)((uint8_t *)ent + 0x30);
          debugPrintf("visible-ent frame=%d i=%d ent=%p vt=%p model=%d type=%u status=%u pos=%.2f,%.2f,%.2f\n",
                      frame + 1, i, ent, vtable, model_id, type, status,
                      pos[0], pos[1], pos[2]);
        }
      }
      if (frame >= 240) {
        static const int xs[3] = {160, 640, 1120};
        static const int ys[3] = {120, 360, 600};
        char line[256];
        int off = snprintf(line, sizeof(line), "pre-swap-grid frame=%d", frame + 1);
        for (int yy = 0; yy < 3; yy++) {
          for (int xx = 0; xx < 3; xx++) {
            unsigned char gp[4] = {0, 0, 0, 0};
            glReadPixels(xs[xx], ys[yy], 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, gp);
            off += snprintf(line + off, sizeof(line) - (size_t)off, " %u,%u,%u,%u",
                            gp[0], gp[1], gp[2], gp[3]);
            if (off >= (int)sizeof(line) - 24) break;
          }
        }
        debugPrintf("%s\n", line);
      }
    }
    int should_present_game_fbo =
        in_gameplay &&
        bound_fbo != 0 && g_current_fbo_color_tex != 0;
    if (should_present_game_fbo) {
      int win_w = 0, win_h = 0;
      get_window_size_fallback(&win_w, &win_h);
      if (win_w > 0 && win_h > 0) {
        present_fbo_texture((GLuint)g_current_fbo_color_tex, win_w, win_h, 1);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }
    }
    SDL_GL_SwapWindow(g_window);
    frame++;
    g_swap_frame = frame;
    g_gl_draw_arrays_frame = 0;
    g_gl_draw_elements_frame = 0;
    if (frame <= 10 || frame % 120 == 0)
      debugPrintf("swapBuffers frame=%d tid=%lu\n", frame, (unsigned long)SDL_ThreadID());
  }
  return 1;
}
int makeCurrent(void) {
  if (g_window && g_glctx) {
    if (SDL_GL_MakeCurrent(g_window, g_glctx) == 0)
      SDL_GL_SetSwapInterval(1); /* vsync na thread que renderiza (Es2Thread) */
  }
  return 1;
}
int unMakeCurrent(void) {
  /* libera o contexto na thread chamadora (init) para a render thread (Es2Thread)
   * poder fazer makeCurrent — senão eglMakeCurrent falha com EGL_BAD_ACCESS
   * (era no-op = causa raiz da tela preta). */
  if (g_window) SDL_GL_MakeCurrent(g_window, NULL);
  return 1;
}

/* ============ files / storage ============ */
char *getAppLocalValue(char *key) {
  if (key && strcmp(key, "STORAGE_ROOT") == 0) return g_storage_root;
  return NULL;
}
char *FileGetArchiveName(int type) {
  switch (type) {
    case 1: return g_obb_path;     /* main OBB */
    case 2: return (char *)"";     /* patch OBB (não temos) */
    default: return NULL;
  }
}
int DeleteFile(char *file) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", g_storage_root, file ? file : "");
  return remove(path) == 0 ? 1 : 0;
}

/* ============ JNI Call*MethodV dispatch ============ */
int CallBooleanMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  (void)env; (void)obj;
  switch (methodID) {
    case INIT_EGL_AND_GLES2: return InitEGLAndGLES2();
    case SWAP_BUFFERS: return swapBuffers();
    case MAKE_CURRENT: return makeCurrent();
    case UN_MAKE_CURRENT: return unMakeCurrent();
    case DELETE_FILE: return DeleteFile((char *)args[0]);
    default: return 0;
  }
}
float __attribute__((pcs("aapcs"))) CallFloatMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  (void)env; (void)obj;
  if (methodID == GET_GAMEPAD_AXIS) return GetGamepadAxis(args[0], args[1]);
  if (methodID == GET_SCREEN_WIDTH_INCHES) return 5.0f;
  return 0.0f;
}
int CallIntMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  (void)env; (void)obj;
  switch (methodID) {
    case GET_GAMEPAD_TYPE: return GetGamepadType(args[0]);
    case GET_GAMEPAD_BUTTONS: return GetGamepadButtons(args[0]);
    case GET_DEVICE_INFO: return GetDeviceInfo();
    case GET_DEVICE_TYPE: return GetDeviceType();
    case GET_DEVICE_LOCALE: return GetDeviceLocale();
    case GET_TOTAL_MEMORY: return 192 * 1024 * 1024;
    case GET_AVAILABLE_MEMORY: return 96 * 1024 * 1024;
    case GET_LOW_THRESHHOLD: return 128 * 1024 * 1024;
    case SERVICE_APP_COMMAND_VALUE: return 0;
    case IS_MOVIE_PLAYING: return 0;
    default: return 0;
  }
}
void *CallObjectMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  (void)env; (void)obj;
  switch (methodID) {
    case GET_APP_LOCAL_VALUE: return getAppLocalValue((char *)args[0]);
    case FILE_GET_ARCHIVE_NAME: return FileGetArchiveName(args[0]);
    case GET_APP_ID: return (void *)g_package;
    case GET_PARAMETER: return (void *)"";   /* nunca NULL (evita strcpy(NULL)) */
    case OPEN_FILE_ANDROID: return NULL;      /* stub: jogo cai no fopen direto */
    default: return (void *)"";               /* string vazia p/ qualquer object-method nao tratado */
  }
}
void CallVoidMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  (void)env; (void)obj;
  if (methodID == PLAY_MOVIE || methodID == PLAY_MOVIE_IN_FILE ||
      methodID == PLAY_MOVIE_IN_WINDOW || methodID == STOP_MOVIE ||
      methodID == MOVIE_SET_SKIPPABLE || methodID == MOVIE_KEEP_ASPECT_RATIO ||
      methodID == MOVIE_SET_TEXT || methodID == MOVIE_DISPLAY_TEXT ||
      methodID == MOVIE_CLEAR_TEXT || methodID == MOVIE_SET_TEXT_SCALE) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[movie] method=%d arg0=%p\n", methodID, args ? (void *)args[0] : NULL);
      fclose(f);
    }
  }
}
int GetMethodID(void *env, void *class, const char *name, const char *sig) {
  (void)env; (void)class;
  { FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[GetMethodID] %s  (%s)\n", name ? name : "?", sig ? sig : "?"); fclose(f); } }
  for (size_t i = 0; i < sizeof(name_to_method_ids) / sizeof(NameToMethodID); i++)
    if (strcmp(name, name_to_method_ids[i].name) == 0)
      return name_to_method_ids[i].id;
  return M_UNKNOWN;
}
void RegisterNatives(void *env, int r1, void *r2) { (void)env; (void)r1; natives = r2; }
void *NewGlobalRef(void) { return (void *)0x42424242; }
char *NewStringUTF(void *env, char *bytes) { (void)env; return bytes; }
char *GetStringUTFChars(void *env, char *string, int *isCopy) {
  (void)env; if (isCopy) *isCopy = 0; return string;
}
void *NVThreadGetCurrentJNIEnv(void) { return fake_env; }

/* stub default p/ qualquer método JNIEnv não-implementado: retorna 0 */
static int jni_default(void) { return 0; }
/* getters de campo-objeto: retornam "" (nunca NULL) p/ evitar strcpy(NULL) */
static void *jni_emptystr(void) { return (void *)""; }
static int jni_dummy_id(void) { return 1; } /* FieldID/MethodID dummy nao-zero */

/* === campos JNI (android.os.Build.*) — spoof p/ Galaxy S3 (Mali-400 MP) === */
static const char *field_names[128];
static int n_fields = 0;
static int FieldID_impl(void *env, void *clazz, const char *name, const char *sig) {
  (void)env; (void)clazz; (void)sig;
  { FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[FieldID] %s\n", name ? name : "?"); fclose(f); } }
  for (int i = 0; i < n_fields; i++)
    if (field_names[i] && name && strcmp(field_names[i], name) == 0) return i + 1;
  if (n_fields < 128) { field_names[n_fields] = name; return ++n_fields; }
  return 1;
}
static const char *build_value(const char *n) {
  if (!n) return "";
  if (strcmp(n, "MODEL") == 0) return "GT-I9300";
  if (strcmp(n, "MANUFACTURER") == 0) return "samsung";
  if (strcmp(n, "BRAND") == 0) return "samsung";
  if (strcmp(n, "DEVICE") == 0) return "m0";
  if (strcmp(n, "PRODUCT") == 0) return "m0xx";
  if (strcmp(n, "HARDWARE") == 0) return "smdk4x12";
  if (strcmp(n, "BOARD") == 0) return "smdk4x12";
  if (strcmp(n, "FINGERPRINT") == 0) return "samsung/m0xx/m0";
  if (strcmp(n, "RELEASE") == 0) return "4.1.2";
  return "";
}
static void *ObjectField_impl(void *env, void *obj, int fid) {
  (void)env; (void)obj;
  const char *n = (fid >= 1 && fid <= n_fields) ? field_names[fid - 1] : NULL;
  return (void *)build_value(n);
}
/* GetIntField: o jogo lê os campos `width`/`height` da surface/display. Se vier
 * lixo, ele cai no default 1024x600 e renderiza no canto. Retorna 1280x720
 * (= nossa janela) p/ o viewport/layout do menu baterem com a tela. */
static int GetIntField_impl(void *env, void *obj, int fid) {
  (void)env; (void)obj;
  const char *n = (fid >= 1 && fid <= n_fields) ? field_names[fid - 1] : NULL;
  if (n) {
    if (strcmp(n, "width") == 0) return 1280;
    if (strcmp(n, "height") == 0) return 720;
  }
  return 0;
}

int GetEnv(void *vm, void **env, int r2) {
  (void)vm; (void)r2;
  /* preenche TODA a vtable com jni_default (em vez de 'A') -> sem crash AAAA */
  for (unsigned i = 0; i + 4 <= sizeof(fake_env); i += 4)
    *(uintptr_t *)(fake_env + i) = (uintptr_t)jni_default;
  *(uintptr_t *)(fake_env + 0x00) = (uintptr_t)fake_env;
  *(uintptr_t *)(fake_env + 0x18) = (uintptr_t)jni_ret0;   // FindClass
  *(uintptr_t *)(fake_env + 0x44) = (uintptr_t)jni_ret0;   // ExceptionClear
  *(uintptr_t *)(fake_env + 0x54) = (uintptr_t)NewGlobalRef;
  *(uintptr_t *)(fake_env + 0x5C) = (uintptr_t)jni_ret0;   // DeleteLocalRef
  *(uintptr_t *)(fake_env + 0x84) = (uintptr_t)GetMethodID;
  *(uintptr_t *)(fake_env + 0x8C) = (uintptr_t)CallObjectMethodV;
  *(uintptr_t *)(fake_env + 0x98) = (uintptr_t)CallBooleanMethodV;
  *(uintptr_t *)(fake_env + 0xC8) = (uintptr_t)CallIntMethodV;
  *(uintptr_t *)(fake_env + 0xE0) = (uintptr_t)CallFloatMethodV;
  *(uintptr_t *)(fake_env + 0xF8) = (uintptr_t)CallVoidMethodV;
  *(uintptr_t *)(fake_env + 0x178) = (uintptr_t)jni_ret0;
  *(uintptr_t *)(fake_env + 0x1C4) = (uintptr_t)GetMethodID;        // GetStaticMethodID
  *(uintptr_t *)(fake_env + 0x1C8) = (uintptr_t)CallObjectMethodV;  // CallStaticObjectMethod
  *(uintptr_t *)(fake_env + 0x1CC) = (uintptr_t)CallObjectMethodV;  // CallStaticObjectMethodV
  *(uintptr_t *)(fake_env + 0x1D8) = (uintptr_t)CallBooleanMethodV; // CallStaticBooleanMethodV
  *(uintptr_t *)(fake_env + 0x208) = (uintptr_t)CallIntMethodV;     // CallStaticIntMethodV
  *(uintptr_t *)(fake_env + 0x238) = (uintptr_t)CallVoidMethodV;    // CallStaticVoidMethodV
  *(uintptr_t *)(fake_env + 0x178) = (uintptr_t)FieldID_impl;    // GetFieldID
  *(uintptr_t *)(fake_env + 0x17C) = (uintptr_t)ObjectField_impl;// GetObjectField
  *(uintptr_t *)(fake_env + 0x190) = (uintptr_t)GetIntField_impl;// GetIntField (width/height)
  *(uintptr_t *)(fake_env + 0x240) = (uintptr_t)FieldID_impl;    // GetStaticFieldID
  *(uintptr_t *)(fake_env + 0x244) = (uintptr_t)ObjectField_impl;// GetStaticObjectField
  *(uintptr_t *)(fake_env + 0x29C) = (uintptr_t)NewStringUTF;
  *(uintptr_t *)(fake_env + 0x2A4) = (uintptr_t)GetStringUTFChars;
  *(uintptr_t *)(fake_env + 0x2A8) = (uintptr_t)jni_ret0;  // ReleaseStringUTFChars
  *(uintptr_t *)(fake_env + 0x35C) = (uintptr_t)RegisterNatives;
  *env = fake_env;
  return 0;
}

void jni_load(void) {
  /* GTA VC: tira o "IsAndroidPaused" (1 por default) — igual CTW tinha */
  uintptr_t paused = so_find_addr_safe("IsAndroidPaused");
  if (paused) *(int *)paused = 0;

  if (!g_obb_path[0]) jni_set_package(g_package, g_obb_ver);

  /* fake_vm (JavaVM): preenche tudo com jni_default (em vez de 'A') */
  for (unsigned i = 0; i + 4 <= sizeof(fake_vm); i += 4)
    *(uintptr_t *)(fake_vm + i) = (uintptr_t)jni_default;
  *(uintptr_t *)(fake_vm + 0x00) = (uintptr_t)fake_vm;
  *(uintptr_t *)(fake_vm + 0x10) = (uintptr_t)GetEnv; /* AttachCurrentThread -> seta *env */
  *(uintptr_t *)(fake_vm + 0x18) = (uintptr_t)GetEnv; /* GetEnv */

  int (*JNI_OnLoad)(void *vm, void *reserved) = (void *)so_find_addr("JNI_OnLoad");
  debugPrintf("Chamando JNI_OnLoad...\n");
  JNI_OnLoad(fake_vm, NULL);

  if (!natives) fatal_error("RegisterNatives nunca foi chamado (natives=NULL)");
  /* natives[8] = função init do NVEvent (env, r1, init_graphics) */
  int (*init)(void *env, int r1, int init_graphics) = *(void **)((char *)natives + 8);
  debugPrintf("Chamando init do jogo (NVEventAppInit)...\n");
  init(fake_env, 0, 1);
}
