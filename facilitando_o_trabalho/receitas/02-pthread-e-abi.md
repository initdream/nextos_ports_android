# 🤝 A Ponte de Vidro: Pthread & ABI (Bionic → Glibc)

Este é o componente mais invisível e mais perigoso de um port. O Android usa a biblioteca C **Bionic**, enquanto o NextOS usa a **Glibc**. Ambas implementam a `pthread` (threads), mas de formas incompatíveis.

## 1. O Conflito de Tamanho
O objeto `pthread_mutex_t` na Bionic tem **40 bytes**. Na Glibc, ele tem **48 bytes**.
*   **O Desastre:** Se a `libunity.so` (compilada para Bionic) aloca um mutex e o passa para a nossa `libpthread.so` (Glibc), a Glibc tentará escrever 48 bytes em um espaço de 40. **Resultado:** Corrupção de memória e Deadlocks aleatórios.

## 2. Nossa Solução: `pthread_bridge.c`
Em vez de deixar o jogo chamar a Glibc direto, nós interceptamos todas as chamadas de thread.
*   Nós usamos um **Mapa de Ponteiros**.
*   Quando o jogo pede para inicializar um mutex, nós alocamos um mutex da Glibc na memória real (heap) e guardamos o endereço dele dentro dos 40 bytes que o jogo nos deu.
*   Sempre que o jogo quiser dar `lock` ou `unlock`, nós pegamos esse endereço e chamamos a Glibc de forma segura.

## 3. Tipos que PRECISAM de Bridge:
*   `pthread_mutex_t`
*   `pthread_cond_t`
*   `pthread_rwlock_t`
*   `pthread_once_t`

## 4. O Problema do `pthread_t`
No Android ARM64, o `pthread_t` é apenas um ponteiro (8 bytes), o que felizmente é compatível com o Linux ARM64. Por isso, funções como `pthread_self()` ou `pthread_create()` costumam funcionar direto, sem ponte.

## 5. ABI de Sinais (sigaction)
A estrutura `sigaction` também difere. A Bionic espera uma máscara de sinais de 64-bit dentro de um espaço pequeno. Se você passar o `sigaction` da Glibc (que é enorme), você estoura a pilha do jogo. 
*   **Fix:** Sempre use o nosso wrapper `my_sigaction` que traduz os campos um por um.

---
*Dica: Se o jogo trava em "Deadlock" logo no início, ou se o carregamento de uma fase secundária congela a CPU em 100%, revise a sua ponte pthread.*
