#!/usr/bin/env python3
"""
R-Watch Screenshot — capture display via USB serial

Sends trigger bytes, firmware dumps shadow framebuffer as raw RGB565.
Handles KISS protocol data interleaved in the stream.

Usage:
    ./scripts/screenshot.py                         # default port + output
    ./scripts/screenshot.py -p /dev/ttyACM4         # specify port
    ./scripts/screenshot.py -o /tmp/watch.png       # specify output
"""

import argparse
import struct
import sys
import time

WIDTH = 410
HEIGHT = 502
PIXEL_BYTES = WIDTH * HEIGHT * 2  # 411,640 bytes
MAGIC = b"RWSS"
HEADER_SIZE = 8  # RWSS + uint16 width + uint16 height


def capture(port, output_path):
    try:
        import serial
    except ImportError:
        sys.exit("pip install pyserial")
    try:
        from PIL import Image
    except ImportError:
        sys.exit("pip install Pillow")

    s = serial.Serial(port, 115200, timeout=1)

    # Drain any pending data
    time.sleep(0.2)
    s.reset_input_buffer()

    # Send trigger
    s.write(MAGIC)
    s.flush()

    # Scan stream for the magic response header
    # The firmware may send KISS frames before/after the screenshot data
    buf = b""
    deadline = time.time() + 10
    magic_idx = -1

    while time.time() < deadline:
        chunk = s.read(max(1, s.in_waiting))
        if not chunk:
            continue
        buf += chunk

        magic_idx = buf.find(MAGIC)
        if magic_idx >= 0 and len(buf) >= magic_idx + HEADER_SIZE:
            break

    if magic_idx < 0:
        s.close()
        sys.exit(f"Magic not found in {len(buf)} bytes of response")

    # Parse header
    hdr = buf[magic_idx:magic_idx + HEADER_SIZE]
    w, h = struct.unpack("<HH", hdr[4:8])
    expected = w * h * 2

    # Collect pixel data (may already have some in buf)
    data = buf[magic_idx + HEADER_SIZE:]
    while len(data) < expected and time.time() < deadline:
        chunk = s.read(min(expected - len(data), 32768))
        if chunk:
            data += chunk

    s.close()

    received = len(data)
    if received < expected:
        print(f"Warning: got {received}/{expected} bytes ({received*100//expected}%)")

    # Convert RGB565 LE to PNG
    img = Image.new("RGB", (w, h))
    pixels = img.load()
    npx = min(w * h, received // 2)
    for i in range(npx):
        pixel = struct.unpack_from("<H", data, i * 2)[0]
        r = ((pixel >> 11) & 0x1F) * 255 // 31
        g = ((pixel >> 5) & 0x3F) * 255 // 63
        b = (pixel & 0x1F) * 255 // 31
        pixels[i % w, i // w] = (r, g, b)

    img.save(output_path)
    print(f"Saved: {output_path} ({w}x{h}, {npx} pixels, {received} bytes)")


def main():
    parser = argparse.ArgumentParser(description="R-Watch screenshot")
    parser.add_argument("-p", "--port", default="/dev/ttyACM4")
    parser.add_argument("-o", "--output", default="/tmp/watch_screenshot.png")
    args = parser.parse_args()
    capture(args.port, args.output)


if __name__ == "__main__":
    main()
