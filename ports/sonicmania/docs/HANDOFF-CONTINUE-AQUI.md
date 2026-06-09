# 🦔 SONIC MANIA PLUS → Mali-450 (so-loader) — HANDOFF p/ próxima sessão

## TL;DR do estado (2026-06-09)
- ✅ **IMAGEM 100%**: tela-título Sonic Mania Plus renderiza inteira, linda, ESTÁVEL (Logos Netflix/SEGA → Title), ~42fps, 0 crash. Ver `docs/sonic_mania_title_COMPLETO.png`.
- ✅ **INPUT PROPAGA 100%** pro `RSDK::controller[]` (verificado por dump: START→keyStart.press com edge perfeito em controller[0] e [1]).
- ❌ **TÍTULO NÃO AVANÇA** com o input (nem auto, nem controle real do o autor). **ESSA É A MISSÃO: descobrir por quê e destravar → jogável.**
- 🔊 ÁUDIO: backend Oboe→opensles_shim→SDL2 wirado, MAS `onAudioReady` do engine lança exceção C++ (SIGABRT) → pump DESABILITADO. (2ª prioridade.)

## O QUE FUNCIONA (não mexer)
1. **Loader 2-módulos**: dlopen libc++_shared via so_load + so_snapshot_symbols (main.c). Engine resolve com tabela combinada.
2. **Render GLES2**: engine importa só glUseProgram (shaders eram do Java!). `gl_trace.c` cria programa de blit GLES2 próprio (pos@0, UV@1, sampler unit0) + intercepta glUseProgram. vsync OFF (SDL_GL_SetSwapInterval 0).
3. **startEngine 16 args** + step=RunGameLoop(float 60.0) + setGameRunning(1).
4. **INPUT (a grande sacada)**: SDL→`OnKeyEvent(env,thiz,keyCode,pressed)`→`SetKeyEvent`(0x17d884) guarda no `Controller::Instance`(0x4a6e60). **Chamar `Java_..._GameControllerManager_onKeyboardConnected`(0x47a210) no startup REGISTRA o device** → engine dirige `CopyToSlot`(0x17d9bc) DEPOIS do ClearInput → controller[0]+[1] recebem .down+.press.
   - Mapa keycode Android→RSDK: tabela em 0x102904. START108→RSDK12→keyStart, A96→RSDK0→keyA, UP19→RSDK14→keyUp. controller stride=144. base=*(text_base+0x490e18). text_base=g_copyslot-0x17d9bc.
   - ⚠️ NÃO chamar onControllerConnected (crasha sem controller real).

## O MURO (a missão): título não avança
Decomp: `~/sonic-build/Sonic-Mania-Decompilation/SonicMania/Objects/Title/TitleSetup.c`
- `TitleSetup_State_WaitForEnter` avança com `ControllerInfo->keyA/B/C/X/Y/Z/Start/Select.press` OU `anyClick`(touch release) OU `Unknown_anyKeyPress` → `RSDK.SetScene("Presentation","Menu")`. Após 800 frames sem input → vídeo de atração.
- `ControllerInfo` = controller[0] (CONT_ANY) = O QUE EU ESCREVO. Logo deveria avançar. NÃO avança.

### Hipóteses (testar nesta ordem)
- **A) Título TRAVADO antes do WaitForEnter.** Estados: Wait→AnimateUntilFlash→FlashIn→WaitForSonic→SetupLogo→WaitForEnter. Talvez WaitForSonic espera o TitleSonic chegar (animação/spline travada). **DIAGNÓSTICO: ler `TitleSetup` global (0x4a83c8) campo `state` (offset do RSDK_THIS object — comparar ponteiros de função dos states) OU hookar (hook_arm64 existe no so_util) os TitleSetup_State_* p/ ver onde para.** Se NÃO chega no WaitForEnter → intro travada (achar qual objeto/anim).
- **B) `ControllerInfo` lido pelo jogo ≠ controller[] que escrevo.** Binding via tabela de API pode apontar p/ outro array. Verificar: achar o símbolo/global `ControllerInfo` real e comparar com base=*(text_base+0x490e18).
- **C) Build mobile/Netflix usa TOUCH ("tap to start") OU título custom.** Testar `OnTouchEvent`(0x17d154)→SetTouchEvent (mandar touch down+up = anyClick). Tb setar `Unknown_anyKeyPress` global.

## COMO BUILDAR/RODAR
- Build: `cd ~/nextos_ports_android/ports/sonicmania && bash build.sh` (toolchain NextOS Amlogic-old aarch64).
- Deploy: `scp ports/sonicmania/sonicmania root@<DEVICE_IP>:/storage/roms/ports/sonicmania/` (senha removida). **scp do path ports/sonicmania/, NÃO da raiz.**
- Rodar: `ssh root@<DEVICE_IP>` → `systemctl stop emustation; sleep 2; cd /storage/roms/ports/sonicmania; ./sonicmania > /tmp/sonic.log 2>&1`. Restart emustation depois.
- Screenshot: `dd if=/dev/fb0 of=/tmp/fb.raw bs=1M count=4` → puxar → PIL frombytes RGBA 1280x720, trocar B/R.
- .so de referência (símbolos/disasm): `/tmp/sm-so/lib/arm64-v8a/libsonicmania.so`. Toolchain nm/objdump em ~/NextOS-Elite-Edition/build.*/toolchain/bin/.

## ARQUIVOS-CHAVE
- `src/sonic_jni.c` = driver (SDL, GLES2 ctx, JNI, input, loop). TEM auto-aperto repetido START+A (f%60) — REMOVER quando resolver.
- `src/gl_trace.c` = blit shader + wrappers GL.
- `src/shims.c` = bionic/cxx/dlopen-OpenSLES shims.
- `src/opensles_shim.c` = ponte audio (pump desabilitado no jni por crash).
- Memória completa: ~/.claude/projects/-home-felipe/memory/project_sonic_mania_soloader_mali450.md
