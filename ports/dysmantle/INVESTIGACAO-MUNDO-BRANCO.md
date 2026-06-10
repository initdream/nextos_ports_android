# DYSMANTLE — Investigação MUNDO BRANCO (diário de testes)

Objetivo: o jogo roda 100% (imagem+som+controle) mas no gameplay o **mundo não renderiza**:
chão branco, árvores/barris/matos/cabeça/armas do player NÃO aparecem; corpo+calça do player
e tampa do bonker renderizam (com cor). "algo surreal".

> ⚠️ NÃO REPETIR testes já feitos abaixo. Atualizar este MD a cada teste novo.

## FATOS CONFIRMADOS (pipeline)
- Cena renderiza num **FBO** (color=tex19 GL_COLOR_ATTACHMENT0, depth=tex20 GL_DEPTH_ATTACHMENT).
  FBO é **COMPLETO** (glCheckFramebufferStatus=0x8CD5). Depois é **composto fullscreen** na tela
  com o shader diffuse: `gl_FragColor = _vary_color * texture2D(_tex_diffuse, uv)`.
- A tela visível = o FBO composto (clear da TELA nunca aparece).
- **60% do FBO fica na cor de clear** = 60% do mundo nunca é desenhado no FBO.
- Draws: começam na tela (cur_fbo=0, menus), depois gameplay desenha no FBO (cur_fbo=1).
  No gameplay há draws no FBO (fbo>4000) E na tela (16000+).

## CAUSA-RAIZ (em apuração): vertex buffers format=0 falham
- `NX_Graphics_CreateVertexBufferWithVertices(fmt, verts, count, flags)` @0x4837d4:
  chama `Legacy::ConvertVertexFormatToVertexElements(fmt)` @0x574034; se fmt=0 → 0 elementos →
  "Can't create a vertex buffer with an unknown vertex format" → retorna NULL → objeto não desenha.
- ConvertVertexFormat trata formatos: 0x1,0x7,0xe,0xf,0x1f,0x60,0x61,0x67,0x6e,0x6f,0x7f (bitfield).
  Bits: 0x1=pos 0x2=uv 0x4=normal 0x8=cor 0x10=tangent (ordem de GetVertexComponentFlags).
- **2 CALLERS de createvb(fmt=0):**
  1. `ModelSurface::GenerateVerticesByFormat(this,fmt)` @0xa025b8 (call @0xa0268c): passa
     createvb(fmt, NULL, count, 4); verts preenchidos depois por GenerateStreamDataForVertexFormat.
  2. `ModelSurface::InitializeVertexAndIndexBuffers()` @0xa03b70 (call @0xa03c20). ← NOVO
- `GetVertexComponentFlagsAkaVertexFormat` @0xa05108 computa fmt dos 5 stream ptrs em
  this+64(pos) +72(cor) +80(uv) +88(normal) +96(tangent).

## FIX PARCIAL APLICADO (ON por default no código)
- `hook_genverts` (main.c, detour @0xa025b8): quando fmt==0, computa o formato real dos 5 stream
  ptrs e passa esse no lugar de 0. **Reduziu erros 41633→2587.** Surfaces com 4 streams
  (pos|cor|uv|normal) viram 0xF. **MAS visualmente NÃO mudou** (FBO segue 60% clear).
- Restam **2587** falhas: vêm de `InitializeVertexAndIndexBuffers`@0xa03c20 + surfaces do
  genverts com **streams TODOS nulos** (GENV-NULLSTREAMS: count=78/32 mas ptrs todos null).

