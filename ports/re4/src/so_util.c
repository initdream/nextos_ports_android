/*
 * so_util.c -- utils to load and hook .so modules  [ARM 32-bit / armhf]
 *
 * Porte do so_util AArch64 (max_arm64) para ARM 32-bit (RE4 Unity v7a).
 * Diferenças: Elf32, relocações REL (.rel.dyn/.rel.plt, addend IN-PLACE), tipos
 * R_ARM_*, hook_arm (LDR PC), init_array com ponteiros de 4 bytes.
 */

#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#include "error.h"
#include "so_util.h"
#include "util.h"

#ifndef EM_ARM
#define EM_ARM 40
#endif
/* tipos de relocacao ARM (do .rel.dyn/.rel.plt) */
#ifndef R_ARM_ABS32
#define R_ARM_ABS32     2
#endif
#ifndef R_ARM_GLOB_DAT
#define R_ARM_GLOB_DAT  21
#endif
#ifndef R_ARM_JUMP_SLOT
#define R_ARM_JUMP_SLOT 22
#endif
#ifndef R_ARM_RELATIVE
#define R_ARM_RELATIVE  23
#endif

void *text_base, *text_virtbase;
size_t text_size;

void *data_base, *data_virtbase;
size_t data_size;

static void *load_base, *load_virtbase;
static size_t load_size;

static void *so_base;

static Elf32_Ehdr *elf_hdr;
static Elf32_Phdr *prog_hdr;
static Elf32_Shdr *sec_hdr;
static Elf32_Sym *syms;
static int num_syms;

static char *shstrtab;
static char *dynstrtab;

/* ===== multi-modulo: salva/restaura o estado dos globais ===== */
typedef struct so_module so_module;
struct so_module {
  void *text_base, *text_virtbase, *data_base, *data_virtbase;
  void *load_base, *load_virtbase, *so_base;
  size_t text_size, data_size, load_size;
  Elf32_Ehdr *elf_hdr; Elf32_Phdr *prog_hdr; Elf32_Shdr *sec_hdr;
  Elf32_Sym *syms; int num_syms; char *shstrtab, *dynstrtab;
};
so_module *so_save(void) {
  so_module *m = (so_module *)malloc(sizeof(so_module));
  m->text_base = text_base; m->text_virtbase = text_virtbase;
  m->data_base = data_base; m->data_virtbase = data_virtbase;
  m->load_base = load_base; m->load_virtbase = load_virtbase;
  m->so_base = so_base;
  m->text_size = text_size; m->data_size = data_size; m->load_size = load_size;
  m->elf_hdr = elf_hdr; m->prog_hdr = prog_hdr; m->sec_hdr = sec_hdr;
  m->syms = syms; m->num_syms = num_syms;
  m->shstrtab = shstrtab; m->dynstrtab = dynstrtab;
  return m;
}
void so_use(so_module *m) {
  text_base = m->text_base; text_virtbase = m->text_virtbase;
  data_base = m->data_base; data_virtbase = m->data_virtbase;
  load_base = m->load_base; load_virtbase = m->load_virtbase;
  so_base = m->so_base;
  text_size = m->text_size; data_size = m->data_size; load_size = m->load_size;
  elf_hdr = m->elf_hdr; prog_hdr = m->prog_hdr; sec_hdr = m->sec_hdr;
  syms = m->syms; num_syms = m->num_syms;
  shstrtab = m->shstrtab; dynstrtab = m->dynstrtab;
}

/* hook ARM 32-bit: LDR PC, [PC, #-4] (0xe51ff004) + endereco destino.
   PC=insn+8; [PC,#-4]=insn+4=hook[1]. Interworking (bit0 do dst) tratado pelo LDR PC. */
void hook_arm(uintptr_t addr, uintptr_t dst) {
  if (addr == 0)
    return;
  uint32_t *hook = (uint32_t *)addr;
  hook[0] = 0xe51ff004u; // LDR PC, [PC, #-4]
  hook[1] = (uint32_t)dst;
}
/* compat: o codigo Unity (vindo do HK) chama hook_arm64 */
void hook_arm64(uintptr_t addr, uintptr_t dst) { hook_arm(addr, dst); }

