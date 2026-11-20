#!/usr/bin/env python3
"""
Print the bounding box of non-background pixels in an XWD dump:
    "<window_w> <window_h> <bbox_w> <bbox_h>"

Used by tests/smoke.sh to check that text grows when the x3179 window
is enlarged.  Handles the ZPixmap 24/32-bit dumps xwd makes on
TrueColor visuals (Xvfb, XQuartz); no PIL needed.

Usage: xwd_bbox.py FILE.xwd
       xwd_bbox.py --count RRGGBB FILE.xwd   (number of pixels of colour)
"""
import struct
import sys


def count_colour(path, rgb):
    """Count pixels whose 24-bit value equals rgb."""
    (header_size, width, height, byte_order, bpp, bytes_per_line,
     ncolors) = read_header(path)
    data = open(path, "rb").read()
    pix = data[header_size + 12 * ncolors:]
    order = "<" if byte_order == 0 else ">"
    n = 0
    for y in range(height):
        row = struct.unpack_from(order + "%dI" % width, pix,
                                 y * bytes_per_line)
        n += sum(1 for p in row if (p & 0xFFFFFF) == rgb)
    print(n)


def read_header(path):
    data = open(path, "rb").read(100)
    f = struct.unpack(">25I", data)
    if f[11] != 32:
        sys.exit("unsupported bits_per_pixel %d" % f[11])
    # header_size, width, height, byte_order, bpp, bytes_per_line, ncolors
    return f[0], f[4], f[5], f[7], f[11], f[12], f[19]


def main(path):
    data = open(path, "rb").read()
    # XWD header: 25 big-endian CARD32 fields, then the window name.
    (header_size, _ver, _fmt, _depth, width, height, _xoff, byte_order,
     _unit, _bit_order, _pad, bpp, bytes_per_line, _vclass,
     _rmask, _gmask, _bmask, _bits_rgb, _cmap_entries, ncolors,
     _ww, _wh, _wx, _wy, _bw) = struct.unpack(">25I", data[:100])
    if bpp != 32:
        sys.exit("unsupported bits_per_pixel %d" % bpp)

    # pixel data follows the header and ncolors 12-byte XWDColor records
    pix = data[header_size + 12 * ncolors:]
    order = "<" if byte_order == 0 else ">"   # LSBFirst = 0

    # background is the pixel in the top-left corner (inside the margin)
    bg = struct.unpack_from(order + "I", pix, 0)[0] & 0xFFFFFF

    min_x, min_y, max_x, max_y = width, height, -1, -1
    for y in range(height):
        row = struct.unpack_from(order + "%dI" % width, pix,
                                 y * bytes_per_line)
        xs = [x for x, p in enumerate(row) if (p & 0xFFFFFF) != bg]
        if xs:
            min_y = min(min_y, y)
            max_y = y
            min_x = min(min_x, xs[0])
            max_x = max(max_x, xs[-1])
    if max_x < 0:
        print(width, height, 0, 0)
    else:
        print(width, height, max_x - min_x + 1, max_y - min_y + 1)


if __name__ == "__main__":
    if sys.argv[1] == "--count":
        count_colour(sys.argv[3], int(sys.argv[2], 16))
    else:
        main(sys.argv[1])
