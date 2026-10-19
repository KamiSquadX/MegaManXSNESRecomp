// consoles/snes/snes_profile.h — the SNES console unit, in one file:
// pad vocabulary, SystemProfile row, name aliases, and the ABI capability
// defaults launcher_profile_apply() writes for "snes".
//
// Include via src/launcher_system.h (profiles + lookups) or
// src/launcher_profile.h (ABI apply) — consumers never include this directly.

#ifndef RUI_CONSOLE_SNES_PROFILE_H
#define RUI_CONSOLE_SNES_PROFILE_H

#include "launcher_system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- pad vocabulary (rebind page) ---------------------------------------------
// Mirrors launcher_button_name()/kButtonNames in launcher_model.c (kept in sync
// by hand — small, stable table). The live carve still calls
// launcher_button_name() directly for the on-screen label (single source of
// truth for TEXT); this table exists so ControllerSpec is a complete,
// self-describing per-system unit as the architecture calls for.
static const ButtonDef kSnesPadButtons[] = {
    { "Up", 0 }, { "Down", 1 }, { "Left", 2 }, { "Right", 3 },
    { "A", 4 }, { "B", 5 }, { "X", 6 }, { "Y", 7 },
    { "L", 8 }, { "R", 9 }, { "Start", 10 }, { "Select", 11 },
};
#define LNG_SNES_PAD_BUTTON_COUNT ((int)(sizeof(kSnesPadButtons) / sizeof(kSnesPadButtons[0])))

// ---- Gamepad Bindings panel order (column-major: down, then next column) ----
// Indices into kSnesPadButtons. Twelve buttons divide exactly into the three
// groups a SNES pad physically has:
//   Col1: D-pad        Col2: face        Col3: shoulders + Start/Select
static const int kSnesGamepadBindOrder[LNG_SNES_PAD_BUTTON_COUNT] = {
    0, 1, 2, 3,      // Up Down Left Right
    4, 5, 6, 7,      // A B X Y
    8, 9, 10, 11,    // L R Start Select
};
#define LNG_SNES_GAMEPAD_BIND_COLS 3
#define LNG_SNES_GAMEPAD_BIND_ROWS 4

// ---- modern pad (GameInfo.snes_modern_pad) ------------------------------------
// The SNES twelve, then a modern pad's extras. Indices 12..19 are the
// keybinds.ini l2, r2, l3, r3, rs_up, rs_down, rs_left, rs_right slots and
// the [GamepadMap] ControlsExt line, in that order.
static const ButtonDef kSnesModernPadButtons[] = {
    { "Up", 0 }, { "Down", 1 }, { "Left", 2 }, { "Right", 3 },
    { "A", 4 }, { "B", 5 }, { "X", 6 }, { "Y", 7 },
    { "L", 8 }, { "R", 9 }, { "Start", 10 }, { "Select", 11 },
    { "L2", 12 }, { "R2", 13 }, { "L3", 14 }, { "R3", 15 },
    { "R.S. Up", 16 }, { "R.S. Down", 17 }, { "R.S. Left", 18 }, { "R.S. Right", 19 },
};
#define LNG_SNES_MODERN_PAD_BUTTON_COUNT \
    ((int)(sizeof(kSnesModernPadButtons) / sizeof(kSnesModernPadButtons[0])))

// A fourth column for the triggers and stick clicks; the right stick stacks
// under it, beside Auto Map / Reset to Defaults.
static const int kSnesModernGamepadBindOrder[LNG_SNES_MODERN_PAD_BUTTON_COUNT] = {
    0, 1, 2, 3,          // Up Down Left Right
    4, 5, 6, 7,          // A B X Y
    8, 9, 10, 11,        // L R Start Select
    12, 13, 14, 15,      // L2 R2 L3 R3
    16, 17, 18, 19,      // R.S. Up Down Left Right (beside the buttons)
};
#define LNG_SNES_MODERN_GAMEPAD_BIND_COLS 4
#define LNG_SNES_MODERN_GAMEPAD_BIND_SIDE 4
#define LNG_SNES_EXT_COUNT 8   /* modern-pad extras after the SNES twelve */

// ---- panel composition --------------------------------------------------------
static const char* const kPanelsSettingsSnes[]  = { "video", "audio", "hotkeys", NULL };

// ---- ROM file-picker filter -----------------------------------------------------
static const char* const kSnesRomPatterns[] = { "*.sfc", "*.smc", "*.fig", "*.swc" };
#define LNG_SNES_ROM_PATTERN_COUNT \
    ((int)(sizeof(kSnesRomPatterns) / sizeof(kSnesRomPatterns[0])))

