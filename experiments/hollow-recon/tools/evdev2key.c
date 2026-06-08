/*
 * evdev2key.c — tradutor gamepad evdev -> teclado virtual (uinput).
 *
 * O "USB Gamepad" generico nao tem perfil no Rewired(HK) nem no SDL/gptokeyb, mas
 * gera eventos evdev crus normalmente (dpad em ABS_HAT0X/Y, botoes em BTN_*).
 * Lemos o device e emitimos TECLAS via uinput; o HK le o teclado virtual (ele ja
 * mapeia teclado de fabrica). Uso: evdev2key /dev/input/event3
 *
 * Mapa (HK / menus): dpad+analog -> setas; botoes -> enter/space/z/x/esc.
 */
#include <linux/input.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static int uf;
static void emit(int type, int code, int val) {
  struct input_event e; memset(&e, 0, sizeof e);
  e.type = type; e.code = code; e.value = val;
  if (write(uf, &e, sizeof e) < 0) {}
}
static void key(int k, int down) {
  emit(EV_KEY, k, down); emit(EV_SYN, SYN_REPORT, 0);
  fprintf(stderr, "evdev2key: KEY %d %s\n", k, down ? "DOWN" : "up");
}

/* botao do gamepad -> tecla. Cobre BTN_JOYSTICK(0x120+) e BTN_GAMEPAD(0x130+). */
static int btn_to_key(int code) {
  switch (code) {
    /* mapeia varios botoes p/ confirmar/cancelar/pular (HK menus + in-game) */
    case 0x130: /* BTN_SOUTH/A */      return KEY_ENTER;
    case 0x131: /* BTN_EAST/B */       return KEY_ESC;
    case 0x133: /* BTN_WEST/X */       return KEY_Z;
    case 0x134: /* BTN_NORTH/Y */      return KEY_X;
    case 0x136: /* BTN_TL/L1 */        return KEY_LEFTSHIFT;
    case 0x137: /* BTN_TR/R1 */        return KEY_C;
    case 0x13a: /* BTN_SELECT */       return KEY_BACKSPACE;
    case 0x13b: /* BTN_START */        return KEY_ENTER;
    /* gamepads genericos (BTN_JOYSTICK / BTN_TRIGGER_HAPPY) */
    case 0x120: return KEY_ENTER;  case 0x121: return KEY_Z;
    case 0x122: return KEY_X;      case 0x123: return KEY_ESC;
    case 0x124: return KEY_ENTER;  case 0x125: return KEY_Z;
    case 0x126: return KEY_ENTER;  case 0x127: return KEY_ESC;
    case 0x128: return KEY_LEFTSHIFT; case 0x129: return KEY_C;
    case 0x12a: return KEY_BACKSPACE; case 0x12b: return KEY_ENTER;
    default: return KEY_ENTER; /* qualquer outro botao confirma */
  }
}

/* eixo analogico/hat -> par de teclas (neg,pos). retorna 1 se eh eixo conhecido. */
static int axis_keys(int code, int *kneg, int *kpos) {
  switch (code) {
    case ABS_X: case ABS_HAT0X: case ABS_RX: *kneg = KEY_LEFT; *kpos = KEY_RIGHT; return 1;
    case ABS_Y: case ABS_HAT0Y: case ABS_RY: *kneg = KEY_UP;   *kpos = KEY_DOWN;  return 1;
    default: return 0;
  }
}

int main(int argc, char **argv) {
  const char *dev = argc > 1 ? argv[1] : "/dev/input/event3";
  int fd = open(dev, O_RDONLY);
  if (fd < 0) { fprintf(stderr, "evdev2key: open %s falhou: %s\n", dev, strerror(errno)); return 1; }

  /* ranges dos eixos (min/max) p/ thresholds */
  struct absinfo { int min, max, flat; } ax[ABS_MAX + 1];
  for (int c = 0; c <= ABS_MAX; c++) { ax[c].min = 0; ax[c].max = 255; ax[c].flat = 0; }
  for (int c = 0; c <= ABS_MAX; c++) {
    struct input_absinfo ai;
    if (ioctl(fd, EVIOCGABS(c), &ai) == 0) { ax[c].min = ai.minimum; ax[c].max = ai.maximum; ax[c].flat = ai.flat; }
  }

  uf = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (uf < 0) { fprintf(stderr, "evdev2key: /dev/uinput: %s\n", strerror(errno)); return 1; }
  ioctl(uf, UI_SET_EVBIT, EV_KEY); ioctl(uf, UI_SET_EVBIT, EV_SYN);
  int ks[] = { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_SPACE, KEY_ESC,
               KEY_Z, KEY_X, KEY_C, KEY_BACKSPACE, KEY_LEFTSHIFT, KEY_A, KEY_S,
               KEY_W, KEY_D, KEY_TAB };
  for (size_t i = 0; i < sizeof(ks) / sizeof(ks[0]); i++) ioctl(uf, UI_SET_KEYBIT, ks[i]);
  struct uinput_user_dev uud; memset(&uud, 0, sizeof uud);
  strcpy(uud.name, "hk-evdev-kbd");
  uud.id.bustype = BUS_USB; uud.id.vendor = 0x1234; uud.id.product = 0x5678; uud.id.version = 1;
  if (write(uf, &uud, sizeof uud) < 0) {}
  ioctl(uf, UI_DEV_CREATE);
  fprintf(stderr, "evdev2key: %s -> teclado virtual 'hk-evdev-kbd' OK\n", dev);

  /* estado por-eixo (-1/0/1) p/ emitir down/up nas bordas */
  int axstate[ABS_MAX + 1]; for (int c = 0; c <= ABS_MAX; c++) axstate[c] = 0;

  struct input_event ev;
  while (read(fd, &ev, sizeof ev) == (int)sizeof ev) {
    if (ev.type == EV_KEY && ev.code >= 0x100) {
      int k = btn_to_key(ev.code);
      if (ev.value == 0 || ev.value == 1) key(k, ev.value);
    } else if (ev.type == EV_ABS) {
      int kneg, kpos;
      if (!axis_keys(ev.code, &kneg, &kpos)) continue;
      int lo = ax[ev.code].min, hi = ax[ev.code].max, mid = (lo + hi) / 2;
      int span = (hi - lo) ? (hi - lo) : 255;
      int dir = 0;
      if (ev.value <= mid - span / 4) dir = -1;
      else if (ev.value >= mid + span / 4) dir = 1;
      if (dir != axstate[ev.code]) {
        if (axstate[ev.code] == -1) key(kneg, 0);
        else if (axstate[ev.code] == 1) key(kpos, 0);
        if (dir == -1) key(kneg, 1);
        else if (dir == 1) key(kpos, 1);
        axstate[ev.code] = dir;
      }
    }
  }
  fprintf(stderr, "evdev2key: device fechou\n");
  return 0;
}
