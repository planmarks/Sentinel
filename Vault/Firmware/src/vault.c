/* Vault: passkey-gated, seed-recoverable encrypted password store.
 *
 * Master key    = SHA256("MuruVault-mk-v1" || entropy[16])   (from the 12 words)
 * Entry crypto  = AES-256-CBC(masterKey) with a fresh random IV per entry
 * Passkey wrap  = pkKey = PBKDF2-HMAC-SHA256(passkey, salt, ITERS)
 *                 verifier   = SHA256(pkKey)                  (checks the passkey)
 *                 wrappedKey = AES-256-ECB(masterKey) under pkKey
 * The master key is never stored in the clear; the passkey is never stored.
 */
#include "vault.h"
#include "flash_store.h"
#include "rng.h"
#include "crypto/sha256.h"
#include "crypto/aes.h"
#include <string.h>

#define AUTH_MAGIC  0x4D564131u   /* 'MVA1' */
#define VAULT_MAGIC 0x4D565431u   /* 'MVT1' */
#define CFG_MAGIC   0x4D564331u   /* 'MVC1' */
#define PBKDF2_ITERS 8000u

#define IV_LEN 16
#define ENTRY_PLAIN ((int)sizeof(Entry))     /* 128 */
#define ENTRY_BLOB  (IV_LEN + ENTRY_PLAIN)   /* 144 */

typedef struct {
  uint32_t magic;
  uint8_t  salt[16];
  uint8_t  verifier[32];
  uint8_t  wrapped[32];
  uint8_t  rsvd[12];
} AuthBlob;                                   /* 96 */

typedef struct {
  uint32_t magic;
  uint32_t count;
  uint8_t  mkVerifier[32];
  uint16_t guard_idx[3];     /* 3 guard-word indices (second-factor gate) */
  uint8_t  guard_set;        /* 1 once guard words are configured */
  uint8_t  rsvd[9];
} VaultHdr;                                   /* 56 */

typedef struct {
  uint32_t magic;
  uint8_t  length, lower, upper, digits, symbols;
  uint8_t  rsvd[7];
} CfgBlob;                                    /* 16 */

#define VHDR_SIZE ((int)sizeof(VaultHdr))     /* 48 */

static uint8_t g_master[32];
static bool    g_unlocked = false;

static const char *CS_LOWER  = "abcdefghijklmnopqrstuvwxyz";
static const char *CS_UPPER  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char *CS_DIGITS = "0123456789";
static const char *CS_SYM    = "!@#$%^&*()-_=+[]{};:,.?/";

static void memzero(void *p, size_t n) { volatile uint8_t *v = (volatile uint8_t*)p; while (n--) *v++ = 0; }

static void derive_master(const uint8_t entropy[16], uint8_t out[32]) {
  sha256_ctx c; sha256_init(&c);
  sha256_update(&c, "MuruVault-mk-v1", 15);
  sha256_update(&c, entropy, 16);
  sha256_final(&c, out);
}
static void master_verifier(const uint8_t master[32], uint8_t out[32]) {
  sha256_ctx c; sha256_init(&c);
  sha256_update(&c, master, 32);
  sha256_update(&c, "mk-verify", 9);
  sha256_final(&c, out);
}

/* ---- header / auth access ---- */
static const VaultHdr *vhdr(void) { return (const VaultHdr *)store_ptr(REGION_VAULT); }
static const AuthBlob *ablob(void){ return (const AuthBlob *)store_ptr(REGION_AUTH); }

bool vault_auth_exists(void) { return ablob()->magic == AUTH_MAGIC; }
bool vault_has_data(void)    { return vhdr()->magic == VAULT_MAGIC; }

/* Write a fresh empty vault header carrying this master key's verifier. */
static bool write_empty_vault(void) {
  uint8_t buf[VHDR_SIZE];
  VaultHdr *h = (VaultHdr *)buf;
  memset(buf, 0, sizeof(buf));
  h->magic = VAULT_MAGIC;
  h->count = 0;
  master_verifier(g_master, h->mkVerifier);
  return store_write(REGION_VAULT, buf, sizeof(buf));
}

bool vault_new_identity(char *phrase_out, size_t outsz) {
  uint8_t entropy[16];
  rng_fill(entropy, 16);
  derive_master(entropy, g_master);
  g_unlocked = true;

  extern size_t bip39_from_entropy(const uint8_t*, char*, size_t);
  bip39_from_entropy(entropy, phrase_out, outsz);
  memzero(entropy, sizeof(entropy));
  return write_empty_vault();
}

bool vault_recover(const char *phrase) {
  extern bool bip39_to_entropy(const char*, uint8_t*);
  uint8_t entropy[16];
  if (!bip39_to_entropy(phrase, entropy)) return false;
  derive_master(entropy, g_master);
  g_unlocked = true;
  memzero(entropy, sizeof(entropy));

  /* Keep an existing vault only if it belongs to this master key. */
  if (vault_has_data()) {
    uint8_t v[32]; master_verifier(g_master, v);
    if (memcmp(v, vhdr()->mkVerifier, 32) == 0) return true;
  }
  return write_empty_vault();
}

