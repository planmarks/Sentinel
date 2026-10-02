# Sentinel — STM32 + OPTIGA Trust M rebuild plan

This is the migration of the working ESP32-C5 proof of concept (`../Cold Wallet/`)
onto an **STM32L442KCU6** MCU with an **Infineon OPTIGA™ Trust M** secure element,
a 0.96" I²C OLED, and 6 tactile switches. **USB-only** (BLE is dropped).

The goal is to get past the ESP32's core weakness — **no certified secure element**,
so a physical attacker with lab gear (power analysis, glitching, decapping) can extract
the seed — by moving the secret at-rest, the RNG, and the anti-brute-force counter into a
Common Criteria **EAL6+** tamper-resistant chip, and locking the MCU down with RDP Level 2.

---

## 0. The one constraint that shapes everything

**OPTIGA Trust M cannot sign your coins' curves.** Per the Infineon datasheet it supports
only **NIST P-256/384/521 and Brainpool** curves. Your wallet signs on **secp256k1**
(BTC/ETH/EVM/XRP/…) and **ed25519** (SOL/XLM/ADA/TON) — *neither is supported*.

So OPTIGA is **not** a signing enclave here. Its role is:

| OPTIGA capability | What it replaces / hardens |
|---|---|
| **Protected data objects** (CC EAL6+, **10 kB** user memory) | Seed/entropy **at rest** — was raw STM32/ESP32 flash |
| **Certified TRNG** | `esp_random()` for seed generation |
| **Hardware monotonic counters** (non-rollback) | The wrong-PIN wipe counter — was "count in flash before the check" |
| **Shielded Connection** (encrypted+authenticated I²C) | Protects the seed while it crosses the bus to the MCU |
| AES-256 / SHA-256 / ECC-NIST engine | Blob wrapping; NIST attestation if ever needed |

**Residual gap (be honest in the README):** the seed still enters STM32 RAM to derive and
sign, so it is exposed *during signing*. The STM32L442 is Cortex-M4 — **no TrustZone**, so
there is no secure world to hide that in. We narrow the gap with **RDP Level 2** (permanently
kills SWD/JTAG and flash read-out), the **L4 Firewall** peripheral (shelters the signing code
+ seed buffer), aggressive **RAM zeroization**, and an optional **case-tamper input**. This is
a large, real improvement over the ESP32 — but it is not a Ledger-class in-SE signer, and the
docs must say so.

> If in future you want signing *inside* a secure element, that needs a different SE that
> supports secp256k1/ed25519 — concretely the **NXP SE050/SE051** (CC EAL6+, does both curve
> families), or an ST33-class chip. OPTIGA can't. See the second-sources table under §1.

---

## 1. Target hardware

| Part | Role | Notes |
|---|---|---|
| **STM32L442KCU6** | MCU | Cortex-M4F @ 80 MHz, **256 KB flash / 64 KB RAM**, UFQFPN-32, hardware **AES-256 + TRNG**, crystal-less **USB 2.0 FS device**, RDP L2, Firewall, MPU, RTC anti-tamper |
| **OPTIGA Trust M** — `SLS32AIA010MLUSON10XTMC1` | Secure element | General-purpose Trust M (NOT Express), 10 kB user memory, CC EAL6+, I²C @ 7-bit `0x30`, PG-USON-10 (3×3 mm), Shielded Connection, monotonic counters, protected objects. Variant **L** = ext temp −40…+105 °C; config **C1**. Chosen for single-unit availability |
| **0.96" SSD1306 OLED** | Display | I²C @ 7-bit `0x3C` — shares the bus with OPTIGA, no address clash |
| **6× tactile switches** | Input | Up / Down / OK / Back / Left / Right, active-LOW with MCU internal pull-ups |
| **USB-C (data)** | Host link | The wallet's **own** data port (see §5 — the charging module is charge-only) |
| **TPS7A02** (or MCP1700) 3.3 V LDO | Power | Ultra-low-Iq; feeds MCU + OPTIGA + OLED from the battery |

**Flash budget watch-item:** 256 KB is a big step down from the ESP32's 8 MB. trezor-crypto +
TweetNaCl + BLAKE2 + mbedTLS + U8g2 + the app should fit with `-Os`, but the full 16-coin set
+ QR + Jade protocol may need trimming. Validate size early (Phase A) and, if tight, ship a
core chain set first (BTC/ETH/SOL) and gate the rest behind a build flag.

**Secure-element sourcing & second sources.** Bare OPTIGA Trust M dice are churny — configs go
MOQ-only / EOL without much notice — so the exact config was chosen for single-unit availability
(`…XTMC1`, ext-temp). Note the config code (`XTMC1`) must be a **general-purpose / customer-
configurable** profile so we can create our own data objects and set the seed's access condition;
verify against Infineon's *OPTIGA Trust M Configuration Guide* before a production order. If OPTIGA
supply fails again, the pre-vetted second sources are:

