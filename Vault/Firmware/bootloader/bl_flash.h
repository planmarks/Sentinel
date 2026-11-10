#ifndef BL_FLASH_H
#define BL_FLASH_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Generic flash erase/program for the bootloader (any 2 KB-page-aligned range
 * in bank 1). Interrupt-masked during the operation. */
bool bl_flash_erase(uint32_t addr, uint32_t len);               /* erases whole pages covering the range */
bool bl_flash_program(uint32_t addr, const void *data, uint32_t len); /* addr 8-byte aligned; len padded to 8 with 0xFF */

#endif
