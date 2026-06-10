# NFS Most Wanted — RESUMO p/ próxima sessão (handoff)

## ONDE PAROU (~95% do boot — engine boota, LÊ o OBB, ENTRA no render loop; trava na construção de objetos de asset CORROMPIDOS)

### Rodar (device nextos-87, ES mascarado, Mali fbdev)
```
cd ~/nextos_ports_android/ports/nfs && ./build.sh && scp -q nfs nextos-87:/storage/roms/nfs/
ssh nextos-87 'cd /storage/roms/nfs && export LD_LIBRARY_PATH="/usr/lib32:/storage/roms/nfs" \
  SDL_VIDEODRIVER=mali NFS_INIT=1; timeout 70 ./nfs 2>&1 | tail -50'
```
✅ **NFS_SKIPCTOR NÃO é mais necessário** — o shim de sigaction (abaixo) fez os 3 últimos ctors
(186/187/188 = detecção de CPU) completarem. init_array roda 100%.
OBB (623MB) já no device: `/storage/roms/nfs/data/Android/obb/com.ea.games.nfs13_row/main.1003128.com.ea.games.nfs13_row.obb`
Recon local: `~/Downloads/nfs-analysis/lib/armeabi-v7a/` e `~/Downloads/obb_extract/com.ea.games.nfs13_row/`
objdump: `~/NextOS-Elite-Edition/build.NextOS-...-Amlogic-ng.aarch64-4/toolchain/bin/armv8a-emuelec-linux-gnueabihf-objdump`

### O QUE JÁ FUNCIONA (não re-investigar)
- Build armhf, multi-módulo (libc++_shared+libNimble+FMOD+libapp), 0 unresolved.
- init_array com NFS_INIT=1 + **malloc PADDING +64B** (NFS_PAD, default 64) — CONFIRMADO necessário:
  com PAD=0/16 crasha cedo em `malloc(): invalid size`. NÃO mexer.
- JNI/GameActivity boot completo + render loop (RunLoop tick).
- **softfp ABI shim** + **stat/fstat/statfs → *64** (NOVO: o stat antigo da glibc lê st_size 32-bit
  no offset errado; rotear p/ stat64/fstat64 corrige o tamanho do OBB). + fseek/ftell/lseek/lseek64 hooks.
- A engine ABRE+LÊ o OBB: é um ZIP (PK\3\4, magic 0x504b0304). Header 1028B + central dir no fim.
  Antes do crash: **4826 seeks, ~375KB lidos** (varre o índice ZIP dos 2411 arquivos).

### O MURO REAL (causa-raiz ainda aberta) — OBJETOS C++ CORROMPIDOS no parse de asset
A engine, ao parsear o índice/tweaks do OBB, constrói objetos C++ **estruturalmente corrompidos** e
faz `dynamic_cast`/RTTI neles → crash. DIAGNÓSTICO PRECISO desta sessão:

1. **Crash = `__dynamic_cast` da libc++** (`__dynamic_cast@0x6fd85`, off +0x36 = `ldr.w ip,[r0,#0x18]`).
   O objeto-fonte tem `vtable[-1]` (slot de typeinfo) apontando p/ **CÓDIGO** (libapp+0x47a920, um `bx lr`)
   em vez de um `__class_type_info` real → libc++ desreferencia typeinfo lixo e faz `blx` num ptr inválido.
   = ponteiro de vtable do objeto **deslocado/corrompido** (lê função virtual como typeinfo).

2. Tipos de lixo observados nos slots de ponteiro dos objetos:
   - `0xe12fff1e` = opcode `bx lr` (lê 1ª instrução de função como ponteiro de typeinfo).
   - `0x313d5448` = **ASCII "HT=1"** (bytes 48 54 3d 31) — dados de TWEAK/config caíram onde devia ter
     ponteiro de função! (ex: trecho de "...HEIGHT=1" / "WEIGHT=1"). PROVA de que o parser põe bytes de
     asset onde espera struct/objeto → **leitura de estrutura errada** (offset/contagem/tamanho).

