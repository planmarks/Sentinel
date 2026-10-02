/* SSD1306 128x64 I2C driver + 1 KB framebuffer.
 * Display is soldered upside-down on the board, so the panel is initialised
 * flipped 180 (segment remap 0xA0 + COM scan 0xC0). */
#include "ssd1306.h"
#include "i2c.h"
#include "font.h"
#include <string.h>

#define OLED_ADDR 0x3C

static uint8_t fb[OLED_W * OLED_H / 8];   /* 1024 bytes, page-major */

static bool oled_cmd(uint8_t c) {
  uint8_t b[2] = { 0x00, c };
  return i2c_write(OLED_ADDR, b, 2);
}

static const uint8_t init_seq[] = {
  0xAE,             /* display off */
  0xD5, 0x80,       /* clock div */
  0xA8, 0x3F,       /* multiplex 1/64 */
  0xD3, 0x00,       /* display offset */
  0x40,             /* start line 0 */
  0x8D, 0x14,       /* charge pump on */
  0x20, 0x00,       /* memory mode: horizontal */
  0xA0,             /* segment remap (flipped: column 0 = SEG0) */
  0xC0,             /* COM scan direction (flipped) */
  0xDA, 0x12,       /* COM pins */
  0x81, 0xCF,       /* contrast */
  0xD9, 0xF1,       /* precharge */
  0xDB, 0x40,       /* vcom detect */
  0xA4,             /* resume to RAM content */
  0xA6,             /* normal (non-inverted) */
  0xAF,             /* display on */
};

bool oled_init(void) {
  for (unsigned i = 0; i < sizeof(init_seq); i++)
    if (!oled_cmd(init_seq[i])) return false;
  fb_clear();
  oled_flush();
  return true;
}

void oled_flush(void) {
  oled_cmd(0x21); oled_cmd(0); oled_cmd(127);   /* column range */
  oled_cmd(0x22); oled_cmd(0); oled_cmd(7);     /* page range */
  uint8_t buf[1 + sizeof(fb)];
  buf[0] = 0x40;                                 /* Co=0, D/C=1 (data) */
  memcpy(buf + 1, fb, sizeof(fb));
  i2c_write(OLED_ADDR, buf, sizeof(buf));
}

void oled_off(void) {
  oled_cmd(0xAE);          /* display off */
  oled_cmd(0x8D); oled_cmd(0x10);   /* charge pump off */
}

void fb_clear(void) { memset(fb, 0, sizeof(fb)); }

void fb_pixel(int x, int y, bool on) {
  if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
  uint8_t *p = &fb[x + (y >> 3) * OLED_W];
  uint8_t m = (uint8_t)(1u << (y & 7));
  if (on) *p |= m; else *p &= (uint8_t)~m;
}

void fb_fillrect(int x, int y, int w, int h, bool on) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      fb_pixel(x + i, y + j, on);
}

void fb_rect(int x, int y, int w, int h, bool on) {
  for (int i = 0; i < w; i++) { fb_pixel(x+i, y, on); fb_pixel(x+i, y+h-1, on); }
  for (int j = 0; j < h; j++) { fb_pixel(x, y+j, on); fb_pixel(x+w-1, y+j, on); }
}

int fb_char(int x, int y, char ch, int scale, bool on) {
  unsigned c = (unsigned char)ch;
  if (c < 0x20 || c > 0x7E) c = '?';
  const uint8_t *g = FONT5X7[c - 0x20];
  for (int col = 0; col < 5; col++) {
    uint8_t bits = g[col];
    for (int row = 0; row < 7; row++) {
      if (bits & (1u << row)) {
        if (scale == 1) fb_pixel(x + col, y + row, on);
        else fb_fillrect(x + col*scale, y + row*scale, scale, scale, on);
      }
    }
  }
  return 6 * scale;   /* 5 columns + 1 space */
}

int fb_text(int x, int y, const char *s, int scale, bool on) {
  int x0 = x;
  while (*s) { x += fb_char(x, y, *s++, scale, on); }
  return x - x0;
}

int text_width(const char *s, int scale) {
  int n = 0; while (s[n]) n++;
  return n * 6 * scale;
}
