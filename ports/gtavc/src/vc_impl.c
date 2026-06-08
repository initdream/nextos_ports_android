/*
 * vc_impl.c — implementações dos 20 símbolos UNKNOWN do GTA Vice City (armhf).
 *
 * Categorias:
 *  - Immersion haptics (libImmEmulatorJ): STUB no-op (TV box não vibra)
 *  - libm/libc que o classificador perdeu: passthrough (entram na tabela como &func)
 *  - setjmp/longjmp: ABI bionic ~ glibc _setjmp/_longjmp (sem máscara de sinal)
 *  - __sF: array stdio estilo bionic (stdin/stdout/stderr) -> std streams da glibc
 *  - _ctype_: tabela ctype estilo BSD/bionic
 *
 * As entradas na dynlib_functions ficam em imports.gen.c (editado p/ apontar aqui).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#include <fcntl.h>

#define SOFTFP_ABI __attribute__((pcs("aapcs")))

/* raise/abort stub — loga QUEM chamou (offset no .so) p/ achar o erro fatal */
extern void *text_base;
extern size_t text_size;
extern volatile unsigned g_gl_draw_arrays_frame;
extern volatile unsigned g_gl_draw_elements_frame;
extern volatile int g_current_framebuffer;
extern volatile unsigned g_current_fbo_color_tex;
extern volatile int g_swap_frame;
extern void vc_update_game_backbuffer_copy_from_viewport(const int viewport[4]);
extern uintptr_t so_find_addr_safe(const char *);
extern void hook_arm(uintptr_t addr, uintptr_t dst);
extern void so_make_text_writable(void);
extern void so_make_text_executable(void);
extern void so_flush_caches(void);
static volatile int g_game_world_draw_seen = 0;
static void patch_thumb8(uintptr_t addr, const uint16_t hw[4]);
static int rw_ptr_valid(const void *p) {
  uintptr_t v = (uintptr_t)p;
  return v >= 0x10000u && v < 0xff000000u;
}

static void *make_thumb_trampoline8(uintptr_t target, const uint16_t orig[4]) {
  uint16_t *tramp = mmap(NULL, 16, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (tramp == MAP_FAILED)
    return NULL;
  tramp[0] = orig[0];
  tramp[1] = orig[1];
  tramp[2] = orig[2];
  tramp[3] = orig[3];
  tramp[4] = 0xf8df; /* LDR.W PC, [PC, #0] */
  tramp[5] = 0xf000;
  *(uint32_t *)(tramp + 6) = (uint32_t)(target + 8 + 1);
  __builtin___clear_cache((char *)tramp, (char *)tramp + 16);
  return (void *)((uintptr_t)tramp + 1);
}
static void log_caller(const char *what, int arg, void *ra) {
  unsigned long tb = (unsigned long)text_base;
  unsigned long r = (unsigned long)ra;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    if (r >= tb && r < tb + text_size)
      fprintf(f, ">>> game %s(%d) de libGTAVC.so+0x%lx\n", what, arg, r - tb);
    else
      fprintf(f, ">>> game %s(%d) de %p (fora do .so)\n", what, arg, ra);
    fclose(f);
  }
}

int raise_stub(int sig) {
  log_caller("raise", sig, __builtin_return_address(0));
  return 0; /* nao crasha — deixa o jogo continuar p/ ver o proximo passo */
}
void abort_stub(void) {
  log_caller("abort", 0, __builtin_return_address(0));
  _exit(42);
}

/* hook de OS_DebugOut: loga a string + QUEM chamou (offset no .so) —
 * p/ achar o site exato do terminate inline (quem loga "HALT"). */
extern void *text_base;
extern size_t text_size;
/* NvAPKOpen: o jogo tenta ler do OBB (zip) e a leitura falha. Forçando NULL,
 * o NvFOpen cai no fopen dos arquivos SOLTOS (OBB extraído) que funcionam. */
void *my_nvapkopen(const char *p) { (void)p; return (void *)0; }

/* al_print (OpenAL interno do jogo): faz fputs/fflush no gLogFile que está NULL
 * (openal-soft nunca setou) -> crash. É só logging de áudio. No-op. */
void my_al_print(void) { /* nada */ }

void my_vehicle_preprocess_noop(void *thiz) {
  static int logged = 0;
  (void)thiz;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[vehicle] PreprocessHierarchy no-op\n"); fclose(f); }
  }
}

void my_vehicle_setenv_noop(void *thiz) {
  static int logged = 0;
  (void)thiz;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[vehicle] SetEnvironmentMap no-op\n"); fclose(f); }
  }
}

void my_vehicle_load_colours_noop(void) {
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) { fprintf(f, "[vehicle] LoadVehicleColours no-op\n"); fclose(f); }
}

void my_vehicle_choose_colour_default(void *thiz, unsigned char *c1, unsigned char *c2) {
  (void)thiz;
  if (c1) *c1 = 0;
  if (c2) *c2 = 0;
}

void my_vehicle_set_colour_noop(void *thiz, unsigned char c1, unsigned char c2) {
  (void)thiz;
  (void)c1;
  (void)c2;
}

void my_generate_one_random_car_noop(void) {
  static int logged = 0;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[traffic] GenerateOneRandomCar no-op\n"); fclose(f); }
  }
}

void my_generate_random_cars_noop(void) {
  static int logged = 0;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[traffic] GenerateRandomCars no-op\n"); fclose(f); }
  }
}

void my_phoneinfo_update_noop(void) {
  static int logged = 0;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[phone] CPhoneInfo::Update no-op\n"); fclose(f); }
  }
}

void my_playerped_process_control_noop(void *thiz) {
  static int logged = 0;
  static int calls = 0;
  static void (*orig_process)(void *) = NULL;
  if (!orig_process) {
    static const uint16_t orig[4] = {0xe92d, 0x43f0, 0xed2d, 0x8b02};
    orig_process = (void (*)(void *))make_thumb_trampoline8((uintptr_t)text_base + 0x1bf5b0, orig);
  }
  if (orig_process) {
    calls++;
    orig_process(thiz);
    return;
  }
  if (logged++ < 8) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[player] CPlayerPed::ProcessControl guarded-noop seenWorld=%d calls=%d ped=%p orig=%p\n",
              g_game_world_draw_seen, calls, thiz, orig_process);
      fclose(f);
    }
  }
}

void my_request_special_model_noop(int model_id, const char *name, int flags) {
  static int logged = 0;
  (void)flags;
  if (logged < 12) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[streaming] RequestSpecialModel no-op model=%d name=%p", model_id, name);
      if ((uintptr_t)name > 0x10000) fprintf(f, " '%s'", name);
      fprintf(f, "\n");
      fclose(f);
    }
    logged++;
  }
}

static void patch_thumb8(uintptr_t addr, const uint16_t hw[4]) {
  uint16_t *p = (uint16_t *)(addr & ~1u);
  p[0] = hw[0];
  p[1] = hw[1];
  p[2] = hw[2];
  p[3] = hw[3];
}

static void log_guard_skip(const char *tag, int model_id, const char *name) {
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (!f) return;
  fprintf(f, "[guard] %s skip model=%d name=%p", tag, model_id, name);
  if ((uintptr_t)name > 0x10000) fprintf(f, " '%s'", name);
  fprintf(f, "\n");
  fclose(f);
}

void my_request_special_model_guard(int model_id, const char *name, int flags) {
  static void **model_info = NULL;
  static unsigned logged = 0;

  if (!model_info)
    model_info = (void **)so_find_addr_safe("_ZN10CModelInfo16ms_modelInfoPtrsE");

  if ((uintptr_t)name < 0x10000 || model_id < 0 || model_id >= 6500 ||
      !model_info || !model_info[model_id]) {
    if (logged++ < 24)
      log_guard_skip("RequestSpecialModel", model_id, name);
    return;
  }

  static const uint16_t orig[4] = {0xe92d, 0x4ff0, 0xb08f, 0xf8df};
  const uintptr_t target = (uintptr_t)text_base + 0x1f8f9c;
  typedef void (*request_fn)(int, const char *, int);

  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  ((request_fn)(target + 1))(model_id, name, flags);
  hook_arm(target + 1, (uintptr_t)my_request_special_model_guard);
  so_flush_caches();
  so_make_text_executable();
}

void my_set_mission_doesnt_require_model_guard(int model_id) {
  static void **model_info = NULL;
  static unsigned logged = 0;

  if (!model_info)
    model_info = (void **)so_find_addr_safe("_ZN10CModelInfo16ms_modelInfoPtrsE");

  if (model_id < 0 || model_id >= 6500 || !model_info || !model_info[model_id]) {
    if (logged++ < 24)
      log_guard_skip("SetMissionDoesntRequireModel", model_id, NULL);
    return;
  }

  static const uint16_t orig[4] = {0xb470, 0x0084, 0x4b17, 0x1822};
  const uintptr_t target = (uintptr_t)text_base + 0x1f8f2c;
  typedef void (*set_mission_fn)(int);

  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  ((set_mission_fn)(target + 1))(model_id);
  hook_arm(target + 1, (uintptr_t)my_set_mission_doesnt_require_model_guard);
  so_flush_caches();
  so_make_text_executable();
}

int my_is_clump_skinned_guard(void *clump) {
  static unsigned logged = 0;
  static void *(*rp_skin_geometry_get_skin)(void *) = NULL;
  int result = 0;
  void *atomic = NULL;
  void *geometry = NULL;
  void *skin = NULL;

  if (!rp_skin_geometry_get_skin)
    rp_skin_geometry_get_skin = (void *(*)(void *))((uintptr_t)text_base + 0x279a04 + 1);

  if ((uintptr_t)clump >= 0x10000 && *(uint8_t *)clump == 2) {
    void **list = (void **)((uint8_t *)clump + 0x08);
    void *node = *list;
    if ((uintptr_t)node >= 0x10000 && node != (void *)list) {
      atomic = (uint8_t *)node - 0x40;
      geometry = *(void **)((uint8_t *)atomic + 0x18);
      if ((uintptr_t)geometry >= 0x10000 && rp_skin_geometry_get_skin) {
        skin = rp_skin_geometry_get_skin(geometry);
        result = ((uintptr_t)skin >= 0x10000) ? 1 : 0;
      }
    }
  }

  if (logged++ < 24) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[guard] IsClumpSkinned(%p) atomic=%p geom=%p skin=%p -> %d\n",
              clump, atomic, geometry, skin, result);
      fclose(f);
    }
  }
  return result;
}

void *my_frame_find_name_safe(void *frame, void *data) {
  (void)data;
  return frame;
}

void my_plane_init_noop(void) {
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) { fprintf(f, "[plane] CPlane::InitPlanes no-op\n"); fclose(f); }
}

void my_bike_process_control_noop(void *bike) {
  static unsigned logged = 0;
  (void)bike;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[bike] ProcessControl no-op\n"); fclose(f); }
  }
}

void my_automobile_process_control_noop(void *car) {
  static unsigned logged = 0;
  (void)car;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[automobile] ProcessControl no-op\n"); fclose(f); }
  }
}

void my_population_update_noop(int generate) {
  static unsigned logged = 0;
  (void)generate;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[population] Update no-op\n"); fclose(f); }
  }
}

void my_population_manage_noop(void) {
  static unsigned logged = 0;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[population] ManagePopulation no-op\n"); fclose(f); }
  }
}

int my_entity_has_prerender_effects_noop(void *thiz) {
  (void)thiz;
  return 0;
}

void *my_entity_get_bound_centre_safe(void *out, void *entity) {
  float *dst = (float *)out;
  const unsigned char *e = (const unsigned char *)entity;
  if (dst && e) {
    dst[0] = *(const float *)(e + 0x34);
    dst[1] = *(const float *)(e + 0x38);
    dst[2] = *(const float *)(e + 0x3c);
  }
  return out;
}

void *my_physical_get_bound_rect_safe(void *out, void *entity) {
  float *rect = (float *)out;
  const unsigned char *e = (const unsigned char *)entity;
  if (rect && e) {
    const float x = *(const float *)(e + 0x34);
    const float y = *(const float *)(e + 0x38);
    const float r = 1.0f;
    rect[0] = x - r;
    rect[1] = y + r;
    rect[2] = x + r;
    rect[3] = y - r;
  }
  return out;
}

uint32_t my_entity_get_distance_base_guard(void *entity) {
  static void **model_info = NULL;
  static unsigned logged = 0;

  if (!model_info)
    model_info = (void **)so_find_addr_safe("_ZN10CModelInfo16ms_modelInfoPtrsE");

  if (!entity || !model_info) {
    if (logged++ < 16)
      log_guard_skip("GetDistanceBase", -1, NULL);
    return 0;
  }

  int16_t model_id = *(int16_t *)((uint8_t *)entity + 0x60);
  void *mi = (model_id >= 0 && model_id < 6500) ? model_info[model_id] : NULL;
  void *col = mi ? *(void **)((uint8_t *)mi + 0x3c) : NULL;
  if (!col) {
    if (logged++ < 32)
      log_guard_skip("GetDistanceBase", model_id, NULL);
    return 0;
  }

  static const uint16_t orig[4] = {0x4b05, 0xf9b0, 0x2060, 0x447b};
  const uintptr_t target = (uintptr_t)text_base + 0x13b228;
  typedef uint32_t (*dist_fn)(void *);

  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  uint32_t ret = ((dist_fn)(target + 1))(entity);
  hook_arm(target + 1, (uintptr_t)my_entity_get_distance_base_guard);
  so_flush_caches();
  so_make_text_executable();
  return ret;
}

void *my_matrix_vector_mul_guard(void *out, void *matrix, void *vec) {
  static unsigned logged = 0;
  if (!out) return out;
  if ((uintptr_t)matrix < 0x10000 || (uintptr_t)vec < 0x10000) {
    float *dst = (float *)out;
    dst[0] = 0.0f;
    dst[1] = 0.0f;
    dst[2] = 0.0f;
    if (logged++ < 24) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[guard] MatrixVectorMul out=%p matrix=%p vec=%p -> zero\n",
                out, matrix, vec);
        fclose(f);
      }
    }
    return out;
  }

  static const uint16_t orig[4] = {0xed92, 0x5a01, 0xedd1, 0x6a04};
  const uintptr_t target = (uintptr_t)text_base + 0x1800bc;
  typedef void *(*mul_fn)(void *, void *, void *);

  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  void *ret = ((mul_fn)(target + 1))(out, matrix, vec);
  hook_arm(target + 1, (uintptr_t)my_matrix_vector_mul_guard);
  so_flush_caches();
  so_make_text_executable();
  return ret;
}

static void *zero_vec_guard(const char *tag, void *out, void *a, void *b,
                            unsigned *logged) {
  if (!out) return out;
  if ((uintptr_t)a < 0x10000 || (uintptr_t)b < 0x10000) {
    float *dst = (float *)out;
    dst[0] = 0.0f;
    dst[1] = 0.0f;
    dst[2] = 0.0f;
    if ((*logged)++ < 24) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[guard] %s out=%p a=%p b=%p -> zero\n", tag, out, a, b);
        fclose(f);
      }
    }
    return out;
  }
  return NULL;
}

void *my_multiply3x3_matrix_vector_guard(void *out, void *matrix, void *vec) {
  static unsigned logged = 0;
  if (!out) return out;
  void *guarded = zero_vec_guard("Multiply3x3MV", out, matrix, vec, &logged);
  if (guarded) return guarded;

  static const uint16_t orig[4] = {0xedd2, 0x7a01, 0xedd1, 0x6a04};
  const uintptr_t target = (uintptr_t)text_base + 0x17fff4;
  typedef void *(*mul_fn)(void *, void *, void *);

  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  void *ret = ((mul_fn)(target + 1))(out, matrix, vec);
  hook_arm(target + 1, (uintptr_t)my_multiply3x3_matrix_vector_guard);
  so_flush_caches();
  so_make_text_executable();
  return ret;
}

void *my_multiply3x3_vector_matrix_guard(void *out, void *vec, void *matrix) {
  static unsigned logged = 0;
  if (!out) return out;
  void *guarded = zero_vec_guard("Multiply3x3VM", out, vec, matrix, &logged);
  if (guarded) return guarded;

  static const uint16_t orig[4] = {0xedd1, 0x7a01, 0xedd2, 0x6a01};
  const uintptr_t target = (uintptr_t)text_base + 0x180058;
  typedef void *(*mul_fn)(void *, void *, void *);

  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  void *ret = ((mul_fn)(target + 1))(out, vec, matrix);
  hook_arm(target + 1, (uintptr_t)my_multiply3x3_vector_matrix_guard);
  so_flush_caches();
  so_make_text_executable();
  return ret;
}

int my_collision_test_line_box_guard(void *line, void *box) {
  static unsigned logged = 0;
  if ((uintptr_t)line < 0x10000 || (uintptr_t)box < 0x10000) {
    if (logged++ < 24) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[guard] TestLineBox line=%p box=%p -> no hit\n", line, box);
        fclose(f);
      }
    }
    return 0;
  }

  static const uint16_t orig[4] = {0xedd0, 0x7a00, 0xed91, 0x7a00};
  const uintptr_t target = (uintptr_t)text_base + 0x0d7280;
  typedef int (*test_fn)(void *, void *);

  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  int ret = ((test_fn)(target + 1))(line, box);
  hook_arm(target + 1, (uintptr_t)my_collision_test_line_box_guard);
  so_flush_caches();
  so_make_text_executable();
  return ret;
}

int my_world_test_sphere_sector_nohit(void) {
  static unsigned logged = 0;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[guard] TestSphereAgainstWorld/SectorList forced-nohit\n"); fclose(f); }
  }
  return 0;
}

void my_emu_flush_alt_render_target_noop(void) {
  static unsigned logged = 0;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[gl-guard] emu_FlushAltRenderTarget no-op\n"); fclose(f); }
  }
}

int my_rp_hanim_id_get_index_guard(void *hierarchy, int id) {
  static unsigned logged = 0;
  if ((uintptr_t)hierarchy < 0x10000) {
    if (logged++ < 32) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[rw-guard] RpHAnimIDGetIndex hierarchy=%p id=%d -> -1\n",
                hierarchy, id);
        fclose(f);
      }
    }
    return -1;
  }

  static const uint16_t orig[4] = {0xb410, 0x6844, 0x6903, 0x2c00};
  const uintptr_t target = (uintptr_t)text_base + 0x2742d0;
  typedef int (*id_get_index_fn)(void *, int);

  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  int ret = ((id_get_index_fn)(target + 1))(hierarchy, id);
  hook_arm(target + 1, (uintptr_t)my_rp_hanim_id_get_index_guard);
  so_flush_caches();
  so_make_text_executable();
  return ret;
}

void *my_rp_hanim_hierarchy_get_matrix_array_guard(void *hierarchy) {
  static unsigned logged = 0;
  if ((uintptr_t)hierarchy < 0x10000) {
    if (logged++ < 32) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[rw-guard] RpHAnimHierarchyGetMatrixArray hierarchy=%p -> NULL\n",
                hierarchy);
        fclose(f);
      }
    }
    return NULL;
  }
  return *(void **)((uint8_t *)hierarchy + 8);
}

int my_entity_get_is_touching_vec_guard(void *entity, void *vec, uint32_t radius_bits) {
  static void **model_info = NULL;
  static unsigned logged = 0;

  if (!model_info)
    model_info = (void **)so_find_addr_safe("_ZN10CModelInfo16ms_modelInfoPtrsE");

  int16_t model_id = entity ? *(int16_t *)((uint8_t *)entity + 0x60) : -1;
  void *mi = (entity && model_info && model_id >= 0 && model_id < 6500) ? model_info[model_id] : NULL;
  void *col = mi ? *(void **)((uint8_t *)mi + 0x3c) : NULL;
  if ((uintptr_t)entity < 0x10000 || (uintptr_t)vec < 0x10000) {
    if (logged++ < 32)
      log_guard_skip("GetIsTouchingVec", model_id, NULL);
    return 0;
  }

  float radius = 1.0f;
  if ((uintptr_t)col >= 0x10000) {
    float col_radius = *(float *)((uint8_t *)col + 0x0c);
    if (isfinite(col_radius) && col_radius > 0.0f && col_radius < 100.0f)
      radius = col_radius;
  }

  float in_radius = 0.0f;
  memcpy(&in_radius, &radius_bits, sizeof(in_radius));
  if (!isfinite(in_radius) || in_radius < 0.0f || in_radius > 100.0f)
    in_radius = 0.0f;

  float center[3];
  my_entity_get_bound_centre_safe(center, entity);
  float *v = (float *)vec;
  const float dx = center[0] - v[0];
  const float dy = center[1] - v[1];
  const float dz = center[2] - v[2];
  const float rr = radius + in_radius;
  const float dist2 = dx * dx + dy * dy + dz * dz;
  const int touching = (isfinite(dist2) && rr > 0.0f && dist2 < rr * rr) ? 1 : 0;

  if (logged++ < 64) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f,
              "[guard] GetIsTouchingVec safe ent=%p model=%d col=%p r=%.3f in=%.3f dist2=%.3f ret=%d\n",
              entity, model_id, col, radius, in_radius, dist2, touching);
      fclose(f);
    }
  }

  return touching;
}

int my_entity_get_is_on_screen_noop(void *entity) {
  static unsigned logged = 0;
  (void)entity;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[guard] GetIsOnScreen no-op true\n"); fclose(f); }
  }
  return 1;
}

int my_camera_get_screen_fade_status_guard(void *camera) {
  static int *curr_level = NULL;
  static unsigned logged = 0;

  if (!curr_level)
    curr_level = (int *)so_find_addr_safe("_ZN5CGame9currLevelE");

  if ((uintptr_t)camera < 0x10000)
    return 0;

  float *fade = (float *)((uint8_t *)camera + 0x904);
  if (curr_level && *curr_level >= 1) {
    if (*fade != 0.0f && (logged++ < 24 || (logged % 300) == 0)) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[camera-fade] force gameplay fade %.2f -> 0\n", *fade);
        fclose(f);
      }
    }
    *fade = 0.0f;
    return 0;
  }

  if (*fade == 0.0f)
    return 0;
  return (*fade == 255.0f) ? 2 : 1;
}

void my_objectdata_set_guard(int model_id, void *object) {
  static void **model_info = NULL;
  static uint8_t *object_info = NULL;
  static unsigned logged_ok = 0;
  static unsigned logged_skip = 0;

  if (!model_info)
    model_info = (void **)so_find_addr_safe("_ZN10CModelInfo16ms_modelInfoPtrsE");
  if (!object_info)
    object_info = (uint8_t *)so_find_addr_safe("_ZN11CObjectData14ms_aObjectInfoE");

  if (model_id < 0 || model_id >= 6500 || (uintptr_t)object < 0x10000 ||
      !model_info || !model_info[model_id] || !object_info) {
    if (logged_skip++ < 24) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[objectdata] SetObjectData skip model=%d object=%p mi=%p info=%p\n",
                model_id, object,
                (model_info && model_id >= 0 && model_id < 6500) ? model_info[model_id] : NULL,
                object_info);
        fclose(f);
      }
    }
    return;
  }

  void *mi = model_info[model_id];
  int16_t info_index = *(int16_t *)((uint8_t *)mi + 0x42);
  if (info_index < 0 || info_index >= 210) {
    if (logged_skip++ < 24) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[objectdata] SetObjectData no info model=%d object=%p mi=%p idx=%d\n",
                model_id, object, mi, info_index);
        fclose(f);
      }
    }
    return;
  }

  uint8_t *src = object_info + ((uintptr_t)info_index << 5);
  uint8_t *dst = (uint8_t *)object;
  *(uint32_t *)(dst + 0x0bc) = *(uint32_t *)(src + 0x00);
  *(uint32_t *)(dst + 0x0c0) = *(uint32_t *)(src + 0x04);
  *(uint32_t *)(dst + 0x0c8) = *(uint32_t *)(src + 0x08);
  *(uint32_t *)(dst + 0x0cc) = *(uint32_t *)(src + 0x0c);
  *(uint32_t *)(dst + 0x0d0) = *(uint32_t *)(src + 0x10);
  *(uint32_t *)(dst + 0x16c) = *(uint32_t *)(src + 0x14);
  *(uint32_t *)(dst + 0x178) = *(uint32_t *)(src + 0x18);
  *(uint8_t *)(dst + 0x17c) = *(uint8_t *)(src + 0x1c);
  *(uint8_t *)(dst + 0x17d) = *(uint8_t *)(src + 0x1d);
  *(uint8_t *)(dst + 0x17e) = *(uint8_t *)(src + 0x1e);

  float mass = *(float *)(src + 0x00);
  if (isfinite(mass) && mass >= 100000.0f) {
    uint8_t flags11e = *(uint8_t *)(dst + 0x11e);
    uint8_t flags56 = *(uint8_t *)(dst + 0x56);
    flags11e |= 0x0c;
    flags11e &= (uint8_t)~0x02;
    flags56 |= 0x02;
    *(uint8_t *)(dst + 0x11e) = flags11e;
    *(uint8_t *)(dst + 0x56) = flags56;
  }

  if (logged_ok++ < 32 || (logged_ok % 1000) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[objectdata] SetObjectData applied model=%d object=%p mi=%p idx=%d mass=%.2f\n",
              model_id, object, mi, info_index, mass);
      fclose(f);
    }
  }
}

