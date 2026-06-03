#ifndef SO_UTIL_X64_H
#define SO_UTIL_X64_H
#include <elf.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  char *name;
  void *file_buf;
  void *load_base;
  size_t load_size;
  Elf64_Ehdr *ehdr;
  Elf64_Phdr *phdr;
  Elf64_Shdr *shdr;
  char *shstr;
  Elf64_Sym *syms;
  int num_syms;
  char *dynstr;
} Module;

typedef uintptr_t (*so_resolver_t)(const char *name, void *user);

int so_load(Module *m, const char *filename);
int so_relocate(Module *m);
int so_resolve(Module *m, so_resolver_t resolve, void *user);
void so_execute_init_array(Module *m);
void so_register_module(Module *m);
uintptr_t so_lookup_global(const char *name);
uintptr_t so_symbol(Module *m, const char *name);
void hook_x64(uintptr_t addr, uintptr_t dst);

#endif
