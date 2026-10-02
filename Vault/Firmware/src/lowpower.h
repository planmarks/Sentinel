#ifndef LOWPOWER_H
#define LOWPOWER_H
/* Enter STOP 2. Returns after the OK button (PA1) wakes the CPU, with the
 * HSI16 clock, SysTick and RNG restored. The caller owns the display: turn it
 * off before calling and re-init/redraw after. */
void low_power_enter(void);
#endif