## TESTES JÁ FEITOS (NÃO REPETIR)
| Teste | Env/método | Resultado |
|---|---|---|
| Remap format 0→0x7 | DYSMANTLE_VB_FMT0=7 | PIOR: bolas pretas (garbage, x1 é NULL p/ esses) |
| Clear TELA magenta | CLEAR_TEST=2 | 0% magenta → tela nunca mostra clear próprio (FBO cobre) |
| Clear FBO magenta | CLEAR_TEST=3 | **60% magenta** → 60% do FBO não é desenhado (chão falta) |
| Clear tudo magenta | CLEAR_TEST=1 | chão fica magenta (confirma chão=clear) |
| Shaders compile/link | SHADER_DUMP + hooks | 0 falhas (124 shaders OK) |
| Shader força vermelho | SHADER_RED | tela 100% vermelha (composite usa shader diffuse) |
| Shader só textura | SHADER_TEX | tela 100% branca = ENGANOSO (FBO vazio, não textura branca) |
| Texturas comprimidas | hook glCompressedTexImage2D | 0 (nenhuma comprimida) |
| Texture arrays GLES3 | hook glTexImage3D/Storage3D | 0 (não usa) |
| Dados das texturas | TEX_LOG + dump pixels | RGBA8 uncompressed, DADOS REAIS (só 3/57 brancas) |
| Params de textura | TEXPARAM_LOG | engine já usa MIN/MAG=LINEAR + WRAP=CLAMP (NPOT-safe!) |
| Force NPOT CLAMP/LINEAR | (default on) / NPOT_OFF | sem diferença (já era safe) |
| Force ETC2 OFF | (default) vs FORCE_ETC2 | idêntico (não era isso) |
| GLVER 3.0/3.1 | DYSMANTLE_GLVER=3.0 | remove erros "unknown vertex" MAS chão segue branco |
| Atributos de vértice | ATTR_LOG | pos→0 cor→1 uv→2 normal→3 (glBindAttribLocation); layout pos vec3@0 + cor rgba8@12 + uv vec2@16 stride24 ✓ |
| Textura bound no draw | DRAW_LOG | texturas REAIS bound unit0 (tex25/26/30); tex[1]=11 fixo unit1 |
| Sampler→unit | UNIF_LOG | _tex_diffuse→loc1, setado val=0 ou 1 (investigar) |
| FBO completo? | hook CheckFramebufferStatus | COMPLETO (0 incompletos) |
| Fix format-0 (genv) | hook_genverts (default) | erros 41633→2587, FBO ainda 60% clear |

## HIPÓTESES ABERTAS (a testar)
- [ ] Os 2587 restantes (InitializeVertexAndIndexBuffers + null-streams) = os objetos visíveis?
- [ ] Surfaces com streams nulos: por que não alocados? (count>0 mas ptrs null) →
      ver AllocateVertexStreamsForVertexFormat @0xa04e84 / InitializeForVertexFormat @0xa05150
- [ ] DEPTH: depth-texture attachment no Utgard quebra depth test? (forçar glDepthFunc ALWAYS)
- [ ] As surfaces 0xF "consertadas" pelo genv DESENHAM mesmo? (verificar se entram no FBO)
- [ ] Composite/lighting: _vary_color do quad fullscreen lava branco? (UNIF/blend)

## FERRAMENTAS
- `capgame.sh <fmt0>`: navega ao gameplay (PB_SELFTEST) + dd /dev/fb0 → fb_game.raw
- PC: converto fb_*.raw → PNG (BGRA, 1280x720, primeiros 8MB) e LEIO a imagem p/ julgar sozinho
- ENVS diag: VB_LOG/VB_FMT0/VB_DUMP/VB_CALLER, GENV_NOFIX, SHADER_DUMP/RED/TEX,
  TEX_LOG/TEXPARAM_LOG/NPOT_OFF, CLEAR_TEST=1/2/3, DRAW_LOG, ATTR_LOG, UNIF_LOG, GLVER,
  PB_SELFTEST (navega: 1× baixo + A/X)

## SESSÃO 2 (continuação)
| Teste | Env/método | Resultado |
|---|---|---|
| Fix format-0 entry e[0]=e[1]@4 | hook_initbufs (INITBUF_FIX) | VB-fail 2586→1347 MAS branco 0.58 (sem mudança) |
| Dump verts@8 das entradas | INITBUF dump | verts@8 começa com PONTEIRO (0x7f...) = OBJETO, não float cru! createvb gera lixo. Por isso 3 fixes de formato não mudaram nada |
| Depth ALWAYS | DEPTH_ALWAYS=1 | 0.58 branco (DESCARTADO) |
| Depth DISABLED | DEPTH_ALWAYS=2 | 0.58 branco (DESCARTADO) |

