/* Clock + SysTick millisecond timebase.
 * SYSCLK = HSI16 (16 MHz), no external crystal (L442 is crystal-less here).
 * HSI48 is enabled so the RNG has its 48 MHz source (CLK48SEL = HSI48).
 */
#include "board.h"

extern uint32_t SystemCoreClock;

static volatile uint32_t s_ms = 0;

void SysTick_Handler(void) { s_ms++; }

uint32_t millis(void) { return s_ms; }

void delay_ms(uint32_t ms) {
  uint32_t start = s_ms;
  while ((uint32_t)(s_ms - start) < ms) { __WFI(); }
}

void clock_init(void) {
  /* Flash: 0 wait states are valid up to 16 MHz in VOS range 1. */
  FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_0WS
             | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

  /* Turn on HSI16 and switch SYSCLK to it. */
  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY)) {}
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}
  /* AHB/APB1/APB2 prescalers = 1 (reset default) -> HCLK=PCLK1=PCLK2=16 MHz. */

  /* HSI48 for the RNG 48 MHz clock domain; CLK48SEL=00 (HSI48) is reset value. */
  RCC->CRRCR |= RCC_CRRCR_HSI48ON;
  while (!(RCC->CRRCR & RCC_CRRCR_HSI48RDY)) {}

  SystemCoreClock = 16000000U;

  /* 1 kHz SysTick from the 16 MHz core clock. */
  SysTick->LOAD = 16000000U/1000U - 1U;
  SysTick->VAL  = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}