void my_object_init_noop(void *thiz) {
  (void)thiz;
}

void my_clear_space_for_mission_entity_noop(void *pos, void *entity) {
  static int logged = 0;
  (void)pos;
  (void)entity;
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[script] ClearSpaceForMissionEntity no-op\n"); fclose(f); }
  }
}

static void force_cutscene_flags_off(void) {
  static uint8_t *playingIntro = NULL;
  static int *skipAllMPEGs = NULL;
  static uint8_t *cutsceneRunning = NULL;
  static uint8_t *cutsceneProcessing = NULL;
  static uint8_t *cutsceneLoaded = NULL;
  static uint8_t *cutsceneAnimLoaded = NULL;
  static uint8_t *cutsceneSkipped = NULL;
  static uint8_t *processCutsceneOnly = NULL;
  static int *cutsceneLoadStatus = NULL;

  if (!playingIntro) playingIntro = (uint8_t *)so_find_addr_safe("_ZN5CGame12playingIntroE");
  if (!skipAllMPEGs) skipAllMPEGs = (int *)so_find_addr_safe("SkipAllMPEGs");
  if (!cutsceneRunning) cutsceneRunning = (uint8_t *)so_find_addr_safe("_ZN12CCutsceneMgr10ms_runningE");
  if (!cutsceneProcessing) cutsceneProcessing = (uint8_t *)so_find_addr_safe("_ZN12CCutsceneMgr21ms_cutsceneProcessingE");
  if (!cutsceneLoaded) cutsceneLoaded = (uint8_t *)so_find_addr_safe("_ZN12CCutsceneMgr9ms_loadedE");
  if (!cutsceneAnimLoaded) cutsceneAnimLoaded = (uint8_t *)so_find_addr_safe("_ZN12CCutsceneMgr13ms_animLoadedE");
  if (!cutsceneSkipped) cutsceneSkipped = (uint8_t *)so_find_addr_safe("_ZN12CCutsceneMgr21ms_wasCutsceneSkippedE");
  if (!processCutsceneOnly) processCutsceneOnly = (uint8_t *)so_find_addr_safe("_ZN6CWorld20bProcessCutsceneOnlyE");
  if (!cutsceneLoadStatus) cutsceneLoadStatus = (int *)so_find_addr_safe("_ZN12CCutsceneMgr21ms_cutsceneLoadStatusE");

  if (playingIntro) *playingIntro = 0;
  if (skipAllMPEGs) *skipAllMPEGs = 1;
  if (cutsceneRunning) *cutsceneRunning = 0;
  if (cutsceneProcessing) *cutsceneProcessing = 0;
  if (cutsceneLoaded) *cutsceneLoaded = 0;
  if (cutsceneAnimLoaded) *cutsceneAnimLoaded = 0;
  if (cutsceneSkipped) *cutsceneSkipped = 1;
  if (processCutsceneOnly) *processCutsceneOnly = 0;
  if (cutsceneLoadStatus) *cutsceneLoadStatus = 0;
}

void my_cutscene_load_noop(const char *name) {
  static int logged = 0;
  force_cutscene_flags_off();
  if (logged < 6) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[cutscene] LoadCutsceneData skip name=%s\n", name ? name : "(null)"); fclose(f); }
    logged++;
  }
}

void my_cutscene_setup_noop(void) {
  static int logged = 0;
  force_cutscene_flags_off();
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[cutscene] SetupCutsceneToStart skip\n"); fclose(f); }
  }
}

void my_cutscene_delete_noop(void) {
  static int logged = 0;
  force_cutscene_flags_off();
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[cutscene] DeleteCutsceneData skip\n"); fclose(f); }
  }
}

void my_cutscene_finish_noop(void) {
  static int logged = 0;
  force_cutscene_flags_off();
  if (!logged++) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[cutscene] FinishCutscene skip\n"); fclose(f); }
  }
}

void my_cutscene_set_anim_noop(const char *name, void *obj) {
  static int logged = 0;
  force_cutscene_flags_off();
  if (logged++ < 12) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[cutscene] SetCutsceneAnim skip name=%p obj=%p", name, obj);
      if ((uintptr_t)name > 0x10000) fprintf(f, " '%s'", name);
      fprintf(f, "\n");
      fclose(f);
    }
  }
}

void my_cutscene_set_anim_loop_noop(const char *name) {
  static int logged = 0;
  force_cutscene_flags_off();
  if (logged++ < 12) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[cutscene] SetCutsceneAnimToLoop skip name=%p", name);
      if ((uintptr_t)name > 0x10000) fprintf(f, " '%s'", name);
      fprintf(f, "\n");
      fclose(f);
    }
  }
}

void my_cutscene_update_noop(void) {
  force_cutscene_flags_off();
}

int my_cutscene_finished_yes(void) {
  force_cutscene_flags_off();
  return 1;
}

int my_cutscene_can_skip_yes(void) {
  force_cutscene_flags_off();
  return 1;
}

static uintptr_t g_anim_clump_cache_key[16];
static int g_anim_clump_cache_num[16];
static unsigned g_anim_clump_cache_pos = 0;

static void vc_anim_cache_num_frames(void *clump, int num_frames) {
  if ((uintptr_t)clump < 0x10000 || num_frames < 2 || num_frames > 128)
    return;
  for (unsigned i = 0; i < 16; i++) {
    if (g_anim_clump_cache_key[i] == (uintptr_t)clump) {
      g_anim_clump_cache_num[i] = num_frames;
      return;
    }
  }
  unsigned slot = g_anim_clump_cache_pos++ % 16;
  g_anim_clump_cache_key[slot] = (uintptr_t)clump;
  g_anim_clump_cache_num[slot] = num_frames;
}

static int vc_anim_cached_num_frames(void *clump) {
  if ((uintptr_t)clump < 0x10000)
    return 0;
  for (unsigned i = 0; i < 16; i++) {
    if (g_anim_clump_cache_key[i] == (uintptr_t)clump)
      return g_anim_clump_cache_num[i];
  }
  return 0;
}

static int vc_anim_try_init_clump(void *clump, int (*is_initialized)(void *), const char *where) {
  static void (*clump_init)(void *) = NULL;
  static int (*get_clump_plugin_offset)(uint32_t) = NULL;
  static uintptr_t failed[16];
  static unsigned failed_pos = 0;
  static unsigned logged_ok = 0;
  static unsigned logged_fail = 0;

  if ((uintptr_t)clump < 0x10000 || !is_initialized)
    return 0;
  if (is_initialized(clump))
    return 1;
  if (*(uint8_t *)clump != 2)
    return 0;

  for (unsigned i = 0; i < sizeof(failed) / sizeof(failed[0]); i++) {
    if (failed[i] == (uintptr_t)clump)
      return 0;
  }

  if (!clump_init)
    clump_init = (void (*)(void *))((uintptr_t)text_base + 0x0b494c + 1);
  if (!get_clump_plugin_offset)
    get_clump_plugin_offset = (int (*)(uint32_t))((uintptr_t)text_base + 0x2aff28 + 1);
  if (!clump_init)
    return 0;

  clump_init(clump);
  if (!is_initialized(clump) && get_clump_plugin_offset) {
    int off = get_clump_plugin_offset(0x0253f2fdU);
    if (off >= 0 && off < 4096) {
      void *data = *(void **)((uint8_t *)clump + off);
      if ((uintptr_t)data >= 0x10000) {
        int *num_bones = (int *)((uint8_t *)data + 0x08);
        void **frames = (void **)((uint8_t *)data + 0x10);
        if (*num_bones == 0 && (uintptr_t)*frames >= 0x10000) {
          int cached_num = vc_anim_cached_num_frames(clump);
          if (cached_num >= 2 && cached_num <= 128)
            *num_bones = cached_num;
          if (logged_ok++ < 32) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) {
              fprintf(f, "[anim-guard] %s restore cached anim frames clump=%p data=%p frames=%p cached=%d applied=%d\n",
                      where, clump, data, *frames, cached_num, *num_bones);
              fclose(f);
            }
          }
        }
      }
    }
  }
  if (is_initialized(clump)) {
    if (logged_ok++ < 32) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[anim-guard] %s initialized clump=%p\n", where, clump);
        fclose(f);
      }
    }
    return 1;
  }

  failed[failed_pos++ % (sizeof(failed) / sizeof(failed[0]))] = (uintptr_t)clump;
  if (logged_fail++ < 32) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[anim-guard] %s init failed clump=%p\n", where, clump);
      fclose(f);
    }
  }
  return 0;
}

void *my_anim_manager_blend_animation_guard(void *clump, int group, int anim, uint32_t blend_bits) {
  static void *(*orig_blend)(void *, int, int, uint32_t) = NULL;
  static int (*is_initialized)(void *) = NULL;
  static unsigned logged_skip = 0;

  if (!orig_blend) {
    static const uint16_t orig[4] = {0xe92d, 0x47f0, 0xed2d, 0x8b02};
    orig_blend = (void *(*)(void *, int, int, uint32_t))
        make_thumb_trampoline8((uintptr_t)text_base + 0x0b18f4, orig);
  }
  if (!is_initialized)
    is_initialized = (int (*)(void *))((uintptr_t)text_base + 0x0b49d0 + 1);

  if ((uintptr_t)clump < 0x10000 || !is_initialized ||
      !vc_anim_try_init_clump(clump, is_initialized, "BlendAnimation")) {
    if (logged_skip++ < 64) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[anim-guard] BlendAnimation skip clump=%p group=%d anim=%d initialized=0\n",
                clump, group, anim);
        fclose(f);
      }
    }
    return NULL;
  }
  if (!orig_blend)
    return NULL;
  return orig_blend(clump, group, anim, blend_bits);
}

void *my_animblend_clump_get_first_assoc_guard(void *clump, unsigned int flags) {
  static int (*is_initialized)(void *) = NULL;
  static unsigned logged_skip = 0;

  if (!is_initialized)
    is_initialized = (int (*)(void *))((uintptr_t)text_base + 0x0b49d0 + 1);

  if ((uintptr_t)clump < 0x10000 || !is_initialized ||
      !vc_anim_try_init_clump(clump, is_initialized, "GetFirstAssociation")) {
    if (logged_skip++ < 64) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[anim-guard] GetFirstAssociation skip clump=%p flags=0x%x initialized=0\n",
                clump, flags);
        fclose(f);
      }
    }
    return NULL;
  }

  static const uint16_t orig[4] = {0x4b07, 0x447b, 0x681b, 0x681b};
  const uintptr_t target = (uintptr_t)text_base + 0x0b4c3c;
  typedef void *(*first_assoc_fn)(void *, unsigned int);

  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  void *result = ((first_assoc_fn)(target + 1))(clump, flags);
  hook_arm(target + 1, (uintptr_t)my_animblend_clump_get_first_assoc_guard);
  so_flush_caches();
  so_make_text_executable();
  return result;
}

void my_animblend_clump_set_blend_deltas_guard(void *clump, unsigned int flags, uint32_t delta_bits) {
  static int (*is_initialized)(void *) = NULL;
  static unsigned logged_skip = 0;

  if (!is_initialized)
    is_initialized = (int (*)(void *))((uintptr_t)text_base + 0x0b49d0 + 1);

  if ((uintptr_t)clump < 0x10000 || !is_initialized ||
      !vc_anim_try_init_clump(clump, is_initialized, "SetBlendDeltas")) {
    if (logged_skip++ < 64) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[anim-guard] SetBlendDeltas skip clump=%p flags=0x%x initialized=0\n",
                clump, flags);
        fclose(f);
      }
    }
    return;
  }

  static const uint16_t orig[4] = {0x4b0e, 0xb410, 0x447b, 0x681b};
  const uintptr_t target = (uintptr_t)text_base + 0x0b4a30;
  typedef void (*set_deltas_fn)(void *, unsigned int, uint32_t);

  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  ((set_deltas_fn)(target + 1))(clump, flags, delta_bits);
  hook_arm(target + 1, (uintptr_t)my_animblend_clump_set_blend_deltas_guard);
  so_flush_caches();
  so_make_text_executable();
}

void *my_anim_manager_add_animation_guard(void *clump, int group, int anim) {
  static void *(*orig_add)(void *, int, int) = NULL;
  static int (*is_initialized)(void *) = NULL;
  static unsigned logged_skip = 0;

  if (!orig_add) {
    static const uint16_t orig[4] = {0xb538, 0x4605, 0x4608, 0x4611};
    orig_add = (void *(*)(void *, int, int))
        make_thumb_trampoline8((uintptr_t)text_base + 0x0b1838, orig);
  }
  if (!is_initialized)
    is_initialized = (int (*)(void *))((uintptr_t)text_base + 0x0b49d0 + 1);

  if ((uintptr_t)clump < 0x10000 || !is_initialized ||
      !vc_anim_try_init_clump(clump, is_initialized, "AddAnimation")) {
    if (logged_skip++ < 64) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[anim-guard] AddAnimation skip clump=%p group=%d anim=%d initialized=0\n",
                clump, group, anim);
        fclose(f);
      }
    }
    return NULL;
  }
  return orig_add ? orig_add(clump, group, anim) : NULL;
}

void *my_anim_manager_add_animation_and_sync_guard(void *clump, void *sync, int group, int anim) {
  static void *(*orig_add_sync)(void *, void *, int, int) = NULL;
  static int (*is_initialized)(void *) = NULL;
  static unsigned logged_skip = 0;

  if (!orig_add_sync) {
    static const uint16_t orig[4] = {0xb570, 0x4605, 0x460e, 0x4610};
    orig_add_sync = (void *(*)(void *, void *, int, int))
        make_thumb_trampoline8((uintptr_t)text_base + 0x0b18a0, orig);
  }
  if (!is_initialized)
    is_initialized = (int (*)(void *))((uintptr_t)text_base + 0x0b49d0 + 1);

  if ((uintptr_t)clump < 0x10000 || !is_initialized ||
      !vc_anim_try_init_clump(clump, is_initialized, "AddAnimationAndSync")) {
    if (logged_skip++ < 64) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[anim-guard] AddAnimationAndSync skip clump=%p sync=%p group=%d anim=%d initialized=0\n",
                clump, sync, group, anim);
        fclose(f);
      }
    }
    return NULL;
  }
  return orig_add_sync ? orig_add_sync(clump, sync, group, anim) : NULL;
}

void my_anim_assoc_set_delete_callback_guard(void *assoc, void *cb, void *data) {
  static unsigned logged_null = 0;

  if ((uintptr_t)assoc < 0x10000) {
    if (logged_null++ < 64) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[anim-guard] SetDeleteCallback skip assoc=%p cb=%p data=%p\n",
                assoc, cb, data);
        fclose(f);
      }
    }
    return;
  }

  *(void **)((char *)assoc + 0x34) = cb;
  *(void **)((char *)assoc + 0x38) = data;
  *(uint32_t *)((char *)assoc + 0x30) = 2;
}

void my_anim_assoc_set_finish_callback_guard(void *assoc, void *cb, void *data) {
  static unsigned logged_null = 0;

  if ((uintptr_t)assoc < 0x10000) {
    if (logged_null++ < 64) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[anim-guard] SetFinishCallback skip assoc=%p cb=%p data=%p\n",
                assoc, cb, data);
        fclose(f);
      }
    }
    return;
  }

  *(void **)((char *)assoc + 0x34) = cb;
  *(void **)((char *)assoc + 0x38) = data;
  *(uint32_t *)((char *)assoc + 0x30) = 1;
}

void my_playerped_set_real_move_anim_guard(void *ped) {
  static void (*orig_set_real_move_anim)(void *) = NULL;
  static int (*is_initialized)(void *) = NULL;
  static unsigned logged_skip = 0;

  if (!orig_set_real_move_anim) {
    static const uint16_t orig[4] = {0xe92d, 0x4ff0, 0x2100, 0xb08d};
    orig_set_real_move_anim = (void (*)(void *))
        make_thumb_trampoline8((uintptr_t)text_base + 0x1bab54, orig);
  }
  if (!is_initialized)
    is_initialized = (int (*)(void *))((uintptr_t)text_base + 0x0b49d0 + 1);

  void *clump = NULL;
  if ((uintptr_t)ped >= 0x10000)
    clump = *(void **)((char *)ped + 0x4c);

  if ((uintptr_t)clump < 0x10000 || !is_initialized ||
      !vc_anim_try_init_clump(clump, is_initialized, "SetRealMoveAnim")) {
    if (logged_skip++ < 64) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[player-anim-guard] SetRealMoveAnim skip ped=%p clump=%p initialized=0\n",
                ped, clump);
        fclose(f);
      }
    }
    return;
  }

  if (orig_set_real_move_anim)
    orig_set_real_move_anim(ped);
}

/* DIAGNÓSTICO game-state: loga se o menu chama as funções de iniciar jogo.
 * (hook substitui a original — quebra o start de verdade, só p/ diagnóstico). */
void my_dosettings_start(void *thiz) { (void)thiz;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) { fprintf(f, "########## DoSettingsBeforeStartingAGame CHAMADO! (select funciona, load quebra)\n"); fclose(f); }
}
void my_frontend_shutdown(void *thiz) { (void)thiz;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) { fprintf(f, "########## RequestFrontEndShutDown CHAMADO!\n"); fclose(f); }
}
/* loga as funções REAIS de carregar o mundo (se chamadas = start dispara) */
void my_game_init_restart(void *thiz) { (void)thiz;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) { fprintf(f, "@@@@@@@@@@ CGame::InitialiseWhenRestarting CHAMADO! (mundo vai carregar)\n"); fclose(f); }
}
void my_game_initialise(void *thiz, const char *p) { (void)thiz; (void)p;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) { fprintf(f, "@@@@@@@@@@ CGame::Initialise CHAMADO!\n"); fclose(f); }
}

/* Pools de nós (CPtrNode/CEntryInfoNode) esgotam ao carregar o mundo de VC
 * (CPtrNode::operator new retornava NULL → crash em CEntity::Add). Substitui
 * por malloc/free direto = sem limite de pool. (CPtrNode=12 bytes, list node). */
static unsigned long g_ptrnode_allocs = 0;
static unsigned long g_ptrnode_frees = 0;
static unsigned char *g_node_arena = NULL;
static unsigned long g_node_arena_used = 0;
#define NODE_ARENA_SIZE (48u * 1024u * 1024u)
extern uintptr_t so_find_addr_safe(const char *);
static void log_node_pressure(const char *tag, const char *line) {
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (!f) return;
  fprintf(f, "[%s] ptr_alloc=%lu ptr_free=%lu live=%ld",
          tag, g_ptrnode_allocs, g_ptrnode_frees,
          (long)g_ptrnode_allocs - (long)g_ptrnode_frees);
  if (line) fprintf(f, " line='%.96s'", line);
  fprintf(f, "\n");
  fclose(f);
}
void *my_ptrnode_new(unsigned sz) {
  if (!g_node_arena) {
    g_node_arena = mmap(NULL, NODE_ARENA_SIZE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_node_arena == MAP_FAILED) g_node_arena = NULL;
    log_node_pressure(g_node_arena ? "node-arena-init" : "node-arena-fail", NULL);
  }
  unsigned need = sz ? sz : 12;
  need = (need + 7u) & ~7u;
  void *p = NULL;
  if (g_node_arena) {
    unsigned long off = __sync_fetch_and_add(&g_node_arena_used, need);
    if (off + need <= NODE_ARENA_SIZE) {
      p = g_node_arena + off;
    } else {
      __sync_fetch_and_sub(&g_node_arena_used, need);
      log_node_pressure("node-arena-full", NULL);
      return NULL;
    }
  } else {
    p = malloc(need);
  }
  g_ptrnode_allocs++;
  if ((g_ptrnode_allocs % 50000) == 0) {
    static char *ms_line = NULL;
    if (!ms_line) ms_line = (char *)so_find_addr_safe("_ZN11CFileLoader7ms_lineE");
    log_node_pressure("node-new", ms_line);
  }
  return p;
}
void my_ptrnode_delete(void *p) {
  if (p) {
    g_ptrnode_frees++;
    if (!g_node_arena || p < (void *)g_node_arena ||
        p >= (void *)(g_node_arena + NODE_ARENA_SIZE))
      free(p);
  }
}

static int vc_name_has_any(const char *name, const char *const *needles, size_t count) {
  if (!name) return 0;
  for (size_t i = 0; i < count; i++) {
    if (strstr(name, needles[i])) return 1;
  }
  return 0;
}

static int vc_name_starts_any(const char *name, const char *const *prefixes, size_t count) {
  if (!name) return 0;
  for (size_t i = 0; i < count; i++) {
    size_t n = strlen(prefixes[i]);
    if (!strncmp(name, prefixes[i], n)) return 1;
  }
  return 0;
}

static int vc_should_skip_broad_decor_model(const char *name, const char **reason) {
  (void)name;
  (void)reason;
  return 0;
#if 0
  static const char *const prefixes[] = {
      "LOD", "lod", "xod_", "od", "spad", "deco_", "plants", "veg_",
      "new_bush", "beach", "park", "garden", "wsh", "wash", "wosh",
      "kb_planter", "dts_telwire", "dts_nitelites", "ocdneon",
  };
  static const char *const contains[] = {
      "shadow", "shado", "neon", "sign", "window", "glass", "wire",
      "tele", "bush", "tree", "grass", "plant", "bench", "newstand",
      "phonebooth", "papermachn", "bouy", "rockpatch", "sexgarden",
      "hotel", "bighotel", "clutter", "dirt", "glue",
  };
  if (vc_name_starts_any(name, prefixes, sizeof(prefixes) / sizeof(prefixes[0]))) {
    if (reason) *reason = "prefix";
    return 1;
  }
  if (!strcmp(name, "bighotelgrnd")) return 0;
  if (vc_name_has_any(name, contains, sizeof(contains) / sizeof(contains[0]))) {
    if (reason) *reason = "contains";
    return 1;
  }
  return 0;
#endif
}

void my_load_object_instance(const char *line);
static void (*g_orig_load_object_instance)(const char *) = NULL;

static void *vc_make_thumb_trampoline(uintptr_t thumb_addr, size_t stolen_bytes) {
  uintptr_t src = thumb_addr & ~(uintptr_t)1;
  size_t tramp_size = stolen_bytes + 8;
  uint8_t *tramp = mmap(NULL, tramp_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (tramp == MAP_FAILED) return NULL;
  memcpy(tramp, (void *)src, stolen_bytes);
  uint16_t *tail = (uint16_t *)(tramp + stolen_bytes);
  tail[0] = 0xf8df; /* LDR.W PC, [PC, #0] */
  tail[1] = 0xf000;
  *(uint32_t *)(tail + 2) = (uint32_t)((src + stolen_bytes) | 1u);
  __builtin___clear_cache((char *)tramp, (char *)tramp + tramp_size);
  return (void *)((uintptr_t)tramp | 1u);
}

void install_load_object_instance_hook(void) {
  uintptr_t target = (uintptr_t)text_base + 0x13e21c + 1;
  if (!g_orig_load_object_instance)
    g_orig_load_object_instance = (void (*)(const char *))vc_make_thumb_trampoline(target, 8);
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    fprintf(f, "[hook] LoadObjectInstance target=%p trampoline=%p\n",
            (void *)target, (void *)g_orig_load_object_instance);
    fclose(f);
  }
  hook_arm(target, (uintptr_t)my_load_object_instance);
}

static void (*g_orig_renderer_prerender)(void) = NULL;
static void (*g_orig_renderer_scanworld)(void) = NULL;
static void (*g_orig_renderer_construct)(void) = NULL;
static void (*g_orig_renderer_everything_bar_roads)(void) = NULL;
static void (*g_orig_renderer_roads)(void) = NULL;
static void (*g_orig_renderer_one_nonroad)(void *) = NULL;
static void (*g_orig_renderer_one_road)(void *) = NULL;
static void (*g_orig_renderer_scan_sector_list)(void *) = NULL;
static int (*g_orig_renderer_setup_entity_visibility)(void *) = NULL;
static void (*g_orig_entity_render)(void *) = NULL;
static void *(*g_orig_rw_plugin_registry_copy_object)(const void *, void *, const void *) = NULL;

static int vc_read_int_symbol(const char *name, int fallback) {
  int *p = (int *)so_find_addr_safe(name);
  return p ? *p : fallback;
}

static void vc_renderer_log(const char *name, unsigned n, const char *phase) {
  if (n >= 16 && (n % 120) != 0) return;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (!f) return;
  fprintf(f, "[renderer] %s %s n=%u drawsA=%u drawsE=%u vis=%d invis=%d fbo=%d tex=%u\n",
          name, phase, n, g_gl_draw_arrays_frame, g_gl_draw_elements_frame,
          vc_read_int_symbol("_ZN9CRenderer23ms_nNoOfVisibleEntitiesE", -999),
          vc_read_int_symbol("_ZN9CRenderer25ms_nNoOfInVisibleEntitiesE", -999),
          g_current_framebuffer, g_current_fbo_color_tex);
  fclose(f);
}