**CONCLUSÃO PARCIAL:** format-0 buffer creation NÃO é a causa visual (3 fixes, 0 mudança).
Streams da surface (this+64..96) são NULL p/ os objetos faltando. verts@8 das entradas é um
OBJETO (ponteiro), não dados crus. **HIPÓTESE FORTE NOVA: os STREAMS de vértice não são
alocados** (AllocateVertexStreamsForVertexFormat @0xa04e84 nunca roda ou falha) → sem geometria.
Possível causa: OOM/alocador, ou path de init que NÓS quebramos. PRÓXIMO: hookar
AllocateVertexStreams + ver se roda p/ as surfaces null-stream; comparar com ports que funcionam.

## SESSÃO 2 — PIVÔ IMPORTANTE
| Teste | Método | Resultado |
|---|---|---|
| Instancing GLES3? | readelf relocs | NÃO usa (descartado) |
| Worker thread sem ctx GL? | BUF_LOG tid | glBufferData SÓ na main thread (tid único), 0 erros → DESCARTADO |
| Formatos de textura | TEXFMT histograma | só RGBA + DEPTH_COMPONENT (FBO). SEM LUMINANCE → descartado fix Bully |
| tex 11 (unidade 1 fixa) | dump | cinza real (a9a9a9), não branca dummy |
| Comparar surface OK vs quebrada | INITBUF_DUMPALL | **OK: streams não-nulos, fmt@0 correto (0x7F), verts@8=NULL, buf criado. QUEBRADA: streams NULOS, fmt@0=0, verts@8=objeto** |

**🔑 CONCLUSÃO-CHAVE:** os 3 fixes de format-0 (genv, initbuf e[0]=e[1], remap) reduziram erros
41633→1347 mas **NENHUM mudou a tela (0.58 branco sempre)**. Logo **format-0 NÃO é o mundo
visível** — são surfaces à parte (LOD/partículas/decals?) que falham e a engine ignora.
O **mundo visível (chão/árvores) usa surfaces FUNCIONANDO** (streams ok) que **renderizam
branco/invisível**. Estava caçando a coisa errada 50 iterações.

**REDIRECIONAR p/:** por que surfaces FUNCIONANDO (streams ok, buffer ok) do chão/props
renderizam branco/invisível? Suspeitas: (a) lighting/blend lava branco (testar glBlendFunc,
ainda NÃO testado — clr3 só testou clear); (b) shader/textura específica por tipo de objeto;
(c) coordenadas/projeção. AllocateVertexStreamsForVertexFormat@0xa04e84 escreve streams
this+64..96 baseado no formato (bits). AllocateVertexStreams(0)=sem streams.

## 🎯 SESSÃO 2 — CAUSA-RAIZ DO CHÃO BRANCO ENCONTRADA (stride mismatch)
| Teste | Método | Resultado |
|---|---|---|
| Ler FBO direto (sem composite) | FBO_DUMP glReadPixels | cena = chão branco + blobs pretos (NÃO é composite; é a cena mesmo) |
| Neutralizar _vary_color | NOCOL (shader) | chão centro AINDA branco + bordas PRETAS (fog of war ok); objetos+detalhados |
| Shader do chão (prog 34) | DUMP_PROG=34 | é o BÁSICO: gl_FragColor=_vary_color*texture2D(_tex_diffuse,uv). tex23=cinza real |
| **DUMP vértices do chão** | VERT_DUMP buffer>200KB | **🔑 dados em STRIDE 40 (formato 0x7F) mas atributos em STRIDE 24 (0x7)! v0/v3 válidos a 0/120=3×40; v1 uv=(-39,-73)=posição de v0** |
| Atributos | ATTR_LOG | stride=24 (pos@0 cor@12 uv@16) — mismatch com buffer 40B |
| Forçar stride 40 GLOBAL | FORCE_STRIDE=40 | tela PRETA (quebra os buffers 24B legítimos) |
| Auto-fix stride por tamanho | size-match | colisão de tamanho (virou 32), pior |
| Auto-fix stride por estrutura UV | detecta {24..48} 85% | **TRIÂNGULOS/geometria APARECERAM** (0.533 branco)! mas detectou 48 (errado, distorce skinned) |
| Auto-fix stride 40 estrito | ok40>90% & ok24<70% | 0 hits (terreno usa UV tiling >1, range apertado) |