| Alt SE | Tier | Fit | Note |
|---|---|---|---|
| **NXP SE050 (C2)** | CC EAL6+ | **Upgrade** | Only mainstream SE that natively does **secp256k1 + Ed25519** → could sign *in-SE* (closes the RAM-exposure gap in §0). In stock as singles (~$5, 3×3 QFN). Needs Plug&Trust middleware; BIP32 still MCU-side |
| **Microchip ATECC608B** (`-SSVDA-T`) | JIL "High" | Vault only | Cheapest/most available (SOIC-8, hand-solderable, ~$1 singles). P-256 only — lower cert tier than OPTIGA; use only if EAL6+ isn't required |
| STSAFE-A120 | EAL5+ | Vault only | First-party to STM32, but EAL5+ and no secp256k1 — does not meet the EAL6+ bar |

Swapping to SE050 is the only alt that changes the design (§0/§4 gain an in-SE signing path);
ATECC608B is a like-for-like vault swap with a different footprint.

---

## 2. Proposed pin map (STM32L442KCU6, UFQFPN-32)

Active-LOW buttons use `INPUT_PULLUP`; no external resistors. I²C needs 4.7 kΩ pull-ups to 3V3.
Adjust to final layout — this avoids USB (PA11/12), SWD (PA13/14) and the I²C pins.

| Signal | Pin | Signal | Pin |
|---|---|---|---|
| I²C1 SCL (OLED + OPTIGA) | **PB6** | Btn Up | **PA0** |
| I²C1 SDA (OLED + OPTIGA) | **PB7** | Btn Down | **PA1** |
| OPTIGA RST | **PB0** | Btn OK/Select | **PA2** |
| USB DM | **PA11** | Btn Back/Del | **PA3** |
| USB DP | **PA12** | Btn Left | **PA4** |
| SWDIO / SWCLK (lock in prod) | **PA13 / PA14** | Btn Right | **PA5** |
| BOOT0 → GND (via R) | — | Case-tamper (opt.) → RTC_TAMP | **PA0/PC13-class** |

OPTIGA and OLED are **both on I²C1** at different addresses — one bus, two devices.

---

## 3. Firmware port strategy

The ESP32 firmware (`../Cold Wallet/firmware/coldwallet/`) is Arduino C++ and ~90 % portable.
Fastest path: **STM32duino** (`Arduino_Core_STM32`), target a **Generic L442KCUx** variant
(or the L432KC variant with AES enabled). Keep the app code; swap the core.

| Area | ESP32 today | STM32 rebuild |
|---|---|---|
| Crypto primitives | trezor-crypto/uBitcoin, TweetNaCl, BLAKE2, mbedTLS | **Unchanged** — pure C/C++, compiles on STM32; mbedTLS ships in the STM32 ecosystem |
| Display | U8g2 over `Wire` | **Unchanged** (STM32duino `Wire`) |
| Buttons | `digitalRead` + pull-ups | **Unchanged** |
| Host link | USB CDC `Serial` (Jade protocol) | **Unchanged** — STM32 USB CDC |
| RNG | `esp_random()` | **OPTIGA `optiga_crypt_random`** (STM32 `RNG` as fallback) |
| Storage (`store.cpp`) | NVS `Preferences` + PBKDF2-in-flash + AES-GCM | **Rewrite against OPTIGA** — see §4 |
| BLE (`ble.cpp`, NimBLE) | optional transport | **Deleted** — remove menu entry, `SignSource::SRC_BLE`, NimBLE dep |

`config.h` changes: replace the `PIN_BTN_*` `D0…D9` macros with the STM32 pin names from §2;
drop the `BLE_*` block; keep the `COIN_*` and PIN/PBKDF2 constants.

---

## 4. Storage & anti-brute-force redesign (the OPTIGA payoff)

`store.cpp` is the piece that most benefits from the SE. Two options — implement A first.

**Option A — SE-as-vault (recommended first):**
1. Seed generated from **OPTIGA TRNG**.
2. Entropy (+ optional passphrase) wrapped **AES-256-GCM** with a key from **PBKDF2(PIN, salt)**
   — same scheme as today — then written to an **OPTIGA protected data object** whose read
   access requires the **Shielded Connection**. Bus-probing the seed is now defeated.
3. Wrong-PIN counter = an **OPTIGA monotonic counter**, incremented *before* the GCM check.
   Because it lives in the SE it **cannot be rolled back** by power-cycling or by rewriting
   flash — closing the exact hole the current "count in flash first" trick papers over.
4. Counter reaches `PIN_MAX_TRIES` → erase the seed object (self-wipe).

**Option B — SE-enforced retry limit (hardening, later):**
Store a PIN-derived secret as an OPTIGA **authorization reference** and set the seed object's
access-condition to "authenticate against it," with a linked monotonic counter as the retry
cap. Brute-force resistance then lives **inside** the SE, not in MCU code.

> **Shielded Connection provisioning note:** the platform-binding secret for the encrypted I²C
> link has to live on the STM32 side (chicken-and-egg). It sits in STM32 flash protected by
> **RDP L2** + the Firewall. Provision it once at manufacture and document the step.

