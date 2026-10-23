/* Axl (Mega Man X8 16-bit) on X's native body. See mmx_axl.h.
 *
 * Everything runs at deterministic points, so replays, rewind and netplay
 * see one transform of the seat's input and RAM:
 *   MmxAxlFrameBegin  rewrites seat 0's buttons: CHANGE PLAYER, rapid fire,
 *                     aim stance, Shot2, roll, dash, hover, GigaCrash;
 *   pre_player/motion (mmx_zero.h body hooks at $81:815C and $82:823E)
 *                     drive hover, air dash and the GigaCrash stance through
 *                     X's native velocities, so terrain collision stays native;
 *   damage/hitbox     give the copy shot its damage and the GigaCrash its
 *                     screen-sized, once-per-enemy hit;
 *   MmxAxlFrameEnd    steers Axl's shots, keeps the GigaCrash alive and
 *                     advances his animation.
 * X1 RAM (player at $0BA8): $0BAA action, $0BAD/$0BB0 position, $0BB6
 * visible, $0BB9/$0C11 bit 6 facing right, $0BBE body group ($18), $0BC2/
 * $0C04 horizontal velocity/speed, $0BC4 vertical velocity (up positive),
 * $0BC6 gravity, $0BD3 bit 2 grounded, $0BDB weapon, $0BDD live buster shots,
 * $0BDF/$0BE1 shot held, $0BFF/$0C00/$0C02/$0C03 charge, $0C13
 * invulnerability; buster shots at $1228+64n, enemies at $0E68+64n. */
#include "mmx_axl.h"
#include "mmx_renderer.h"
#include "mmx_weapons.h"
#include "mmx_wide_policy.h"
#include "mmx_zero.h"
#include "saber/mmx_saber_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t g_ram[0x20000];
uint8_t RtlGetPadExt(int slot);
/* Built from assets/axl by cmake/axl.cmake. */
extern const uint8_t kMmxAxlEmbeddedSprites[];
extern const size_t kMmxAxlEmbeddedSpritesSize;

enum {
  PAD_B = 1 << 0, PAD_Y = 1 << 1, PAD_SELECT = 1 << 2, PAD_UP = 1 << 4,
  PAD_DOWN = 1 << 5, PAD_LEFT = 1 << 6, PAD_RIGHT = 1 << 7, PAD_A = 1 << 8,
  PAD_X = 1 << 9, PAD_DPAD = PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT,
  EXT_ARMOR = 1 << 0,
  /* PlayerAxl.tscn at 60 fps. Pistol: shoot_delay 0.1 s, 450 px/s. Copy
   * shot: 250 px/s, 2 damage. Dodge: 8 frames at 15 fps, moving and
   * invulnerable for the first six. Dash/AirDash: 210 px/s for 0.475 s,
   * 0.6x up, 1.2x and double speed down. Hover: 2 s, 90 px/s up/down. */
  FIRE_PERIOD = 6, SHOT_POSE = 15, COPY_PERIOD = 15,
  BULLET_SPEED = 0x780, BULLET_DIAGONAL = 0x500, COPY_SPEED = 0x430, COPY_DIAGONAL = 0x2f0,
  ROLL_FRAMES = 32, ROLL_MOVE = 24,
  ROLL_GUARD = 16,  /* the tumble (steps 0-3); the crouch and rise can be hit */
  DASH_SPEED = 0x380, DASH_DIAGONAL = 0x278, DASH_FRAMES = 28, DASH_UP_FRAMES = 17,
  DASH_DOWN_FRAMES = 17,
  HOVER_FRAMES = 120, HOVER_CLIMB = 0x180, HOVER_DRIFT = 0x100, HOVER_SINK = 0x100,
  /* GigaCrashStart.gd: 0.75 s intro, then GigaCrashExplosion for 2 s. */
  GIGA_INTRO = 45, GIGA_FIRE = 165, GIGA_END = 180,
  GIGA_DAMAGE = 32, GIGA_BOSS_DAMAGE = 8, COPY_DAMAGE = 2,
  SPECIAL_UNIT = 16, SPECIAL_REGEN = 60,
  GIGA_BOX = 0xfff0, GIGA_BOX_ROM = 0x37ff0, /* only during a GigaCrash */
};

static bool enabled;
static MmxSaberAssets *assets;
static MmxAxlState state;
static uint8_t live_slots;   /* active buster slots after the last frame */
static bool cap_lifted;
static unsigned team_start = MMX_AXL_CHAR_X, team_partner = MMX_AXL_CHAR_NONE;
static void (*sound_hook)(unsigned cue);
void MmxAxlSetSoundHook(void (*play)(unsigned cue)) { sound_hook = play; }

static unsigned word(const uint8_t *r, unsigned a) { return r[a] | r[a + 1] << 8; }
static void putword(uint8_t *r, unsigned a, unsigned v) { r[a] = (uint8_t)v; r[a + 1] = (uint8_t)(v >> 8); }

bool MmxAxlLoad(void) {
  if (assets) return true;
  char reason[256] = {0};
  assets = MmxSaberAssetsParse(kMmxAxlEmbeddedSprites, kMmxAxlEmbeddedSpritesSize,
                               NULL, reason, sizeof(reason));
  if (!assets) fprintf(stderr, "[mmx-axl] cannot load Axl sprites: %s\n", reason);
  return assets != NULL;
}
void MmxAxlSetTeam(unsigned start, unsigned partner) {
  team_start = start <= MMX_AXL_CHAR_AXL ? start : MMX_AXL_CHAR_X;
  team_partner = partner == MMX_AXL_CHAR_ALONE ||
      (partner <= MMX_AXL_CHAR_AXL && partner != team_start) ? partner : MMX_AXL_CHAR_NONE;
}
unsigned MmxAxlTeamStart(void) { return team_start; }
void MmxAxlResetState(void) {
  memset(&state, 0, sizeof(state));
  /* X's body is Axl when Axl starts, or when the team is Zero and Axl. */
  state.active = team_start == MMX_AXL_CHAR_AXL ||
      (team_start == MMX_AXL_CHAR_ZERO && team_partner == MMX_AXL_CHAR_AXL);
  state.aim = state.pose_aim = 2;
  state.special = MMX_AXL_SPECIAL_MAX * SPECIAL_UNIT;
  live_slots = 0; cap_lifted = false;
}
void MmxAxlEnable(bool on) {
  enabled = on && MmxAxlLoad();
  MmxAxlResetState();
}
void MmxAxlDisable(void) {
  enabled = false;
  team_start = MMX_AXL_CHAR_X; team_partner = MMX_AXL_CHAR_NONE;
  MmxAxlResetState();
}
bool MmxAxlEnabled(void) { return enabled; }
static bool zero_live(void) { return MmxZeroEnabled() && MmxZeroActive(); }
bool MmxAxlActive(void) { return enabled && state.active && !zero_live(); }
MmxAxlState MmxAxlGetState(void) { return state; }
bool MmxAxlValidState(const MmxAxlState *s) {
  if (!s || s->active > 1 || s->arms_lent > 1 || s->wall_side > 3 || s->aim > 4 || s->pose_aim > 4 || s->roll > ROLL_FRAMES || s->dash_dir > 7 ||
      s->giga > GIGA_END || s->special > MMX_AXL_SPECIAL_MAX * SPECIAL_UNIT) return false;
  for (unsigned i = 0; i < MMX_AXL_SHOTS; ++i)
    if (s->shots[i].slot && (s->shots[i].slot < 0x1228 || s->shots[i].slot >= 0x1428 ||
        (s->shots[i].slot - 0x1228) % 64 || s->shots[i].kind > 1)) return false;
  return true;
}
void MmxAxlSetState(const MmxAxlState *s) {
  if (!MmxAxlValidState(s)) return;
  state = *s;
  live_slots = 0;
  for (unsigned i = 0; i < 8; ++i) if (g_ram[0x1228 + i * 64]) live_slots |= 1u << i;
  cap_lifted = true; /* recount once */
}

