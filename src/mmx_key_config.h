#pragma once
/* KEY CONFIG: the title screen's X8-style action bindings.
 *
 * The launcher maps a device (keyboard or pad) onto a modern virtual pad: the
 * SNES twelve plus L2, R2, L3, R3 and the right stick. KEY CONFIG maps that
 * pad's twenty controls onto the game's twenty actions, using the X8 16-bit
 * demake's list, labels and defaults. The mapping runs on the host before a
 * seat's input is sent anywhere (SnesDesktopHostGame.map_seat_input), so
 * replays and netplay carry the actions, never a player's bindings.
 *
 * Actions with a native button become that button (shot = Y, jump = B,
 * dash = A, shot2 = X, prev/next weapon = L/R, pause menu = Start, Change
 * Player = Select); X1's own button masks at $7E:FFC0 stay at their
 * defaults so those bits mean exactly that. The rest arrive through
 * RtlGetPadExt as MMX_EXT_* bits. */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  MMX_KEY_LEFT, MMX_KEY_RIGHT, MMX_KEY_UP, MMX_KEY_DOWN,
  MMX_KEY_SHOT, MMX_KEY_SHOT2, MMX_KEY_JUMP, MMX_KEY_DASH,
  MMX_KEY_CHANGE_PLAYER, MMX_KEY_ARMOR, MMX_KEY_PREV, MMX_KEY_NEXT,
  MMX_KEY_BUSTER, MMX_KEY_WHEEL_LEFT, MMX_KEY_WHEEL_RIGHT, MMX_KEY_WHEEL_UP,
  MMX_KEY_WHEEL_DOWN, MMX_KEY_PAUSE, MMX_KEY_CONFIRM, MMX_KEY_CANCEL,
  MMX_KEY_ACTIONS
};

/* The virtual pad's controls, in X8's (Godot's) names. */
enum {
  MMX_PAD_DPAD_UP, MMX_PAD_DPAD_DOWN, MMX_PAD_DPAD_LEFT, MMX_PAD_DPAD_RIGHT,
  MMX_PAD_FACE_BOTTOM, MMX_PAD_FACE_RIGHT, MMX_PAD_FACE_LEFT, MMX_PAD_FACE_TOP,
  MMX_PAD_L, MMX_PAD_R, MMX_PAD_SELECT, MMX_PAD_START,
  MMX_PAD_L2, MMX_PAD_R2, MMX_PAD_L3, MMX_PAD_R3,
  MMX_PAD_RS_UP, MMX_PAD_RS_DOWN, MMX_PAD_RS_LEFT, MMX_PAD_RS_RIGHT,
  MMX_PAD_CONTROLS
};

/* Mapped actions with no SNES button (RtlGetPadExt after mapping). */
enum {
  MMX_EXT_ARMOR = 1 << 0,
  MMX_EXT_BUSTER = 1 << 1,
  MMX_EXT_WHEEL_LEFT = 1 << 2,
  MMX_EXT_WHEEL_RIGHT = 1 << 3,
  MMX_EXT_WHEEL_UP = 1 << 4,
  MMX_EXT_WHEEL_DOWN = 1 << 5,
  MMX_EXT_CONFIRM = 1 << 6,
  MMX_EXT_CANCEL = 1 << 7,
};
/* Every ext bit at once: a seat leaving KEY CONFIG (see MmxKeyConfigClosing).
 * Only sent while the title's KEY CONFIG page is open, where no action is
 * read. */
#define MMX_EXT_CLOSE_KEY_CONFIG 0xff

const char *MmxKeyConfigActionName(unsigned action);
const char *MmxKeyConfigControlName(unsigned control);
unsigned MmxKeyConfigBinding(unsigned action);
void MmxKeyConfigSetBinding(unsigned action, unsigned control);
void MmxKeyConfigResetDefaults(void);
/* key_config.ini beside the executable. Load reads it (defaults when absent);
 * Save writes the current bindings. */
void MmxKeyConfigLoad(void);
void MmxKeyConfigSave(void);

/* SnesDesktopHostGame.map_seat_input. */
void MmxKeyConfigMapSeat(int seat, uint16_t *snes, uint8_t *ext);

/* The page itself. The title opens it in simulation state (Boss Rush state
 * menu == 3); this side follows, drives the cursor from the local seats and
 * signals the close. */
bool MmxKeyConfigPageOpen(void);
/* Draw the page over the frame (width x 224 ARGB). */
void MmxKeyConfigDraw(uint32_t *out, int width);

/* Once per guest frame, before X1 reads its pad: KEY CONFIG replaces X1's
 * own button assignment, so its masks ($7E:FFC0..5) keep their defaults
 * (shot Y, jump B, dash A, L, R, Start). */
void MmxKeyConfigSimFrame(uint8_t ram[0x20000]);

struct RecompLauncherCGameInfo;
/* SnesDesktopHostGame.configure_launcher: the modern-pad controller page,
 * then the netplay wiring. */
void MmxConfigureLauncher(struct RecompLauncherCGameInfo *info);

#ifdef __cplusplus
}
#endif