**🔑 CAUSA-RAIZ: o chão (e provavelmente árvores/props) é desenhado ligando o buffer da variante
0x7F (40 bytes: pos+uv+normal+cor+tangent) MAS configurando os atributos com stride do formato
0x7 (24 bytes: pos+cor+uv). A GPU lê os vértices no passo errado → posições/UVs erradas → branco.**
Os 24 PRIMEIROS bytes do vértice 0x7F batem com 0x7 (pos@0 cor@12 uv@16) — por isso v0 sempre ok.
**Por que:** a variante 0x7 (24B) FALHOU de criar (erro format-0), o engine caiu p/ a variante
0x7F (40B) mas manteve atributos de 0x7. → format-0 IMPORTA, mas o fix não é o formato do buffer
e sim **fazer a variante 0x7 existir** OU corrigir o stride do atributo no draw.

**PROGRESSO:** forçar o stride certo FAZ a geometria aparecer (triângulos). Falta detecção exata
do stride por buffer (heurística UV frágil pq terreno tem UV de tiling). PRÓXIMO: detecção robusta
(testar stride por validade de POSIÇÃO contígua, não UV) OU criar a variante 0x7 de verdade
(re-interleave dos dados 0x7F→0x7 no createvb quando format-0).

## ⚠️ SESSÃO 2 — STRIDE FOI RED HERRING; chão = textura cinza + vértice branco
| Teste | Método | Resultado |
|---|---|---|
| Stride dos buffers GRANDES no attr | STRIDE_DBG filtro >50KB | **buffers grandes (chão) usam attr_stride=40 CORRETO (size%40=0)!** Os de 24 eram sprites (9216B). SEM mismatch no chão |
| → stride-fix | gateado OFF (DYSMANTLE_STRIDEFIX) | os "triângulos" eram SPRITES de 24B quebrados pelo meu fix, NÃO chão. Stride = red herring |
| Grid de luminância das texturas | TEXGRID | tex23/24 (chão 256x256) = CINZA (a9a9a9, R=G=B). Outras têm cor (azul 1e78bf etc). tex23 é grayscale |
| Cor de vértice do terreno | VERTDUMP col@12 | **255,255,255,255 (BRANCO) em TODOS os vértices do chão** |
| Shader do chão (prog 34 e 35) | DUMP_PROG | ambos BÁSICOS: gl_FragColor = _vary_color * texture2D(_tex_diffuse,uv) |

**🔑 CONCLUSÃO SESSÃO 2:** o chão renderiza com geometria/stride/UV CORRETOS, mas fica
cinza-claro (~branco) porque: **(a) a textura do chão (tex23) é GRAYSCALE (a9a9a9)** e
**(b) a cor de vértice do terreno é BRANCA (255,255,255)**. gl_FragColor = branco × cinza =
cinza-claro. A COR (verde do gramado) deveria vir do _vary_color de vértice (tint+luz assada do
terreno) OU da textura — **nenhum dos dois fornece cor**. As bordas pretas = fog of war (correto).

**DUAS HIPÓTESES p/ a cor faltando (próxima sessão):**
1. **Cor de vértice do terreno** deveria codificar tint/luz (verde) mas é branca → a geração da
   mesh do terreno (GenerateStreamData / stream de cor) não preenche a cor, fica default 255.
   → investigar o stream de cor (this+72) e como a luz/tint do tile é assada no vértice.
