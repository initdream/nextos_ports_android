# 🎮 Domando a Mali-450 (GLES 2.0 / Utgard)

A Mali-450 é uma GPU "Utgard". Ela é valente, mas tem limites rígidos de OpenGL ES 2.0. Para jogos modernos de Android, precisamos aplicar várias "receitas" de Alquimia Gráfica.

## 1. O Problema da Precisão (highp vs mediump)
Muitos shaders de Android assumem que podem usar `highp` (precisão de 32-bit) no **Fragment Shader**. A Mali-450 suporta `highp` apenas no **Vertex Shader**.
*   **Sintoma:** O shader falha ao compilar com erro `P0004`.
*   **Solução:** Interceptar o `glShaderSource` e substituir `highp` por `mediump` via regex ou `strstr`.

## 2. Coordenadas Z e o Clipping (Fix do im2d)
Em engines como **Unity** ou **GameMaker**, coordenadas 2D (HUD/Menus) podem ser enviadas com um valor `Z` que a Mali interpreta como fora do plano de visão, resultando em tela preta (mesmo com o jogo rodando).
*   **Receita (Vice City):**
    ```glsl
    // Original
    gl_Position.xyz *= gl_Position.w;
    // Fix
    xy *= w; z = 0.0;
    ```
    Isso força os elementos 2D para a frente, garantindo que apareçam.

## 3. Formatos de Textura: O Bug do LUMINANCE
A Mali-450 tem um comportamento estranho com `GL_LUMINANCE`. Ela lê como `(L, L, L, L)`, onde o Alpha vira o brilho, resultando em texturas transparentes ou escuras.
*   **Solução:** No `glTexImage2D`, se o formato for `LUMINANCE`, converta os dados na CPU para `RGBA8888` antes de enviar para a GPU.

## 4. O Fix do "Jimmy Vestido" (glClear & FBO)
No port do **Bully**, as roupas do personagem sumiam. Descobrimos que a Mali-450 bugava ao forçar o Clear do buffer de cor dentro de um **FBO** (Render-to-Texture).
*   **Regra:** Force o `GL_COLOR_BUFFER_BIT` no `glClear` apenas quando estiver desenhando na TELA principal. Se estiver dentro de um FBO (usado para compor a roupa do personagem, por exemplo), respeite o que o jogo pede.

## 5. Limite de Instruções (L0005)
A Mali-400/450 GP tem um limite de cerca de 512 palavras de instrução no Vertex Shader.
*   **Sintoma:** `MAX_LIGHTS 8` em GTA Vice City estoura o limite.
*   **Solução:** Reduzir constantes. Mudar `MAX_LIGHTS 8` para `4` reduziu o shader para um tamanho que a Mali aceita, sem perda visual perceptível no device.

---
*Lembre-se: A Mali-450 não suporta GLES 3.0. Se o jogo exige `gl_FragData[n]` (MRT), você precisará de um shim para colapsar isso em uma única saída ou desabilitar a iluminação deferida.*