void so_make_text_writable(void) {
  const size_t text_asize = ALIGN_MEM(text_size, 0x1000);
  if (mprotect(text_virtbase, text_asize,
               PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
    debugPrintf("Warning: Could not make text segment writable\n");
  }
}

void so_make_text_executable(void) {
  const size_t text_asize = ALIGN_MEM(text_size, 0x1000);
  if (mprotect(text_virtbase, text_asize, PROT_READ | PROT_EXEC) != 0) {
    debugPrintf("Warning: Could not restore text segment permissions\n");
  }
}

void so_flush_caches(void) {
  __builtin___clear_cache((char *)load_virtbase,
                          (char *)load_virtbase + load_size);
}

void so_free_temp(void) {
  free(so_base);
  so_base = NULL;
}

static inline size_t round_up(size_t x, size_t a) {
  return (x + a - 1) & ~(a - 1);
}

static int protect_range(void *start, size_t len, int prot) {
  int ps = getpagesize();
  if (ps <= 0)
    ps = 4096;
  uintptr_t addr = (uintptr_t)start;
  uintptr_t page_base = addr & ~((uintptr_t)ps - 1);
  size_t head = addr - page_base;
  size_t plen = round_up(len + head, (size_t)ps);
  if (mprotect((void *)page_base, plen, prot) != 0) {
    debugPrintf("mprotect(%p, %zu, 0x%x) failed: %s\n", (void *)page_base,
                plen, prot, strerror(errno));
    return -1;
  }
  return 0;
}

void so_finalize(void) {
  if (protect_range(text_virtbase, text_size, PROT_READ | PROT_EXEC) != 0) {
    fatal_error("Error: could not set RX on text at %p (size %zu)",
                text_virtbase, text_size);
  }
  if (protect_range(data_virtbase, data_size, PROT_READ | PROT_WRITE) != 0) {
    fatal_error("Error: could not set RW on data at %p (size %zu)",
                data_virtbase, data_size);
  }
}

int so_load(const char *filename, void *base, size_t max_size) {
  int res = 0;
  size_t so_size = 0;
  int text_segno = -1;
  int data_segno = -1;

  debugPrintf("so_load: Opening %s\n", filename);
  FILE *fd = fopen(filename, "rb");
  if (fd == NULL) {
    debugPrintf("so_load: Failed to open file\n");
    return -1;
  }

  fseek(fd, 0, SEEK_END);
  so_size = ftell(fd);
  fseek(fd, 0, SEEK_SET);
  debugPrintf("so_load: File size: %zu bytes\n", so_size);

  so_base = malloc(so_size);
  if (!so_base) {
    fclose(fd);
    return -2;
  }

  if (fread(so_base, so_size, 1, fd) != 1) {
    fclose(fd);
    free(so_base);
    return -3;
  }
  fclose(fd);

  if (memcmp(so_base, ELFMAG, SELFMAG) != 0) {
    debugPrintf("so_load: Not a valid ELF file\n");
    res = -1;
    goto err_free_so;
  }

  elf_hdr = (Elf32_Ehdr *)so_base;

  if (elf_hdr->e_ident[EI_CLASS] != ELFCLASS32) {
    debugPrintf("so_load: Not a 32-bit ELF file\n");
    res = -1;
    goto err_free_so;
  }

  if (elf_hdr->e_machine != EM_ARM) {
    debugPrintf("so_load: Not an ARM ELF (machine=%d)\n", elf_hdr->e_machine);
    res = -1;
    goto err_free_so;
  }

  debugPrintf("so_load: ELF32 ARM, %d program headers, %d section headers\n",
              elf_hdr->e_phnum, elf_hdr->e_shnum);

  if (elf_hdr->e_phoff + (elf_hdr->e_phnum * sizeof(Elf32_Phdr)) > so_size) {
    debugPrintf("so_load: Program headers extend beyond file\n");
    res = -1;
    goto err_free_so;
  }

  prog_hdr = (Elf32_Phdr *)((uintptr_t)so_base + elf_hdr->e_phoff);

  if (elf_hdr->e_shoff + (elf_hdr->e_shnum * sizeof(Elf32_Shdr)) > so_size) {
    debugPrintf("so_load: Section headers extend beyond file\n");
    res = -1;
    goto err_free_so;
  }

  sec_hdr = (Elf32_Shdr *)((uintptr_t)so_base + elf_hdr->e_shoff);

  if (elf_hdr->e_shstrndx >= elf_hdr->e_shnum) {
    debugPrintf("so_load: Invalid string table index\n");
    res = -1;
    goto err_free_so;
  }

  shstrtab =
      (char *)((uintptr_t)so_base + sec_hdr[elf_hdr->e_shstrndx].sh_offset);

  for (int i = 0; i < elf_hdr->e_phnum; i++) {
    if (prog_hdr[i].p_type == PT_LOAD) {
      const size_t prog_size =
          ALIGN_MEM(prog_hdr[i].p_memsz, prog_hdr[i].p_align);
      if ((prog_hdr[i].p_flags & PF_X) == PF_X) {
        text_segno = i;
      } else {
        if (text_segno < 0)
          goto err_free_so;
        data_segno = i;
        load_size = prog_hdr[i].p_vaddr + prog_size;
      }
    }
  }

  load_size = ALIGN_MEM(load_size, 0x1000);
  debugPrintf("so_load: Total load size: %zu bytes (max: %zu)\n", load_size,
              max_size);
  if (load_size > max_size) {
    res = -3;
    goto err_free_so;
  }

  load_base = base;
  if (!load_base)
    goto err_free_so;

  memset(load_base, 0, load_size);
  load_virtbase = load_base;

  debugPrintf("so_load: load base = %p\n", load_virtbase);

  // text
  text_size = prog_hdr[text_segno].p_memsz;
  text_virtbase =
      (void *)(prog_hdr[text_segno].p_vaddr + (Elf32_Addr)(uintptr_t)load_virtbase);
  text_base = (void *)(prog_hdr[text_segno].p_vaddr + (Elf32_Addr)(uintptr_t)load_base);
  prog_hdr[text_segno].p_vaddr = (Elf32_Addr)(uintptr_t)text_virtbase;
  memcpy(text_base,
         (void *)((uintptr_t)so_base + prog_hdr[text_segno].p_offset),
         prog_hdr[text_segno].p_filesz);

  // data
  data_size = prog_hdr[data_segno].p_memsz;
  data_virtbase =
      (void *)(prog_hdr[data_segno].p_vaddr + (Elf32_Addr)(uintptr_t)load_virtbase);
  data_base = (void *)(prog_hdr[data_segno].p_vaddr + (Elf32_Addr)(uintptr_t)load_base);
  prog_hdr[data_segno].p_vaddr = (Elf32_Addr)(uintptr_t)data_virtbase;
  memcpy(data_base,
         (void *)((uintptr_t)so_base + prog_hdr[data_segno].p_offset),
         prog_hdr[data_segno].p_filesz);

  syms = NULL;
  dynstrtab = NULL;

  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".dynsym") == 0) {
      syms = (Elf32_Sym *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
      num_syms = sec_hdr[i].sh_size / sizeof(Elf32_Sym);
    } else if (strcmp(sh_name, ".dynstr") == 0) {
      dynstrtab = (char *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
    }
  }

  if (syms == NULL || dynstrtab == NULL) {
    res = -2;
    goto err_free_load;
  }

  debugPrintf("so_load: %d dynamic symbols found\n", num_syms);
  return 0;

err_free_load:
  free(load_base);
err_free_so:
  free(so_base);
  return res;
}

