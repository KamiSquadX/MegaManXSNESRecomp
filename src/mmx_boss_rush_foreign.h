#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* X2/X3 Mavericks for Boss Rush. Their packs hold the user's validated source
 * ROM in memory only; nothing derived from it is written or distributed.
 * A boss is offered to new runs only when its game's pack is loaded and its
 * body is implemented, so X1-only sessions keep the original roster. */
bool MmxBossRushForeignLoad(unsigned game,const char *rom,char *error,size_t error_size);
void MmxBossRushForeignUnload(void);
bool MmxBossRushForeignReady(unsigned game);
const uint8_t *MmxBossRushForeignRom(unsigned game,size_t *size);
/* Roster ids (8..23) whose bodies exist in this build. */
uint32_t MmxBossRushForeignImplemented(void);
/* Body controller built in a cart extension bank for a roster id, or 0.
 * Registering one marks that body implemented; 0 removes it. */
bool MmxBossRushForeignSetController(unsigned id,uint32_t controller);
uint32_t MmxBossRushForeignController(unsigned id);
/* Body states (+$01) that mean the entrance finished and that it is dying;
 * X1 bodies use 4 and 6. Defaults match X1 until a body registers its own. */
bool MmxBossRushForeignSetStates(unsigned id,uint8_t fighting,uint8_t dying);
void MmxBossRushForeignStates(unsigned id,uint8_t *fighting,uint8_t *dying);
/* Controllers for the boss's own projectile classes (enemy-projectile list). */
enum { MMX_RUSH_FOREIGN_CHILDREN=4 };
bool MmxBossRushForeignSetChild(unsigned id,uint8_t kind,uint32_t controller);
uint32_t MmxBossRushForeignChild(unsigned id,uint8_t kind);
/* Shim addresses standing in for the source game's sound-effect calls
 * (plain and positional); the runtime hooks them to queue cached clips. */
void MmxBossRushForeignSetSoundShims(uint32_t plain,uint32_t positional);
void MmxBossRushForeignSoundShims(uint32_t *plain,uint32_t *positional);
/* Rebuild every relocated body for the loaded source ROMs into the cart's
 * patch windows (mmx_boss_rush_relocate.c). Restores the cart first. */
bool MmxBossRushForeignBuild(char *error,size_t error_size);
/* After a pack loads or unloads, rebuild or restore the cart once the game
 * image exists. Called from the host frame; idempotent. */
void MmxBossRushForeignSync(void);
/* Bosses the next run may draw from: X1 plus every loaded body that is
 * finished (art, sound, weaknesses). Preview also offers unfinished bodies. */
uint32_t MmxBossRushForeignOffered(void);
void MmxBossRushForeignSetPreview(bool enabled);
