# 🎭 Fake JNI e Android Shim

Mesmo que o código principal do jogo seja em C++ (`.so`), ele ainda tenta conversar com o "mundo Java" do Android. Como não temos uma VM Android completa (ART), nós criamos um "Teatro de Fantoches" chamado **Fake JNI**.

## 1. O que é o JNI?
O JNI (Java Native Interface) permite que o C++ chame funções Java (como "me dá o ID do device") e vice-versa. 

## 2. Como enganamos o jogo?
No arquivo `jni_shim.c`, nós implementamos as funções fundamentais da JNI:

*   **`CallObjectMethod`:** Quando o jogo pergunta `getClassLoader()` ou `getAssets()`, nós retornamos um ponteiro "fake" que diz: "Aqui está o seu objeto".
*   **`GetStringUTFChars`:** Quando o jogo pede uma string (como o caminho da pasta de saves), nós retornamos uma string do Linux (ex: `/home/root/ports/jogo/saves`).

## 3. O Shim de Atividade
Muitos jogos usam o `android_native_app_glue`. Nós simulamos os eventos dessa "cola":
*   `APP_CMD_START`: Disparado quando o jogo começa.
*   `APP_CMD_INIT_WINDOW`: Disparado quando o SDL2 cria a janela.
*   `APP_CMD_GAINED_FOCUS`: Faz o jogo acreditar que o usuário clicou na tela.

## 4. Substituindo Assets
Em vez de ler o arquivo `.apk`, nós redirecionamos as chamadas de arquivo.
*   Se o jogo pede `AAssetManager_open("config.bin")`, nós buscamos o arquivo `assets/config.bin` na pasta do port.

## 5. System Properties
O jogo pode chamar `__system_property_get("ro.product.model")` para saber se está em um celular potente. 
*   **Fix:** Retorne sempre um modelo conhecido (ex: `Pixel 6`) para evitar que o jogo tente rodar em modo de "baixa qualidade" ou trave por achar o hardware desconhecido.

---
*Dica: Se o jogo trava logo após o `JNI_OnLoad`, verifique se ele está tentando chamar algum método Java que você ainda não implementou no seu Fake JNI.*