2. **Textura tex23 deveria ser colorida** (grama) mas decodificou grayscale → bug no nosso
   pipeline de textura (fix_empty_textures.py ETC2→JPEG) p/ as texturas de terreno.
   → comparar tex23 com o asset original; ver se R=G=B é decode ou design.

Quick test p/ desambiguar: forçar gl_FragColor = vec4(0,1,0,1)*diffuse no shader do chão — se
virar grama verde, tex23 é detalhe-luminância e falta o tint (hip 1); se cinza-esverdeado, tex23
é o problema (hip 2).

## ✅ SESSÃO 2 FINAL — tint verde confirma: falta a COR do terreno
- DYSMANTLE_GREEN (tinta o shader básico de verde): o chão mostra DETALHE/padrão (relevo do
  bonker, variação do terreno) tudo verde. **A textura tem o padrão certo (detalhe grayscale);
  falta só a COR.** Confirma hip 1: a cor (verde do gramado) deveria vir do _vary_color, que é
  branco (255). [imagem: ~/dysmantle-build/fb_green.png]
- **DIAGNÓSTICO FINAL DO MUNDO BRANCO:** geometria/stride/UV/textura-detalhe TODOS corretos.
  Só a COR do terreno está faltando: cor-de-vértice branca (255,255,255) em vez do tint verde.
- **PRÓXIMO (cor do terreno):** descobrir por que o stream de cor do vértice do terreno
  (this+72, escrito por GenerateStreamData) fica branco. OU se tex23 deveria ser colorida e
  nosso fix_empty_textures.py (ETC2→JPEG) a decodificou grayscale (testar: comparar tex23 com
  asset original; ver se o decode ETC2 das texturas de TERRENO perde cor). Provável "algo que
  NÓS mudamos" (pak/textura) como no Bully.
- ⚠️ infra de diag toda env-gated (default estável). hooks default-ON inofensivos: hook_genverts/
  initbufs (format-0, reduz erros mas não muda visual), STRIDEFIX OFF.

## SESSÃO 3 (manhã) — testada hip da TEXTURA; chão não usa vary_color
| Teste | Método | Resultado |
|---|---|---|
| Decode ETC2 do script | ler fix_empty_textures.py | CORRETO (decode_etc2→RGB colorido); NÃO trunca (anexa se não cabe); só preenche size==0 |
| Texturas de grama no pak orig | parse + decode | TÊM DADOS (não mexemos) e são COLORIDAS (edge-grassland-grass-diffuse [76,93,42] verde, sat 53) |
| tex23/24 coloridas? | TEXRGB (canais R,G,B) | tex24=COLORIDA (azul 2f3849); **tex23=grayscale-esverdeado dessaturado (4c5655=76,86,85)** ≠ grama |
| offset da cor por stride | ATTRCOL | cor@12 tanto stride 24 quanto 40 (meu tint estava no offset certo) |
| Tingir vary_color de verde | TINT_GREEN (reescreve cor@12 dos buffers 40B) | **chão NÃO mudou** → o chão NÃO usa vary_color; cor vem 100% da textura tex23 |

**🔑 CONCLUSÃO SESSÃO 3:** o chão amostra a textura **tex23 (cinza-esverdeada dessaturada)**, NÃO
a grama verde ([76,93,42]) que existe no pak. E o chão não responde ao vary_color. Então:
**a textura ERRADA (ou um blend/detail map cinza) está sendo amostrada como base do chão**, em vez
da textura de grama colorida. Não é decode (correto), não é vary_color (chão ignora), não é
geometria (correta). É a **seleção/composição de textura do terreno** — provavelmente um atlas de
tiles ou lookup que aponta pro tile errado/cinza no nosso ambiente.

**ESTADO:** mundo renderiza geometricamente perfeito; falta a COR do terreno (amostra tex cinza
em vez da grama verde). É um bug fundo do pipeline de terreno do DYSMANTLE (atlas/tile lookup),
nível Hollow Knight de dificuldade. Jogo segue 100% jogável (imagem+som+controle), só o visual do
chão washed-white. Infra de diag toda env-gated; TINT_GREEN/STRIDEFIX off por default.

