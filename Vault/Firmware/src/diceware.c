#include "diceware.h"
#include "estonian_words.h"
#include "rng.h"
#include <string.h>

const char *diceware_word(int index) {
  if (index < 0 || index >= 2048) return "";
  return ET_WORDS[index];
}

int diceware_find(const char *word) {
  for (int i = 0; i < 2048; i++)
    if (strcmp(word, ET_WORDS[i]) == 0) return i;
  return -1;
}

int diceware_generate(int nwords, char *out, size_t outsz) {
  if (nwords < 1 || outsz == 0) { if (outsz) out[0] = 0; return 0; }
  size_t pos = 0; out[0] = 0;
  for (int i = 0; i < nwords; i++) {
    uint32_t idx = rng_below(2048);          /* uniform, unbiased */
    const char *w = ET_WORDS[idx];
    size_t wl = strlen(w);
    if (pos + wl + 2 >= outsz) break;
    if (i) out[pos++] = ' ';
    memcpy(out + pos, w, wl); pos += wl; out[pos] = 0;
  }
  return (int)pos;
}
