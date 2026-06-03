# hollow-recon — STATUS (Hollow Knight / Unity 2020.2 IL2CPP → NextOS Mali)

Harness que carrega `libunity.so` via so-loader e dirige a inicialização do Unity,
pra portar Hollow Knight (Unity, Java-driven, sem `android_main`) ao Mali-450.

## ✅ O QUE JÁ FUNCIONA (provado no device .87, Mali-450)
1. **Load + relocate + resolve** do `libunity.so` (355 imports): **0 UNRESOLVED**.
   - Import table auto: **293 passthrough via dlsym(RTLD_DEFAULT)** + stubs Android-specific.
   - Alias bionic→glibc: `__errno`→`__errno_location`, `__assert2`→`__assert_fail`.
2. **`JNI_OnLoad`** roda limpo → retorna `0x10006`. Captura o contrato: **7 classes Java / 38 métodos nativos** (UnityPlayer 25 + Choreographer/Swappy/ARCore/Camera2/HFP/AudioVolume).
3. **`initJni(Context)` COMPLETA** — passa por TODO o init do Android Context
   (getPackageName, Intent/addFlags, ClassLoader/forName, StringBuilder, Scanner...).
   - jni_shim ciente de nomes (registry method-ID): dispatch por nome.
   - `GetJavaVM[219]` implementado (destravou o init profundo).
4. **Gráfico no Mali OK**: `egl_shim` (SDL2) cria **janela 1280x720 + contexto GLES2**
   ("Window created", "GL share-root context created"). EGL do Unity (20 fns) +
   ANativeWindow (6) mapeados → egl_shim.

## 🧱 ARQUITETURA descoberta
- **IL2CPP auto-load**: `libunity` importa 0 símbolos il2cpp; faz `dlopen("libil2cpp.so")`
  + dlsym em runtime. Nosso `dlopen` é passthrough REAL → **Unity carrega o il2cpp sozinho**
  (libil2cpp é auto-contido: deps só libc/m/dl/log). Integração potencialmente automática.
- **GL via eglGetProcAddress**: libunity importa 0 `gl*`; pega tudo via `eglGetProcAddress`
  (nosso egl_shim delega a SDL_GL_GetProcAddress → driver Mali real).

## 🧱 BLOQUEIO ATUAL
`nativeRecreateGfxState(api, surface)` **crasha imediatamente** (NULL deref dentro do
libunity, ANTES de qualquer chamada EGL). Causa: **engine/player não inicializado** —
está sendo chamado fora de ordem. A janela/EGL não é o problema (funcionam).

## 🔑 DESCOBERTA (fase 2): como o Unity lê os dados
Durante o init, o Unity chama via JNI:
```
GetMethodID(getAssets, ()Landroid/content/res/AssetManager;)
CallObjectMethod(getAssets)
NewStringUTF("bin/Data/boot.config")   <- 1o arquivo
```
→ **Unity lê os dados pelo `AssetManager` JAVA (JNI), NÃO pelo NDK AAssetManager.**
Como controlamos o JNIEnv, dá pra implementar `getAssets` + `AssetManager.open/openFd`
lendo dos arquivos REAIS extraídos. **Dados copiados pro device** em
`/storage/hollow-recon/assets/bin/Data/` (413M: data.unity3d 303M, global-metadata.dat
6.9M, boot.config, sharedassets*, Managed/).

## ▶️ PRÓXIMOS PASSOS
0. **AssetManager bridge no jni_shim** (o desbloqueio agora):
   - `getAssets()` → objeto AssetManager fake (tagged)
   - `AssetManager.open(path)` → InputStream lendo `ASSET_BASE/path` (read/close)
   - `AssetManager.openFd/openNonAssetFd(path)` → AssetFileDescriptor c/ fd real
     (data.unity3d 303M é mmap'd → precisa fd + offset + length)
   - byte[] handling (GetByteArrayElements/SetByteArrayRegion)
1. **Sequência de init correta** (UnityPlayer.java): reverse-engineer a ordem real das
   chamadas nativas. Provavelmente o engine init acontece no 1º `nativeRender` OU há um
   `UnityInitApplication*` a chamar antes do gfx. Decompilar UnityPlayer.java (2020.2) ajuda.
2. **Game data no device**: copiar `assets/bin/Data/` do APK (`data.unity3d`,
   `global-metadata.dat`, `sharedassets*`, `Managed/`) p/ o cwd — `nativeRender` precisa.
3. Driver: chamar a sequência {init engine → nativeRecreateGfxState → loop nativeRender}.
4. Quando o engine subir, o `dlopen(libil2cpp)` dispara → 1º frame.

## COMO RODAR
```
# device: /storage/hollow-recon/ com libunity.so + libil2cpp.so
systemctl stop emustation
cd /storage/hollow-recon && ./hollow-recon 2> recon.log   # cria janela SDL (precisa ES parado)
systemctl start emustation
```
Build: `tools/build-port.sh experiments/hollow-recon`. Tabela: `gen-unity-imports.sh`.
Logs de referência em `docs/`.
