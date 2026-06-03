/*
 * main.c -- entry point for LSWTCS ARM64 Linux port
 *
 * Loads libTTapp.so, resolves imports, creates a fake Android/JNI
 * environment, and drives the TTActivity startup sequence.
 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "android_shim.h"
#include "egl_shim.h"
#include "error.h"
#include "imports.h"
#include "jni_shim.h"
#include "opensles_shim.h"
#include "so_util.h"
#include "util.h"

#define MEMORY_MB 256
#define SO_NAME "libTTapp.so"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

static pthread_t g_main_thread;

#define ACTIVITY_CLASS ((void *)0x42424242)

/* Paths matching Android asset pack layout */
#define INTERNAL_PATH "/data/user/0/com.wb.lego.tcs/files"

#define ASSET_PACK_AUDIO    INTERNAL_PATH "/assetpacks/asset_Audio/20202/20202/assets/Audio.dat"
#define ASSET_PACK_LEVELS   INTERNAL_PATH "/assetpacks/asset_Levels/20202/20202/assets/Levels.dat"
#define ASSET_PACK_OTHERS   INTERNAL_PATH "/assetpacks/asset_Others/20202/20202/assets/Others.dat"
#define ASSET_PACK_TEXTURES INTERNAL_PATH "/assetpacks/asset_Textures/20202/20202/assets/Textures.dat"

#define APK_VERSION_NAME "2.0.2.02"

/* OBB info from obb_info.xml */
#define OBB_INFO_MAIN_VERSION 2017
#define OBB_INFO_MAIN_SIZE 2
#define OBB_INFO_PATCH_VERSION 2017
#define OBB_INFO_PATCH_SIZE 0
#define OBB_INFO_FORCE_ETC1 1

/* TTActivity function pointers */
typedef void (*fn_void_env_cls)(void *, void *);
typedef void (*fn_void_env_cls_str)(void *, void *, void *);
typedef void (*fn_void_env_cls_int)(void *, void *, int);
typedef void (*fn_void_env_cls_bool)(void *, void *, int);
typedef void (*fn_void_env_cls_obj)(void *, void *, void *);
typedef void (*fn_void_env_cls_6int_str_int)(void *, void *, int, int, int, int, void *, int);
typedef void (*fn_void_env_cls_objarray)(void *, void *, void *);
typedef void (*fn_void_env_cls_2float)(void *, void *, float, float);
typedef void (*fn_void_env_cls_int_int_2float)(void *, void *, int, int, float, float);
typedef void (*fn_void_env_cls_int_int)(void *, void *, int, int);
typedef void (*fn_void_env_cls_6float)(void *, void *, float, float, float, float, float, float);
typedef int (*fn_int_vm_ptr)(void *, void *);

static struct {
    fn_void_env_cls           nativeCacheJNIVars;
    fn_void_env_cls_str       nativeSetManufacturer;
    fn_void_env_cls_str       nativeSetModel;
    fn_void_env_cls_6int_str_int nativeSetObbInfo;
    fn_void_env_cls_objarray  nativeAddAssetsPath;
    fn_void_env_cls_int       nativeSetCaps;
    fn_void_env_cls_str       nativeSetPath;
    fn_void_env_cls_str       nativeSetLanguage;
    fn_void_env_cls_str       nativeSetAndroidVersion;
    fn_void_env_cls_obj       nativeSetAssetManager;
    fn_void_env_cls           nativeOnCreate;
    fn_void_env_cls           nativeOnStart;
    fn_void_env_cls           nativeOnResume;
    fn_void_env_cls_obj       nativeSetSurface;
    fn_void_env_cls_2float    nativeSetScreenDimesions;
    fn_void_env_cls_bool      nativeOnWindowFocusChanged;
    fn_void_env_cls           nativeOnPause;
    fn_void_env_cls           nativeOnStop;
    fn_void_env_cls_int       nativeOnKeyDown;
    fn_void_env_cls_int       nativeOnKeyUp;
    fn_void_env_cls_int_int_2float nativeOnTouchDown;
    fn_void_env_cls_int_int_2float nativeOnTouchMove;
    fn_void_env_cls_int_int   nativeOnTouchUp;
    fn_void_env_cls_6float    nativeUpdateGamepadAxisValues;
} activity;

/* Resolve a TTActivity symbol or die */
static void *must_resolve(const char *name) {
    uintptr_t addr = so_find_addr(name);
    if (!addr)
        fatal_error("Could not find %s in %s", name, SO_NAME);
    return (void *)addr;
}

