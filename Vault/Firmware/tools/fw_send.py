#!/usr/bin/env python3
"""Send a signed firmware blob to Muru Vault over its USB-CDC update mode.

On the device:  Settings -> Firmware Update -> OK  (it enumerates as a COM port).
Then run:       python tools/fw_send.py <PORT> [blob]
  <PORT>  e.g. COM5  (Windows)  or  /dev/ttyACM0 (Linux)
  [blob]  defaults to build/muru_update.bin  (made by build_signed.sh)

Protocol: "MVUP" + uint32 LE length, wait "RDY\n", stream blob, wait "DONE\n".
Requires pyserial:  pip install pyserial
"""
import sys, os, struct, time
try:
    import serial
except ImportError:
    sys.exit("pyserial not installed. Run:  pip install pyserial")

def main(port, blob_path):
    blob = open(blob_path, "rb").read()
    print(f"blob: {blob_path} ({len(blob)} bytes)")
    ser = serial.Serial(port, 115200, timeout=5)
    time.sleep(0.2)
    ser.reset_input_buffer()
    ser.write(b"MVUP" + struct.pack("<I", len(blob)))
    ser.flush()
    line = ser.readline().strip()
    if line != b"RDY":
        sys.exit(f"device did not accept header (got {line!r})")
    print("device ready, streaming…")
    CHUNK = 64
    sent = 0
    t0 = time.time()
    for i in range(0, len(blob), CHUNK):
        ser.write(blob[i:i+CHUNK]); sent += len(blob[i:i+CHUNK])
        if sent % 8192 < CHUNK:
            print(f"  {sent*100//len(blob):3d}%  ({sent}/{len(blob)})", end="\r")
    ser.flush()
    print(f"  100%  ({sent}/{len(blob)}) in {time.time()-t0:.1f}s")
    resp = ser.readline().strip()
    if resp == b"DONE":
        print("device accepted image - it is installing and rebooting. [OK]")
    else:
        print(f"device reported: {resp!r} (image rejected)")
    ser.close()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(2)
    port = sys.argv[1]
    blob = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.dirname(__file__), "..", "build", "muru_update.bin")
    main(port, blob)
