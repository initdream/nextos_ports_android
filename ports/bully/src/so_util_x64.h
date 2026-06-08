/* so_util_x64.h (DEVICE compat) -- o jni_shim foi escrito p/ o so_util x86_64
 * multi-módulo (Module/so_symbol/hook_x64). No device usamos o so_util AArch64
 * single-module (global, do reVC). Mapeamos: libGame é o ÚLTIMO módulo carregado
 * -> so_find_addr(name) acha os símbolos dele; hook_x64 -> hook_arm64. */
#ifndef SO_UTIL_X64_COMPAT_H
#define SO_UTIL_X64_COMPAT_H
#include "so_util.h"
typedef int Module;
extern Module mod_game, mod_cxx;
#define so_symbol(m, name) so_find_addr(name)
#define hook_x64(addr, dst) hook_arm64((uintptr_t)(addr), (uintptr_t)(dst))
#endif
