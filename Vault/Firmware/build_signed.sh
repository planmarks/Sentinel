#!/usr/bin/env bash
# Build the app, sign it, build the bootloader, and merge into one flashable hex.
# Usage:  ./build_signed.sh [version]    (version defaults to 1)
# Requires: tools/fw_signing_key.pem (run `python tools/fw_sign.py keygen` once).
set -e
cd "$(dirname "$0")"
VER="${1:-1}"

echo "[1/4] building application (relocated to 0x08008000)…"
make >/dev/null

echo "[2/4] signing app as version $VER…"
python tools/fw_sign.py sign   build/muru_vault.bin "$VER" build/manifest.bin
python tools/fw_sign.py verify build/muru_vault.bin build/manifest.bin

echo "[3/4] building secure bootloader…"
make -C bootloader >/dev/null

echo "[4/4] merging bootloader + manifest + app, and emitting staging blob…"
python tools/fw_sign.py merge bootloader/build/bootloader.bin build/manifest.bin \
       build/muru_vault.bin build/muru_signed.hex
# streamable signed image (manifest+payload): what the updater writes to STAGING.
python tools/fw_sign.py blob build/manifest.bin build/muru_vault.bin build/muru_update.bin

echo
echo "Fresh install : flash build/muru_signed.hex to 0x08000000 (BL+manifest+app)."
echo "Field update  : deliver build/muru_update.bin to STAGING (0x08020000);"
echo "                the bootloader verifies + installs it on next boot."
echo "The bootloader verifies the app's Ed25519 signature before every boot."