3. A corrupção é PROFUNDA: guardar `__dynamic_cast` faz o MESMO lixo reaparecer 1 nível abaixo (no
   handler de typeinfo `__do_dyncast`, que faz `blx r0` com r0="HT=1"). Band-aids de RTTI só adiam.

### DEFESAS já implementadas (defense-in-depth, NÃO são o fix — mas avançaram o boot)
Em `src/imports.c` + `src/main.c`:
- **my_dynamic_cast** (shim na tabela + **inline-hook na entrada do __dynamic_cast** p/ pegar a recursão
  interna da libc++ que não passa pela GOT da libapp): valida a cadeia sub→vtable→typeinfo→ti_vt e que
  o handler `ti_vt[0x18]` é EXECUTÁVEL (via /proc/self/maps cacheado); senão retorna NULL. Trampolim
  com os 8 bytes originais p/ rodar o dcast real (`g_dyncast_tramp`). `nfs_install_dyncast_hook()`.
- **DCAST-REC** no crash_handler: se faultar em `__dynamic_cast+0x36`, força retorno NULL pulando p/ o
  epílogo (+0xa6, `mov r0,r4; add sp,#64; pop`) com r4=0. (NFS_NODCASTREC=1 desliga.)
- **mem_readable** à prova de fault: sigsetjmp(g_probe_jmp)+leitura guardada; crash_handler honra
  g_probe_armed (siglongjmp). (Substituiu o probe por pipe que era furado.)
- crash_handler robusto: regiões de TODOS os módulos (maps), hexdump @PC só se mapeado (g_pc_mapped),
  stack scan resolve retornos em qualquer região r-x.

### ✅ RESOLVIDO nesta sessão — sigaction ABI (era a causa dos ctors 186/187/188 crasharem)
Os 3 últimos ctors são detecção de CPU (probes crypto/`mrrc` CNTVCT que dão SIGILL e são
capturados). Eles preenchem um `struct sigaction` **BIONIC** (16B: handler@0, mask@4 4B, flags@8,
restorer@12) e chamam a `sigaction` da **GLIBC** (sa_mask=128B → flags@132, restorer@136) → glibc
lia flags/restorer LIXO → handler de SIGILL retornava p/ `0xba`. **FIX (src/imports.c):
my_sigaction + my_sigprocmask** traduzem o struct/sigset bionic↔glibc (dropam SA_RESTORER/
SA_THIRTYTWO; glibc põe o restorer certo). NFS_NOSIGSHIM=1 desliga. → ctors completam, sem SKIPCTOR.
**OBS: isso NÃO consertou a corrupção de asset** (mesma cadeia RTTI; lixo mudou HT=1→TDBG).

### 🎯🔑 DESCOBERTA — o muro é o GERADOR DE SHADERS, não o parse do OBB!
O `dynamic_cast` que crasha (em `0x46b6fc`, dentro de `0x46b220`) é:
**`dynamic_cast<im::isis::shadergen::UniformNode*>( (im::isis::shadergen::Node*)sub )`**
(type_infos resolvidos: src=`N2im4isis9shadergen4NodeE`, dst=`N2im4isis9shadergen11UniformNodeE`).
- `sub` = `[fp-32]`. O loop em `0x46afe0` itera um array de **nós de grafo de shader** (`[sl+24]`,
  índice `[r0+r8<<2]`) e faz dcast de cada Node→UniformNode. UM nó está SELVAGEM (vtable lixo).
- `0x47a920` (que aparecia como "typeinfo lixo") é na verdade a **vtable legítima de um type_info**
  da libapp (o type_info @0xab36a4 tem [0]=0x47a920) — confirma que o nó selvagem tinha
  `vtable[-1]` apontando p/ uma vtable de type_info em vez de um OBJETO type_info.
- **Implicação**: a engine gera shaders no load (provável p/ GLES2/Mali). O grafo de shader tem um nó
  inválido — misparse dos dados de shader OU um tipo de nó cujo construtor/factory não rodou (vtable
  não setada) OU RTTI cross-módulo quebrado no factory de nós.
