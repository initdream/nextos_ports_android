/*
 * so_util_x64.c -- so-loader x86_64, multi-modulo (bring-up no PC)
 * Base: core/so_util.c (AArch64) adaptado p/ EM_X86_64 + relocs R_X86_64_*
 * + fix ABS64-UNDEF (descoberto no hollow-recon). Multi-modulo via struct Module.
 * Na fase device, troca-se pela versao AArch64 do core.
 */
#include <elf.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "so_util_x64.h"

#ifndef EM_X86_64
#define EM_X86_64 62
#endif

#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

/* loaded modules (p/ resolver imports cruzados, ex: libGame -> libc++_shared) */
static Module *g_modules[16];
static int g_num_modules = 0;

void so_register_module(Module *m) {
  if (g_num_modules < 16)
    g_modules[g_num_modules++] = m;
}

/* acha um simbolo DEFINIDO (exportado) em qualquer modulo ja carregado */
uintptr_t so_lookup_global(const char *name) {
  for (int i = 0; i < g_num_modules; i++) {
    Module *m = g_modules[i];
    for (int j = 0; j < m->num_syms; j++) {
      Elf64_Sym *s = &m->syms[j];
      if (s->st_shndx == SHN_UNDEF)
        continue;
      if (ELF64_ST_TYPE(s->st_info) != STT_FUNC &&
          ELF64_ST_TYPE(s->st_info) != STT_OBJECT)
        continue;
      const char *sn = m->dynstr + s->st_name;
      if (strcmp(sn, name) == 0)
        return (uintptr_t)m->load_base + s->st_value;
    }
  }
  return 0;
}

uintptr_t so_symbol(Module *m, const char *name) {
  for (int j = 0; j < m->num_syms; j++) {
    if (m->syms[j].st_shndx == SHN_UNDEF)
      continue;
    const char *sn = m->dynstr + m->syms[j].st_name;
    if (strcmp(sn, name) == 0)
      return (uintptr_t)m->load_base + m->syms[j].st_value;
  }
  return 0;
}

