/*
 * main.c — GTA Vice City Android -> NextOS Mali-450 (armhf so-loader)
 *
 * Backbone Linux portado de gtactw_vita/loader/main.c (Vita) + idiomas do
 * framework nextos_ports_android (Linux). Plataforma = SDL2 + libMali + glibc.
 *
 * Fluxo: SDL_Init -> heap -> so_load/relocate/resolve -> patches -> init_array
 *        -> jni_load (monta JNIEnv fake, chama JNI_OnLoad, roda NVEvent app).
 *
 * O contexto GL NAO e criado aqui — o jogo o pede via o metodo JNI
 * INIT_EGL_AND_GLES2 (em jni_patch.c), igual no CTW.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <SDL2/SDL.h>

#include "error.h"
#include "imports.h"
#include "so_util.h"
#include "util.h"
#include "jni_patch.h"

/* === config do port === */
#ifndef SO_NAME
#define SO_NAME "/storage/roms/ports/gtavc/libGTAVC.so"
#endif
#ifndef GTAVC_PACKAGE
#define GTAVC_PACKAGE "com.rockstargames.gtavc"
#endif
#ifndef GTAVC_OBB_VER
#define GTAVC_OBB_VER 11 /* main.11.com.rockstargames.gtavc.obb */
#endif
#ifndef MEMORY_MB
#define MEMORY_MB 256 /* heap p/ o .so (libGTAVC ~3.9MB + bss/relocs) */
#endif

/* patches portados (audio/GL) — implementados em *_patch.c */
extern void patch_mpg123(void);
extern void patch_openal(void);
extern void patch_opengl(void);
extern void patch_game(void);

/* __sF / ctype init (vc_impl.c) */
extern void vc_impl_init(void);

/* crash handler (crash.c) */
extern void install_crash_handler(void);

