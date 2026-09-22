#!/usr/bin/env python3
"""Turns a raw MiSTer framebuffer dump into a PNG so the interface can be reviewed
without looking at the television.

Usage: fbshot.py <raw-file> <png-file> [width] [height]
"""

import struct
import sys
import zlib


def raw_to_png(raw_path, png_path, width=1920, height=1080, stride=None):
    stride = stride or width * 4
    with open(raw_path, "rb") as handle:
        data = handle.read()

    expected = stride * height
    if len(data) < expected:
        raise SystemExit(f"short dump: {len(data)} bytes, expected {expected}")

    rows = bytearray()
    for y in range(height):
        base = y * stride
        rows.append(0)  # PNG filter type: none
        line = data[base:base + width * 4]
        # Framebuffer is XRGB little-endian (B,G,R,X); PNG wants RGB.
        rows.extend(b"".join(bytes((line[i + 2], line[i + 1], line[i]))
                             for i in range(0, len(line), 4)))

    def chunk(tag, payload):
        body = tag + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", header)
           + chunk(b"IDAT", zlib.compress(bytes(rows), 6))
           + chunk(b"IEND", b""))

    with open(png_path, "wb") as handle:
        handle.write(png)

    print(f"wrote {png_path} ({width}x{height}, {len(png) // 1024} KiB)")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)

    w = int(sys.argv[3]) if len(sys.argv) > 3 else 1920
    h = int(sys.argv[4]) if len(sys.argv) > 4 else 1080
    raw_to_png(sys.argv[1], sys.argv[2], w, h)
