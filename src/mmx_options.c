#include "mmx_options.h"

#include <stdio.h>
#include <string.h>

#include "common_rtl.h"
#include "cpu_state.h"
#include "host_paths.h"
#include "snes/interp_bridge.h"
#include "mmx_replay.h"
#include "netplay/snes_netplay.h"

extern int snes_frame_counter;

static MmxOptionsState state;

MmxOptionsState MmxOptionsGetState(void) { return state; }
bool MmxOptionsValidState(const MmxOptionsState *s) {
  return s && s->no_double_tap <= 1 && s->hold_dash <= 1 &&
         !s->reserved[0] && !s->reserved[1];
}
bool MmxOptionsSetState(const MmxOptionsState *s) {
  if (!MmxOptionsValidState(s)) return false;
  state = *s;
  return true;
}
void MmxOptionsUseDefaults(void) { memset(&state, 0, sizeof(state)); }

static void file_path(char *out, size_t cap) {
  if (snesrecomp_exe_dir_path("mmx_options.ini", out, cap) != 0)
    snprintf(out, cap, "mmx_options.ini");
}
static bool value_on(const char *v) {
  while (*v == ' ' || *v == '\t') ++v;
  return (v[0] == 'o' || v[0] == 'O') && (v[1] == 'n' || v[1] == 'N');
}
void MmxOptionsLoadPreferences(void) {
  MmxOptionsUseDefaults();
  char path[1024];
  file_path(path, sizeof(path));
  FILE *f = fopen(path, "r");
  if (!f) return;
  char line[128];
  while (fgets(line, sizeof(line), f)) {
    char *eq = strchr(line, '=');
    if (!eq || line[0] == '#') continue;
    if (!strncmp(line, "double_tap_dash", 15)) state.no_double_tap = !value_on(eq + 1);
    else if (!strncmp(line, "hold_dash", 9)) state.hold_dash = value_on(eq + 1);
  }
  fclose(f);
}
static void save_preferences(void) {
  /* A replay or a remote seat's change is not this player's choice. */
  if (MmxReplayPlaying() || snes_netplay_active()) return;
  char path[1024];
  file_path(path, sizeof(path));
  FILE *f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "# OPTION MODE (title screen)\n"
             "double_tap_dash = %s\nhold_dash = %s\n",
          state.no_double_tap ? "off" : "on", state.hold_dash ? "on" : "off");
  fclose(f);
}

/* ---- gameplay ------------------------------------------------------------
 * X's update runs with D = $0BA8. */

/* $81:9742: X1's second-tap check compares the direction armed by the first
 * tap ($0BFB). Nothing armed, nothing matches. */
static void double_tap_hook(CpuState *cpu, uint32_t pc24) {
  (void)pc24;
  if (state.no_double_tap) g_ram[(uint16_t)(cpu->D + 0x53)] = 0;
}
/* $81:8767: the wall kick takes dash speed when the dash window ($0BFE,
 * opened for ten frames by a dash press in the air) is still open. Held
 * dash ($0BDE bit 7) keeps it open. */
static void hold_dash_hook(CpuState *cpu, uint32_t pc24) {
  (void)pc24;
  uint16_t d = cpu->D;
  if (state.hold_dash && (g_ram[(uint16_t)(d + 0x36)] & 0x80) && !g_ram[(uint16_t)(d + 0x56)])
    g_ram[(uint16_t)(d + 0x56)] = 1;
}

/* ---- the options screen ($80:EA07) ---------------------------------------
 * X1's cursor is $7E:FF80: rows 0..5 its key configuration, 6 SOUND MODE,
 * 7 BGM, 8 S.E., 9 EXIT. Rows 0 and 1 become DOUBLE TAP DASH and HOLD DASH
 * and rows 2..5 are skipped. */
static int screen_frame = -1000;
bool MmxOptionsScreenActive(void) {
  /* The loop yields each frame; a short tail covers the closing fade. */
  return snes_frame_counter - screen_frame >= 0 && snes_frame_counter - screen_frame < 60;
}
static void screen_hook(CpuState *cpu, uint32_t pc24) {
  (void)cpu; (void)pc24;
  screen_frame = snes_frame_counter;
}
/* $80:EAA3, before the frame's presses are read. */
static void option_input_hook(CpuState *cpu, uint32_t pc24) {
  (void)cpu; (void)pc24;
  screen_frame = snes_frame_counter;
  uint8_t row = g_ram[0xff80];
  if (row > 1) return;
  bool change = (g_ram[0xac] & 0xd3) || (g_ram[0xab] & 0x80); /* B Y Start Left Right A */
  if (change) {
    if (row == 0) state.no_double_tap ^= 1;
    else state.hold_dash ^= 1;
    save_preferences();
  }
  /* X1's handler for these rows reassigns a button on any press. */
  g_ram[0xab] = 0;
  g_ram[0xac] &= 0x0c;
}
/* $80:EAC6, after X1 moved the cursor (a press or its hold repeat). */
static void option_cursor_hook(CpuState *cpu, uint32_t pc24) {
  (void)cpu; (void)pc24;
  uint8_t row = g_ram[0xff80], from = g_ram[0xff81];
  if (row >= 2 && row <= 5) g_ram[0xff80] = from <= 1 ? 6 : 1;
}

/* Generated player code calls this at the same two blocks
 * (tools/apply_options_hooks.py). */
void MmxOptionsHook(CpuState *cpu, uint32_t pc24) {
  if ((pc24 & 0x7fffff) == 0x019742) double_tap_hook(cpu, pc24);
  else if ((pc24 & 0x7fffff) == 0x018767) hold_dash_hook(cpu, pc24);
}

void MmxOptionsRegisterHooks(void) {
  interp_bridge_add_pre_opcode_hook(0x819742, double_tap_hook);
  interp_bridge_add_pre_opcode_hook(0x818767, hold_dash_hook);
  interp_bridge_add_pre_opcode_hook(0x80ea07, screen_hook);
  interp_bridge_add_pre_opcode_hook(0x80eaa3, option_input_hook);
  interp_bridge_add_pre_opcode_hook(0x80eac6, option_cursor_hook);
}