static void tt_activity_init(void) {
#define RESOLVE(field, sym) activity.field = (typeof(activity.field))must_resolve("Java_com_tt_tech_TTActivity_" sym)
    RESOLVE(nativeCacheJNIVars,        "nativeCacheJNIVars");
    RESOLVE(nativeSetManufacturer,     "nativeSetManufacturer");
    RESOLVE(nativeSetModel,            "nativeSetModel");
    RESOLVE(nativeSetObbInfo,          "nativeSetObbInfo");
    RESOLVE(nativeAddAssetsPath,       "nativeAddAssetsPath");
    RESOLVE(nativeSetCaps,             "nativeSetCaps");
    RESOLVE(nativeSetPath,             "nativeSetPath");
    RESOLVE(nativeSetLanguage,         "nativeSetLanguage");
    RESOLVE(nativeSetAndroidVersion,   "nativeSetAndroidVersion");
    RESOLVE(nativeSetAssetManager,     "nativeSetAssetManager");
    RESOLVE(nativeOnCreate,            "nativeOnCreate");
    RESOLVE(nativeOnStart,             "nativeOnStart");
    RESOLVE(nativeOnResume,            "nativeOnResume");
    RESOLVE(nativeSetSurface,          "nativeSetSurface");
    RESOLVE(nativeSetScreenDimesions,  "nativeSetScreenDimesions");
    RESOLVE(nativeOnWindowFocusChanged,"nativeOnWindowFocusChanged");
    RESOLVE(nativeOnPause,             "nativeOnPause");
    RESOLVE(nativeOnStop,              "nativeOnStop");
    RESOLVE(nativeOnKeyDown,           "nativeOnKeyDown");
    RESOLVE(nativeOnKeyUp,             "nativeOnKeyUp");
    RESOLVE(nativeOnTouchDown,         "nativeOnTouchDown");
    RESOLVE(nativeOnTouchMove,         "nativeOnTouchMove");
    RESOLVE(nativeOnTouchUp,           "nativeOnTouchUp");
    RESOLVE(nativeUpdateGamepadAxisValues, "nativeUpdateGamepadAxisValues");
#undef RESOLVE

    debugPrintf("TTActivity: all 24 symbols resolved\n");
}

/* Fake jobjectArray for asset paths (game reads it via JNI GetObjectArrayElement) */
static void *g_asset_paths[4];
static struct {
    int length;
    void **elements;
} g_asset_array = { 4, g_asset_paths };

static void tt_activity_on_create(void) {
    void *env = &g_jni_env;

    /* Build fake asset paths array */
    g_asset_paths[0] = (void *)ASSET_PACK_AUDIO;
    g_asset_paths[1] = (void *)ASSET_PACK_LEVELS;
    g_asset_paths[2] = (void *)ASSET_PACK_OTHERS;
    g_asset_paths[3] = (void *)ASSET_PACK_TEXTURES;

    activity.nativeCacheJNIVars(env, ACTIVITY_CLASS);
    debugPrintf("TTActivity: nativeCacheJNIVars done\n");

    activity.nativeSetManufacturer(env, ACTIVITY_CLASS, (void *)"Trimui");
    activity.nativeSetModel(env, ACTIVITY_CLASS, (void *)"Smart Pro");

    activity.nativeSetObbInfo(env, ACTIVITY_CLASS,
        OBB_INFO_MAIN_VERSION, OBB_INFO_MAIN_SIZE,
        OBB_INFO_PATCH_VERSION, OBB_INFO_PATCH_SIZE,
        (void *)APK_VERSION_NAME, OBB_INFO_FORCE_ETC1);

    activity.nativeAddAssetsPath(env, ACTIVITY_CLASS, &g_asset_array);
    activity.nativeSetCaps(env, ACTIVITY_CLASS, 0);

    activity.nativeSetPath(env, ACTIVITY_CLASS, (void *)INTERNAL_PATH);
    activity.nativeSetLanguage(env, ACTIVITY_CLASS, (void *)"en");
    activity.nativeSetAndroidVersion(env, ACTIVITY_CLASS, (void *)"5.0.2");
    activity.nativeSetAssetManager(env, ACTIVITY_CLASS, (void *)0x24242424);

    activity.nativeOnCreate(env, ACTIVITY_CLASS);
    debugPrintf("TTActivity: nativeOnCreate done\n");
}

static void patch_nurenderdevice_initialize_stack_check(void) {
    uintptr_t init_addr = so_find_addr("_ZN14NuRenderDevice10InitializeEv");
    if (!init_addr) {
        debugPrintf("Patch: NuRenderDevice::Initialize not found, skipping canary workaround\n");
        return;
    }

    /* libTTapp.so+0x5cf130: `b.ne 0x5cf150` -> false-positive stack canary trip
       on this wrapper path. NOP it so the function can return normally. */
    uint32_t *branch = (uint32_t *)(init_addr + 0x404);
    uint32_t old = *branch;
    *branch = 0xd503201f; /* NOP */
    debugPrintf("Patch: disabled NuRenderDevice::Initialize stack-check branch at %p (old=0x%08x)\n",
                (void *)branch, old);
}

static void patch_nurenderdevice_initialise_openglcontext_stack_check(void) {
    uintptr_t initgl_addr =
        so_find_addr("_ZN14NuRenderDevice23InitialiseOpenGLContextEP13ANativeWindow");
    if (!initgl_addr) {
        debugPrintf("Patch: NuRenderDevice::InitialiseOpenGLContext not found, skipping canary workaround\n");
        return;
    }

    /* libTTapp.so+0x5cf9d4: `b.ne 0x5cf9f0` -> false-positive stack canary trip
       on this wrapper path. NOP it so the function can return normally.
       Symbol base is 0x5cf43c, so the branch sits at +0x598. */
    uint32_t *branch = (uint32_t *)(initgl_addr + 0x598);
    uint32_t old = *branch;
    *branch = 0xd503201f; /* NOP */
    debugPrintf("Patch: disabled NuRenderDevice::InitialiseOpenGLContext stack-check branch at %p (old=0x%08x)\n",
                (void *)branch, old);
}

static void patch_nupostfilter_initsharedresources_stack_check(void) {
    uint32_t *branch = (uint32_t *)((uintptr_t)text_base + 0x5ce1c4);
    uint32_t old = *branch;
    *branch = 0xd503201f; /* NOP */
    debugPrintf("Patch: disabled NuPostFilter::initSharedResources stack-check branch at %p (old=0x%08x)\n",
                (void *)branch, old);
}