void my_renderer_prerender_trace(void) {
  static unsigned n = 0;
  unsigned id = ++n;
  vc_renderer_log("PreRender", id, "begin");
  if (g_orig_renderer_prerender) g_orig_renderer_prerender();
  vc_renderer_log("PreRender", id, "end");
}

void my_renderer_scanworld_trace(void) {
  static unsigned n = 0;
  unsigned id = ++n;
  vc_renderer_log("ScanWorld", id, "begin");
  if (g_orig_renderer_scanworld) g_orig_renderer_scanworld();
  vc_renderer_log("ScanWorld", id, "end");
}

void my_renderer_construct_trace(void) {
  static unsigned n = 0;
  unsigned id = ++n;
  vc_renderer_log("ConstructRenderList", id, "begin");
  if (g_orig_renderer_construct) g_orig_renderer_construct();
  vc_renderer_log("ConstructRenderList", id, "end");
}

void my_renderer_everything_bar_roads_trace(void) {
  static unsigned n = 0;
  unsigned id = ++n;
  vc_renderer_log("RenderEverythingBarRoads", id, "begin");
  if (g_orig_renderer_everything_bar_roads) g_orig_renderer_everything_bar_roads();
  vc_renderer_log("RenderEverythingBarRoads", id, "end");
}

void my_renderer_roads_trace(void) {
  static unsigned n = 0;
  unsigned id = ++n;
  vc_renderer_log("RenderRoads", id, "begin");
  if (g_orig_renderer_roads) g_orig_renderer_roads();
  vc_renderer_log("RenderRoads", id, "end");
}

void my_renderer_one_nonroad_trace(void *entity) {
  static unsigned n = 0;
  unsigned id = ++n;
  unsigned draws_before = g_gl_draw_elements_frame;
  void *vtable = NULL;
  void *slot38 = NULL;
  void *slot3c = NULL;
  void *slot40 = NULL;
  int16_t model_id = -1;
  unsigned char flags54 = 0;
  if ((uintptr_t)entity >= 0x10000) {
    vtable = *(void **)entity;
    if ((uintptr_t)vtable >= 0x10000) {
      slot38 = *(void **)((uint8_t *)vtable + 0x38);
      slot3c = *(void **)((uint8_t *)vtable + 0x3c);
      slot40 = *(void **)((uint8_t *)vtable + 0x40);
    }
    model_id = *(int16_t *)((uint8_t *)entity + 0x60);
    flags54 = *((uint8_t *)entity + 0x54);
  }
  if (id <= 96 || (id % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[renderer] RenderOneNonRoad begin n=%u swapFrame=%d ent=%p model=%d flags54=0x%02x vt=%p slot38=%p slot3c=%p slot40=%p drawsA=%u drawsE=%u\n",
              id, g_swap_frame, entity, model_id, flags54, vtable, slot38, slot3c, slot40,
              g_gl_draw_arrays_frame, g_gl_draw_elements_frame);
      fclose(f);
    }
  }
  if (g_orig_renderer_one_nonroad) g_orig_renderer_one_nonroad(entity);
  if (id <= 96 || (id % 500) == 0 || g_gl_draw_elements_frame != draws_before) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[renderer] RenderOneNonRoad end n=%u swapFrame=%d ent=%p model=%d drawsE=%u->%u drawsA=%u\n",
              id, g_swap_frame, entity, model_id, draws_before, g_gl_draw_elements_frame,
              g_gl_draw_arrays_frame);
      fclose(f);
    }
  }
}

void my_renderer_one_road_trace(void *entity) {
  static unsigned n = 0;
  unsigned id = ++n;
  if (id <= 48 || (id % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[renderer] RenderOneRoad n=%u ent=%p drawsA=%u drawsE=%u\n",
              id, entity, g_gl_draw_arrays_frame, g_gl_draw_elements_frame);
      fclose(f);
    }
  }
  if (g_orig_renderer_one_road) g_orig_renderer_one_road(entity);
}

void my_renderer_scan_sector_poly_noop(void *poly, int sectors, void *callback) {
  static unsigned n = 0;
  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[renderer-guard] ScanSectorPoly no-op poly=%p sectors=%d cb=%p\n",
              poly, sectors, callback);
      fclose(f);
    }
  }
}

void my_renderer_scan_sector_list_trace(void *list) {
  static unsigned n = 0;
  unsigned id = ++n;
  int vis_before = vc_read_int_symbol("_ZN9CRenderer23ms_nNoOfVisibleEntitiesE", -999);
  int invis_before = vc_read_int_symbol("_ZN9CRenderer25ms_nNoOfInVisibleEntitiesE", -999);
  void *first = NULL;
  if ((uintptr_t)list >= 0x10000) first = *(void **)list;
  if (g_orig_renderer_scan_sector_list) g_orig_renderer_scan_sector_list(list);
  int vis_after = vc_read_int_symbol("_ZN9CRenderer23ms_nNoOfVisibleEntitiesE", -999);
  int invis_after = vc_read_int_symbol("_ZN9CRenderer25ms_nNoOfInVisibleEntitiesE", -999);
  if (id <= 64 || (id % 500) == 0 || vis_after != vis_before || invis_after != invis_before) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[renderer] ScanSectorList n=%u list=%p first=%p vis=%d->%d invis=%d->%d drawsE=%u\n",
              id, list, first, vis_before, vis_after, invis_before, invis_after,
              g_gl_draw_elements_frame);
      fclose(f);
    }
  }
}

void install_renderer_scan_sector_list_trace_hook(void) {
  uintptr_t target = (uintptr_t)text_base + 0x16afd9;
  if (!g_orig_renderer_scan_sector_list)
    g_orig_renderer_scan_sector_list =
        (void (*)(void *))vc_make_thumb_trampoline(target, 8);
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    fprintf(f, "[hook] renderer ScanSectorList target=%p trampoline=%p\n",
            (void *)target, (void *)g_orig_renderer_scan_sector_list);
    fclose(f);
  }
  hook_arm(target, (uintptr_t)my_renderer_scan_sector_list_trace);
}

int my_renderer_setup_entity_visibility_trace(void *entity) {
  static unsigned n = 0;
  unsigned id = ++n;
  int16_t model_id = -1;
  unsigned char flags56 = 0;
  unsigned char flags59 = 0;
  unsigned char flags63 = 0;
  if ((uintptr_t)entity >= 0x10000) {
    model_id = *(int16_t *)((uint8_t *)entity + 0x60);
    flags56 = *((uint8_t *)entity + 0x56);
    flags59 = *((uint8_t *)entity + 0x59);
    flags63 = *((uint8_t *)entity + 0x63);
  }
  int ret = g_orig_renderer_setup_entity_visibility
                ? g_orig_renderer_setup_entity_visibility(entity)
                : 0;
  if (id <= 160 || (id % 1000) == 0 || ret != 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[renderer] SetupEntityVisibility n=%u ent=%p model=%d flags56=0x%02x flags59=0x%02x flags63=0x%02x ret=%d vis=%d invis=%d\n",
              id, entity, model_id, flags56, flags59, flags63, ret,
              vc_read_int_symbol("_ZN9CRenderer23ms_nNoOfVisibleEntitiesE", -999),
              vc_read_int_symbol("_ZN9CRenderer25ms_nNoOfInVisibleEntitiesE", -999));
      fclose(f);
    }
  }
  return ret;
}

void install_renderer_setup_entity_visibility_trace_hook(void) {
  uintptr_t target = (uintptr_t)text_base + 0x16aad9;
  if (!g_orig_renderer_setup_entity_visibility)
    g_orig_renderer_setup_entity_visibility =
        (int (*)(void *))vc_make_thumb_trampoline(target, 8);
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    fprintf(f, "[hook] renderer SetupEntityVisibility target=%p trampoline=%p\n",
            (void *)target, (void *)g_orig_renderer_setup_entity_visibility);
    fclose(f);
  }
  hook_arm(target, (uintptr_t)my_renderer_setup_entity_visibility_trace);
}

void install_renderer_render_one_trace_hook(void) {
  uintptr_t target = (uintptr_t)text_base + 0x169f35;
  if (!g_orig_renderer_one_nonroad)
    g_orig_renderer_one_nonroad =
        (void (*)(void *))vc_make_thumb_trampoline(target, 8);
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    fprintf(f, "[hook] renderer RenderOneNonRoad target=%p trampoline=%p\n",
            (void *)target, (void *)g_orig_renderer_one_nonroad);
    fclose(f);
  }
  hook_arm(target, (uintptr_t)my_renderer_one_nonroad_trace);
}

void my_entity_render_trace(void *entity) {
  static unsigned n = 0;
  unsigned id = ++n;
  unsigned draws_before = g_gl_draw_elements_frame;
  void *rwobj = NULL;
  unsigned char rwtype = 255;
  void *render_cb = NULL;
  int16_t model_id = -1;
  unsigned char flags50 = 0;
  unsigned char flags58 = 0;
  if ((uintptr_t)entity >= 0x10000) {
    model_id = *(int16_t *)((uint8_t *)entity + 0x60);
    flags50 = *((uint8_t *)entity + 0x50);
    flags58 = *((uint8_t *)entity + 0x58);
    rwobj = *(void **)((uint8_t *)entity + 0x4c);
    if ((uintptr_t)rwobj >= 0x10000) {
      rwtype = *(uint8_t *)rwobj;
      render_cb = *(void **)((uint8_t *)rwobj + 0x48);
    }
  }
  if (id <= 96 || (id % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[renderer] CEntity::Render begin n=%u swapFrame=%d ent=%p model=%d flags50=0x%02x flags58=0x%02x rw=%p rwtype=%u cb=%p drawsE=%u\n",
              id, g_swap_frame, entity, model_id, flags50, flags58, rwobj, rwtype, render_cb,
              g_gl_draw_elements_frame);
      fclose(f);
    }
  }
  if (g_orig_entity_render) g_orig_entity_render(entity);
  if (id <= 96 || (id % 500) == 0 || g_gl_draw_elements_frame != draws_before) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[renderer] CEntity::Render end n=%u swapFrame=%d ent=%p model=%d drawsE=%u->%u\n",
              id, g_swap_frame, entity, model_id, draws_before, g_gl_draw_elements_frame);
      fclose(f);
    }
  }
}

void install_entity_render_trace_hook(void) {
  uintptr_t target = (uintptr_t)text_base + 0x13a4e1;
  if (!g_orig_entity_render)
    g_orig_entity_render = (void (*)(void *))vc_make_thumb_trampoline(target, 8);
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    fprintf(f, "[hook] renderer CEntity::Render target=%p trampoline=%p\n",
            (void *)target, (void *)g_orig_entity_render);
    fclose(f);
  }
  hook_arm(target, (uintptr_t)my_entity_render_trace);
}

void install_renderer_trace_hooks(void) {
  struct Hook {
    uintptr_t off;
    void *dst;
    void **orig;
    const char *name;
  } hooks[] = {
      {0x169dd5, my_renderer_prerender_trace, (void **)&g_orig_renderer_prerender, "PreRender"},
      {0x16b7c1, my_renderer_scanworld_trace, (void **)&g_orig_renderer_scanworld, "ScanWorld"},
      {0x16be3d, my_renderer_construct_trace, (void **)&g_orig_renderer_construct, "ConstructRenderList"},
      {0x16a129, my_renderer_everything_bar_roads_trace, (void **)&g_orig_renderer_everything_bar_roads, "RenderEverythingBarRoads"},
      {0x169eb1, my_renderer_roads_trace, (void **)&g_orig_renderer_roads, "RenderRoads"},
      {0x169f35, my_renderer_one_nonroad_trace, (void **)&g_orig_renderer_one_nonroad, "RenderOneNonRoad"},
      {0x169ea5, my_renderer_one_road_trace, (void **)&g_orig_renderer_one_road, "RenderOneRoad"},
  };
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  for (size_t i = 0; i < sizeof(hooks) / sizeof(hooks[0]); i++) {
    uintptr_t target = (uintptr_t)text_base + hooks[i].off + 1;
    if (!*hooks[i].orig)
      *hooks[i].orig = vc_make_thumb_trampoline(target, 8);
    if (f) fprintf(f, "[hook] renderer %s target=%p trampoline=%p\n",
                   hooks[i].name, (void *)target, *hooks[i].orig);
    hook_arm(target, (uintptr_t)hooks[i].dst);
  }
  if (f) fclose(f);
}

void install_renderer_scanworld_guard_patches(void) {
  uintptr_t target = (uintptr_t)text_base + 0x16b9fc;
  uintptr_t target2 = (uintptr_t)text_base + 0x16ba9a;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    fprintf(f, "[patch] CRenderer::ScanWorld guards flag=%p tail=%p\n",
            (void *)target, (void *)target2);
    fclose(f);
  }
  so_make_text_writable();
  *(uint16_t *)target = 0xbf00; /* NOP: strb r2, [r3] can hit an invalid camera-derived pointer. */
  *(uint16_t *)target2 = 0xe006; /* B +12: skip tail list access when global pointer is NULL. */
  so_flush_caches();
  so_make_text_executable();
}

void *my_rw_plugin_registry_copy_object_guard(const void *registry, void *dst, const void *src) {
  static unsigned n = 0;
  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[rw-guard] _rwPluginRegistryCopyObject no-op registry=%p dst=%p src=%p\n",
              registry, dst, src);
      fclose(f);
    }
  }
  return dst;
}

void install_rw_guard_hooks(void) {
  uintptr_t target = (uintptr_t)text_base + 0x28fdd5;
  if (!g_orig_rw_plugin_registry_copy_object)
    g_orig_rw_plugin_registry_copy_object =
        (void *(*)(const void *, void *, const void *))vc_make_thumb_trampoline(target, 8);
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    fprintf(f, "[hook] rw _rwPluginRegistryCopyObject target=%p trampoline=%p\n",
            (void *)target, (void *)g_orig_rw_plugin_registry_copy_object);
    fclose(f);
  }
  hook_arm(target, (uintptr_t)my_rw_plugin_registry_copy_object_guard);
}

void my_rw_frame_destroy_hierarchy_noop(void *frame) {
  static unsigned n = 0;
  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[rw-guard] RwFrameDestroyHierarchy no-op frame=%p\n", frame);
      fclose(f);
    }
  }
}

void *my_rw_frame_clone_internal_abort(void *frame, void *root) {
  static unsigned n = 0;
  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[rw-guard] internal frame clone passthrough frame=%p root=%p\n", frame, root);
      fclose(f);
    }
  }
  return frame;
}

void *my_rp_atomic_clone_passthrough(void *atomic) {
  static unsigned n = 0;
  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[rw-guard] RpAtomicClone passthrough atomic=%p\n", atomic);
      fclose(f);
    }
  }
  return atomic;
}

void *my_rp_atomic_destroy_noop(void *atomic) {
  static unsigned n = 0;
  if (n++ < 48 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[rw-guard] RpAtomicDestroy no-op atomic=%p\n", atomic);
      fclose(f);
    }
  }
  return atomic;
}

void *my_rp_clump_clone_passthrough(void *clump) {
  static unsigned n = 0;
  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[rw-guard] RpClumpClone passthrough clump=%p\n", clump);
      fclose(f);
    }
  }
  return clump;
}

static void *vc_prepare_player_skin_hierarchy(void *clump) {
  static unsigned logged = 0;
  static void *(*get_hier_from_skin_clump)(void *) = NULL;
  static void *(*get_hier_from_clump)(void *) = NULL;
  static void *(*skin_geom_get_skin)(void *) = NULL;
  static unsigned (*skin_get_num_bones)(void *) = NULL;
  static void *(*skin_atomic_get_hier)(void *) = NULL;
  void *hier = NULL;
  void *first_skin_atomic = NULL;
  void *first_atomic_hier = NULL;
  unsigned first_num_bones = 0;
  unsigned atomics = 0;
  unsigned skinned = 0;

  if (!get_hier_from_skin_clump) {
    get_hier_from_skin_clump = (void *(*)(void *))((uintptr_t)text_base + 0x2010a8 + 1);
    get_hier_from_clump = (void *(*)(void *))((uintptr_t)text_base + 0x2010d4 + 1);
    skin_geom_get_skin = (void *(*)(void *))((uintptr_t)text_base + 0x279a04 + 1);
    skin_get_num_bones = (unsigned (*)(void *))((uintptr_t)text_base + 0x27a130 + 1);
    skin_atomic_get_hier = (void *(*)(void *))((uintptr_t)text_base + 0x2799f4 + 1);
  }

  if ((uintptr_t)clump < 0x10000)
    return NULL;

  if (get_hier_from_skin_clump)
    hier = get_hier_from_skin_clump(clump);
  if ((uintptr_t)hier < 0x10000 && get_hier_from_clump)
    hier = get_hier_from_clump(clump);

  void **list = (void **)((uint8_t *)clump + 0x08);
  void *node = *list;
  unsigned guard = 0;
  while ((uintptr_t)node >= 0x10000 && node != (void *)list && guard++ < 4096) {
    uint8_t *atomic = (uint8_t *)node - 0x40;
    void *next = *(void **)node;
    void *geometry = *(void **)(atomic + 0x18);
    void *skin = NULL;
    void *atomic_hier = NULL;
    atomics++;

    if ((uintptr_t)geometry >= 0x10000 && skin_geom_get_skin)
      skin = skin_geom_get_skin(geometry);
    if ((uintptr_t)skin >= 0x10000) {
      skinned++;
      if (!first_skin_atomic) {
        first_skin_atomic = atomic;
        if (skin_get_num_bones)
          first_num_bones = skin_get_num_bones(skin);
      }
      if (skin_atomic_get_hier)
        atomic_hier = skin_atomic_get_hier(atomic);
      if (!first_atomic_hier)
        first_atomic_hier = atomic_hier;
    }
    node = next;
  }

  if (logged++ < 64 || (logged % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f,
              "[ped-guard] skin hierarchy inspect clump=%p hier=%p atomics=%u skinned=%u first=%p atomicHier=%p bones=%u\n",
              clump, hier, atomics, skinned, first_skin_atomic, first_atomic_hier,
              first_num_bones);
      fclose(f);
    }
  }
  return hier;
}

void *my_rp_clump_render_guard(void *clump) {
  static unsigned logged_skip = 0;
  static unsigned logged_skin = 0;
  static void *(*rw_frame_get_ltm)(void *) = NULL;
  static void *(*skin_geom_get_skin)(void *) = NULL;
  static void *(*skin_atomic_get_hier)(void *) = NULL;
  static void *(*hanim_attach)(void *) = NULL;
  static void *(*update_hanim_matrices)(void *) = NULL;
  static void *(*prepare_skin_atomic)(void *, void *, void *) = NULL;
  if (!rw_frame_get_ltm) {
    rw_frame_get_ltm = (void *(*)(void *))((uintptr_t)text_base + 0x2839e4 + 1);
    skin_geom_get_skin = (void *(*)(void *))((uintptr_t)text_base + 0x279a04 + 1);
    skin_atomic_get_hier = (void *(*)(void *))((uintptr_t)text_base + 0x2799f4 + 1);
    hanim_attach = (void *(*)(void *))((uintptr_t)text_base + 0x27428c + 1);
    update_hanim_matrices = (void *(*)(void *))((uintptr_t)text_base + 0x2742fc + 1);
    prepare_skin_atomic = (void *(*)(void *, void *, void *))((uintptr_t)text_base + 0x278d68 + 1);
  }

  if ((uintptr_t)clump < 0x10000)
    return clump;

  void **list = (void **)((uint8_t *)clump + 0x08);
  void *node = *list;
  unsigned guard = 0;
  int any_failed = 0;

  while ((uintptr_t)node >= 0x10000 && node != (void *)list && guard++ < 4096) {
    uint8_t *atomic = (uint8_t *)node - 0x40;
    void *next = *(void **)node;
    unsigned char flags = *((uint8_t *)atomic + 0x02);
    void *frame = *(void **)(atomic + 0x04);
    void *geometry = *(void **)(atomic + 0x18);
    void *render_cb = *(void **)(atomic + 0x48);

    if ((flags & 0x04) && (uintptr_t)frame >= 0x10000 &&
        (uintptr_t)geometry >= 0x10000 && (uintptr_t)render_cb >= 0x10000) {
      void *skin = skin_geom_get_skin ? skin_geom_get_skin(geometry) : NULL;
      void *hier = skin_atomic_get_hier ? skin_atomic_get_hier(atomic) : NULL;
      if ((uintptr_t)skin >= 0x10000 && (uintptr_t)hier >= 0x10000) {
        int hier_nodes = *(int *)((uintptr_t)hier + 4);
        void *hier_root = *(void **)((uintptr_t)hier + 20);
        if (hanim_attach)
          hanim_attach(hier);
        if (update_hanim_matrices)
          update_hanim_matrices(hier);
        if (prepare_skin_atomic)
          prepare_skin_atomic(atomic, skin, hier);
        if (logged_skin++ < 48 || (logged_skin % 500) == 0) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) {
            fprintf(f, "[rw-guard] skin prepare atomic=%p skin=%p hier=%p nodes=%d root=%p geom=%p clump=%p\n",
                    atomic, skin, hier, hier_nodes, hier_root, geometry, clump);
            fclose(f);
          }
        }
      }
      if (rw_frame_get_ltm)
        rw_frame_get_ltm(frame);
      void *(*cb)(void *) = (void *(*)(void *))render_cb;
      if (!cb(atomic))
        any_failed = 1;
    } else if (flags & 0x04) {
      if (logged_skip++ < 96 || (logged_skip % 500) == 0) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) {
          fprintf(f, "[rw-guard] RpClumpRender skip atomic=%p flags=0x%02x frame=%p geom=%p cb=%p clump=%p\n",
                  atomic, flags, frame, geometry, render_cb, clump);
          fclose(f);
        }
      }
    }

    node = next;
  }

  return any_failed ? NULL : clump;
}

void my_ped_model_create_hit_col_skinned_noop(void *thiz, void *clump) {
  static unsigned n = 0;
  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[ped-guard] CreateHitColModelSkinned no-op this=%p clump=%p\n", thiz, clump);
      fclose(f);
    }
  }
}

void my_ped_undress_noop(void *ped, const char *tex_name) {
  static unsigned n = 0;
  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[ped-guard] CPed::Undress no-op ped=%p tex=%p", ped, tex_name);
      if ((uintptr_t)tex_name > 0x10000) fprintf(f, " '%s'", tex_name);
      fprintf(f, "\n");
      fclose(f);
    }
  }
}

void my_ped_play_footsteps_noop(void *ped) {
  static unsigned n = 0;
  if (n++ < 16 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[ped-guard] CPed::PlayFootSteps no-op ped=%p\n", ped);
      fclose(f);
    }
  }
}

int SOFTFP_ABI my_ped_is_head_above_pos_guard(void *ped, float pos) {
  static void *tramp = NULL;
  static unsigned logged_skip = 0;
  static unsigned logged_call = 0;
  static void *(*get_skin_hierarchy)(void *) = NULL;

  if (!get_skin_hierarchy)
    get_skin_hierarchy = (void *(*)(void *))((uintptr_t)text_base + 0x2010a8 + 1);

  if (!rw_ptr_valid(ped)) {
    if (logged_skip++ < 48) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[ped-guard] IsPedHeadAbovePos invalid ped=%p\n", ped);
        fclose(f);
      }
    }
    return 0;
  }

  void *clump = *(void **)((uint8_t *)ped + 0x4c);
  void *anim = *(void **)((uint8_t *)ped + 0x1b0);
  void *hier = rw_ptr_valid(clump) && get_skin_hierarchy ? get_skin_hierarchy(clump) : NULL;

  if (!rw_ptr_valid(clump) || !rw_ptr_valid(anim) || !rw_ptr_valid(hier)) {
    if (logged_skip++ < 96 || (logged_skip % 500) == 0) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[ped-guard] IsPedHeadAbovePos skip ped=%p clump=%p anim=%p hier=%p pos=%.3f\n",
                ped, clump, anim, hier, pos);
        fclose(f);
      }
    }
    return 0;
  }

  if (!tramp) {
    static const uint16_t orig[4] = {0xb570, 0xed2d, 0x8b02, 0xb084};
    tramp = make_thumb_trampoline8((uintptr_t)text_base + 0x1b1e0c, orig);
  }
  if (!tramp)
    return 0;

  if (logged_call++ < 24 || (logged_call % 1000) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[ped-guard] IsPedHeadAbovePos passthrough ped=%p clump=%p hier=%p pos=%.3f\n",
              ped, clump, hier, pos);
      fclose(f);
    }
  }

  typedef int (SOFTFP_ABI *head_above_fn)(void *, float);
  return ((head_above_fn)tramp)(ped, pos);
}

