#pragma once
/* Axl from Mega Man X8 16-bit (Alysson da Paz; Zashiko/TagTeam mod), played
 * on X's native body. X's physics, collision, health and weapons stay native;
 * Axl adds his art, rapid fire with 8-way aim, the copy shot, hover, the
 * 8-way air dash, the Down+Dash dodge roll and his GigaCrash armor ability. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct MmxRenderAxlFrame;

/* Animation ids, as in tools/axl/make_axl_manifest.py. */
enum {
  MMX_AXL_ANIM_IDLE = 1, MMX_AXL_ANIM_WALK, MMX_AXL_ANIM_JUMP, MMX_AXL_ANIM_FALL,
  MMX_AXL_ANIM_DASH, MMX_AXL_ANIM_DODGE, MMX_AXL_ANIM_HOVER, MMX_AXL_ANIM_SLIDE,
  MMX_AXL_ANIM_WALLJUMP, MMX_AXL_ANIM_SHOT, MMX_AXL_ANIM_SHOT_AIR,
  MMX_AXL_ANIM_SHOT_SLIDE, MMX_AXL_ANIM_DAMAGE, MMX_AXL_ANIM_STAIRS,
  MMX_AXL_ANIM_BEAM_IN, MMX_AXL_ANIM_VICTORY, MMX_AXL_ANIM_CROUCH,
  MMX_AXL_ANIM_RECOVER, MMX_AXL_ANIM_WALK_START, MMX_AXL_ANIM_SHOT_RECOVER,
  MMX_AXL_ANIM_STAIRS_START, MMX_AXL_ANIM_STAIRS_END, MMX_AXL_ANIM_BEAM_OUT,
  MMX_AXL_ANIM_DIAGONAL_DASH,
  MMX_AXL_ANIM_GIGA_INTRO, MMX_AXL_ANIM_GIGA_FIRE, MMX_AXL_ANIM_GIGA_IDLE,
  MMX_AXL_ANIM_GIGA_END,
  MMX_AXL_ANIM_GIGA_AIR_INTRO, MMX_AXL_ANIM_GIGA_AIR_FIRE, MMX_AXL_ANIM_GIGA_AIR_IDLE,
  MMX_AXL_ANIM_GIGA_AIR_END,
  MMX_AXL_ANIM_COPY_SHOT, MMX_AXL_ANIM_BULLET, MMX_AXL_ANIM_GIGA_SCREEN,
  MMX_AXL_ANIM_BEAM,
  /* Turned copies of the dash and the pistol bullet. */
  MMX_AXL_ANIM_DASH_UP, MMX_AXL_ANIM_DASH_DOWN, MMX_AXL_ANIM_DASH_DOWN_TILT,
  MMX_AXL_ANIM_BULLET_UP, MMX_AXL_ANIM_BULLET_UP_TILT, MMX_AXL_ANIM_BULLET_DOWN_TILT,
  MMX_AXL_ANIM_BULLET_DOWN,
  /* The HUD: the health bar's A badge, the pause menu's life head and portrait. */
  MMX_AXL_ANIM_HUD_BADGE, MMX_AXL_ANIM_HUD_LIFE, MMX_AXL_ANIM_HUD_PAUSE,
};

/* Sound cues: GigaCrash's start (CrashIllusion) and its blast. */
enum { MMX_AXL_SOUND_GIGA_START, MMX_AXL_SOUND_GIGA_SHOT };

/* Characters, for the team. */
enum { MMX_AXL_CHAR_X = 0, MMX_AXL_CHAR_ZERO = 1, MMX_AXL_CHAR_AXL = 2,
       MMX_AXL_CHAR_ALONE = 254, /* partner None: CHANGE PLAYER does nothing */
       MMX_AXL_CHAR_NONE = 255 };

enum { MMX_AXL_SHOTS = 8, MMX_AXL_SPECIAL_MAX = 64 };

/* One of Axl's shots: the native buster slot and Axl's own straight line in
 * 16.8 fixed point. The native code still owns its collision and expiry. */
typedef struct MmxAxlShot {
  uint16_t slot;
  int16_t vx, vy;
  uint8_t kind;    /* 0 pistol bullet, 1 copy shot */
  uint8_t age;
  int32_t x, y;
} MmxAxlShot;

/* Savestate chunk v26. Seat 0 only. */
typedef struct MmxAxlState {
  uint8_t active;         /* Axl, not X, when X's body is live */
  uint8_t prev_input_lo, prev_input_hi, prev_ext;
  uint8_t roll, roll_facing, fire_timer, aim, shot_pose, copy_timer, copy_pending;
  uint8_t hover, hover_used, dash, dash_dir, dash_used;
  uint8_t giga, giga_air;  /* frames into the GigaCrash, 0 = none */
  uint16_t giga_slot, giga_hits;
  uint8_t giga_hp[16];     /* enemy health seen last frame of the attack */
  uint16_t special;        /* armor ability bar, 1/16 units */
  uint8_t regen, title_shot;
  uint8_t anim, anim_step, anim_tick, template_valid;
  uint8_t arms_lent;       /* Shot2 lent the arm parts' charge bit for a frame */
  uint8_t pose_aim;        /* the aim of the last shot, for its pose */
  uint8_t wall_y[2];       /* the height held while shooting on a wall, 0 = none */
  uint8_t wall_side;       /* the wall Axl clings to: PAD_LEFT/PAD_RIGHT >> 6, 0 = none */
  MmxAxlShot shots[MMX_AXL_SHOTS];
  uint8_t giga_template[64];
} MmxAxlState;

bool MmxAxlLoad(void);
void MmxAxlEnable(bool enabled);
void MmxAxlDisable(void);
bool MmxAxlEnabled(void);
/* Enabled, and Axl is the live character. */
bool MmxAxlActive(void);
MmxAxlState MmxAxlGetState(void);
bool MmxAxlValidState(const MmxAxlState *state);
void MmxAxlSetState(const MmxAxlState *state);
void MmxAxlResetState(void);

/* The solo team: who starts and who CHANGE PLAYER brings in. A partner of
 * MMX_AXL_CHAR_NONE cycles through every enabled character instead;
 * MMX_AXL_CHAR_ALONE keeps the starting character. The team applies also
 * when Add Axl itself is off. */
void MmxAxlSetTeam(unsigned start, unsigned partner);
unsigned MmxAxlTeamStart(void);

/* Host frame hooks: input before the game frame, shots after it. */
void MmxAxlFrameBegin(uint32_t *inputs);
void MmxAxlFrameEnd(void);
/* The giant GigaCrash box in free ROM padding at $86:FFF0. */
void MmxAxlCollisionRom(uint8_t *rom, size_t size);
/* Registers the player and combat hooks (mmx_zero.h's body hooks). */
void MmxAxlInstallHooks(bool on);

/* Plays a sound cue; the host skips speculative (rollback) frames. */
void MmxAxlSetSoundHook(void (*play)(unsigned cue));

/* Renderer provider. */
bool MmxAxlFrame(const uint8_t *ram, struct MmxRenderAxlFrame *out);
