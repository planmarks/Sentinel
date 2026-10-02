#ifndef FONT_H
#define FONT_H
#include <stdint.h>
/* 5x7 font, column-major, 5 bytes per glyph, ASCII 0x20..0x7E.
 * Bit0 = top row. A 1px space column is added by the renderer. */
extern const uint8_t FONT5X7[(0x7F-0x20)][5];
#endif
