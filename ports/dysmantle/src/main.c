/*
 * main.c -- entry point for Syberia ARM64 Linux port
 *
 * Loads libsyberia1.so, resolves imports, creates a fake Android
 * environment, and calls android_main().
 */

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>

#include "android_shim.h"
#include "error.h"
#include "imports.h"
#include "jni_shim.h"
#include "so_util.h"
#include "util.h"

#define MEMORY_MB 256
#define SO_NAME "libsyberia1.so"

// Forward declarations for PadScheme crash recovery
static sigjmp_buf padscheme_jmpbuf;
static volatile sig_atomic_t in_padscheme_update = 0;

// Sentinel crash recovery: track consecutive recoveries to detect loops
static volatile uintptr_t last_recovery_pc = 0;
static volatile int recovery_streak = 0;
#define MAX_RECOVERY_STREAK 64

// Try to recover from a sentinel-pointer crash by decoding the ARM64
// instruction at the crash PC and skipping it.  Returns 1 if recovered.
static int try_skip_sentinel_crash(ucontext_t *uc, uintptr_t fault_addr) {
  uintptr_t pc = uc->uc_mcontext.pc;
  uintptr_t text = (uintptr_t)text_base;

  // Case 1: BLR / BR jumped to an invalid address (PC itself is small)
  if (pc < 0x10000) {
    uc->uc_mcontext.regs[0] = 0;           // fake return value = NULL
    uc->uc_mcontext.pc = uc->uc_mcontext.regs[30]; // return to caller (LR)
    return 1;
  }

  // Case 2: crash in system library (e.g. pthread) caused by sentinel data.
  // Return to the .so caller with x0=0 (fake success/NULL).
  if (fault_addr < 0x10000 && (pc < text || pc >= text + text_size)) {
    uintptr_t lr = uc->uc_mcontext.regs[30];
    fprintf(stderr, "[sentinel-skip] syslib crash at %p, fault 0x%lx → ret to LR %p\n",
            (void *)pc, (unsigned long)fault_addr, (void *)lr);
    uc->uc_mcontext.regs[0] = 0;
    uc->uc_mcontext.pc = lr;
    return 1;
  }

  // Only handle crashes inside .so text with small fault addresses
  if (pc < text || pc >= text + text_size)
    return 0;
  if (fault_addr >= 0x10000)
    return 0;

  // Guard against infinite skip loops (same region crashing repeatedly)
  if (pc == last_recovery_pc) {
    if (++recovery_streak > MAX_RECOVERY_STREAK)
      return 0;  // give up, let it crash
  } else {
    last_recovery_pc = pc;
    recovery_streak = 1;
  }

  uint32_t insn = *(uint32_t *)pc;
  int rt  = insn & 0x1F;         // bits [4:0]
  int rt2 = (insn >> 10) & 0x1F; // bits [14:10] (for LDP)

  int is_load = 0, is_store = 0, is_pair = 0;

  // LDR/STR (unsigned offset): xx 111 x 01 xx imm12 Rn Rt
  //   V=bit26, opc=bits[23:22]: 00=STR, 01=LDR, 10=LDRSW, 11=PRFM/LDR(SIMD)
  if ((insn & 0x3B000000) == 0x39000000) {
    int opc = (insn >> 22) & 3;
    is_load  = (opc >= 1);
    is_store = (opc == 0);
  }
  // LDUR/STUR (unscaled immediate): xx 111 x 00 xx 0 imm9 00 Rn Rt
  else if ((insn & 0x3B200C00) == 0x38000000) {
    int opc = (insn >> 22) & 3;
    is_load  = (opc >= 1);
    is_store = (opc == 0);
  }
  // LDR/STR (register offset): xx 111 x 00 xx 1 Rm opt S 10 Rn Rt
  else if ((insn & 0x3B200C00) == 0x38200800) {
    int opc = (insn >> 22) & 3;
    is_load  = (opc >= 1);
    is_store = (opc == 0);
  }
  // LDP/STP (various): xx 101 x 0xx x imm7 Rt2 Rn Rt
  else if ((insn & 0x3A000000) == 0x28000000) {
    int L = (insn >> 22) & 1;
    is_load  = L;
    is_store = !L;
    is_pair  = 1;
  }

  if (is_load) {
    if (rt < 31)
      uc->uc_mcontext.regs[rt] = 0;
    if (is_pair && rt2 < 31)
      uc->uc_mcontext.regs[rt2] = 0;
    uc->uc_mcontext.pc += 4;
    fprintf(stderr, "[sentinel-skip] LDR at .so+0x%lx → zeroed x%d, skip\n",
            (unsigned long)(pc - text), rt);
    return 1;
  }

  if (is_store) {
    uc->uc_mcontext.pc += 4;
    fprintf(stderr, "[sentinel-skip] STR at .so+0x%lx → skip\n",
            (unsigned long)(pc - text));
    return 1;
  }

  // Unknown instruction but fault_addr < 0x10000 and PC is in .so:
  // skip it and hope for the best
  uc->uc_mcontext.pc += 4;
  fprintf(stderr, "[sentinel-skip] insn 0x%08x at .so+0x%lx → skip\n",
          insn, (unsigned long)(pc - text));
  return 1;
}