static void patch_numtlinitex_stack_check(void) {
    uint32_t *branch = (uint32_t *)((uintptr_t)text_base + 0x5be37c);
    uint32_t old = *branch;
    *branch = 0xd503201f; /* NOP */
    debugPrintf("Patch: disabled NuMtlInitEx stack-check branch at %p (old=0x%08x)\n",
                (void *)branch, old);
}

static void patch_loaddefaulttexture_stack_check(void) {
    uint32_t *branch = (uint32_t *)((uintptr_t)text_base + 0x5c7bac);
    uint32_t old = *branch;
    *branch = 0xd503201f; /* NOP */
    debugPrintf("Patch: disabled loadDefaultTexture stack-check branch at %p (old=0x%08x)\n",
                (void *)branch, old);
}

static void patch_nuqfntreadbuffer_stack_check(void) {
    uint32_t *branch = (uint32_t *)((uintptr_t)text_base + 0x597eb8);
    uint32_t old = *branch;
    *branch = 0xd503201f; /* NOP */
    debugPrintf("Patch: disabled NuQFntReadBuffer stack-check branch at %p (old=0x%08x)\n",
                (void *)branch, old);
}

static void patch_nuinithardware_stack_check(void) {
    uint32_t *branch = (uint32_t *)((uintptr_t)text_base + 0x60310c);
    uint32_t old = *branch;
    *branch = 0xd503201f; /* NOP */
    debugPrintf("Patch: disabled NuInitHardware stack-check branch at %p (old=0x%08x)\n",
                (void *)branch, old);
}

static void patch_nuframeend_stack_check(void) {
    uint32_t *branch = (uint32_t *)((uintptr_t)text_base + 0x603ce0);
    uint32_t old = *branch;
    *branch = 0xd503201f; /* NOP */
    debugPrintf("Patch: disabled NuFrameEnd stack-check branch at %p (old=0x%08x)\n",
                (void *)branch, old);
}

static intptr_t sign_extend_bits(uint32_t value, int bits) {
    uint32_t sign_bit = 1u << (bits - 1);
    uint32_t mask = (1u << bits) - 1u;
    value &= mask;
    return (intptr_t)((value ^ sign_bit) - sign_bit);
}

static int decode_b_cond_target(uintptr_t pc, uint32_t insn, uintptr_t *target) {
    if ((insn & 0xff000010u) != 0x54000000u) {
        return 0;
    }

    intptr_t imm = sign_extend_bits((insn >> 5) & 0x7ffffu, 19) << 2;
    *target = pc + imm;
    return 1;
}

static int decode_cbz_target(uintptr_t pc, uint32_t insn, uintptr_t *target) {
    uint32_t op = insn & 0x7e000000u;
    if (op != 0x34000000u && op != 0x35000000u) {
        return 0;
    }

    intptr_t imm = sign_extend_bits((insn >> 5) & 0x7ffffu, 19) << 2;
    *target = pc + imm;
    return 1;
}

static uintptr_t decode_bl_target(uintptr_t pc, uint32_t insn) {
    intptr_t imm = sign_extend_bits(insn & 0x03ffffffu, 26) << 2;
    return pc + imm;
}

static void patch_all_stack_chk_branches(void) {
    if (!text_base || text_size < 4) {
        debugPrintf("Patch: text not ready, skipping global stack-check workaround\n");
        return;
    }

    uintptr_t fail_plt = (uintptr_t)text_base + 0x1c5c40;
    uint32_t *words = (uint32_t *)text_base;
    size_t count = text_size / sizeof(uint32_t);
    int patched = 0;
    int missed = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t insn = words[i];
        uintptr_t pc = (uintptr_t)&words[i];

        if ((insn & 0xfc000000u) != 0x94000000u) {
            continue;
        }
        if (decode_bl_target(pc, insn) != fail_plt) {
            continue;
        }

        int found = 0;
        for (size_t back = 1; back <= 16 && back <= i; back++) {
            uintptr_t branch_target = 0;
            uintptr_t branch_pc = (uintptr_t)&words[i - back];
            uint32_t branch_insn = words[i - back];

            if (!decode_b_cond_target(branch_pc, branch_insn, &branch_target) &&
                !decode_cbz_target(branch_pc, branch_insn, &branch_target)) {
                continue;
            }

            if (branch_target == pc) {
                words[i - back] = 0xd503201f; /* NOP */
                patched++;
                found = 1;
                break;
            }
        }

        if (!found) {
            missed++;
            if (missed <= 5) {
                debugPrintf("Patch: missed stack-check branch for call at libTTapp.so+0x%lx\n",
                            (unsigned long)(pc - (uintptr_t)text_base));
            }
        }
    }

    debugPrintf("Patch: disabled %d stack-check branches to __stack_chk_fail@plt (missed %d)\n",
                patched, missed);
}

static void patch_endcriticalsectiongl_force_release(void) {
    uintptr_t end_addr = so_find_addr("EndCriticalSectionGL");
    if (!end_addr) {
        debugPrintf("Patch: EndCriticalSectionGL not found, skipping forced GL release patch\n");
        return;
    }

    /* On this SDL/EGL stack we still need the outermost EndCriticalSectionGL
       to drop the current context, otherwise the render thread hits
       EGL_BAD_ACCESS when trying to acquire the window context. */
    uint32_t *branch = (uint32_t *)(end_addr + 0x28);
    uint32_t old = *branch;
    *branch = 0x1400000f; /* b +0x3c -> jump to 0x5d010c */
    debugPrintf("Patch: forced EndCriticalSectionGL to unbind EGL context at %p (old=0x%08x)\n",
                (void *)branch, old);
}

