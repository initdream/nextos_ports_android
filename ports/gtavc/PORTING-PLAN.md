# GTA Vice City Android → NextOS Mali-450 (armhf so-loader) — PORTING PLAN

Base de port: `~/gtavc-build/refs/gtactw_vita` (e `gtasa_vita` p/ detalhes 3D).
Estratégia: portar o loader Vita → Linux armhf, trocando a camada de plataforma
(sce*/vitaGL/kubridge/fios) por SDL2 + libMali (EGL/GLES) + glibc + nosso so_util armhf.

Alvo: `libGTAVC.so` (armeabi-v7a) + OBB `main.11.com.rockstargames.gtavc`.
Engine = NVIDIA app framework (NVEvent) + RenderWare mobile — MESMA do CTW.
JNIEnv vtable é ARM 32-bit → offsets do gtactw_vita batem direto no armhf. ✅

## Arquivos (de gtactw_vita/loader/ → ports/gtavc/src/)

| Arquivo | O que é | Port p/ Linux |
|---|---|---|
| `so_util.c` | loader ELF | ✅ **JÁ FEITO** (nosso armhf, REL relocs) |
| `main.c` | orquestração + init | **REESCREVER**: tirar sce*/kubridge/fios/vglInit; pôr SDL_Init + criar janela+GL (egl_shim) + so_load/relocate/resolve + patches + jni_load |
| `jni_patch.c` | JNIEnv fake + métodos NVEvent (a "montanha") | **PORTAR ~15 funcs**: trocar sce*/vgl→SDL2. Vtable/offsets mantêm (ARM32). Detalhe abaixo |
| `openal_patch.c` | áudio (OpenSL→OpenAL) | portar p/ openal-soft do device OU nosso opensles_shim |
| `mpg123_patch.c` | mp3 streaming (rádio) | portar (mpg123 existe no /usr/lib32) |
| `opengl_patch.c` | fixups GL (BGRA→RGBA etc.) | mostrar quase portável; checar highp/precision p/ Utgard |
| `default_dynlib[]` (em main.c) | tabela de imports | comparar com nosso imports.gen.c (243 símbolos); usar a do gtactw_vita como referência + vc_impl.c p/ os 20 |

## jni_patch.c — funções de plataforma a trocar (sce/vgl → SDL2)
- `InitEGLAndGLES2` → criar contexto GL via SDL (`SDL_GL_CreateContext`) — NÃO forçar driver (auto sobe no Mali fbdev, validado no CTW)
- `swapBuffers` → `SDL_GL_SwapWindow`
- `makeCurrent`/`unMakeCurrent` → `SDL_GL_MakeCurrent`
- `GetGamepadType/Buttons/Axis` → `SDL_GameController*`
- `GetDeviceInfo/Type/Locale` → valores fixos (Linux/genérico) + locale do device_info
- `getAppLocalValue("STORAGE_ROOT")` → pasta do port (`/storage/roms/ports/gtavc`)
- `FileGetArchiveName` → caminho do OBB (main.11.com.rockstargames.gtavc.obb)
- `GetStringUTFChars/NewStringUTF/NewGlobalRef/GetMethodID/CallXMethodV/GetEnv/NVThreadGetCurrentJNIEnv` → portáteis (lógica pura, mantém)
- `PlayMovie/StopMovie/IsMoviePlaying` → stub (sem decoder; vídeos opcionais) OU ver gtasa
- relógio → **nextclock.so** (LD_PRELOAD) ou integrar no resolve

## Build (armhf!)
- Host loader DEVE ser armhf (roda código 32-bit no mesmo processo).
- Toolchain: `armv8a-emuelec-linux-gnueabihf-gcc` (NextOS, o do nextclock).
- Linkar SDL2 + EGL/GLESv2 armhf (sysroot do toolchain OU /usr/lib32 do device).
- Saída: ELF armhf → /storage/roms/ports/gtavc/gtavc

## Ordem de execução (próximas sessões)
1. main.c Linux (backbone SDL2 + so_util + chama patches/jni_load)
2. jni_patch.c portado (vtable + ~15 métodos SDL2)
3. opengl_patch + audio (openal/mpg123)
4. Makefile/CMake armhf + 1ª compilação (resolver erros de link)
5. Deploy device + bootar (log: JNI_OnLoad→NVEventAppInit→MakeCurrent) + iterar stubs
6. Tela/áudio/controle + reduzir se preciso + empacotar R2

## Reuso pronto
- `~/gtactw-build/nextclock.so` (relógio) — copiar
- `ports/gtavc/src/vc_impl.c` — os 20 UNKNOWN (haptics stub, ctype, __sF, math/libc)
- `ports/gtavc/src/so_util.c` — loader armhf
- launcher padrão PORT_32BIT=Y (igual CTW)
