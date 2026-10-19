#include "mmx_key_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common_rtl.h"
#include "host_paths.h"
#include "recomp_launcher.h"
#include "snes/ppu.h"
#include "snes_overlay_draw.h"
#include "mmx_boss_rush.h"
#include "mmx_netplay.h"
#include "mmx_replay.h"
#include "mmx_wide_policy.h"
#include "mmx_weapons.h"

/* X8 16-bit's ActionLoader order and English labels. Its cancel row has no
 * translation ("CANCEL_ACT"); it reads as the action it is. */
static const char *const k_action_names[MMX_KEY_ACTIONS] = {
  "LEFT", "RIGHT", "UP", "DOWN", "SHOT", "SHOT2", "JUMP", "DASH",
  "CHANGE PLAYER", "ARMOR ABILITY", "PREV WEAPON", "NEXT WEAPON",
  "SELECT BUSTER", "WHEEL LEFT", "WHEEL RIGHT", "WHEEL UP", "WHEEL DOWN",
  "PAUSE MENU", "MENU CONFIRM", "CANCEL",
};
/* Godot's joypad names, as X8 shows them. The right stick is listed by
 * direction: a binding is one direction of it, never the whole axis. */
static const char *const k_control_names[MMX_PAD_CONTROLS] = {
  "DPAD UP", "DPAD DOWN", "DPAD LEFT", "DPAD RIGHT",
  "FACE BUTTON BOTTOM", "FACE BUTTON RIGHT", "FACE BUTTON LEFT", "FACE BUTTON TOP",
  "L", "R", "SELECT", "START", "L2", "R2", "L3", "R3",
  "RIGHT STICK UP", "RIGHT STICK DOWN", "RIGHT STICK LEFT", "RIGHT STICK RIGHT",
};
/* Config-file spellings of the same. */
static const char *const k_control_keys[MMX_PAD_CONTROLS] = {
  "DpadUp", "DpadDown", "DpadLeft", "DpadRight",
  "FaceBottom", "FaceRight", "FaceLeft", "FaceTop",
  "L", "R", "Select", "Start", "L2", "R2", "L3", "R3",
  "RsUp", "RsDown", "RsLeft", "RsRight",
};
static const char *const k_action_keys[MMX_KEY_ACTIONS] = {
  "move_left", "move_right", "move_up", "move_down", "fire", "alt_fire",
  "jump", "dash", "char_switch", "select_special", "weapon_select_left",
  "weapon_select_right", "reset_weapon", "analog_left", "analog_right",
  "analog_up", "analog_down", "pause", "ui_accept", "ui_cancel",
};
/* X8 16-bit's joypad defaults, except Change Player on Select (the recomp's
 * long-standing swap button) and cancel on Face Button Bottom. */
static const uint8_t k_defaults[MMX_KEY_ACTIONS] = {
  MMX_PAD_DPAD_LEFT, MMX_PAD_DPAD_RIGHT, MMX_PAD_DPAD_UP, MMX_PAD_DPAD_DOWN,
  MMX_PAD_FACE_LEFT, MMX_PAD_FACE_TOP, MMX_PAD_FACE_BOTTOM, MMX_PAD_FACE_RIGHT,
  MMX_PAD_SELECT, MMX_PAD_R2, MMX_PAD_L, MMX_PAD_R,
  MMX_PAD_R3, MMX_PAD_RS_LEFT, MMX_PAD_RS_RIGHT, MMX_PAD_RS_UP,
  MMX_PAD_RS_DOWN, MMX_PAD_START, MMX_PAD_FACE_LEFT, MMX_PAD_FACE_BOTTOM,
};
/* Each control's bit in the runner word (bits 0..11) or ext (12..19). */
static const uint8_t k_control_bit[MMX_PAD_CONTROLS] = {
  4, 5, 6, 7, 0, 8, 1, 9, 10, 11, 2, 3, 12, 13, 14, 15, 16, 17, 18, 19,
};

static uint8_t bindings[MMX_KEY_ACTIONS];
static bool loaded;

