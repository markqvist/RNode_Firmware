#!/usr/bin/env python3

# Quick KISS protocol smoke test for RNode devices.
# Sends standard KISS commands over USB serial and verifies responses.
#
# Usage:
#   python3 test_kiss.py [port]
#   make test-kiss PORT=/dev/ttyACM4

import serial
import struct
import sys
import time

FEND = 0xC0

def parse_kiss_frames(data):
    frames = []
    i = 0
    while i < len(data):
        if data[i] == 0xC0:
            while i < len(data) and data[i] == 0xC0:
                i += 1
            frame = bytearray()
            while i < len(data) and data[i] != 0xC0:
                frame.append(data[i])
                i += 1
            if len(frame) > 0:
                frames.append(frame)
        else:
            i += 1
    return frames

def wait_for_cmd(ser, target_cmd, timeout=2.0):
    start = time.time()
    buf = bytearray()
    while time.time() - start < timeout:
        chunk = ser.read(256)
        if chunk:
            buf.extend(chunk)
            for f in parse_kiss_frames(buf):
                if f[0] == target_cmd:
                    return f
        else:
            time.sleep(0.05)
    return None

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM4"

    ser = serial.Serial(port, 115200, timeout=0.1)
    ser.dtr = False
    time.sleep(0.1)
    ser.dtr = True
    time.sleep(4)
    ser.reset_input_buffer()
    time.sleep(1)
    ser.read(8192)

    tests = []

    # Batch detect (same sequence rnodeconf uses)
    detect_seq = bytes([
        FEND, 0x08, 0x73,   # CMD_DETECT, DETECT_REQ
        FEND, 0x50, 0x00,   # CMD_FW_VERSION
        FEND, 0x48, 0x00,   # CMD_PLATFORM
        FEND, 0x49, 0x00,   # CMD_MCU
        FEND, 0x47, 0x00,   # CMD_BOARD
        FEND,
    ])
    ser.write(detect_seq)
    time.sleep(1)
    resp = ser.read(8192)
    received = {}
    if resp:
        for f in parse_kiss_frames(resp):
            received[f[0]] = f

    checks = [
        ("DETECT",     0x08, lambda f: f"0x{f[1]:02X}" if len(f) > 1 else "?"),
        ("FW_VERSION", 0x50, lambda f: f"v{f[1]}.{f[2]:02X}" if len(f) >= 3 else "?"),
        ("PLATFORM",   0x48, lambda f: f"0x{f[1]:02X}" if len(f) > 1 else "?"),
        ("MCU",        0x49, lambda f: f"0x{f[1]:02X}" if len(f) > 1 else "?"),
        ("BOARD",      0x47, lambda f: f"0x{f[1]:02X}" if len(f) > 1 else "?"),
    ]
    for name, cmd, fmt in checks:
        f = received.get(cmd)
        ok = f is not None
        detail = fmt(f) if ok else "no response"
        tests.append((name, ok, detail))

    # Individual queries
    queries = [
        ("STAT_RX",  0x21),
        ("STAT_TX",  0x22),
        ("STAT_GPS", 0x2A),
    ]
    for name, cmd in queries:
        ser.write(bytes([FEND, cmd, 0x00, FEND]))
        f = wait_for_cmd(ser, cmd, timeout=2.0)
        if f:
            if cmd == 0x2A and len(f) >= 3:
                detail = f"fix={f[1]}, sats={f[2]}"
            elif cmd in (0x21, 0x22) and len(f) >= 5:
                detail = f"count={int.from_bytes(f[1:5], 'big')}"
            else:
                detail = f"len={len(f)}"
            tests.append((name, True, detail))
        else:
            tests.append((name, False, "no response"))

    # Periodic stats check
    ser.reset_input_buffer()
    time.sleep(4)
    resp = ser.read(8192)
    stats = {}
    if resp:
        for f in parse_kiss_frames(resp):
            stats[f[0]] = stats.get(f[0], 0) + 1
    periodic_ok = len(stats) > 0
    detail = ", ".join(f"0x{k:02X}({v})" for k, v in sorted(stats.items())) if stats else "none"
    tests.append(("PERIODIC_STATS", periodic_ok, detail))

    ser.close()

    # Results
    passed = sum(1 for _, ok, _ in tests if ok)
    total = len(tests)
    print(f"KISS smoke test on {port}: {passed}/{total} passed")
    for name, ok, detail in tests:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}: {detail}")

    sys.exit(0 if passed == total else 1)

if __name__ == "__main__":
    main()
