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
/* Bosses the next run may draw from: X1 plus every loaded, implemented boss. */
uint32_t MmxBossRushForeignOffered(void);
