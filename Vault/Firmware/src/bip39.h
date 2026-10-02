#ifndef BIP39_H
#define BIP39_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 12-word English BIP39 mnemonics over 128-bit entropy. */
#define BIP39_ENTROPY_LEN 16
#define BIP39_WORDS_N     12

/* Build a space-separated 12-word phrase from 16 bytes of entropy.
 * `out` must hold at least 12*9+1 bytes. Returns strlen written. */
size_t bip39_from_entropy(const uint8_t entropy[16], char *out, size_t outsz);

/* Validate a phrase (word membership + checksum). */
bool bip39_validate(const char *phrase);

/* Decode a valid phrase back to 16 bytes of entropy. false if invalid. */
bool bip39_to_entropy(const char *phrase, uint8_t entropy[16]);

/* Wordlist helpers for the recovery keyboard's autocomplete. */
const char *bip39_word(int index);            /* 0..2047 */
int  bip39_find(const char *word);            /* exact match -> index, else -1 */
int  bip39_prefix_first(const char *prefix);  /* first index matching prefix, or -1 */

#endif
