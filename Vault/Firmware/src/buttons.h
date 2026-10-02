#ifndef BUTTONS_H
#define BUTTONS_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
  EV_NONE = -1,
  EV_UP = 0, EV_DOWN, EV_OK, EV_BACK, EV_LEFT, EV_RIGHT,
  EV_OK_LONG, EV_BACK_LONG
} Event;

void  buttons_init(void);
int   read_event(void);          /* returns an Event, non-blocking-ish (6ms) */
void  wait_any_key(void);
bool  ok_is_down(void);          /* raw level, used to wake from low power */
uint32_t last_activity_ms(void); /* millis() of the most recent key edge */

#endif
