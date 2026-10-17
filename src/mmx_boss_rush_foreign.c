#include "mmx_boss_rush_foreign.h"
#include "mmx_boss_rush.h"
#include "mmx_source_assets.h"
#include <stdlib.h>

static uint8_t *roms[2];
static size_t sizes[2];

/* A body is implemented once its relocated controller is registered.
 * Tunnel Rhino (id 23) is the pilot. */
static uint32_t implemented;
static uint32_t controllers[MMX_RUSH_ROSTER];

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
bool MmxBossRushForeignSetController(unsigned id,uint32_t controller) {
  if(MmxBossRushNative(id) || id>=MMX_RUSH_ROSTER || controller>0xffffff) return false;
  controllers[id]=controller;
  if(controller) implemented|=1u<<id; else implemented&=~(1u<<id);
  publish();return true;
}
uint32_t MmxBossRushForeignController(unsigned id) {
  return id<MMX_RUSH_ROSTER?controllers[id]:0;
}
uint32_t MmxBossRushForeignOffered(void) {
  uint32_t mask=MMX_RUSH_X1_MASK;
  if(MmxBossRushForeignReady(2)) mask|=implemented&MMX_RUSH_X2_MASK;
  if(MmxBossRushForeignReady(3)) mask|=implemented&MMX_RUSH_X3_MASK;
  return mask;
}
