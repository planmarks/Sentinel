/* Muru Vault — signed firmware image format + flash map (shared by the
 * bootloader, the app and the host signing tool).
 *
 * Flash layout (STM32L442, 256 KB):
 *   0x08000000  BOOTLOADER   30 KB  (code)
 *   0x08007800  MANIFEST      2 KB  (fw_header of the installed app)
 *   0x08008000  APP          96 KB  (active application)
 *   0x08020000  STAGING      96 KB  (downloaded image, Phase 2 USB)
 *   0x0803E000  STORAGE       8 KB  (vault/auth/config — unchanged)
 *
 * Trust model: the bootloader hashes (header-core || app-payload) with SHA-256
 * and Ed25519-verifies `sig` against a public key baked into the bootloader.
 * The private key never leaves the host. Verifying over the 32-byte digest
 * keeps RAM tiny (no need to hold the whole image).
 */
#ifndef FWIMAGE_H
#define FWIMAGE_H
#include <stdint.h>

#define BL_BASE        0x08000000u
#define MANIFEST_ADDR  0x08007800u
#define APP_BASE       0x08008000u
#define APP_MAX        (96u * 1024u)
#define STAGING_BASE   0x08020000u
#define STAGING_MAX    (96u * 1024u)

#define FW_MAGIC       0x5746564Du    /* 'M','V','F','W' */

typedef struct {
  uint32_t magic;        /* FW_MAGIC */
  uint32_t version;      /* monotonic; used for anti-rollback (Phase 2) */
  uint32_t length;       /* payload size in bytes, placed at load_addr */
  uint32_t load_addr;    /* == APP_BASE */
  uint8_t  reserved[16];
  /* --- first 32 bytes above = "core"; the signature covers SHA-256(core||payload) --- */
  uint8_t  sig[64];      /* Ed25519 signature */
} fw_header;             /* 96 bytes */

#define FW_HEADER_CORE  32   /* bytes of header covered by the hash */

#endif
