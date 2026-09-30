#!/usr/bin/env python3
"""Turn the game's headless BMP snapshots into the README screenshots.

Usage:  python3 docs/screenshots/render.py BMP_DIR

BMP_DIR must contain hero.bmp, arcade.bmp, deck.bmp, menu.bmp, classic.bmp and modern.bmp as
produced by docs/screenshots/capture.sh.  Writes hero.png, menu.png and
classic-vs-modern.png and savara-deck-16x10.png next to this script.

Nearest-neighbour only, with the original 1.2 pixel aspect (each source pixel
is drawn 1.2x taller than wide), so the screenshots look like the real screen.
Standard library only.
"""
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def read_bmp(path):
    data = open(path, "rb").read()
    assert data[:2] == b"BM", "not a BMP"
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = struct.unpack_from("<i", data, 22)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    bottom_up = height > 0
    height = abs(height)

    if bpp == 24:
        stride = (width * 3 + 3) & ~3
        rows = []
        for y in range(height):
            off = pixel_offset + y * stride
            src = data[off:off + width * 3]
            rows.append([(src[x * 3 + 2], src[x * 3 + 1], src[x * 3]) for x in range(width)])
    elif bpp == 8:
        stride = (width + 3) & ~3
        palette = []
        for i in range(256):
            b, g, r, _ = data[14 + 40 + i * 4:14 + 40 + i * 4 + 4]
            palette.append((r, g, b))
        rows = []
        for y in range(height):
            off = pixel_offset + y * stride
            rows.append([palette[data[off + x]] for x in range(width)])
    else:
        raise SystemExit("unsupported BMP depth: %d" % bpp)

    if bottom_up:
        rows.reverse()
    return width, height, rows


def scale(rows, w, h, out_w, out_h=None):
    """Nearest-neighbour scale to out_w px wide, 1.2 pixel aspect."""
    sx = out_w / w
    sy = sx * 1.2
    out_h = int(round(h * sy)) if out_h is None else out_h
    sy = out_h / h
    out = []
    for oy in range(out_h):
        src = rows[min(h - 1, int(oy / sy))]
        out.append([src[min(w - 1, int(ox / sx))] for ox in range(out_w)])
    return out, out_h


def write_png(path, width, height, rows):
    raw = bytearray()
    for row in rows:
        raw.append(0)
        for r, g, b in row:
            raw += bytes((r, g, b))

    def chunk(tag, payload):
        c = struct.pack(">I", len(payload)) + tag + payload
        return c + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)
    print("%s  %dx%d  %.0f KB" % (path, width, height, os.path.getsize(path) / 1024))


def main():
    bmp_dir = sys.argv[1] if len(sys.argv) > 1 else HERE
    hero = read_bmp(os.path.join(bmp_dir, "hero.bmp"))
    arcade = read_bmp(os.path.join(bmp_dir, "arcade.bmp"))
    menu = read_bmp(os.path.join(bmp_dir, "menu.bmp"))
    classic = read_bmp(os.path.join(bmp_dir, "classic.bmp"))
    modern = read_bmp(os.path.join(bmp_dir, "modern.bmp"))

    # Full-window shots: the Modern canvas is 427x200 logical pixels.
    for name, (w, h, rows) in (("hero.png", hero), ("arcade.png", arcade), ("menu.png", menu)):
        out, oh = scale(rows, w, h, 1280, 720)
        write_png(os.path.join(HERE, name), 1280, oh, out)

    w, h, rows = read_bmp(os.path.join(bmp_dir, "deck.bmp"))
    out, oh = scale(rows, w, h, 1280, 800)
    write_png(os.path.join(HERE, "savara-deck-16x10.png"), 1280, oh, out)

    # Classic 4:3 vs Modern 16:9, same demo frame, same display scale.
    sw = 854  # modern side width (2x)
    cw = int(round(320 * sw / 427))  # classic side width, same px size and PAR
    c_rows, c_h = scale(classic[2], classic[0], classic[1], cw)
    m_rows, m_h = scale(modern[2], modern[0], modern[1], sw)
    gap = 14
    W, H = cw + gap + sw, max(c_h, m_h)
    canvas = [[(0, 0, 0)] * W for _ in range(H)]
    for y in range(c_h):
        canvas[y][0:cw] = c_rows[y]
    for y in range(m_h):
        canvas[y][cw + gap:W] = m_rows[y]
    write_png(os.path.join(HERE, "classic-vs-modern.png"), W, H, canvas)


if __name__ == "__main__":
    main()
