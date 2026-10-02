#ifndef SSD1306_H
#define SSD1306_H
#include <stdint.h>
#include <stdbool.h>

#define OLED_W 128
#define OLED_H 64

bool oled_init(void);
void oled_flush(void);
void oled_off(void);   /* display + charge pump off (for low power) */

void fb_clear(void);
void fb_pixel(int x, int y, bool on);
void fb_fillrect(int x, int y, int w, int h, bool on);
void fb_rect(int x, int y, int w, int h, bool on);

/* scale=1 -> 6x8 cell, scale=2 -> 12x16, etc. Returns x advance in pixels. */
int  fb_char(int x, int y, char ch, int scale, bool on);
int  fb_text(int x, int y, const char *s, int scale, bool on);
int  text_width(const char *s, int scale);

#endif
