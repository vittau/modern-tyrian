#!/usr/bin/env python3
"""Dump the non-gameplay background pictures of Tyrian 2.1 to PNG.

Decodes the pictures stored in tyrian.pic (the 13 RLE pictures selected by
JE_loadPic in src/picload.c) and the 320x200 PCX screens (src/pcxload.c),
applies the correct palette from palette.dat, and writes PNGs plus a
16:9 zoom-and-crop ("Vert-") variant of every picture.

This is an investigation aid: it reimplements the game's loaders so the
assets can be inspected without launching the game.

Usage:
    tools/dump_screens.py --data /path/to/Tyrian --out /path/to/outdir

No paths are hardcoded; --data defaults to $TYRIAN_DATA or ./data.
"""

import argparse
import os
import struct
import sys
import zlib

# src/pcxmast.c: pcxpal[PCX_NUM] = palette index used by each tyrian.pic picture.
PCXPAL = [0, 7, 5, 8, 10, 5, 18, 19, 19, 20, 21, 22, 5]
PCX_NUM = 13


def load_palettes(path):
    """palette.dat: palettesCount * 256 * 3 bytes of 6-bit VGA values."""
    with open(path, "rb") as f:
        data = f.read()
    count = len(data) // (256 * 3)
    pals = []
    for p in range(count):
        pal = []
        base = p * 256 * 3
        for i in range(256):
            r, g, b = data[base + i * 3], data[base + i * 3 + 1], data[base + i * 3 + 2]
            # src/palette.c loadPals(): 6-bit -> 8-bit expansion
            pal.append(((r << 2) | (r >> 4), (g << 2) | (g >> 4), (b << 2) | (b >> 4)))
        pals.append(pal)
    return pals


def rle_decode(data, size):
    """PCX-style RLE used by both picload.c and pcxload.c."""
    out = bytearray()
    i = 0
    n = len(data)
    while len(out) < size and i < n:
        b = data[i]
        i += 1
        if (b & 0xC0) == 0xC0:
            run = b & 0x3F
            if i >= n:
                break
            out.extend([data[i]] * run)
            i += 1
        else:
            out.append(b)
    if len(out) < size:
        out.extend([0] * (size - len(out)))
    return bytes(out[:size])


def decode_tyrian_pic(path):
    """Returns list of (id, 320*200 index bytes) for tyrian.pic."""
    with open(path, "rb") as f:
        data = f.read()
    count = struct.unpack_from("<H", data, 0)[0]
    offsets = list(struct.unpack_from("<%dI" % count, data, 2))
    offsets.append(len(data))  # pcxpos[count] = end of file
    pics = []
    for idx in range(count):
        blob = data[offsets[idx]:offsets[idx + 1]]
        pics.append((idx + 1, rle_decode(blob, 320 * 200)))
    return pics


def decode_pcx(path):
    """Minimal PCX decoder using the trailing 256-colour palette."""
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 128 + 768:
        return None, None
    # header: xmin,xmax,ymin,ymax are little-endian u16 at 2,4,6,8
    xmin, ymin, xmax, ymax = struct.unpack_from("<4H", data, 4)
    w = xmax - xmin + 1
    h = ymax - ymin + 1
    nplanes = data[65]
    bpp = data[3]
    bytes_per_row = data[66] | (data[67] << 8)
    if bpp != 8 or nplanes != 1:
        return None, None
    img = rle_decode(data[128:len(data) - 768], w * h)
    paldata = data[-768:]
    pal = []
    for i in range(256):
        pal.append((paldata[i * 3], paldata[i * 3 + 1], paldata[i * 3 + 2]))
    return (w, h, img), pal


def zoom_crop_16to9(indices, pal, target_w):
    """Nearest-neighbour scale 320x200 so it fills target_w, center-crop to 200 rows."""
    src_w, src_h = 320, 200
    scale = target_w / float(src_w)
    new_h = int(round(src_h * scale))
    crop = (new_h - src_h) // 2
    out = bytearray()
    for y in range(src_h):
        sy = min(src_h - 1, int((y + crop) / scale))
        row = indices[sy * src_w:(sy + 1) * src_w]
        for x in range(target_w):
            sx = min(src_w - 1, int(x / scale))
            out.append(row[sx])
    return bytes(out), target_w, src_h


