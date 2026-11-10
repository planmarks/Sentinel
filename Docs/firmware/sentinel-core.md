# Sentinel Core — prebuilt cold-wallet firmware

`sentinel_stm32.hex` is a prebuilt **cryptocurrency cold-wallet** firmware for the
[Sentinel V1B](../hardware/sentinel-v1b.md) board. Two identical copies are in the repository:

- [`Vault/sentinel_stm32.hex`](../../Vault/sentinel_stm32.hex)
- [`Cold Wallet/Hardware/sentinel_stm32.hex`](../../Cold%20Wallet/Hardware/sentinel_stm32.hex)

> **The source code for this firmware is not in this repository.** Everything below was determined by inspecting
> the image (load address, vector table and embedded strings). Treat it as a description, not a specification.
> According to [`ARCHITECTURE.md`](../../Cold%20Wallet/Hardware/ARCHITECTURE.md), it is a port of an earlier ESP32‑C5 proof of concept
> (Arduino C++, trezor-crypto / TweetNaCl / mbedTLS / U8g2) to the STM32L442 with the OPTIGA Trust M.

---

## Image facts

| Property | Value |
|---|---|
| Format | Intel HEX, single segment |
| Load address | `0x0800_0000` |
| Size | **≈ 226 KB** (231 528 bytes) — **≈ 88 % of the STM32L442's 256 KB flash** |
| Initial stack pointer | `0x2000_C000` (top of the 48 KB SRAM1) |
| Target MCU | STM32L442KCU6 (device ID `0x435` in the programming log) |
| Version string | `Sentinel Core` **`0.1.0-stm32`**, protocol tag `sentinel-stm32/v1` |
| Built with | C++ (libstdc++ `basic_string` symbols present) |

Because the image ends at ≈ `0x0803_8868`, it does **not** overlap the Muru Vault storage region
(`0x0803_E000`–`0x0803_FFFF`). See [switching firmwares](flashing.md#switching-between-firmwares).

---

## Features (from embedded strings)

| Area | Evidence in the image |
|---|---|
| Coins | **Bitcoin** (`m/84h/0h/0h/0/n` — native SegWit) and **Ethereum** (`m/44h/60h/0h/0/n`); *Install Coins* / *Uninstall All Coins* menus |
| Seed | BIP39 **12 / 18 / 24-word** phrases, generate + *Restore* with word-by-word entry and checksum check (*"Bad checksum"*), backup *Verify* step |
| PIN | *Set PIN* / *Confirm PIN* / *Change PIN*, *"tries left"*, *"Too many tries"* → *Wiped*, *Hardware Lockout*, *"Security limit reached"* |
| Secure element | Uses the OPTIGA Trust M (`optiga_util_create` error path) |
| Host link | USB serial protocol with `HELLO`, `SENTINEL_READY|…` handshake and `ADDR_REQ|` address requests; *Sentinel Interface* / *Sentinel Config* descriptors |
| Settings | *Lock Device*, *Power Off*, *Battery Saver* (auto-off 1 / 3 / 5 / 10 min), *Device Information*, *Reset Device* |
| Languages | English, Spanish, Portuguese, Estonian, Russian |
| Self-test | An embedded test PSBT (Base64 `cHNidP8…`) suggests a signing self-test |

---

## Flashing

```bash
STM32_Programmer_CLI -c port=SWD -e all -d Vault/sentinel_stm32.hex -v -rst
```

`-e all` performs a full chip erase first, which is recommended when moving from Muru Vault to Sentinel Core
(it removes any leftover vault data). See [flashing.md](flashing.md).

---

## Relation to the V1C ("2MB") board

At ~226 KB the wallet image leaves almost no room on the STM32L442 for more coins, QR transaction transport or
larger UI assets — the flash-budget risk called out in `ARCHITECTURE.md §1`. The [V1C board](../hardware/sentinel-v1c.md)
moves to the STM32U585 (2 MB flash, 786 KB RAM, TrustZone) to remove that limit. The `Cold Wallet/Firmware/`
folder intended for its firmware is currently empty.