static bool grounded(const uint8_t *r) { return (r[0xbd3] & 4) != 0; }
/* Untouchable: the hurt latches ($0BD8, $0C32) X1 sets on a hit, held by
 * its invulnerability countdown $0C13, which clears them when it runs out. */
static void guard(void) {
  if (!g_ram[0xbd8]) g_ram[0xbd8] = 8;
  if (!g_ram[0xc32]) g_ram[0xc32] = 8;
  if (g_ram[0xc13] < 4) g_ram[0xc13] = 4;
}
static bool on_ground(unsigned action) {
  return action <= 0x04 || action == 0x0a || action == 0x14 || action == 0x20;
}
static bool can_fire(unsigned action) {
  return action <= 0x0a || action == 0x10 || action == 0x12 || action == 0x20;
}
static bool airborne(const uint8_t *r) {
  return !grounded(r) && (r[0xbaa] == 0x06 || r[0xbaa] == 0x08);
}
static bool facing_right(void) { return (g_ram[0xbb9] & 0x40) != 0; }
static void face(bool right) {
  if (g_ram[0xbaa] == 0x12) return; /* X1 turns him away from a wall to shoot */
  g_ram[0xbb9] = (uint8_t)((g_ram[0xbb9] & ~0x40) | (right ? 0x40 : 0));
  g_ram[0xc11] = (uint8_t)((g_ram[0xc11] & ~0x40) | (right ? 0x40 : 0));
}
/* On a wall ($12) Axl aims up, down, away or diagonally away, never into
 * it: the side he clings to (or, before that is known, either side) counts
 * as no direction. */
static bool on_wall(unsigned action) { return action == 0x12; }
static unsigned aim_from(unsigned pad) {
  if (on_wall(g_ram[0xbaa]))
    pad &= state.wall_side ? ~((unsigned)state.wall_side << 6) : ~(unsigned)(PAD_LEFT | PAD_RIGHT);
  bool up = pad & PAD_UP, down = pad & PAD_DOWN, side = pad & (PAD_LEFT | PAD_RIGHT);
  if (up) return side ? 1 : 0;
  if (down) return side ? 3 : 4;
  return 2;
}
/* X's body uses sprite group $00, or $18 once he has armor; the scripted
 * groups are Vile's capture ($66) and the Ride Armor pilot ($6A/$6B). */
static bool body_group(const uint8_t *r) {
  return r[0xbbe] != 0x66 && r[0xbbe] != 0x6a && r[0xbbe] != 0x6b;
}
static bool playable(const uint8_t *r) {
  return MmxWidePolicy_IsStageScene(r) && !MmxWeaponsMenuVisible(r) &&
      body_group(r) && (r[0xbcf] & 127);
}
/* Axl is drawn also while he dies (action $0C, no health), when his input
 * is no longer his: the Boss Rush result screen reads it. */
static bool shown(const uint8_t *r) {
  return playable(r) || (MmxWidePolicy_IsStageScene(r) && !MmxWeaponsMenuVisible(r) &&
                         body_group(r) && r[0xbaa] == 0x0c);
}
static bool title_cursor(const uint8_t *r) {
  unsigned y = word(r, 0xbb0);
  return r[0xd1] == 0 && r[0xba9] == 2 && r[0xbbe] == 0 && word(r, 0xbad) == 32 &&
      y >= 150 && y <= 214;
}

/* ---- the team ---------------------------------------------------------- */
static unsigned current_character(void) {
  if (zero_live()) return MMX_AXL_CHAR_ZERO;
  return state.active ? MMX_AXL_CHAR_AXL : MMX_AXL_CHAR_X;
}
static unsigned next_character(unsigned now) {
  if (team_partner == MMX_AXL_CHAR_ALONE) return now;
  if (team_partner != MMX_AXL_CHAR_NONE)
    return now == team_start ? team_partner : team_start;
  /* No team: X, Zero (when added), Axl, in turn. */
  for (unsigned step = 1; step < 3; ++step) {
    unsigned c = (now + step) % 3;
    if (c == MMX_AXL_CHAR_ZERO && !MmxZeroEnabled()) continue;
    return c;
  }
  return now;
}

/* ---- input ------------------------------------------------------------- */
static void start_dash(unsigned in, bool from_ground) {
  bool up = in & PAD_UP, down = in & PAD_DOWN;
  bool left = in & PAD_LEFT, right = in & PAD_RIGHT;
  if (!left && !right && !up && !down) { right = facing_right(); left = !right; }
  unsigned dir;                     /* 0 right, counter-clockwise to 7 */
  if (up) dir = right ? 1 : left ? 3 : 2;
  else if (down) dir = right ? 7 : left ? 5 : 6;
  else dir = right ? 0 : 4;
  if (from_ground && dir >= 5) return;
  if (left || right) face(right);
  state.dash = 1; state.dash_dir = (uint8_t)dir; state.dash_used = 1;
  state.hover = 0; state.shot_pose = 0;
}
static unsigned dash_frames(void) {
  unsigned d = state.dash_dir;
  return d == 0 || d == 4 ? DASH_FRAMES : d <= 3 ? DASH_UP_FRAMES : DASH_DOWN_FRAMES;
}

/* Whether CHANGE PLAYER may reach Zero's own exchange (Select through to
 * the game): only for a step of the team that starts or ends with Zero. */
