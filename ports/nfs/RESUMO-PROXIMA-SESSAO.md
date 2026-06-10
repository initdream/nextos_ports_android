# NFS Most Wanted — RESUMO p/ próxima sessão (handoff)

## ONDE PAROU (~95% do boot — engine boota, LÊ o OBB, ENTRA no render loop; trava na construção de objetos de asset CORROMPIDOS)

### Rodar (device nextos-87, ES mascarado, Mali fbdev)
```
cd ~/nextos_ports_android/ports/nfs && ./build.sh && scp -q nfs nextos-87:/storage/roms/nfs/
ssh nextos-87 'cd /storage/roms/nfs && export LD_LIBRARY_PATH="/usr/lib32:/storage/roms/nfs" \
  SDL_VIDEODRIVER=mali NFS_INIT=1 NFS_SKIPCTOR="186,187,188"; timeout 70 ./nfs 2>&1 | tail -50'
```
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

### PRÓXIMOS PASSOS (atacar a CAUSA-RAIZ, não os sintomas)
1. **🔑 TESTAR NFS_SKIPCTOR**: hoje pula ctors 186/187/188 (crashavam no init). Se um deles inicializa
   o **registro de tipos / alocador de assets / RTTI**, pular deixa o sistema de tipos sem bootstrap →
   typeinfos lixo. Desmontar os 3 ctors (offsets no `.init_array`; usar NFS_INITDBG p/ logar por ctor)
   e ver o que fazem. Tentar fixar o crash deles (em vez de pular) — pode ser a causa-raiz inteira.
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
