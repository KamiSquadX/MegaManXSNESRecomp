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
  uint8_t no_double_tap;  /* 0 = X1's double-tap dash */
  uint8_t hold_dash;      /* 0 = X1's press-to-dash wall kick */
  uint8_t reserved[2];
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

#ifdef __cplusplus
}
#endif
