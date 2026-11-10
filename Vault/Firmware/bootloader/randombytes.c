/* tweetnacl references randombytes() in its key-generation paths. The
 * bootloader only *verifies* signatures (crypto_sign_open), which never calls
 * it, but the symbol must resolve. Back it with the hardware TRNG anyway so it
 * is safe if ever reached. */
#include <stdint.h>
#include "rng.h"

void randombytes(uint8_t *p, unsigned long long n) {
  rng_fill(p, (size_t)n);
}