---

## 5. Power & charging integration

Your `Charging Module` is **charge + protect only** — MCP73833 + DW01A/FS8205A, protected
`OUT+/OUT-` at raw cell voltage (~3.0–4.2 V), and by design **no 3.3 V rail and no USB data
path**. The wallet board therefore adds:

- **3.3 V LDO** (TPS7A02 — ~25 nA Iq — or MCP1700) from `OUT+` → one 3V3 rail for MCU + OPTIGA
  (1.62–5.5 V ok) + OLED. Add bulk + decoupling caps.
- Optional **power/enable switch** or load switch; a cold wallet is off most of its life.

**Two-USB-C decision (flag):** because the charging module's USB-C carries **power only**, the
wallet board needs its **own USB-C data port** wired to STM32 `PA11/PA12` for the host link.
- **Now (simplest):** two USB-C ports — charge on the module, data on the wallet. Matches the
  current module scope.
- **Later (product):** merge into a **single USB-C** that does both — one receptacle, VBUS to
  the charger, D+/D− to the STM32. Cleanest UX; a small redesign of the module.

Wiring: charging module `J1 OUT+/OUT-` → wallet board `VIN/GND` → LDO → 3V3.

---

## 6. Security hardening checklist (STM32 side, Phase C)

- **RDP Level 2** — permanently disables SWD/JTAG and boot-loader flash read-out. *Irreversible;*
  do it on a dedicated unit, Level 1 during development.
- **L4 Firewall** — enclose the signing routine + seed RAM buffer in the Firewall segment so
  the rest of the app can't read it.
- **PCROP** — read-out-protect the crypto code regions.
- **RAM zeroization audit** — confirm entropy/PIN/derived-key buffers are wiped after every use
  (the ESP32 build already does this via `mbedtls_platform_zeroize`; verify on STM32).
- **Anti-tamper** — wire a case-open switch to an RTC tamper pin to auto-erase backup registers
  (and optionally trigger a seed wipe) on intrusion.
- **Reproducible build** — pin the toolchain + all deps to exact commits; document a container
  recipe so a third party can reproduce the binary (important for a wallet you distribute).

---

## 7. Phased roadmap

**Phase A — Board bring-up & feature parity (no OPTIGA yet)**
STM32L442 + OLED + 6 buttons + USB CDC. Port the firmware under STM32duino with a *temporary*
software storage layer (STM32 flash + the existing PBKDF2/AES-GCM). Prove: boot self-test PASS,
seed gen, PIN, reboot/unlock, wrong-PIN wipe, restore, receive addresses match, and a **testnet
sign+broadcast over USB**. Confirm the flash budget (§1).

**Phase B — Integrate OPTIGA Trust M**
Bring up I²C to OPTIGA; TRNG for seed gen; move storage to Option A (protected object + monotonic
counter); enable the Shielded Connection. Re-run the full Phase-A test set.

**Phase C — Hardening**
RDP L2, Firewall/PCROP over signing, disable SWD, anti-tamper, zeroization audit, reproducible
build. Consider Option B.

**Phase D — Power, enclosure, product**
Integrate the charging module + 3.3 V rail + battery; decide one- vs two-USB-C; case with
tamper switch; custom PCB.

---

## 8. New-board BOM (first pass)

| Qty | Part | Value / MPN | Notes |
|---|---|---|---|
| 1 | MCU | STM32L442KCU6 | UFQFPN-32 |
| 1 | Secure element | OPTIGA Trust M `SLS32AIA010MLUSON10XTMC1` | General-purpose config (not Express); 10 kB, PG-USON-10, ext-temp; single-unit sourceable |
| 1 | OLED | 0.96" SSD1306 128×64 I²C | addr `0x3C` |
| 6 | Tactile switch | 6 mm SMD | Up/Down/OK/Back/Left/Right |
| 1 | LDO | TPS7A0233 (3.3 V) | ultra-low Iq; MCP1700 alt |
| 1 | USB-C (data) | USB2.0 receptacle | wallet host port |
| 2 | I²C pull-ups | 4.7 kΩ 0402/0805 | SDA/SCL |
| — | Decoupling | 100 nF ×N, 1–10 µF bulk | per MCU/OPTIGA/OLED/LDO |
| 1 | BOOT0 resistor | 10 kΩ → GND | |
| — | (opt.) tamper switch | NO/NC contact | to RTC_TAMP |

Charging module (existing, separate board): MCP73833, DW01A, FS8205A, USB-C (power), etc. —
see `../../General Electronics/Charging Module/`.

---

## 9. Open decisions to confirm

1. **Two USB-C ports now, or merge charge+data into one** (§5)?
2. **Trim the coin set** to fit 256 KB, or optimize/keep all 16 (§1)?
3. **Storage Option A now, add Option B later** — agree with that ordering (§4)?
4. **Custom PCB vs. dev-board bring-up first** for Phase A?