static bool zero_exchange_allowed(void) {
  if (team_partner == MMX_AXL_CHAR_NONE) return true; /* no solo team (co-op) */
  if (!MmxZeroEnabled() || team_partner == MMX_AXL_CHAR_ALONE) return false;
  unsigned now = current_character(), next = next_character(now);
  return now != next && (now == MMX_AXL_CHAR_ZERO || next == MMX_AXL_CHAR_ZERO);
}
void MmxAxlFrameBegin(uint32_t *inputs) {
  /* The team owns CHANGE PLAYER on every frame, before anything below can
   * return early: Select never starts an exchange the team does not have. */
  uint32_t raw = *inputs;
  if (!zero_exchange_allowed()) *inputs &= ~(uint32_t)PAD_SELECT;
  if (!enabled) return;
  unsigned in = raw & 0x0fff, prev = state.prev_input_lo | state.prev_input_hi << 8;
  unsigned pressed = in & ~prev;
  uint8_t ext = RtlGetPadExt(0), ext_pressed = ext & ~state.prev_ext;
  state.prev_input_lo = (uint8_t)in; state.prev_input_hi = (uint8_t)(in >> 8);
  state.prev_ext = ext;
  if (!playable(g_ram) || MmxZeroSwapping()) {
    state.roll = state.dash = state.hover = 0; state.fire_timer = 0;
    return;
  }
  /* `in` keeps the player's Select for the X/Axl switch below; `out` starts
   * from the team-filtered input. */
  unsigned action = g_ram[0xbaa], out = *inputs & 0x0fff;
  bool busy = state.roll || state.giga;
  /* CHANGE PLAYER (Select): Zero's own exchange (with its teleport and
   * separate health) whenever Zero is involved; X and Axl share the body,
   * its health and weapons, and switch at once. */
  unsigned now = current_character();
  if ((pressed & PAD_SELECT) && !busy) {
    unsigned next = next_character(now);
    if (next == now) out &= ~PAD_SELECT;
    else if (now == MMX_AXL_CHAR_ZERO || next == MMX_AXL_CHAR_ZERO) {
      if (now == MMX_AXL_CHAR_ZERO) state.active = next == MMX_AXL_CHAR_AXL;
    } else {
      out &= ~PAD_SELECT;
      state.active = next == MMX_AXL_CHAR_AXL;
      state.shot_pose = state.fire_timer = 0;
    }
  } else if (now == MMX_AXL_CHAR_AXL || !MmxZeroEnabled() || team_partner == MMX_AXL_CHAR_ALONE)
    out &= ~PAD_SELECT;
  if (!MmxAxlActive()) { *inputs = (*inputs & ~0x0fffu) | out; return; }

  if (state.special < MMX_AXL_SPECIAL_MAX * SPECIAL_UNIT && !state.giga &&
      ++state.regen >= SPECIAL_REGEN) {
    state.regen = 0;
    state.special += SPECIAL_UNIT;
  }
  bool ground = grounded(g_ram);
  if (ground || action == 0x10 || action == 0x12) { state.hover_used = 0; state.dash_used = 0; }

  /* GigaCrash: ARMOR ABILITY with a full bar. Axl plants himself and is
   * untouchable for the whole attack. */
  if (!state.giga && !state.roll && (ext_pressed & EXT_ARMOR) &&
      state.special >= MMX_AXL_SPECIAL_MAX * SPECIAL_UNIT &&
      (can_fire(action) || action == 0x14)) {
    state.giga = 1; state.giga_air = !ground; state.giga_hits = 0; state.giga_slot = 0;
    for (unsigned i = 0; i < 15; ++i)
      state.giga_hp[i] = g_ram[0xe68 + i * 64] ? g_ram[0xe68 + i * 64 + 0x27] & 127 : 0;
    state.special = 0; state.regen = 0;
    guard();
    if (sound_hook) sound_hook(MMX_AXL_SOUND_GIGA_START);
    state.dash = state.hover = 0; state.shot_pose = 0;
  }
  if (state.giga) { *inputs = (*inputs & ~0x0fffu); return; }

  /* Dodge roll: Down + Dash on the ground, or Down while dashing. Native
   * dash input drives the motion so terrain collision stays native. */
  bool down = in & PAD_DOWN;
  if (!state.roll && on_ground(action) && ground &&
      ((down && (pressed & PAD_A)) || (action == 0x14 && (pressed & PAD_DOWN)))) {
    state.roll = 1;
    guard();
    state.roll_facing = g_ram[0xbb9] & 0x40;
    state.shot_pose = 0; state.dash = 0;
  }
  if (state.roll) {
    if (!on_ground(action) && state.roll > 2) state.roll = 0;
    else {
      out = 0;
      if (state.roll <= ROLL_MOVE)
        out = PAD_A | (state.roll_facing ? PAD_RIGHT : PAD_LEFT);
      if (++state.roll > ROLL_FRAMES) state.roll = 0;
      *inputs = (*inputs & ~0x0fffu) | out;
      return;
    }
  }

  /* Dash in 8 directions: once per airtime, or upward from the ground.
   * A level ground dash stays X's native dash. */
  bool takeoff = false;
  if ((pressed & PAD_A) && !state.dash) {
    if (!ground && !state.dash_used && (airborne(g_ram) || action == 0x10)) start_dash(in, false);
    else if (ground && (in & PAD_UP) && on_ground(action)) {
      start_dash(in, true);
      takeoff = state.dash != 0;
    }
  }
  /* X1's air control moves at $0C04 in the held direction; the dash holds
   * its own direction and pre_player sets the speed. */
  if (state.dash) {
    unsigned d = state.dash_dir;
    out &= ~(PAD_A | PAD_DPAD | PAD_B);
    if (d == 0 || d == 1 || d == 7) out |= PAD_RIGHT;
    else if (d >= 3 && d <= 5) out |= PAD_LEFT;
    /* From the ground, X1's own jump lifts him off (pre_player leaves that
     * first frame alone); the dash takes over in the air. */
    if (takeoff) out |= PAD_B;
  }

  /* Hover: jump again in the air and hold it. */
  if (!state.dash && !ground && (pressed & PAD_B) && !state.hover_used && airborne(g_ram)) {
    state.hover = 1; state.hover_used = 1;
  }
  if (state.hover && (!(in & PAD_B) || ground || !airborne(g_ram))) state.hover = 0;

  /* On a wall, SHOT or SHOT2 held keeps Axl on it: the side he clings to
   * stays held for X1, and the d-pad cannot pull him away. */
  if (on_wall(action)) {
    unsigned side = in & (PAD_LEFT | PAD_RIGHT);
    if (side && side != (PAD_LEFT | PAD_RIGHT) && !state.wall_side)
      state.wall_side = (uint8_t)(side >> 6);
    if (state.wall_side && (in & (PAD_Y | PAD_X)))
      out = (out & ~(unsigned)(PAD_LEFT | PAD_RIGHT)) | (unsigned)state.wall_side << 6;
  } else if (action != 0x10) state.wall_side = 0;

  /* Shot: rapid fire for as long as it is held, never a charge. Each shot is
   * a one-frame native press. On the ground Axl plants his feet and the
   * d-pad only aims and turns him. */
  out &= ~(PAD_Y | PAD_X);
  bool shooting = (in & PAD_Y) && can_fire(action) && !state.dash;
  if (shooting) {
    state.aim = (uint8_t)aim_from(in);
    if (in & PAD_LEFT) face(false);
    else if (in & PAD_RIGHT) face(true);
    if (on_ground(action) && action != 0x14) out &= ~PAD_DPAD;
    if (state.hover) out &= ~PAD_DPAD;
    if (!state.fire_timer) {
      out |= PAD_Y;
      state.fire_timer = FIRE_PERIOD;
      state.shot_pose = SHOT_POSE; state.pose_aim = state.aim;
      /* X1 allows three buster shots; Axl's pistol outpaces that. Special
       * weapons keep their own native limits. */
      if (!g_ram[0xbdb] && g_ram[0xbdd] >= 3) { g_ram[0xbdd] = 2; cap_lifted = true; }
    }
  } else if (!(in & PAD_Y)) state.aim = 2;
  if (state.fire_timer) --state.fire_timer;
  if (state.copy_timer) --state.copy_timer;

  /* Shot2: the copy shot with no weapon equipped; otherwise the equipped
   * weapon's full charge at once, which the native release fires and pays
   * for. */
  if ((pressed & PAD_X) && can_fire(action) && !state.dash && !(out & PAD_Y)) {
    state.aim = (uint8_t)aim_from(in);
    if (in & PAD_LEFT) face(false);
    else if (in & PAD_RIGHT) face(true);
    if (!g_ram[0xbdb]) {
      if (!state.copy_timer) {
        out |= PAD_Y;
        state.copy_pending = 1; state.copy_timer = COPY_PERIOD;
        state.shot_pose = SHOT_POSE; state.pose_aim = state.aim;
        if (!g_ram[0xbdb] && g_ram[0xbdd] >= 3) { g_ram[0xbdd] = 2; cap_lifted = true; }
      }
    } else {
      /* A full native charge: $0BFF counts down to 0 while shot is held, and
       * the release reads the charge stage (+$10 = 6) from the charge glow
       * object, class 1 in $0C98..$0E17, as a full hold leaves it. */
      unsigned glow = 0;
      for (unsigned d = 0xc98; d < 0xe18 && !glow; d += 32)
        if (g_ram[d] && g_ram[d + 10] == 1) glow = d;
      for (unsigned d = 0xc98; d < 0xe18 && !glow; d += 32) if (!g_ram[d]) glow = d;
      if (glow) {
        static const uint8_t full[32] = {
          0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x81, 0x00,
          0x06, 0x66, 0x02, 0x01, 0x31, 0xd2, 0x71, 0x8c};
        memcpy(g_ram + glow, full, sizeof(full));
        memcpy(g_ram + glow + 5, g_ram + 0xbad, 2);
        memcpy(g_ram + glow + 8, g_ram + 0xbb0, 2);
      }
      g_ram[0xbff] = 0; g_ram[0xc00] = 4; g_ram[0xc02] = 2; g_ram[0xc03] = 1;
      /* X1 charges specials only with the arm parts ($1F99 bit 1). SHOT2
       * always fires the full charge, so lend the bit for this release. */
      if (!(g_ram[0x1f99] & 2)) { g_ram[0x1f99] |= 2; state.arms_lent = 1; }
      g_ram[0xbdf] |= 0x40; g_ram[0xbe1] |= 0x40;
      state.shot_pose = SHOT_POSE; state.pose_aim = state.aim;
    }
  }
  *inputs = (*inputs & ~0x0fffu) | out;
}