void my_ped_render_noop(void *ped) {
  static void (*orig_ped_render)(void *) = NULL;
  static unsigned n = 0;
  int16_t model_id = -1;

  if (!orig_ped_render) {
    static const uint16_t orig[4] = {0xe92d, 0x47f0, 0x4606, 0x4fad};
    orig_ped_render = (void (*)(void *))
        make_thumb_trampoline8((uintptr_t)text_base + 0x19f9fc, orig);
  }

  if ((uintptr_t)ped >= 0x10000)
    model_id = *(int16_t *)((uint8_t *)ped + 0x60);

  if (model_id != 0 && orig_ped_render) {
    orig_ped_render(ped);
    return;
  }

  void *clump = NULL;
  if ((uintptr_t)ped >= 0x10000)
    clump = *(void **)((uint8_t *)ped + 0x4c);
  if ((uintptr_t)clump >= 0x10000) {
    static void (*entity_update_rw_frame)(void *) = NULL;
    static void (*entity_update_rp_hanim)(void *) = NULL;
    if (!entity_update_rw_frame) {
      entity_update_rw_frame = (void (*)(void *))((uintptr_t)text_base + 0x13aaec + 1);
      entity_update_rp_hanim = (void (*)(void *))((uintptr_t)text_base + 0x13aaf8 + 1);
    }
    if (entity_update_rw_frame)
      entity_update_rw_frame(ped);
    if (entity_update_rp_hanim)
      entity_update_rp_hanim(ped);
    vc_prepare_player_skin_hierarchy(clump);
    if (orig_ped_render) {
      orig_ped_render(ped);
      if (n++ < 48 || (n % 500) == 0) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) {
          fprintf(f, "[ped-guard] CPed::Render player/native ped=%p model=%d clump=%p\n",
                  ped, model_id, clump);
          fclose(f);
        }
      }
      return;
    }
    void *rendered = my_rp_clump_render_guard(clump);
    if (n++ < 48 || (n % 500) == 0) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[ped-guard] CPed::Render player/clump-only ped=%p model=%d clump=%p ret=%p\n",
                ped, model_id, clump, rendered);
        fclose(f);
      }
    }
    return;
  }

  if (n++ < 48 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[ped-guard] CPed::Render player/no-op ped=%p model=%d clump=%p\n",
              ped, model_id, clump);
      fclose(f);
    }
  }
}

void *SOFTFP_ABI my_visibility_render_fading_atomic_guard(void *atomic, float alpha) {
  static unsigned n = 0;
  static void *(*orig_render_fading_atomic)(void *, uint32_t) = NULL;

  if (!orig_render_fading_atomic) {
    static const uint16_t orig[4] = {0xe92d, 0x41f0, 0xed2d, 0x8b02};
    orig_render_fading_atomic =
        (void *(*)(void *, uint32_t))make_thumb_trampoline8((uintptr_t)text_base + 0x20329c, orig);
  }

  if ((uintptr_t)atomic >= 0x10000 && isfinite(alpha) &&
      alpha > -100000.0f && alpha < 100000.0f && orig_render_fading_atomic) {
    uint32_t alpha_bits;
    memcpy(&alpha_bits, &alpha, sizeof(alpha_bits));
    return orig_render_fading_atomic(atomic, alpha_bits);
  }

  if (!atomic || n < 48 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[visibility-guard] RenderFadingAtomic skip atomic=%p alpha=%f\n",
              atomic, alpha);
      fclose(f);
    }
  }
  n++;
  return atomic;
}

void my_particle_object_update_all_noop(void) {
  static unsigned n = 0;
  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[particle-guard] CParticleObject::UpdateAll no-op\n");
      fclose(f);
    }
  }
}

void my_moving_things_update_noop(void) {
  static unsigned n = 0;
  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[moving-guard] CMovingThings::Update no-op\n");
      fclose(f);
    }
  }
}

void my_rw_frame_sync_dirty_noop(void) {
  static unsigned n = 0;
  static int in_call = 0;
  static int *curr_level = NULL;
  static const uint16_t orig[4] = {0xe92d, 0x47f0, 0xf8df, 0x80b0};
  const uintptr_t target = (uintptr_t)text_base + 0x2866e4;
  typedef void (*sync_fn)(void);

  if (in_call)
    return;

  if (!curr_level)
    curr_level = (int *)so_find_addr_safe("_ZN5CGame9currLevelE");
  if (curr_level && *curr_level >= 1) {
    if (n++ < 24 || (n % 500) == 0) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[rw-guard] _rwFrameSyncDirty level=%d no-op\n", *curr_level);
        fclose(f);
      }
    }
    return;
  }

  if (n++ < 24 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[rw-guard] _rwFrameSyncDirty passthrough\n");
      fclose(f);
    }
  }

  in_call = 1;
  so_make_text_writable();
  patch_thumb8(target, orig);
  so_flush_caches();
  ((sync_fn)(target + 1))();
  hook_arm(target + 1, (uintptr_t)my_rw_frame_sync_dirty_noop);
  so_flush_caches();
  so_make_text_executable();
  in_call = 0;
}

void *my_animblend_clump_get_assoc_null(void *clump, unsigned int id) {
  static unsigned n = 0;
  if (n++ < 48 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[anim-guard] RpAnimBlendClumpGetAssociation clump=%p id=%u -> NULL\n",
              clump, id);
      fclose(f);
    }
  }
  return NULL;
}

void my_anim_manager_uncompress_animation_noop(void *hierarchy) {
  static unsigned n = 0;
  if (n++ < 64 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[anim-guard] CAnimManager::UncompressAnimation no-op hierarchy=%p\n",
              hierarchy);
      fclose(f);
    }
  }
}

static int vc_anim_plausible_ptr(void *ptr) {
  uintptr_t p = (uintptr_t)ptr;
  return p >= 0xd0000000u && p < 0xf0000000u && !(p & 3u);
}

static int vc_anim_update_arg_safe(void *arg, int *bad_slot, void **bad_node, void **bad_seq) {
  if (!vc_anim_plausible_ptr(arg))
    return 0;

  uint8_t *slots = (uint8_t *)arg + 4;
  for (int i = 0; i < 64; ++i) {
    void *node = *(void **)(slots + (i * 4));
    if (!node)
      return 1;
    if (!vc_anim_plausible_ptr(node)) {
      if (bad_slot) *bad_slot = i;
      if (bad_node) *bad_node = node;
      if (bad_seq) *bad_seq = NULL;
      return 0;
    }

    void *blend_node = *(void **)((uint8_t *)node + 20);
    if (blend_node) {
      void *seq = *(void **)((uint8_t *)node + 24);
      if (!vc_anim_plausible_ptr(blend_node) || !vc_anim_plausible_ptr(seq)) {
        if (bad_slot) *bad_slot = i;
        if (bad_node) *bad_node = node;
        if (bad_seq) *bad_seq = seq;
        return 0;
      }
    }
  }

  if (bad_slot) *bad_slot = 64;
  if (bad_node) *bad_node = NULL;
  if (bad_seq) *bad_seq = NULL;
  return 0;
}

void my_animblend_clumpdata_for_all_frames_guard(void *data, void (*cb)(void *, void *), void *arg) {
  static void (*orig_for_all_frames)(void *, void (*)(void *, void *), void *) = NULL;
  static unsigned skipped = 0;
  static unsigned sampled = 0;
  static unsigned bad_args = 0;
  uintptr_t ra = (uintptr_t)__builtin_return_address(0);

  if (!orig_for_all_frames) {
    static const uint16_t orig[4] = {0xe92d, 0x41f0, 0x4606, 0x6883};
    orig_for_all_frames = (void (*)(void *, void (*)(void *, void *), void *))
        make_thumb_trampoline8((uintptr_t)text_base + 0x0ae608, orig);
  }

  if (ra >= (uintptr_t)text_base + 0x0b1204 &&
      ra < (uintptr_t)text_base + 0x0b1460) {
    uintptr_t cb_off = (uintptr_t)cb - (uintptr_t)text_base;
    if (cb_off == 0x0b036d || cb_off == 0x0afecd) {
      int count = data ? *(int *)((uintptr_t)data + 8) : 0;
      uint8_t *frames = data ? *(uint8_t **)((uintptr_t)data + 16) : NULL;
      int called = 0;
      int null_frame = 0;
      int first_idx = -1;
      void *first_rw = NULL;
      int max_calls = 1;
      int bad_slot = -1;
      void *bad_node = NULL;
      void *bad_seq = NULL;
      int arg_safe = vc_anim_update_arg_safe(arg, &bad_slot, &bad_node, &bad_seq);
      if (count > 0 && count < 256 && frames) {
        if (sampled++ < 12) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) {
            int sample_count = count < 8 ? count : 8;
            fprintf(f, "[anim-guard] frame sample cb=+0x%lx data=%p frames=%p count=%d arg=%p ra=+0x%lx\n",
                    cb_off, data, frames, count, arg, ra - (uintptr_t)text_base);
            for (int j = 0; j < sample_count; ++j) {
              uint8_t *fd = frames + (j * 24);
              uint32_t w0 = *(uint32_t *)(fd + 0);
              uint32_t w4 = *(uint32_t *)(fd + 4);
              uint32_t w8 = *(uint32_t *)(fd + 8);
              uint32_t w12 = *(uint32_t *)(fd + 12);
              uint32_t w16 = *(uint32_t *)(fd + 16);
              uint32_t w20 = *(uint32_t *)(fd + 20);
              fprintf(f,
                      "[anim-guard] frame[%d] fd=%p w=%08x %08x %08x %08x %08x %08x ptrmask=%c%c%c%c%c%c\n",
                      j, fd, w0, w4, w8, w12, w16, w20,
                      (w0 >= 0x80000000u && !(w0 & 3u)) ? '0' : '-',
                      (w4 >= 0x80000000u && !(w4 & 3u)) ? '4' : '-',
                      (w8 >= 0x80000000u && !(w8 & 3u)) ? '8' : '-',
                      (w12 >= 0x80000000u && !(w12 & 3u)) ? 'c' : '-',
                      (w16 >= 0x80000000u && !(w16 & 3u)) ? 'g' : '-',
                      (w20 >= 0x80000000u && !(w20 & 3u)) ? 'k' : '-');
            }
            fclose(f);
          }
        }
        if ((cb_off == 0x0b036d && count >= 30) || cb_off == 0x0afecd)
          max_calls = count;
        if (!arg_safe) {
          if (bad_args++ < 32 || (bad_args % 500) == 0) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) {
              fprintf(f,
                      "[anim-guard] ForAllFrames unsafe arg cb=+0x%lx data=%p frames=%p count=%d arg=%p badSlot=%d node=%p seq=%p ra=+0x%lx\n",
                      cb_off, data, frames, count, arg, bad_slot, bad_node, bad_seq,
                      ra - (uintptr_t)text_base);
              fclose(f);
            }
          }
          called = 0;
          null_frame = count;
          goto done_safe_for_all_frames;
        }
        for (int i = 0; i < count; ++i) {
          uint8_t *frame_data = frames + (i * 24);
          void *rw_frame = *(void **)(frame_data + 16);
          uintptr_t rw = (uintptr_t)rw_frame;
          if (!vc_anim_plausible_ptr(rw_frame)) {
            null_frame++;
            continue;
          }
          if (called >= max_calls) {
            null_frame++;
            continue;
          }
          if (!vc_anim_update_arg_safe(arg, &bad_slot, &bad_node, &bad_seq)) {
            if (bad_args++ < 32 || (bad_args % 500) == 0) {
              FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
              if (f) {
                fprintf(f,
                        "[anim-guard] ForAllFrames arg became unsafe cb=+0x%lx data=%p frames=%p count=%d i=%d called=%d arg=%p badSlot=%d node=%p seq=%p ra=+0x%lx\n",
                        cb_off, data, frames, count, i, called, arg, bad_slot, bad_node, bad_seq,
                        ra - (uintptr_t)text_base);
                fclose(f);
              }
            }
            null_frame += count - i;
            break;
          }
          first_idx = i;
          first_rw = rw_frame;
          cb(frame_data, arg);
          called++;
        }
      }
done_safe_for_all_frames:
      if (skipped++ < 64 || (skipped % 500) == 0) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) {
          fprintf(f, "[anim-guard] ForAllFrames safe cb=+0x%lx data=%p frames=%p count=%d called=%d null=%d first=%d/%p arg=%p ra=+0x%lx\n",
                  cb_off, data, frames, count, called, null_frame, first_idx, first_rw, arg,
                  ra - (uintptr_t)text_base);
          fclose(f);
        }
      }
      return;
    }
    if (skipped++ < 64 || (skipped % 500) == 0) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[anim-guard] ForAllFrames skip from update data=%p cb=%p(+0x%lx) arg=%p ra=+0x%lx\n",
                data, cb, cb_off, arg, ra - (uintptr_t)text_base);
        fclose(f);
      }
    }
    return;
  }

  if (orig_for_all_frames)
    orig_for_all_frames(data, cb, arg);
}

void SOFTFP_ABI my_animblend_clump_update_animations_noop(void *clump, float step, int on_screen) {
  static unsigned n = 0;
  static int (*get_clump_plugin_offset)(uint32_t) = NULL;
  void *data = NULL;
  void *link_next = NULL;
  void *link_prev = NULL;
  int num_frames = -1;
  void *velocity = NULL;
  void *frames = NULL;
  void *assoc = NULL;
  int assoc_nodes_count = -1;
  int assoc_pad = -1;
  void *assoc_nodes = NULL;
  void *assoc_hier = NULL;
  float assoc_blend = 0.0f;
  float assoc_delta = 0.0f;
  float assoc_time = 0.0f;
  float assoc_speed = 0.0f;
  int assoc_anim = -1;
  int assoc_flags = 0;
  int assoc_cb_type = -1;
  void *assoc_cb = NULL;
  int assoc_valid = 0;

  if (!get_clump_plugin_offset)
    get_clump_plugin_offset = (int (*)(uint32_t))((uintptr_t)text_base + 0x2aff28 + 1);
  if ((uintptr_t)clump >= 0x10000 && get_clump_plugin_offset) {
    int off = get_clump_plugin_offset(0x0253f2fdU);
    if (off >= 0 && off < 4096)
      data = *(void **)((uint8_t *)clump + off);
  }
  if ((uintptr_t)data >= 0x10000) {
    link_next = *(void **)((uint8_t *)data + 0x00);
    link_prev = *(void **)((uint8_t *)data + 0x04);
    num_frames = *(int *)((uint8_t *)data + 0x08);
    velocity = *(void **)((uint8_t *)data + 0x0c);
    frames = *(void **)((uint8_t *)data + 0x10);
    vc_anim_cache_num_frames(clump, num_frames);
    if ((uintptr_t)link_next >= 0x10000) {
      assoc = (uint8_t *)link_next - 0x04;
      assoc_nodes_count = *(int16_t *)((uint8_t *)assoc + 0x0c);
      assoc_pad = *(int16_t *)((uint8_t *)assoc + 0x0e);
      assoc_nodes = *(void **)((uint8_t *)assoc + 0x10);
      assoc_hier = *(void **)((uint8_t *)assoc + 0x14);
      assoc_blend = *(float *)((uint8_t *)assoc + 0x18);
      assoc_delta = *(float *)((uint8_t *)assoc + 0x1c);
      assoc_time = *(float *)((uint8_t *)assoc + 0x20);
      assoc_speed = *(float *)((uint8_t *)assoc + 0x24);
      assoc_anim = *(int *)((uint8_t *)assoc + 0x28);
      assoc_flags = *(uint16_t *)((uint8_t *)assoc + 0x2e);
      assoc_cb_type = *(int *)((uint8_t *)assoc + 0x30);
      assoc_cb = *(void **)((uint8_t *)assoc + 0x34);
      assoc_valid = (assoc_nodes_count > 0 && assoc_nodes_count <= 128 &&
                     (uintptr_t)assoc_nodes >= 0x10000 &&
                     (uintptr_t)assoc_hier >= 0x10000);
    }
  }
  if (n++ < 48 || (n % 500) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f,
              "[anim-guard] RpAnimBlendClumpUpdateAnimations skip clump=%p step=%f on=%d data=%p link=%p/%p num=%d vel=%p frames=%p assoc=%p assocNum=%d pad=%d nodes=%p hier=%p blend=%f delta=%f time=%f speed=%f anim=%d flags=0x%x cb=%d/%p valid=%d\n",
              clump, step, on_screen, data, link_next, link_prev, num_frames,
              velocity, frames, assoc, assoc_nodes_count, assoc_pad, assoc_nodes,
              assoc_hier, assoc_blend, assoc_delta, assoc_time, assoc_speed,
              assoc_anim, assoc_flags, assoc_cb_type, assoc_cb, assoc_valid);
      fclose(f);
    }
  }
  /*
   * Keep this hook stable.  The original prologue starts with a PC-relative
   * literal load, so a tiny trampoline is not safe, and hot unhook/rehook here
   * races with animation callbacks that can still hold pointers into the
   * patched entrypoint.
   */
  (void)assoc_valid;
}

void *my_rw_v3d_transform_points_guard(void *out, const void *in, int count, const void *matrix) {
  if (!rw_ptr_valid(out)) return NULL;
  if (!rw_ptr_valid(in) || count <= 0 || count > 4096) return out;
  if (!rw_ptr_valid(matrix)) {
    static unsigned n = 0;
    if (n++ < 24 || (n % 500) == 0) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[rw-guard] RwV3dTransformPoints invalid matrix=%p out=%p in=%p count=%d\n",
                matrix, out, in, count);
        fclose(f);
      }
    }
    memcpy(out, in, (size_t)count * 12u);
    return out;
  }

  const float *m = (const float *)matrix;
  const float *src = (const float *)in;
  float *dst = (float *)out;
  for (int i = 0; i < count; i++) {
    float x = src[i * 3 + 0];
    float y = src[i * 3 + 1];
    float z = src[i * 3 + 2];
    dst[i * 3 + 0] = x * m[0] + y * m[4] + z * m[8]  + m[12];
    dst[i * 3 + 1] = x * m[1] + y * m[5] + z * m[9]  + m[13];
    dst[i * 3 + 2] = x * m[2] + y * m[6] + z * m[10] + m[14];
  }
  return out;
}

void install_rw_transform_guard_hooks(void) {
  uintptr_t target = (uintptr_t)text_base + 0x290165;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    fprintf(f, "[hook] rw RwV3dTransformPoints target=%p shim-transform\n",
            (void *)target);
    fclose(f);
  }
  hook_arm(target, (uintptr_t)my_rw_v3d_transform_points_guard);
}

static volatile const char *g_ipl_watch_line = NULL;
static volatile unsigned long g_ipl_watch_n = 0;
static volatile int g_ipl_watch_active = 0;
static volatile int g_ipl_watch_samples = 0;

static char *vc_append_lit(char *p, const char *s) {
  while (*s) *p++ = *s++;
  return p;
}

static char *vc_append_hex(char *p, unsigned long v) {
  static const char hex[] = "0123456789abcdef";
  *p++ = '0';
  *p++ = 'x';
  int started = 0;
  for (int shift = (int)(sizeof(unsigned long) * 8) - 4; shift >= 0; shift -= 4) {
    unsigned d = (unsigned)((v >> shift) & 0xf);
    if (d || started || shift == 0) {
      *p++ = hex[d];
      started = 1;
    }
  }
  return p;
}

static char *vc_append_dec(char *p, unsigned long v) {
  char tmp[24];
  int n = 0;
  if (!v) {
    *p++ = '0';
    return p;
  }
  while (v && n < (int)sizeof(tmp)) {
    tmp[n++] = (char)('0' + (v % 10));
    v /= 10;
  }
  while (n > 0) *p++ = tmp[--n];
  return p;
}

static void vc_watchdog_write_minimal(unsigned long pc, unsigned long lr) {
  int fd = open("/storage/roms/ports/gtavc/debug.log",
                O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0) return;
  char buf[768];
  char *p = buf;
  unsigned long tb = (unsigned long)text_base;
  p = vc_append_lit(p, "\n=== IPL WATCHDOG MIN stuck n=");
  p = vc_append_dec(p, g_ipl_watch_n);
  p = vc_append_lit(p, " sample=");
  p = vc_append_dec(p, (unsigned long)g_ipl_watch_samples + 1);
  p = vc_append_lit(p, " PC=");
  p = vc_append_hex(p, pc);
  if (pc >= tb && pc < tb + text_size) {
    p = vc_append_lit(p, " off=");
    p = vc_append_hex(p, pc - tb);
  }
  p = vc_append_lit(p, " LR=");
  p = vc_append_hex(p, lr);
  if (lr >= tb && lr < tb + text_size) {
    p = vc_append_lit(p, " lro=");
    p = vc_append_hex(p, lr - tb);
  }
  p = vc_append_lit(p, " line='");
  const char *s = (const char *)g_ipl_watch_line;
  for (int i = 0; s && s[i] && i < 220 && p < buf + (int)sizeof(buf) - 4; i++)
    *p++ = s[i];
  p = vc_append_lit(p, "'\n");
  write(fd, buf, (size_t)(p - buf));
  close(fd);
}

static void vc_ipl_watchdog_handler(int sig, siginfo_t *si, void *uc_) {
  (void)sig;
  (void)si;
  if (!g_ipl_watch_active) return;
  ucontext_t *uc = (ucontext_t *)uc_;
  unsigned long pc = uc->uc_mcontext.arm_pc;
  unsigned long lr = uc->uc_mcontext.arm_lr;
  unsigned long tb = (unsigned long)text_base;
  vc_watchdog_write_minimal(pc, lr);
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    fprintf(f, "\n=== IPL WATCHDOG stuck n=%lu sample=%d PC=0x%lx LR=0x%lx ===\n",
            g_ipl_watch_n, g_ipl_watch_samples + 1, pc, lr);
    if (pc >= tb && pc < tb + text_size)
      fprintf(f, "  PC = libGTAVC.so + 0x%lx\n", pc - tb);
    else
      fprintf(f, "  PC fora do .so\n");
    if (lr >= tb && lr < tb + text_size)
      fprintf(f, "  LR = libGTAVC.so + 0x%lx\n", lr - tb);
    fprintf(f, "  r0=%lx r1=%lx r2=%lx r3=%lx sp=%lx\n",
            uc->uc_mcontext.arm_r0, uc->uc_mcontext.arm_r1,
            uc->uc_mcontext.arm_r2, uc->uc_mcontext.arm_r3,
            uc->uc_mcontext.arm_sp);
    fprintf(f, "  r4=%lx r5=%lx r6=%lx r7=%lx r8=%lx r9=%lx r10=%lx r11=%lx r12=%lx\n",
            uc->uc_mcontext.arm_r4, uc->uc_mcontext.arm_r5,
            uc->uc_mcontext.arm_r6, uc->uc_mcontext.arm_r7,
            uc->uc_mcontext.arm_r8, uc->uc_mcontext.arm_r9,
            uc->uc_mcontext.arm_r10, uc->uc_mcontext.arm_fp,
            uc->uc_mcontext.arm_ip);
    if (g_ipl_watch_line)
      fprintf(f, "  watch-line='%.*s'\n", 220, (const char *)g_ipl_watch_line);
    fprintf(f, "  call-chain no .so (stack scan):\n");
    unsigned long *sp = (unsigned long *)uc->uc_mcontext.arm_sp;
    int found = 0;
    for (int i = 0; i < 512 && found < 16; i++) {
      unsigned long v = sp[i];
      if (v >= tb && v < tb + text_size) {
        fprintf(f, "    sp[%03d] libGTAVC.so+0x%lx\n", i, v - tb);
        found++;
      }
    }
    fclose(f);
  }
  if (++g_ipl_watch_samples >= 3) _exit(75);
}

static void vc_install_ipl_watchdog(void) {
  static int installed = 0;
  if (installed) return;
  installed = 1;
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = vc_ipl_watchdog_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGALRM, &sa, NULL);
}

static void vc_arm_ipl_watchdog(const char *line, unsigned long n) {
  vc_install_ipl_watchdog();
  g_ipl_watch_line = line;
  g_ipl_watch_n = n;
  g_ipl_watch_samples = 0;
  g_ipl_watch_active = 1;
  struct itimerval tv;
  memset(&tv, 0, sizeof(tv));
  tv.it_value.tv_sec = 2;
  tv.it_interval.tv_sec = 1;
  setitimer(ITIMER_REAL, &tv, NULL);
}

static void vc_disarm_ipl_watchdog(void) {
  struct itimerval tv;
  memset(&tv, 0, sizeof(tv));
  setitimer(ITIMER_REAL, &tv, NULL);
  g_ipl_watch_active = 0;
  g_ipl_watch_line = NULL;
}

