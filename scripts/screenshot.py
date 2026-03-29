#!/usr/bin/env python3
"""
R-Watch Remote Debug Tool

Commands:
    screenshot [-o file.png]     Capture display screenshot
    metrics                      Show frame timing and memory stats
    touch <x> <y> [duration_ms]  Inject touch at coordinates
    swipe <direction>            Swipe up/down/left/right
    navigate <screen>            Jump to: watch, radio, gps, messages, settings
    invalidate                   Force full screen redraw

Usage:
    ./scripts/screenshot.py screenshot
    ./scripts/screenshot.py metrics
    ./scripts/screenshot.py touch 200 250
    ./scripts/screenshot.py swipe down
    ./scripts/screenshot.py navigate radio
"""

import argparse
import struct
import sys
import time

WIDTH = 410
HEIGHT = 502
PREFIX = b"RWS"
DEFAULT_PORT = "/dev/ttyACM4"

TILES = {
    "watch":    (1, 1),
    "radio":    (1, 0),
    "gps":      (0, 1),
    "messages": (2, 1),
    "settings": (1, 2),
}

SWIPES = {
    "down":  [(205, 400), (205, 100)],  # swipe up on screen → show tile below
    "up":    [(205, 100), (205, 400)],
    "left":  [(350, 250), (60, 250)],
    "right": [(60, 250), (350, 250)],
}


def get_serial(port):
    try:
        import serial
    except ImportError:
        sys.exit("pip install pyserial")
    s = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)
    s.reset_input_buffer()
    return s


def send_cmd(s, cmd_byte, payload=b""):
    s.write(PREFIX + bytes([cmd_byte]) + payload)
    s.flush()