static void patch_gl_constant_setter_table(void) {
    uintptr_t table_addr = so_find_addr_safe("g_glConstantSetterTable");
    if (!table_addr) {
        debugPrintf("Patch: g_glConstantSetterTable not found, skipping uniform setter table patch\n");
        return;
    }

    uintptr_t *table = (uintptr_t *)table_addr;
    uintptr_t old0 = table[0];
    uintptr_t old1 = table[1];
    uintptr_t old2 = table[2];
    uintptr_t old3 = table[3];

    table[0] = (uintptr_t)&glUniform1fv_wrap;
    table[1] = (uintptr_t)&glUniform2fv_wrap;
    table[2] = (uintptr_t)&glUniform3fv_wrap;
    table[3] = (uintptr_t)&glUniform4fv_wrap;

    debugPrintf("Patch: patched g_glConstantSetterTable at %p\n",
                (void *)table);
    debugPrintf("Patch: uniform setters old=[%p %p %p %p] new=[%p %p %p %p]\n",
                (void *)old0, (void *)old1, (void *)old2, (void *)old3,
                (void *)table[0], (void *)table[1], (void *)table[2],
                (void *)table[3]);
}

static void patch_disable_touch_controls(void) {
    uintptr_t addr = so_find_addr_safe("enable_touch_controls");
    if (!addr) {
        debugPrintf("Patch: enable_touch_controls not found, skipping touch-control disable patch\n");
        return;
    }

    int *flag = (int *)addr;
    int old = *flag;
    *flag = 0;
    debugPrintf("Patch: disabled enable_touch_controls at %p (old=%d new=%d)\n",
                (void *)flag, old, *flag);
}

static void patch_skip_press_start_overlay(void) {
    /* The game has a global flag "isPressedStart" that controls the
       "Press the START button" overlay.  On a gamepad-only device the
       touch-based dismissal never fires.  Force the flag to 1. */
    uintptr_t addr = so_find_addr_safe("isPressedStart");
    if (!addr) {
        debugPrintf("Patch: isPressedStart not found, skipping\n");
        return;
    }
    uint8_t *flag = (uint8_t *)addr;
    int old = *flag;
    *flag = 1;
    debugPrintf("Patch: isPressedStart at %p -> 1 (old=%d)\n",
                (void *)flag, old);
}

static void patch_nupadupdatepads_skip_activation_gate(void) {
    uintptr_t addr = so_find_addr_safe("NuPadUpdatePads");
    if (!addr) {
        debugPrintf("Patch: NuPadUpdatePads not found, skipping activation-gate patch\n");
        return;
    }

    /* NuPadUpdatePads+0x294:
       tbz w9, #0, <not-yet-activated path>
       Skip this gate so the first frontend screen uses the same active-pad
       path as the rest of the menus. */
    uint32_t *branch = (uint32_t *)(addr + 0x294);
    uint32_t old = *branch;
    *branch = 0xd503201f; /* NOP */
    debugPrintf("Patch: disabled NuPadUpdatePads activation gate at %p (old=0x%08x)\n",
                (void *)branch, old);
}

typedef struct {
    int pad;
    int port;
    int is_active;
} NuPadMapping;

static NuPadMapping *g_nupad_mapping = NULL;
static int g_nupad_mapping_logged = 0;
static int g_pending_activation_press = 0;
static int g_pending_activation_release = 0;

static void patch_force_primary_gamepad_mapping(void) {
    uintptr_t addr = so_find_addr_safe("g_nupadMapping");
    if (!addr) {
        debugPrintf("Patch: g_nupadMapping not found, skipping gamepad mapping patch\n");
        return;
    }

    g_nupad_mapping = (NuPadMapping *)addr;
    g_nupad_mapping[0].pad = 0;
    g_nupad_mapping[0].port = 1;
    g_nupad_mapping[0].is_active = 1;

    debugPrintf("Patch: forced g_nupadMapping[0] at %p -> {pad=%d, port=%d, is_active=%d}\n",
                (void *)&g_nupad_mapping[0],
                g_nupad_mapping[0].pad,
                g_nupad_mapping[0].port,
                g_nupad_mapping[0].is_active);
}

static void maintain_primary_gamepad_mapping(void) {
    if (!g_nupad_mapping) {
        return;
    }

    if (g_nupad_mapping[0].pad != 0 ||
        g_nupad_mapping[0].port != 1 ||
        g_nupad_mapping[0].is_active != 1) {
        g_nupad_mapping[0].pad = 0;
        g_nupad_mapping[0].port = 1;
        g_nupad_mapping[0].is_active = 1;
        if (!g_nupad_mapping_logged) {
            /* debugPrintf("Input: restored g_nupadMapping[0] -> {pad=0, port=1, is_active=1}\n"); */
            g_nupad_mapping_logged = 1;
        }
    }
}

