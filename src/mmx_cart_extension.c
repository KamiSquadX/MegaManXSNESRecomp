#include "mmx_cart_extension.h"
#include "common_cpu_infra.h"
#include "common_rtl.h"
#include "snes/snes.h"
#include "snes/cart.h"
#include "snes/interp_bridge.h"
#include <stdlib.h>
#include <string.h>

enum { ORIGINAL=0x200000, EXTENDED=0x400000 };

static size_t offset(uint32_t address) {
  /* LoROM: bank & $7F selects a 32 KB page of the image. */
  return ((size_t)((address>>16)&0x7f)<<15)|(address&0x7fff);
}
bool MmxCartExtensionReady(void) {
  return g_snes && g_snes->cart && g_snes->cart->type==CART_LOROM &&
      g_snes->cart->romSize==EXTENDED;
}
bool MmxCartExtensionEnsure(void) {
  if(MmxCartExtensionReady()) return true;
  Cart *cart=g_snes?g_snes->cart:NULL;
  if(!cart || cart->type!=CART_LOROM || cart->romSize!=ORIGINAL || !cart->rom) return false;
  uint8_t *image=malloc(EXTENDED);
  if(!image) return false;
  memcpy(image,cart->rom,ORIGINAL);memcpy(image+ORIGINAL,cart->rom,ORIGINAL);
  /* Every ROM read resolves through the cart and g_rom; replace both before
   * freeing the old image. romImageSize keeps the original identity. */
  uint8_t *old=cart->rom;cart->rom=image;cart->romSize=EXTENDED;g_rom=image;
  free(old);
  return true;
}
bool MmxCartExtensionWrite(uint32_t address,const void *data,size_t size) {
  unsigned bank=(address>>16)&0xff;
  if(!MmxCartExtensionReady() || !data || bank<MMX_CART_EXTENSION_FIRST_BANK ||
      bank>MMX_CART_EXTENSION_LAST_BANK || (address&0xffff)<0x8000 ||
      size>0x10000-(address&0xffff)) return false;
  memcpy(g_snes->cart->rom+offset(address),data,size);
  interp_bridge_reset_dynamic_cache(); /* never execute stale decoded code */
  return true;
}
void MmxCartExtensionClear(void) {
  if(!MmxCartExtensionReady()) return;
  uint8_t *rom=g_snes->cart->rom;
  for(unsigned bank=MMX_CART_EXTENSION_FIRST_BANK;bank<=MMX_CART_EXTENSION_LAST_BANK;++bank) {
    size_t at=offset((uint32_t)bank<<16|0x8000);
    memcpy(rom+at,rom+at-ORIGINAL,0x8000);
  }
  interp_bridge_reset_dynamic_cache();
}
