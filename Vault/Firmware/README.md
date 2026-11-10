# Sentinel Vault — firmware for Sentinel **V1B** (STM32L442 + OPTIGA)

Offline password **generator + encrypted vault** by **@papathought** · **muru.global**

A self-contained, offline password manager for the Sentinel V1B board: a
0.96" SSD1306 OLED, six tactile buttons, an STM32L442KCU6 MCU (hardware TRNG +
AES) and an OPTIGA Trust M secure element. Keys are generated on-device from the
STM32 true RNG, the vault is AES-256 encrypted, and nothing ever leaves the box.
The USB-C port carries **only signed firmware updates** — never vault data.

Built from the ESP32 `PIGEON_PW_GEN` reference, rewritten bare-metal for STM32.

The device boots through a **secure bootloader** that only runs an
Ed25519-signed application (see [Secure boot](#secure-boot--signed-firmware-phase-1)).
So the thing you flash to a blank board is the **signed bundle**
`build/muru_signed.hex` (bootloader + manifest + signed app), *not* the bare app.

---

## Flash a blank board (STM32CubeProgrammer + ST-LINK V2)

```bash
python tools/fw_sign.py keygen        # ONCE per project: makes the signing key + boot_pubkey.c
./build_signed.sh 1                   # build app, sign v1, build bootloader, merge
STM32_Programmer_CLI -c port=SWD -d build/muru_signed.hex -rst
```

**GUI alternative:** STM32CubeProgrammer → connect ST-LINK (SWD) → open
`build/muru_signed.hex` → **Download**. SWD is on the Tag-Connect footprint (J2).

> On this dev board SWD is open (RDP Level 0) so you can reflash freely. The
> **production** unit raises RDP to Level 2 (locks SWD + the ROM bootloader) —
> at which point the signed USB-C updater below is the *only* way to update, and
> the signing key is the root of trust.

Later firmware goes on over **USB-C** (preferred, no debugger) or SWD — see
[USB-CDC update transport](#usb-cdc-update-transport-phase-2b).

### Rebuild from source

Needs the GNU Arm toolchain (bundled with STM32CubeIDE is fine — built with
`arm-none-eabi-gcc` 14.3.1), `make`, and Python with the `cryptography` package
(for signing; `pyserial` too for USB sending).

```bash
./build_signed.sh <version>   # the normal build: app + signature + bootloader + merged hex
make                          # app only (dev/size checks); won't boot unsigned under the bootloader
make -C bootloader            # bootloader only
make clean
```

Footprint on the 256 KB / 64 KB part: **bootloader** ~10 KB (of a 30 KB region),
**app** ~72 KB (of 96 KB), **RAM** ~24 KB (of 64 KB).

---

## Secure boot & signed firmware (Phase 1)

The device now boots through a **secure bootloader** that refuses to run any
application whose **Ed25519 signature** doesn't verify against a public key baked
into the bootloader. The private signing key never leaves the host.

**Flash layout** (see [src/fwimage.h](src/fwimage.h)):

| Region | Address | Size | Contents |
|---|---|---|---|
| Bootloader | `0x08000000` | 30 KB | verify-then-jump, OLED status |
| Manifest | `0x08007800` | 2 KB | `fw_header` (version, length, 64-byte sig) |
| Application | `0x08008000` | 96 KB | the vault app (relocated here) |
| Staging | `0x08020000` | 96 KB | reserved for USB updates (Phase 2) |
| Storage | `0x0803E000` | 8 KB | vault/auth/config (unchanged) |

On boot the bootloader streams **SHA-256 over (manifest-core ‖ app)**, then
Ed25519-verifies the manifest signature over that digest (tiny RAM — the image
isn't held in memory). Valid → jump to the app; invalid/absent → it stops and
shows a status screen (Phase 2 will enter USB update mode here).

### Build & flash a signed image

```bash
python tools/fw_sign.py keygen     # once: makes tools/fw_signing_key.pem + boot_pubkey.c
./build_signed.sh 1                # build app, sign as v1, build BL, merge
# -> build/muru_signed.hex  (bootloader + manifest + app, one file)
STM32_Programmer_CLI -c port=SWD -d build/muru_signed.hex -rst
```

> ⚠️ **`tools/fw_signing_key.pem` is the root of trust — keep it secret and backed
> up.** Anyone with it can sign firmware your devices will accept; losing it means
> no future updates for devices carrying this bootloader's public key. It is
> `.gitignore`d. (Plain `make` still builds just the app for SWD development, but
> the bootloader won't boot it until it's signed and the manifest is flashed.)

### Field updates — the install engine (Phase 2, done)

The bootloader now also **installs** a new image staged at `0x08020000`. On
every boot it checks STAGING for a `[fw_header][payload]` blob and, if present:

1. verifies its Ed25519 signature (same check as boot),
2. enforces **anti-rollback** — refuses any `version` older than the installed
   one (equal is allowed, so you can re-flash),
3. programs the payload → APP and the header → MANIFEST, re-verifies, then
4. wipes STAGING and resets.

The order (STAGING wiped **last**) is power-fail safe: an interrupted install
simply retries on the next boot. See [bootloader/update.c](bootloader/update.c).

`build_signed.sh` emits **`build/muru_update.bin`** (= manifest ‖ payload). To
exercise the installer today over SWD:

```bash
./build_signed.sh 2                                   # sign app as version 2
STM32_Programmer_CLI -c port=SWD -d build/muru_update.bin 0x08020000 -rst
# the bootloader verifies it, installs, and boots the new app
```

The install/anti-rollback/corrupt-reject logic is covered by a host simulation
(upgrade installs, older version rejected, bad signature discarded).

### USB-CDC update transport (Phase 2b)

The device can now receive an update over **USB-C** instead of SWD. On-device:
**Settings → Firmware Update → OK**; it enumerates as a **USB-CDC virtual COM
port** ([src/usbfs.c](src/usbfs.c)) and streams the blob into STAGING
([src/fwupdate.c](src/fwupdate.c)), showing a progress bar, then resets so the
bootloader verifies + installs it. From the host:

```bash
pip install pyserial                       # one-time
./build_signed.sh 4                        # sign a newer version than installed
python tools/fw_send.py COM5               # Windows; or /dev/ttyACM0 on Linux
```

Protocol: host sends `"MVUP"`+`uint32` length, device replies `RDY`, host streams
the blob, device replies `DONE` and reboots into the installer. Anti-rollback
still applies (send a version **newer** than what's installed).

The USB device stack is a compact **polled** implementation of the STM32L4 USB
FS peripheral (control EP0 enumeration + a bulk pair for CDC).

**Hardware-verified (2026-10-04):** the full path was exercised on a real
Sentinel V1B — the host enumerated the CDC port, streamed a v7 image over USB-C
in ~1.8 s, and the bootloader verified + anti-rollback-checked + installed it;
the manifest advanced v6 → v7 and the device booted the new firmware. The entire
Option C chain (secure boot + signed install + USB-C transport) now works end to
end on silicon.

### How this ties into RDP-2 / OPTIGA (production)

On the hardened unit (RDP Level 2) the STM32 ROM bootloader and SWD are locked,
so this **self-authored, signature-verified bootloader is the only update path**
— and it still works, because it programs flash as your own code.

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
2. It shows **3 guard words** — a second factor for the saved-passwords page
   (see [Guard words](#guard-words-second-factor-on-saved-passwords)).
3. Set a **passkey** — either typed on the QWERTY keyboard, or a generated
   **Estonian passphrase** (see
   [Estonian Diceware](#estonian-diceware-passphrase-experimental)).

Thereafter, boot asks for the **passkey/passphrase**. Enter it correctly → main
menu. **Three wrong tries wipes the device** (full erase, anti-theft).

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
   - **Firmware Update** — install a signed image over USB-C (see
     [USB-CDC update transport](#usb-cdc-update-transport-phase-2b)).
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
- **Firmware authenticity:** the bootloader Ed25519-verifies the app on every
  boot, and field updates (USB-C or SWD) are signature- and anti-rollback-checked
  before install — so the device only ever runs firmware you signed.

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
- **Speed (16 MHz):** the core runs at 16 MHz, so the boot-time signature verify
  (~5 s, behind a "Verifying…" screen) and the passkey PBKDF2 (~2–4 s) are slow.
  Raising the clock to 80 MHz would cut both ~5× with no security cost — planned,
  not yet applied. Keep I²C on its own 16 MHz source so the display is unaffected.

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

---

## Source layout

```
Firmware/
  build_signed.sh          one-shot: build app -> sign -> build BL -> merge (+ staging blob)
  Makefile                 app build (-> build/muru_vault.{hex,elf,bin})
  linker/
    stm32l442.ld           app @ 0x08008000, 96 KB (storage stays @ 0x0803E000)
    bootloader.ld          bootloader @ 0x08000000, 30 KB
  src/                     APPLICATION
    main.c  ui.c           entry + all screens/menus/keyboard/idle
    vault.c  flash_store.c passkey auth, seed-derived master key, encrypted entries, guard words
    bip39.c  diceware.c    12-word recovery (BIP39) + Estonian passphrase/guard wordlist
    fwupdate.c  usbfs.c    USB update mode + polled USB-FS CDC device
    flashraw.c             generic flash writer (streams image into STAGING)
    fwimage.h              shared image/manifest format + flash map
    ssd1306/font/i2c/buttons/rng/clock/lowpower   drivers
    crypto/{sha256,aes}.c  SHA-256/HMAC/PBKDF2 + AES-256
    cmsis/, startup_*.s, system_stm32l4xx.c       device scaffolding
  bootloader/              SECURE BOOTLOADER
    main.c                 verify app -> jump, or install a staged update first
    boot_verify.c          SHA-256(core||payload) + Ed25519 verify (tweetnacl)
    boot_pubkey.c          baked-in public key (generated by fw_sign.py keygen)
    update.c  bl_flash.c   staged-image install + anti-rollback, flash ops
  tools/
    fw_sign.py             keygen / sign / verify / blob / merge
    fw_send.py             stream a signed blob to the device over USB-CDC (pyserial)
    curate_estonian.py  validate_wordlist.py   wordlist generation/checking
    fw_signing_key.pem     ROOT OF TRUST - private key, gitignored, keep secret + backed up
```

Flash map and the signed-image format live in [src/fwimage.h](src/fwimage.h).