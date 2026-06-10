# 🛠️ Facilitando o Trabalho: O Ecossistema de Ports NextOS

Bem-vindo ao centro de inteligência do projeto **nextos_ports_android**. Este espaço foi criado para transformar o "porting" de uma arte mística em uma engenharia documentada e replicável. 

Aqui, consolidamos o conhecimento acumulado nos sucessos inéditos de **Bully: Anniversary Edition** e **Sonic Mania Plus (Netflix)** — os dois marcos portados 100% do zero usando este framework — além do progresso em **Dysmantle**, **Cuphead** e as lições de **GTA Vice City**.

---

## 📖 O Manifesto do Portador (Mali-450 / Kernel 3.14)

Portar para o Amlogic-old não é apenas "fazer o código rodar"; é negociar com o hardware. Nossos pilares são:

1.  **Direct-to-Hardware:** Não emulamos. Carregamos o `.so` nativo do Android (`ARM64`) e o fazemos acreditar que ainda está em casa.
2.  **A Ponte de Vidro (Bionic → Glibc):** O Android fala Bionic; nós falamos Glibc. Nossa missão é construir os "stubs" e "bridges" (especialmente `pthread`) para que essa conversa não quebre.
3.  **Alquimia de Shaders:** A Mali-450 é uma GPU Utgard (GLES 2.0 estrito). Transformamos shaders modernos e pesados em algo que a "vovó Mali" consiga desenhar sem engasgar.
4.  **Guardião da VRAM:** Temos pouca memória de GPU. Reduzimos no upload, limpamos mips e ignoramos o que não é essencial para manter o jogo vivo.

---

## 🗺️ Guia de Navegação

### 1. [Iniciando um Port do Zero](receitas/01-iniciando-um-port.md)
O fluxo do `new-port.sh`, como ler o `imports.gen.c` e o que atacar primeiro.

### 2. [A Ponte Pthread e ABI](receitas/02-pthread-e-abi.md)
Por que o `pthread_mutex` do Android crasha o Linux e como nossa `pthread_bridge` resolve isso.

### 3. [O Domador de Mali (GLES 2.0)](receitas/03-domando-a-mali450.md)
Fixes de `glClear`, conversão de texturas `LUMINANCE`, `highp` para `mediump` e o famoso "Fix do Jimmy" (render-to-texture).

### 4. [Fake JNI e Android Shim](receitas/04-fake-jni-shim.md)
Como enganar o jogo para ele achar que tem um sistema Android por trás (Package Name, Paths, Assets).

### 5. [Troubleshooting: Quando tudo explode](troubleshooting/01-diagnostico-de-crash.md)
Como usar o Crash Handler, ler o Stack Scan e usar o `gdb` para achar o exato momento do `Null access`.

---

## 🚀 Dica de Ouro: O Segredo do Vídeo
Se o jogo carrega, tem som, mas a tela está preta, 90% das vezes é:
*   **Depth Buffer:** A Mali-450 às vezes se perde com o Depth Test em 2D.
*   **Shader Compilation:** Verifique o log. Se o shader não compilou, nada desenha.
*   **Z-Clipping:** Coordenadas `Z` fora do range (especialmente em Unity/GameMaker).

---
*Este ecossistema cresce a cada port. Se você descobriu um fix novo, documente aqui!*