- **Refino (ring de dcast no crash)**: só ~2-3 dcasts antes do crash. A cadeia:
  (1) `libapp+0x46addc` faz `dcast<Node>(obj=0x47c6858)` — obj VÁLIDO, vtable=libapp+0xab3830.
  (2) recursão INTERNA da libc++ (caller na libc++) caminha a hierarquia de Node e CRASHA
      (handler faz `blx r0`=lixo "HT=1").
  (3) `libapp+0x46b700` faz `dcast<UniformNode>(sub=NULL)` → retorna NULL (ok, array tem nó nulo).
  Ou seja: castar um objeto VÁLIDO (0x47c6858, typeinfo @0xab3854) p/ Node crasha caminhando a
  hierarquia de classes-base → **um type_info BASE da cadeia está corrompido** (vtable do type_info
  = região "HT=1").
- ❌ **DESCARTADO: bug de relocação.** libapp usa REL padrão (DT_REL 388264B, RELCOUNT=45699, tudo
  em `.rel.dyn` que o so_relocate processa 100%). SEM packed/RELR/DT_ANDROID_REL. Então os type_infos
  ESTÃO relocados certos. A corrupção é RUNTIME/heap (consistente c/ PAD mudar o crash).
- **Próximo passo concreto (RUNTIME)**: dumpar NO DEVICE, no momento do dcast que crasha, a cadeia
  real: obj(0x47c6858)→*obj=vtable→vtable[-1]=typeinfo→percorrer __base_info. Achar o type_info-base
  cuja vtable cai FORA das faixas de módulo (= o corrompido). Aí ver QUEM escreveu por cima (provável
  overflow de heap de um buffer vizinho, OU o objeto foi liberado e o slot reusado). Já tem
  g_dcring + dump no crash_handler; estender p/ caminhar a cadeia de type_info e logar cada base.