def write_png(path, width, height, rgb_rows):
    """rgb_rows: flat bytes, width*height*3."""
    def chunk(tag, payload):
        c = struct.pack(">I", len(payload)) + tag + payload
        c += struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        return c

    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)  # filter type 0
        raw.extend(rgb_rows[y * stride:(y + 1) * stride])

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def to_rgb(indices, pal):
    out = bytearray()
    for i in indices:
        r, g, b = pal[i]
        out.append(r)
        out.append(g)
        out.append(b)
    return bytes(out)


def write_edge_strip(path, indices, width, height, pal, band=24, mag=4):
    """Outer `band` columns of the left and right edges, magnified horizontally."""
    strip_w = band * 2 * mag + 4
    rgb = bytearray()
    for y in range(height):
        # left band
        for x in range(band):
            col = pal[indices[y * width + x]]
            rgb.extend(col * mag)
        rgb.extend(bytes([0, 0, 0]) * 2)  # separator
        # right band
        for x in range(width - band, width):
            col = pal[indices[y * width + x]]
            rgb.extend(col * mag)
        rgb.extend(bytes([0, 0, 0]) * 2)
    write_png(path, strip_w, height, bytes(rgb))



def edge_report(name, indices, width, height):
    """Summarise the left/right 8 columns: solid? repeating? unique?"""
    def cols(x0, x1):
        return [indices[y * width + x] for y in range(height) for x in range(x0, x1)]

    def sample(x0, x1):
        # per-row tuple for the given column band
        return [tuple(indices[y * width + x] for x in range(x0, x1)) for y in range(height)]

    left = sample(0, 8)
    right = sample(width - 8, width)
    solid_l = len(set(left)) == 1
    solid_r = len(set(right)) == 1
    uniq_l = len(set(left))
    uniq_r = len(set(right))
    # vertical periodicity: do rows repeat with a small period?
    def period(rows):
        for p in (2, 3, 4, 5, 6, 8, 10, 12, 16):
            if all(rows[i] == rows[i - p] for i in range(p, len(rows))):
                return p
        return 0

    return ("%s: left8 solid=%s uniqrows=%d period=%d | right8 solid=%s uniqrows=%d period=%d"
            % (name, solid_l, uniq_l, period(left), solid_r, uniq_r, period(right)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default=os.environ.get("TYRIAN_DATA", "./data"))
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    pal_path = os.path.join(args.data, "palette.dat")
    pic_path = os.path.join(args.data, "tyrian.pic")
    if not os.path.exists(pal_path) or not os.path.exists(pic_path):
        sys.exit("could not find palette.dat / tyrian.pic in %s" % args.data)

    pals = load_palettes(pal_path)
    os.makedirs(args.out, exist_ok=True)

    # 16:9 canvas width at original pixel aspect: round(200 * 1.2 * 16/9) = 427
    wide_w = 427

    report = []
    for pid, indices in decode_tyrian_pic(pic_path):
        pal = pals[PCXPAL[pid - 1]]
        base = "pic_%02d" % pid
        write_png(os.path.join(args.out, base + ".png"), 320, 200, to_rgb(indices, pal))
        crop, cw, ch = zoom_crop_16to9(indices, pal, wide_w)
        write_png(os.path.join(args.out, base + "_169crop.png"), cw, ch, to_rgb(crop, pal))
        write_edge_strip(os.path.join(args.out, base + "_edges.png"), indices, 320, 200, pal)
        report.append(edge_report("tyrian.pic id=%d (pal %d) %s" % (pid, PCXPAL[pid - 1], base),
                                  indices, 320, 200))

    # PCX screens used by the network setup / non-gameplay UI.
    for fn in sorted(os.listdir(args.data)):
        if not fn.lower().endswith(".pcx"):
            continue
        decoded, _ = decode_pcx(os.path.join(args.data, fn))
        if decoded is None:
            continue
        w, h, indices = decoded
        if w != 320 or h != 200:
            continue
        pal = pals[0]  # overridden by PCX's own palette below
        _, pal = decode_pcx(os.path.join(args.data, fn))
        base = "pcx_" + os.path.splitext(fn)[0]
        write_png(os.path.join(args.out, base + ".png"), w, h, to_rgb(indices, pal))
        crop, cw, ch = zoom_crop_16to9(indices, pal, wide_w)
        write_png(os.path.join(args.out, base + "_169crop.png"), cw, ch, to_rgb(crop, pal))
        report.append(edge_report("PCX %s %s" % (fn, base), indices, w, h))

    with open(os.path.join(args.out, "edges.txt"), "w") as f:
        f.write("\n".join(report) + "\n")
    print("\n".join(report))
    print("wrote PNGs to", args.out)


if __name__ == "__main__":
    main()