/* ---- physics, through X's native velocities ------------------------------ */
static int dash_vx(void) {
  static const int8_t sx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
  unsigned d = state.dash_dir;
  bool diagonal = d & 1;
  return sx[d] * (diagonal ? DASH_DIAGONAL : DASH_SPEED);
}
static int dash_vy(void) {
  static const int8_t sy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  unsigned d = state.dash_dir;
  int speed = (d & 1) ? DASH_DIAGONAL : DASH_SPEED;
  if (d >= 5) speed *= 2;
  return sy[d] * speed;
}
static void pre_player(uint8_t *r) {
  if (!MmxAxlActive() || !playable(r)) return;
  if (state.giga) {
    if (!grounded(r)) { putword(r, 0xbc4, 0); r[0xbc6] = 0; }
    return;
  }
  if (state.dash) {
    if (state.dash == 1 && grounded(r)) return; /* the native jump takes off */
    if (state.dash_dir >= 5 && grounded(r)) { state.dash = 0; return; } /* a down dash lands */
    if (r[0xbaa] != 0x06 && r[0xbaa] != 0x08) {
      if (r[0xbaa] == 0x12 || r[0xbaa] == 0x0e) { state.dash = 0; return; }
      r[0xbaa] = 0x06; r[0xbab] = 0; r[0xbfd] = 0;
    }
    putword(r, 0xbc4, (unsigned)dash_vy()); r[0xbc6] = 0;
    putword(r, 0xc04, (unsigned)abs(dash_vx()));
    return;
  }
  /* Shooting on a wall holds Axl where he is instead of sliding. */
  unsigned held = state.prev_input_lo | state.prev_input_hi << 8;
  if (on_wall(r[0xbaa]) && (state.shot_pose || (held & (PAD_Y | PAD_X)))) {
    if (!state.wall_y[0] && !state.wall_y[1]) {
      unsigned y = word(r, 0xbb0);
      state.wall_y[0] = (uint8_t)y; state.wall_y[1] = (uint8_t)(y >> 8);
    }
    putword(r, 0xbc4, 0); r[0xbc6] = 0;
  } else state.wall_y[0] = state.wall_y[1] = 0;
  if (state.hover) {
    unsigned in = state.prev_input_lo | state.prev_input_hi << 8;
    int vy = 0;
    if (state.hover > HOVER_FRAMES) vy = -HOVER_SINK;
    else if (!state.shot_pose && (in & PAD_UP)) vy = HOVER_CLIMB;
    else if (!state.shot_pose && (in & PAD_DOWN)) vy = -HOVER_CLIMB;
    putword(r, 0xbc4, (unsigned)vy); r[0xbc6] = 0;
    putword(r, 0xc04, HOVER_DRIFT);
  }
}
static void motion(uint8_t *r, unsigned object) {
  if (object != 0xba8 || !MmxAxlActive() || !playable(r)) return;
  if (!state.giga) return;
  putword(r, 0xbc2, 0);
  putword(r, 0xc04, 0);
}

