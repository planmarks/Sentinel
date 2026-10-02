/* I2C1 blocking master on PB6(SCL)/PB7(SDA), AF4. 400 kHz @ PCLK1=16 MHz. */
#include "board.h"
#include "i2c.h"

#define I2C_TIMINGR_400K_16M  0x0010061Au   /* ST reference value */
#define WAITLOOP  200000u

static bool wait_flag(volatile uint32_t *reg, uint32_t mask, bool set) {
  uint32_t g = WAITLOOP;
  while (g--) {
    bool has = (*reg & mask) != 0;
    if (has == set) return true;
  }
  return false;
}

void i2c_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
  RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;
  (void)RCC->APB1ENR1;

  /* PB6/PB7 -> AF mode, AF4, open-drain, high speed, pull-up. */
  GPIOB->MODER   = (GPIOB->MODER   & ~((3u<<(6*2))|(3u<<(7*2)))) | (2u<<(6*2)) | (2u<<(7*2));
  GPIOB->OTYPER |= (1u<<6) | (1u<<7);
  GPIOB->OSPEEDR |= (3u<<(6*2)) | (3u<<(7*2));
  GPIOB->PUPDR   = (GPIOB->PUPDR  & ~((3u<<(6*2))|(3u<<(7*2)))) | (1u<<(6*2)) | (1u<<(7*2));
  GPIOB->AFR[0]  = (GPIOB->AFR[0] & ~((0xFu<<(6*4))|(0xFu<<(7*4)))) | (4u<<(6*4)) | (4u<<(7*4));

  I2C1->CR1 &= ~I2C_CR1_PE;
  I2C1->TIMINGR = I2C_TIMINGR_400K_16M;
  I2C1->CR1 |= I2C_CR1_PE;
}

bool i2c_write(uint8_t addr, const uint8_t *buf, size_t len) {
  if (!wait_flag(&I2C1->ISR, I2C_ISR_BUSY, false)) return false;

  size_t remaining = len;
  size_t idx = 0;
  uint32_t chunk = (remaining > 255) ? 255 : remaining;
  uint32_t cr2 = ((uint32_t)addr << 1) | ((uint32_t)chunk << I2C_CR2_NBYTES_Pos);
  cr2 |= (remaining > 255) ? I2C_CR2_RELOAD : I2C_CR2_AUTOEND;
  I2C1->CR2 = cr2;
  I2C1->CR2 |= I2C_CR2_START;

  while (remaining) {
    for (uint32_t i = 0; i < chunk; i++) {
      if (!wait_flag(&I2C1->ISR, I2C_ISR_TXIS, true)) {
        if (I2C1->ISR & I2C_ISR_NACKF) { I2C1->ICR = I2C_ICR_NACKCF; }
        return false;
      }
      I2C1->TXDR = buf[idx++];
    }
    remaining -= chunk;
    if (remaining) {
      if (!wait_flag(&I2C1->ISR, I2C_ISR_TCR, true)) return false;
      chunk = (remaining > 255) ? 255 : remaining;
      uint32_t c2 = ((uint32_t)addr << 1) | ((uint32_t)chunk << I2C_CR2_NBYTES_Pos);
      c2 |= (remaining > 255) ? I2C_CR2_RELOAD : I2C_CR2_AUTOEND;
      I2C1->CR2 = c2;
    }
  }

  /* AUTOEND generated a STOP; acknowledge it. */
  wait_flag(&I2C1->ISR, I2C_ISR_STOPF, true);
  I2C1->ICR = I2C_ICR_STOPCF;
  return true;
}