## 🔄 SESSÃO 3 — CORREÇÃO: tex23=máquina, chão é UNDRAWN (volta pro format-0)
- Salvei as imagens das texturas (DYSMANTLE_TEX_LOG=1 DYSMANTLE_TEX_SAVE=1 → texdump_*.raw).
  **tex23 = atlas de peças da MÁQUINA/bonker** (engrenagens/canos cinza+vermelho), NÃO o chão!
  tex9=fumaça(blobs), tex11=grunge, tex19=splash art. NENHUMA textura verde de terreno carrega.
- **clr3 já provava: 60% do FBO = cor de clear (undrawn).** Então o "chão branco" é o CLEAR do
  FBO, não uma textura cinza. O terreno **não desenha** (geometria format-0 falha).
- **TEORIA DO USUÁRIO (validada):** os que somem (chão, árvores, barris, cabeça, armas) são
  objetos COMPLEXOS (destrutíveis/animados/skinned) que usam o caminho de vertex-stream
  format-0/objeto que falha. Os que desenham (bonker=tex23, corpo do player, HUD) são meshes
  simples. → **UMA causa: a classe de geometria com streams em OBJETO (verts@8) + format 0.**
- Detalhe do objeto: entry verts@8 aponta p/ {field0=ponteiro 0x7f.., ...} = OBJETO (não float
  cru). createvb trata como raw → lixo. entry @4=0x7F (formato real). Working surfaces: verts@8
  NULL (data dos streams). Broken: verts@8=objeto (data dentro).
- Blend aditivo existe (SRC_ALPHA,ONE); NO_ADD reduz branco 0.58→0.477 (luz/sol contribui pouco).

**PRÓXIMO (caminho de geometria que falha):** entender o OBJETO em verts@8 (field0=ptr p/ floats?)
e fazer createvb/GenerateStreamData extrair os dados certos quando format-0/objeto. OU achar onde
o entry+0 (formato) deveria ser escrito (é 0; deveria ser e[4]=0x7F) ANTES de AllocateVertexStreams,
p/ os streams alocarem e a geometria preencher. Comparável a Hollow Knight em dificuldade.

## SESSÃO 3 — fix arriscado + teoria textura: ambos negativos
| Teste | Método | Resultado |
|---|---|---|
| Fix genstreams | DYSMANTLE_GENSTREAMS (chama GenerateVertexStreamsFromInterleaved p/ fmt0) | rodou 6x mas streams continuam NULOS (fmt=0) — a função lê o formato de this+232 que é a CONTAGEM (=2), não o formato; ambiguidade de layout. Não populou |
| Teoria memória textura (Felipe) | DYSMANTLE_TEX_HALF (reduz texturas ≥256 pela metade, box2x2) | TEXHALF rodou (4+ texturas) MAS branco 0.58 inalterado → NÃO é limite de memória de textura do Utgard. Geometria não desenha (≠ Bully) |

**CONFIRMADO:** o terreno/objetos complexos NÃO desenham por GEOMETRIA (format-0/vertex-stream
em objeto interleaved), não por textura nem memória. O fix via GenerateVertexStreamsFromInterleaved
trava porque o campo de formato (this+232) é a contagem de entries (=2), não os bits de formato —
o formato real (0x7F) está em entry@4 mas a função não o usa. Layout da ModelSurface ambíguo p/
RE estática. PRÓXIMO REAL: gdb no device p/ rastrear a vtable que decide stream-path × interleaved-path,
ou RE do construtor da ModelSurface p/ achar onde o formato deveria ser setado (é 0). Nível HK.

