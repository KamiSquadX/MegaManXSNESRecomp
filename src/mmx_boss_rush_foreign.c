#include "mmx_boss_rush_foreign.h"
#include "mmx_boss_rush.h"
#include "mmx_source_assets.h"
#include <stdlib.h>

static uint8_t *roms[2];
static size_t sizes[2];

/* No X2/X3 body is implemented yet; Tunnel Rhino (id 23) is the pilot. */
static const uint32_t implemented=0;

static void publish(void) {MmxBossRushSetOffered(MmxBossRushForeignOffered());}
bool MmxBossRushForeignLoad(unsigned game,const char *rom,char *error,size_t error_size) {
  if(game!=2 && game!=3) return false;
  unsigned char *image=NULL;size_t size=0;
  if(!MmxSourceRomLoad(rom,game,&image,&size,error,error_size)) return false;
  free(roms[game-2]);roms[game-2]=image;sizes[game-2]=size;
  publish();return true;
}
void MmxBossRushForeignUnload(void) {
  for(unsigned i=0;i<2;++i) {free(roms[i]);roms[i]=NULL;sizes[i]=0;}
  publish();
}
bool MmxBossRushForeignReady(unsigned game) {
  return (game==2 || game==3) && roms[game-2]!=NULL;
}
const uint8_t *MmxBossRushForeignRom(unsigned game,size_t *size) {
  if(!MmxBossRushForeignReady(game)) {if(size) *size=0;return NULL;}
  if(size) *size=sizes[game-2];
  return roms[game-2];
}
uint32_t MmxBossRushForeignImplemented(void) {return implemented;}
uint32_t MmxBossRushForeignOffered(void) {
  uint32_t mask=MMX_RUSH_X1_MASK;
  if(MmxBossRushForeignReady(2)) mask|=implemented&MMX_RUSH_X2_MASK;
  if(MmxBossRushForeignReady(3)) mask|=implemented&MMX_RUSH_X3_MASK;
  return mask;
}
