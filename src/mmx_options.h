#pragma once
/* OPTION MODE's quality-of-life rows, which replace X1's KEY CONFIG. section
 * (KEY CONFIG is its own title entry now, see mmx_key_config.h).
 *
 *   DOUBLE TAP DASH  ON  (X1)  / OFF: only the dash button dashes.
 *   HOLD DASH        OFF (X1)  / ON: a wall kick with dash held is a dash
 *                                    wall kick, as in X2 onwards.
 *
 * Both change the simulation, so they are snapshot state (the game chunk),
 * changed only by guest input on the options screen. The player's choice is
 * remembered in mmx_options.ini and applied at launch; online sessions start
 * from the defaults so every peer agrees. */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MmxOptionsState {
  uint8_t no_double_tap;  /* 0 = double-tap dash */
  uint8_t hold_dash;      /* 0 = X1's press-to-dash wall kick */
  uint8_t reserved[2];
  /* Double-tap tracking per seat (0, 1): left/right as bits 1 and 2. */
  uint8_t tap_held[2];    /* held last frame */
  uint8_t tap_dir[2];     /* armed by a first tap */
  uint8_t tap_timer[2];   /* frames left for the second tap */
  uint8_t tap_dashing[2]; /* the direction whose hold keeps dash held */
} MmxOptionsState;

MmxOptionsState MmxOptionsGetState(void);
bool MmxOptionsValidState(const MmxOptionsState *state);
bool MmxOptionsSetState(const MmxOptionsState *state);
/* X1's behavior (no file, online sessions). */
void MmxOptionsUseDefaults(void);
/* mmx_options.ini beside the executable into the state. */
void MmxOptionsLoadPreferences(void);
/* Install the X1 code hooks (idempotent). */
void MmxOptionsRegisterHooks(void);
/* The options screen is up (presentation only). */
bool MmxOptionsScreenActive(void);
/* RtlGameInfo.frame_begin: the replay hook, then DOUBLE TAP DASH. A second
 * tap of left or right becomes the dash button, held while that direction
 * is, so everything that answers dash (a saber combo's cancel, the air
 * dash window) answers a double tap too. Runs on the recorded word, so
 * replays and netplay reproduce it. */
bool MmxOptionsFrameBegin(uint32_t *inputs);
/* Host frame_end: Axl's post-frame work, then the replay. */
void MmxOptionsFrameEnd(void);

#ifdef __cplusplus
}
#endif