void my_load_object_instance(const char *line) {
  static unsigned long n = 0;
  void (*orig)(const char *) = g_orig_load_object_instance;
  static void *(*get_model_info)(const char *, int *) = NULL;
  n++;
  if (!orig) {
    static unsigned missing_orig = 0;
    if (missing_orig++ < 8) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[hook-error] LoadObjectInstance trampoline missing line=%s\n", line ? line : "(null)"); fclose(f); }
    }
    return;
  }
  if (!get_model_info)
    get_model_info = (void *(*)(const char *, int *))so_find_addr_safe("_ZN10CModelInfo12GetModelInfoEPKcPi");
  if (line) {
    if (n <= 16 || (n % 1000) == 0) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[ipl-wide] calling orig without skip n=%lu line=%s\n", n, line);
        fclose(f);
      }
    }
    vc_arm_ipl_watchdog(line, n);
    orig(line);
    vc_disarm_ipl_watchdog();
    return;
  }
  if (line) {
    static unsigned trace_dts_offglassa = 0;
    if (strstr(line, "dts_offglassa") && trace_dts_offglassa++ < 12 && get_model_info) {
      int fail_id = -1;
      int ctrl_id = -1;
      void *fail_mi = get_model_info("dts_offglassa", &fail_id);
      uint8_t fail_mi_copy[24] = {0};
      void *fail_col = fail_mi ? *(void **)((uint8_t *)fail_mi + 0x3c) : NULL;
      uint8_t fail_col_copy[16] = {0};
      if (fail_mi) memcpy(fail_mi_copy, fail_mi, sizeof(fail_mi_copy));
      if (fail_col) memcpy(fail_col_copy, fail_col, sizeof(fail_col_copy));
      void *ctrl_mi = get_model_info("dts_offglassb", &ctrl_id);
      uint8_t ctrl_mi_copy[24] = {0};
      void *ctrl_col = ctrl_mi ? *(void **)((uint8_t *)ctrl_mi + 0x3c) : NULL;
      uint8_t ctrl_col_copy[16] = {0};
      if (ctrl_mi) memcpy(ctrl_mi_copy, ctrl_mi, sizeof(ctrl_mi_copy));
      if (ctrl_col) memcpy(ctrl_col_copy, ctrl_col, sizeof(ctrl_col_copy));
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f,
                "[ipl-trace] fail='dts_offglassa' id=%d mi=%p col=%p ctrl='dts_offglassb' id=%d mi=%p col=%p line=%s\n",
                fail_id, fail_mi, fail_col, ctrl_id, ctrl_mi, ctrl_col, line);
        if (fail_mi && ctrl_mi) {
          uint32_t *fm = (uint32_t *)fail_mi_copy;
          uint32_t *cm = (uint32_t *)ctrl_mi_copy;
          uint32_t *fc = fail_col ? (uint32_t *)fail_col_copy : NULL;
          uint32_t *cc = ctrl_col ? (uint32_t *)ctrl_col_copy : NULL;
          fprintf(f,
                  "[ipl-trace] fail_mi=%08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                  fm[0], fm[1], fm[2], fm[3], fm[4], fm[5], fm[6], fm[7],
                  fm[8], fm[9], fm[10], fm[11], fm[12], fm[13], fm[14], fm[15]);
          fprintf(f,
                  "[ipl-trace] ctrl_mi=%08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                  cm[0], cm[1], cm[2], cm[3], cm[4], cm[5], cm[6], cm[7],
                  cm[8], cm[9], cm[10], cm[11], cm[12], cm[13], cm[14], cm[15]);
          if (fc && cc) {
            fprintf(f,
                    "[ipl-trace] fail_col=%08x %08x %08x %08x %08x %08x %08x %08x ctrl_col=%08x %08x %08x %08x %08x %08x %08x %08x\n",
                    fc[0], fc[1], fc[2], fc[3], fc[4], fc[5], fc[6], fc[7],
                    cc[0], cc[1], cc[2], cc[3], cc[4], cc[5], cc[6], cc[7]);
          }
        }
        fclose(f);
      }
    }
    static unsigned trace_dts_nitelites1 = 0;
    if (strstr(line, "dts_nitelites1") && trace_dts_nitelites1++ < 12 && get_model_info) {
      int fail_id = -1;
      int ctrl_id = -1;
      void *fail_mi = get_model_info("dts_nitelites1", &fail_id);
      void *ctrl_mi = get_model_info("dts_offglassb", &ctrl_id);
      if (fail_mi && ctrl_mi) {
        memcpy(fail_mi, ctrl_mi, 128);
        FILE *pf = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (pf) {
          fprintf(pf,
                  "[ipl-patch] copied full modelinfo from dts_offglassb to dts_nitelites1 fail_id=%d ctrl_id=%d line=%s\n",
                  fail_id, ctrl_id, line);
          fclose(pf);
        }
      }
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-call] dts_nitelites1 entering orig line=%s\n", line); fclose(f); }
      return;
    }
    if (strstr(line, "dts_nitelites1")) {
      static unsigned skip_dts_nitelites1 = 0;
      if (skip_dts_nitelites1++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dts_nitelites1 line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "dts_telwire")) {
      static unsigned skip_dts_telwire = 0;
      if (skip_dts_telwire++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dts_telwire_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    static unsigned trace_dts_telwire2 = 0;
    if (strstr(line, "dts_telwire2") && trace_dts_telwire2++ < 12 && get_model_info) {
      int fail_id = -1;
      int ctrl_id = -1;
      void *fail_mi = get_model_info("dts_telwire2", &fail_id);
      void *ctrl_mi = get_model_info("dts_telwire1", &ctrl_id);
      if (fail_mi && ctrl_mi) {
        memcpy(fail_mi, ctrl_mi, 128);
        FILE *pf = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (pf) {
          fprintf(pf,
                  "[ipl-patch] copied full modelinfo from dts_telwire1 to dts_telwire2 fail_id=%d ctrl_id=%d line=%s\n",
                  fail_id, ctrl_id, line);
          fclose(pf);
        }
      }
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-skip] dts_telwire2 line=%s\n", line); fclose(f); }
      return;
    }
    if (strstr(line, "cardboardbox")) {
      static unsigned skipped_box = 0;
      if (skipped_box++ < 24) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] cardboardbox line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "bouy")) {
      static unsigned skipped_bouy = 0;
      if (skipped_bouy++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] bouy_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "rockpatch")) {
      static unsigned skipped_rockpatch = 0;
      if (skipped_rockpatch++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] rockpatch_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "shado") || strstr(line, "shadow")) {
      static unsigned skipped_shadow_any = 0;
      if (skipped_shadow_any++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] shadow_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "sexgarden")) {
      static unsigned skipped_sexgarden_any = 0;
      if (skipped_sexgarden_any++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] sexgarden_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "papermachn01")) {
      static unsigned skipped_paper = 0;
      if (skipped_paper++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] papermachn01 line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "veg_palwee03")) {
      static unsigned skipped_veg = 0;
      if (skipped_veg++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] veg_palwee03 line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "veg_")) {
      static unsigned skipped_veg_any = 0;
      if (skipped_veg_any++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] veg_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "miamiland")) {
      static unsigned skipped_miami = 0;
      if (skipped_miami++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] miamiland line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhroad")) {
      static unsigned skipped_lhroad = 0;
      if (skipped_lhroad++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhroad line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "littlehacoast")) {
      static unsigned skipped_littleha = 0;
      if (skipped_littleha++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] littlehacoast line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lh_")) {
      static unsigned skipped_lh_any = 0;
      if (skipped_lh_any++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lh_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "havanahouse")) {
      static unsigned skipped_havanahouse = 0;
      if (skipped_havanahouse++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] havanahouse line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lithava")) {
      static unsigned skipped_lithava = 0;
      if (skipped_lithava++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lithava line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lithavroad")) {
      static unsigned skipped_lithavroad = 0;
      if (skipped_lithavroad++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lithavroad line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "littlharoad")) {
      static unsigned skipped_littlharoad = 0;
      if (skipped_littlharoad++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] littlharoad line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhwallbit")) {
      static unsigned skipped_lhwallbit = 0;
      if (skipped_lhwallbit++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhwallbit line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhavnew_bush")) {
      static unsigned skipped_lhavnew_bush = 0;
      if (skipped_lhavnew_bush++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhavnew_bush line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "hospital")) {
      static unsigned skipped_hospital = 0;
      if (skipped_hospital++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] hospital line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhcoast")) {
      static unsigned skipped_lhcoast = 0;
      if (skipped_lhcoast++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhcoast line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "havhotel")) {
      static unsigned skipped_havhotel = 0;
      if (skipped_havhotel++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] havhotel line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "havbuild")) {
      static unsigned skipped_havbuild = 0;
      if (skipped_havbuild++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] havbuild line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lithahouse")) {
      static unsigned skipped_lithahouse = 0;
      if (skipped_lithahouse++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lithahouse line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "cof_")) {
      static unsigned skipped_cof = 0;
      if (skipped_cof++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] cof_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "cofsho_")) {
      static unsigned skipped_cofsho = 0;
      if (skipped_cofsho++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] cofsho_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lithawaste")) {
      static unsigned skipped_lithawaste = 0;
      if (skipped_lithawaste++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lithawaste line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "newramp")) {
      static unsigned skipped_newramp = 0;
      if (skipped_newramp++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] newramp line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhbasket")) {
      static unsigned skipped_lhbasket = 0;
      if (skipped_lhbasket++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhbasket line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhbuild")) {
      static unsigned skipped_lhbuild = 0;
      if (skipped_lhbuild++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhbuild line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhgrnda")) {
      static unsigned skipped_lhgrnda = 0;
      if (skipped_lhgrnda++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhgrnda line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhroofst")) {
      static unsigned skipped_lhroofst = 0;
      if (skipped_lhroofst++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhroofst line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhparkline")) {
      static unsigned skipped_lhparkline = 0;
      if (skipped_lhparkline++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhparkline line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "LODoastsky")) {
      static unsigned skipped_lodoastsky = 0;
      if (skipped_lodoastsky++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] LODoastsky line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "LODpital")) {
      static unsigned skipped_lodpital = 0;
      if (skipped_lodpital++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] LODpital line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "LODmiland")) {
      static unsigned skipped_lodmiland = 0;
      if (skipped_lodmiland++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] LODmiland line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "LODbuilding")) {
      static unsigned skipped_lodbuilding = 0;
      if (skipped_lodbuilding++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] LODbuilding line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "LODtlehacoast")) {
      static unsigned skipped_lodtlehacoast = 0;
      if (skipped_lodtlehacoast++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] LODtlehacoast line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "LODicecream")) {
      static unsigned skipped_lodicecream = 0;
      if (skipped_lodicecream++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] LODicecream line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "LODtleha")) {
      static unsigned skipped_lodtleha = 0;
      if (skipped_lodtleha++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] LODtleha line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "vegetationb")) {
      static unsigned skipped_vegetationb = 0;
      if (skipped_vegetationb++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] vegetationb line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "havbit")) {
      static unsigned skipped_havbit = 0;
      if (skipped_havbit++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] havbit line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "fuckedcars")) {
      static unsigned skipped_fuckedcars = 0;
      if (skipped_fuckedcars++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] fuckedcars line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "car_fucko")) {
      static unsigned skipped_car_fucko = 0;
      if (skipped_car_fucko++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] car_fucko line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "veged")) {
      static unsigned skipped_veged = 0;
      if (skipped_veged++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] veged line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "deli_exterior_front")) {
      static unsigned skipped_deli_exterior_front = 0;
      if (skipped_deli_exterior_front++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] deli_exterior_front line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "deli_interior")) {
      static unsigned skipped_deli_interior = 0;
      if (skipped_deli_interior++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] deli_interior line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "drycleaners")) {
      static unsigned skipped_drycleaners = 0;
      if (skipped_drycleaners++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] drycleaners line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "havgangshop")) {
      static unsigned skipped_havgangshop = 0;
      if (skipped_havgangshop++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] havgangshop line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "littleha")) {
      static unsigned skipped_littleha_any = 0;
      if (skipped_littleha_any++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] littleha_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "bushtest")) {
      static unsigned skipped_bushtest = 0;
      if (skipped_bushtest++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] bushtest line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "glue_")) {
      static unsigned skipped_glue = 0;
      if (skipped_glue++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] glue_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "telewire")) {
      static unsigned skipped_telewire = 0;
      if (skipped_telewire++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] telewire_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lihawaste")) {
      static unsigned skipped_lihawaste = 0;
      if (skipped_lihawaste++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lihawaste line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhavroad")) {
      static unsigned skipped_lhavroad = 0;
      if (skipped_lhavroad++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhavroad line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "Streetlamp")) {
      static unsigned skipped_streetlamp = 0;
      if (skipped_streetlamp++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] Streetlamp line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhdirtpass")) {
      static unsigned skipped_lhdirtpass = 0;
      if (skipped_lhdirtpass++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhdirtpass line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhddirt")) {
      static unsigned skipped_lhddirt = 0;
      if (skipped_lhddirt++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhddirt_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lh2glue")) {
      static unsigned skipped_lh2glue = 0;
      if (skipped_lh2glue++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lh2glue line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhglue")) {
      static unsigned skipped_lhglue = 0;
      if (skipped_lhglue++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhglue_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "grasspatch")) {
      static unsigned skipped_grasspatch = 0;
      if (skipped_grasspatch++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] grasspatch line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "Stonebench")) {
      static unsigned skipped_stonebench = 0;
      if (skipped_stonebench++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] Stonebench line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "miamphon")) {
      static unsigned skipped_miamphon = 0;
      if (skipped_miamphon++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] miamphon line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "BillBd")) {
      static unsigned skipped_billbd = 0;
      if (skipped_billbd++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] BillBd line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "BlackBag")) {
      static unsigned skipped_blackbag = 0;
      if (skipped_blackbag++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] BlackBag line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "Barrierm")) {
      static unsigned skipped_barrierm = 0;
      if (skipped_barrierm++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] Barrierm line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "mlamppost")) {
      static unsigned skipped_mlamppost = 0;
      if (skipped_mlamppost++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] mlamppost line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "fire_hydrant")) {
      static unsigned skipped_fire_hydrant = 0;
      if (skipped_fire_hydrant++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] fire_hydrant line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "miamifirehyd")) {
      static unsigned skipped_miamifirehyd = 0;
      if (skipped_miamifirehyd++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] miamifirehyd line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodhavabit")) {
      static unsigned skipped_lodhavabit = 0;
      if (skipped_lodhavabit++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodhavabit line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodanahouse")) {
      static unsigned skipped_lodanahouse = 0;
      if (skipped_lodanahouse++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodanahouse line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodmiland")) {
      static unsigned skipped_lodmiland = 0;
      if (skipped_lodmiland++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodmiland line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "kb_planter+bush")) {
      static unsigned skipped_kb_planter_bush = 0;
      if (skipped_kb_planter_bush++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] kb_planter+bush line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "pot_02")) {
      static unsigned skipped_pot_02 = 0;
      if (skipped_pot_02++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] pot_02 line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "hav_car_show_out")) {
      static unsigned skipped_hav_car_show_out = 0;
      if (skipped_hav_car_show_out++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] hav_car_show_out line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "hav_garagedoor")) {
      static unsigned skipped_hav_garagedoor = 0;
      if (skipped_hav_garagedoor++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] hav_garagedoor line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "hav_car_show_int")) {
      static unsigned skipped_hav_car_show_int = 0;
      if (skipped_hav_car_show_int++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] hav_car_show_int line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhbillboard05xx")) {
      static unsigned skipped_lhbillboard05xx = 0;
      if (skipped_lhbillboard05xx++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhbillboard05xx line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "carshowflags")) {
      static unsigned skipped_carshowflags = 0;
      if (skipped_carshowflags++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] carshowflags line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "wglasssmash")) {
      static unsigned skipped_wglasssmash = 0;
      if (skipped_wglasssmash++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] wglasssmash line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lha_carfence")) {
      static unsigned skipped_lha_carfence = 0;
      if (skipped_lha_carfence++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lha_carfence line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodawasteb2")) {
      static unsigned skipped_lodawasteb2 = 0;
      if (skipped_lodawasteb2++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodawasteb2 line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodhawaste")) {
      static unsigned skipped_lodhawaste = 0;
      if (skipped_lodhawaste++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodhawaste line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodbit")) {
      static unsigned skipped_lodbit = 0;
      if (skipped_lodbit++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodbit line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodbuild")) {
      static unsigned skipped_lodbuild = 0;
      if (skipped_lodbuild++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodbuild line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "phonebooth1")) {
      static unsigned skipped_phonebooth1 = 0;
      if (skipped_phonebooth1++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] phonebooth1 line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhbacknbit")) {
      static unsigned skipped_lhbacknbit = 0;
      if (skipped_lhbacknbit++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhbacknbit line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "vrates")) {
      static unsigned skipped_vrates = 0;
      if (skipped_vrates++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] vrates line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhsbackbit")) {
      static unsigned skipped_lhsbackbit = 0;
      if (skipped_lhsbackbit++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhsbackbit line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhsteps")) {
      static unsigned skipped_lhsteps = 0;
      if (skipped_lhsteps++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhsteps line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodasket")) {
      static unsigned skipped_lodasket = 0;
      if (skipped_lodasket++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodasket line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodsho_ext")) {
      static unsigned skipped_lodsho_ext = 0;
      if (skipped_lodsho_ext++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodsho_ext line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodtleha_")) {
      static unsigned skipped_lodtleha = 0;
      if (skipped_lodtleha++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodtleha_ line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodrnda")) {
      static unsigned skipped_lodrnda = 0;
      if (skipped_lodrnda++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodrnda line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lodi_")) {
      static unsigned skipped_lodi = 0;
      if (skipped_lodi++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lodi_ line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhcargo")) {
      static unsigned skipped_lhcargo = 0;
      if (skipped_lhcargo++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhcargo line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhstairs")) {
      static unsigned skipped_lhstairs = 0;
      if (skipped_lhstairs++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhstairs line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhdirt2s")) {
      static unsigned skipped_lhdirt2s = 0;
      if (skipped_lhdirt2s++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhdirt2s line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lhblbrd")) {
      static unsigned skipped_lhblbrd = 0;
      if (skipped_lhblbrd++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lhblbrd line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "havbackbit")) {
      static unsigned skipped_havbackbit = 0;
      if (skipped_havbackbit++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] havbackbit line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "labiggrass")) {
      static unsigned skipped_labiggrass = 0;
      if (skipped_labiggrass++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] labiggrass line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "bussign")) {
      static unsigned skipped_bussign = 0;
      if (skipped_bussign++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] bussign line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "vegeha")) {
      static unsigned skipped_vegeha = 0;
      if (skipped_vegeha++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] vegeha line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "signagelh")) {
      static unsigned skipped_signagelh = 0;
      if (skipped_signagelh++ < 32) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] signagelh line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "doontoon")) {
      static unsigned skipped_doontoon = 0;
      if (skipped_doontoon++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] doontoon line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "dtns_")) {
      static unsigned skipped_dtns = 0;
      if (skipped_dtns++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dtns_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "dt_")) {
      static unsigned skipped_dt = 0;
      if (skipped_dt++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dt_ line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "newbuild")) {
      static unsigned skipped_newbuild = 0;
      if (skipped_newbuild++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] newbuild line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "pharmacy_")) {
      static unsigned skipped_pharmacy = 0;
      if (skipped_pharmacy++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] pharmacy_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "dispensary_")) {
      static unsigned skipped_dispensary = 0;
      if (skipped_dispensary++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dispensary_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "bb_")) {
      static unsigned skipped_bb = 0;
      if (skipped_bb++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] bb_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "blimp_")) {
      static unsigned skipped_blimp = 0;
      if (skipped_blimp++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] blimp_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    {
      char family_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, family_check_name) == 2) {
        if (!strncmp(family_check_name, "dock", 4)) {
          static unsigned skipped_dock_family = 0;
          if (skipped_dock_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] dock-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "dk", 2)) {
          static unsigned skipped_dk_family = 0;
          if (skipped_dk_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] dk-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "b_hse_", 6)) {
          static unsigned skipped_b_hse_family = 0;
          if (skipped_b_hse_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] b_hse-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "nt_", 3)) {
          static unsigned skipped_nt_family = 0;
          if (skipped_nt_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] nt-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "sub_", 4)) {
          static unsigned skipped_sub_family = 0;
          if (skipped_sub_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] sub-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "newash", 6)) {
          static unsigned skipped_newash_family = 0;
          if (skipped_newash_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] newash-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "waterjump", 9)) {
          static unsigned skipped_waterjump_family = 0;
          if (skipped_waterjump_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] waterjump-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "plants", 6)) {
          static unsigned skipped_plants_family = 0;
          if (skipped_plants_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] plants-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "deco_", 5)) {
          static unsigned skipped_deco_family = 0;
          if (skipped_deco_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] deco-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "oceanrda", 8)) {
          static unsigned skipped_oceanrda_family = 0;
          if (skipped_oceanrda_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] oceanrda-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "oceanroad", 9) ||
            !strncmp(family_check_name, "oceanrd", 7)) {
          static unsigned skipped_oceanrd_family = 0;
          if (skipped_oceanrd_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] oceanrd-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "od", 2)) {
          static unsigned skipped_od_family = 0;
          if (skipped_od_family++ < 192) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] od-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "ocdneon", 7)) {
          static unsigned skipped_ocdneon_family = 0;
          if (skipped_ocdneon_family++ < 64) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] ocdneon-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "spad", 4)) {
          static unsigned skipped_spad_family = 0;
          if (skipped_spad_family++ < 64) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] spad-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "xod_", 4)) {
          static unsigned skipped_xod_family = 0;
          if (skipped_xod_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] xod-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "beach", 5)) {
          static unsigned skipped_beach_family = 0;
          if (skipped_beach_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] beach-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "wash", 4)) {
          static unsigned skipped_wash_family = 0;
          if (skipped_wash_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] wash-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "wsh", 3)) {
          static unsigned skipped_wsh_family = 0;
          if (skipped_wsh_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] wsh-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "noparking", 9)) {
          static unsigned skipped_noparking_family = 0;
          if (skipped_noparking_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] noparking-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "park", 4)) {
          static unsigned skipped_park_family = 0;
          if (skipped_park_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] park-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "garden", 6)) {
          static unsigned skipped_garden_family = 0;
          if (skipped_garden_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] garden-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "carwash", 7)) {
          static unsigned skipped_carwash_family = 0;
          if (skipped_carwash_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] carwash-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "basketball", 10)) {
          static unsigned skipped_basketball_family = 0;
          if (skipped_basketball_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] basketball-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "kb_planter", 10)) {
          static unsigned skipped_kb_planter_family = 0;
          if (skipped_kb_planter_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] kb_planter-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "wosh", 4)) {
          static unsigned skipped_wosh_family = 0;
          if (skipped_wosh_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] wosh-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "ggsalon", 7)) {
          static unsigned skipped_ggsalon_family = 0;
          if (skipped_ggsalon_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] ggsalon-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "ggneon", 6)) {
          static unsigned skipped_ggneon_family = 0;
          if (skipped_ggneon_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] ggneon-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "cokpool", 7)) {
          static unsigned skipped_cokpool_family = 0;
          if (skipped_cokpool_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] cokpool-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "nrth", 4)) {
          static unsigned skipped_nrth_family = 0;
          if (skipped_nrth_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] nrth-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "wtn", 3)) {
          static unsigned skipped_wtn_family = 0;
          if (skipped_wtn_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] wtn-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "wslawyers", 9)) {
          static unsigned skipped_wslawyers_family = 0;
          if (skipped_wslawyers_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] wslawyers-family line=%s\n", line); fclose(f); }
          }
          return;
        }
        if (!strncmp(family_check_name, "svehsebuild", 11)) {
          static unsigned skipped_svehsebuild_family = 0;
          if (skipped_svehsebuild_family++ < 128) {
            FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
            if (f) { fprintf(f, "[ipl-skip] svehsebuild-family line=%s\n", line); fclose(f); }
          }
          return;
        }
      }
    }
    {
      char ramp_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, ramp_check_name) == 2 &&
          !strcmp(ramp_check_name, "ramp")) {
      static unsigned skipped_ramp = 0;
      if (skipped_ramp++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] ramp_exact line=%s\n", line); fclose(f); }
      }
      return;
      }
    }
    {
      char dw_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dw_check_name) == 2 &&
          !strcmp(dw_check_name, "dw_scuz_kb")) {
        static unsigned skipped_dw_scuz_kb = 0;
        if (skipped_dw_scuz_kb++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dw_scuz_kb_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char wtn_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, wtn_check_name) == 2 &&
          !strcmp(wtn_check_name, "wtn_roadsigns01")) {
        static unsigned skipped_wtn_roadsigns01 = 0;
        if (skipped_wtn_roadsigns01++ < 128) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] wtn_roadsigns01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char shopwindows_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, shopwindows_check_name) == 2 &&
          !strcmp(shopwindows_check_name, "shopwindows_dts")) {
        static unsigned skipped_shopwindows_dts = 0;
        if (skipped_shopwindows_dts++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] shopwindows_dts_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_check_name) == 2 &&
          !strcmp(dk_check_name, "dk_dockroads01")) {
        static unsigned skipped_dk_dockroads01 = 0;
        if (skipped_dk_dockroads01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_dockroads01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk2_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk2_check_name) == 2 &&
          !strcmp(dk2_check_name, "dk_dockroads02")) {
        static unsigned skipped_dk_dockroads02 = 0;
        if (skipped_dk_dockroads02++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_dockroads02_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk3_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk3_check_name) == 2 &&
          !strcmp(dk3_check_name, "dk_dockroads03")) {
        static unsigned skipped_dk_dockroads03 = 0;
        if (skipped_dk_dockroads03++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_dockroads03_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk4_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk4_check_name) == 2 &&
          !strcmp(dk4_check_name, "dk_dockroads04")) {
        static unsigned skipped_dk_dockroads04 = 0;
        if (skipped_dk_dockroads04++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_dockroads04_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk6_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk6_check_name) == 2 &&
          !strcmp(dk6_check_name, "dk_dockroads06")) {
        static unsigned skipped_dk_dockroads06 = 0;
        if (skipped_dk_dockroads06++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_dockroads06_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks10_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks10_check_name) == 2 &&
          !strcmp(docks10_check_name, "docks10")) {
        static unsigned skipped_docks10 = 0;
        if (skipped_docks10++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks10_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks21_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks21_check_name) == 2 &&
          !strcmp(docks21_check_name, "docks21")) {
        static unsigned skipped_docks21 = 0;
        if (skipped_docks21++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks21_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks29_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks29_check_name) == 2 &&
          !strcmp(docks29_check_name, "docks29")) {
        static unsigned skipped_docks29 = 0;
        if (skipped_docks29++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks29_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks30_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks30_check_name) == 2 &&
          !strcmp(docks30_check_name, "docks30")) {
        static unsigned skipped_docks30 = 0;
        if (skipped_docks30++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks30_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks31_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks31_check_name) == 2 &&
          !strcmp(docks31_check_name, "docks31")) {
        static unsigned skipped_docks31 = 0;
        if (skipped_docks31++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks31_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks37_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks37_check_name) == 2 &&
          !strcmp(docks37_check_name, "docks37")) {
        static unsigned skipped_docks37 = 0;
        if (skipped_docks37++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks37_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks40_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks40_check_name) == 2 &&
          !strcmp(docks40_check_name, "docks40")) {
        static unsigned skipped_docks40 = 0;
        if (skipped_docks40++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks40_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks42_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks42_check_name) == 2 &&
          !strcmp(docks42_check_name, "docks42")) {
        static unsigned skipped_docks42 = 0;
        if (skipped_docks42++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks42_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks43_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks43_check_name) == 2 &&
          !strcmp(docks43_check_name, "docks43")) {
        static unsigned skipped_docks43 = 0;
        if (skipped_docks43++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks43_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk8_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk8_check_name) == 2 &&
          !strcmp(dk8_check_name, "dk_dockroads08")) {
        static unsigned skipped_dk_dockroads08 = 0;
        if (skipped_dk_dockroads08++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_dockroads08_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks46_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks46_check_name) == 2 &&
          !strcmp(docks46_check_name, "docks46")) {
        static unsigned skipped_docks46 = 0;
        if (skipped_docks46++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks46_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks47_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks47_check_name) == 2 &&
          !strcmp(docks47_check_name, "docks47")) {
        static unsigned skipped_docks47 = 0;
        if (skipped_docks47++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks47_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks48_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks48_check_name) == 2 &&
          !strcmp(docks48_check_name, "docks48")) {
        static unsigned skipped_docks48 = 0;
        if (skipped_docks48++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks48_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks49_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks49_check_name) == 2 &&
          !strcmp(docks49_check_name, "docks49")) {
        static unsigned skipped_docks49 = 0;
        if (skipped_docks49++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks49_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks50_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks50_check_name) == 2 &&
          !strcmp(docks50_check_name, "docks50")) {
        static unsigned skipped_docks50 = 0;
        if (skipped_docks50++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks50_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks51_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks51_check_name) == 2 &&
          !strcmp(docks51_check_name, "docks51")) {
        static unsigned skipped_docks51 = 0;
        if (skipped_docks51++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks51_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks52_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks52_check_name) == 2 &&
          !strcmp(docks52_check_name, "docks52")) {
        static unsigned skipped_docks52 = 0;
        if (skipped_docks52++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks52_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks53_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks53_check_name) == 2 &&
          !strcmp(docks53_check_name, "docks53")) {
        static unsigned skipped_docks53 = 0;
        if (skipped_docks53++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks53_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks85_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks85_check_name) == 2 &&
          !strcmp(docks85_check_name, "docks85")) {
        static unsigned skipped_docks85 = 0;
        if (skipped_docks85++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks85_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docksprops04_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docksprops04_check_name) == 2 &&
          !strcmp(docksprops04_check_name, "docksprops04")) {
        static unsigned skipped_docksprops04 = 0;
        if (skipped_docksprops04++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docksprops04_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docksprops10_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docksprops10_check_name) == 2 &&
          !strcmp(docksprops10_check_name, "docksprops10")) {
        static unsigned skipped_docksprops10 = 0;
        if (skipped_docksprops10++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docksprops10_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char doc_shedbig30_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, doc_shedbig30_check_name) == 2 &&
          !strcmp(doc_shedbig30_check_name, "doc_shedbig30")) {
        static unsigned skipped_doc_shedbig30 = 0;
        if (skipped_doc_shedbig30++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] doc_shedbig30_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char boatcranelg0_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, boatcranelg0_check_name) == 2 &&
          !strcmp(boatcranelg0_check_name, "boatcranelg0")) {
        static unsigned skipped_boatcranelg0 = 0;
        if (skipped_boatcranelg0++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] boatcranelg0_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char crgoshp010_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, crgoshp010_check_name) == 2 &&
          !strcmp(crgoshp010_check_name, "crgoshp010")) {
        static unsigned skipped_crgoshp010 = 0;
        if (skipped_crgoshp010++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] crgoshp010_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char cranebasea0_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, cranebasea0_check_name) == 2 &&
          !strcmp(cranebasea0_check_name, "cranebasea0")) {
        static unsigned skipped_cranebasea0 = 0;
        if (skipped_cranebasea0++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] cranebasea0_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char doc_dockwareold_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, doc_dockwareold_check_name) == 2 &&
          !strcmp(doc_dockwareold_check_name, "doc_dockwareold")) {
        static unsigned skipped_doc_dockwareold = 0;
        if (skipped_doc_dockwareold++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] doc_dockwareold_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_waretank_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_waretank_check_name) == 2 &&
          !strcmp(dk_waretank_check_name, "dk_waretank")) {
        static unsigned skipped_dk_waretank = 0;
        if (skipped_dk_waretank++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_waretank_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dockcranescale0_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dockcranescale0_check_name) == 2 &&
          !strcmp(dockcranescale0_check_name, "dockcranescale0")) {
        static unsigned skipped_dockcranescale0 = 0;
        if (skipped_dockcranescale0++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dockcranescale0_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char doc_crane_cab0_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, doc_crane_cab0_check_name) == 2 &&
          !strcmp(doc_crane_cab0_check_name, "doc_crane_cab0")) {
        static unsigned skipped_doc_crane_cab0 = 0;
        if (skipped_doc_crane_cab0++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] doc_crane_cab0_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char boatcranesm0_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, boatcranesm0_check_name) == 2 &&
          !strcmp(boatcranesm0_check_name, "boatcranesm0")) {
        static unsigned skipped_boatcranesm0 = 0;
        if (skipped_boatcranesm0++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] boatcranesm0_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dockcranescale01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dockcranescale01_check_name) == 2 &&
          !strcmp(dockcranescale01_check_name, "dockcranescale01")) {
        static unsigned skipped_dockcranescale01 = 0;
        if (skipped_dockcranescale01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dockcranescale01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char doc_crane_cab01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, doc_crane_cab01_check_name) == 2 &&
          !strcmp(doc_crane_cab01_check_name, "doc_crane_cab01")) {
        static unsigned skipped_doc_crane_cab01 = 0;
        if (skipped_doc_crane_cab01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] doc_crane_cab01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char doc_crane_cab02_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, doc_crane_cab02_check_name) == 2 &&
          !strcmp(doc_crane_cab02_check_name, "doc_crane_cab02")) {
        static unsigned skipped_doc_crane_cab02 = 0;
        if (skipped_doc_crane_cab02++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] doc_crane_cab02_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char doc_crane_cab03_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, doc_crane_cab03_check_name) == 2 &&
          !strcmp(doc_crane_cab03_check_name, "doc_crane_cab03")) {
        static unsigned skipped_doc_crane_cab03 = 0;
        if (skipped_doc_crane_cab03++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] doc_crane_cab03_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char doc_crane_cab04_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, doc_crane_cab04_check_name) == 2 &&
          !strcmp(doc_crane_cab04_check_name, "doc_crane_cab04")) {
        static unsigned skipped_doc_crane_cab04 = 0;
        if (skipped_doc_crane_cab04++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] doc_crane_cab04_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char doc_craneeggs04_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, doc_craneeggs04_check_name) == 2 &&
          !strcmp(doc_craneeggs04_check_name, "doc_craneeggs04")) {
        static unsigned skipped_doc_craneeggs04 = 0;
        if (skipped_doc_craneeggs04++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] doc_craneeggs04_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dkshipment01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dkshipment01_check_name) == 2 &&
          !strcmp(dkshipment01_check_name, "dkshipment01")) {
        static unsigned skipped_dkshipment01 = 0;
        if (skipped_dkshipment01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dkshipment01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dockfuel02_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dockfuel02_check_name) == 2 &&
          !strcmp(dockfuel02_check_name, "dockfuel02")) {
        static unsigned skipped_dockfuel02 = 0;
        if (skipped_dockfuel02++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dockfuel02_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dockfuel07_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dockfuel07_check_name) == 2 &&
          !strcmp(dockfuel07_check_name, "dockfuel07")) {
        static unsigned skipped_dockfuel07 = 0;
        if (skipped_dockfuel07++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dockfuel07_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char portcabGAZ06_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, portcabGAZ06_check_name) == 2 &&
          !strcmp(portcabGAZ06_check_name, "portcabGAZ06")) {
        static unsigned skipped_portcabGAZ06 = 0;
        if (skipped_portcabGAZ06++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] portcabGAZ06_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp03_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp03_check_name) == 2 &&
          !strcmp(dk_cargoshp03_check_name, "dk_cargoshp03")) {
        static unsigned skipped_dk_cargoshp03 = 0;
        if (skipped_dk_cargoshp03++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp03_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp04_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp04_check_name) == 2 &&
          !strcmp(dk_cargoshp04_check_name, "dk_cargoshp04")) {
        static unsigned skipped_dk_cargoshp04 = 0;
        if (skipped_dk_cargoshp04++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp04_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp05_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp05_check_name) == 2 &&
          !strcmp(dk_cargoshp05_check_name, "dk_cargoshp05")) {
        static unsigned skipped_dk_cargoshp05 = 0;
        if (skipped_dk_cargoshp05++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp05_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp10_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp10_check_name) == 2 &&
          !strcmp(dk_cargoshp10_check_name, "dk_cargoshp10")) {
        static unsigned skipped_dk_cargoshp10 = 0;
        if (skipped_dk_cargoshp10++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp10_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp12_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp12_check_name) == 2 &&
          !strcmp(dk_cargoshp12_check_name, "dk_cargoshp12")) {
        static unsigned skipped_dk_cargoshp12 = 0;
        if (skipped_dk_cargoshp12++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp12_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp24_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp24_check_name) == 2 &&
          !strcmp(dk_cargoshp24_check_name, "dk_cargoshp24")) {
        static unsigned skipped_dk_cargoshp24 = 0;
        if (skipped_dk_cargoshp24++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp24_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp25_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp25_check_name) == 2 &&
          !strcmp(dk_cargoshp25_check_name, "dk_cargoshp25")) {
        static unsigned skipped_dk_cargoshp25 = 0;
        if (skipped_dk_cargoshp25++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp25_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp28_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp28_check_name) == 2 &&
          !strcmp(dk_cargoshp28_check_name, "dk_cargoshp28")) {
        static unsigned skipped_dk_cargoshp28 = 0;
        if (skipped_dk_cargoshp28++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp28_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp31_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp31_check_name) == 2 &&
          !strcmp(dk_cargoshp31_check_name, "dk_cargoshp31")) {
        static unsigned skipped_dk_cargoshp31 = 0;
        if (skipped_dk_cargoshp31++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp31_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp32_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp32_check_name) == 2 &&
          !strcmp(dk_cargoshp32_check_name, "dk_cargoshp32")) {
        static unsigned skipped_dk_cargoshp32 = 0;
        if (skipped_dk_cargoshp32++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp32_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp35_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp35_check_name) == 2 &&
          !strcmp(dk_cargoshp35_check_name, "dk_cargoshp35")) {
        static unsigned skipped_dk_cargoshp35 = 0;
        if (skipped_dk_cargoshp35++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp35_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp40_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp40_check_name) == 2 &&
          !strcmp(dk_cargoshp40_check_name, "dk_cargoshp40")) {
        static unsigned skipped_dk_cargoshp40 = 0;
        if (skipped_dk_cargoshp40++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp40_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp41_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp41_check_name) == 2 &&
          !strcmp(dk_cargoshp41_check_name, "dk_cargoshp41")) {
        static unsigned skipped_dk_cargoshp41 = 0;
        if (skipped_dk_cargoshp41++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp41_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp47_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp47_check_name) == 2 &&
          !strcmp(dk_cargoshp47_check_name, "dk_cargoshp47")) {
        static unsigned skipped_dk_cargoshp47 = 0;
        if (skipped_dk_cargoshp47++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp47_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp50_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp50_check_name) == 2 &&
          !strcmp(dk_cargoshp50_check_name, "dk_cargoshp50")) {
        static unsigned skipped_dk_cargoshp50 = 0;
        if (skipped_dk_cargoshp50++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp50_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp51_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp51_check_name) == 2 &&
          !strcmp(dk_cargoshp51_check_name, "dk_cargoshp51")) {
        static unsigned skipped_dk_cargoshp51 = 0;
        if (skipped_dk_cargoshp51++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp51_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp53_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp53_check_name) == 2 &&
          !strcmp(dk_cargoshp53_check_name, "dk_cargoshp53")) {
        static unsigned skipped_dk_cargoshp53 = 0;
        if (skipped_dk_cargoshp53++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp53_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp54_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp54_check_name) == 2 &&
          !strcmp(dk_cargoshp54_check_name, "dk_cargoshp54")) {
        static unsigned skipped_dk_cargoshp54 = 0;
        if (skipped_dk_cargoshp54++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp54_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp64_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp64_check_name) == 2 &&
          !strcmp(dk_cargoshp64_check_name, "dk_cargoshp64")) {
        static unsigned skipped_dk_cargoshp64 = 0;
        if (skipped_dk_cargoshp64++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp64_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp66_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp66_check_name) == 2 &&
          !strcmp(dk_cargoshp66_check_name, "dk_cargoshp66")) {
        static unsigned skipped_dk_cargoshp66 = 0;
        if (skipped_dk_cargoshp66++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp66_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp70_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp70_check_name) == 2 &&
          !strcmp(dk_cargoshp70_check_name, "dk_cargoshp70")) {
        static unsigned skipped_dk_cargoshp70 = 0;
        if (skipped_dk_cargoshp70++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp70_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp71_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp71_check_name) == 2 &&
          !strcmp(dk_cargoshp71_check_name, "dk_cargoshp71")) {
        static unsigned skipped_dk_cargoshp71 = 0;
        if (skipped_dk_cargoshp71++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp71_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp72_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp72_check_name) == 2 &&
          !strcmp(dk_cargoshp72_check_name, "dk_cargoshp72")) {
        static unsigned skipped_dk_cargoshp72 = 0;
        if (skipped_dk_cargoshp72++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp72_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp7_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp7_check_name) == 2 &&
          !strncmp(dk_cargoshp7_check_name, "dk_cargoshp7", 12)) {
        static unsigned skipped_dk_cargoshp7 = 0;
        if (skipped_dk_cargoshp7++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp7_prefix line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char shipstairs_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, shipstairs_check_name) == 2 &&
          !strcmp(shipstairs_check_name, "shipstairs")) {
        static unsigned skipped_shipstairs = 0;
        if (skipped_shipstairs++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] shipstairs_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp9_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp9_check_name) == 2 &&
          !strncmp(dk_cargoshp9_check_name, "dk_cargoshp9", 12)) {
        static unsigned skipped_dk_cargoshp9 = 0;
        if (skipped_dk_cargoshp9++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp9_prefix line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp100_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp100_check_name) == 2 &&
          !strcmp(dk_cargoshp100_check_name, "dk_cargoshp100")) {
        static unsigned skipped_dk_cargoshp100 = 0;
        if (skipped_dk_cargoshp100++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp100_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dockfence_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dockfence_check_name) == 2 &&
          !strcmp(dockfence_check_name, "dockfence")) {
        static unsigned skipped_dockfence = 0;
        if (skipped_dockfence++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dockfence_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dock_camjones_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dock_camjones_check_name) == 2 &&
          !strcmp(dock_camjones_check_name, "dock_camjones")) {
        static unsigned skipped_dock_camjones = 0;
        if (skipped_dock_camjones++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dock_camjones_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dock_grassarea_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dock_grassarea_check_name) == 2 &&
          !strcmp(dock_grassarea_check_name, "dock_grassarea")) {
        static unsigned skipped_dock_grassarea = 0;
        if (skipped_dock_grassarea++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dock_grassarea_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docksware01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docksware01_check_name) == 2 &&
          !strcmp(docksware01_check_name, "docksware01")) {
        static unsigned skipped_docksware01 = 0;
        if (skipped_docksware01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docksware01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_gatebar01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_gatebar01_check_name) == 2 &&
          !strcmp(dk_gatebar01_check_name, "dk_gatebar01")) {
        static unsigned skipped_dk_gatebar01 = 0;
        if (skipped_dk_gatebar01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_gatebar01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_gatebox01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_gatebox01_check_name) == 2 &&
          !strcmp(dk_gatebox01_check_name, "dk_gatebox01")) {
        static unsigned skipped_dk_gatebox01 = 0;
        if (skipped_dk_gatebox01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_gatebox01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dockgate01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dockgate01_check_name) == 2 &&
          !strcmp(dockgate01_check_name, "dockgate01")) {
        static unsigned skipped_dockgate01 = 0;
        if (skipped_dockgate01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dockgate01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dockgate02_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dockgate02_check_name) == 2 &&
          !strcmp(dockgate02_check_name, "dockgate02")) {
        static unsigned skipped_dockgate02 = 0;
        if (skipped_dockgate02++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dockgate02_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks92_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks92_check_name) == 2 &&
          !strcmp(docks92_check_name, "docks92")) {
        static unsigned skipped_docks92 = 0;
        if (skipped_docks92++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks92_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks93_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks93_check_name) == 2 &&
          !strcmp(docks93_check_name, "docks93")) {
        static unsigned skipped_docks93 = 0;
        if (skipped_docks93++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks93_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docksprops11_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docksprops11_check_name) == 2 &&
          !strcmp(docksprops11_check_name, "docksprops11")) {
        static unsigned skipped_docksprops11 = 0;
        if (skipped_docksprops11++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docksprops11_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks95_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks95_check_name) == 2 &&
          !strcmp(docks95_check_name, "docks95")) {
        static unsigned skipped_docks95 = 0;
        if (skipped_docks95++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks95_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_gatebar02_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_gatebar02_check_name) == 2 &&
          !strcmp(dk_gatebar02_check_name, "dk_gatebar02")) {
        static unsigned skipped_dk_gatebar02 = 0;
        if (skipped_dk_gatebar02++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_gatebar02_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks96_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks96_check_name) == 2 &&
          !strcmp(docks96_check_name, "docks96")) {
        static unsigned skipped_docks96 = 0;
        if (skipped_docks96++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks96_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_bridgesupp01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_bridgesupp01_check_name) == 2 &&
          !strcmp(dk_bridgesupp01_check_name, "dk_bridgesupp01")) {
        static unsigned skipped_dk_bridgesupp01 = 0;
        if (skipped_dk_bridgesupp01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_bridgesupp01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_bridgesupp03_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_bridgesupp03_check_name) == 2 &&
          !strcmp(dk_bridgesupp03_check_name, "dk_bridgesupp03")) {
        static unsigned skipped_dk_bridgesupp03 = 0;
        if (skipped_dk_bridgesupp03++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_bridgesupp03_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_dockbridge_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_dockbridge_check_name) == 2 &&
          !strcmp(dk_dockbridge_check_name, "dk_dockbridge")) {
        static unsigned skipped_dk_dockbridge = 0;
        if (skipped_dk_dockbridge++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_dockbridge_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_rail01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_rail01_check_name) == 2 &&
          !strcmp(dk_rail01_check_name, "dk_rail01")) {
        static unsigned skipped_dk_rail01 = 0;
        if (skipped_dk_rail01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_rail01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dkcargohull2_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dkcargohull2_check_name) == 2 &&
          !strcmp(dkcargohull2_check_name, "dkcargohull2")) {
        static unsigned skipped_dkcargohull2 = 0;
        if (skipped_dkcargohull2++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dkcargohull2_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_bridgesupp04_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_bridgesupp04_check_name) == 2 &&
          !strcmp(dk_bridgesupp04_check_name, "dk_bridgesupp04")) {
        static unsigned skipped_dk_bridgesupp04 = 0;
        if (skipped_dk_bridgesupp04++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_bridgesupp04_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dkshipment03_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dkshipment03_check_name) == 2 &&
          !strcmp(dkshipment03_check_name, "dkshipment03")) {
        static unsigned skipped_dkshipment03 = 0;
        if (skipped_dkshipment03++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dkshipment03_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dkshipment05_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dkshipment05_check_name) == 2 &&
          !strcmp(dkshipment05_check_name, "dkshipment05")) {
        static unsigned skipped_dkshipment05 = 0;
        if (skipped_dkshipment05++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dkshipment05_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    if (line && strstr(line, "svegrgedoor")) {
      static unsigned skipped_svegrgedoor_early = 0;
      if (skipped_svegrgedoor_early++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] svegrgedoor_early line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (line && strstr(line, "stiltsville")) {
      static unsigned skipped_stiltsville = 0;
      if (skipped_stiltsville++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] stiltsville_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (line && strstr(line, "pierentrance")) {
      static unsigned skipped_pierentrance = 0;
      if (skipped_pierentrance++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] pierentrance_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (line && strstr(line, "scarinterior")) {
      static unsigned skipped_scarinterior = 0;
      if (skipped_scarinterior++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] scarinterior_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (line && strstr(line, "undermall")) {
      static unsigned skipped_undermall = 0;
      if (skipped_undermall++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] undermall_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (line && strstr(line, "rafael")) {
      static unsigned skipped_rafael = 0;
      if (skipped_rafael++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] rafael_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (line && strstr(line, "rafel")) {
      static unsigned skipped_rafel = 0;
      if (skipped_rafel++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] rafel_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (line && strstr(line, "wsgbuild")) {
      static unsigned skipped_wsgbuild = 0;
      if (skipped_wsgbuild++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] wsgbuild_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (line && strstr(line, "strpclub")) {
      static unsigned skipped_strpclub = 0;
      if (skipped_strpclub++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] strpclub_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (line && strstr(line, "ticketbooth")) {
      static unsigned skipped_ticketbooth = 0;
      if (skipped_ticketbooth++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] ticketbooth_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    {
      char pooladder_check_name[128] = {0};
      if (sscanf(line, " %*s %127s", pooladder_check_name) == 1 &&
          !strcmp(pooladder_check_name, "pooladder")) {
        static unsigned skipped_pooladder = 0;
        if (skipped_pooladder++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] pooladder_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char divingboard_check_name[128] = {0};
      if (sscanf(line, " %*s %127s", divingboard_check_name) == 1 &&
          !strcmp(divingboard_check_name, "divingboard")) {
        static unsigned skipped_divingboard = 0;
        if (skipped_divingboard++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] divingboard_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char washints_exact_name[128] = {0};
      if (sscanf(line, " %*s %127s", washints_exact_name) == 1 &&
          (!strcmp(washints_exact_name, "scarinterblood1") ||
           !strcmp(washints_exact_name, "poolwaterwshs") ||
           !strcmp(washints_exact_name, "pooladder01") ||
           !strcmp(washints_exact_name, "scarbuildglas"))) {
        static unsigned skipped_washints_exact = 0;
        if (skipped_washints_exact++ < 96) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] washints_exact '%s' line=%s\n", washints_exact_name, line); fclose(f); }
        }
        return;
      }
    }
    {
      char b_hse_ext_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, b_hse_ext_check_name) == 2 &&
          !strcmp(b_hse_ext_check_name, "b_hse_ext")) {
        static unsigned skipped_b_hse_ext = 0;
        if (skipped_b_hse_ext++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] b_hse_ext_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char b_hse_pier_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, b_hse_pier_check_name) == 2 &&
          !strcmp(b_hse_pier_check_name, "b_hse_pier")) {
        static unsigned skipped_b_hse_pier = 0;
        if (skipped_b_hse_pier++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] b_hse_pier_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char boat_kb1_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, boat_kb1_check_name) == 2 &&
          !strcmp(boat_kb1_check_name, "boat_kb1")) {
        static unsigned skipped_boat_kb1 = 0;
        if (skipped_boat_kb1++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] boat_kb1_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char boat_kb2_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, boat_kb2_check_name) == 2 &&
          !strcmp(boat_kb2_check_name, "boat_kb2")) {
        static unsigned skipped_boat_kb2 = 0;
        if (skipped_boat_kb2++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] boat_kb2_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char b_hse_interior_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, b_hse_interior_check_name) == 2 &&
          !strcmp(b_hse_interior_check_name, "b_hse_interior")) {
        static unsigned skipped_b_hse_interior = 0;
        if (skipped_b_hse_interior++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] b_hse_interior_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks28_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks28_check_name) == 2 &&
          !strcmp(docks28_check_name, "docks28")) {
        static unsigned skipped_docks28 = 0;
        if (skipped_docks28++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks28_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks32_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks32_check_name) == 2 &&
          !strcmp(docks32_check_name, "docks32")) {
        static unsigned skipped_docks32 = 0;
        if (skipped_docks32++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks32_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_rail02_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_rail02_check_name) == 2 &&
          !strcmp(dk_rail02_check_name, "dk_rail02")) {
        static unsigned skipped_dk_rail02 = 0;
        if (skipped_dk_rail02++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_rail02_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_rail03_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_rail03_check_name) == 2 &&
          !strcmp(dk_rail03_check_name, "dk_rail03")) {
        static unsigned skipped_dk_rail03 = 0;
        if (skipped_dk_rail03++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_rail03_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp65_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp65_check_name) == 2 &&
          !strcmp(dk_cargoshp65_check_name, "dk_cargoshp65")) {
        static unsigned skipped_dk_cargoshp65 = 0;
        if (skipped_dk_cargoshp65++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp65_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp01_check_name) == 2 &&
          !strcmp(dk_cargoshp01_check_name, "dk_cargoshp01")) {
        static unsigned skipped_dk_cargoshp01 = 0;
        if (skipped_dk_cargoshp01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_cargoshp68_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_cargoshp68_check_name) == 2 &&
          !strcmp(dk_cargoshp68_check_name, "dk_cargoshp68")) {
        static unsigned skipped_dk_cargoshp68 = 0;
        if (skipped_dk_cargoshp68++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_cargoshp68_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dock_props01_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dock_props01_check_name) == 2 &&
          !strcmp(dock_props01_check_name, "dock_props01")) {
        static unsigned skipped_dock_props01 = 0;
        if (skipped_dock_props01++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dock_props01_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dkglue3_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dkglue3_check_name) == 2 &&
          !strcmp(dkglue3_check_name, "dkglue3")) {
        static unsigned skipped_dkglue3 = 0;
        if (skipped_dkglue3++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dkglue3_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dkfirtpass2_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dkfirtpass2_check_name) == 2 &&
          !strcmp(dkfirtpass2_check_name, "dkfirtpass2")) {
        static unsigned skipped_dkfirtpass2 = 0;
        if (skipped_dkfirtpass2++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dkfirtpass2_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char rustship_structure0_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, rustship_structure0_check_name) == 2 &&
          !strcmp(rustship_structure0_check_name, "rustship_structure0")) {
        static unsigned skipped_rustship_structure0 = 0;
        if (skipped_rustship_structure0++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] rustship_structure0_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_waretankdoor1_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_waretankdoor1_check_name) == 2 &&
          !strcmp(dk_waretankdoor1_check_name, "dk_waretankdoor1")) {
        static unsigned skipped_dk_waretankdoor1 = 0;
        if (skipped_dk_waretankdoor1++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_waretankdoor1_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docksprops12_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docksprops12_check_name) == 2 &&
          !strcmp(docksprops12_check_name, "docksprops12")) {
        static unsigned skipped_docksprops12 = 0;
        if (skipped_docksprops12++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docksprops12_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docksprops13_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docksprops13_check_name) == 2 &&
          !strcmp(docksprops13_check_name, "docksprops13")) {
        static unsigned skipped_docksprops13 = 0;
        if (skipped_docksprops13++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docksprops13_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_shipstair_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_shipstair_check_name) == 2 &&
          !strcmp(dk_shipstair_check_name, "dk_shipstair")) {
        static unsigned skipped_dk_shipstair = 0;
        if (skipped_dk_shipstair++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_shipstair_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docksprops14_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docksprops14_check_name) == 2 &&
          !strcmp(docksprops14_check_name, "docksprops14")) {
        static unsigned skipped_docksprops14 = 0;
        if (skipped_docksprops14++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docksprops14_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_rail04_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_rail04_check_name) == 2 &&
          !strcmp(dk_rail04_check_name, "dk_rail04")) {
        static unsigned skipped_dk_rail04 = 0;
        if (skipped_dk_rail04++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_rail04_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_rail05_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_rail05_check_name) == 2 &&
          !strcmp(dk_rail05_check_name, "dk_rail05")) {
        static unsigned skipped_dk_rail05 = 0;
        if (skipped_dk_rail05++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_rail05_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_rail06_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_rail06_check_name) == 2 &&
          !strcmp(dk_rail06_check_name, "dk_rail06")) {
        static unsigned skipped_dk_rail06 = 0;
        if (skipped_dk_rail06++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_rail06_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_rail07_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_rail07_check_name) == 2 &&
          !strcmp(dk_rail07_check_name, "dk_rail07")) {
        static unsigned skipped_dk_rail07 = 0;
        if (skipped_dk_rail07++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_rail07_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_paynspray_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_paynspray_check_name) == 2 &&
          !strcmp(dk_paynspray_check_name, "dk_paynspray")) {
        static unsigned skipped_dk_paynspray = 0;
        if (skipped_dk_paynspray++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_paynspray_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_paynspraydoor_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_paynspraydoor_check_name) == 2 &&
          !strcmp(dk_paynspraydoor_check_name, "dk_paynspraydoor")) {
        static unsigned skipped_dk_paynspraydoor = 0;
        if (skipped_dk_paynspraydoor++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_paynspraydoor_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char b_hse_pierfence_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, b_hse_pierfence_check_name) == 2 &&
          !strcmp(b_hse_pierfence_check_name, "b_hse_pierfence")) {
        static unsigned skipped_b_hse_pierfence = 0;
        if (skipped_b_hse_pierfence++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] b_hse_pierfence_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docksprops15_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docksprops15_check_name) == 2 &&
          !strcmp(docksprops15_check_name, "docksprops15")) {
        static unsigned skipped_docksprops15 = 0;
        if (skipped_docksprops15++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docksprops15_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks61_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks61_check_name) == 2 &&
          !strcmp(docks61_check_name, "docks61")) {
        static unsigned skipped_docks61 = 0;
        if (skipped_docks61++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks61_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks60_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks60_check_name) == 2 &&
          !strcmp(docks60_check_name, "docks60")) {
        static unsigned skipped_docks60 = 0;
        if (skipped_docks60++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks60_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks62_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks62_check_name) == 2 &&
          !strcmp(docks62_check_name, "docks62")) {
        static unsigned skipped_docks62 = 0;
        if (skipped_docks62++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks62_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_bombdoor_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_bombdoor_check_name) == 2 &&
          !strcmp(dk_bombdoor_check_name, "dk_bombdoor")) {
        static unsigned skipped_dk_bombdoor = 0;
        if (skipped_dk_bombdoor++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_bombdoor_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dock_props02_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dock_props02_check_name) == 2 &&
          !strcmp(dock_props02_check_name, "dock_props02")) {
        static unsigned skipped_dock_props02 = 0;
        if (skipped_dock_props02++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dock_props02_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char docks10rail_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, docks10rail_check_name) == 2 &&
          !strcmp(docks10rail_check_name, "docks10rail")) {
        static unsigned skipped_docks10rail = 0;
        if (skipped_docks10rail++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] docks10rail_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dkcargohull2b_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dkcargohull2b_check_name) == 2 &&
          !strcmp(dkcargohull2b_check_name, "dkcargohull2b")) {
        static unsigned skipped_dkcargohull2b = 0;
        if (skipped_dkcargohull2b++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dkcargohull2b_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dkcargohull2c_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dkcargohull2c_check_name) == 2 &&
          !strcmp(dkcargohull2c_check_name, "dkcargohull2c")) {
        static unsigned skipped_dkcargohull2c = 0;
        if (skipped_dkcargohull2c++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dkcargohull2c_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char sub_floodlite_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, sub_floodlite_check_name) == 2 &&
          !strcmp(sub_floodlite_check_name, "sub_floodlite")) {
        static unsigned skipped_sub_floodlite = 0;
        if (skipped_sub_floodlite++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] sub_floodlite_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_rdsignsn_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_rdsignsn_check_name) == 2 &&
          !strcmp(dk_rdsignsn_check_name, "dk_rdsignsn")) {
        static unsigned skipped_dk_rdsignsn = 0;
        if (skipped_dk_rdsignsn++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_rdsignsn_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dk_rdsignsbr_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dk_rdsignsbr_check_name) == 2 &&
          !strcmp(dk_rdsignsbr_check_name, "dk_rdsignsbr")) {
        static unsigned skipped_dk_rdsignsbr = 0;
        if (skipped_dk_rdsignsbr++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dk_rdsignsbr_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char nt_aircon1dbl_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, nt_aircon1dbl_check_name) == 2 &&
          !strcmp(nt_aircon1dbl_check_name, "nt_aircon1dbl")) {
        static unsigned skipped_nt_aircon1dbl = 0;
        if (skipped_nt_aircon1dbl++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] nt_aircon1dbl_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char dockgrass_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, dockgrass_check_name) == 2 &&
          !strcmp(dockgrass_check_name, "dockgrass")) {
        static unsigned skipped_dockgrass = 0;
        if (skipped_dockgrass++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] dockgrass_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    {
      char b_hse_interiorrays_check_name[128] = {0};
      if (sscanf(line, " %127s %127s", (char[128]){0}, b_hse_interiorrays_check_name) == 2 &&
          !strcmp(b_hse_interiorrays_check_name, "b_hse_interiorrays")) {
        static unsigned skipped_b_hse_interiorrays = 0;
        if (skipped_b_hse_interiorrays++ < 64) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] b_hse_interiorrays_exact line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    if (strstr(line, "ammu")) {
      static unsigned skipped_ammu = 0;
      if (skipped_ammu++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] ammu line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "hogans")) {
      static unsigned skipped_hogans = 0;
      if (skipped_hogans++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] hogans line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "recordingstudio")) {
      static unsigned skipped_recordingstudio = 0;
      if (skipped_recordingstudio++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] recordingstudio line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "instruments")) {
      static unsigned skipped_instruments = 0;
      if (skipped_instruments++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] instruments line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "compound_bits")) {
      static unsigned skipped_compound_bits = 0;
      if (skipped_compound_bits++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] compound_bits line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "compound_fence")) {
      static unsigned skipped_compound_fence = 0;
      if (skipped_compound_fence++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] compound_fence line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "gluetest")) {
      static unsigned skipped_gluetest = 0;
      if (skipped_gluetest++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] gluetest line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "xpolytrees")) {
      static unsigned skipped_xpolytrees = 0;
      if (skipped_xpolytrees++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] xpolytrees line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "dtn_")) {
      static unsigned skipped_dtn = 0;
      if (skipped_dtn++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dtn_ line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "dtoon")) {
      static unsigned skipped_dtoon = 0;
      if (skipped_dtoon++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dtoon line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "test_track")) {
      static unsigned skipped_test_track = 0;
      if (skipped_test_track++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] test_track line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "Mpostbox")) {
      static unsigned skipped_mpostbox = 0;
      if (skipped_mpostbox++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] Mpostbox line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "parkingmeterg")) {
      static unsigned skipped_parkingmeterg = 0;
      if (skipped_parkingmeterg++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] parkingmeterg line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "newstandnew")) {
      static unsigned skipped_newstandnew = 0;
      if (skipped_newstandnew++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] newstandnew line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "bollardlight")) {
      static unsigned skipped_bollardlight = 0;
      if (skipped_bollardlight++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] bollardlight line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "road_downtown")) {
      static unsigned skipped_road_downtown = 0;
      if (skipped_road_downtown++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] road_downtown line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "road_stadiuma")) {
      static unsigned skipped_road_stadiuma = 0;
      if (skipped_road_stadiuma++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] road_stadiuma line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "stad_")) {
      static unsigned skipped_stad = 0;
      if (skipped_stad++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] stad_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "dowbikershop")) {
      static unsigned skipped_dowbikershop = 0;
      if (skipped_dowbikershop++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dowbikershop line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "dow_")) {
      static unsigned skipped_dow_any = 0;
      if (skipped_dow_any++ < 96) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dow_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "recsho_")) {
      static unsigned skipped_recsho = 0;
      if (skipped_recsho++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] recsho_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "dwn_")) {
      static unsigned skipped_dwn = 0;
      if (skipped_dwn++ < 96) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dwn_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "new_inside")) {
      static unsigned skipped_new_inside = 0;
      if (skipped_new_inside++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] new_inside line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "dts_")) {
      static unsigned skipped_dts_any = 0;
      if (skipped_dts_any++ < 128) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] dts_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "glass")) {
      static unsigned skipped_glass_any = 0;
      if (skipped_glass_any++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] glass_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    if (strstr(line, "lamppost")) {
      static unsigned skipped_lamppost = 0;
      if (skipped_lamppost++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] lamppost_any line=%s\n", line); fclose(f); }
      }
      return;
    }
    char model_name[128] = {0};
    int model_id = -1;
    if (sscanf(line, " %127s %127s", (char[128]){0}, model_name) == 2) {
      if (!strncmp(model_name, "lh", 2)) {
        static unsigned skipped_lh_prefix = 0;
        if (skipped_lh_prefix++ < 128) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] lh-prefix line=%s\n", line); fclose(f); }
        }
        return;
      }
      if (!strncmp(model_name, "lod", 3)) {
        static unsigned skipped_lod_prefix_lower = 0;
        if (skipped_lod_prefix_lower++ < 128) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] lod-prefix-lower line=%s\n", line); fclose(f); }
        }
        return;
      }
      if (!strncmp(model_name, "cof", 3)) {
        static unsigned skipped_cof_prefix = 0;
        if (skipped_cof_prefix++ < 128) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] cof-prefix line=%s\n", line); fclose(f); }
        }
        return;
      }
      if (!strncmp(model_name, "LOD", 3)) {
        static unsigned skipped_lod_prefix = 0;
        if (skipped_lod_prefix++ < 96) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) { fprintf(f, "[ipl-skip] LOD-prefix line=%s\n", line); fclose(f); }
        }
        return;
      }
    }
    if (sscanf(line, " %127s %127s", (char[128]){0}, model_name) == 2 && get_model_info) {
      void *mi = get_model_info(model_name, &model_id);
      void *col = mi ? *(void **)((uint8_t *)mi + 0x3c) : NULL;
      if (!mi || !col) {
        static unsigned skipped = 0;
        if (skipped++ < 48) {
          FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
          if (f) {
            fprintf(f, "[ipl-skip] bad model '%s' id=%d mi=%p col=%p line=%s\n",
                    model_name, model_id, mi, col, line);
            fclose(f);
          }
        }
        return;
      }
    }
    if (strstr(line, "biker_bar_exterior") && get_model_info) {
      static unsigned trace_biker_bar = 0;
      if (trace_biker_bar++ < 8) {
        int fail_id = -1;
        int ctrl_id = -1;
        void *fail_mi = get_model_info("biker_bar_exterior", &fail_id);
        void *ctrl_mi = get_model_info("dwn_bikerbarfrnt", &ctrl_id);
        void *fail_col = fail_mi ? *(void **)((uint8_t *)fail_mi + 0x3c) : NULL;
        void *ctrl_col = ctrl_mi ? *(void **)((uint8_t *)ctrl_mi + 0x3c) : NULL;
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) {
          fprintf(f,
                  "[ipl-trace] fail='biker_bar_exterior' id=%d mi=%p col=%p ctrl='dwn_bikerbarfrnt' id=%d mi=%p col=%p line=%s\n",
                  fail_id, fail_mi, fail_col, ctrl_id, ctrl_mi, ctrl_col, line);
          if (fail_mi && ctrl_mi) {
            uint32_t *fm = (uint32_t *)fail_mi;
            uint32_t *cm = (uint32_t *)ctrl_mi;
            fprintf(f,
                    "[ipl-trace] biker_fail_mi=%08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                    fm[0], fm[1], fm[2], fm[3], fm[4], fm[5], fm[6], fm[7],
                    fm[8], fm[9], fm[10], fm[11], fm[12], fm[13], fm[14], fm[15]);
            fprintf(f,
                    "[ipl-trace] biker_ctrl_mi=%08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                    cm[0], cm[1], cm[2], cm[3], cm[4], cm[5], cm[6], cm[7],
                    cm[8], cm[9], cm[10], cm[11], cm[12], cm[13], cm[14], cm[15]);
          }
          fclose(f);
        }
      }
      static unsigned skip_biker_bar_exterior = 0;
      if (skip_biker_bar_exterior++ < 64) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) { fprintf(f, "[ipl-skip] biker_bar_exterior line=%s\n", line); fclose(f); }
      }
      return;
    }
  }
  if (strstr(line, "svehsebuild1")) {
    static unsigned skipped_svehsebuild1 = 0;
    if (skipped_svehsebuild1++ < 128) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-skip] svehsebuild1 line=%s\n", line); fclose(f); }
    }
    return;
  }
  if (strstr(line, "svehsebuild")) {
    static unsigned skipped_svehsebuild = 0;
    if (skipped_svehsebuild++ < 128) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-skip] svehsebuild_any line=%s\n", line); fclose(f); }
    }
    return;
  }
  if (strstr(line, "svehse")) {
    static unsigned skipped_svehse = 0;
    if (skipped_svehse++ < 128) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-skip] svehse_any line=%s\n", line); fclose(f); }
    }
    return;
  }
  if (strstr(line, "new_bush")) {
    static unsigned skipped_new_bush = 0;
    if (skipped_new_bush++ < 128) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-skip] new_bush_any line=%s\n", line); fclose(f); }
    }
    return;
  }
  if (strstr(line, "new_bushsm")) {
    static unsigned skipped_new_bushsm = 0;
    if (skipped_new_bushsm++ < 128) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-skip] new_bushsm line=%s\n", line); fclose(f); }
    }
    return;
  }
  if (strstr(line, "marina")) {
    static unsigned skipped_marina = 0;
    if (skipped_marina++ < 128) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-skip] marina_any line=%s\n", line); fclose(f); }
    }
    return;
  }
  if (strstr(line, "marina1")) {
    static unsigned skipped_marina1 = 0;
    if (skipped_marina1++ < 128) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-skip] marina1 line=%s\n", line); fclose(f); }
    }
    return;
  }
  if (strstr(line, "svegrgedoor")) {
    static unsigned skipped_svegrgedoor = 0;
    if (skipped_svegrgedoor++ < 128) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-skip] svegrgedoor line=%s\n", line); fclose(f); }
    }
    return;
  }
  if (line) {
    char broad_name[128] = {0};
    const char *broad_reason = NULL;
    if (sscanf(line, " %*s %127s", broad_name) == 1 &&
        vc_should_skip_broad_decor_model(broad_name, &broad_reason)) {
      static unsigned skipped_broad_decor = 0;
      if (skipped_broad_decor++ < 512) {
        FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (f) {
          fprintf(f, "[ipl-skip] broad-decor reason=%s model=%s line=%s\n",
                  broad_reason ? broad_reason : "unknown", broad_name, line);
          fclose(f);
        }
      }
      return;
    }
  }
  if (n <= 8 || (n % 1000) == 0) log_node_pressure("ipl-obj-before", line);
  if (line && strstr(line, "dts_offglassa") && get_model_info) {
    static unsigned trace_dts_call = 0;
    if (trace_dts_call++ < 12) {
      int fail_id = -1;
      int ctrl_id = -1;
      void *fail_mi = get_model_info("dts_offglassa", &fail_id);
      void *ctrl_mi = get_model_info("dts_offglassb", &ctrl_id);
      if (fail_mi && ctrl_mi) {
        memcpy(fail_mi, ctrl_mi, 128);
        FILE *pf = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (pf) {
          fprintf(pf,
                  "[ipl-patch] copied full modelinfo from dts_offglassb to dts_offglassa fail_id=%d ctrl_id=%d line=%s\n",
                  fail_id, ctrl_id, line);
          fclose(pf);
        }
      }
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-call] dts_offglassa entering orig line=%s\n", line); fclose(f); }
      vc_arm_ipl_watchdog(line, n);
      orig(line);
      vc_disarm_ipl_watchdog();
      f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) { fprintf(f, "[ipl-return] dts_offglassa returned from orig line=%s\n", line); fclose(f); }
      if (n <= 8 || (n % 1000) == 0) log_node_pressure("ipl-obj-after", line);
      return;
    }
  }
  int trace_bighotelgrnd = line && strstr(line, "bighotelgrnd");
  if (trace_bighotelgrnd) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[ipl-call] bighotelgrnd n=%lu line=%s\n", n, line); fclose(f); }
  }
  vc_arm_ipl_watchdog(line, n);
  orig(line);
  vc_disarm_ipl_watchdog();
  if (trace_bighotelgrnd) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[ipl-return] bighotelgrnd n=%lu line=%s\n", n, line); fclose(f); }
  }
  if (n <= 8 || (n % 1000) == 0) log_node_pressure("ipl-obj-after", line);
}