bool vault_set_passkey(const char *passkey, int kind) {
  if (!g_unlocked) return false;
  AuthBlob a; memset(&a, 0, sizeof(a));
  a.magic = AUTH_MAGIC;
  a.rsvd[1] = (uint8_t)kind;        /* rsvd[0] = attempts, rsvd[1] = kind */
  rng_fill(a.salt, 16);

  uint8_t pk[32];
  pbkdf2_sha256((const uint8_t*)passkey, strlen(passkey), a.salt, 16, PBKDF2_ITERS, pk, 32);
  sha256(pk, 32, a.verifier);

  aes256_ctx ctx; aes256_init(&ctx, pk);
  aes256_encrypt_block(&ctx, g_master,      a.wrapped);
  aes256_encrypt_block(&ctx, g_master + 16, a.wrapped + 16);
  memzero(pk, sizeof(pk)); memzero(&ctx, sizeof(ctx));

  bool ok = store_write(REGION_AUTH, &a, sizeof(a));
  memzero(&a, sizeof(a));
  return ok;
}

bool vault_unlock(const char *passkey) {
  if (!vault_auth_exists()) return false;
  const AuthBlob *a = ablob();
  uint8_t pk[32];
  pbkdf2_sha256((const uint8_t*)passkey, strlen(passkey), a->salt, 16, PBKDF2_ITERS, pk, 32);
  uint8_t v[32]; sha256(pk, 32, v);
  if (memcmp(v, a->verifier, 32) != 0) { memzero(pk, sizeof(pk)); return false; }

  aes256_ctx ctx; aes256_init(&ctx, pk);
  aes256_decrypt_block(&ctx, a->wrapped,      g_master);
  aes256_decrypt_block(&ctx, a->wrapped + 16, g_master + 16);
  memzero(pk, sizeof(pk)); memzero(&ctx, sizeof(ctx));
  g_unlocked = true;
  return true;
}

void vault_lock(void) { memzero(g_master, sizeof(g_master)); g_unlocked = false; }

int vault_attempts(void) {
  if (!vault_auth_exists()) return 0;
  return ablob()->rsvd[0];
}
int vault_passkey_kind(void) {
  if (!vault_auth_exists()) return 0;
  return ablob()->rsvd[1];
}
void vault_set_attempts(int n) {
  if (!vault_auth_exists()) return;
  AuthBlob a;
  memcpy(&a, ablob(), sizeof(a));   /* preserve salt/verifier/wrapped */
  a.rsvd[0] = (uint8_t)n;
  store_write(REGION_AUTH, &a, sizeof(a));
  memzero(&a, sizeof(a));
}

void vault_destroy_all(void) {
  store_erase(REGION_AUTH);
  store_erase(REGION_CFG);
  store_erase(REGION_VAULT);
  vault_lock();
}
void vault_soft_reset(void) {
  store_erase(REGION_AUTH);
  store_erase(REGION_CFG);
  vault_lock();
}

/* ---- entries ---- */
int vault_count(void) {
  if (!vault_has_data()) return 0;
  uint32_t n = vhdr()->count;
  return (n > VAULT_MAX_ENTRIES) ? VAULT_MAX_ENTRIES : (int)n;
}

static const uint8_t *entry_blob_ptr(int idx) {
  return store_ptr(REGION_VAULT) + VHDR_SIZE + (size_t)idx * ENTRY_BLOB;
}

bool vault_get(int idx, Entry *out) {
  if (!g_unlocked || idx < 0 || idx >= vault_count()) return false;
  const uint8_t *blob = entry_blob_ptr(idx);
  aes256_ctx ctx; aes256_init(&ctx, g_master);
  aes256_cbc_decrypt(&ctx, blob, blob + IV_LEN, (uint8_t*)out, ENTRY_PLAIN);
  memzero(&ctx, sizeof(ctx));
  return true;
}

/* Rebuild the whole vault region from a RAM image with `n` blobs. */
static bool vault_write_region(const uint8_t *blobs, int n) {
  static uint8_t buf[VHDR_SIZE + VAULT_MAX_ENTRIES * ENTRY_BLOB];
  VaultHdr *h = (VaultHdr *)buf;
  memset(buf, 0, sizeof(buf));
  h->magic = VAULT_MAGIC;
  h->count = (uint32_t)n;
  master_verifier(g_master, h->mkVerifier);
  const VaultHdr *cur = vhdr();          /* preserve guard words across rewrites */
  if (cur->magic == VAULT_MAGIC && cur->guard_set) {
    h->guard_set = 1;
    h->guard_idx[0] = cur->guard_idx[0];
    h->guard_idx[1] = cur->guard_idx[1];
    h->guard_idx[2] = cur->guard_idx[2];
  }
  memcpy(buf + VHDR_SIZE, blobs, (size_t)n * ENTRY_BLOB);
  return store_write(REGION_VAULT, buf, VHDR_SIZE + (size_t)n * ENTRY_BLOB);
}