int so_relocate(void) {
  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".rel.dyn") == 0 || strcmp(sh_name, ".rel.plt") == 0) {
      Elf32_Rel *rels = (Elf32_Rel *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
      for (int j = 0; j < (int)(sec_hdr[i].sh_size / sizeof(Elf32_Rel)); j++) {
        uint32_t *ptr = (uint32_t *)((uintptr_t)text_base + rels[j].r_offset);
        Elf32_Sym *sym = &syms[ELF32_R_SYM(rels[j].r_info)];
        int type = ELF32_R_TYPE(rels[j].r_info);
        uintptr_t base = (uintptr_t)text_virtbase;
        switch (type) {
        case R_ARM_RELATIVE:
          /* REL: addend = valor in-place (*ptr) */
          *ptr = (uint32_t)(base + *ptr);
          break;
        case R_ARM_ABS32:
          if (sym->st_shndx != SHN_UNDEF)
            *ptr = (uint32_t)(base + sym->st_value + *ptr);
          break;
        case R_ARM_GLOB_DAT:
        case R_ARM_JUMP_SLOT:
          /* JUMP_SLOT in-place aponta pro stub PLT -> NAO usar como addend */
          if (sym->st_shndx != SHN_UNDEF)
            *ptr = (uint32_t)(base + sym->st_value);
          break;
        default:
          fatal_error("Error: unknown ARM relocation type: %d\n", type);
          break;
        }
      }
    }
  }
  return 0;
}

int so_resolve(DynLibFunction *funcs, int num_funcs,
               int taint_missing_imports) {
  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".rel.dyn") == 0 || strcmp(sh_name, ".rel.plt") == 0) {
      Elf32_Rel *rels = (Elf32_Rel *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
      for (int j = 0; j < (int)(sec_hdr[i].sh_size / sizeof(Elf32_Rel)); j++) {
        uint32_t *ptr = (uint32_t *)((uintptr_t)text_base + rels[j].r_offset);
        Elf32_Sym *sym = &syms[ELF32_R_SYM(rels[j].r_info)];
        int type = ELF32_R_TYPE(rels[j].r_info);
        switch (type) {
        case R_ARM_ABS32:
        case R_ARM_GLOB_DAT:
        case R_ARM_JUMP_SLOT: {
          if (sym->st_shndx == SHN_UNDEF) {
            char *name = dynstrtab + sym->st_name;
            int found = 0;
            for (int k = 0; k < num_funcs; k++) {
              if (strcmp(name, funcs[k].symbol) == 0) {
                *ptr = (uint32_t)funcs[k].func;
                found = 1;
                break;
              }
            }
            if (!found) {
              extern void *re4_resolve(const char *);
              void *fp = re4_resolve(name);
              if (fp) { *ptr = (uint32_t)(uintptr_t)fp; }
              else { if (taint_missing_imports) *ptr = (uint32_t)rels[j].r_offset;
                fprintf(stderr, "*** UNRESOLVED import: \"%s\" ***\n", name); }
            }
          }
          break;
        }
        default:
          break;
        }
      }
    }
  }
  return 0;
}