/* TextureDatabaseRuntime::Unload (0x293200): GetDatabase("menu") retorna NULL
 * (DB já desregistrado no destrutor do CMenuManager = double-unload) e Unload
 * não checa `this` -> ldr [NULL+24] crasha. Reimplementado em C COM null-check;
 * mantém a liberação real (RwTextureDestroy) p/ não vazar VRAM. */
void my_texdb_unload(void *thiz) {
  if (!thiz) return; /* <-- a correção: this NULL = nada a liberar */
  unsigned char *self = (unsigned char *)thiz;
  unsigned int count = *(unsigned int *)(self + 24);
  if (!count) return;
  unsigned char *base = *(unsigned char **)(self + 28);
  void (*RwTextureDestroy)(void *) =
      (void (*)(void *))((char *)text_base + 0x286b28 + 1); /* Thumb */
  unsigned int i = 0, off = 0;
  do {
    unsigned char *entry = base + off;
    i++;
    off += 22; /* sizeof(entry) = 0x16 */
    unsigned char flags = entry[10];
    if (!(flags & 0x04)) { /* lsls#29+bmi testa bit 0x04 */
      void *tex = *(void **)(entry + 18);
      if (tex) RwTextureDestroy(tex);
    }
    count = *(unsigned int *)(self + 24); /* recarrega (como o original) */
  } while (i < count);
}