const char *MmxKeyConfigActionName(unsigned action) {
  return action < MMX_KEY_ACTIONS ? k_action_names[action] : "";
}
const char *MmxKeyConfigControlName(unsigned control) {
  return control < MMX_PAD_CONTROLS ? k_control_names[control] : "";
}
void MmxKeyConfigResetDefaults(void) {
  memcpy(bindings, k_defaults, sizeof(bindings));
  loaded = true;
}
static void ensure_loaded(void) {
  if (!loaded) MmxKeyConfigLoad();
}
unsigned MmxKeyConfigBinding(unsigned action) {
  ensure_loaded();
  return action < MMX_KEY_ACTIONS ? bindings[action] : 0;
}
void MmxKeyConfigSetBinding(unsigned action, unsigned control) {
  ensure_loaded();
  if (action < MMX_KEY_ACTIONS && control < MMX_PAD_CONTROLS)
    bindings[action] = (uint8_t)control;
}

static bool config_path(char *out, size_t cap) {
  return snesrecomp_exe_dir_path("key_config.ini", out, cap) == 0 ||
         (snprintf(out, cap, "key_config.ini"), true);
}
static char *trim(char *s) {
  while (*s == ' ' || *s == '\t') ++s;
  char *e = s + strlen(s);
  while (e > s && (e[-1] == '\r' || e[-1] == '\n' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
  return s;
}
static bool same(const char *a, const char *b) {
  for (; *a && *b; ++a, ++b)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
  return !*a && !*b;
}
void MmxKeyConfigLoad(void) {
  MmxKeyConfigResetDefaults();
  char path[1024];
  config_path(path, sizeof(path));
  FILE *f = fopen(path, "r");
  if (!f) return;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    char *p = trim(line);
    if (!*p || *p == '#' || *p == ';' || *p == '[') continue;
    char *eq = strchr(p, '=');
    if (!eq) continue;
    *eq = 0;
    char *key = trim(p), *value = trim(eq + 1);
    for (unsigned a = 0; a < MMX_KEY_ACTIONS; ++a) {
      if (!same(key, k_action_keys[a])) continue;
      for (unsigned c = 0; c < MMX_PAD_CONTROLS; ++c)
        if (same(value, k_control_keys[c])) bindings[a] = (uint8_t)c;
      break;
    }
  }
  fclose(f);
}
void MmxKeyConfigSave(void) {
  char path[1024];
  config_path(path, sizeof(path));
  FILE *f = fopen(path, "w");
  if (!f) return;
  fprintf(f,
      "# KEY CONFIG (title screen). Each action names one control of the\n"
      "# controller page's pad: DpadUp DpadDown DpadLeft DpadRight FaceBottom\n"
      "# FaceRight FaceLeft FaceTop L R Select Start L2 R2 L3 R3 RsUp RsDown\n"
      "# RsLeft RsRight. Delete this file for the defaults.\n"
      "[KeyConfig]\n");
  for (unsigned a = 0; a < MMX_KEY_ACTIONS; ++a)
    fprintf(f, "%s = %s\n", k_action_keys[a], k_control_keys[bindings[a]]);
  fclose(f);
}

/* ---- the page ------------------------------------------------------------ */

enum { ROW_DEFAULT = MMX_KEY_ACTIONS, ROW_EXIT, PAGE_ROWS };
typedef struct {
  bool open;          /* following the simulation's open page */
  bool closing;       /* left the page: hold zeros until the pad rests */
  bool signalling;    /* sending MMX_EXT_CLOSE_KEY_CONFIG */
  bool capturing;     /* waiting for a control for `cursor` */
  bool armed;         /* capture saw the pad at rest */
  unsigned rest;      /* closing: consecutive calls with nothing held */
  bool dirty;
  unsigned cursor;
  uint32_t held[2];   /* each local seat's controls last frame */
  unsigned blink;
} Page;
static Page page;

bool MmxKeyConfigPageOpen(void) { return page.open; }

static bool sim_open(void) { return MmxBossRushGetState().menu == 3; }

static uint32_t controls_of(uint16_t snes, uint8_t ext) {
  return (uint32_t)(snes & 0x0fff) | ((uint32_t)ext << 12);
}
static bool control_held(uint32_t phys, unsigned control) {
  return control < MMX_PAD_CONTROLS && (phys >> k_control_bit[control] & 1);
}
static bool action_held(uint32_t phys, unsigned action) {
  return control_held(phys, bindings[action]);
}
static int first_control(uint32_t phys) {
  for (unsigned c = 0; c < MMX_PAD_CONTROLS; ++c)
    if (control_held(phys, c)) return (int)c;
  return -1;
}

static void page_input(int seat, uint32_t phys) {
  uint32_t *last = &page.held[seat & 1];
  uint32_t pressed = phys & ~*last;
  *last = phys;
  if (page.closing) return;
  if (page.capturing) {
    /* The press that opened the capture must be released first. */
    if (!page.armed) { if (!phys) page.armed = true; return; }
    int c = first_control(pressed);
    if (c < 0) return;
    bindings[page.cursor] = (uint8_t)c;
    page.capturing = false;
    page.dirty = true;
    return;
  }
  bool up = control_held(pressed, MMX_PAD_DPAD_UP) || action_held(pressed, MMX_KEY_UP);
  bool down = control_held(pressed, MMX_PAD_DPAD_DOWN) || action_held(pressed, MMX_KEY_DOWN);
  bool confirm = action_held(pressed, MMX_KEY_CONFIRM) || control_held(pressed, MMX_PAD_START);
  bool cancel = action_held(pressed, MMX_KEY_CANCEL);
  if (up) page.cursor = (page.cursor + PAGE_ROWS - 1) % PAGE_ROWS;
  else if (down) page.cursor = (page.cursor + 1) % PAGE_ROWS;
  else if (confirm && page.cursor < MMX_KEY_ACTIONS) {
    page.capturing = true;
    page.armed = false;
  } else if (confirm && page.cursor == ROW_DEFAULT) {
    MmxKeyConfigResetDefaults();
    page.dirty = true;
  } else if ((confirm && page.cursor == ROW_EXIT) || cancel) {
    page.closing = true;
  }
}

static void page_follow_sim(void) {
  bool open = sim_open();
  if (open && !page.open && !page.signalling && !MmxReplayPlaying()) {
    memset(&page, 0, sizeof(page));
    page.open = true;
    page.held[0] = page.held[1] = 0xfffff;   /* the opening press is held */
  } else if (!open && (page.open || page.signalling)) {
    /* Closed by another seat (netplay) or by the signal landing. */
    if (page.dirty) MmxKeyConfigSave();
    memset(&page, 0, sizeof(page));
  }
}

void MmxKeyConfigMapSeat(int seat, uint16_t *snes, uint8_t *ext) {
  ensure_loaded();
  page_follow_sim();
  uint32_t phys = controls_of(*snes, *ext);
  if (page.open || page.signalling) {
    if (page.open) page_input(seat, phys);
    if (page.closing) page.rest = phys ? 0 : page.rest + 1;
    if (page.closing && page.rest >= 2) {
      /* The pad is at rest: let the title close the page. */
      page.open = false;
      page.closing = false;
      page.signalling = true;
    }
    *snes = 0;
    *ext = page.signalling ? MMX_EXT_CLOSE_KEY_CONFIG : 0;
    return;
  }

  uint16_t out = 0;
  uint8_t x = 0;
  static const struct { uint8_t action, bit; } k_native[] = {
    {MMX_KEY_UP, 4}, {MMX_KEY_DOWN, 5}, {MMX_KEY_LEFT, 6}, {MMX_KEY_RIGHT, 7},
    {MMX_KEY_JUMP, 0}, {MMX_KEY_SHOT, 1}, {MMX_KEY_CHANGE_PLAYER, 2},
    {MMX_KEY_PAUSE, 3}, {MMX_KEY_DASH, 8}, {MMX_KEY_SHOT2, 9},
    {MMX_KEY_PREV, 10}, {MMX_KEY_NEXT, 11},
  };
  for (unsigned i = 0; i < sizeof(k_native) / sizeof(k_native[0]); ++i)
    if (action_held(phys, k_native[i].action)) out |= (uint16_t)(1u << k_native[i].bit);
  static const struct { uint8_t action, bit; } k_ext[] = {
    {MMX_KEY_ARMOR, MMX_EXT_ARMOR}, {MMX_KEY_BUSTER, MMX_EXT_BUSTER},
    {MMX_KEY_WHEEL_LEFT, MMX_EXT_WHEEL_LEFT}, {MMX_KEY_WHEEL_RIGHT, MMX_EXT_WHEEL_RIGHT},
    {MMX_KEY_WHEEL_UP, MMX_EXT_WHEEL_UP}, {MMX_KEY_WHEEL_DOWN, MMX_EXT_WHEEL_DOWN},
    {MMX_KEY_CONFIRM, MMX_EXT_CONFIRM}, {MMX_KEY_CANCEL, MMX_EXT_CANCEL},
  };
  for (unsigned i = 0; i < sizeof(k_ext) / sizeof(k_ext[0]); ++i)
    if (action_held(phys, k_ext[i].action)) x |= k_ext[i].bit;
  /* X1's menus confirm with Start. Outside a stage (title, stage select,
   * password, options) and on the pause menu, menu confirm is Start too. */
  if ((x & MMX_EXT_CONFIRM) &&
      (!MmxWidePolicy_IsStageScene(g_ram) || MmxWeaponsMenuVisible(g_ram)))
    out |= SNES_PAD_START;
  /* Never all eight at once: that is KEY CONFIG's close signal. */
  if (x == MMX_EXT_CLOSE_KEY_CONFIG) x &= (uint8_t)~MMX_EXT_CANCEL;
  *snes = out;
  *ext = x;
}

/* ---- drawing --------------------------------------------------------------
 * X1's options font is the title's BG3 character set, indexed by ASCII. The
 * title's own two text palettes: cyan rows, orange for the selected one. */
static void draw_text(uint32_t *out, int width, int x, int y, const char *s, unsigned palette) {
  const Ppu *p = g_ppu;
  if (!p) return;
  unsigned base = PPU_bgTileAdr(p, 2);
  unsigned bright = PPU_brightness(p);
  for (unsigned n = 0; s[n]; ++n) {
    unsigned ch = (unsigned char)toupper((unsigned char)s[n]);
    for (int row = 0; row < 8; ++row) {
      if (y + row < 0 || y + row >= 224) continue;
      unsigned a = (base + ch * 8 + row) & 0x7fff;
      for (int col = 0; col < 8; ++col) {
        int dx = x + (int)n * 8 + col;
        if (dx < 0 || dx >= width) continue;
        unsigned bits = p->vram[a] >> (7 - col);
        unsigned pixel = (bits & 1) | ((bits >> 7) & 2);
        if (!pixel) continue;
        uint16_t color = p->cgram[(palette * 4 + pixel) & 0xff];
        uint32_t rgb = 0;
        for (unsigned c = 0; c < 3; ++c) {
          unsigned v = (color >> (c * 5)) & 31;
          v = (v << 3) | (v >> 2);
          v = v * bright / 15;
          rgb |= v << (16 - c * 8);
        }
        out[(y + row) * width + dx] = rgb;
      }
    }
  }
}

void MmxKeyConfigDraw(uint32_t *out, int width) {
  if (!out || width < 256) return;
  memset(out, 0, (size_t)width * 224 * sizeof(*out));
  int left = (width - 256) / 2;
  ++page.blink;
  draw_text(out, width, left + 88, 8, "KEY CONFIG", 1);
  /* X8's list: the selected row lights up, actions on the left and their
   * controls flush right. At 32 columns the longest of each ("CHANGE
   * PLAYER", "FACE BUTTON BOTTOM") still leave a column between them. */
  for (unsigned i = 0; i < PAGE_ROWS; ++i) {
    int y = 24 + 8 * (int)i + (i >= ROW_DEFAULT ? 8 : 0);
    unsigned palette = i == page.cursor ? 1 : 0;
    if (i < MMX_KEY_ACTIONS) {
      draw_text(out, width, left, y, k_action_names[i], palette);
      const char *value = k_control_names[MmxKeyConfigBinding(i)];
      if (page.capturing && i == page.cursor)
        value = (page.blink / 16) & 1 ? "" : "PRESS A BUTTON";
      draw_text(out, width, left + 256 - 8 * (int)strlen(value), y, value, palette);
    } else {
      draw_text(out, width, left, y, i == ROW_DEFAULT ? "DEFAULT" : "EXIT", palette);
    }
  }
}

void MmxKeyConfigSimFrame(uint8_t ram[0x20000]) {
  static const uint8_t k_masks[6] = {0x40, 0x80, 0x20, 0x08, 0x04, 0x01};
  memcpy(ram + 0xffc0, k_masks, sizeof(k_masks));
}

void MmxConfigureLauncher(struct RecompLauncherCGameInfo *info) {
  if (!info) return;
  info->snes_modern_pad = 1;
  MmxNetplayConfigureLauncher(info);
}
