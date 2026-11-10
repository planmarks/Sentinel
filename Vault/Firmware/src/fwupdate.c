/* USB-CDC firmware update mode (app side).
 *
 * Host protocol (see tools/fw_send.py):
 *   host -> "MVUP" + uint32 LE total_len        (total_len = size of the signed blob)
 *   dev  -> "RDY\n"
 *   host -> <total_len bytes of blob>           (streamed)
 *   dev  -> "DONE\n" (reset follows) or "ERR\n"
 *
 * The blob = [fw_header(96)][payload]; we write it verbatim to STAGING and
 * reset. The bootloader does the Ed25519 verify + anti-rollback + install. */
#include "board.h"
#include "ssd1306.h"
#include "usbfs.h"
#include "flashraw.h"
#include "fwimage.h"
#include "fwupdate.h"
#include <string.h>

static void screen(const char *l1, const char *l2, const char *l3) {
  fb_clear();
  fb_fillrect(0, 0, OLED_W, 11, true);
  fb_text(2, 2, "Firmware update", 1, false);
  if (l1) fb_text(0, 20, l1, 1, true);
  if (l2) fb_text(0, 32, l2, 1, true);
  if (l3) fb_text(0, 46, l3, 1, true);
  oled_flush();
}
static void progress(uint32_t done, uint32_t total) {
  fb_clear();
  fb_fillrect(0, 0, OLED_W, 11, true);
  fb_text(2, 2, "Receiving...", 1, false);
  int pct = total ? (int)((uint64_t)done * 100 / total) : 0;
  fb_rect(10, 28, 108, 14, true);
  fb_fillrect(12, 30, (104 * pct) / 100, 10, true);
  char p[6]; int n = 0;
  if (pct >= 100) { p[n++]='1'; p[n++]='0'; p[n++]='0'; }
  else { if (pct >= 10) p[n++]=(char)('0'+pct/10); p[n++]=(char)('0'+pct%10); }
  p[n++]='%'; p[n]=0;
  fb_text((OLED_W - (int)strlen(p)*6)/2, 48, p, 1, true);
  oled_flush();
}
static bool back_pressed(void) { return (BTN_PORT->IDR & (1u<<PIN_BACK)) == 0; }

void fwupdate_run(void) {
  screen("Plug into a PC", "and run the", "updater. BACK=x");

  usb_clock_init();
  usb_init();

  uint8_t hdr[8]; int hdr_n = 0;
  uint32_t total = 0, written = 0, last_shown = 0;
  uint8_t  acc[256]; int acc_n = 0;
  uint32_t idle = millis();
  enum { WAIT_CFG, WAIT_HDR, STREAM } st = WAIT_CFG;
  bool rdy_sent = false;

  for (;;) {
    usb_poll();
    if (back_pressed()) { usb_deinit(); screen("Cancelled", 0, 0); delay_ms(600); return; }

    uint8_t tmp[64];
    int n = usb_cdc_recv(tmp, sizeof tmp);
    if (n > 0) idle = millis();

    if (st == WAIT_CFG) {
      if (usb_cdc_configured()) { st = WAIT_HDR; screen("Connected", "waiting for", "image..."); }
    }

    for (int i = 0; i < n; i++) {
      uint8_t b = tmp[i];
      if (st == WAIT_HDR) {
        hdr[hdr_n++] = b;
        if (hdr_n == 8) {
          if (hdr[0]=='M'&&hdr[1]=='V'&&hdr[2]=='U'&&hdr[3]=='P') {
            total = (uint32_t)hdr[4] | (hdr[5]<<8) | (hdr[6]<<16) | ((uint32_t)hdr[7]<<24);
            if (total >= sizeof(fw_header) && total <= STAGING_MAX) {
              flashraw_erase(STAGING_BASE, total);
              written = 0; acc_n = 0; last_shown = 0;
              usb_cdc_send((const uint8_t*)"RDY\n", 4);
              st = STREAM; progress(0, total);
            } else { usb_cdc_send((const uint8_t*)"ERR\n", 4); hdr_n = 0; }
          } else { hdr_n = 0; }   /* resync */
        }
      } else if (st == STREAM) {
        acc[acc_n++] = b;
        if (acc_n == (int)sizeof acc) {
          flashraw_program(STAGING_BASE + written, acc, acc_n);
          written += acc_n; acc_n = 0;
          if (written - last_shown >= 4096) { progress(written, total); last_shown = written; }
        }
        if (written + acc_n >= total) {   /* final partial chunk */
          if (acc_n) { flashraw_program(STAGING_BASE + written, acc, acc_n); written += acc_n; acc_n = 0; }
          progress(written, total);
          const fw_header *stg = (const fw_header *)STAGING_BASE;
          if (stg->magic == FW_MAGIC) {
            usb_cdc_send((const uint8_t*)"DONE\n", 5);
            screen("Received OK", "installing &", "restarting...");
            delay_ms(800);
            NVIC_SystemReset();
          } else {
            usb_cdc_send((const uint8_t*)"ERR\n", 4);
            screen("Bad image", "magic mismatch", "BACK to exit");
            st = WAIT_HDR; hdr_n = 0;
          }
        }
      }
    }

    if ((uint32_t)(millis() - idle) > 60000u) { usb_deinit(); screen("Timed out", 0, 0); delay_ms(800); return; }
  }
}
