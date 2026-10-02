#ifndef AES_H
#define AES_H
#include <stdint.h>
#include <stddef.h>

/* AES-256 only (32-byte keys). Block size 16 bytes. */
typedef struct { uint8_t rk[240]; } aes256_ctx;

void aes256_init(aes256_ctx *c, const uint8_t key[32]);
void aes256_encrypt_block(const aes256_ctx *c, const uint8_t in[16], uint8_t out[16]);
void aes256_decrypt_block(const aes256_ctx *c, const uint8_t in[16], uint8_t out[16]);

/* CBC over whole blocks (len must be a multiple of 16). iv is not modified. */
void aes256_cbc_encrypt(const aes256_ctx *c, const uint8_t iv[16],
                        const uint8_t *in, uint8_t *out, size_t len);
void aes256_cbc_decrypt(const aes256_ctx *c, const uint8_t iv[16],
                        const uint8_t *in, uint8_t *out, size_t len);

#endif