static void apply_lowmem_runtime_prefs(void) {
  uint8_t *isLowMemoryDevice = (uint8_t *)so_find_addr_safe("isLowMemoryDevice");
  uint8_t *m_PrefsHighpolyModels = (uint8_t *)so_find_addr_safe("_ZN12CMenuManager21m_PrefsHighpolyModelsE");
  uint8_t *m_PrefsMobileEffects = (uint8_t *)so_find_addr_safe("_ZN12CMenuManager20m_PrefsMobileEffectsE");
  uint8_t *m_PrefsDynamicShadows = (uint8_t *)so_find_addr_safe("_ZN12CMenuManager21m_PrefsDynamicShadowsE");
  uint8_t *m_PrefsLODE = (uint8_t *)so_find_addr_safe("_ZN12CMenuManager10m_PrefsLODE");
  float *m_PrefsDrawDistance = (float *)so_find_addr_safe("_ZN12CMenuManager19m_PrefsDrawDistanceE");
  float *m_PrefsMobileResolution = (float *)so_find_addr_safe("_ZN12CMenuManager23m_PrefsMobileResolutionE");
  int *streamMemAvailable = (int *)so_find_addr_safe("_ZN10CStreaming18ms_memoryAvailableE");
  int *streamMemUsed = (int *)so_find_addr_safe("_ZN10CStreaming13ms_memoryUsedE");
  uint8_t *playingIntro = (uint8_t *)so_find_addr_safe("_ZN5CGame12playingIntroE");
  int *skipAllMPEGs = (int *)so_find_addr_safe("SkipAllMPEGs");
  uint8_t *cutsceneRunning = (uint8_t *)so_find_addr_safe("_ZN12CCutsceneMgr10ms_runningE");
  uint8_t *cutsceneProcessing = (uint8_t *)so_find_addr_safe("_ZN12CCutsceneMgr21ms_cutsceneProcessingE");
  uint8_t *cutsceneLoaded = (uint8_t *)so_find_addr_safe("_ZN12CCutsceneMgr9ms_loadedE");
  uint8_t *cutsceneAnimLoaded = (uint8_t *)so_find_addr_safe("_ZN12CCutsceneMgr13ms_animLoadedE");
  uint8_t *cutsceneSkipped = (uint8_t *)so_find_addr_safe("_ZN12CCutsceneMgr21ms_wasCutsceneSkippedE");
  uint8_t *processCutsceneOnly = (uint8_t *)so_find_addr_safe("_ZN6CWorld20bProcessCutsceneOnlyE");
  int *cutsceneLoadStatus = (int *)so_find_addr_safe("_ZN12CCutsceneMgr21ms_cutsceneLoadStatusE");
  uint8_t *drawFadeValue = (uint8_t *)so_find_addr_safe("_ZN5CDraw9FadeValueE");
  uint8_t *drawFadeRed = (uint8_t *)so_find_addr_safe("_ZN5CDraw7FadeRedE");
  uint8_t *drawFadeGreen = (uint8_t *)so_find_addr_safe("_ZN5CDraw9FadeGreenE");
  uint8_t *drawFadeBlue = (uint8_t *)so_find_addr_safe("_ZN5CDraw8FadeBlueE");
  uint8_t *stillToFadeOut = (uint8_t *)so_find_addr_safe("StillToFadeOut");
  uint8_t *justLoadedDontFade = (uint8_t *)so_find_addr_safe("JustLoadedDontFadeInYet");
  uint32_t *timeToStayFaded = (uint32_t *)so_find_addr_safe("TimeToStayFadedBeforeFadeOut");
  uint32_t *timeStartedFade = (uint32_t *)so_find_addr_safe("TimeStartedCountingForFade");
  uint8_t *menuStartupReq = (uint8_t *)so_find_addr_safe("_ZN12CMenuManager27m_bStartUpFrontEndRequestedE");
  uint8_t *menuShutdownReq = (uint8_t *)so_find_addr_safe("_ZN12CMenuManager28m_bShutDownFrontEndRequestedE");
  int *currLevel = (int *)so_find_addr_safe("_ZN5CGame9currLevelE");
  int *currArea = (int *)so_find_addr_safe("_ZN5CGame8currAreaE");

  if (isLowMemoryDevice) *isLowMemoryDevice = 1;
  int *isAndroidPaused = (int *)so_find_addr_safe("IsAndroidPaused");
  if (isAndroidPaused) *isAndroidPaused = 0;
  if (m_PrefsHighpolyModels) *m_PrefsHighpolyModels = 0;
  if (m_PrefsMobileEffects) *m_PrefsMobileEffects = 0;
  if (m_PrefsDynamicShadows) *m_PrefsDynamicShadows = 0;
  if (m_PrefsLODE) *m_PrefsLODE = 0;
  if (m_PrefsDrawDistance) *m_PrefsDrawDistance = 0.25f;
  if (m_PrefsMobileResolution) *m_PrefsMobileResolution = 0.42f;
  if (streamMemAvailable) *streamMemAvailable = 24 * 1024 * 1024;
  if (streamMemUsed && *streamMemUsed < 0) *streamMemUsed = 0;
  if (playingIntro) *playingIntro = 0;
  if (skipAllMPEGs) *skipAllMPEGs = 1;
  if (cutsceneRunning) *cutsceneRunning = 0;
  if (cutsceneProcessing) *cutsceneProcessing = 0;
  if (cutsceneLoaded) *cutsceneLoaded = 0;
  if (cutsceneAnimLoaded) *cutsceneAnimLoaded = 0;
  if (cutsceneSkipped) *cutsceneSkipped = 1;
  if (processCutsceneOnly) *processCutsceneOnly = 0;
  if (cutsceneLoadStatus) *cutsceneLoadStatus = 0;
  if (drawFadeValue) *drawFadeValue = 0;
  if (drawFadeRed) *drawFadeRed = 0;
  if (drawFadeGreen) *drawFadeGreen = 0;
  if (drawFadeBlue) *drawFadeBlue = 0;
  if (stillToFadeOut) *stillToFadeOut = 0;
  if (justLoadedDontFade) *justLoadedDontFade = 0;
  if (timeToStayFaded) *timeToStayFaded = 0;
  if (timeStartedFade) *timeStartedFade = 0;
  if (currLevel && *currLevel >= 1) {
    if (currArea) *currArea = 0;
    if (menuStartupReq) *menuStartupReq = 0;
    if (menuShutdownReq) *menuShutdownReq = 0;
  }
  debugPrintf("lowmem prefs: low=%p paused=%p highpoly=%p effects=%p shadows=%p lode=%p draw=%p res=%p streamAvail=%p streamUsed=%p intro=%p skipmpeg=%p cutrun=%p cutproc=%p cutload=%p cutstatus=%p cutonly=%p fade=%p still=%p just=%p menustart=%p menushut=%p\n",
              isLowMemoryDevice, isAndroidPaused, m_PrefsHighpolyModels, m_PrefsMobileEffects,
              m_PrefsDynamicShadows, m_PrefsLODE, m_PrefsDrawDistance,
              m_PrefsMobileResolution, streamMemAvailable, streamMemUsed,
              playingIntro, skipAllMPEGs, cutsceneRunning, cutsceneProcessing,
              cutsceneLoaded, cutsceneLoadStatus, processCutsceneOnly,
              drawFadeValue, stillToFadeOut, justLoadedDontFade,
              menuStartupReq, menuShutdownReq);
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  install_crash_handler();
  debugPrintf("=== GTA Vice City (Android) — NextOS Mali-450 armhf ===\n");

  /* SDL: video (janela/GL via Mali fbdev — NAO forcar driver), audio, controle */
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
    fatal_error("SDL_Init falhou: %s", SDL_GetError());
  debugPrintf("SDL_Init OK (video driver=%s)\n", SDL_GetCurrentVideoDriver());

  /* heap executavel p/ o loader (igual framework) */
  size_t heap_size = (size_t)MEMORY_MB * 1024 * 1024;
  void *heap = mmap(NULL, heap_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap == MAP_FAILED)
    fatal_error("Falha ao alocar heap de %d MB", MEMORY_MB);
  debugPrintf("Heap: %p (%d MB)\n", heap, MEMORY_MB);

  debugPrintf("Carregando %s...\n", SO_NAME);
  if (so_load(SO_NAME, heap, heap_size) < 0)
    fatal_error("Falha ao carregar %s", SO_NAME);
  debugPrintf("Carregado: text=%p+%zu data=%p+%zu\n", text_base, text_size,
              data_base, data_size);

  debugPrintf("Relocando (REL armhf)...\n");
  if (so_relocate() < 0)
    fatal_error("Falha ao relocar");

  debugPrintf("Resolvendo %zu imports...\n", dynlib_numfunctions);
  if (so_resolve(dynlib_functions, dynlib_numfunctions, 0) < 0)
    fatal_error("Falha ao resolver imports");

  /* hook OS_DebugBreak (0x2f52b8, Thumb): loga o caller (qual dos 4 sites) e
   * RETORNA (era usleep-loop infinito = HALT). */
  extern void hook_arm(uintptr_t addr, uintptr_t dst);
  extern void my_os_debugbreak(void);
  hook_arm((uintptr_t)text_base + 0x2f52b8 + 1, (uintptr_t)my_os_debugbreak);
  /* NvAPKOpen->NULL: força .img a vir dos arquivos SOLTOS (fopen) em vez do OBB
   * (que precisaria do .obb.idx). OBB-init ainda abre via fix_obb. */
  extern void *my_nvapkopen(const char *p);
  hook_arm((uintptr_t)text_base + 0x2fbc78 + 1, (uintptr_t)my_nvapkopen);
  /* al_print (OpenAL log) crasha em gLogFile NULL -> no-op */
  extern void my_al_print(void);
  hook_arm((uintptr_t)text_base + 0x2e12ec + 1, (uintptr_t)my_al_print);
  /* TextureDatabaseRuntime::Unload null-safe (double-unload no destrutor do menu) */
  extern void my_texdb_unload(void *thiz);
  hook_arm((uintptr_t)text_base + 0x293200 + 1, (uintptr_t)my_texdb_unload);
  /* GAMEPAD: o menu (CPad) usa OS_GamepadButton/IsConnected (não JNI). Hooka p/
   * forçar "conectado" e alimentar botões do SDL_GameController. */
  extern int my_os_gamepad_isconnected(unsigned, int *);
  extern int my_os_gamepad_button(unsigned, unsigned);
  hook_arm((uintptr_t)text_base + 0x2f54c0 + 1, (uintptr_t)my_os_gamepad_isconnected);
  hook_arm((uintptr_t)text_base + 0x2f54e0 + 1, (uintptr_t)my_os_gamepad_button);
  /* node pools (CPtrNode/CEntryInfoNode) -> malloc/free (sem limite de pool;
   * o pool esgotava no load do mundo -> NULL -> crash em CEntity::Add) */
  extern void *my_ptrnode_new(unsigned);
  extern void my_ptrnode_delete(void *);
  hook_arm((uintptr_t)text_base + 0x16567c + 1, (uintptr_t)my_ptrnode_new);    /* CPtrNode::new */
  hook_arm((uintptr_t)text_base + 0x1656e4 + 1, (uintptr_t)my_ptrnode_delete); /* CPtrNode::delete */
  hook_arm((uintptr_t)text_base + 0x13be64 + 1, (uintptr_t)my_ptrnode_new);    /* CEntryInfoNode::new */
  hook_arm((uintptr_t)text_base + 0x13becc + 1, (uintptr_t)my_ptrnode_delete); /* CEntryInfoNode::delete */
  /* Alguns DFFs de veiculo do dump Android quebram em hierarquia/env-map no Mali.
   * Mantem o clump carregado, mas pula o preprocess visual que causa SIGSEGV. */
  extern void my_vehicle_preprocess_noop(void *);
  extern void my_vehicle_setenv_noop(void *);
  extern void my_vehicle_load_colours_noop(void);
  extern void my_vehicle_choose_colour_default(void *, unsigned char *, unsigned char *);
  extern void my_vehicle_set_colour_noop(void *, unsigned char, unsigned char);
  extern void my_generate_one_random_car_noop(void);
  extern void my_generate_random_cars_noop(void);
  extern void my_phoneinfo_update_noop(void);
  extern void my_playerped_process_control_noop(void *);
  extern void my_request_special_model_guard(int, const char *, int);
  extern void my_set_mission_doesnt_require_model_guard(int);
  extern uint32_t my_entity_get_distance_base_guard(void *);
  extern void *my_multiply3x3_matrix_vector_guard(void *, void *, void *);
  extern void *my_multiply3x3_vector_matrix_guard(void *, void *, void *);
  extern void *my_matrix_vector_mul_guard(void *, void *, void *);
  extern int my_collision_test_line_box_guard(void *, void *);
  extern int my_entity_get_is_touching_vec_guard(void *, void *, uint32_t);
  extern int my_entity_get_is_on_screen_noop(void *);
  extern int my_camera_get_screen_fade_status_guard(void *);
  extern int my_world_test_sphere_sector_nohit(void);
  extern void my_bike_process_control_noop(void *);
  extern void my_automobile_process_control_noop(void *);
  extern void my_population_update_noop(int);
  extern void my_population_manage_noop(void);
  extern int my_is_clump_skinned_guard(void *);
  extern void my_plane_init_noop(void);
  extern int my_entity_has_prerender_effects_noop(void *);
  extern void my_objectdata_set_guard(int, void *);
  extern void my_clear_space_for_mission_entity_noop(void *, void *);
  extern void install_load_object_instance_hook(void);
  extern void install_renderer_trace_hooks(void);
  extern void install_renderer_scan_sector_list_trace_hook(void);
  extern void install_renderer_setup_entity_visibility_trace_hook(void);
  extern void install_renderer_render_one_trace_hook(void);
  extern void install_entity_render_trace_hook(void);
  extern void install_renderer_scanworld_guard_patches(void);
  extern void install_rw_guard_hooks(void);
  extern void *my_entity_get_bound_centre_safe(void *, void *);
  extern void *my_physical_get_bound_rect_safe(void *, void *);
  extern void my_cutscene_load_noop(const char *);
  extern void my_cutscene_setup_noop(void);
  extern void my_cutscene_update_noop(void);
  extern void my_cutscene_delete_noop(void);
  extern void my_cutscene_finish_noop(void);
  extern void my_cutscene_set_anim_noop(const char *, void *);
  extern void my_cutscene_set_anim_loop_noop(const char *);
  extern int my_cutscene_finished_yes(void);
  extern int my_cutscene_can_skip_yes(void);
  extern void my_rw_frame_destroy_hierarchy_noop(void *);
  extern void *my_rw_frame_clone_internal_abort(void *, void *);
  extern void *my_rp_atomic_clone_passthrough(void *);
  extern void *my_rp_atomic_destroy_noop(void *);
  extern void *my_rp_clump_clone_passthrough(void *);
  extern void *my_rp_clump_render_guard(void *);
  extern void my_ped_model_create_hit_col_skinned_noop(void *, void *);
  extern void my_ped_undress_noop(void *, const char *);
  extern void my_ped_play_footsteps_noop(void *);
  extern int my_ped_is_head_above_pos_guard(void *, float);
  extern void my_ped_render_noop(void *);
  extern void *my_visibility_render_fading_atomic_guard(void *, float);
  extern void my_particle_object_update_all_noop(void);
  extern void my_moving_things_update_noop(void);
  extern void my_rw_frame_sync_dirty_noop(void);
  extern void *my_anim_manager_blend_animation_guard(void *, int, int, uint32_t);
  extern void *my_anim_manager_add_animation_guard(void *, int, int);
  extern void *my_anim_manager_add_animation_and_sync_guard(void *, void *, int, int);
  extern void my_anim_assoc_set_delete_callback_guard(void *, void *, void *);
  extern void my_anim_assoc_set_finish_callback_guard(void *, void *, void *);
  extern void *my_animblend_clump_get_first_assoc_guard(void *, unsigned int);
  extern void my_animblend_clump_set_blend_deltas_guard(void *, unsigned int, uint32_t);
  extern void *my_animblend_clump_get_assoc_null(void *, unsigned int);
  extern void my_animblend_clumpdata_for_all_frames_guard(void *, void (*)(void *, void *), void *);
  extern void my_animblend_clump_update_animations_noop(void *, float, int);
  extern void my_playerped_set_real_move_anim_guard(void *);
  extern int my_rp_hanim_id_get_index_guard(void *, int);
  extern void *my_rp_hanim_hierarchy_get_matrix_array_guard(void *);
  extern void my_renderer_scan_sector_poly_noop(void *, int, void *);
  extern void install_rw_transform_guard_hooks(void);
  hook_arm((uintptr_t)text_base + 0x1fc049, (uintptr_t)my_vehicle_preprocess_noop); /* CVehicleModelInfo::PreprocessHierarchy */
  hook_arm((uintptr_t)text_base + 0x1fcf5d, (uintptr_t)my_vehicle_setenv_noop);     /* CVehicleModelInfo::SetEnvironmentMap */
  hook_arm((uintptr_t)text_base + 0x1fcbd9, (uintptr_t)my_vehicle_load_colours_noop); /* CVehicleModelInfo::LoadVehicleColours */
  hook_arm((uintptr_t)text_base + 0x1fc961, (uintptr_t)my_vehicle_choose_colour_default); /* CVehicleModelInfo::ChooseVehicleColour */
  hook_arm((uintptr_t)text_base + 0x1fc8dd, (uintptr_t)my_vehicle_set_colour_noop);  /* CVehicleModelInfo::SetVehicleColour */
  hook_arm((uintptr_t)text_base + 0x0e4059, (uintptr_t)my_generate_one_random_car_noop); /* CCarCtrl::GenerateOneRandomCar */
  hook_arm((uintptr_t)text_base + 0x0e5709, (uintptr_t)my_generate_random_cars_noop); /* CCarCtrl::GenerateRandomCars */
  hook_arm((uintptr_t)text_base + 0x0f5b5d, (uintptr_t)my_phoneinfo_update_noop); /* CPhoneInfo::Update */
  hook_arm((uintptr_t)text_base + 0x1bf5b1, (uintptr_t)my_playerped_process_control_noop); /* CPlayerPed::ProcessControl guarded window */
  hook_arm((uintptr_t)text_base + 0x1f8f2d, (uintptr_t)my_set_mission_doesnt_require_model_guard); /* CStreaming::SetMissionDoesntRequireModel */
  hook_arm((uintptr_t)text_base + 0x1f8f9d, (uintptr_t)my_request_special_model_guard); /* CStreaming::RequestSpecialModel */
  hook_arm((uintptr_t)text_base + 0x200f95, (uintptr_t)my_is_clump_skinned_guard); /* IsClumpSkinned */
  hook_arm((uintptr_t)text_base + 0x240e59, (uintptr_t)my_plane_init_noop);          /* CPlane::InitPlanes */
  hook_arm((uintptr_t)text_base + 0x2240b1, (uintptr_t)my_automobile_process_control_noop); /* CAutomobile::ProcessControl */
  hook_arm((uintptr_t)text_base + 0x22ec55, (uintptr_t)my_bike_process_control_noop); /* CBike::ProcessControl */
  hook_arm((uintptr_t)text_base + 0x1c43f5, (uintptr_t)my_population_update_noop);   /* CPopulation::Update */
  hook_arm((uintptr_t)text_base + 0x1c1311, (uintptr_t)my_population_manage_noop);   /* CPopulation::ManagePopulation */
  hook_arm((uintptr_t)text_base + 0x13ab15, (uintptr_t)my_entity_has_prerender_effects_noop); /* CEntity::HasPreRenderEffects */
  hook_arm((uintptr_t)text_base + 0x13aa99, (uintptr_t)my_entity_get_bound_centre_safe); /* CEntity::GetBoundCentre */
  hook_arm((uintptr_t)text_base + 0x13ae4d, (uintptr_t)my_entity_get_is_on_screen_noop); /* CEntity::GetIsOnScreen */
  hook_arm((uintptr_t)text_base + 0x1278cd, (uintptr_t)my_camera_get_screen_fade_status_guard); /* CCamera::GetScreenFadeStatus */
  hook_arm((uintptr_t)text_base + 0x13adc9, (uintptr_t)my_entity_get_is_touching_vec_guard); /* CEntity::GetIsTouching(CVector,float) */
  hook_arm((uintptr_t)text_base + 0x13b229, (uintptr_t)my_entity_get_distance_base_guard); /* CEntity::GetDistanceFromCentreOfMassToBaseOfModel */
  hook_arm((uintptr_t)text_base + 0x17fff5, (uintptr_t)my_multiply3x3_matrix_vector_guard); /* Multiply3x3(CMatrix, CVector) */
  hook_arm((uintptr_t)text_base + 0x180059, (uintptr_t)my_multiply3x3_vector_matrix_guard); /* Multiply3x3(CVector, CMatrix) */
  hook_arm((uintptr_t)text_base + 0x1800bd, (uintptr_t)my_matrix_vector_mul_guard); /* operator*(CMatrix const&, CVector const&) */
  hook_arm((uintptr_t)text_base + 0x0d7281, (uintptr_t)my_collision_test_line_box_guard); /* CCollision::TestLineBox */
  hook_arm((uintptr_t)text_base + 0x17c749, (uintptr_t)my_world_test_sphere_sector_nohit); /* CWorld::TestSphereAgainstWorld */
  hook_arm((uintptr_t)text_base + 0x17c535, (uintptr_t)my_world_test_sphere_sector_nohit); /* CWorld::TestSphereAgainstSectorList */
  hook_arm((uintptr_t)text_base + 0x2742d1, (uintptr_t)my_rp_hanim_id_get_index_guard); /* RpHAnimIDGetIndex */
  hook_arm((uintptr_t)text_base + 0x274129, (uintptr_t)my_rp_hanim_hierarchy_get_matrix_array_guard); /* RpHAnimHierarchyGetMatrixArray */
  hook_arm((uintptr_t)text_base + 0x15a789, (uintptr_t)my_physical_get_bound_rect_safe); /* CPhysical::GetBoundRect */
  hook_arm((uintptr_t)text_base + 0x183885, (uintptr_t)my_objectdata_set_guard);     /* CObjectData::SetObjectData */
  install_load_object_instance_hook();                              /* CFileLoader::LoadObjectInstance */
  /* Renderer tracing hooks are useful for diagnosis, but trampolines in this
   * path can perturb the gameplay renderer. Keep only targeted guards. */
  /* The generic plugin-registry hook is intentionally disabled: this RW helper
   * has a Thumb-2/PC-relative prologue that is not safe for hook_arm here.
   * Specific RW contracts are guarded below instead. */
  hook_arm((uintptr_t)text_base + 0x283811, (uintptr_t)my_rw_frame_destroy_hierarchy_noop); /* RwFrameDestroyHierarchy */
  hook_arm((uintptr_t)text_base + 0x283875, (uintptr_t)my_rw_frame_clone_internal_abort); /* internal RwFrame clone helper */
  hook_arm((uintptr_t)text_base + 0x2b0081, (uintptr_t)my_rp_atomic_clone_passthrough); /* RpAtomicClone */
  hook_arm((uintptr_t)text_base + 0x2af49d, (uintptr_t)my_rp_atomic_destroy_noop); /* RpAtomicDestroy */
  hook_arm((uintptr_t)text_base + 0x2af621, (uintptr_t)my_rp_clump_clone_passthrough); /* RpClumpClone */
  hook_arm((uintptr_t)text_base + 0x2af2f9, (uintptr_t)my_rp_clump_render_guard); /* RpClumpRender */
  hook_arm((uintptr_t)text_base + 0x1e61f1, (uintptr_t)my_ped_model_create_hit_col_skinned_noop); /* CPedModelInfo::CreateHitColModelSkinned */
  hook_arm((uintptr_t)text_base + 0x19cfb1, (uintptr_t)my_ped_undress_noop); /* CPed::Undress */
  hook_arm((uintptr_t)text_base + 0x195119, (uintptr_t)my_ped_play_footsteps_noop); /* CPed::PlayFootSteps */
  hook_arm((uintptr_t)text_base + 0x1b1e0d, (uintptr_t)my_ped_is_head_above_pos_guard); /* CPed::IsPedHeadAbovePos */
  hook_arm((uintptr_t)text_base + 0x19f9fd, (uintptr_t)my_ped_render_noop); /* CPed::Render: player clump guard */
  hook_arm((uintptr_t)text_base + 0x20329d, (uintptr_t)my_visibility_render_fading_atomic_guard); /* CVisibilityPlugins::RenderFadingAtomic */
  hook_arm((uintptr_t)text_base + 0x186741, (uintptr_t)my_particle_object_update_all_noop); /* CParticleObject::UpdateAll */
  hook_arm((uintptr_t)text_base + 0x1d0c11, (uintptr_t)my_moving_things_update_noop); /* CMovingThings::Update */
  hook_arm((uintptr_t)text_base + 0x2866e5, (uintptr_t)my_rw_frame_sync_dirty_noop); /* _rwFrameSyncDirty */
  hook_arm((uintptr_t)text_base + 0x0b1839, (uintptr_t)my_anim_manager_add_animation_guard); /* CAnimManager::AddAnimation */
  hook_arm((uintptr_t)text_base + 0x0b18a1, (uintptr_t)my_anim_manager_add_animation_and_sync_guard); /* CAnimManager::AddAnimationAndSync */
  hook_arm((uintptr_t)text_base + 0x0ae591, (uintptr_t)my_anim_assoc_set_delete_callback_guard); /* CAnimBlendAssociation::SetDeleteCallback */
  hook_arm((uintptr_t)text_base + 0x0ae59d, (uintptr_t)my_anim_assoc_set_finish_callback_guard); /* CAnimBlendAssociation::SetFinishCallback */
  hook_arm((uintptr_t)text_base + 0x0b18f5, (uintptr_t)my_anim_manager_blend_animation_guard); /* CAnimManager::BlendAnimation */
  hook_arm((uintptr_t)text_base + 0x0b4c3d, (uintptr_t)my_animblend_clump_get_first_assoc_guard); /* RpAnimBlendClumpGetFirstAssociation(flags) */
  hook_arm((uintptr_t)text_base + 0x0b4a31, (uintptr_t)my_animblend_clump_set_blend_deltas_guard); /* RpAnimBlendClumpSetBlendDeltas */
  hook_arm((uintptr_t)text_base + 0x0b4af5, (uintptr_t)my_animblend_clump_get_assoc_null); /* RpAnimBlendClumpGetAssociation */
  hook_arm((uintptr_t)text_base + 0x0ae609, (uintptr_t)my_animblend_clumpdata_for_all_frames_guard); /* CAnimBlendClumpData::ForAllFrames */
  hook_arm((uintptr_t)text_base + 0x0b1205, (uintptr_t)my_animblend_clump_update_animations_noop); /* RpAnimBlendClumpUpdateAnimations */
  hook_arm((uintptr_t)text_base + 0x1bab55, (uintptr_t)my_playerped_set_real_move_anim_guard); /* CPlayerPed::SetRealMoveAnim */
  /* Keep CRenderer::ScanSectorPoly original: no-oping it leaves the gameplay
   * renderer alive but with zero visible entities, producing a black world. */
  install_rw_transform_guard_hooks(); /* RwV3dTransformPoints */
  hook_arm((uintptr_t)text_base + 0x1113d1, (uintptr_t)my_clear_space_for_mission_entity_noop); /* CTheScripts::ClearSpaceForMissionEntity */
  hook_arm((uintptr_t)text_base + 0x0b3691, (uintptr_t)my_cutscene_load_noop);       /* CCutsceneMgr::LoadCutsceneData */
  hook_arm((uintptr_t)text_base + 0x0b3955, (uintptr_t)my_cutscene_delete_noop);     /* CCutsceneMgr::DeleteCutsceneData */
  hook_arm((uintptr_t)text_base + 0x0b3dcd, (uintptr_t)my_cutscene_set_anim_noop);   /* CCutsceneMgr::SetCutsceneAnim */
  hook_arm((uintptr_t)text_base + 0x0b3e51, (uintptr_t)my_cutscene_set_anim_loop_noop); /* CCutsceneMgr::SetCutsceneAnimToLoop */
  hook_arm((uintptr_t)text_base + 0x0b3e71, (uintptr_t)my_cutscene_set_anim_noop);   /* CCutsceneMgr::SetHeadAnim */
  hook_arm((uintptr_t)text_base + 0x0b3ef9, (uintptr_t)my_cutscene_setup_noop);      /* CCutsceneMgr::SetupCutsceneToStart */
  hook_arm((uintptr_t)text_base + 0x0b402d, (uintptr_t)my_cutscene_finish_noop);     /* CCutsceneMgr::FinishCutscene */
  hook_arm((uintptr_t)text_base + 0x0b40ad, (uintptr_t)my_cutscene_update_noop);     /* CCutsceneMgr::Update */
  hook_arm((uintptr_t)text_base + 0x0b4371, (uintptr_t)my_cutscene_finished_yes);    /* CCutsceneMgr::HasCutsceneFinished */
  hook_arm((uintptr_t)text_base + 0x0b319d, (uintptr_t)my_cutscene_can_skip_yes);    /* CCutsceneMgr::CanSkipCutscene */
  /* CREATE_OBJECT script command derefs modelInfo[model]+0x38 even when the
   * model slot is NULL on this Android data set. Treat flags as zero there. */
  {
    uint16_t *p = (uint16_t *)((uintptr_t)text_base + 0x10a748);
    p[0] = 0x2300; /* movs r3, #0 */
    p[1] = 0xbf00; /* nop */
  }
  {
    uint16_t *p = (uint16_t *)((uintptr_t)text_base + 0x180a4a);
    p[0] = 0x2000; /* movs r0, #0 */
    p[1] = 0xbf00; /* nop */
  }
  {
    uint16_t *p = (uint16_t *)((uintptr_t)text_base + 0x180abc);
    p[0] = 0x2200; /* movs r2, #0 */
    p[1] = 0xf884; p[2] = 0x2050; /* strb.w r2, [r4, #0x50] */
    p[3] = 0x2300; /* movs r3, #0 */
    p[4] = 0xf884; p[5] = 0x3051; /* strb.w r3, [r4, #0x51] */
    p[6] = 0xbf00; p[7] = 0xbf00;
    p[8] = 0xbf00; p[9] = 0xbf00;
  }
  {
    uint16_t *p = (uint16_t *)((uintptr_t)text_base + 0x223dcc);
    p[0] = 0x2300; /* movs r3, #0: no suspension lines */
    p[1] = 0xbf00; /* nop */
  }
  {
    uint16_t *p = (uint16_t *)((uintptr_t)text_base + 0x224010);
    p[0] = 0xe00d; /* skip optional suspension alloc block when aux ptr is NULL */
  }
  {
    uint16_t *p = (uint16_t *)((uintptr_t)text_base + 0x22eb70);
    p[0] = 0xe009; /* CBike: skip suspension block when col data is missing */
  }
  {
    uint16_t *p = (uint16_t *)((uintptr_t)text_base + 0x15e5ee);
    p[0] = 0x2300; /* CPhysical::ProcessCollisionSectorList: missing col radius -> 0 */
  }
  {
    uint16_t *p = (uint16_t *)((uintptr_t)text_base + 0x26d78a);
    p[0] = 0xbf00; /* emu_FlushAltRenderTarget: skip unsafe vtable callback only. */
  }
  install_renderer_scanworld_guard_patches();                      /* CRenderer guard patches */
  debugPrintf("hooks instalados (+ node arena)\n");

  /* fixups da engine (audio/GL) — portados do gtactw_vita */
  vc_impl_init(); /* __sF -> std streams da glibc */
  patch_mpg123();
  patch_openal();
  patch_opengl();
  patch_game();

  so_flush_caches();
  so_finalize();

  debugPrintf("Rodando .init_array...\n");
  so_execute_init_array();

  /* configura package/OBB p/ o jni_shim antes do JNI_OnLoad */
  jni_set_package(GTAVC_PACKAGE, GTAVC_OBB_VER);

  /* monta JNIEnv fake + chama JNI_OnLoad + roda o NVEvent app (loop do jogo) */
  debugPrintf("jni_load: JNI_OnLoad -> NVEventAppInit...\n");
  jni_load();

  /* O jogo roda numa THREAD spawnada (init() retornou apos spawn). main() NAO
   * pode sair, senao mata o processo. Fica vivo bombeando eventos SDL. */
  debugPrintf("jni_load retornou; main aguardando (jogo roda em thread)...\n");

  /* injeta input via AND_TouchEvent(action, pointerId, x, y) — igual o Lego
   * resolve os natives por nome. action 0=down 1=up. Coords no espaço que o
   * jogo assume (1024x600). DEBUG: auto-tap no centro p/ avançar a tela título. */
  extern uintptr_t so_find_addr_safe(const char *);
  void (*AND_TouchEvent)(int, int, int, int) =
      (void *)so_find_addr_safe("_Z14AND_TouchEventiiii");
  debugPrintf("AND_TouchEvent=%p\n", (void *)AND_TouchEvent);

  extern int g_gp_debug_pulse;
  g_gp_debug_pulse = -1;

  void *menumgr = (void *)so_find_addr_safe("FrontEndMenuManager");
  int *currLevel = (int *)so_find_addr_safe("_ZN5CGame9currLevelE");
  uint8_t *menuStartupReq = (uint8_t *)so_find_addr_safe("_ZN12CMenuManager27m_bStartUpFrontEndRequestedE");
  uint8_t *menuShutdownReq = (uint8_t *)so_find_addr_safe("_ZN12CMenuManager28m_bShutDownFrontEndRequestedE");
  void (*DoSettingsStart)(void *) = (void *)((char *)text_base + 0x145f14 + 1);
  void (*ReqFrontEndShutDown)(void *) = (void *)((char *)text_base + 0x144f40 + 1);
  debugPrintf("FrontEndMenuManager=%p\n", menumgr);

  long tick = 0;
  int start_armed = 0;
  int start_done = 0;
  int forced_start_done = 0;
  apply_lowmem_runtime_prefs();
  for (;;) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) { SDL_Quit(); return 0; }
    }
    /* abre o menu uma vez (a tela de titulo precisa de 1 toque) */
    if (AND_TouchEvent) {
      if (tick == 90)      AND_TouchEvent(0, 0, 512, 300);
      else if (tick == 96) AND_TouchEvent(1, 0, 512, 300);
    }
    if (!start_armed && tick >= 360 && currLevel && *currLevel == 0) {
      if (menuStartupReq) *menuStartupReq = 1;
      if (menuShutdownReq) *menuShutdownReq = 1;
      start_armed = 1;
      debugPrintf(">>>> menu start armed: req flags set (level=%d)\n", currLevel ? *currLevel : -1);
    }
    if (start_armed && !start_done) {
      if (tick >= 380) {
        long phase = (tick - 380) % 180;
        if (phase < 18)             g_gp_debug_pulse = 7;  /* Start */
        else if (phase < 36)        g_gp_debug_pulse = -1;
        else if (phase < 54)        g_gp_debug_pulse = 0;  /* A/Cross */
        else if (phase < 72)        g_gp_debug_pulse = 11; /* DpadDown */
        else if (phase == 72 && AND_TouchEvent) AND_TouchEvent(0, 0, 512, 240);
        else if (phase == 78 && AND_TouchEvent) AND_TouchEvent(1, 0, 512, 240);
        else if (phase == 84 && AND_TouchEvent) AND_TouchEvent(0, 0, 512, 300);
        else if (phase == 90 && AND_TouchEvent) AND_TouchEvent(1, 0, 512, 300);
        else if (phase == 96 && AND_TouchEvent) AND_TouchEvent(0, 0, 512, 360);
        else if (phase == 102 && AND_TouchEvent) AND_TouchEvent(1, 0, 512, 360);
        else if (phase == 108 && AND_TouchEvent) AND_TouchEvent(0, 0, 512, 420);
        else if (phase == 114 && AND_TouchEvent) AND_TouchEvent(1, 0, 512, 420);
        else if (phase < 126)       g_gp_debug_pulse = 7;  /* Start de novo */
        else if (phase < 144)       g_gp_debug_pulse = -1;
        else if (phase < 162)       g_gp_debug_pulse = 0;  /* confirma */
        else if (phase < 174)       g_gp_debug_pulse = -1;
        else if (phase == 174 && AND_TouchEvent) AND_TouchEvent(0, 0, 512, 540);
        else if (phase == 178 && AND_TouchEvent) AND_TouchEvent(1, 0, 512, 540);
      }
      if (currLevel && *currLevel >= 1) {
        start_done = 1;
        debugPrintf(">>>> jogo entrou no level=%d\n", *currLevel);
      }
    }
    if (start_armed && !forced_start_done && tick >= 720 && menumgr &&
        currLevel && *currLevel == 0) {
      forced_start_done = 1;
      debugPrintf(">>>> FORCANDO START TARDE: DoSettings + RequestFrontEndShutDown\n");
      DoSettingsStart(menumgr);
      ReqFrontEndShutDown(menumgr);
      debugPrintf(">>>> start tardio retornou\n");
    }
    if (tick == 30 || tick == 120 || tick == 180 || tick == 300 || tick == 360 ||
        (tick > 360 && (tick % 300) == 0))
      apply_lowmem_runtime_prefs();
    g_gp_debug_pulse = -1;
    tick++;
    SDL_Delay(16);
  }
}
