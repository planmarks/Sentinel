# Sentinel Vault — firmware for Sentinel **V1B** (STM32L442 + OPTIGA)

Offline password **generator + encrypted vault** by **@papathought** · **muru.global**

A self-contained, USB-free password manager for the Sentinel V1B board: a
0.96" SSD1306 OLED, six tactile buttons, an STM32L442KCU6 MCU (hardware TRNG +
AES) and an OPTIGA Trust M secure element. Keys are generated on-device from the
STM32 true RNG, the vault is AES-256 encrypted, and nothing ever leaves the box.

Built from the ESP32 `PIGEON_PW_GEN` reference, rewritten bare-metal for STM32.

---

## Flash it (STM32CubeProgrammer + ST-LINK V2)

Build output is in `build/`:

- **`build/muru_vault.hex`** ← flash this (carries its own load address, 0x08000000)
- `build/muru_vault.elf` / `.bin` — same image in other formats

**GUI:** open STM32CubeProgrammer → connect ST-LINK (SWD) → *Open file* →
`build/muru_vault.hex` → **Download**.

**CLI:**

```bash
STM32_Programmer_CLI -c port=SWD -d build/muru_vault.hex -rst
```

> The board exposes SWD on the Tag-Connect footprint (J2). SWD is **not** locked
> (RDP Level 0) so you can reflash freely. For a production unit you would raise
> RDP — see "Security notes" below.

### Rebuild from source

Needs the GNU Arm toolchain (bundled with STM32CubeIDE is fine — this was built
with `arm-none-eabi-gcc` 14.3.1) and `make`:

```bash
make            # -> build/muru_vault.hex + .elf + .bin
make clean
```

Footprint: **~44 KB flash / ~20 KB RAM** on a 256 KB / 64 KB part — lots of head-room.

---

## Hardware map (Sentinel V1B, from the KiCad netlist)

| Function | STM32L442 pin | Notes |
|---|---|---|
| OLED + OPTIGA I²C SCL | **PB6** (AF4) | shared bus |
| OLED + OPTIGA I²C SDA | **PB7** (AF4) | OLED @0x3C, OPTIGA @0x30 |
| Button **UP** | PA0 | active-LOW, internal pull-ups |
| Button **OK** | PA1 | also the wake source (EXTI1) |
| Button **DOWN** | PA2 | |
| Button **RIGHT** | PA3 | |
| Button **LEFT** | PA4 | |
| Button **BACK** | PA5 | |
| OPTIGA reset | PB0 | not driven by this firmware yet |

Clocks: HSI16 (16 MHz) core, HSI48 feeding the RNG. The display is initialised
**flipped 180°** (segment remap `0xA0` + COM `0xC0`) because it is soldered
upside-down on V1B.

---

## How to use it

**First boot** → *Welcome* → **Create new vault**:
1. The device generates a random master seed and shows a **12-word recovery
   phrase** — write it down. It is shown once.
2. Set a **passkey** (typed on the on-screen QWERTY keyboard, entered twice).

Thereafter, boot asks for the **passkey**. Enter it correctly → main menu.
**Three wrong passkeys wipes the device** (full erase, anti-theft).

### Main menu
1. **Vault** — browse saved entries; open one to view username / password
   (LEFT/RIGHT pages), or press OK to delete.
2. **New Password**
   - **Generate** — length + character-set options, device-generated password,
     then save with a name & username.
   - **Manual** — type a password yourself on the QWERTY keyboard.
3. **Settings**
   - **Shut Down** — enter low-power STOP mode. **Press OK to wake.**
   - **Wipe** — reset to factory state (see semantics below).
   - **Recover** — enter a 12-word phrase to restore a removed vault.
4. **About** — description & on-device guide.
5. **Help** — QR code + link to the support portal (muru.global/support).
6. **Credits** — @papathought · muru.global (with a website QR).

### Keyboard controls (typing screens)
- **UP / DOWN / LEFT / RIGHT** — move the key cursor
- **OK** — type the highlighted key (function row: `cap` `123` `spc` `del`)
- **BACK** — backspace
- **hold OK** — save / confirm · **hold BACK** — cancel

Typed text is rendered in a **large (2×) font** for readability.

### Idle & power
After **2 minutes** with no input the device auto-enters low-power STOP mode
and re-locks. **Press OK** to wake; you'll be asked for your passkey again.

---

## Security model

```
master key   = SHA-256("MuruVault-mk-v1" || entropy)         (from the 12 words)
entry crypto = AES-256-CBC(master key), fresh random IV per entry
passkey wrap = pkKey = PBKDF2-HMAC-SHA256(passkey, salt, 8000)
               verifier   = SHA-256(pkKey)            (checks the passkey)
               wrapped    = AES-256-ECB(master key) under pkKey
```

- The **master key is never stored in the clear**; the **passkey is never
  stored** (only a verifier hash).
- Randomness is the **STM32 hardware TRNG**.
- The wrong-passkey counter is written to flash **before** each check, so a
  power-cycle cannot cheat the 3-strike wipe.

