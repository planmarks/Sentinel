#ifndef SHA256_H
#define SHA256_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
  uint32_t state[8];
  uint64_t bitlen;
  uint8_t  buf[64];
  size_t   idx;
} sha256_ctx;

void sha256_init(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const void *data, size_t len);
void sha256_final(sha256_ctx *c, uint8_t out[32]);
void sha256(const void *data, size_t len, uint8_t out[32]);

/* HMAC-SHA256 */
void hmac_sha256(const uint8_t *key, size_t keylen,
                 const uint8_t *msg, size_t msglen, uint8_t out[32]);

/* PBKDF2-HMAC-SHA256 */
void pbkdf2_sha256(const uint8_t *pw, size_t pwlen,
                   const uint8_t *salt, size_t saltlen,
                   uint32_t iters, uint8_t *out, size_t outlen);

/* Optional progress hook, called periodically during pbkdf2_sha256 so the UI
 * can animate. Pass NULL to disable. `done`/`total` are iteration counts. */
typedef void (*pbkdf2_progress_cb)(uint32_t done, uint32_t total);
void pbkdf2_set_progress(pbkdf2_progress_cb cb);

#endif
