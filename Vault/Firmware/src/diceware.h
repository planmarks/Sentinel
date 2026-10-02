#ifndef DICEWARE_H
#define DICEWARE_H
#include <stddef.h>
/* Estonian Diceware passphrase generator.
 * Picks `nwords` words uniformly at random (TRNG) from the 2048-word Estonian
 * list -> 11 bits of entropy per word. Writes a lowercase, space-separated
 * phrase to `out`; returns its length (0 on failure). */
int diceware_generate(int nwords, char *out, size_t outsz);
const char *diceware_word(int index);   /* 0..2047 */
int diceware_find(const char *word);    /* exact match -> index, else -1 */
#endif
