#!/usr/bin/env python3
"""Compute the ESP32 partition hash for an RNode firmware binary.

The ESP32 build toolchain appends a SHA256 hash to firmware binaries.
esp_partition_get_sha256() returns this embedded hash (the last 32 bytes).
This is different from sha256sum of the entire file.

Usage:
    python3 get_partition_hash.py <firmware.bin>

    # Flash and set hash:
    python3 get_partition_hash.py build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin
    rnodeconf -H <output_hash> /dev/ttyACM0
"""
import hashlib
import sys

def get_partition_hash(firmware_path):
    with open(firmware_path, "rb") as f:
        fw = f.read()

    embedded_hash = fw[-32:]
    calc_hash = hashlib.sha256(fw[:-32]).digest()

    if calc_hash != embedded_hash:
        print(f"WARNING: Embedded hash doesn't match SHA256(fw[:-32])", file=sys.stderr)
        print(f"  Calculated: {calc_hash.hex()}", file=sys.stderr)
        print(f"  Embedded:   {embedded_hash.hex()}", file=sys.stderr)
        return None

    return embedded_hash.hex()

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <firmware.bin>", file=sys.stderr)
        sys.exit(1)

    result = get_partition_hash(sys.argv[1])
    if result:
        print(result)
    else:
        sys.exit(1)