static void crash_handler(int sig, siginfo_t *info, void *uctx) {
  ucontext_t *uc = (ucontext_t *)uctx;
  uintptr_t pc = uc->uc_mcontext.pc;
  uintptr_t fault_addr = (uintptr_t)info->si_addr;

  // Try automatic sentinel-crash recovery first (silent on success)
  if ((sig == SIGSEGV || sig == SIGBUS) &&
      try_skip_sentinel_crash(uc, fault_addr)) {
    return;  // resume execution at adjusted PC
  }

  uintptr_t text = (uintptr_t)text_base;
  uintptr_t data = (uintptr_t)data_base;

  fprintf(stderr, "\n=== CRASH ===\n");
  fprintf(stderr, "Signal: %d (%s)\n", sig,
          sig == SIGSEGV ? "SIGSEGV" : sig == SIGBUS ? "SIGBUS" : sig == SIGABRT ? "SIGABRT" : "?");
  fprintf(stderr, "Fault addr: %p\n", (void *)fault_addr);
  fprintf(stderr, "PC:         %p\n", (void *)pc);

  if (pc >= text && pc < text + text_size)
    fprintf(stderr, "PC in .text: offset 0x%lx\n", (unsigned long)(pc - text));
  else if (pc >= data && pc < data + data_size)
    fprintf(stderr, "PC in .data: offset 0x%lx\n", (unsigned long)(pc - data));
  else
    fprintf(stderr, "PC outside libsyberia1.so\n");

  fprintf(stderr, "\nRegisters:\n");
  for (int i = 0; i < 31; i++) {
    fprintf(stderr, "  x%-2d = 0x%016lx", i, (unsigned long)uc->uc_mcontext.regs[i]);
    if (i % 3 == 2 || i == 30)
      fprintf(stderr, "\n");
  }
  fprintf(stderr, "  sp  = 0x%016lx\n", (unsigned long)uc->uc_mcontext.sp);
  fprintf(stderr, "  pc  = 0x%016lx\n", (unsigned long)uc->uc_mcontext.pc);

  // Stack trace: walk LR/FP chain
  fprintf(stderr, "\nBacktrace:\n");
  fprintf(stderr, "  #0  pc %p", (void *)pc);
  if (pc >= text && pc < text + text_size)
    fprintf(stderr, " (libsyberia1.so+0x%lx)", (unsigned long)(pc - text));
  fprintf(stderr, "\n");

  uintptr_t fp = uc->uc_mcontext.regs[29]; // x29 = frame pointer
  for (int frame = 1; frame < 32 && fp; frame++) {
    uintptr_t *fp_ptr = (uintptr_t *)fp;
    uintptr_t next_fp = fp_ptr[0];
    uintptr_t lr = fp_ptr[1];
    if (!lr)
      break;
    fprintf(stderr, "  #%-2d lr %p", frame, (void *)lr);
    if (lr >= text && lr < text + text_size)
      fprintf(stderr, " (libsyberia1.so+0x%lx)", (unsigned long)(lr - text));
    fprintf(stderr, "\n");
    if (next_fp <= fp)
      break; // avoid infinite loops
    fp = next_fp;
  }

  fprintf(stderr, "\nso text_base=%p text_size=0x%zx\n", text_base, text_size);
  fprintf(stderr, "so data_base=%p data_size=0x%zx\n", data_base, data_size);

  // Dump memory maps to identify which library crashed
  fprintf(stderr, "\nMemory maps (near PC):\n");
  FILE *maps = fopen("/proc/self/maps", "r");
  if (maps) {
    char line[512];
    while (fgets(line, sizeof(line), maps)) {
      unsigned long start, end;
      if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
        if (pc >= start && pc < end) {
          fprintf(stderr, ">>> %s", line);
        }
      }
    }
    fclose(maps);
  }

  fprintf(stderr, "=== END CRASH ===\n");
  fflush(stderr);

  // If we crashed inside a PadScheme update wrapper, recover gracefully
  if (in_padscheme_update) {
    in_padscheme_update = 0;
    siglongjmp(padscheme_jmpbuf, 1);
  }

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

