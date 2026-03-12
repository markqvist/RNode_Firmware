#!/bin/bash
#
# Workaround for ESP32-S3 rev v0.2 USB-Serial/JTAG controller bug:
# the USB controller drops after ~80KB of sustained compressed writes.
#
# This script splits the firmware into small page-aligned chunks and
# flashes each with a full device reset between them, keeping each
# transfer well under the ~80KB compressed limit.
# Only pauses on failure — successful writes proceed immediately.
#
# Usage:
#   ./flash_parts.sh [port] [firmware_bin]
#
# Defaults:
#   port          = /dev/ttyACM4
#   firmware_bin  = build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin

set -uo pipefail

PORT="${1:-/dev/ttyACM4}"
FIRMWARE="${2:-build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin}"
BOOTLOADER="build/esp32.esp32.esp32s3/RNode_Firmware.ino.bootloader.bin"
PARTITIONS="build/esp32.esp32.esp32s3/RNode_Firmware.ino.partitions.bin"
BOOT_APP0="${HOME}/.arduino15/packages/esp32/hardware/esp32/2.0.17/tools/partitions/boot_app0.bin"

BAUD=460800
DELAY=1            # seconds between successful writes (USB recovery)
RETRY_DELAY=8      # seconds to wait before retry on failure
N_PARTS=16
FW_BASE=0x10000    # firmware flash offset
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

# --- Validate inputs ---
for f in "$FIRMWARE" "$BOOTLOADER" "$PARTITIONS" "$BOOT_APP0"; do
    if [ ! -f "$f" ]; then
        echo "ERROR: Missing file: $f"
        echo "Run 'make firmware-tbeam_supreme' first."
        exit 1
    fi
done

if [ ! -e "$PORT" ]; then
    echo "ERROR: Serial port $PORT not found"
    exit 1
fi

FW_SIZE=$(stat -c%s "$FIRMWARE")
echo "Firmware: $FIRMWARE ($FW_SIZE bytes)"
echo "Port:     $PORT"
echo ""

# --- Split firmware into page-aligned parts ---
CHUNK_RAW=$(( (FW_SIZE / N_PARTS / 4096) * 4096 ))

python3 -c "
import sys
with open('$FIRMWARE', 'rb') as f:
    data = f.read()
size = len(data)
chunk = $CHUNK_RAW
n = $N_PARTS
for i in range(n):
    start = i * chunk
    end = size if i == n - 1 else start + chunk
    part = data[start:end]
    pad = ((len(part) + 4095) // 4096) * 4096
    part = part.ljust(pad, b'\xff')
    path = '$TMPDIR/part_{}.bin'.format(i)
    with open(path, 'wb') as pf:
        pf.write(part)
    addr = $FW_BASE + start
    print('Part {:2d}: 0x{:06x}  {:6d} bytes (padded to {})'.format(i, addr, end - start, pad))
"

echo ""

# --- Flash bootloader + partition table + boot_app0 ---
echo "=== Flashing bootloader, partitions, boot_app0 ==="
OUTPUT=$(esptool --chip esp32s3 --port "$PORT" --baud "$BAUD" \
    --before default-reset --after hard-reset \
    write-flash -z --flash-mode dio --flash-freq 80m --flash-size 8MB \
    0x0     "$BOOTLOADER" \
    0x8000  "$PARTITIONS" \
    0xe000  "$BOOT_APP0" 2>&1)

if ! echo "$OUTPUT" | grep -q "Hash of data verified"; then
    echo "Bootloader: FAILED — retrying after ${RETRY_DELAY}s..."
    sleep "$RETRY_DELAY"
    OUTPUT=$(esptool --chip esp32s3 --port "$PORT" --baud "$BAUD" \
        --before default-reset --after hard-reset \
        write-flash -z --flash-mode dio --flash-freq 80m --flash-size 8MB \
        0x0     "$BOOTLOADER" \
        0x8000  "$PARTITIONS" \
        0xe000  "$BOOT_APP0" 2>&1)
    if ! echo "$OUTPUT" | grep -q "Hash of data verified"; then
        echo "Bootloader: FAILED after retry"
        echo "$OUTPUT" | tail -n 5
        exit 1
    fi
fi
echo "Bootloader: OK"
sleep "$DELAY"

# --- Flash each firmware part ---
FAILED=0
for i in $(seq 0 $((N_PARTS - 1))); do
    ADDR=$(printf "0x%06x" $((FW_BASE + i * CHUNK_RAW)))

    echo "=== Part $i/$((N_PARTS - 1)): $ADDR ==="
    OUTPUT=$(esptool --chip esp32s3 --port "$PORT" --baud "$BAUD" \
        --before default-reset --after hard-reset \
        write-flash -z --flash-mode dio --flash-freq 80m --flash-size 8MB \
        "$ADDR" "$TMPDIR/part_${i}.bin" 2>&1)

    if echo "$OUTPUT" | grep -q "Hash of data verified"; then
        echo "Part $i: VERIFIED"
        sleep "$DELAY"
    else
        echo "Part $i: FAILED — retrying after ${RETRY_DELAY}s..."
        sleep "$RETRY_DELAY"
        OUTPUT=$(esptool --chip esp32s3 --port "$PORT" --baud "$BAUD" \
            --before default-reset --after hard-reset \
            write-flash -z --flash-mode dio --flash-freq 80m --flash-size 8MB \
            "$ADDR" "$TMPDIR/part_${i}.bin" 2>&1)

        if echo "$OUTPUT" | grep -q "Hash of data verified"; then
            echo "Part $i: VERIFIED (retry)"
        else
            echo "Part $i: FAILED after retry"
            echo "$OUTPUT" | tail -n 5
            FAILED=1
            break
        fi
    fi
done

echo ""
if [ "$FAILED" -eq 0 ]; then
    echo "=== ALL $N_PARTS PARTS FLASHED AND VERIFIED ==="
else
    echo "=== FLASH INCOMPLETE — see errors above ==="
    exit 1
fi