/* Crash handler — just dump and exit (no recovery, like Vita) */
static void crash_handler(int sig, siginfo_t *info, void *uctx) {
    ucontext_t *uc = (ucontext_t *)uctx;
    uintptr_t pc = uc->uc_mcontext.pc;
    uintptr_t fault_addr = (uintptr_t)info->si_addr;
    uintptr_t text = (uintptr_t)text_base;
    uintptr_t data = (uintptr_t)data_base;

    fprintf(stderr, "\n=== CRASH ===\n");
    fprintf(stderr, "Signal: %d (%s)\n", sig,
            sig == SIGSEGV ? "SIGSEGV" : sig == SIGBUS ? "SIGBUS" :
            sig == SIGABRT ? "SIGABRT" : sig == SIGFPE ? "SIGFPE" : "?");
    fprintf(stderr, "Fault addr: %p\n", (void *)fault_addr);
    fprintf(stderr, "PC:         %p\n", (void *)pc);

    if (pc >= text && pc < text + text_size)
        fprintf(stderr, "PC in .text: offset 0x%lx\n", (unsigned long)(pc - text));
    else if (pc >= data && pc < data + data_size)
        fprintf(stderr, "PC in .data: offset 0x%lx\n", (unsigned long)(pc - data));
    else
        fprintf(stderr, "PC outside libTTapp.so\n");

    fprintf(stderr, "\nRegisters:\n");
    for (int i = 0; i < 31; i++) {
        fprintf(stderr, "  x%-2d = 0x%016lx", i, (unsigned long)uc->uc_mcontext.regs[i]);
        if (i % 3 == 2 || i == 30)
            fprintf(stderr, "\n");
    }
    fprintf(stderr, "  sp  = 0x%016lx\n", (unsigned long)uc->uc_mcontext.sp);
    fprintf(stderr, "  pc  = 0x%016lx\n", (unsigned long)uc->uc_mcontext.pc);

    /* Backtrace */
    fprintf(stderr, "\nBacktrace:\n");
    fprintf(stderr, "  #0  pc %p", (void *)pc);
    if (pc >= text && pc < text + text_size)
        fprintf(stderr, " (libTTapp.so+0x%lx)", (unsigned long)(pc - text));
    fprintf(stderr, "\n");

    uintptr_t fp = uc->uc_mcontext.regs[29];
    for (int frame = 1; frame < 32 && fp; frame++) {
        uintptr_t *fp_ptr = (uintptr_t *)fp;
        uintptr_t next_fp = fp_ptr[0];
        uintptr_t lr = fp_ptr[1];
        if (!lr) break;
        fprintf(stderr, "  #%-2d lr %p", frame, (void *)lr);
        if (lr >= text && lr < text + text_size)
            fprintf(stderr, " (libTTapp.so+0x%lx)", (unsigned long)(lr - text));
        fprintf(stderr, "\n");
        if (next_fp <= fp) break;
        fp = next_fp;
    }

    fprintf(stderr, "\nso text_base=%p text_size=0x%zx\n", text_base, text_size);
    fprintf(stderr, "so data_base=%p data_size=0x%zx\n", data_base, data_size);
    fprintf(stderr, "Thread: %lx (main=%lx)\n",
            (unsigned long)pthread_self(), (unsigned long)g_main_thread);

    /* Show which library contains the crash PC */
    fprintf(stderr, "\nMemory maps (near PC):\n");
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[512];
        while (fgets(line, sizeof(line), maps)) {
            unsigned long start, end;
            if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                if (pc >= start && pc < end)
                    fprintf(stderr, ">>> %s", line);
            }
        }
        fclose(maps);
    }

    fprintf(stderr, "=== END CRASH ===\n");
    fflush(stderr);

    _exit(128 + sig);
}

static void install_crash_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
}

/* SDL gamepad button to Android keycode mapping */
typedef struct {
    SDL_GameControllerButton sdl_button;
    int android_keycode;
} ButtonMapping;

/* Android keycodes (from android/keycodes.h) */
#define AKEYCODE_DPAD_UP        19
#define AKEYCODE_DPAD_DOWN      20
#define AKEYCODE_DPAD_LEFT      21
#define AKEYCODE_DPAD_RIGHT     22
#define AKEYCODE_BACK           4
#define AKEYCODE_BUTTON_A       96
#define AKEYCODE_BUTTON_B       97
#define AKEYCODE_BUTTON_X       99
#define AKEYCODE_BUTTON_Y       100
#define AKEYCODE_BUTTON_L1      102
#define AKEYCODE_BUTTON_R1      103
#define AKEYCODE_BUTTON_L2      104
#define AKEYCODE_BUTTON_R2      105
#define AKEYCODE_BUTTON_THUMBL  106
#define AKEYCODE_BUTTON_THUMBR  107
#define AKEYCODE_BUTTON_START   108
#define AKEYCODE_BUTTON_SELECT  109

static ButtonMapping button_map[] = {
    /* Trimui uses Nintendo face-button layout, so swap A/B for Android gamepad semantics. */
    { SDL_CONTROLLER_BUTTON_A,             AKEYCODE_BUTTON_B },
    { SDL_CONTROLLER_BUTTON_B,             AKEYCODE_BUTTON_A },
    { SDL_CONTROLLER_BUTTON_X,             AKEYCODE_BUTTON_X },
    { SDL_CONTROLLER_BUTTON_Y,             AKEYCODE_BUTTON_Y },
    { SDL_CONTROLLER_BUTTON_BACK,          AKEYCODE_BACK },
    { SDL_CONTROLLER_BUTTON_START,         AKEYCODE_BUTTON_START },
    { SDL_CONTROLLER_BUTTON_LEFTSTICK,     AKEYCODE_BUTTON_THUMBL },
    { SDL_CONTROLLER_BUTTON_RIGHTSTICK,    AKEYCODE_BUTTON_THUMBR },
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  AKEYCODE_BUTTON_L1 },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, AKEYCODE_BUTTON_R1 },
    { SDL_CONTROLLER_BUTTON_DPAD_UP,       AKEYCODE_DPAD_UP },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN,     AKEYCODE_DPAD_DOWN },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT,     AKEYCODE_DPAD_LEFT },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT,    AKEYCODE_DPAD_RIGHT },
};

