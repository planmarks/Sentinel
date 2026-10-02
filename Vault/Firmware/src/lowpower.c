/* STOP 2 low-power mode with wake on the OK button (PA1 -> EXTI line 1). */
#include "board.h"
#include "lowpower.h"
#include "rng.h"

volatile uint32_t g_wake;

void EXTI1_IRQHandler(void) {
  if (EXTI->PR1 & EXTI_PR1_PIF1) { EXTI->PR1 = EXTI_PR1_PIF1; g_wake++; }
}

static void exti_ok_enable(void) {
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  /* Map EXTI1 to port A (PA1 = OK). EXTICR[0] field for line 1 = bits [7:4]. */
  SYSCFG->EXTICR[0] &= ~(0xFu << 4);            /* 0 = port A */
  EXTI->IMR1  |= EXTI_IMR1_IM1;
  EXTI->FTSR1 |= EXTI_FTSR1_FT1;                /* falling edge: press pulls low */
  EXTI->RTSR1 &= ~EXTI_RTSR1_RT1;
  EXTI->PR1 = EXTI_PR1_PIF1;                    /* clear any pending */
  NVIC_SetPriority(EXTI1_IRQn, 2);
  NVIC_EnableIRQ(EXTI1_IRQn);
}

void low_power_enter(void) {
  RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
  exti_ok_enable();

  /* Select STOP 2 (LPMS = 010) and clear stale wake flags. */
  PWR->CR1 = (PWR->CR1 & ~PWR_CR1_LPMS) | (0x2u << PWR_CR1_LPMS_Pos);
  PWR->SCR = PWR_SCR_CWUF;

  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __DSB(); __WFI();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

  /* STOP falls back to MSI; bring HSI16, SysTick and the RNG back. */
  clock_init();
  rng_init();
}