/* ---- combat ------------------------------------------------------------ */
static int enemy_bit(unsigned enemy) {
  return enemy >= 0xe68 && enemy < 0x1228 && (enemy & 63) == 0x28 ? (int)((enemy - 0xe68) / 64) : -1;
}
static const MmxAxlShot *shot_for(unsigned slot) {
  for (unsigned i = 0; i < MMX_AXL_SHOTS; ++i)
    if (state.shots[i].slot == slot) return &state.shots[i];
  return NULL;
}
static bool giga_live(unsigned projectile) {
  return enabled && state.giga > GIGA_INTRO && state.giga <= GIGA_FIRE &&
      state.giga_slot && projectile == state.giga_slot;
}
static unsigned hook_damage(uint8_t *r, unsigned enemy, unsigned projectile, unsigned value) {
  if (!enabled || !value || (value & 128)) return value;
  if (giga_live(projectile)) {
    int bit = enemy_bit(enemy);
    if (bit < 0) return value;
    if (state.giga_hits & (1u << bit)) return 0;
    state.giga_hits |= (uint16_t)(1u << bit);
    return MmxWidePolicy_IsBossEncounter(r[enemy + 10]) ? GIGA_BOSS_DAMAGE : GIGA_DAMAGE;
  }
  const MmxAxlShot *s = shot_for(projectile);
  if (s && s->kind == 1) return COPY_DAMAGE;
  return value;
}
static unsigned hook_hitbox(const uint8_t *r, unsigned enemy, unsigned projectile, unsigned value) {
  (void)r;
  if (!giga_live(projectile)) return value;
  int bit = enemy_bit(enemy);
  if (bit < 0) return value;
  return (state.giga_hits & (1u << bit)) ? 0 : GIGA_BOX;
}
static unsigned hook_upgrade(unsigned pc, unsigned original) {
  if (!MmxAxlActive()) return original;
  switch (pc & 0x7fffff) {
    case 0x01971c: case 0x019793: case 0x0198fc: return original | 8;
    default: return original;
  }
}
static const MmxZeroBodyHooks kHooks = {
  .upgrade = hook_upgrade, .pre_player = pre_player, .motion = motion,
  .damage = hook_damage, .hitbox = hook_hitbox,
};
void MmxAxlInstallHooks(bool on) { MmxZeroSetBodyHooks(on ? &kHooks : NULL); }

void MmxAxlCollisionRom(uint8_t *rom, size_t size) {
  /* {x, y, half width, half height}: the whole 4:3 screen around its centre.
   * The bytes lie inside Saber Zero's ground window ($86:FFD8..FFFF), which
   * he installs only into empty padding while he is live: hold the box only
   * through a GigaCrash, when Zero cannot be. */
  static const uint8_t box[4] = {0, 0, 127, 112}, empty[4] = {255, 255, 255, 255};
  if (!rom || size < GIGA_BOX_ROM + 4) return;
  if (memcmp(rom + GIGA_BOX_ROM, empty, 4) && memcmp(rom + GIGA_BOX_ROM, box, 4)) return;
  memcpy(rom + GIGA_BOX_ROM, enabled && state.giga ? box : empty, 4);
}

/* ---- after the frame --------------------------------------------------- */
static void default_template(uint8_t *t) {
  /* A buster lemon as $81:9D47 builds it, for a GigaCrash before any shot. */
  memset(t, 0, 64);
  t[0] = 1; t[1] = 2; t[0x0e] = 0x81; t[0x11] = 0x64; t[0x13] = 2;
  t[0x14] = 0x55; t[0x15] = 0xa8; t[0x16] = 0x08; t[0x1f] = 0x40;
}
static void place(unsigned d, int32_t x, int32_t y) {
  g_ram[d + 4] = (uint8_t)x; g_ram[d + 5] = (uint8_t)(x >> 8); g_ram[d + 6] = (uint8_t)(x >> 16);
  g_ram[d + 7] = (uint8_t)y; g_ram[d + 8] = (uint8_t)(y >> 8); g_ram[d + 9] = (uint8_t)(y >> 16);
}
static void track_shot(unsigned d, unsigned kind, unsigned in) {
  unsigned i = 0;
  while (i < MMX_AXL_SHOTS && state.shots[i].slot) ++i;
  if (i == MMX_AXL_SHOTS) return;
  bool copy = kind == 1;
  int straight = copy ? COPY_SPEED : BULLET_SPEED, diagonal = copy ? COPY_DIAGONAL : BULLET_DIAGONAL;
  const int speed[5][2] = {{0, -straight}, {diagonal, -diagonal}, {straight, 0},
                           {diagonal, diagonal}, {0, straight}};
  /* Muzzles from X8's ShotDirection.gd, relative to the body origin. */
  static const int8_t muzzle[5][2] = {{2, -34}, {24, -28}, {34, -8}, {29, 13}, {10, 26}};
  /* Along his facing; on a wall, away from it. */
  int dir = facing_right() ? 1 : -1;
  unsigned wall = state.wall_side ? (unsigned)state.wall_side << 6 : in & (PAD_LEFT | PAD_RIGHT);
  if (on_wall(g_ram[0xbaa]) && wall) dir = (wall & PAD_LEFT) ? 1 : -1;
  unsigned aim = state.pose_aim <= 4 ? state.pose_aim : 2;
  MmxAxlShot *s = &state.shots[i];
  memset(s, 0, sizeof(*s));
  s->slot = (uint16_t)d; s->kind = (uint8_t)kind;
  s->vx = (int16_t)(speed[aim][0] * dir);
  s->vy = (int16_t)speed[aim][1];
  s->x = ((int32_t)word(g_ram, 0xbad) + muzzle[aim][0] * dir) << 8;
  s->y = ((int32_t)word(g_ram, 0xbb0) + 1 + muzzle[aim][1]) << 8;
  place(d, s->x, s->y);
}
/* Bosses take a hit through their own class tables ($84:9E1A), not the
 * generic subtraction the damage hook sees. Count the first drop in a
 * boss's health during the attack as its GigaCrash hit and make up the rest
 * of the damage, leaving the finishing blow to the boss's own hit code. */