- Investigar tb: o nó NULO no array (entry #3, `dcast(NULL)` em 0x46b700) — o array de nós de shader
  tem entradas nulas e/ou selvagens. Ver a construção do array (`this->[24]`, callers 0x3de064 etc).
- Hipótese de heap: algum buffer de shader/string transborda e esmaga a .data.rel.ro? NÃO — .data.rel.ro
  é do módulo (read-only após load idealmente). Mais provável: o OBJETO (nó) é que é selvagem/reusado,
  e por sorte teve vtable plausível; ou um buffer de nós transbordou esmagando nós vizinhos.

### (antigo) caracterização genérica do ponteiro selvagem (ainda válida)
- O objeto passado ao dynamic_cast tem vtable apontando p/ lixo. Os valores ("HT=1", "TDBG") são
  **ASCII e MUDAM** conforme PAD/ctors (não são tags fixas; NÃO existem em libapp nem no OBB) →
  o ponteiro do OBJETO/vtable é SELVAGEM, aponta p/ memória arbitrária com dados de asset.
- **Depende do layout do heap**: NFS_PAD=64 → crash no RTTI do parse (consistente). PAD=256 → chega
  ao render loop e crasha em addr=0x4 (libc). PAD=128/512 → outros. Ou seja, é **overflow de heap /
  misparse** que o padding só REALOCA, não elimina. (PAD=0/16 crasha cedo em malloc invalid size.)
- Hipótese principal: a engine lê um array de PONTEIROS de objeto do índice do OBB e uma entrada
  aponta p/ DADOS de asset em vez de um objeto construído (misparse de offset/tamanho), OU um
  overflow de heap esmaga a vtable de um objeto vizinho.
- **Próximo passo**: instrumentar `0x46b220` (processa cada elemento; chamado no loop de
  `0x46afe0`, indexa array `[r0+r8<<2]`) e a construção do objeto — logar o ponteiro do elemento +
  1as palavras p/ achar a 1ª entrada ruim e de qual offset do arquivo veio. Cadeia (pilha):
  `0x46afe0 ← 0x3de064 ← 0x46b958 ← 0x43c6fc ← 0x5643cc ← 0x466f28 ← 0x3eabdc ← 0x3d4808`.
- Suspeitar: leitura byte-a-byte do índice (vi 4826 seeks perto do EOF lendo 1 byte/vez) — formato
  CUSTOM da EU (não ZIP padrão); algum campo lido com tamanho/sinal errado desalinha tudo.

### (histórico) ctor 186 — detalhes da investigação que levou ao fix de sigaction
init_array tem 189 ctors (0-188); **186/187/188 são os 3 ÚLTIMOS** (init de subsistemas do app).
- **ctor 186 @ libapp+0x7433c = DETECÇÃO DE CPU** (ARM mode): instala sigaction(SIGILL)+sigsetjmp e/ou
  lê getauxval p/ montar flags de feature (AES/PMULL/SHA/CRC32) num global, e seleciona rotinas
  otimizadas por feature.
- Por que crasha (e era pulado): **PC=0xba (SIGILL), chama um ponteiro de função LIXO (0xba)** —
  provável rotina feature-selected (CRC32/hash/memcpy HW) cujo ponteiro não foi setado.
- Já tentei: **getauxval shim** força AT_HWCAP bit 0x1000 (NFS_NOAUXHACK desliga) → ctor 186 PEGA o
  caminho sem-probe (lê HWCAP2=0x1f real: AES/PMULL/SHA1/SHA2/CRC32) — CONFIRMADO via log — **mas
  AINDA crasha em 0xba**. Então o 0xba não é o probe SIGILL; é uma chamada indireta depois das flags.
- Também: nosso init-recovery (so_execute_init_array+crash_handler) só auto-pula SIGSEGV/SIGBUS, NÃO
  SIGILL → por isso 186 precisa de SKIPCTOR explícito (a recovery não o pega).
- **HIPÓTESE-CHAVE (testar)**: a engine usa **CRC32/hash de HARDWARE** (feature ARMv8) p/ o hash dos
  NOMES de asset no índice do OBB. Com ctor 186 pulado, o ponteiro da rotina de hash fica lixo (ou cai
  num default que calcula hash DIFERENTE) → lookups de asset acham offsets ERRADOS → objetos
  construídos sobre bytes errados → typeinfos/vtables lixo ("HT=1"). Isso LIGA ctor 186 à corrupção.
- **Próximo passo concreto**: desmontar de 74538 até o fim de ctor 186 (e o que ele chama) p/ achar
  ONDE o ponteiro 0xba é carregado/chamado e qual tabela/rotina ele seleciona. Consertar esse ponteiro
  (ou prover a rotina) → rodar 186 → ver se a corrupção de asset SOME. Ferramenta: `objdump -d
  --start-address=0x74538 --stop-address=0x74630 libapp.so` (ARM). Os literais PC-rel estão em
  0x74604-0x7461c.

### OUTROS PRÓXIMOS PASSOS
2. **Achar a leitura de estrutura errada**: "HT=1" vem do OBB. Hookar a alocação/construção do objeto
   corrompido e ver de qual offset do arquivo os bytes vêm. Cadeia de chamada (da pilha):
   `0x46afe0 ← 0x3de064 ← 0x46b958 ← 0x43c6fc ← 0x5643cc ← 0x466f28 ← 0x3eabdc ← 0x3d4808 ← 0x75e54`
   (libapp). Desmontar 0x46afe0 / 0x3de064 (perto do parse que cria o objeto).
3. **Suspeitar de tamanho de struct/ABI**: a engine é armeabi-v7a (Android). Algum campo lido como
   32/64-bit errado, ou packing diferente, desalinharia o parse → bytes de string viram ponteiros.
   Conferir se algum import de I/O (read/pread/mmap) devolve tamanho/posição divergente.
4. Comparar com como Bully/reVC tratam o resource/asset system (mesmo framework so-loader).

### Flags de debug (env)
`NFS_INIT` `NFS_SKIPCTOR` `NFS_PAD`(default 64) `NFS_FOPENLOG` `NFS_SEEKLOG` `NFS_DCASTLOG`
`NFS_NODCASTREC`(desliga recovery do dcast) `NFS_NOASSERTIGNORE` `NFS_INITDBG`(log por ctor)
`NFS_PTLOG` `NFS_DLSYMLOG` `NFS_TICKRECOVER` `NFS_FRAMES=N`

### Estado git
Trabalho commitado em ~/nextos_ports_android/ports/nfs (ver git log). Device .87 (DHCP, IP pode mudar).