def cmd_screenshot(s, output):
    try:
        from PIL import Image
    except ImportError:
        sys.exit("pip install Pillow")

    send_cmd(s, ord('S'))

    # Scan for response header
    buf = b""
    deadline = time.time() + 10
    while time.time() < deadline:
        chunk = s.read(max(1, s.in_waiting or 1))
        if chunk:
            buf += chunk
        magic = PREFIX + b"S"
        idx = buf.find(magic)
        if idx >= 0 and len(buf) >= idx + 8:
            break
    else:
        sys.exit(f"Timeout ({len(buf)} bytes, no header)")

    hdr = buf[idx:idx + 8]
    w, h = struct.unpack("<HH", hdr[4:8])
    if w == 0:
        sys.exit("Screenshot buffer not allocated")

    expected = w * h * 2
    data = buf[idx + 8:]
    while len(data) < expected and time.time() < deadline:
        chunk = s.read(min(expected - len(data), 32768))
        if chunk:
            data += chunk

    img = Image.new("RGB", (w, h))
    pixels = img.load()
    for i in range(min(w * h, len(data) // 2)):
        pixel = struct.unpack_from(">H", data, i * 2)[0]
        r = ((pixel >> 11) & 0x1F) * 255 // 31
        g = ((pixel >> 5) & 0x3F) * 255 // 63
        b = (pixel & 0x1F) * 255 // 31
        pixels[i % w, i // w] = (r, g, b)

    img.save(output)
    print(f"Saved: {output} ({w}x{h}, {len(data)} bytes)")


def cmd_metrics(s):
    send_cmd(s, ord('M'))
    buf = b""
    deadline = time.time() + 3
    while time.time() < deadline:
        chunk = s.read(max(1, s.in_waiting or 1))
        if chunk:
            buf += chunk
        magic = PREFIX + b"M"
        idx = buf.find(magic)
        if idx >= 0:
            # Find the JSON after the header
            json_start = idx + 4
            nl = buf.find(b"\n", json_start)
            if nl >= 0:
                print(buf[json_start:nl].decode())
                return
    print(f"Timeout ({len(buf)} bytes)")


def cmd_touch(s, x, y, duration_ms=200):
    dur = max(1, duration_ms // 100)
    payload = struct.pack("<HHB", x, y, dur)
    send_cmd(s, ord('T'), payload)
    print(f"Touch injected: ({x}, {y}) for {dur*100}ms")


def cmd_swipe(s, direction):
    if direction not in SWIPES:
        sys.exit(f"Unknown direction: {direction}. Use: {', '.join(SWIPES)}")
    points = SWIPES[direction]
    # Press at start, hold briefly, then move to end
    cmd_touch(s, points[0][0], points[0][1], 400)
    time.sleep(0.15)
    cmd_touch(s, points[1][0], points[1][1], 300)
    print(f"Swipe {direction}")


def cmd_navigate(s, screen):
    if screen not in TILES:
        sys.exit(f"Unknown screen: {screen}. Use: {', '.join(TILES)}")
    col, row = TILES[screen]
    send_cmd(s, ord('N'), bytes([col, row]))
    print(f"Navigate → {screen} ({col},{row})")


def cmd_invalidate(s):
    send_cmd(s, ord('I'))
    print("Invalidated — full redraw requested")


def cmd_files(s):
    """List files on SD card"""
    send_cmd(s, ord('F'))
    buf = b""
    deadline = time.time() + 10
    while time.time() < deadline:
        chunk = s.read(max(1, s.in_waiting or 1))
        if chunk:
            buf += chunk
        magic = PREFIX + b"F"
        idx = buf.find(magic)
        if idx >= 0 and b"]}" in buf[idx:]:
            end = buf.find(b"]}", idx) + 2
            print(buf[idx + 4:end].decode())
            return
    print(f"Timeout ({len(buf)} bytes)")


def cmd_log(s):
    send_cmd(s, ord('L'))
    buf = b""
    deadline = time.time() + 5
    while time.time() < deadline:
        chunk = s.read(max(1, s.in_waiting or 1))
        if chunk:
            buf += chunk
        magic = PREFIX + b"L"
        idx = buf.find(magic)
        if idx >= 0:
            nl = buf.find(b"\n", idx + 4)
            if nl >= 0:
                print(buf[idx + 4:nl].decode())
                return
    print(f"Timeout ({len(buf)} bytes)")


def main():
    parser = argparse.ArgumentParser(description="R-Watch remote debug")
    parser.add_argument("-p", "--port", default=DEFAULT_PORT)
    sub = parser.add_subparsers(dest="command")

    ss = sub.add_parser("screenshot", aliases=["ss"])
    ss.add_argument("-o", "--output", default="/tmp/watch_screenshot.png")

    sub.add_parser("metrics", aliases=["m"])

    t = sub.add_parser("touch", aliases=["t"])
    t.add_argument("x", type=int)
    t.add_argument("y", type=int)
    t.add_argument("duration", type=int, nargs="?", default=200)

    sw = sub.add_parser("swipe", aliases=["sw"])
    sw.add_argument("direction", choices=["up", "down", "left", "right"])

    n = sub.add_parser("navigate", aliases=["nav", "n"])
    n.add_argument("screen", choices=list(TILES.keys()))

    sub.add_parser("invalidate", aliases=["inv"])

    sub.add_parser("log", aliases=["l"],
                    help="Toggle IMU logging to SD card")

    sub.add_parser("files", aliases=["f"],
                    help="List files on SD card")

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        return

    s = get_serial(args.port)
    try:
        if args.command in ("screenshot", "ss"):
            cmd_screenshot(s, args.output)
        elif args.command in ("metrics", "m"):
            cmd_metrics(s)
        elif args.command in ("touch", "t"):
            cmd_touch(s, args.x, args.y, args.duration)
        elif args.command in ("swipe", "sw"):
            cmd_swipe(s, args.direction)
        elif args.command in ("navigate", "nav", "n"):
            cmd_navigate(s, args.screen)
        elif args.command in ("invalidate", "inv"):
            cmd_invalidate(s)
        elif args.command in ("log", "l"):
            cmd_log(s)
        elif args.command in ("files", "f"):
            cmd_files(s)
    finally:
        s.close()


if __name__ == "__main__":
    main()