static SDL_GameController *g_controller = NULL;
static float g_last_hat_x = 0.0f;
static float g_last_hat_y = 0.0f;
static float g_last_lx = 0.0f;
static float g_last_ly = 0.0f;
static float g_last_rx = 0.0f;
static float g_last_ry = 0.0f;
static int g_last_l2_down = 0;
static int g_last_r2_down = 0;
static int g_request_quit = 0;
static uint8_t g_button_states[SDL_CONTROLLER_BUTTON_MAX];

#define STICK_DEADZONE   8000
#define TRIGGER_THRESHOLD 16000

static void send_native_key_down(void *env, int keycode, const char *source) {
    /* debugPrintf("JNI input: nativeOnKeyDown(%d) source=%s\n", keycode, source); */
    activity.nativeOnKeyDown(env, ACTIVITY_CLASS, keycode);
}

static void send_native_key_up(void *env, int keycode, const char *source) {
    /* debugPrintf("JNI input: nativeOnKeyUp(%d) source=%s\n", keycode, source); */
    activity.nativeOnKeyUp(env, ACTIVITY_CLASS, keycode);
}

static void send_native_touch_down(void *env, int pointer_id, float x, float y, const char *source) {
    /* debugPrintf("JNI input: nativeOnTouchDown(id=%d, x=%.1f, y=%.1f) source=%s\n",
                pointer_id, x, y, source); */
    activity.nativeOnTouchDown(env, ACTIVITY_CLASS, pointer_id, pointer_id, x, y);
}

static void send_native_touch_move(void *env, int pointer_id, float x, float y, const char *source) {
    /* debugPrintf("JNI input: nativeOnTouchMove(id=%d, x=%.1f, y=%.1f) source=%s\n",
                pointer_id, x, y, source); */
    activity.nativeOnTouchMove(env, ACTIVITY_CLASS, pointer_id, pointer_id, x, y);
}

static void send_native_touch_up(void *env, int pointer_id, const char *source) {
    /* debugPrintf("JNI input: nativeOnTouchUp(id=%d) source=%s\n", pointer_id, source); */
    activity.nativeOnTouchUp(env, ACTIVITY_CLASS, pointer_id, pointer_id);
}

static void send_native_gamepad_axes(void *env, float hat_x, float hat_y,
                                     float lx, float ly, float rx, float ry,
                                     const char *source) {
    /* debugPrintf("JNI input: nativeUpdateGamepadAxisValues hat(%.2f, %.2f) left(%.2f, %.2f) right(%.2f, %.2f) source=%s\n",
                hat_x, hat_y, lx, ly, rx, ry, source); */
    activity.nativeUpdateGamepadAxisValues(env, ACTIVITY_CLASS, hat_x, hat_y, lx, ly, rx, ry);
}

static void queue_gamepad_activation_pulse(void) {
    g_pending_activation_press = 1;
    g_pending_activation_release = 0;
}

static void pump_gamepad_activation_pulse(void) {
    if (!g_controller) {
        return;
    }

    void *env = &g_jni_env;

    if (g_pending_activation_press) {
        /* debugPrintf("Input: synthetic activation pulse DOWN\n"); */
        send_native_key_down(env, AKEYCODE_BUTTON_A, "activation_pulse");
        g_pending_activation_press = 0;
        g_pending_activation_release = 3;
        return;
    }

    if (g_pending_activation_release > 0) {
        g_pending_activation_release--;
        if (g_pending_activation_release == 0) {
            /* debugPrintf("Input: synthetic activation pulse UP\n"); */
            send_native_key_up(env, AKEYCODE_BUTTON_A, "activation_pulse");
        }
    }
}

static void open_controller(void) {
    int num_joysticks = SDL_NumJoysticks();
    /* debugPrintf("Input: SDL_NumJoysticks() = %d\n", num_joysticks); */

    for (int i = 0; i < num_joysticks; i++) {
        int is_controller = SDL_IsGameController(i);
        /* const char *joy_name = SDL_JoystickNameForIndex(i);
        debugPrintf("Input: joystick[%d] name=%s gamecontroller=%d\n",
                    i, joy_name ? joy_name : "(null)", is_controller); */
        if (is_controller) {
            g_controller = SDL_GameControllerOpen(i);
            if (g_controller) {
                memset(g_button_states, 0, sizeof(g_button_states));
                /* debugPrintf("Controller opened: %s\n", SDL_GameControllerName(g_controller)); */
                queue_gamepad_activation_pulse();
                return;
            } else {
                /* debugPrintf("Input: SDL_GameControllerOpen(%d) failed: %s\n",
                            i, SDL_GetError()); */
            }
        }
    }

    /* debugPrintf("Input: no SDL game controller opened\n"); */
}

static void poll_controller_buttons(void) {
    if (!g_controller) return;

    void *env = &g_jni_env;
    uint8_t guide_down = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_GUIDE) ? 1 : 0;

    if (guide_down != g_button_states[SDL_CONTROLLER_BUTTON_GUIDE]) {
        g_button_states[SDL_CONTROLLER_BUTTON_GUIDE] = guide_down;
        /* debugPrintf("Input: guide/menu button %s\n", guide_down ? "DOWN" : "UP"); */
        if (guide_down) {
            g_request_quit = 1;
        }
    }

    for (int i = 0; i < (int)(sizeof(button_map) / sizeof(button_map[0])); i++) {
        SDL_GameControllerButton button = button_map[i].sdl_button;
        uint8_t down = SDL_GameControllerGetButton(g_controller, button) ? 1 : 0;
        if (down == g_button_states[button])
            continue;

        g_button_states[button] = down;
        /* debugPrintf("Input: button %d -> keycode %d %s\n",
                    (int)button, button_map[i].android_keycode,
                    down ? "DOWN" : "UP"); */
        if (down)
            send_native_key_down(env, button_map[i].android_keycode, "controller_button");
        else
            send_native_key_up(env, button_map[i].android_keycode, "controller_button");
    }
}

