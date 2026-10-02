/* Buttons on PA0..PA5, active-LOW with internal pull-ups.
 *   UP=PA0  OK=PA1  DOWN=PA2  RIGHT=PA3  LEFT=PA4  BACK=PA5
 * Event indices (Event enum) map to a fixed pin per logical key. */
#include "board.h"
#include "buttons.h"

/* logical key order = Event order: UP,DOWN,OK,BACK,LEFT,RIGHT */
static const uint8_t key_pin[6] = { PIN_UP, PIN_DOWN, PIN_OK, PIN_BACK, PIN_LEFT, PIN_RIGHT };

static bool     st[6];
static uint32_t repeat_at[6];
static uint32_t down_time[6];
static bool     long_fired[6];
static uint32_t s_last_activity;

static const uint32_t REPEAT_DELAY = 420;
static const uint32_t REPEAT_RATE  = 110;
static const uint32_t LONG_MS      = 550;

void buttons_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  (void)RCC->AHB2ENR;
  for (int i = 0; i < 6; i++) {
    uint32_t p = key_pin[i];
    BTN_PORT->MODER &= ~(3u << (p*2));               /* input */
    BTN_PORT->PUPDR = (BTN_PORT->PUPDR & ~(3u<<(p*2))) | (1u<<(p*2)); /* pull-up */
  }
  for (int i = 0; i < 6; i++) { st[i]=false; long_fired[i]=false; }
  s_last_activity = millis();
}

bool ok_is_down(void) {
  return (BTN_PORT->IDR & (1u << PIN_OK)) == 0;
}

uint32_t last_activity_ms(void) { return s_last_activity; }

static bool pressed(int i) {
  return (BTN_PORT->IDR & (1u << key_pin[i])) == 0;
}

int read_event(void) {
  int ev = EV_NONE;
  uint32_t now = millis();
  for (int i = 0; i < 6; i++) {
    bool dir = (i == EV_UP || i == EV_DOWN || i == EV_LEFT || i == EV_RIGHT);
    bool pr = pressed(i);
    if (pr && !st[i]) {
      st[i] = true; down_time[i] = now; long_fired[i] = false;
      repeat_at[i] = now + REPEAT_DELAY;
      s_last_activity = now;
      if (dir) ev = i;
    } else if (pr && st[i]) {
      if (dir) {
        if (now >= repeat_at[i]) { repeat_at[i] = now + REPEAT_RATE; ev = i; s_last_activity = now; }
      } else if (!long_fired[i] && (now - down_time[i] >= LONG_MS)) {
        long_fired[i] = true;
        ev = (i == EV_OK) ? EV_OK_LONG : EV_BACK_LONG;
        s_last_activity = now;
      }
    } else if (!pr && st[i]) {
      st[i] = false; s_last_activity = now;
      if (!dir && !long_fired[i]) ev = i;   /* short tap on release */
    }
  }
  delay_ms(6);   /* crude debounce */
  return ev;
}

void wait_any_key(void) {
  while (read_event() == EV_NONE) {}
}
