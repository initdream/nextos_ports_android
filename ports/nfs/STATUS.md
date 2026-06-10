# NFS Most Wanted (2012) → NextOS Mali-450 (so-loader armhf)

**Jogo:** Need for Speed Most Wanted, `com.ea.games.nfs13_row` v1.3.128 (EA/Firemonkeys/Ironmonkey).
**APK:** `~/Downloads/need-for-speed-most-wanted-v1.3.128.apk` (19.5MB).
**OBB/dados:** `~/Downloads/need-for-speed-most-wanted-v1.3.128-cache-apkvision.zip` → `main.1003128.com.ea.games.nfs13_row.obb` (623MB).

## ✅ Análise de viabilidade (PROMISSOR)
- **GLES2** (libGLESv2 + libEGL, zero GLES3) → passa no filtro nº1 do Mali-450 Utgard. 🟢
- **ABI armhf** (armeabi-v7a, ELF32 ARM, NDK r18) → caminho 32-bit do so-loader (igual GTA CTW/VC, runtime /usr/lib32). 🟢
- **Entry = JNI_OnLoad** em libapp.so + Java `com.ea.ironmonkey.GameActivityMain.nativeOnCreate` → padrão GameActivity+JNI (dominado no DYSMANTLE/Bully). 🟢
- **Risco real = PERFORMANCE** (open-world 3D de 2012 num Mali-450 fraco). Só o teste dirá. 🟡

## Módulos (multi-módulo, igual DYSMANTLE)
- `libapp.so` (11MB) = ENGINE principal (JNI_OnLoad aqui)
- `libNimble.so` (13KB) = bridge JNI pequeno (GameActivity)
- `libfmodex.so` + `libfmodevent.so` = áudio FMOD (middleware comercial)
- `libc++_shared.so` = STL
- NEEDED: libc++_shared, libfmodex, libfmodevent, libGLESv2, libEGL, liblog, libjnigraphics, libNimble, libc/m/dl

## Recon de símbolos (new-port-arm.sh)
- 540 importados (UND), **352 auto-resolvidos (65%)**, 188 a implementar.
- 145 GLES, 48 pthread, 128 passthrough, 30 cxx/abi/log.
- **188 UNKNOWN = maioria libc/posix trivial** (dlopen/dlsym/clock/chmod/fork/feof/ferror/getaddrinfo/getauxval/...) → passthrough glibc.
- Específicos a tratar: `AndroidBitmap_*` (libjnigraphics), `FMOD_*` (áudio), dlopen/dlsym (nossa versão do loader).

## Roadmap (fases, estilo DYSMANTLE)
- [ ] **F0 — build:** loader armhf (so_util REL do gtavc-build) + wire dos 188 (libc→glibc), compila.
- [ ] **F1 — load:** carrega libapp+libNimble+deps, relocs OK, JNI_OnLoad roda sem crash.
- [ ] **F2 — boot JNI:** replicar GameActivityMain.nativeOnCreate + JNI env (igual DYSMANTLE GameActivity struct). Pegar o init da engine.
- [ ] **F3 — GLES2/EGL:** EGL→SDL2 Mali fbdev, contexto GLES2, primeiro frame.
- [ ] **F4 — dados:** montar o OBB (623MB) no path que a engine espera.
- [ ] **F5 — áudio:** FMOD (rodar real ou bridge).
- [ ] **F6 — render/perf:** otimizar p/ Mali-450; controle USB; empacotar.

## Notas/pegadinhas (da experiência)
- ⚠️ Canary bionic tpidr+0x28: pad TLS 256B `_Thread_local` no exe (vale p/ todo so-loader). Ver [[DYSMANTLE]].
- ⚠️ armhf: runtime /usr/lib32 (glibc 2.43 32-bit + libMali.m450 + SDL2), launcher PORT_32BIT=Y. Kernel 3.14 sem time64 → shim nextclock.so (time32) como no GTA CTW.
- ⚠️ Buildar sempre da pasta do port. ES mascarado p/ testes.
- so_util armhf: `~/gtavc-build/loader/so_util.c` (REL relocs).
