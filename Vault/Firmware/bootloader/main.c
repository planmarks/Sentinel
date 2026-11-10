/* Muru Vault secure bootloader.
 * Verifies the installed app's Ed25519 signature (over its SHA-256) and, only
 * if valid, jumps to it. An unsigned/corrupt/absent app is refused — the device
 * shows a status screen and waits (Phase 2: enter USB update mode here). */
#include "board.h"
#include "i2c.h"
#include "ssd1306.h"
#include "boot_verify.h"
#include "fwimage.h"
#include "update.h"

static void jump_to_app(uint32_t base) {
  uint32_t sp = *(volatile uint32_t *)(base);
  uint32_t pc = *(volatile uint32_t *)(base + 4);
  SysTick->CTRL = 0;                 /* stop the tick we started */
  SysTick->LOAD = 0; SysTick->VAL = 0;
  __disable_irq();
  SCB->VTOR = base;                  /* app re-sets this in its SystemInit too */
  __set_MSP(sp);
  __DSB(); __ISB();
  ((void (*)(void))pc)();            /* never returns */
  for (;;) {}
}

static void show(const char *l1, const char *l2, const char *l3) {
  fb_clear();
  fb_fillrect(0, 0, OLED_W, 11, true);
  fb_text(2, 2, "Muru Vault boot", 1, false);
  if (l1) fb_text(0, 20, l1, 1, true);
  if (l2) fb_text(0, 32, l2, 1, true);
  if (l3) fb_text(0, 44, l3, 1, true);
  oled_flush();
}

int main(void) {
  clock_init();
  i2c_init();
  oled_init();
  show("Verifying", "firmware...", 0);   /* secure-boot check takes a moment */

  /* If a validly-signed, non-rollback image is staged, install it then reboot
   * so the freshly-installed app is re-verified from a clean state. */
  if (bl_try_update()) {
    NVIC_SystemReset();
  }

  const fw_header *mf = (const fw_header *)MANIFEST_ADDR;
  if (fw_verify(mf, APP_BASE)) {
    jump_to_app(APP_BASE);
  }

  /* No valid firmware: refuse to run anything and report it. */
  i2c_init();
  oled_init();
  for (;;) {
    show("No valid firmware", "Signature check", "failed - update");
    delay_ms(1500);
    show("Connect USB to", "install signed", "firmware (soon)");
    delay_ms(1500);
  }
}
