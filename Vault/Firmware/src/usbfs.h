#ifndef USBFS_H
#define USBFS_H
#include <stdint.h>
#include <stdbool.h>

/* Minimal polled USB 2.0 FS device (STM32L4 USB peripheral) exposing a single
 * CDC-ACM virtual COM port. Call usb_clock_init() (clock.c) before usb_init(). */
void usb_init(void);
void usb_poll(void);                 /* service enumeration + transfers; call often */
bool usb_cdc_configured(void);       /* true once enumerated and configured */

/* Drain received bytes (host -> device). Returns count copied (0 if none). */
int  usb_cdc_recv(uint8_t *buf, int max);
/* Queue bytes to send (device -> host). Returns bytes accepted (may be < len). */
int  usb_cdc_send(const uint8_t *buf, int len);

void usb_deinit(void);               /* disconnect + power down the peripheral */

#endif
