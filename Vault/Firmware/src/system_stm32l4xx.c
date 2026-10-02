/* Minimal SystemInit for Muru Vault.
 * All real clock configuration is done in clock_init() (clock.c); here we only
 * enable the FPU and set the vector table offset. Leaves the CPU on the reset
 * default MSI ~4 MHz clock until clock_init() runs from main().
 */
#include "stm32l442xx.h"

uint32_t SystemCoreClock = 4000000U;   /* updated by clock_init() */

void SystemInit(void)
{
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  SCB->CPACR |= ((3UL << 20) | (3UL << 22));   /* CP10 & CP11 full access */
#endif
  /* Vector table located at start of flash. */
  SCB->VTOR = FLASH_BASE;
}

void SystemCoreClockUpdate(void) { /* handled by clock_init() */ }
