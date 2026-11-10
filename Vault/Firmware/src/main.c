/* Muru Vault - STM32L442KCU6 (Sentinel V1B)
 * Offline password generator + encrypted vault by @papathought / muru.global */
#include "board.h"
#include "i2c.h"
#include "ssd1306.h"
#include "buttons.h"
#include "rng.h"
#include "ui.h"

int main(void) {
  __enable_irq();          /* the bootloader hands off with IRQs masked; re-enable
                              so SysTick (and thus delay_ms/millis) runs */
  clock_init();
  buttons_init();
  rng_init();
  i2c_init();

  if (!oled_init()) {
    /* No display detected: nothing safe to do. Idle low-power forever. */
    for (;;) __WFI();
  }

  ui_run();          /* never returns */
  for (;;) {}
}