static void giga_boss_hits(void) {
  for (unsigned i = 0; i < 15; ++i) {
    unsigned d = 0xe68 + i * 64, hp = g_ram[d + 0x27] & 127;
    if (!g_ram[d]) { state.giga_hp[i] = 0; continue; }
    if (!(state.giga_hits & (1u << i)) && state.giga_hp[i] && hp < state.giga_hp[i]) {
      state.giga_hits |= (uint16_t)(1u << i);
      unsigned dealt = state.giga_hp[i] - hp, want = MmxWidePolicy_IsBossEncounter(g_ram[d + 10]) ?
          GIGA_BOSS_DAMAGE : GIGA_DAMAGE;
      if (hp > 1 && want > dealt) {
        unsigned left = hp > want - dealt ? hp - (want - dealt) : 1;
        g_ram[d + 0x27] = (uint8_t)((g_ram[d + 0x27] & 128) | left);
        hp = left;
      }
    }
    state.giga_hp[i] = (uint8_t)hp;
  }
}
static void giga_tick(void) {
  ++state.giga;
  if (sound_hook && state.giga == GIGA_INTRO + 1) sound_hook(MMX_AXL_SOUND_GIGA_SHOT);
  if (state.giga > GIGA_END) {
    state.giga = 0;
    if (state.giga_slot && g_ram[state.giga_slot] == 1) memset(g_ram + state.giga_slot, 0, 64);
    state.giga_slot = 0;
    cap_lifted = true;
    return;
  }
  if (state.giga <= GIGA_INTRO || state.giga > GIGA_FIRE) {
    if (state.giga > GIGA_FIRE && state.giga_slot) {
      memset(g_ram + state.giga_slot, 0, 64); state.giga_slot = 0; cap_lifted = true;
    }
    return;
  }
  /* GigaCrashExplosion: one projectile at the screen centre whose box covers
   * the 4:3 screen. A projectile spent on a hit returns the next frame. */
  unsigned d = state.giga_slot;
  if (!d || !g_ram[d]) {
    d = 0;
    for (unsigned i = 0; i < 8 && !d; ++i) if (!g_ram[0x1228 + i * 64]) d = 0x1228 + i * 64;
    if (!d) return;
    if (!state.template_valid) default_template(state.giga_template);
    memcpy(g_ram + d, state.giga_template, 64);
    g_ram[d + 1] = 2;
    state.giga_slot = (uint16_t)d;
    cap_lifted = true;
  }
  int32_t x = ((int32_t)word(g_ram, 0x1e4d) + 128) << 8, y = ((int32_t)word(g_ram, 0x1e50) + 112) << 8;
  place(d, x, y);
  giga_boss_hits();
  putword(g_ram, d + 0x1a, 0);
  putword(g_ram, d + 0x20, GIGA_BOX);
}
static unsigned pick_animation(unsigned action) {
  if (state.giga) {
    unsigned base = state.giga_air ? MMX_AXL_ANIM_GIGA_AIR_INTRO : MMX_AXL_ANIM_GIGA_INTRO;
    return base + (state.giga <= GIGA_INTRO ? 0 : state.giga <= GIGA_INTRO + 60 ? 1 :
                   state.giga <= GIGA_FIRE ? 2 : 3);
  }
  if (state.roll) return MMX_AXL_ANIM_DODGE;
  if (state.dash) {
    unsigned d = state.dash_dir;
    return d == 0 || d == 4 ? MMX_AXL_ANIM_DASH : d == 1 || d == 3 ? MMX_AXL_ANIM_DIAGONAL_DASH :
        d == 2 ? MMX_AXL_ANIM_DASH_UP : d == 6 ? MMX_AXL_ANIM_DASH_DOWN : MMX_AXL_ANIM_DASH_DOWN_TILT;
  }
  if (state.shot_pose) {
    if (action == 0x12) return MMX_AXL_ANIM_SHOT_SLIDE;
    return on_ground(action) && grounded(g_ram) ? MMX_AXL_ANIM_SHOT : MMX_AXL_ANIM_SHOT_AIR;
  }
  if (state.hover) return MMX_AXL_ANIM_HOVER;
  switch (action) {
    case 0x02: return MMX_AXL_ANIM_WALK_START;
    case 0x04: return MMX_AXL_ANIM_WALK;
    case 0x06: return MMX_AXL_ANIM_JUMP;
    case 0x08: return MMX_AXL_ANIM_FALL;
    case 0x0a: return MMX_AXL_ANIM_RECOVER;
    case 0x0c: case 0x0e: return MMX_AXL_ANIM_DAMAGE; /* X8 dies in its hurt pose */
    case 0x10: return MMX_AXL_ANIM_WALLJUMP;
    case 0x12: return MMX_AXL_ANIM_SLIDE;
    case 0x14: return MMX_AXL_ANIM_DASH;
    case 0x18: case 0x1a: case 0x1c: return MMX_AXL_ANIM_WALK; /* through a boss door */
    case 0x24: case 0x2a: return MMX_AXL_ANIM_STAIRS;
    case 0x26: return MMX_AXL_ANIM_STAIRS_END;
    case 0x28: return MMX_AXL_ANIM_STAIRS_START;
    default: return MMX_AXL_ANIM_IDLE;
  }
}
/* X1's beam: the travelling column is pose $3C; the beam-in ($22) lands as
 * $3D, $42, $41, $40, $3F, $3E, and teleporting out (CHANGE PLAYER too)
 * runs those morphs the other way. Axl lands in X8's intro and leaves in
 * its finish. False for any other pose. */
static bool beam_frame(unsigned action, unsigned pose, unsigned *anim, unsigned *step) {
  pose &= 127;
  if (pose < 0x3c || pose > 0x42) return false;
  if (pose == 0x3c) { *anim = MMX_AXL_ANIM_BEAM; *step = 0; return true; }
  if (action == 0x22) {
    static const uint8_t in[7] = {0, 2, 9, 8, 6, 5, 4}; /* $3C..$42 */
    *anim = MMX_AXL_ANIM_BEAM_IN; *step = in[pose - 0x3c];
  } else {
    *anim = MMX_AXL_ANIM_BEAM_OUT; *step = pose == 0x3d ? 4 : pose - 0x3e;
  }
  return true;
}
static bool looping(unsigned anim) {
  return anim == MMX_AXL_ANIM_IDLE || anim == MMX_AXL_ANIM_WALK ||
      anim == MMX_AXL_ANIM_HOVER || anim == MMX_AXL_ANIM_STAIRS ||
      anim == MMX_AXL_ANIM_GIGA_FIRE || anim == MMX_AXL_ANIM_GIGA_AIR_FIRE;
}
static void advance_animation(unsigned anim) {
  if (anim != state.anim) { state.anim = (uint8_t)anim; state.anim_step = 0; state.anim_tick = 0; }
  const MmxSaberAnimation *a = assets ? MmxSaberAssetsAnimationById(assets, (uint16_t)anim) : NULL;
  if (!a || !a->step_count) return;
  if (anim == MMX_AXL_ANIM_SHOT || anim == MMX_AXL_ANIM_SHOT_AIR || anim == MMX_AXL_ANIM_SHOT_SLIDE) {
    state.anim_step = state.pose_aim < a->step_count ? state.pose_aim : 0;
    return;
  }
  if (anim == MMX_AXL_ANIM_DODGE) {
    unsigned t = state.roll ? state.roll - 1u : 0, step = 0;
    while (step + 1 < a->step_count && t >= a->steps[step].duration_ticks) t -= a->steps[step++].duration_ticks;
    state.anim_step = (uint8_t)step;
    return;
  }
  if (++state.anim_tick >= a->steps[state.anim_step].duration_ticks) {
    state.anim_tick = 0;
    if (state.anim_step + 1u < a->step_count) ++state.anim_step;
    else if (looping(anim)) state.anim_step = 0;
  }
}

