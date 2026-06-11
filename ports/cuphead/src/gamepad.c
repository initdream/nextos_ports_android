/* ===== Controle REAL p/ Cuphead (Mali-450) via USB Gamepad (joydev js0) =====
 *
 * O jogo lê input por Rewired.Player.GetButton/GetButtonDown/GetAxis(string actionName).
 * SEM controle reconhecido, o Rewired retorna false/0 pra tudo. Em vez de fazer o Rewired
 * reconhecer o controle (depende de templates/maps), SUBSTITUÍMOS esses métodos: lemos o
 * nome da ação (il2cpp String) e respondemos com o estado do /dev/input/js0.
 *
 * Gated por CUP_GAMEPAD=1 (no main.c). CUP_GPLOG=1 loga eventos do js0 + nomes de ação
 * pedidos (1ª vez) p/ calibrar. Overrides de botão: CUP_GP_<ACAO>=<num> (ex CUP_GP_JUMP=0).
 */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <linux/joystick.h>

extern void hook_arm64(uintptr_t addr, uintptr_t fn);


#define GP_NBTN  32
#define GP_NAXIS 16

static int     gp_fd = -1;
static unsigned char gp_btn[GP_NBTN];        /* estado atual (este frame) */
static unsigned char gp_btn_prev[GP_NBTN];   /* estado no frame anterior  */
static int16_t gp_axis[GP_NAXIS];
static int     gp_log = 0;

/* ---- mapa ação -> botão do gamepad (defaults p/ USB pad genérico; env override) ---- */
enum { B_JUMP, B_SHOOT, B_DASH, B_LOCK, B_SUPER, B_SWITCH, B_PAUSE,
       B_ACCEPT, B_CANCEL, B_PARRY, B_COUNT };
static int gp_map[B_COUNT] = {
  /*JUMP */ 0, /*SHOOT*/ 2, /*DASH */ 5, /*LOCK */ 4, /*SUPER*/ 1,
  /*SWITCH*/3, /*PAUSE*/ 9, /*ACCEPT*/0, /*CANCEL*/1, /*PARRY*/ 0,
};
static const char *gp_envname[B_COUNT] = {
  "CUP_GP_JUMP","CUP_GP_SHOOT","CUP_GP_DASH","CUP_GP_LOCK","CUP_GP_SUPER",
  "CUP_GP_SWITCH","CUP_GP_PAUSE","CUP_GP_ACCEPT","CUP_GP_CANCEL","CUP_GP_PARRY",
};

/* eixos: stick/dpad principal. Override: CUP_GP_AXH / CUP_GP_AXV (default 0/1). */
static int gp_axh = 0, gp_axv = 1;
static int gp_axdead = 8000;            /* zona morta */
static int gp_axthresh = 16000;         /* limiar p/ Up/Down/Left/Right digital */
static int gp_invv = 0;                 /* inverter vertical (Unity up=+1; js up=neg) */

/* ---- leitura do js0 ---- */
int gp_open(const char *path) {
  gp_fd = open(path, O_RDONLY | O_NONBLOCK);
  return gp_fd;
}

void gp_poll(void) {
  if (gp_fd < 0) return;
  struct js_event e;
  while (read(gp_fd, &e, sizeof(e)) == (int)sizeof(e)) {
    int t = e.type & ~JS_EVENT_INIT;
    if (t == JS_EVENT_BUTTON && e.number < GP_NBTN) {
      gp_btn[e.number] = e.value ? 1 : 0;
      if (gp_log) { fprintf(stderr, "[GP] btn %d = %d\n", e.number, e.value); fflush(stderr); }
    } else if (t == JS_EVENT_AXIS && e.number < GP_NAXIS) {
      gp_axis[e.number] = e.value;
      if (gp_log && (e.value > 20000 || e.value < -20000)) {
        fprintf(stderr, "[GP] axis %d = %d\n", e.number, e.value); fflush(stderr);
      }
    }
  }
}

/* fim do frame: snapshot p/ edge-detect do GetButtonDown/Up */
void gp_frame_end(void) {
  memcpy(gp_btn_prev, gp_btn, sizeof(gp_btn));
}

