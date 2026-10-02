#ifndef RNG_H
#define RNG_H
#include <stdint.h>
#include <stddef.h>
void     rng_init(void);
uint32_t rng_u32(void);
void     rng_fill(uint8_t *buf, size_t n);
/* Uniform integer in [0,range) via rejection sampling (no modulo bias). */
uint32_t rng_below(uint32_t range);
#endif