void so_execute_init_array(void) {
  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".init_array") == 0) {
      int (**init_array)() =
          (void *)((uintptr_t)text_virtbase + sec_hdr[i].sh_addr);
      for (int j = 0; j < (int)(sec_hdr[i].sh_size / 4); j++) {
        if(getenv("RE4_SKIP")&&j==atoi(getenv("RE4_SKIP"))){fprintf(stderr,"[init %d PULADO]\n",j);continue;}
        fprintf(stderr,"[init %d] @ unity+0x%lx\n",j,(unsigned long)((uintptr_t)init_array[j]-(uintptr_t)text_virtbase)); fflush(stderr);
        if (init_array[j] != 0)
          init_array[j]();
      }
    }
  }
}

uintptr_t so_find_addr(const char *symbol) {
  for (int i = 0; i < num_syms; i++) {
    char *name = dynstrtab + syms[i].st_name;
    if (strcmp(name, symbol) == 0)
      return (uintptr_t)text_base + syms[i].st_value;
  }
  fatal_error("Error: could not find symbol: %s\n", symbol);
  return 0;
}

uintptr_t so_find_addr_safe(const char *symbol) {
  for (int i = 0; i < num_syms; i++) {
    char *name = dynstrtab + syms[i].st_name;
    if (strcmp(name, symbol) == 0)
      return (uintptr_t)text_base + syms[i].st_value;
  }
  return 0;
}

uintptr_t so_find_addr_rx(const char *symbol) {
  for (int i = 0; i < num_syms; i++) {
    char *name = dynstrtab + syms[i].st_name;
    if (strcmp(name, symbol) == 0)
      return (uintptr_t)text_virtbase + syms[i].st_value;
  }
  fatal_error("Error: could not find symbol: %s\n", symbol);
  return 0;
}

uintptr_t so_find_rel_addr(const char *symbol) {
  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".rel.dyn") == 0 || strcmp(sh_name, ".rel.plt") == 0) {
      Elf32_Rel *rels = (Elf32_Rel *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
      for (int j = 0; j < (int)(sec_hdr[i].sh_size / sizeof(Elf32_Rel)); j++) {
        Elf32_Sym *sym = &syms[ELF32_R_SYM(rels[j].r_info)];
        int type = ELF32_R_TYPE(rels[j].r_info);
        if (type == R_ARM_GLOB_DAT || type == R_ARM_JUMP_SLOT) {
          char *name = dynstrtab + sym->st_name;
          if (strcmp(name, symbol) == 0)
            return (uintptr_t)text_base + rels[j].r_offset;
        }
      }
    }
  }
  fatal_error("Error: could not find symbol: %s\n", symbol);
  return 0;
}

uintptr_t so_find_rel_addr_safe(const char *symbol) {
  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".rel.dyn") == 0 || strcmp(sh_name, ".rel.plt") == 0) {
      Elf32_Rel *rels = (Elf32_Rel *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
      for (int j = 0; j < (int)(sec_hdr[i].sh_size / sizeof(Elf32_Rel)); j++) {
        Elf32_Sym *sym = &syms[ELF32_R_SYM(rels[j].r_info)];
        int type = ELF32_R_TYPE(rels[j].r_info);
        if (type == R_ARM_GLOB_DAT || type == R_ARM_JUMP_SLOT) {
          char *name = dynstrtab + sym->st_name;
          if (strcmp(name, symbol) == 0)
            return (uintptr_t)text_base + rels[j].r_offset;
        }
      }
    }
  }
  return 0;
}

DynLibFunction *so_find_import(DynLibFunction *funcs, int num_funcs,
                               const char *name) {
  for (int i = 0; i < num_funcs; ++i)
    if (!strcmp(funcs[i].symbol, name))
      return &funcs[i];
  return NULL;
}

int so_unload(void) {
  if (load_base == NULL)
    return -1;
  if (so_base)
    so_free_temp();
  if (munmap(load_base, load_size) != 0)
    fatal_error("Error: could not unmap library memory");
  return 0;
}
