#include "bip39.h"
#include "bip39_words.h"
#include "crypto/sha256.h"
#include <string.h>

const char *bip39_word(int index) {
  if (index < 0 || index >= 2048) return "";
  return BIP39_WORDS[index];
}

int bip39_find(const char *word) {
  for (int i = 0; i < 2048; i++)
    if (strcmp(word, BIP39_WORDS[i]) == 0) return i;
  return -1;
}

int bip39_prefix_first(const char *prefix) {
  size_t n = strlen(prefix);
  if (n == 0) return -1;
  for (int i = 0; i < 2048; i++)
    if (strncmp(prefix, BIP39_WORDS[i], n) == 0) return i;
  return -1;
}

size_t bip39_from_entropy(const uint8_t entropy[16], char *out, size_t outsz) {
  uint8_t hash[32];
  sha256(entropy, 16, hash);
  /* 128 bits entropy + 4 bits checksum (top 4 bits of hash[0]) = 132 bits. */
  uint8_t bits[17];
  memcpy(bits, entropy, 16);
  bits[16] = hash[0] & 0xF0;   /* only top 4 bits used */

  size_t pos = 0;
  out[0] = '\0';
  for (int w = 0; w < 12; w++) {
    int idx = 0;
    for (int b = 0; b < 11; b++) {
      int bit = w * 11 + b;
      int byte = bit >> 3;
      int off  = 7 - (bit & 7);
      idx = (idx << 1) | ((bits[byte] >> off) & 1);
    }
    const char *word = BIP39_WORDS[idx];
    size_t wl = strlen(word);
    if (pos + wl + 2 >= outsz) break;
    if (w) out[pos++] = ' ';
    memcpy(out + pos, word, wl); pos += wl;
    out[pos] = '\0';
  }
  return pos;
}

bool bip39_to_entropy(const char *phrase, uint8_t entropy[16]) {
  int idxs[12], count = 0;
  char word[16];
  const char *p = phrase;
  while (*p && count < 12) {
    while (*p == ' ') p++;
    size_t k = 0;
    while (*p && *p != ' ' && k < sizeof(word)-1) word[k++] = *p++;
    word[k] = '\0';
    if (k == 0) break;
    int idx = bip39_find(word);
    if (idx < 0) return false;
    idxs[count++] = idx;
  }
  while (*p == ' ') p++;
  if (count != 12 || *p != '\0') return false;

  uint8_t bits[17];
  memset(bits, 0, sizeof(bits));
  for (int w = 0; w < 12; w++) {
    for (int b = 0; b < 11; b++) {
      int bit = w * 11 + b;
      int byte = bit >> 3;
      int off  = 7 - (bit & 7);
      int v = (idxs[w] >> (10 - b)) & 1;
      bits[byte] |= (uint8_t)(v << off);
    }
  }
  uint8_t hash[32];
  sha256(bits, 16, hash);
  if ((bits[16] & 0xF0) != (hash[0] & 0xF0)) return false;
  memcpy(entropy, bits, 16);
  return true;
}

bool bip39_validate(const char *phrase) {
  uint8_t e[16];
  return bip39_to_entropy(phrase, e);
}
