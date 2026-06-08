# GTA VC Android -> NextOS status

Device de teste: `DEVICE_IP`, login `root`, senha (padrao do CFW).

Regra de teste importante:
- parar EmulationStation antes de rodar jogos/ports pesados;
- se SSH travar ou GPU ficar em estado sujo, pedir reboot/power-cycle antes de continuar.

Comandos usados:
- parar ES: `systemctl stop emustation || systemctl stop emulationstation || killall -9 emulationstation || killall -9 EmulationStation`
- rodar: `cd /storage/roms/ports/gtavc && LD_PRELOAD=./nextclock.so ./gtavc >run.out 2>&1 &`
- logs: `/storage/roms/ports/gtavc/debug.log` e `run.out`
- preparo/teste apos reboot: `./run-device-test.sh` neste diretorio local.

Achados em 2026-06-05:
- com ES aberto, GTA VC entra em swap pesado no load do mapa e fica preso/crasha perto de `DATA/MAPS/littleha/littleha.ipl`;
- com ES parado, a RAM inicial melhora muito, mas o build antigo ainda carregava `TEXDB/VEH_HIGH.IMG`, `PED_HIGH`, `CUT_HIGH`, `weap_high` e chegava em `VmRSS` ~644 MB + `VmSwap` ~571 MB;
- o log mostra perfil `device 19`, renderer spoofado como `Mali-400 MP`, `m_PrefsMobileResolution 0.64`;
- foi aplicado patch low-memory em `jni_patch.c` e `main.c`:
  - `GetDeviceType` agora anuncia 192 MB / 2 cores;
  - `GetTotalMemory` 192 MB, `GetAvailableMemory` 96 MB, low threshold 128 MB;
  - antes do start automatico, forca `isLowMemoryDevice=1`, highpoly/effects/shadows off, draw distance 0.35, mobile resolution 0.50.
- depois do reboot, o teste confirmou que o low-memory entrou: log `tegrapablet processors 2 memory 192`;
- novo bloqueio: low-memory tenta `TEXDB/CUT_LOW.IMG`, mas o dump so tinha `cut_high.*`; foram copiadas aliases fisicas no device:
  - `/storage/roms/ports/gtavc/TEXDB/cut_low.img/.dir`
  - `/storage/roms/ports/gtavc/texdb/cut_low.img/.dir`
- swap melhor para teste:
  - arquivo existente `/storage/roms/gtavc.swap` reformatado/ativado com 2 GB;
  - `vm.swappiness=20`, `vfs_cache_pressure=50`;
  - apos reboot precisa reativar com `swapon /storage/roms/gtavc.swap`;
  - script local `run-device-test.sh` para ES, ativa swap, ajusta sysctl e inicia o port.
- node pools:
  - hooks de `CPtrNode::new/delete` e `CEntryInfoNode::new/delete` sao necessarios para passar do init/load;
  - build atual usa arena diagnostica de 48 MB e loga `CFileLoader::ms_line` em `node-new` / `node-arena-full`;
  - `node-arena-full` ou hang em `CEntity::Add` indica modelo IPL com bounding/collision invalido.
- dados de mapa filtrados no device, sempre com backup `*.pre_<modelo>_bak`:
  - `telgrphpole02` (id 384): 284 instancias removidas dos `.ipl` ativos;
  - `MTraffic1` (id 428): 246 instancias removidas;
  - `roadsign` (id 601): 28 instancias removidas;
  - `washbuild068` (id 3300): 1 instancia removida;
  - `kb_chr_tbl_test` (id 471): 72 instancias removidas;
  - `kb_canopy_test` (id 470): 38 instancias removidas;
  - `wshbuildws25` (id 2823): 1 instancia removida.
  - `wshbuildws13` (id 2951): 2 instancias removidas.
- teste apos esses filtros:
  - passa de `littleha.ipl`, `downtown.ipl`, `downtows.ipl`, `docks.ipl`, `washintn.ipl`;
  - passou para `washints.ipl`; ultimo bloqueio encontrado foi `wshbuildws13`, ja removido;
  - proximo passo e testar novamente a partir desse estado e capturar o proximo `ms_line` se houver novo hang.

Estado do binario:
- local: `/home/runner/nextos_ports_android/ports/gtavc/gtavc`
- remoto: `/storage/roms/ports/gtavc/gtavc`
- md5 do build low-memory enviado: `0813b804b7f13ff467f7687a9c1f5a72`

Proximo passo:
- rodar `./run-device-test.sh`, monitorar `/storage/roms/ports/gtavc/debug.log`;
- se o log parar sem crash, anexar gdb e ler `CFileLoader::ms_line` em `text_base + 0x578ac0`;
- remover do `.ipl` ativo o proximo modelo com backup, ou corrigir o asset/collision de origem se preferir preservar o mapa.
