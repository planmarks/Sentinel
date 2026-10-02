#ifndef FLASH_STORE_H
#define FLASH_STORE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Logical regions inside the 8 KB reserved STORE area (see linker script).
 *   AUTH  : 1 page  (2 KB)  0x0803E000
 *   CFG   : 1 page  (2 KB)  0x0803E800
 *   VAULT : 2 pages (4 KB)  0x0803F000
 */
typedef enum { REGION_AUTH = 0, REGION_CFG = 1, REGION_VAULT = 2 } StoreRegion;

const uint8_t *store_ptr(StoreRegion r);   /* memory-mapped read pointer */
size_t         store_size(StoreRegion r);  /* bytes available in the region */

/* Erase the region's pages then program `len` bytes (padded to /8 with 0xFF).
 * Returns false on any flash error. len must be <= store_size(r). */
bool store_write(StoreRegion r, const void *data, size_t len);
bool store_erase(StoreRegion r);

#endif