/* ---- il2cpp String -> ascii (layout: +0x10 len int32, +0x14 chars utf16) ---- */
static void str_ascii(void *s, char *out, int cap) {
  out[0] = 0;
  if (!s) return;
  int32_t len = *(int32_t *)((char *)s + 0x10);
  if (len < 0 || len > 64) len = (len < 0) ? 0 : 64;
  uint16_t *c = (uint16_t *)((char *)s + 0x14);
  int n = 0;
  for (int i = 0; i < len && n < cap - 1; i++) {
    uint16_t ch = c[i];
    out[n++] = (ch < 128) ? (char)ch : '?';
  }
  out[n] = 0;
}

/* nome de ação -> índice de botão (B_*) ou -1 se não é botão simples */
static int name_btn(const char *n) {
  if (!strcasecmp(n, "Jump"))                  return B_JUMP;
  if (!strcasecmp(n, "Shoot"))                 return B_SHOOT;
  if (!strcasecmp(n, "Dash"))                  return B_DASH;
  if (!strcasecmp(n, "Lock") || !strcasecmp(n, "Aim")) return B_LOCK;
  if (!strcasecmp(n, "Super") || !strcasecmp(n, "EX")) return B_SUPER;
  if (!strcasecmp(n, "Switch") || !strcasecmp(n, "SwitchWeapon") ||
      !strcasecmp(n, "SwitchTailDirection"))   return B_SWITCH;
  if (!strcasecmp(n, "Pause"))                 return B_PAUSE;
  if (!strcasecmp(n, "Accept") || !strcasecmp(n, "Submit")) return B_ACCEPT;
  if (!strcasecmp(n, "Cancel"))                return B_CANCEL;
  if (!strcasecmp(n, "Parry"))                 return B_PARRY;
  return -1;
}

/* direções digitais (Up/Down/Left/Right) a partir do eixo */
static int name_dir(const char *n) {
  /* retorna: 1=Up 2=Down 3=Left 4=Right 0=não-direção */
  if (!strcasecmp(n, "Up"))    return 1;
  if (!strcasecmp(n, "Down"))  return 2;
  if (!strcasecmp(n, "Left"))  return 3;
  if (!strcasecmp(n, "Right")) return 4;
  if (!strcasecmp(n, "MenuUp"))    return 1;
  if (!strcasecmp(n, "MenuDown"))  return 2;
  if (!strcasecmp(n, "MenuLeft"))  return 3;
  if (!strcasecmp(n, "MenuRight")) return 4;
  return 0;
}

static int dir_held(int d) {
  int h = gp_axis[gp_axh], v = gp_axis[gp_axv];
  switch (d) {
    case 3: return h < -gp_axthresh;  /* Left  */
    case 4: return h >  gp_axthresh;  /* Right */
    case 1: return gp_invv ? (v > gp_axthresh) : (v < -gp_axthresh);  /* Up   */
    case 2: return gp_invv ? (v < -gp_axthresh) : (v > gp_axthresh);  /* Down */
  }
  return 0;
}

/* loga nome de ação 1× p/ calibrar */
static void log_action(const char *kind, const char *n) {
  static char seen[64][24]; static int sn = 0;
  if (!gp_log) return;
  for (int i = 0; i < sn; i++) if (!strcmp(seen[i], n)) return;
  if (sn < 64) { strncpy(seen[sn], n, 23); seen[sn][23] = 0; sn++; }
  fprintf(stderr, "[GP] action %s(\"%s\")\n", kind, n); fflush(stderr);
}