## 🔌 CRASH ao mudar config / re-importar mapa = ESTADO DE GPU DEGRADADO (reboot resolve)
- Sintoma: depois de horas de testes (muitos `killall -9 dysmantle`), o jogo passou a CRASHAR
  no carregamento do mundo (`StageImporter::AddActorFromNode → SOActor::SetType →
  ActorRendererModelSkinned::OnAllocateActor → ModelInstance::InitializeFromModel`), num
  `memcpy/op(dst=NULL, n=11)` numa worker thread. NÃO é canary (__stack_chk_fail=0), NÃO é RAM
  (500MB livres, swap vazio, overcommit=1), NÃO é o save, NÃO é meu hook (genv/initbuf/memcpy guards).
- **CAUSA = memória/estado de GPU do Mali Utgard (fbdev) VAZANDO entre runs.** Cada `killall -9`
  não devolve a memória de textura da GPU direito → após N runs, alocação de textura do skinned
  actor falha → buffer nulo → crash. (Confirma a teoria do usuário de "memória de textura", mas
  é ACUMULADO entre execuções, não num run só.)
- **FIX = REBOOT do device.** Após reboot: CRASH=0, jogo carrega o mundo normal de novo.
- ⚠️ LIÇÃO p/ testes: ao testar o mesmo jogo Mali dezenas de vezes com killall -9, REBOOTAR o
  device periodicamente (ou quando aparecerem crashes estranhos no load). Não é bug do port.
- Infra anti-crash adicionada (env-default, inofensiva): guards de memcpy/memmove/__memcpy_chk/
  __memmove_chk que pulam cópia com dst/src nulo (não pegou esse crash pq é interno ao libc, mas
  protege contra null genuíno). TEX_HALF disponível p/ reduzir pressão de textura se reincidir.

## 🎯 RESOLVIDO (causa do crash no loading): DYNAMIC SHADOWS (descoberto pelo usuário)
- **Ativar "Dynamic Shadows" nas opções → trava/crasha no loading do mundo.** Com OFF, carrega
  normal (funcionou o dia todo).
- Causa técnica: sombras dinâmicas renderizam os atores SKINNED num shadow map (FBO de
  profundidade). No Mali-450 Utgard (GLES2) esse caminho de render skinned-p/-shadow quebra →
  crash em StageImporter::AddActorFromNode → ActorRendererModelSkinned::OnAllocateActor →
  ModelInstance::InitializeFromModel (ponteiro nulo/lixo).
- O setting PERSISTE (por isso relançar continuava crashando após ativar). NÃO era race/device/
  overcommit/GPU-state/meu hook — foi o setting "dynamic shadows".
- **FIX p/ usuário: manter Dynamic Shadows OFF.** (Opcional: travar o setting no port p/ não dar
  pra ativar — DYSMANTLE_SKIP_BADACTORS=1 existe como rede de segurança mas corrompe o importer,
  evitar.)
- Reverte a teoria de "mundo branco" tb? NÃO — mundo branco (geometria format-0) é outro assunto
  (terreno não desenha). Este achado é só sobre o CRASH no loading quando shadows ON.

## 📝 OBSERVAÇÕES DE GAMEPLAY (usuário, p/ a próxima sessão do mundo branco)
- Jogando um pouco (com shadows OFF = config correta), o quadro é MAIS NUANCEADO do que "tudo some":
  - **RENDERIZA:** baús/barris, portas, prateleiras (muitos props aparecem normal!)
  - **NÃO renderiza:** latas de lixo (lixeiras) — e o chão/terreno (branco).
- **Implicação:** NÃO é o pipeline de geometria inteiro quebrado — a MAIORIA dos objetos desenha.
  Só tipos ESPECÍFICOS faltam (lixeira + terreno). Logo é estudável como um problema localizado
  (modelo/textura/material específico daqueles objetos), não um muro do engine.
- ⚠️ REVALIDAR o mundo branco com shadows OFF na próxima sessão — parte da investigação anterior
  pode ter sido contaminada pelo estado de crash do dynamic-shadows. Reconfirmar o que realmente
  falta com o jogo estável.
- Próxima sessão = "nova seção" dedicada (decisão do usuário): jogar, mapear o que aparece × falta,
  e atacar os casos específicos (lixeira, terreno).
