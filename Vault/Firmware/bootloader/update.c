/* Bootloader firmware installer.
 *
 * STAGING holds a streamed signed image: [fw_header(96)] [payload]. We verify
 * its Ed25519 signature (over SHA-256(core||payload)), refuse anything older
 * than the installed version (anti-rollback), then program the payload to APP
 * and the header to MANIFEST, re-verify, and wipe staging. The install order
 * (staging wiped last) makes it power-fail safe: an interrupted install simply
 * retries on the next boot. */
#include "board.h"
#include "fwimage.h"
#include "boot_verify.h"
#include "bl_flash.h"
#include "i2c.h"
#include "ssd1306.h"
#include "update.h"

#define STAGING_PAYLOAD (STAGING_BASE + (uint32_t)sizeof(fw_header))

static void banner(const char *l1, const char *l2) {
  fb_clear();
  fb_fillrect(0, 0, OLED_W, 11, true);
  fb_text(2, 2, "Firmware update", 1, false);
  if (l1) fb_text(0, 24, l1, 1, true);
  if (l2) fb_text(0, 38, l2, 1, true);
  oled_flush();
}

bool bl_try_update(void) {
  const fw_header *stg = (const fw_header *)STAGING_BASE;
  if (stg->magic != FW_MAGIC) return false;        /* no staged image */

  /* Bound checks before trusting length. */
  if (stg->length == 0 || stg->length > APP_MAX ||
      (uint32_t)sizeof(fw_header) + stg->length > STAGING_MAX) {
    bl_flash_erase(STAGING_BASE, 2048u);
    return false;
  }

  if (!fw_verify(stg, STAGING_PAYLOAD)) {          /* bad signature -> discard */
    bl_flash_erase(STAGING_BASE, 2048u);
    return false;
  }

  /* Anti-rollback: never install a version older than what's installed. */
  const fw_header *cur = (const fw_header *)MANIFEST_ADDR;
  uint32_t curver = (cur->magic == FW_MAGIC) ? cur->version : 0u;
  if (stg->version < curver) {
    bl_flash_erase(STAGING_BASE, 2048u);
    return false;
  }

  /* Install. */
  i2c_init();
  oled_init();
  banner("Installing...", "do not power off");

  bool ok = bl_flash_erase(APP_BASE, stg->length);
  ok = bl_flash_program(APP_BASE, (const void *)STAGING_PAYLOAD, stg->length) && ok;
  ok = bl_flash_erase(MANIFEST_ADDR, (uint32_t)sizeof(fw_header)) && ok;
  ok = bl_flash_program(MANIFEST_ADDR, stg, (uint32_t)sizeof(fw_header)) && ok;

  if (ok && fw_verify((const fw_header *)MANIFEST_ADDR, APP_BASE)) {
    banner("Update complete", "restarting");
  } else {
    banner("Install FAILED", "will retry/boot");
  }

  /* Wipe staging last so an interrupted install retries on next boot. */
  bl_flash_erase(STAGING_BASE, (uint32_t)sizeof(fw_header) + stg->length);
  for (volatile int i = 0; i < 2000000; i++) {}   /* brief pause to show status */
  return true;
}
