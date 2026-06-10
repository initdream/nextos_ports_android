# 💣 Troubleshooting: Quando tudo explode

A regra nº 1 do porting é: **O jogo vai crashar.** O segredo é saber ler o que ele diz no último suspiro.

## 1. O Crash Handler (Seu melhor amigo)
Nosso framework (veja `ports/dysmantle/src/main.c`) inclui um handler de sinais (`SIGSEGV`, `SIGILL`, `SIGABRT`).

### Como ler o log de Crash:
```text
=== CRASH sig=11 addr=0x0 tid=1234 ===
  comm=UnityMain
  PC=0x7f88a12345 game.so@0x456789
  x30(LR) libunity.so@0xabc123
```
*   **sig=11 (SIGSEGV):** Acesso a memória inválida. Se `addr=0x0`, é um **Null Access**. Alguém tentou usar um ponteiro que não foi inicializado.
*   **PC (Program Counter):** Onde o crime aconteceu. O log já te dá o offset (`@0x456789`). Use `nm -DC game.so | grep 456789` (ou `objdump`) para ver que função é essa.
*   **x30 (Link Register):** De onde veio a chamada. Muito útil para ver quem chamou a função que crashou.

---

## 2. Stack Scan (Onde eu estava?)
O `stack scan` tenta encontrar endereços de retorno na pilha.
*   Se você vê uma sequência de endereços de `game.so`, você tem o caminho que o código percorreu.
*   Compare com o código fonte (se tiver) ou com o desassembly para entender a lógica.

---

## 3. Debugando com GDB (No Device)
Se o crash for silencioso ou intermitente:
1.  Pare o EmulationStation: `systemctl stop emustation`.
2.  Rode com gdb: `gdb --args ./meujogo`.
3.  Quando crashar, digite `bt` (backtrace).

---

## 4. O Mistério do "Som mas sem Vídeo"
Se o jogo está rodando (você ouve o som, os logs de lógica fluem) mas a tela está preta:

| Suspeito | O que checar |
|---|---|
| **EGL/SDL Context** | O `egl_shim` criou o contexto? O `SDL_GL_SwapWindow` está sendo chamado? |
| **Shaders** | A Mali-450 dá erro `L0005` (instruções demais) ou `P0004` (highp no fragment)? Cheque o `stderr`. |
| **Clear Color** | O jogo está limpando a tela com preto todo frame e não desenhando nada por cima? |
| **Z-Buffer** | Tente desabilitar o Depth Test globalmente no `stubs.c` para ver se a imagem aparece. |

---

## 5. BRK Traps (Técnica Avançada)
Usada no **Dysmantle**. Se você quer saber por onde o código passa mas o `printf` é lento demais ou o código é multithread:
1.  Insira um `arm_brk(offset, "tag")` no `main.c`.
2.  Isso força um sinal de interrupção naquele exato endereço.
3.  Nosso `brk_handler` captura isso e loga os registradores sem travar o jogo.

---
*Dica: Se o erro for `Undefined Symbol`, o problema está no `imports.gen.c`. Se for `SIGILL`, você pode estar tentando rodar uma instrução ARM v8.4 em um Cortex-A53 (v8.0).*
