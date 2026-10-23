/* Add Axl and Solo Team select: Axl from Mega Man X8 16-bit on X's body
 * (src/mmx_axl.c), and the starting/partner characters of a solo game. */
#include "mmx_mod_constructor.h"
#include "snes/interp_bridge.h"
#include "mmx_axl.h"
#include "mmx_renderer.h"
#include "mmx_zero.h"
#include "mod_audio.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char *const kAxl = "megaman-x.character.axl";
static const char *const kZero = "megaman-x.character.zero";
static const char *const kTeam = "megaman-x.team-select";

static unsigned character(const char *value) {
  return !strcmp(value, "zero") ? MMX_AXL_CHAR_ZERO :
      !strcmp(value, "axl") ? MMX_AXL_CHAR_AXL :
      !strcmp(value, "none") ? MMX_AXL_CHAR_ALONE : MMX_AXL_CHAR_X;
}
/* The chosen team. Solo Team select follows Add Zero and Add Axl, and the
 * launcher greys out a character whose mod is off; a value saved before it
 * was turned off falls back: the start to X, the partner to another added
 * character (or none). False when Solo Team select is off. */
static bool team(unsigned *start, unsigned *partner) {
  if (!snes_mod_runtime_feature_enabled_c(kTeam, "team")) return false;
  char a[16] = {0}, b[16] = {0};
  snes_mod_runtime_feature_option_value_c(kTeam, "team", "start", a, sizeof(a));
  snes_mod_runtime_feature_option_value_c(kTeam, "team", "partner", b, sizeof(b));
  bool added[3] = {true, snes_mod_runtime_feature_enabled_c(kZero, "zero") != 0,
                   snes_mod_runtime_feature_enabled_c(kAxl, "axl") != 0};
  *start = character(a); *partner = character(b);
  if (*start > MMX_AXL_CHAR_AXL || !added[*start]) *start = MMX_AXL_CHAR_X;
  if (*partner != MMX_AXL_CHAR_ALONE && (*partner > MMX_AXL_CHAR_AXL || !added[*partner] ||
                                         *partner == *start)) {
    static const unsigned order[3] = {MMX_AXL_CHAR_ZERO, MMX_AXL_CHAR_AXL, MMX_AXL_CHAR_X};
    *partner = MMX_AXL_CHAR_ALONE;
    for (unsigned i = 0; i < 3; ++i)
      if (added[order[i]] && order[i] != *start) { *partner = order[i]; break; }
  }
  return true;
}
/* Interpreted copies of the dash-upgrade loads ($81:971C, $81:9793,
 * $81:98FC); the compiled ones call MmxZeroUpgradeBits. */
static void upgrade_hook(CpuState *cpu, uint32_t pc) {
  (void)pc;
  if (MmxAxlActive()) cpu->A |= 8;
}
/* GigaCrash's sounds, X8 16-bit's own OGGs compiled in (cmake/axl.cmake). */
extern const uint8_t kMmxAxlEmbeddedGigaStart[], kMmxAxlEmbeddedGigaShot[];
extern const size_t kMmxAxlEmbeddedGigaStartSize, kMmxAxlEmbeddedGigaShotSize;
extern int stb_vorbis_decode_memory(const unsigned char *mem, int len, int *channels,
                                    int *sample_rate, short **output);
bool RtlSpeculativeFrame(void);
static SNESModAudioClip giga_clips[2];
static SNESModAudioClip register_ogg(const uint8_t *data, size_t size) {
  int channels = 0, rate = 0;
  short *pcm = NULL;
  int frames = stb_vorbis_decode_memory(data, (int)size, &channels, &rate, &pcm);
  SNESModAudioClip clip = SNES_MOD_AUDIO_CLIP_INVALID;
  if (frames > 0 && pcm && (channels == 1 || channels == 2))
    clip = snes_mod_audio_register_pcm_s16(pcm, (uint32_t)frames, (uint32_t)rate, (uint32_t)channels);
  free(pcm);
  return clip;
}
static void play_sound(unsigned cue) {
  if (cue < 2 && giga_clips[cue] && !RtlSpeculativeFrame()) snes_mod_audio_play(giga_clips[cue], 100);
}
static void activate(void) {
  /* The co-op modes claim this plugin only to keep apart from Add Axl. */
  if (!snes_mod_runtime_feature_enabled_c(kAxl, "axl")) return;
  unsigned start = MMX_AXL_CHAR_X, partner = MMX_AXL_CHAR_NONE;
  team(&start, &partner);
  MmxAxlSetTeam(start, partner);
  MmxAxlEnable(true);
  if (!MmxAxlEnabled()) return;
  MmxAxlInstallHooks(true);
  MmxRendererSetAxlProvider(MmxAxlFrame);
  if (!giga_clips[0]) giga_clips[0] = register_ogg(kMmxAxlEmbeddedGigaStart, kMmxAxlEmbeddedGigaStartSize);
  if (!giga_clips[1]) giga_clips[1] = register_ogg(kMmxAxlEmbeddedGigaShot, kMmxAxlEmbeddedGigaShotSize);
  MmxAxlSetSoundHook(play_sound);
  const unsigned pcs[] = {0x01971f, 0x019796, 0x0198ff};
  for (unsigned i = 0; i < sizeof(pcs) / sizeof(pcs[0]); ++i)
    interp_bridge_add_pre_opcode_hook(pcs[i], upgrade_hook);
  fprintf(stderr, "[mmx-axl] X8 Axl 0.0.1 enabled; team %u/%u\n", start, partner);
}
static void activate_team(void) {
  if (!snes_mod_runtime_feature_enabled_c(kTeam, "team")) return;
  unsigned start, partner;
  if (!team(&start, &partner)) return;
  /* Also without Add Axl (an X and Zero team, or one character alone):
   * the team decides what CHANGE PLAYER does. */
  MmxAxlSetTeam(start, partner);
  MmxAxlResetState();
}
static void reset(void) {
  MmxAxlSetSoundHook(NULL);
  for (unsigned i = 0; i < 2; ++i)
    if (giga_clips[i]) { snes_mod_audio_unregister(giga_clips[i]); giga_clips[i] = SNES_MOD_AUDIO_CLIP_INVALID; }
  MmxAxlDisable();
  MmxAxlInstallHooks(false);
  MmxRendererSetAxlProvider(NULL);
}
SNES_MOD_CONSTRUCTOR(mmx_register_axl_plugin) {
  (void)snes_mod_register_reset_callback(reset);
  (void)snes_mod_register_activation_plugin("megaman-x.axl", activate);
  (void)snes_mod_register_activation_plugin("megaman-x.team", activate_team);
}
