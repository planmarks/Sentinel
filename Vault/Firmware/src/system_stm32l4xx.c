/* Minimal SystemInit for Muru Vault.
 * All real clock configuration is done in clock_init() (clock.c); here we only
 * enable the FPU and set the vector table offset. Leaves the CPU on the reset
 * default MSI ~4 MHz clock until clock_init() runs from main().
 */
#include "stm32l442xx.h"

uint32_t SystemCoreClock = 4000000U;   /* updated by clock_init() */

/* Vector table base: bootloader builds leave this at FLASH_BASE (0x08000000);
 * the app build passes -DVECT_TAB_ADDR=0x08008000 so its table is relocated. */
#ifndef VECT_TAB_ADDR
#define VECT_TAB_ADDR FLASH_BASE
#endif

void SystemInit(void)
{
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  SCB->CPACR |= ((3UL << 20) | (3UL << 22));   /* CP10 & CP11 full access */
#endif
  SCB->VTOR = VECT_TAB_ADDR;
}

void SystemCoreClockUpdate(void) { /* handled by clock_init() */ }
