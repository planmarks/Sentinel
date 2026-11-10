#ifndef UPDATE_H
#define UPDATE_H
#include <stdbool.h>
/* If STAGING holds a validly-signed image whose version is not older than the
 * installed one, install it to the app + manifest regions and wipe staging.
 * Returns true if an install was performed (caller should system-reset).
 * A present-but-invalid staging image is wiped and false is returned. */
bool bl_try_update(void);
#endif