static void poll_controller_axes(void) {
    if (!g_controller) return;

    void *env = &g_jni_env;

    int raw_lx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
    int raw_ly = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
    int raw_rx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
    int raw_ry = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);
    int raw_l2 = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    int raw_r2 = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

    float lx = (raw_lx > STICK_DEADZONE || raw_lx < -STICK_DEADZONE)
                   ? (float)raw_lx / 32767.0f
                   : 0.0f;
    float ly = (raw_ly > STICK_DEADZONE || raw_ly < -STICK_DEADZONE)
                   ? (float)raw_ly / 32767.0f
                   : 0.0f;
    float rx = (raw_rx > STICK_DEADZONE || raw_rx < -STICK_DEADZONE)
                   ? (float)raw_rx / 32767.0f
                   : 0.0f;
    float ry = (raw_ry > STICK_DEADZONE || raw_ry < -STICK_DEADZONE)
                   ? (float)raw_ry / 32767.0f
                   : 0.0f;

    /* Hat from D-pad (already handled as buttons, but report axis too) */
    int du = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
    int dd = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    int dl = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    int dr = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    float hatX = (float)(dr - dl);
    float hatY = (float)(dd - du);

    if (hatX != g_last_hat_x || hatY != g_last_hat_y ||
        lx != g_last_lx || ly != g_last_ly ||
        rx != g_last_rx || ry != g_last_ry) {
        /* debugPrintf("Input: axes hat(%.2f, %.2f) left(%.2f, %.2f) right(%.2f, %.2f)\n",
                    hatX, hatY, lx, ly, rx, ry); */
        send_native_gamepad_axes(env, hatX, hatY, lx, ly, rx, ry, "controller_axes");
        g_last_hat_x = hatX;
        g_last_hat_y = hatY;
        g_last_lx = lx;
        g_last_ly = ly;
        g_last_rx = rx;
        g_last_ry = ry;
    }

    int l2_down = raw_l2 > TRIGGER_THRESHOLD;
    int r2_down = raw_r2 > TRIGGER_THRESHOLD;

    if (l2_down != g_last_l2_down) {
        /* debugPrintf("Input: trigger L2 %s (raw=%d)\n",
                    l2_down ? "DOWN" : "UP", raw_l2); */
        if (l2_down)
            send_native_key_down(env, AKEYCODE_BUTTON_L2, "controller_l2");
        else
            send_native_key_up(env, AKEYCODE_BUTTON_L2, "controller_l2");
        g_last_l2_down = l2_down;
    }

    if (r2_down != g_last_r2_down) {
        /* debugPrintf("Input: trigger R2 %s (raw=%d)\n",
                    r2_down ? "DOWN" : "UP", raw_r2); */
        if (r2_down)
            send_native_key_down(env, AKEYCODE_BUTTON_R2, "controller_r2");
        else
            send_native_key_up(env, AKEYCODE_BUTTON_R2, "controller_r2");
        g_last_r2_down = r2_down;
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    g_main_thread = pthread_self();

    debugPrintf("=== LSWTCS ARM64 Linux Port (Trimui Smart Pro) ===\n");

    /* Set data path for AAssetManager (current directory by default) */
    android_shim_set_data_path(".");

    /* Allocate heap for the .so loader */
    size_t heap_size = MEMORY_MB * 1024 * 1024;
    void *heap = mmap(NULL, heap_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (heap == MAP_FAILED)
        fatal_error("Failed to allocate %d MB heap", MEMORY_MB);
    debugPrintf("Heap allocated: %p (%d MB)\n", heap, MEMORY_MB);

    /* Load the shared object */
    debugPrintf("Loading %s...\n", SO_NAME);
    if (so_load(SO_NAME, heap, heap_size) < 0)
        fatal_error("Failed to load %s", SO_NAME);
    debugPrintf("Loaded %s: text=%p+%zu data=%p+%zu\n",
                SO_NAME, text_base, text_size, data_base, data_size);

    /* Relocate */
    debugPrintf("Relocating...\n");
    if (so_relocate() < 0)
        fatal_error("Failed to relocate %s", SO_NAME);

    /* Resolve imports */
    debugPrintf("Resolving %zu imports...\n", dynlib_numfunctions);
    if (so_resolve(dynlib_functions, dynlib_numfunctions, 0) < 0)
        fatal_error("Failed to resolve imports");

    patch_nurenderdevice_initialize_stack_check();
    patch_nurenderdevice_initialise_openglcontext_stack_check();
    patch_nupostfilter_initsharedresources_stack_check();
    patch_numtlinitex_stack_check();
    patch_loaddefaulttexture_stack_check();
    patch_nuqfntreadbuffer_stack_check();
    patch_nuinithardware_stack_check();
    patch_nuframeend_stack_check();
    patch_all_stack_chk_branches();
    patch_endcriticalsectiongl_force_release();
    patch_gl_constant_setter_table();
    patch_disable_touch_controls();
    patch_skip_press_start_overlay();
    patch_nupadupdatepads_skip_activation_gate();
    patch_force_primary_gamepad_mapping();

    /* Finalize: make text read-only+exec, flush caches */
    so_finalize();
    so_flush_caches();

    /* Run .init_array constructors */
    debugPrintf("Running init array...\n");
    so_execute_init_array();

    /* Initialize SDL */
    debugPrintf("Initializing SDL...\n");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0)
        fatal_error("SDL_Init failed: %s", SDL_GetError());
    debugPrintf("SDL initialized OK\n");

    /* Create SDL window + GL context from main thread (before .so render thread starts) */
    egl_shim_create_window();

    /* Install crash handler AFTER SDL (SDL_Init installs its own signal handlers,
       we need ours to override them so we see real crash locations) */
    install_crash_handler();

    /* Dump memory maps for crash analysis */
    {
        FILE *maps = fopen("/proc/self/maps", "r");
        if (maps) {
            char line[512];
            debugPrintf("=== Memory maps (libraries) ===\n");
            while (fgets(line, sizeof(line), maps)) {
                /* Only show executable or interesting mappings */
                if (strstr(line, " r-xp ") || strstr(line, ".so"))
                    debugPrintf("  %s", line);
            }
            fclose(maps);
            debugPrintf("=== End maps ===\n");
        }
    }

    /* Initialize shims */
    jni_shim_init();
    debugPrintf("JNI shim initialized\n");

    /* Resolve all TTActivity function pointers */
    tt_activity_init();

    /* Call JNI_OnLoad */
    debugPrintf("Calling JNI_OnLoad...\n");
    fn_int_vm_ptr jni_onload = (fn_int_vm_ptr)so_find_addr("JNI_OnLoad");
    if (jni_onload) {
        int ver = jni_onload(&g_jni_vm, NULL);
        debugPrintf("JNI_OnLoad returned: 0x%x\n", ver);
    } else {
        debugPrintf("JNI_OnLoad not found, skipping\n");
    }

    /* Register gamepad as connected */
    debugPrintf("Registering gamepad...\n");
    {
        typedef void (*fn_gamepad)(void *, void *, int);
        fn_gamepad set_gamepad = (fn_gamepad)so_find_addr(
            "Java_com_tt_tech_CheckGamepadStatus_nativeSetGamePadConnected");
        if (set_gamepad) {
            set_gamepad(&g_jni_env, ACTIVITY_CLASS, 1);
            debugPrintf("Gamepad registered as connected\n");
        }
    }

    /* TTActivity startup sequence */
    debugPrintf("Starting TTActivity sequence...\n");
    tt_activity_on_create();

    void *env = &g_jni_env;

    activity.nativeOnStart(env, ACTIVITY_CLASS);
    debugPrintf("TTActivity: nativeOnStart done\n");

    activity.nativeOnResume(env, ACTIVITY_CLASS);
    debugPrintf("TTActivity: nativeOnResume done\n");

    /* Surface created/changed */
    activity.nativeSetSurface(env, ACTIVITY_CLASS, (void *)0x24242424);
    debugPrintf("TTActivity: nativeSetSurface done\n");

    /* Screen dimensions: physical size in mm for Trimui Smart Pro (4.96" @ 1280x720) */
    float width_mm = (1280.0f / 220.0f) * 25.4f;
    float height_mm = (720.0f / 220.0f) * 25.4f;
    activity.nativeSetScreenDimesions(env, ACTIVITY_CLASS, width_mm, height_mm);
    debugPrintf("TTActivity: nativeSetScreenDimesions done (%.1f x %.1f mm)\n", width_mm, height_mm);

    activity.nativeOnWindowFocusChanged(env, ACTIVITY_CLASS, 1);
    debugPrintf("TTActivity: nativeOnWindowFocusChanged done\n");

    debugPrintf("=== Game started, entering event loop ===\n");

    /* Open controller if available */
    open_controller();
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);

    /* Main event loop */
    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_CONTROLLERDEVICEADDED:
                if (!g_controller) open_controller();
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                if (g_controller &&
                    event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_controller))) {
                    SDL_GameControllerClose(g_controller);
                    g_controller = NULL;
                    debugPrintf("Controller disconnected\n");
                }
                break;

            case SDL_FINGERDOWN:
                send_native_touch_down(env, (int)event.tfinger.fingerId,
                    event.tfinger.x * SCREEN_WIDTH, event.tfinger.y * SCREEN_HEIGHT,
                    "sdl_finger");
                break;

            case SDL_FINGERMOTION:
                send_native_touch_move(env, (int)event.tfinger.fingerId,
                    event.tfinger.x * SCREEN_WIDTH, event.tfinger.y * SCREEN_HEIGHT,
                    "sdl_finger");
                break;

            case SDL_FINGERUP:
                send_native_touch_up(env, (int)event.tfinger.fingerId, "sdl_finger");
                break;

            default:
                break;
            }
        }

        SDL_GameControllerUpdate();

        /* Poll digital buttons every frame instead of relying on SDL button events.
           This is more reliable on Trimui/SDL builds where controller events can be flaky. */
        poll_controller_buttons();

        /* Poll analog axes */
        poll_controller_axes();

        /* Android frontend path can keep trying to remap back to touch-first.
           Keep logical pad 0 bound to the physical gamepad port. */
        maintain_primary_gamepad_mapping();
        pump_gamepad_activation_pulse();

        /* Pump OpenSL ES audio callbacks */
        opensles_shim_pump_callbacks();

        if (g_request_quit) {
            /* debugPrintf("Input: guide/menu requested quit\n"); */
            running = 0;
        }

        SDL_Delay(1);
    }

    /* Clean shutdown */
    activity.nativeOnPause(env, ACTIVITY_CLASS);
    activity.nativeOnStop(env, ACTIVITY_CLASS);

    if (g_controller) SDL_GameControllerClose(g_controller);
    SDL_Quit();

    _exit(0);
}
