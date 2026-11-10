#ifndef FLASHRAW_H
#define FLASHRAW_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Generic flash erase/program for the app side (used to stream a firmware image
 * into the STAGING region). Interrupt-masked during the operation. */
bool flashraw_erase(uint32_t addr, uint32_t len);               /* whole 2 KB pages covering range */
bool flashraw_program(uint32_t addr, const void *data, uint32_t len); /* addr 8-aligned; len padded to 8 */
#endif
