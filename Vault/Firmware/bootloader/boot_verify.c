/* Secure-boot image verification: SHA-256 over (header-core || payload),
 * then Ed25519 verify of the 64-byte signature against the baked-in key.
 * Hashing streams straight from flash, so RAM use is a few hundred bytes. */
#include "boot_verify.h"
#include "sha256.h"
#include "tweetnacl.h"
#include <string.h>

bool fw_verify(const fw_header *h, uint32_t payload_base) {
  if (h->magic != FW_MAGIC) return false;
  if (h->load_addr != APP_BASE) return false;
  if (h->length == 0 || h->length > APP_MAX) return false;

  uint8_t digest[32];
  sha256_ctx c;
  sha256_init(&c);
  sha256_update(&c, (const uint8_t *)h, FW_HEADER_CORE);          /* 32-byte core */
  sha256_update(&c, (const uint8_t *)payload_base, h->length);    /* app payload, from flash */
  sha256_final(&c, digest);

  /* tweetnacl verifies an *attached* signed message: sm = sig(64) || msg(32). */
  uint8_t sm[64 + 32];
  uint8_t m[64 + 32];
  unsigned long long mlen = 0;
  memcpy(sm, h->sig, 64);
  memcpy(sm + 64, digest, 32);

  if (crypto_sign_ed25519_tweet_open(m, &mlen, sm, sizeof(sm), BOOT_PUBKEY) != 0)
    return false;
  if (mlen != 32 || memcmp(m, digest, 32) != 0) return false;
  return true;
}
