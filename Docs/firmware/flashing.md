# Building & flashing

- [What you need](#what-you-need)
- [Connecting the programmer](#connecting-the-programmer)
- [Building Muru Vault from source](#building-muru-vault-from-source)
- [Flashing with STM32CubeProgrammer](#flashing-with-stm32cubeprogrammer)
- [Switching between firmwares](#switching-between-firmwares)
- [Read-out protection (RDP)](#read-out-protection-rdp)
- [Troubleshooting](#troubleshooting)

---

## What you need

| Item | Notes |
|---|---|
| ST‑LINK V2 / V3 (or any SWD probe supported by STM32CubeProgrammer) | |
| Tag‑Connect **TC2030‑IDC‑NL** cable (+ ARM 10‑pin adapter if needed) | Matches J2 (V1B) / J1 (V1C) |
| [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) | GUI or `STM32_Programmer_CLI` |
| GNU Arm toolchain + `make` | Only to rebuild. STM32CubeIDE 2.2.0 bundles `arm-none-eabi-gcc` 14.3.1 and `make` |
| Charged battery or USB power | The board must be powered during programming |

---

## Connecting the programmer

| TC2030 pin | Signal | ST‑LINK |
|---|---|---|
| 1 | VCC (3.3 V, reference only) | T_VCC / VAPP |
| 2 | SWDIO | SWDIO |
| 3 | NRST | NRST |
| 4 | SWCLK | SWCLK |
| 5 | GND | GND |
| 6 | SWO | not connected on the board |

1. Fit the battery (or plug in USB) and **switch the device on** — the probe does not power the board.
2. Press the Tag‑Connect cable onto the footprint (V1B: J2, on the front side).
3. On **V1C** you can alternatively use the four 2 mm pogo pads (GND, 3V3, SWDIO, SWCLK) with the flashing rig;
   connect under reset is then not available (no NRST pad), use *hot plug* / *normal* mode.

---

## Building Muru Vault from source

```bash
cd Vault/Firmware
make
```

Outputs in `Vault/Firmware/build/`:

| File | Use |
|---|---|
| `muru_vault.hex` | **Flash this** (contains its own load address `0x08000000`) |
| `muru_vault.bin` | Raw binary — must be written at `0x08000000` |
| `muru_vault.elf` | Debugging (symbols, `-g3`) — generated locally, not committed |
| `muru_vault.map` | Linker map — generated locally, not committed |

`make` finishes by printing the section sizes and the linker's memory-usage table. `make clean` removes build outputs
(it leaves the wordlist text files in `build/` alone).

Compiler flags: `-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -Os -g3 -std=c11
-ffunction-sections -fdata-sections`, linked with `nano.specs` / `nosys.specs` and `--gc-sections`.

### Windows note

STM32CubeIDE's toolchain and `make` are not on `PATH` by default. Either use the IDE's terminal or add, for example:

```
C:\ST\STM32CubeIDE_<ver>\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.<ver>\tools\bin
C:\ST\STM32CubeIDE_<ver>\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.<ver>\tools\bin
```

The Makefile uses `mkdir -p` and `rm -f`, so run it from Git Bash / MSYS or the CubeIDE shell rather than `cmd.exe`.

---

## Flashing with STM32CubeProgrammer

### GUI

1. Select **ST‑LINK**, port **SWD**, mode *Normal* (or *Under reset*), click **Connect**.
2. *Erasing & programming* → browse to the `.hex` file → tick *Verify programming* → **Start Programming**.
3. Disconnect and power-cycle (or press *Reset*).

### CLI

```bash
STM32_Programmer_CLI -c port=SWD -d Vault/Firmware/build/muru_vault.hex -v -rst
```

| Firmware | File |
|---|---|
| Muru Vault | `Vault/Firmware/build/muru_vault.hex` |
| Sentinel Core 0.1.0 | `Vault/sentinel_stm32.hex` |

For a `.bin` file give the address explicitly: `-d muru_vault.bin 0x08000000`.

---

## Switching between firmwares

STM32CubeProgrammer only erases the flash pages it is about to write. Muru Vault keeps its data in the **last
8 KB** (`0x0803E000`–`0x0803FFFF`), which neither image overwrites, so a plain re-flash **preserves**:

- the encrypted vault, AUTH blob and settings when you update Muru Vault — **this is what you want for upgrades**;
- but also leaves that data in flash if you switch to Sentinel Core.

To start from a clean device (or before handing a board to someone else), do a full erase first:

```bash
STM32_Programmer_CLI -c port=SWD -e all
```

To erase only the Muru Vault storage region (factory-reset without reflashing), erase pages 124–127:

```bash
STM32_Programmer_CLI -c port=SWD -e 124 125 126 127
```

---

## Read-out protection (RDP)

Both firmwares currently ship with **RDP Level 0** (debug and flash read-out enabled) for development.

| Level | Effect | Reversible? |
|---|---|---|
| 0 | Full SWD access | — |
| 1 | Flash not readable via SWD/bootloader; regressing to 0 **mass-erases** the chip | Yes (with erase) |
| 2 | SWD permanently disabled, option bytes frozen | **No — the chip can never be reprogrammed over SWD again** |

```bash
# Level 1 (0xBB) — recommended for daily-use units
STM32_Programmer_CLI -c port=SWD -ob RDP=0xBB
# Back to Level 0 (0xAA) — erases the whole flash, including the vault
STM32_Programmer_CLI -c port=SWD -ob RDP=0xAA
```

Only use Level 2 (`0xCC`) on a final, fully tested unit.

---

## Troubleshooting

| Symptom | Things to check |
|---|---|
| `No STM32 target found` | Board powered and switched on? Tag‑Connect seated firmly? Try *Connect under reset* with NRST wired |
| Connection drops when the firmware sleeps | Muru Vault enters STOP 2 after 2 min idle; press **OK** to wake, or connect *under reset* |
| *"Debug in Low Power mode is not supported"* in the log | Informational on this device; harmless |
| Screen stays black after flashing | OLED not acknowledging on I²C — firmware halts in `WFI`. Check DS1 FPC solder joints and the 0x3C address straps |
| Display upside-down | Expected only if the OLED is mounted on the opposite side from V1B; change `0xA0`/`0xC0` to `0xA1`/`0xC8` in `ssd1306.c` |
| Device immediately shows *Welcome* after an update | The AUTH page was erased. If only AUTH/CFG were lost, **Recover** with your 12 words (+ guard words) brings the vault back; after a full chip erase (`-e all`) the vault itself is gone and Recover starts an empty one |
