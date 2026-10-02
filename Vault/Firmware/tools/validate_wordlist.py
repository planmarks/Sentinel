#!/usr/bin/env python3
"""Validate a Diceware/BIP39-style wordlist for the Muru Vault firmware.

Usage:  python tools/validate_wordlist.py path/to/words.txt
Rules checked (same ones the firmware relies on):
  * exactly 2048 words            (2^11 -> 11 bits of entropy per word)
  * all ASCII, all alphabetic     (typeable on the on-screen QWERTY)
  * length 4..8
  * unique first-4-letters prefix (typo tolerance / autocomplete)
  * no duplicates
  * sorted ascending              (allows binary search; nice-to-have)
Exits non-zero and prints the first problems found if invalid.
"""
import sys

def main(path):
    words = [w.strip() for w in open(path, encoding="utf-8") if w.strip()]
    problems = []
    if len(words) != 2048:
        problems.append(f"count is {len(words)}, must be exactly 2048")
    bad_char = [w for w in words if not (w.isascii() and w.isalpha())]
    if bad_char:
        problems.append(f"{len(bad_char)} non-ASCII/non-alpha words, e.g. {bad_char[:5]}")
    bad_len = [w for w in words if not (4 <= len(w) <= 8)]
    if bad_len:
        problems.append(f"{len(bad_len)} words outside length 4..8, e.g. {bad_len[:5]}")
    dupes = sorted({w for w in words if words.count(w) > 1})
    if dupes:
        problems.append(f"{len(dupes)} duplicate words, e.g. {dupes[:5]}")
    seen = {}
    pfx_clash = []
    for w in words:
        p = w[:4]
        if p in seen and seen[p] != w:
            pfx_clash.append((seen[p], w))
        seen.setdefault(p, w)
    if pfx_clash:
        problems.append(f"{len(pfx_clash)} 4-letter-prefix clashes, e.g. {pfx_clash[:5]}")
    if words != sorted(words):
        problems.append("list is not sorted ascending")

    if problems:
        print("INVALID:")
        for p in problems:
            print("  -", p)
        return 1
    print(f"VALID: {len(words)} words, unique 4-prefixes, ASCII, len 4-8, sorted.")
    print(f"Entropy: 11 bits/word -> 5 words = 55 bits, 6 = 66, 8 = 88 bits.")
    return 0

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__); sys.exit(2)
    sys.exit(main(sys.argv[1]))