// ---- SystemProfile row ----------------------------------------------------------
// One row, two pads: the classic twelve-button page and the modern-pad page
// share every other module.
#define LNG_SNES_SYSTEM_PROFILE(buttons, count, order, cols, side) { \
    /* id       */ "snes", \
    /* platform */ "SUPER NINTENDO", \
    /* theme    */ NULL,        /* default CRT-console theme */ \
    /* rom_noun */ "ROM", \
    /* controller */ { \
        buttons, count, \
        "pad.tga", NULL, NULL, \
        /* max_players */ 2, /* has_pad_mode */ 0, \
        /* binds_per_input */ 1, /* modes */ NULL, /* mode_count */ 0, \
        /* has_pad_binds */ 1, \
        /* pad_bind_order */ order, \
        /* pad_bind_cols  */ cols, \
        /* pad_bind_rows  */ LNG_SNES_GAMEPAD_BIND_ROWS, \
        /* has_pak        */ 0, \
        /* pad_bind_side  */ side, \
    }, \
    /* save */    { SAVE_SRAM, 1, NULL }, \
    /* video */   { \
        /*window_scale*/1, /*fullscreen*/1, /*linear_filter*/1, /*widescreen*/1, \
        /*renderer*/0, /*supersampling*/0, /*screen_kind*/0, /*frame_interp*/0, /*aspect*/0, \
        /*texture_filter*/0, /*antialiasing*/0, /*spu_hq*/0, /*skip_fmv*/0, /*turbo_loads*/0, \
        /*bios*/0, /*deadzone*/0, \
    }, \
    /* verify */  { 0, NULL }, \
    /* hotkeys_mask */ (uint32_t)(LNG_HOTKEYS_ALL | \
                                   /* Save-state slot browser: the SNES runner \
                                    * has had snes_savestate_menu.c for a while, \
                                    * but the bind was unreachable here because \
                                    * LNG_HOTKEYS_ALL stops at bit 10 and this \
                                    * one is bit 15 — ports hardcoded a key \
                                    * instead. Note the framework leaves it \
                                    * UNBOUND by default (F1..F10 are the ten \
                                    * LoadState slots on SNES), so this row \
                                    * shows "(unbound)" until a player picks a \
                                    * key or a port's config.ini names one. */ \
                                   (1u << LNG_HK_SAVE_STATE_MENU) | \
                                   /* Rewind: added now that snes_rewind.c \
                                    * exists behind it. It was deliberately \
                                    * withheld while there was nothing to \
                                    * bind, on the same principle as \
                                    * n64_profile.h's debug-tools note -- a \
                                    * control that does nothing is worse than \
                                    * an absent one. Unbound by default here \
                                    * too: F8 is LoadState slot 8. */ \
                                   (1u << LNG_HK_REWIND)), \
    /* panels_dashboard  */ kPanelsDashboardCommon, \
    /* panels_settings   */ kPanelsSettingsSnes, \
    /* panels_controller */ kPanelsControllerCommon, \
    /* screen_kind_names */ NULL,   /* legacy Raw/CRT/Composite/Trinitron set */ \
    /* screen_kind_count */ 0, \
    /* rom_filter        */ { kSnesRomPatterns, LNG_SNES_ROM_PATTERN_COUNT, \
                              "SNES ROM (.sfc .smc .fig .swc)" }, \
 \
}

static const SystemProfile kSystemProfileSnes = LNG_SNES_SYSTEM_PROFILE(
    kSnesPadButtons, LNG_SNES_PAD_BUTTON_COUNT, kSnesGamepadBindOrder,
    LNG_SNES_GAMEPAD_BIND_COLS, 0);
static const SystemProfile kSystemProfileSnesModern = LNG_SNES_SYSTEM_PROFILE(
    kSnesModernPadButtons, LNG_SNES_MODERN_PAD_BUTTON_COUNT,
    kSnesModernGamepadBindOrder, LNG_SNES_MODERN_GAMEPAD_BIND_COLS,
    LNG_SNES_MODERN_GAMEPAD_BIND_SIDE);
#undef LNG_SNES_SYSTEM_PROFILE

// ---- name aliases + ABI capability defaults -------------------------------------
static inline int launcher_console_is_snes(const char* name) {
    return lps_streq_ci(name, "snes") || lps_streq_ci(name, "sfc") ||
           lps_streq_ci(name, "supernintendo");
}

// The "snes" row of launcher_profile_apply(): system identity + capability
// defaults onto the C ABI GameInfo. Per-game fields (name, hashes, SRAM,
// per-title capability overrides) are set by the host AFTER this.
static inline void launcher_profile_apply_snes(RecompLauncherCGameInfo* gi) {
    gi->theme    = NULL;               // default CRT-console theme (violet + scanlines)
    gi->platform = "SUPER NINTENDO";
    gi->rom_noun = "ROM";
    gi->widescreen_supported = 1;      // 16:9 toggle (legacy aspect path)
    // (MSU-1 / SRAM are per-game; window scale + linear filter are the defaults.)
}

#ifdef __cplusplus
}
#endif

#endif // RUI_CONSOLE_SNES_PROFILE_H
