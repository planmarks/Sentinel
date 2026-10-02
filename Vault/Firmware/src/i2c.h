#ifndef I2C_H
#define I2C_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
void i2c_init(void);
/* Blocking master write of `len` bytes to 7-bit address `addr`. */
bool i2c_write(uint8_t addr, const uint8_t *buf, size_t len);
#endif