void MmxAxlFrameEnd(void) {
  if (!enabled) return;
  if (state.arms_lent) { g_ram[0x1f99] &= (uint8_t)~2; state.arms_lent = 0; }
  uint8_t now = 0;
  for (unsigned i = 0; i < 8; ++i) if (g_ram[0x1228 + i * 64]) now |= 1u << i;
  uint8_t fresh = now & ~live_slots;
  unsigned in = state.prev_input_lo | state.prev_input_hi << 8;

  /* The title: when Axl starts, his three pistol shots replace the title
   * shot (drawn by MmxAxlFrame). */
  if (team_start == MMX_AXL_CHAR_AXL && title_cursor(g_ram)) {
    if (fresh && !state.title_shot) state.title_shot = 1;
    else if (state.title_shot && state.title_shot < 120) ++state.title_shot;
    advance_animation(state.title_shot && state.title_shot < 30 ? MMX_AXL_ANIM_SHOT : MMX_AXL_ANIM_IDLE);
    live_slots = now;
    return;
  }
  state.title_shot = 0;

  /* Any freshly reused slot ends the shot that held it. */
  for (unsigned i = 0; i < MMX_AXL_SHOTS; ++i) {
    MmxAxlShot *s = &state.shots[i];
    if (!s->slot) continue;
    unsigned b = (s->slot - 0x1228) / 64;
    if (!g_ram[s->slot] || (fresh & (1u << b))) s->slot = 0;
  }
  bool active = MmxAxlActive() && shown(g_ram);
  for (unsigned i = 0; i < 8; ++i) {
    unsigned d = 0x1228 + i * 64;
    /* Only buster lemons: a special weapon's first object can share kind
     * $00, and its own motion and expiry must stay native. */
    if (!(fresh & (1u << i)) || d == state.giga_slot || g_ram[d + 10] != 0 || g_ram[0xbdb]) continue;
    if (!state.template_valid) {
      memcpy(state.giga_template, g_ram + d, 64);
      state.template_valid = 1;
    }
    if (active) {
      track_shot(d, state.copy_pending ? 1 : 0, in);
      state.copy_pending = 0;
    }
  }
  state.copy_pending = 0;
  /* Axl's shots keep their own straight lines; the native frame's motion is
   * replaced, its collision and expiry are not. */
  int cx = (int)word(g_ram, 0x1e4d), cy = (int)word(g_ram, 0x1e50);
  for (unsigned i = 0; i < MMX_AXL_SHOTS; ++i) {
    MmxAxlShot *s = &state.shots[i];
    if (!s->slot) continue;
    s->x += s->vx; s->y += s->vy;
    if (s->age < 255) ++s->age;
    int px = s->x >> 8, py = s->y >> 8;
    if (px < cx - 48 || px > cx + 304 || py < cy - 48 || py > cy + 272) {
      memset(g_ram + s->slot, 0, 64); s->slot = 0; cap_lifted = true;
      continue;
    }
    place(s->slot, s->x, s->y);
  }
  if (state.giga) {
    giga_tick();
    cap_lifted = true;
  }
  live_slots = 0;
  for (unsigned i = 0; i < 8; ++i) if (g_ram[0x1228 + i * 64]) live_slots |= 1u << i;
  /* $0BDD counts the live shots X1 limits, and X1 changes weapons only at
   * zero. After the cap was lifted, recount the buster shots, but only while
   * no special-weapon shot is alive: those keep their own native count. */
  if (cap_lifted) {
    unsigned count = 0, special = 0;
    for (unsigned i = 0; i < 8; ++i) {
      unsigned d = 0x1228 + i * 64;
      if (!(live_slots & (1u << i)) || d == state.giga_slot) continue;
      if (g_ram[d + 10] <= 2) ++count; else ++special;
    }
    if (!special) {
      g_ram[0xbdd] = (uint8_t)count;
      if (count < 3 && !state.giga) cap_lifted = false;
    }
  }
  unsigned beam_anim, beam_step;
  if (MmxAxlActive() && MmxWidePolicy_IsStageScene(g_ram) && body_group(g_ram) &&
      beam_frame(g_ram[0xbaa], g_ram[0xbbf], &beam_anim, &beam_step)) {
    state.anim = (uint8_t)beam_anim; state.anim_step = (uint8_t)beam_step; state.anim_tick = 0;
    return;
  }
  if (!active) { state.anim = 0; return; }
  if (state.wall_y[0] || state.wall_y[1]) {
    if (on_wall(g_ram[0xbaa])) putword(g_ram, 0xbb0, state.wall_y[0] | state.wall_y[1] << 8);
    else state.wall_y[0] = state.wall_y[1] = 0;
  }
  if (state.shot_pose) --state.shot_pose;
  if (state.dash && ++state.dash > dash_frames()) {
    state.dash = 0;
    putword(g_ram, 0xbc4, 0);
  }
  if (state.hover && state.hover < 255) ++state.hover;
  if ((state.roll && state.roll <= ROLL_GUARD) || state.giga) guard();
  /* The crouch out of the roll can be hit at once: end the guard's latches,
   * and X1's blink with them, which would otherwise stop on a hidden frame. */
  if (state.roll == ROLL_GUARD + 1) {
    g_ram[0xc13] = g_ram[0xbd8] = g_ram[0xc32] = 0;
    g_ram[0xbb6] = 1;
  }

  advance_animation(pick_animation(g_ram[0xbaa]));
}