// GOT hook wrapper for TeLuaGUI layout functions.
// They return invalid pointers (e.g. 0xb) when a layout doesn't exist.
// We intercept and return NULL instead.
static void *(*orig_buttonLayout)(void *, const void *);
static void *(*orig_layout)(void *, const void *);

static void *buttonLayout_wrapper(void *gui, const void *name) {
  void *result = orig_buttonLayout(gui, name);
  if ((uintptr_t)result < 0x1000)
    return NULL;
  return result;
}

static void *layout_wrapper(void *gui, const void *name) {
  void *result = orig_layout(gui, name);
  if ((uintptr_t)result < 0x1000)
    return NULL;
  return result;
}

// No-op stub for TeSFX functions (audio is stubbed via OpenSL ES)
static void sfx_noop(void *self) { (void)self; }

// PadScheme::updateUp/updateDown can crash when they access TeButtonLayout
// objects with corrupted signal data (from gamepad UI layouts that don't
// exist on this device). We wrap them with sigsetjmp to recover from crashes.
static int (*orig_updateUp)(void *, unsigned int);
static int (*orig_updateDown)(void *, unsigned int);

static int padScheme_updateUp_safe(void *self, unsigned int buttonId) {
  in_padscheme_update = 1;
  if (sigsetjmp(padscheme_jmpbuf, 1) == 0) {
    int result = orig_updateUp(self, buttonId);
    in_padscheme_update = 0;
    return result;
  }
  in_padscheme_update = 0;
  return 0;
}

static int padScheme_updateDown_safe(void *self, unsigned int buttonId) {
  in_padscheme_update = 1;
  if (sigsetjmp(padscheme_jmpbuf, 1) == 0) {
    int result = orig_updateDown(self, buttonId);
    in_padscheme_update = 0;
    return result;
  }
  in_padscheme_update = 0;
  return 0;
}