int so_load(Module *m, const char *filename) {
  memset(m, 0, sizeof(*m));
  m->name = strdup(filename);

  FILE *fd = fopen(filename, "rb");
  if (!fd) { fprintf(stderr, "so_load: cannot open %s\n", filename); return -1; }
  fseek(fd, 0, SEEK_END);
  size_t so_size = ftell(fd);
  fseek(fd, 0, SEEK_SET);
  m->file_buf = malloc(so_size);
  if (fread(m->file_buf, so_size, 1, fd) != 1) { fclose(fd); return -3; }
  fclose(fd);

  if (memcmp(m->file_buf, ELFMAG, SELFMAG) != 0) return -1;
  Elf64_Ehdr *eh = (Elf64_Ehdr *)m->file_buf;
  if (eh->e_ident[EI_CLASS] != ELFCLASS64) return -1;
  if (eh->e_machine != EM_X86_64) {
    fprintf(stderr, "so_load: not x86_64 (machine=%d)\n", eh->e_machine);
    return -1;
  }
  m->ehdr = eh;
  m->phdr = (Elf64_Phdr *)((uintptr_t)m->file_buf + eh->e_phoff);
  m->shdr = (Elf64_Shdr *)((uintptr_t)m->file_buf + eh->e_shoff);
  m->shstr = (char *)((uintptr_t)m->file_buf + m->shdr[eh->e_shstrndx].sh_offset);

  /* descobre extensao total do espaco de carga (max p_vaddr+p_memsz dos PT_LOAD) */
  size_t load_size = 0;
  for (int i = 0; i < eh->e_phnum; i++) {
    if (m->phdr[i].p_type == PT_LOAD) {
      size_t end = m->phdr[i].p_vaddr + m->phdr[i].p_memsz;
      if (end > load_size) load_size = end;
    }
  }
  load_size = ALIGN_UP(load_size, 0x1000);
  m->load_size = load_size;

  /* mmap contiguo RWX (no PC e ok; no device o core faz RX/RW separado) */
  m->load_base = mmap(NULL, load_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (m->load_base == MAP_FAILED) { fprintf(stderr, "so_load: mmap failed\n"); return -2; }
  memset(m->load_base, 0, load_size);

  /* copia cada PT_LOAD pro vaddr correspondente (base+vaddr) */
  for (int i = 0; i < eh->e_phnum; i++) {
    if (m->phdr[i].p_type == PT_LOAD) {
      memcpy((void *)((uintptr_t)m->load_base + m->phdr[i].p_vaddr),
             (void *)((uintptr_t)m->file_buf + m->phdr[i].p_offset),
             m->phdr[i].p_filesz);
    }
  }

  /* .dynsym / .dynstr */
  for (int i = 0; i < eh->e_shnum; i++) {
    const char *sn = m->shstr + m->shdr[i].sh_name;
    if (strcmp(sn, ".dynsym") == 0) {
      m->syms = (Elf64_Sym *)((uintptr_t)m->load_base + m->shdr[i].sh_addr);
      m->num_syms = m->shdr[i].sh_size / sizeof(Elf64_Sym);
    } else if (strcmp(sn, ".dynstr") == 0) {
      m->dynstr = (char *)((uintptr_t)m->load_base + m->shdr[i].sh_addr);
    }
  }
  if (!m->syms || !m->dynstr) return -2;
  fprintf(stderr, "so_load: %s @ %p  size=%zu  syms=%d\n",
          filename, m->load_base, load_size, m->num_syms);
  return 0;
}

/* relocs internas (RELATIVE + ABS64/GLOB_DAT/JUMP_SLOT de simbolo DEFINIDO) */
int so_relocate(Module *m) {
  Elf64_Ehdr *eh = m->ehdr;
  for (int i = 0; i < eh->e_shnum; i++) {
    const char *sn = m->shstr + m->shdr[i].sh_name;
    if (strcmp(sn, ".rela.dyn") && strcmp(sn, ".rela.plt")) continue;
    Elf64_Rela *r = (Elf64_Rela *)((uintptr_t)m->load_base + m->shdr[i].sh_addr);
    int n = m->shdr[i].sh_size / sizeof(Elf64_Rela);
    for (int j = 0; j < n; j++) {
      uintptr_t *ptr = (uintptr_t *)((uintptr_t)m->load_base + r[j].r_offset);
      Elf64_Sym *sym = &m->syms[ELF64_R_SYM(r[j].r_info)];
      int type = ELF64_R_TYPE(r[j].r_info);
      switch (type) {
      case R_X86_64_RELATIVE:
        *ptr = (uintptr_t)m->load_base + r[j].r_addend;
        break;
      case R_X86_64_64: /* = ABS64: so p/ DEFINIDO; UNDEF resolve em so_resolve */
      case R_X86_64_GLOB_DAT:
      case R_X86_64_JUMP_SLOT:
        if (sym->st_shndx != SHN_UNDEF)
          *ptr = (uintptr_t)m->load_base + sym->st_value + r[j].r_addend;
        break;
      default:
        break; /* TLS etc.: ignora no bring-up */
      }
    }
  }
  return 0;
}

/* resolve imports (UNDEF) via callback do harness; loga UNRESOLVED */
int so_resolve(Module *m, so_resolver_t resolve, void *user) {
  Elf64_Ehdr *eh = m->ehdr;
  int unresolved = 0;
  for (int i = 0; i < eh->e_shnum; i++) {
    const char *sn = m->shstr + m->shdr[i].sh_name;
    if (strcmp(sn, ".rela.dyn") && strcmp(sn, ".rela.plt")) continue;
    Elf64_Rela *r = (Elf64_Rela *)((uintptr_t)m->load_base + m->shdr[i].sh_addr);
    int n = m->shdr[i].sh_size / sizeof(Elf64_Rela);
    for (int j = 0; j < n; j++) {
      Elf64_Sym *sym = &m->syms[ELF64_R_SYM(r[j].r_info)];
      int type = ELF64_R_TYPE(r[j].r_info);
      if (type != R_X86_64_64 && type != R_X86_64_GLOB_DAT &&
          type != R_X86_64_JUMP_SLOT)
        continue;
      if (sym->st_shndx != SHN_UNDEF)
        continue;
      const char *name = m->dynstr + sym->st_name;
      uintptr_t addr = resolve(name, user);
      uintptr_t *ptr = (uintptr_t *)((uintptr_t)m->load_base + r[j].r_offset);
      if (addr) {
        *ptr = addr + r[j].r_addend;
      } else {
        unresolved++;
        if (unresolved <= 80)
          fprintf(stderr, "  UNRESOLVED: %s\n", name);
      }
    }
  }
  fprintf(stderr, "so_resolve(%s): %d UNRESOLVED\n", m->name, unresolved);
  return unresolved;
}

void so_execute_init_array(Module *m) {
  Elf64_Ehdr *eh = m->ehdr;
  for (int i = 0; i < eh->e_shnum; i++) {
    const char *sn = m->shstr + m->shdr[i].sh_name;
    if (strcmp(sn, ".init_array") == 0) {
      void (**ia)(void) = (void *)((uintptr_t)m->load_base + m->shdr[i].sh_addr);
      int n = m->shdr[i].sh_size / 8;
      fprintf(stderr, "so: init_array(%s) %d entradas\n", m->name, n);
      for (int j = 0; j < n; j++)
        if (ia[j]) ia[j]();
    }
  }
}

/* hook x86_64: mov rax, imm64 ; jmp rax  (12 bytes) */
void hook_x64(uintptr_t addr, uintptr_t dst) {
  if (!addr) return;
  unsigned char *p = (unsigned char *)addr;
  p[0] = 0x48; p[1] = 0xB8;            /* movabs rax, imm64 */
  *(uint64_t *)(p + 2) = dst;
  p[10] = 0xFF; p[11] = 0xE0;          /* jmp rax */
}