/* ---- presentation ------------------------------------------------------ */
static bool plane_for(unsigned anim, unsigned age, bool hold, MmxRenderPlayerOverlayPlane *out) {
  const MmxSaberAnimation *a = MmxSaberAssetsAnimationById(assets, (uint16_t)anim);
  if (!a || !a->step_count) return false;
  unsigned step = 0, t = a->total_ticks ? (hold ? age : age % a->total_ticks) : 0;
  while (step + 1 < a->step_count && t >= a->steps[step].duration_ticks) t -= a->steps[step++].duration_ticks;
  const MmxSaberFrame *f = MmxSaberAssetsFrameForStep(assets, (uint16_t)anim, (uint16_t)step);
  if (!f || !f->body.pixels) return false;
  out->pixels = f->body.pixels; out->width = f->body.width; out->height = f->body.height;
  out->origin_x = f->body.origin_x; out->origin_y = f->body.origin_y;
  return true;
}
static void add_sprite(MmxRenderAxlFrame *out, unsigned anim, unsigned age, bool hold,
                       int32_t x, int32_t y, bool left) {
  if (out->sprite_count >= MMX_RENDER_AXL_SPRITES) return;
  MmxRenderAxlSprite *s = &out->sprites[out->sprite_count];
  if (!plane_for(anim, age, hold, &s->plane)) return;
  s->x = x; s->y = y; s->facing_left = left;
  ++out->sprite_count;
}
static void art(unsigned anim, MmxRenderPlayerOverlayPlane *out) {
  const MmxSaberFrame *f = MmxSaberAssetsFrameForStep(assets, (uint16_t)anim, 0);
  if (!f || !f->body.pixels) return;
  out->pixels = f->body.pixels; out->width = f->body.width; out->height = f->body.height;
  out->origin_x = f->body.origin_x; out->origin_y = f->body.origin_y;
}
/* X1's pause menu: X's portrait object at $1988 (group $00 or $18). */
static bool pause_menu(const uint8_t *ram) {
  return ram[0x1989] == 1 && word(ram, 0x198d) == 128 && word(ram, 0x1990) == 160 &&
      (ram[0x199e] == 0 || ram[0x199e] == 0x18);
}
static bool body_for(const uint8_t *ram, MmxRenderAxlFrame *out) {
  const MmxSaberFrame *f = MmxSaberAssetsFrameForStep(assets, state.anim, state.anim_step);
  if (!f || !f->body.pixels) return false;
  MmxRenderPlayerOverlay *b = &out->body;
  b->active = true;
  b->body.pixels = f->body.pixels;
  b->body.width = f->body.width; b->body.height = f->body.height;
  b->body.origin_x = f->body.origin_x; b->body.origin_y = f->body.origin_y;
  b->palette = MmxSaberAssetsPalette(assets);
  b->palette_count = MmxSaberAssetsPaletteCount(assets);
  b->facing_left = !(ram[0xbb9] & 0x40);
  if (state.roll) b->facing_left = !state.roll_facing;
  return true;
}
bool MmxAxlFrame(const uint8_t *ram, MmxRenderAxlFrame *out) {
  memset(out, 0, sizeof(*out));
  out->special = -1;
  if (!enabled || !assets || !ram) return false;
  out->body.palette = MmxSaberAssetsPalette(assets);
  out->body.palette_count = MmxSaberAssetsPaletteCount(assets);
  if (team_start == MMX_AXL_CHAR_AXL && title_cursor(ram)) {
    if (!state.anim || !body_for(ram, out)) return false;
    out->title = true;
    out->body.facing_left = false;
    out->screen_x = 32; out->screen_y = (int32_t)word(ram, 0xbb0) + 1;
    out->hidden_slots = 0xff;
    /* Three pistol shots, six frames apart. */
    if (state.title_shot) for (unsigned k = 0; k < 3; ++k) {
      int t = (int)state.title_shot - 1 - (int)k * FIRE_PERIOD;
      if (t < 0) continue;
      int32_t x = 32 + 34 + t * (BULLET_SPEED >> 8) + (t * (BULLET_SPEED & 255) >> 8);
      if (x < 300) add_sprite(out, MMX_AXL_ANIM_BULLET, (unsigned)t, true, x, out->screen_y - 8, false);
    }
    return true;
  }
  /* CHANGE PLAYER's beam, when it carries Axl (X's body). */
  if (MmxZeroSwapping()) {
    MmxZeroState z = MmxZeroGetState();
    unsigned pose = MmxZeroSwapPose(&z), anim = MMX_AXL_ANIM_IDLE, step = 0;
    if (!z.active_x || !state.active || pose == 255) return false;
    if (pose && !beam_frame(0, pose, &anim, &step)) return false;
    const MmxSaberFrame *f = MmxSaberAssetsFrameForStep(assets, (uint16_t)anim, (uint16_t)step);
    if (!f || !f->body.pixels) return false;
    out->teleport.pixels = f->body.pixels;
    out->teleport.width = f->body.width; out->teleport.height = f->body.height;
    out->teleport.origin_x = f->body.origin_x; out->teleport.origin_y = f->body.origin_y;
    return true;
  }
  /* Axl's HUD badge, and his portrait and life head in the pause menu. */
  if (MmxAxlActive() && ram[0xd1] == 2 && ram[0xd2] == 4) {
    art(MMX_AXL_ANIM_HUD_BADGE, &out->badge);
    if (pause_menu(ram)) {
      out->menu = true;
      art(MMX_AXL_ANIM_HUD_LIFE, &out->life);
      art(MMX_AXL_ANIM_HUD_PAUSE, &out->portrait);
      return true;
    }
  }
  unsigned beam_anim, beam_step;
  bool beam = MmxAxlActive() && MmxWidePolicy_IsStageScene(ram) && body_group(ram) &&
      beam_frame(ram[0xbaa], ram[0xbbf], &beam_anim, &beam_step);
  if (!beam && (!MmxAxlActive() || !shown(ram) || ram[0xbaa] == 0x2c)) return false;
  /* Axl owns X's body: X's art never shows through, also on blank frames. */
  out->own_body = true;
  out->special = beam ? -1 : state.special / SPECIAL_UNIT;
  for (unsigned i = 0; i < MMX_AXL_SHOTS; ++i) {
    const MmxAxlShot *s = &state.shots[i];
    if (!s->slot || !ram[s->slot]) continue;
    out->hidden_slots |= (uint8_t)(1u << ((s->slot - 0x1228) / 64));
    /* The pistol bullet turns along its aim. */
    unsigned anim = s->kind ? MMX_AXL_ANIM_COPY_SHOT : !s->vy ? MMX_AXL_ANIM_BULLET :
        s->vy < 0 ? (s->vx ? MMX_AXL_ANIM_BULLET_UP_TILT : MMX_AXL_ANIM_BULLET_UP) :
        (s->vx ? MMX_AXL_ANIM_BULLET_DOWN_TILT : MMX_AXL_ANIM_BULLET_DOWN);
    add_sprite(out, anim, s->age, !s->kind, s->x >> 8, s->y >> 8, s->vx < 0);
  }
  if (state.giga_slot) out->hidden_slots |= (uint8_t)(1u << ((state.giga_slot - 0x1228) / 64));
  /* GigaCrashExplosion: the scene behind darkens and the streaks play over
   * the 4:3 screen through the blast, fading after it. */
  if (state.giga > GIGA_INTRO) {
    unsigned t = state.giga - GIGA_INTRO;
    out->screen_dim = (uint8_t)(state.giga <= GIGA_FIRE ? (t < 6 ? t : 6) :
                                (unsigned)(GIGA_END - state.giga) * 6 / (GIGA_END - GIGA_FIRE));
    if (state.giga <= GIGA_FIRE) {
      MmxRenderPlayerOverlayPlane plane;
      if (plane_for(MMX_AXL_ANIM_GIGA_SCREEN, t, false, &plane)) out->screen = plane;
    }
  }
  /* The roll flashes like a hurt blink, every two frames, to show it cannot
   * be hit; GigaCrash stays solid. Otherwise follow X's own hurt blink. */
  if (!state.anim) return true;
  if (state.roll && state.roll <= ROLL_GUARD ? ((state.roll >> 1) & 1) :
      (!ram[0xbb6] && !state.giga)) return true;
  body_for(ram, out);
  return true;
}