### Wipe vs. Recover (deliberate design)
- **Settings → Wipe** removes the passkey + config but leaves the vault
  ciphertext in flash, cryptographically inert without the seed. The device
  returns to the *Welcome* screen — it looks factory-fresh.
- **Recover** (from *Welcome* or *Settings*) takes your 12 words, rebuilds the
  master key, and if the sealed vault is still present it **comes back**. This is
  what makes "recover a vault that was removed" real.
- **Three wrong passkeys** performs a **full destroy** (auth + config + vault) —
  an attacker gets nothing back, even with the seed absent.

### Honest limitations
- **RDP is Level 0** in this build (so you can develop/flash). A physical
  attacker with SWD can read flash — but the vault there is AES-256 sealed and
  the master key is only recoverable with the passkey. Raise to RDP L2 for a
  hardened unit.
- **The OPTIGA Trust M is not yet used.** This firmware runs entirely on the
  STM32 (its TRNG + software AES). The Trust M can't sign the wallet's coin
  curves, but for *this* app it is an ideal home for: the wrapped master key in
  a protected data object, the seed entropy generated by its certified TRNG, and
  the wrong-passkey counter in a hardware **monotonic counter** (true anti-
  rollback). Those are the natural next hardening step; the storage layer
  (`flash_store.c` / `vault.c`) is deliberately isolated so it can move behind
  the OPTIGA Shielded Connection without touching the UI.

---

## Estonian Diceware passphrase (experimental)

When setting a passkey (first setup, or Recover), you can choose
**"Estonian phrase"** instead of typing one. The device then:

1. lets you pick a **word count (4–8)** and shows the resulting entropy, then
2. picks that many words **uniformly at random from a 2048-word Estonian list**
   using the hardware TRNG (unbiased rejection sampling), and
3. shows the phrase to write down; **hold OK** to accept it.

The phrase (lowercase, space-separated) becomes your passkey and runs through
the **exact same PBKDF2 → key-wrap path** as a typed passkey — so at unlock you
just type it back on the QWERTY keyboard (all words are ASCII, lowercase).

**Where the strength comes from:** the entropy is in the *random selection*
(11 bits/word → 5 words = 55 bits, 6 = 66, 8 = 88), not the language. Estonian
is the bonus: generic English cracking dictionaries (`rockyou`, credential
dumps) don't cover it, so it also dodges commodity dictionary attacks. Against a
*targeted* adversary with Estonian corpora, the random entropy is what protects
you — pick 6+ words.

**The wordlist** is built from an Estonian frequency corpus (OpenSubtitles
2018), ASCII-folded (õ ä ö ü → o a o u), length 4–8, with unique 4-letter
prefixes, and **hand-pruned over several review passes** to remove personal
names (via a first-name dataset + manual review), surnames, place names, English
artifacts from the subtitle source, and profanity/slurs. The exact pipeline is
reproducible:

```bash
# inputs live in tools/first-names.txt and build/et_50k_freq.txt
python tools/curate_estonian.py <dir-with-inputs> .   # writes build/estonian_2048.txt
python tools/validate_wordlist.py build/estonian_2048.txt   # must print VALID
make
```

A handful of entries still coincide with names in other languages (e.g. `meri`,
`maja`, `tere` are real Estonian words that happen to be names elsewhere) —
those are kept because they're genuine vocabulary. Give the final list a human
read before production and prune to taste; `validate_wordlist.py` enforces the
structural rules on any replacement.

`estonian_words.h` is a drop-in table; nothing else in the firmware changes when
you replace it. If a future list keeps Estonian diacritics, add õ/ä/ö/ü to the
keyboard rows in `ui.c` so the passphrase stays typeable.

## Guard words (second factor on saved passwords)

At vault creation the device generates **3 random Estonian words** (from the same
2048-word list) and shows them to write down — a third secret alongside the seed
phrase and the passkey. They gate two things:

- **Opening the Vault (saved-passwords) page.** After unlocking with the passkey
  you can freely Generate/Manual-enter new passwords, but opening *Vault* runs a
  **recognition challenge**: three rounds, each showing one of your guard words
  plus two random decoys. Pick all three correctly, in order, and the page opens
  for the rest of the session (it re-challenges after lock/sleep). One wrong pick
  → "Access denied".
- **Recovery.** Before the seed-phrase step, you must **type the 3 guard words in
  order**. Only then can you enter the seed and restore the vault.

Stored in the vault header (which survives a Wipe, so recovery still works),
the guard-word indices are plaintext — because the device must know the answers
to render the multiple-choice challenge. So this is a **usability second factor,
not a cryptographic layer**: it stops casual/shoulder access to saved passwords,
but a flash dump would reveal the words. The real confidentiality still rests on
the passkey + seed (and, on the hardened firmware, RDP L2 + the OPTIGA). A guard
challenge is 1-in-27 to blind-guess, so treat it as a convenience gate.