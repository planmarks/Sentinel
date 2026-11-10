#ifndef FWUPDATE_H
#define FWUPDATE_H
/* Enter USB update mode: receive a signed image over USB-CDC into STAGING,
 * then reset so the bootloader verifies + installs it. Returns if the user
 * cancels (BACK) or the transfer times out; resets the MCU on success. */
void fwupdate_run(void);
#endif