/* ---- hooks (substituem Rewired.Player.* string-overload) ---- */
/* held: GetButton */
int gp_GetButton(void *self, void *name) {
  (void)self; char nm[32]; str_ascii(name, nm, sizeof nm); log_action("GetButton", nm);
  int b = name_btn(nm); if (b >= 0) return gp_btn[gp_map[b]] ? 1 : 0;
  int d = name_dir(nm); if (d)      return dir_held(d) ? 1 : 0;
  return 0;
}
/* edge down: GetButtonDown */
int gp_GetButtonDown(void *self, void *name) {
  (void)self; char nm[32]; str_ascii(name, nm, sizeof nm); log_action("GetButtonDown", nm);
  int b = name_btn(nm);
  if (b >= 0) { int j = gp_map[b]; return (gp_btn[j] && !gp_btn_prev[j]) ? 1 : 0; }
  /* direções: edge não temos prev por-eixo; aproxima com held (menus toleram repeat) */
  int d = name_dir(nm); if (d) return dir_held(d) ? 1 : 0;
  return 0;
}
/* edge up: GetButtonUp */
int gp_GetButtonUp(void *self, void *name) {
  (void)self; char nm[32]; str_ascii(name, nm, sizeof nm);
  int b = name_btn(nm);
  if (b >= 0) { int j = gp_map[b]; return (!gp_btn[j] && gp_btn_prev[j]) ? 1 : 0; }
  return 0;
}
/* eixo: GetAxis / GetAxisRaw */
float gp_GetAxis(void *self, void *name) {
  (void)self; char nm[32]; str_ascii(name, nm, sizeof nm); log_action("GetAxis", nm);
  int ax = -1, inv = 0;
  if (!strcasecmp(nm, "Horizontal") || !strcasecmp(nm, "MoveHorizontal") ||
      !strcasecmp(nm, "MenuHorizontal")) ax = gp_axh;
  else if (!strcasecmp(nm, "Vertical") || !strcasecmp(nm, "MoveVertical") ||
           !strcasecmp(nm, "MenuVertical")) { ax = gp_axv; inv = !gp_invv; }
  if (ax < 0) return 0.0f;
  int v = gp_axis[ax];
  if (v > -gp_axdead && v < gp_axdead) return 0.0f;
  float f = v / 32767.0f; if (f > 1) f = 1; if (f < -1) f = -1;
  return inv ? -f : f;   /* Unity: up=+1 */
}

/* "qualquer botão" — p/ o disclaimer "press any button to skip" e menus.
   GetAnyButton = algum botão segurado; GetAnyButtonDown = edge (algum botão novo). */
int gp_GetAnyButton(void *self) {
  (void)self;
  for (int i = 0; i < GP_NBTN; i++) if (gp_btn[i]) return 1;
  return 0;
}
int gp_GetAnyButtonDown(void *self) {
  (void)self;
  for (int i = 0; i < GP_NBTN; i++) if (gp_btn[i] && !gp_btn_prev[i]) return 1;
  return 0;
}

void gp_install_hooks(uintptr_t base) {
  /* string-overloads do Rewired.Player (RVAs do dump) */
  hook_arm64(base + 0x11a5378, (uintptr_t)gp_GetButton);     /* GetButton(string)     */
  hook_arm64(base + 0x11a5448, (uintptr_t)gp_GetButtonDown); /* GetButtonDown(string) */
  hook_arm64(base + 0x11a5518, (uintptr_t)gp_GetButtonUp);   /* GetButtonUp(string)   */
  hook_arm64(base + 0x11a7f90, (uintptr_t)gp_GetAxis);       /* GetAxis(string)       */
  hook_arm64(base + 0x11a807c, (uintptr_t)gp_GetAxis);       /* GetAxisRaw(string)    */
  /* "any button" — disclaimer skip + Rewired.Player.GetAnyButton/Down */
  hook_arm64(base + 0xCC2854,  (uintptr_t)gp_GetAnyButtonDown); /* CupheadInput.AnyPlayerInput.GetAnyButtonDown */
  hook_arm64(base + 0x11a66f8, (uintptr_t)gp_GetAnyButton);     /* Rewired.Player.GetAnyButton     */
  hook_arm64(base + 0x11a672c, (uintptr_t)gp_GetAnyButtonDown); /* Rewired.Player.GetAnyButtonDown */
}

void gp_init(uintptr_t il2cpp_base) {
  gp_log = getenv("CUP_GPLOG") ? 1 : 0;
  if (getenv("CUP_GP_AXH")) gp_axh = atoi(getenv("CUP_GP_AXH"));
  if (getenv("CUP_GP_AXV")) gp_axv = atoi(getenv("CUP_GP_AXV"));
  if (getenv("CUP_GP_INVV")) gp_invv = atoi(getenv("CUP_GP_INVV"));
  for (int i = 0; i < B_COUNT; i++)
    if (getenv(gp_envname[i])) gp_map[i] = atoi(getenv(gp_envname[i]));
  const char *dev = getenv("CUP_GP_DEV") ? getenv("CUP_GP_DEV") : "/dev/input/js0";
  int fd = gp_open(dev);
  fprintf(stderr, "[GAMEPAD] js=%s fd=%d log=%d (hooks Rewired.Player string overloads)\n",
          dev, fd, gp_log);
  gp_install_hooks(il2cpp_base);
}
