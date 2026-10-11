#!/usr/bin/env python3
"""Write 8-bit RGBA pixels as an opaque 24-bit BMP composited onto black."""
import struct
import sys


def main():
    if len(sys.argv) != 5:
        raise SystemExit("usage: rgba-to-bmp.py INPUT OUTPUT WIDTH HEIGHT")
    source, destination = sys.argv[1:3]
    width, height = map(int, sys.argv[3:5])
    with open(source, "rb") as image:
        rgba = image.read()
    if width <= 0 or height <= 0 or len(rgba) != width * height * 4:
        raise SystemExit("invalid RGBA dimensions or input length")

    dib_size = 40
    pixel_offset = 14 + dib_size
    row_stride = (width * 3 + 3) & ~3
    image_size = row_stride * height
    file_header = struct.pack("<2sIHHI", b"BM", pixel_offset + image_size, 0, 0,
                              pixel_offset)
    info_header = struct.pack("<IiiHHIIiiII", dib_size, width, height, 1, 24, 0,
                              image_size, 2835, 2835, 0, 0)
    pixels = bytearray(image_size)
    for y in range(height):
        source_row = y * width * 4
        target_row = (height - y - 1) * row_stride
        for x in range(width):
            r, g, b, a = rgba[source_row + x * 4:source_row + x * 4 + 4]
            offset = target_row + x * 3
            pixels[offset:offset + 3] = bytes((b * a // 255, g * a // 255, r * a // 255))
    with open(destination, "wb") as output:
        output.write(file_header + info_header + pixels)


if __name__ == "__main__":
    main()