/* ---- guard words (second-factor gate) ---- */
bool vault_guard_set(void) { return vault_has_data() && vhdr()->guard_set; }

void vault_get_guard(int out_idx[3]) {    /* plaintext; works without the master key */
  const VaultHdr *h = vhdr();
  out_idx[0] = h->guard_idx[0];
  out_idx[1] = h->guard_idx[1];
  out_idx[2] = h->guard_idx[2];
}

bool vault_set_guard(const int idx[3]) {
  if (!g_unlocked || !vault_has_data()) return false;
  int n = vault_count();
  static uint8_t buf[VHDR_SIZE + VAULT_MAX_ENTRIES * ENTRY_BLOB];
  memset(buf, 0, sizeof(buf));
  VaultHdr *h = (VaultHdr *)buf;
  h->magic = VAULT_MAGIC;
  h->count = (uint32_t)n;
  master_verifier(g_master, h->mkVerifier);
  h->guard_idx[0] = (uint16_t)idx[0];
  h->guard_idx[1] = (uint16_t)idx[1];
  h->guard_idx[2] = (uint16_t)idx[2];
  h->guard_set = 1;
  memcpy(buf + VHDR_SIZE, store_ptr(REGION_VAULT) + VHDR_SIZE, (size_t)n * ENTRY_BLOB);
  bool ok = store_write(REGION_VAULT, buf, VHDR_SIZE + (size_t)n * ENTRY_BLOB);
  memzero(buf, sizeof(buf));
  return ok;
}

bool vault_add(const Entry *e) {
  if (!g_unlocked) return false;
  int n = vault_count();
  if (n >= VAULT_MAX_ENTRIES) return false;

  static uint8_t blobs[VAULT_MAX_ENTRIES * ENTRY_BLOB];
  memcpy(blobs, store_ptr(REGION_VAULT) + VHDR_SIZE, (size_t)n * ENTRY_BLOB);

  uint8_t *nb = blobs + (size_t)n * ENTRY_BLOB;
  rng_fill(nb, IV_LEN);                       /* IV */
  aes256_ctx ctx; aes256_init(&ctx, g_master);
  aes256_cbc_encrypt(&ctx, nb, (const uint8_t*)e, nb + IV_LEN, ENTRY_PLAIN);
  memzero(&ctx, sizeof(ctx));

  bool ok = vault_write_region(blobs, n + 1);
  memzero(blobs, sizeof(blobs));
  return ok;
}

bool vault_delete(int idx) {
  if (!g_unlocked) return false;
  int n = vault_count();
  if (idx < 0 || idx >= n) return false;

  static uint8_t blobs[VAULT_MAX_ENTRIES * ENTRY_BLOB];
  const uint8_t *src = store_ptr(REGION_VAULT) + VHDR_SIZE;
  int w = 0;
  for (int i = 0; i < n; i++) {
    if (i == idx) continue;
    memcpy(blobs + (size_t)w * ENTRY_BLOB, src + (size_t)i * ENTRY_BLOB, ENTRY_BLOB);
    w++;
  }
  bool ok = vault_write_region(blobs, w);
  memzero(blobs, sizeof(blobs));
  return ok;
}

/* ---- config ---- */
void gencfg_load(GenCfg *c) {
  const CfgBlob *b = (const CfgBlob *)store_ptr(REGION_CFG);
  if (b->magic == CFG_MAGIC) {
    c->length  = b->length; c->lower = b->lower; c->upper = b->upper;
    c->digits  = b->digits; c->symbols = b->symbols;
  } else {
    c->length = 20; c->lower = 1; c->upper = 1; c->digits = 1; c->symbols = 1;
  }
  if (c->length < 8) c->length = 8;
  if (c->length > PWD_LEN - 1) c->length = PWD_LEN - 1;
}
void gencfg_save(const GenCfg *c) {
  CfgBlob b; memset(&b, 0, sizeof(b));
  b.magic = CFG_MAGIC;
  b.length = c->length; b.lower = c->lower; b.upper = c->upper;
  b.digits = c->digits; b.symbols = c->symbols;
  store_write(REGION_CFG, &b, sizeof(b));
}

bool vault_generate(const GenCfg *c, char *out, int maxlen) {
  char pool[128]; int pn = 0;
  if (c->lower)   { int l=strlen(CS_LOWER);  memcpy(pool+pn, CS_LOWER,  l); pn+=l; }
  if (c->upper)   { int l=strlen(CS_UPPER);  memcpy(pool+pn, CS_UPPER,  l); pn+=l; }
  if (c->digits)  { int l=strlen(CS_DIGITS); memcpy(pool+pn, CS_DIGITS, l); pn+=l; }
  if (c->symbols) { int l=strlen(CS_SYM);    memcpy(pool+pn, CS_SYM,    l); pn+=l; }
  if (pn == 0) return false;
  int len = c->length; if (len > maxlen - 1) len = maxlen - 1;
  for (int i = 0; i < len; i++) out[i] = pool[rng_below((uint32_t)pn)];
  out[len] = '\0';
  return true;
}
