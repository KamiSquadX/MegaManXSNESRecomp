#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Extension banks for code relocated from X2/X3 at activation.
 *
 * X1's 1.5 MB image is padded to 2 MB on load. LoROM banks $C0-$FF map to
 * image offsets 2-4 MB, which X1 never reads. Growing the image to 4 MB with
 * its upper half mirroring the original 2 MB keeps every existing read
 * identical; banks $F0-$FD ($8000-$FFFF) then hold built code and data.
 * The bytes are rebuilt identically on every peer from the same verified
 * source ROMs, so they are not part of snapshots. */
enum { MMX_CART_EXTENSION_FIRST_BANK=0xf0, MMX_CART_EXTENSION_LAST_BANK=0xfd };

/* Grow the loaded LoROM image once; false if the cart is not the expected
 * 2 MB LoROM image. Call before any extension code can run. */
bool MmxCartExtensionEnsure(void);
bool MmxCartExtensionReady(void);
/* Write within one extension bank's $8000-$FFFF window. */
bool MmxCartExtensionWrite(uint32_t address,const void *data,size_t size);
/* Restore every extension bank to the mirrored original image. */
void MmxCartExtensionClear(void);
