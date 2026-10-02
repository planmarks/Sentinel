/* Muru Vault - board definitions for Sentinel V1B (STM32L442KCU6).
 *
 * Pin map (from STM32 Cold Wallet.net, V1B):
 *   I2C1  SCL = PB6 (AF4)   SDA = PB7 (AF4)   -> OLED SSD1306 @0x3C, OPTIGA @0x30
 *   OPTIGA reset (PB0/RST net) = PB0
 *   Buttons (active-LOW, internal pull-ups):
 *     UP=PA0  OK=PA1  DOWN=PA2  RIGHT=PA3  LEFT=PA4  BACK=PA5
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32l442xx.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ---- timing ---- */
void     clock_init(void);
uint32_t millis(void);
void     delay_ms(uint32_t ms);

/* ---- button GPIO port/pins (all on GPIOA) ---- */
#define BTN_PORT   GPIOA
#define PIN_UP     0
#define PIN_OK     1
#define PIN_DOWN   2
#define PIN_RIGHT  3
#define PIN_LEFT   4
#define PIN_BACK   5
#define BTN_MASK   ((1u<<PIN_UP)|(1u<<PIN_OK)|(1u<<PIN_DOWN)|(1u<<PIN_RIGHT)|(1u<<PIN_LEFT)|(1u<<PIN_BACK))

#endif