void my_os_debugbreak(void) {
  void *ra = __builtin_return_address(0);
  unsigned long off = (unsigned long)ra - (unsigned long)text_base;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) { fprintf(f, ">>> OS_DebugBreak(HALT) chamado de .so+0x%lx — ignorado, continua\n", off); fclose(f); }
  /* retorna (sem hang infinito) */
}
void my_os_debugout(const char *s) {
  void *ra = __builtin_return_address(0);
  unsigned long off = (unsigned long)ra - (unsigned long)text_base;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) {
    if (off < text_size) fprintf(f, "[DBG ra=.so+0x%lx] %s\n", off, s ? s : "");
    else fprintf(f, "[DBG] %s\n", s ? s : "");
    fclose(f);
  }
}

/* intercepta syscall: o "HALT" do jogo chama syscall(exit_group) DIRETO
 * (bypassa abort/exit stubs). Bloqueia exit/exit_group p/ o jogo continuar. */
#include <sys/syscall.h>
long my_syscall(long n, long a, long b, long c, long d, long e, long f) {
  if (n == 248 /*exit_group*/ || n == 1 /*exit*/ || n == 252 /*exit_group alt*/) {
    FILE *ff = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (ff) { fprintf(ff, ">>> BLOQUEADO syscall exit (%ld) — jogo continua\n", n); fclose(ff); }
    return 0;
  }
  return syscall(n, a, b, c, d, e, f);
}

/* spoof de /proc p/ passar a whitelist de device (nvGetSystemCapabilities):
 * o jogo le /proc/config.gz (CONFIG_ARCH_TEGRA) e /proc/cpuinfo. Amlogic nao
 * e reconhecido. Redireciona p/ arquivos fake (device = Tegra, reconhecido). */
