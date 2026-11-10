#ifndef BOOT_VERIFY_H
#define BOOT_VERIFY_H
#include "fwimage.h"
#include <stdbool.h>

/* The 32-byte Ed25519 public key baked into the bootloader (boot_pubkey.c). */
extern const uint8_t BOOT_PUBKEY[32];

/* Verify the image described by `h` whose payload lives at `payload_base`.
 * Returns true only if magic/length/load_addr are sane AND the Ed25519
 * signature over SHA-256(core || payload) checks out against BOOT_PUBKEY. */
bool fw_verify(const fw_header *h, uint32_t payload_base);

#endif
