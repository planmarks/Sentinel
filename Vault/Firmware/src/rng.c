/* STM32L442 true hardware RNG (TRNG). Clocked from HSI48 (see clock.c). */
#include "board.h"
#include "rng.h"

void rng_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
  (void)RCC->AHB2ENR;
  RNG->CR |= RNG_CR_RNGEN;
}

uint32_t rng_u32(void) {
  for (;;) {
    uint32_t sr = RNG->SR;
    if (sr & (RNG_SR_SEIS | RNG_SR_CEIS)) {
      /* Clear seed/clock error and re-arm, per RM recovery sequence. */
      RNG->SR &= ~(RNG_SR_SEIS | RNG_SR_CEIS);
      RNG->CR &= ~RNG_CR_RNGEN;
      RNG->CR |= RNG_CR_RNGEN;
      continue;
    }
    if (sr & RNG_SR_DRDY) return RNG->DR;
  }
}

void rng_fill(uint8_t *buf, size_t n) {
  size_t i = 0;
  while (i < n) {
    uint32_t r = rng_u32();
    size_t c = (n - i < 4) ? (n - i) : 4;
    for (size_t j = 0; j < c; j++) buf[i+j] = (uint8_t)(r >> (j*8));
    i += c;
  }
}

uint32_t rng_below(uint32_t range) {
  if (range == 0) return 0;
  uint32_t limit = 0xFFFFFFFFu - (0xFFFFFFFFu % range);
  uint32_t r;
  do { r = rng_u32(); } while (r >= limit);
  return r % range;
}