#include <fcntl.h>
#include <stdarg.h>
static void log_open(const char *what, const char *path, int ok) {
  if (!path || strncmp(path, "/proc", 5) == 0) return;
  FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
  if (f) { fprintf(f, "[%s] %s -> %s\n", what, path, ok ? "OK" : "FALHOU"); fclose(f); }
}
/* Resolve caminho CASE-INSENSITIVE: a extração do OBB tem casing inconsistente
 * (DATA existe mas data não, textures/FONTS mas o jogo pede Textures/Fonts).
 * Ext4 é case-sensitive -> arquivos "somem". Caminha componente a componente
 * sob /storage/roms/ports/gtavc casando por strcasecmp. Retorna 1 se resolveu. */
#include <dirent.h>
#include <sys/stat.h>
#define GTAVC_ROOT "/storage/roms/ports/gtavc"
static int resolve_ci(const char *path, char *out, size_t outsz) {
  /* só mexe em caminhos sob a raiz do jogo */
  size_t rootlen = strlen(GTAVC_ROOT);
  if (strncmp(path, GTAVC_ROOT, rootlen) != 0) return 0;
  if (access(path, F_OK) == 0) { /* já existe com o casing pedido */
    snprintf(out, outsz, "%s", path); return 1;
  }
  /* copia a raiz e itera os componentes restantes */
  char cur[1024];
  snprintf(cur, sizeof(cur), "%s", GTAVC_ROOT);
  const char *rest = path + rootlen;
  while (*rest == '/') rest++;
  char comp[256];
  while (*rest) {
    size_t i = 0;
    while (rest[i] && rest[i] != '/' && i < sizeof(comp) - 1) { comp[i] = rest[i]; i++; }
    comp[i] = 0;
    rest += i;
    while (*rest == '/') rest++;
    /* tenta casar 'comp' no diretório 'cur' (sem aliasing no snprintf) */
    char trial[1024];
    snprintf(trial, sizeof(trial), "%s/%s", cur, comp);
    if (access(trial, F_OK) == 0) {
      memcpy(cur, trial, sizeof(cur));
      continue;
    }
    DIR *d = opendir(cur);
    if (!d) return 0;
    struct dirent *e; int found = 0;
    while ((e = readdir(d))) {
      if (strcasecmp(e->d_name, comp) == 0) {
        snprintf(trial, sizeof(trial), "%s/%s", cur, e->d_name);
        memcpy(cur, trial, sizeof(cur));
        found = 1; break;
      }
    }
    closedir(d);
    if (!found) return 0;
  }
  snprintf(out, outsz, "%s", cur);
  return 1;
}

/* o jogo monta o caminho do OBB errado ("//main.11...obb"); redireciona p/ real */
static const char *fix_obb(const char *path) {
  if (path && strstr(path, "main.11.com.rockstargames.gtavc.obb")) {
    if (strstr(path, ".idx"))
      return "/storage/roms/ports/gtavc/main.11.com.rockstargames.gtavc.obb.idx";
    return "/storage/roms/ports/gtavc/main.11.com.rockstargames.gtavc.obb";
  }
  return path;
}
int my_open(const char *path, int flags, ...) {
  va_list ap; va_start(ap, flags); int m = va_arg(ap, int); va_end(ap);
  if (path && strcmp(path, "/proc/config.gz") == 0)
    return open("/storage/roms/ports/gtavc/fake_config.gz", flags, m);
  const char *rp = fix_obb(path);
  char ci[1024];
  if (access(rp, F_OK) != 0 && resolve_ci(rp, ci, sizeof(ci))) rp = ci;
  int fd = open(rp, flags, m);
  log_open("open", rp, fd >= 0);
  return fd;
}
FILE *my_fopen(const char *path, const char *mode) {
  if (path && strcmp(path, "/proc/cpuinfo") == 0)
    return fopen("/storage/roms/ports/gtavc/fake_cpuinfo", mode);
  const char *rp = fix_obb(path);
  char ci[1024];
  if (access(rp, F_OK) != 0 && resolve_ci(rp, ci, sizeof(ci))) rp = ci;
  if (rp && strstr(rp, "/AUDIO/") && strstr(rp, ".mp3")) {
    static unsigned n = 0;
    if (n++ < 32 || (n % 200) == 0) {
      FILE *lf = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (lf) {
        fprintf(lf, "[audio-guard] block mp3 fopen %s\n", rp);
        fclose(lf);
      }
    }
    return NULL;
  }
  FILE *f = fopen(rp, mode);
  log_open("fopen", rp, f != NULL);
  return f;
}

/* glGetString spoof: o jogo tem whitelist de GPU; "Mali-450" e' undefined->HALT.
 * Spoofa GL_RENDERER -> "Mali-400 MP" (mesma arch Utgard, reconhecida pelo jogo). */
#include <GLES2/gl2.h>
const GLubyte *my_glGetString(GLenum name) {
  if (name == GL_RENDERER) return (const GLubyte *)"Mali-400 MP";
  return glGetString(name); /* real (libGLESv2) p/ VENDOR/VERSION/EXTENSIONS */
}

/* DIAGNÓSTICO: loga os primeiros glViewport p/ saber que resolução o jogo assume */
void my_glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
  static int n = 0;
  if (n++ < 4) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[glViewport] x=%d y=%d w=%d h=%d\n", x, y, w, h); fclose(f); }
  }
  glViewport(x, y, w, h);
}
/* DIAGNÓSTICO render: loga clearColor e blendFunc (primeiras chamadas) */
void SOFTFP_ABI my_glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  static int n = 0;
  if (n++ < 6) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[glClearColor] %.2f %.2f %.2f %.2f\n", r, g, b, a); fclose(f); }
  }
  glClearColor(r, g, b, a);
}
void SOFTFP_ABI my_glClearDepthf(GLfloat d) {
  glClearDepthf(d);
}
void my_glClear(GLbitfield mask) {
  static int n = 0;
  static int *curr_level = NULL;
  static unsigned char *menu_startup_req = NULL;
  static unsigned char *menu_shutdown_req = NULL;
  int id = ++n;
  int level = -1;
  int menu_start = -1;
  int menu_shutdown = -1;
  int in_game = 0;
  if (!curr_level) {
    curr_level = (int *)so_find_addr_safe("_ZN5CGame9currLevelE");
    menu_startup_req = (unsigned char *)so_find_addr_safe("_ZN12CMenuManager27m_bStartUpFrontEndRequestedE");
    menu_shutdown_req = (unsigned char *)so_find_addr_safe("_ZN12CMenuManager28m_bShutDownFrontEndRequestedE");
  }
  if (curr_level) level = *curr_level;
  if (menu_startup_req) menu_start = *menu_startup_req;
  if (menu_shutdown_req) menu_shutdown = *menu_shutdown_req;
  in_game = (level >= 1 &&
             (!menu_startup_req || menu_start == 0) &&
             (!menu_shutdown_req || menu_shutdown == 0));
  GLint bound_fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound_fbo);
  GLbitfield actual_mask = mask;
  int color_skip = 0;
  if (in_game && bound_fbo == 0 && (actual_mask & GL_COLOR_BUFFER_BIT)) {
    actual_mask &= ~GL_COLOR_BUFFER_BIT;
    color_skip = 1;
  }
  static int color_skip_logged = 0;
  int should_log_skip = color_skip && (color_skip_logged++ < 80 || (color_skip_logged % 1000) == 0);
  if (id <= 32 || (id % 1000) == 0 || should_log_skip) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[glClear] n=%d swapFrame=%d level=%d menu=%d/%d inGame=%d mask=0x%x actual=0x%x fbo=%d shimFbo=%d\n",
              id, g_swap_frame, level, menu_start, menu_shutdown,
              in_game, mask, actual_mask, bound_fbo, g_current_framebuffer);
      fclose(f);
    }
  }
  if (actual_mask) glClear(actual_mask);
}
void SOFTFP_ABI my_glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
  glTexParameterf(target, pname, param);
}
void my_glBlendFunc(GLenum s, GLenum d) {
  static int n = 0;
  if (n++ < 8) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[glBlendFunc] src=0x%x dst=0x%x\n", s, d); fclose(f); }
  }
  glBlendFunc(s, d);
}
void my_glBindFramebuffer(GLenum target, GLuint framebuffer) {
  static int n = 0;
  static int *curr_level = NULL;
  static unsigned char *menu_startup_req = NULL;
  static unsigned char *menu_shutdown_req = NULL;
  GLuint requested = framebuffer;
  if (!curr_level) {
    curr_level = (int *)so_find_addr_safe("_ZN5CGame9currLevelE");
    menu_startup_req = (unsigned char *)so_find_addr_safe("_ZN12CMenuManager27m_bStartUpFrontEndRequestedE");
    menu_shutdown_req = (unsigned char *)so_find_addr_safe("_ZN12CMenuManager28m_bShutDownFrontEndRequestedE");
  }
  if (target == GL_FRAMEBUFFER && framebuffer != 0 &&
      curr_level && *curr_level >= 1 &&
      (!menu_startup_req || *menu_startup_req == 0) &&
      (!menu_shutdown_req || *menu_shutdown_req == 0)) {
    framebuffer = 0;
  }
  g_current_framebuffer = (int)framebuffer;
  if (n++ < 24 || (n % 1000) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[glBindFramebuffer] n=%d target=0x%x fbo=%u req=%u\n",
              n, target, framebuffer, requested);
      fclose(f);
    }
  }
  glBindFramebuffer(target, framebuffer);
}
void my_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                               GLuint texture, GLint level) {
  static int n = 0;
  if (target == GL_FRAMEBUFFER && attachment == GL_COLOR_ATTACHMENT0)
    g_current_fbo_color_tex = texture;
  if (n++ < 24 || (n % 1000) == 0) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[glFramebufferTexture2D] n=%d target=0x%x att=0x%x textarget=0x%x tex=%u level=%d fbo=%d\n",
              n, target, attachment, textarget, texture, level, g_current_framebuffer);
      fclose(f);
    }
  }
  glFramebufferTexture2D(target, attachment, textarget, texture, level);
}
void my_glCompileShader(GLuint shader) {
  static int n = 0;
  glCompileShader(shader);
  if (n++ < 24 || (n % 1000) == 0) {
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[glCompileShader] n=%d shader=%u ok=%d\n", n, shader, ok);
      if (!ok || n <= 8) {
        char logbuf[2048];
        GLsizei out = 0;
        glGetShaderInfoLog(shader, (GLsizei)sizeof(logbuf), &out, logbuf);
        fprintf(f, "[glCompileShaderInfo] shader=%u len=%d %.*s\n",
                shader, (int)out, (int)out, logbuf);
      }
      fclose(f);
    }
  }
}
void my_glLinkProgram(GLuint program) {
  static int n = 0;
  glLinkProgram(program);
  if (n++ < 24 || (n % 1000) == 0) {
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[glLinkProgram] n=%d program=%u ok=%d\n", n, program, ok);
      if (!ok || n <= 8) {
        char logbuf[4096];
        GLsizei out = 0;
        glGetProgramInfoLog(program, (GLsizei)sizeof(logbuf), &out, logbuf);
        fprintf(f, "[glLinkProgramInfo] program=%u len=%d %.*s\n",
                program, (int)out, (int)out, logbuf);
      }
      fclose(f);
    }
  }
}
GLenum my_glCheckFramebufferStatus(GLenum target) {
  GLenum status = glCheckFramebufferStatus(target);
  static int n = 0;
  if (n++ < 32 || status != GL_FRAMEBUFFER_COMPLETE) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[glCheckFramebufferStatus] n=%d target=0x%x status=0x%x\n",
              n, target, status);
      fclose(f);
    }
  }
  return status;
}
void my_glUseProgram(GLuint program) {
  static int n = 0;
  if (n++ < 16) {
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) { fprintf(f, "[glUseProgram] program=%u\n", program); fclose(f); }
  }
  glUseProgram(program);
}
void my_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  static int n = 0;
  static int *curr_level = NULL;
  static unsigned char *menu_startup_req = NULL;
  static unsigned char *menu_shutdown_req = NULL;
  int id = ++n;
  int level = -1;
  int menu_start = -1;
  int menu_shutdown = -1;
  int in_game = 0;
  g_gl_draw_arrays_frame++;
  if (!curr_level) {
    curr_level = (int *)so_find_addr_safe("_ZN5CGame9currLevelE");
    menu_startup_req = (unsigned char *)so_find_addr_safe("_ZN12CMenuManager27m_bStartUpFrontEndRequestedE");
    menu_shutdown_req = (unsigned char *)so_find_addr_safe("_ZN12CMenuManager28m_bShutDownFrontEndRequestedE");
  }
  if (curr_level) level = *curr_level;
  if (menu_startup_req) menu_start = *menu_startup_req;
  if (menu_shutdown_req) menu_shutdown = *menu_shutdown_req;
  in_game = (level >= 1 &&
             (!menu_startup_req || menu_start == 0) &&
             (!menu_shutdown_req || menu_shutdown == 0));
  if (id <= 24 || (id % 1000) == 0 || (in_game && g_game_world_draw_seen)) {
    GLint bound_fbo = 0;
    GLint viewport[4] = {0, 0, 0, 0};
    GLint program = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound_fbo);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    if (in_game && g_game_world_draw_seen && bound_fbo == 0 &&
        viewport[2] == 1024 && viewport[3] == 600 &&
        (program == 1 || program == 4 || program == 7)) {
      static int skip_logged = 0;
      if (skip_logged++ < 80 || (skip_logged % 1000) == 0) {
        FILE *sf = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (sf) {
          fprintf(sf, "[glDrawArrays-skip-overlay] n=%d swapFrame=%d prog=%d mode=0x%x count=%d\n",
                  id, g_swap_frame, program, mode, count);
          fclose(sf);
        }
      }
      return;
    }
    if (id <= 24 || (id % 1000) == 0) {
      FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
      if (f) {
        fprintf(f, "[glDrawArrays] n=%d swapFrame=%d level=%d menu=%d/%d inGame=%d mode=0x%x first=%d count=%d fbo=%d shimFbo=%d vp=%d,%d,%d,%d prog=%d errBefore=0x%x\n",
                id, g_swap_frame, level, menu_start, menu_shutdown,
                in_game, mode, first, count, bound_fbo, g_current_framebuffer,
                viewport[0], viewport[1], viewport[2], viewport[3],
                program, glGetError());
        fclose(f);
      }
    }
  }
  glDrawArrays(mode, first, count);
}
void my_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
  static int n = 0;
  static int gameplay_logged = 0;
  static int *curr_level = NULL;
  static unsigned char *menu_startup_req = NULL;
  static unsigned char *menu_shutdown_req = NULL;
  int id = ++n;
  int level = -1;
  int menu_start = -1;
  int menu_shutdown = -1;
  int in_game = 0;
  g_gl_draw_elements_frame++;
  if (!curr_level) {
    curr_level = (int *)so_find_addr_safe("_ZN5CGame9currLevelE");
    menu_startup_req = (unsigned char *)so_find_addr_safe("_ZN12CMenuManager27m_bStartUpFrontEndRequestedE");
    menu_shutdown_req = (unsigned char *)so_find_addr_safe("_ZN12CMenuManager28m_bShutDownFrontEndRequestedE");
  }
  if (curr_level) level = *curr_level;
  if (menu_startup_req) menu_start = *menu_startup_req;
  if (menu_shutdown_req) menu_shutdown = *menu_shutdown_req;
  in_game = (level >= 1 &&
             (!menu_startup_req || menu_start == 0) &&
             (!menu_shutdown_req || menu_shutdown == 0));
  int should_log = (id <= 64 || (id % 1000) == 0 ||
                    (in_game && gameplay_logged < 160));
  if (should_log) {
    GLint bound_fbo = 0;
    GLint viewport[4] = {0, 0, 0, 0};
    GLint program = 0;
    GLint array_buffer = 0;
    GLint element_buffer = 0;
    GLboolean color_mask[4] = {0, 0, 0, 0};
    GLboolean depth_mask = 0;
    GLboolean depth_enabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend_enabled = glIsEnabled(GL_BLEND);
    GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
    unsigned char px_before[4] = {0, 0, 0, 0};
    unsigned char px_after[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound_fbo);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_buffer);
    glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
    if (in_game && bound_fbo == 0 && viewport[2] == 615 && viewport[3] == 360 &&
        program != 10) {
      g_game_world_draw_seen = 1;
    }
    if (in_game && g_game_world_draw_seen && bound_fbo == 0 &&
        viewport[2] == 1024 && viewport[3] == 600 && program == 10) {
      static int skip_logged = 0;
      if (skip_logged++ < 32 || (skip_logged % 1000) == 0) {
        FILE *sf = fopen("/storage/roms/ports/gtavc/debug.log", "a");
        if (sf) {
          fprintf(sf, "[glDrawElements-skip-overlay] n=%d swapFrame=%d prog=%d mode=0x%x count=%d\n",
                  id, g_swap_frame, program, mode, count);
          fclose(sf);
        }
      }
      return;
    }
    if (viewport[2] > 0 && viewport[3] > 0) {
      glReadPixels(viewport[0] + viewport[2] / 2, viewport[1] + viewport[3] / 2,
                   1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px_before);
    }
    GLenum err_before = glGetError();
    glDrawElements(mode, count, type, indices);
    GLenum err_after = glGetError();
    if (viewport[2] > 0 && viewport[3] > 0) {
      glReadPixels(viewport[0] + viewport[2] / 2, viewport[1] + viewport[3] / 2,
                   1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px_after);
    }
    if (in_game && bound_fbo == 0 && viewport[2] == 615 && viewport[3] == 360 &&
        program != 10 && ((int)px_after[0] + (int)px_after[1] + (int)px_after[2]) > 24) {
      vc_update_game_backbuffer_copy_from_viewport(viewport);
    }
    FILE *f = fopen("/storage/roms/ports/gtavc/debug.log", "a");
    if (f) {
      fprintf(f, "[glDrawElements] n=%d gameplayLog=%d swapFrame=%d level=%d menu=%d/%d inGame=%d mode=0x%x count=%d type=0x%x indices=%p fbo=%d shimFbo=%d vp=%d,%d,%d,%d prog=%d ab=%d eb=%d cm=%d%d%d%d dm=%d depth=%d blend=%d cull=%d px=%u,%u,%u,%u->%u,%u,%u,%u err=0x%x/0x%x\n",
              id, gameplay_logged, g_swap_frame, level, menu_start, menu_shutdown,
              in_game, mode, count, type, indices, bound_fbo, g_current_framebuffer,
              viewport[0], viewport[1], viewport[2], viewport[3],
              program, array_buffer, element_buffer,
              color_mask[0], color_mask[1], color_mask[2], color_mask[3],
              depth_mask, depth_enabled, blend_enabled, cull_enabled,
              px_before[0], px_before[1], px_before[2], px_before[3],
              px_after[0], px_after[1], px_after[2], px_after[3],
              err_before, err_after);
      fclose(f);
    }
    if (in_game) gameplay_logged++;
    return;
  }
  glDrawElements(mode, count, type, indices);
}
/* DIAGNÓSTICO: dumpa cada shader p/ arquivo (shader_N.glsl) p/ inspecionar
 * precision/lógica (suspeita: highp no fragment = quebra no Mali Utgard). */
void my_glShaderSource(GLuint sh, GLsizei count, const GLchar *const *str, const GLint *len) {
  static int n = 0;
  char path[128];
  snprintf(path, sizeof(path), "/storage/roms/ports/gtavc/shader_%d.glsl", n++);
  FILE *f = fopen(path, "w");
  if (f) {
    for (int i = 0; i < count; i++) {
      if (len && len[i] > 0) fwrite(str[i], 1, len[i], f);
      else fputs(str[i], f);
    }
    fclose(f);
  }
  glShaderSource(sh, count, str, len);
}

float SOFTFP_ABI vc_acosf(float x) { return acosf(x); }
float SOFTFP_ABI vc_asinf(float x) { return asinf(x); }
float SOFTFP_ABI vc_atanf(float x) { return atanf(x); }
float SOFTFP_ABI vc_atan2f(float y, float x) { return atan2f(y, x); }
float SOFTFP_ABI vc_ceilf(float x) { return ceilf(x); }
float SOFTFP_ABI vc_cosf(float x) { return cosf(x); }
float SOFTFP_ABI vc_floorf(float x) { return floorf(x); }
float SOFTFP_ABI vc_fmodf(float x, float y) { return fmodf(x, y); }
float SOFTFP_ABI vc_log10f(float x) { return log10f(x); }
float SOFTFP_ABI vc_powf(float x, float y) { return powf(x, y); }
float SOFTFP_ABI vc_sinf(float x) { return sinf(x); }
float SOFTFP_ABI vc_sqrtf(float x) { return sqrtf(x); }
float SOFTFP_ABI vc_tanf(float x) { return tanf(x); }

/* sigaction stub — impede o jogo de instalar handler proprio (mascara crash) */
int sigaction_stub(int sig, const void *act, void *old) { (void)sig; (void)act; (void)old; return 0; }
void *signal_stub(int sig, void *h) { (void)sig; (void)h; return (void *)0; }

/* ---------- Immersion haptics: stubs no-op (VIBE_S_SUCCESS = 0) ---------- */
int ImmVibeInitialize2(int a, int b) { (void)a; (void)b; return 0; }
int ImmVibeTerminate(void) { return 0; }
int ImmVibeOpenDevice(int dev, void *out) { (void)dev; if (out) *(int *)out = 0; return 0; }
int ImmVibeCloseDevice(int h) { (void)h; return 0; }
int ImmVibeGetEffectState(int h, int idx, void *st) { (void)h; (void)idx; if (st) *(int *)st = 0; return 0; }
int ImmVibeGetIVTEffectIndexFromName(int h, const void *ivt, const char *name, void *idx) {
  (void)h; (void)ivt; (void)name; if (idx) *(int *)idx = -1; return 0;
}
int ImmVibePlayUHLEffect(int h, int effect, void *handle) { (void)h; (void)effect; (void)handle; return 0; }
int ImmVibeStopPlayingEffect(int h, int handle) { (void)h; (void)handle; return 0; }

/* ---------- __sF: stdio bionic. bionic usa stdin/out/err = &__sF[0/1/2].
 * Damos um array de FILE* que aponta pros streams da glibc. O jogo passa
 * &__sF[i] pra fprintf/fwrite (resolvidos pra glibc) — então __sF[i] PRECISA
 * ser um FILE válido da glibc. Truque: __sF é array de FILE cujos ponteiros
 * internos copiamos dos std streams no init (vc_impl_init).
 * Para a maioria dos jogos isso só serve a logging (fprintf(stderr,...)). ---------- */
/* Reservamos espaço >= sizeof(FILE) por slot (bionic FILE é menor que glibc;
 * usamos o tamanho da glibc p/ caber). O jogo faz &__sF[i]; mantemos i*sizeof(FILE). */
FILE __sF[3];
void vc_impl_init(void) {
  /* copia o conteúdo dos std streams da glibc pros slots __sF */
  __sF[0] = *stdin;
  __sF[1] = *stdout;
  __sF[2] = *stderr;
}

/* ---------- _ctype_: tabela ctype estilo BSD (bionic exporta _ctype_[1+256]).
 * Índice -1..255. Bits: _U _L _N _S _P _C _X _B. Geramos via classificação. ---------- */
#define _CTU 0x01  /* upper */
#define _CTL 0x02  /* lower */
#define _CTN 0x04  /* digit */
#define _CTS 0x08  /* space */
#define _CTP 0x10  /* punct */
#define _CTC 0x20  /* control */
#define _CTX 0x40  /* hex */
#define _CTB 0x80  /* blank/print-space */
/* tabela com 1 byte de guarda (EOF) + 256 entradas; _ctype_ aponta p/ &tab[1] */
static unsigned char _ctype_tab[1 + 256];
const unsigned char *_ctype_ = &_ctype_tab[1];
/* _toupper_tab_ (bionic): ponteiro p/ tabela short, _toupper_tab_[c]=toupper(c) */
static short _toupper_data[1 + 256];
const short *_toupper_tab_ = &_toupper_data[1];

__attribute__((constructor)) static void build_ctype(void) {
  for (int c = 0; c < 256; c++) {
    _toupper_data[1 + c] = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
  }
  _toupper_data[0] = 0;
  for (int c = 0; c < 256; c++) {
    unsigned char f = 0;
    if (c >= 'A' && c <= 'Z') f |= _CTU;
    if (c >= 'a' && c <= 'z') f |= _CTL;
    if (c >= '0' && c <= '9') f |= _CTN;
    if (c == ' ' || (c >= '\t' && c <= '\r')) f |= _CTS;
    if (c == ' ' || c == '\t') f |= _CTB;
    if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9')) f |= _CTX;
    if (c < 0x20 || c == 0x7f) f |= _CTC;
    if (c >= 0x21 && c <= 0x7e && !(c >= '0' && c <= '9') &&
        !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) f |= _CTP;
    _ctype_tab[1 + c] = f;
  }
  _ctype_tab[0] = 0; /* EOF */
}