int main(int argc, char *argv[]) {
  install_crash_handler();

  // Default: BUKA OBB (Russian, has all assets).
  // Use --google flag to switch to Google Play OBB.
  int use_buka = 1;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--google") == 0) {
      use_buka = 0;
      debugPrintf("=== Using Google Play OBB ===\n");
    }
  }
  if (use_buka) {
    jni_shim_set_package("ru.buka.syberia1", 3);
    debugPrintf("=== Using BUKA OBB (default) ===\n");
  }

  debugPrintf("=== Syberia ARM64 Linux Port ===\n");

  // Allocate heap for the .so loader
  size_t heap_size = MEMORY_MB * 1024 * 1024;
  void *heap = mmap(NULL, heap_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap == MAP_FAILED)
    fatal_error("Failed to allocate %d MB heap", MEMORY_MB);
  debugPrintf("Heap allocated: %p (%d MB)\n", heap, MEMORY_MB);

  // Load the shared object
  debugPrintf("Loading %s...\n", SO_NAME);
  if (so_load(SO_NAME, heap, heap_size) < 0)
    fatal_error("Failed to load %s", SO_NAME);
  debugPrintf("Loaded %s: text=%p+%zu data=%p+%zu\n", SO_NAME, text_base,
              text_size, data_base, data_size);

  // Patch "BUKA" distributor string to "GOOG" when not using BUKA OBB,
  // so the game won't force Russian language.
  // The string is in .rodata (part of text segment), so we need to make
  // text writable first.
  if (!use_buka) {
    so_make_text_writable();
    int patched = 0;
    // Scan text segment (.rodata lives here)
    char *t = (char *)text_base;
    for (size_t i = 0; i + 4 < text_size; i++) {
      if (t[i] == 'B' && t[i + 1] == 'U' && t[i + 2] == 'K' &&
          t[i + 3] == 'A' && t[i + 4] == '\0') {
        t[i] = 'G';
        t[i + 1] = 'O';
        t[i + 2] = 'O';
        t[i + 3] = 'G';
        debugPrintf("Patched BUKA -> GOOG at text+0x%zx\n", i);
        patched++;
      }
    }
    // Also scan data segment
    char *d = (char *)data_base;
    for (size_t i = 0; i + 4 < data_size; i++) {
      if (d[i] == 'B' && d[i + 1] == 'U' && d[i + 2] == 'K' &&
          d[i + 3] == 'A' && d[i + 4] == '\0') {
        d[i] = 'G';
        d[i + 1] = 'O';
        d[i + 2] = 'O';
        d[i + 3] = 'G';
        debugPrintf("Patched BUKA -> GOOG at data+0x%zx\n", i);
        patched++;
      }
    }
    // Also patch the hardcoded "ru" language default to "en"
    // (there's exactly one \0ru\0 string in the .so)
    for (size_t i = 1; i + 2 < text_size; i++) {
      if (t[i - 1] == '\0' && t[i] == 'r' && t[i + 1] == 'u' &&
          t[i + 2] == '\0') {
        t[i] = 'e';
        t[i + 1] = 'n';
        debugPrintf("Patched ru -> en at text+0x%zx\n", i);
        patched++;
      }
    }
    for (size_t i = 1; i + 2 < data_size; i++) {
      if (d[i - 1] == '\0' && d[i] == 'r' && d[i + 1] == 'u' &&
          d[i + 2] == '\0') {
        d[i] = 'e';
        d[i + 1] = 'n';
        debugPrintf("Patched ru -> en at data+0x%zx\n", i);
        patched++;
      }
    }

    so_make_text_executable();
    debugPrintf("Patched %d occurrences total\n", patched);
  }

  // Relocate
  debugPrintf("Relocating...\n");
  if (so_relocate() < 0)
    fatal_error("Failed to relocate %s", SO_NAME);

  // Resolve imports
  debugPrintf("Resolving %zu imports...\n", dynlib_numfunctions);
  if (so_resolve(dynlib_functions, dynlib_numfunctions, 0) < 0)
    fatal_error("Failed to resolve imports");

  // Finalize: make text read-only+exec, flush caches
  so_finalize();
  so_flush_caches();

  // Hook buttonLayout via GOT to return NULL for missing layouts.
  // Patching GOT (data segment, RW) avoids instruction cache issues.
  {
    uintptr_t got_slot = so_find_rel_addr_safe(
        "_ZN8TeLuaGUI12buttonLayoutERK8TeString");
    if (got_slot) {
      uintptr_t *slot = (uintptr_t *)got_slot;
      orig_buttonLayout = (void *(*)(void *, const void *))*slot;
      *slot = (uintptr_t)buttonLayout_wrapper;
      debugPrintf("GOT-hooked buttonLayout: orig=%p wrapper=%p\n",
                  (void *)orig_buttonLayout, (void *)buttonLayout_wrapper);
    } else {
      debugPrintf("Warning: buttonLayout GOT slot not found\n");
    }
  }

  // Hook layout() too — same invalid pointer issue
  {
    uintptr_t got_slot = so_find_rel_addr_safe(
        "_ZN8TeLuaGUI6layoutERK8TeString");
    if (got_slot) {
      uintptr_t *slot = (uintptr_t *)got_slot;
      orig_layout = (void *(*)(void *, const void *))*slot;
      *slot = (uintptr_t)layout_wrapper;
      debugPrintf("GOT-hooked layout: orig=%p wrapper=%p\n",
                  (void *)orig_layout, (void *)layout_wrapper);
    }
  }

  // Wrap PadScheme::updateUp/updateDown with crash recovery.
  // They can crash accessing corrupted TeButtonLayout objects from missing
  // gamepad UI layouts. The wrapper catches the crash via sigsetjmp.
  {
    uintptr_t got_slot = so_find_rel_addr_safe(
        "_ZN9PadScheme8updateUpEj");
    if (got_slot) {
      uintptr_t *slot = (uintptr_t *)got_slot;
      orig_updateUp = (int (*)(void *, unsigned int))*slot;
      *slot = (uintptr_t)padScheme_updateUp_safe;
      debugPrintf("GOT-hooked PadScheme::updateUp (crash-safe)\n");
    }
  }
  {
    uintptr_t got_slot = so_find_rel_addr_safe(
        "_ZN9PadScheme10updateDownEj");
    if (got_slot) {
      uintptr_t *slot = (uintptr_t *)got_slot;
      orig_updateDown = (int (*)(void *, unsigned int))*slot;
      *slot = (uintptr_t)padScheme_updateDown_safe;
      debugPrintf("GOT-hooked PadScheme::updateDown (crash-safe)\n");
    }
  }

  // No-op TeSFX::stop and TeSFX::play — OpenSL ES is a stub so these
  // just crash on corrupted sound player pointers (0x8000000080000000).
  {
    uintptr_t slot_addr;
    slot_addr = so_find_rel_addr_safe("_ZN5TeSFX4stopEv");
    if (slot_addr) {
      *(uintptr_t *)slot_addr = (uintptr_t)sfx_noop;
      debugPrintf("GOT-hooked TeSFX::stop → no-op\n");
    }
    // TeSFX::play left unhooked — valid SFX objects need to play
    // (corrupted ones are caught by universal crash recovery)
    slot_addr = so_find_rel_addr_safe("_ZN5TeSFX5closeEv");
    if (slot_addr) {
      *(uintptr_t *)slot_addr = (uintptr_t)sfx_noop;
      debugPrintf("GOT-hooked TeSFX::close → no-op\n");
    }
  }

  // Run .init_array constructors
  debugPrintf("Running init array...\n");
  so_execute_init_array();

  // Find android_main
  uintptr_t android_main_addr = so_find_addr("android_main");
  if (!android_main_addr)
    fatal_error("Could not find android_main in %s", SO_NAME);
  debugPrintf("android_main found at %p\n", (void *)android_main_addr);

  // Initialize fake Android environment (also inits SDL)
  struct android_app *app = android_shim_init();
  if (!app)
    fatal_error("Failed to initialize Android shim");

  // Send initial lifecycle commands
  android_shim_send_cmd(app, APP_CMD_INIT_WINDOW);
  android_shim_send_cmd(app, APP_CMD_GAINED_FOCUS);

  // Call android_main
  debugPrintf("Calling android_main...\n");
  void (*android_main_func)(struct android_app *) =
      (void (*)(struct android_app *))android_main_addr;
  android_main_func(app);

  debugPrintf("android_main returned\n");

  // Hard exit — the .so's destructors crash during normal cleanup
  _exit(0);
}
