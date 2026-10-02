#ifndef VAULT_H
#define VAULT_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define LBL_LEN 24
#define USR_LEN 32
#define PWD_LEN 64

typedef struct {
  char label[LBL_LEN];
  char user[USR_LEN];
  char pass[PWD_LEN];
  char pad[8];
} Entry;                 /* 128 bytes (AES block multiple) */

#define VAULT_MAX_ENTRIES 28

typedef struct {
  uint8_t length;        /* generated password length */
  uint8_t lower, upper, digits, symbols;
} GenCfg;

/* ---- lifecycle ---- */
bool vault_auth_exists(void);   /* an AUTH blob is present (device is set up) */
bool vault_has_data(void);      /* a VAULT with a header is present in flash */

/* First run / new identity: makes fresh 128-bit entropy, derives the master key,
 * writes an empty vault, and returns the 12-word phrase (>=128 byte buffer). */
bool vault_new_identity(char *phrase_out, size_t outsz);

/* Recovery: rebuild the master key from a phrase. If a vault with matching
 * master key is already in flash it is kept; otherwise an empty one is written.
 * Returns false if the phrase is invalid. Leaves the master key loaded. */
bool vault_recover(const char *phrase);

/* Set / replace the passkey that gates the loaded master key.
 * kind records how it was entered: 0 = typed passkey/PIN, 1 = Estonian phrase. */
bool vault_set_passkey(const char *passkey, int kind);
int  vault_passkey_kind(void);   /* 0 passkey, 1 phrase (0 if not set) */

/* Verify a passkey and, on success, unwrap the master key into RAM. */
bool vault_unlock(const char *passkey);
void vault_lock(void);          /* wipe the master key from RAM */

/* Wrong-passkey counter, persisted in flash so a power-cycle can't reset it. */
int  vault_attempts(void);
void vault_set_attempts(int n);

/* ---- guard words: 3-word second-factor gate on the saved-passwords page ---- */
bool vault_guard_set(void);              /* true once guard words are configured */
void vault_get_guard(int out_idx[3]);    /* the 3 correct indices (no master key needed) */
bool vault_set_guard(const int idx[3]);  /* configure at creation (vault must exist) */

/* Full destroy (auth + config + vault) — factory/anti-theft. */
void vault_destroy_all(void);
/* Soft reset: remove passkey + config, keep the encrypted vault for Recover. */
void vault_soft_reset(void);

/* ---- entries (require an unlocked master key) ---- */
int  vault_count(void);
bool vault_get(int idx, Entry *out);
bool vault_add(const Entry *e);
bool vault_delete(int idx);

/* ---- generator config ---- */
void gencfg_load(GenCfg *c);
void gencfg_save(const GenCfg *c);

/* Generate a password into out (>= maxlen). false if no charset enabled. */
bool vault_generate(const GenCfg *c, char *out, int maxlen);

#endif
