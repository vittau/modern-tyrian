#!/usr/bin/env python3
"""Synthetic fixtures for tools/check_installer.sh.

Everything here is generated from a fixed seed: random bytes shaped just enough
(header counts, palette size) to pass the engine's structural check of a
Tyrian 2000 directory.  No original game data, and nothing derived from it.

Usage: installer_fixtures.py OUTDIR
Writes into OUTDIR: zips (*.zip), folders (folder-*), and a test spec for the
installer (*.spec: archive-size, archive-sha256 and a "size crc name" manifest).
"""
import hashlib
import os
import random
import struct
import sys
import zipfile

# The files game_data.c requires of a Tyrian 2000 directory, and the header
# counts it checks (game_schema.c, 2000 column).
NAMES = [
    "tyrian.shp", "tyrian.hdt", "tyrian.pic", "palette.dat", "tyrian.snd", "voices.snd",
    "music.mus", "tyrian.cdt", "estsc.shp",
    "tyrian1.lvl", "tyrian2.lvl", "tyrian3.lvl", "tyrian4.lvl", "tyrian5.lvl",
    "levels1.dat", "levels2.dat", "levels3.dat", "levels4.dat", "levels5.dat",
    "cubetxt1.dat", "cubetxt2.dat", "cubetxt3.dat", "cubetxt4.dat", "cubetxt5.dat",
]
SHAPE_BANKS_2000, SHAPE_BANKS_21 = 13, 12
PICTURES, SOUNDS, PALETTES = 14, 31, 24

CKSUM_TABLE = []
for i in range(256):
    v = i << 24
    for _ in range(8):
        v = ((v << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if v & 0x80000000 else (v << 1) & 0xFFFFFFFF
    CKSUM_TABLE.append(v)


def cksum(data):
    crc = 0
    for b in data:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ CKSUM_TABLE[(crc >> 24) ^ b]
    n = len(data)
    while n:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ CKSUM_TABLE[(crc >> 24) ^ (n & 0xFF)]
        n >>= 8
    return ~crc & 0xFFFFFFFF


def make_files(rng, shape_banks=SHAPE_BANKS_2000):
    files = {}
    for name in NAMES:
        body = bytes(rng.randrange(256) for _ in range(rng.randrange(200, 900)))
        if name == "tyrian.shp":
            body = struct.pack("<H", shape_banks) + body
        elif name == "tyrian.pic":
            body = struct.pack("<H", PICTURES) + body
        elif name == "tyrian.snd":
            body = struct.pack("<H", SOUNDS) + body
        elif name == "palette.dat":
            body = bytes(rng.randrange(64) for _ in range(PALETTES * 256 * 3))
        files[name] = body
    return files


def write_spec(path, archive, files):
    data = open(archive, "rb").read() if archive else b""
    with open(path, "w") as out:
        out.write("# synthetic test spec: not Tyrian 2000\n")
        if archive:
            out.write("archive-size %d\n" % len(data))
            out.write("archive-sha256 %s\n" % hashlib.sha256(data).hexdigest())
        for name, body in files.items():
            out.write("%d %d %s\n" % (len(body), cksum(body), name))


def build_zip(path, files, top="tyrian2000", extra=(), compress=True):
    with zipfile.ZipFile(path, "w") as z:
        z.writestr(zipfile.ZipInfo(top + "/"), b"")
        for i, (name, body) in enumerate(files.items()):
            info = zipfile.ZipInfo(top + "/" + name, (2000, 1, 1, 0, 0, 0))
            info.external_attr = 0o100644 << 16
            info.create_system = 3
            method = zipfile.ZIP_DEFLATED if compress and i % 2 == 0 else zipfile.ZIP_STORED
            info.compress_type = method
            z.writestr(info, body)
        # Files the installer ignores (a readme and an executable) still get checked.
        z.writestr(top + "/readme.txt", b"synthetic readme\n")
        z.writestr(top + "/tyrian2.exe", b"MZ synthetic\n")
        for name, body, mode in extra:
            info = zipfile.ZipInfo(name, (2000, 1, 1, 0, 0, 0))
            info.external_attr = mode << 16
            info.create_system = 3
            info.compress_type = zipfile.ZIP_STORED
            z.writestr(info, body)


def write_folder(path, files, case=lambda n: n):
    os.makedirs(path, exist_ok=True)
    for name, body in files.items():
        with open(os.path.join(path, case(name)), "wb") as f:
            f.write(body)


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    rng = random.Random(20260929)
    files = make_files(rng)

    # A valid archive, with its spec (the canonical one for these tests).
    build_zip(os.path.join(out, "valid.zip"), files)
    write_spec(os.path.join(out, "valid.spec"), os.path.join(out, "valid.zip"), files)

    # Same archive and manifest, but the spec names the wrong SHA-256.
    spec = open(os.path.join(out, "valid.spec")).read().splitlines()
    with open(os.path.join(out, "wrong-sha.spec"), "w") as f:
        for line in spec:
            if line.startswith("archive-sha256"):
                line = "archive-sha256 " + "0" * 64
            f.write(line + "\n")

    # A stored (uncompressed) entry with one flipped byte: a bad CRC in a
    # structurally fine archive.  The spec matches this exact file, so only the
    # entry CRC can catch it.
    corrupt = os.path.join(out, "badcrc.zip")
    build_zip(corrupt, files, compress=False)
    data = bytearray(open(corrupt, "rb").read())
    marker = files["estsc.shp"][:32]
    at = bytes(data).find(marker)
    assert at > 0
    data[at + 8] ^= 0xFF
    open(corrupt, "wb").write(bytes(data))
    write_spec(os.path.join(out, "badcrc.spec"), corrupt, files)

    valid = open(os.path.join(out, "valid.zip"), "rb").read()
    open(os.path.join(out, "truncated.zip"), "wb").write(valid[: len(valid) // 2])
    open(os.path.join(out, "garbage.zip"), "wb").write(bytes(rng.randrange(256) for _ in range(5000)))
    for label in ("truncated", "garbage"):
        write_spec(os.path.join(out, label + ".spec"), os.path.join(out, label + ".zip"), files)

    # CRC errors must also be caught in files the engine doesn't consume.
    ignored = os.path.join(out, "badcrc-ignored.zip")
    build_zip(ignored, files, compress=False)
    data = bytearray(open(ignored, "rb").read())
    at = bytes(data).find(b"synthetic readme\n")
    assert at > 0
    data[at] ^= 0xFF
    open(ignored, "wb").write(data)
    write_spec(os.path.join(out, "badcrc-ignored.spec"), ignored, files)

    unsafe = {
        "dotdot": [("tyrian2000/../evil.txt", b"x", 0o100644)],
        "absolute": [("/tmp/tyrian2000-installer-evil.txt", b"x", 0o100644)],
        "symlink": [("tyrian2000/link.dat", b"tyrian1.lvl", 0o120777)],
        "duplicate": [("tyrian2000/TYRIAN1.LVL", b"x", 0o100644)],
        "backslash": [("tyrian2000\\evil.txt", b"x", 0o100644)],
        "second-top": [("other/tyrian1.lvl", b"x", 0o100644)],
        "root-file": [("stray.txt", b"x", 0o100644)],
        "nul": [("tyrian2000/evil-nul.txt", b"x", 0o100644)],
    }
    for label, extra in unsafe.items():
        p = os.path.join(out, "unsafe-%s.zip" % label)
        build_zip(p, files, extra=extra)
        if label == "nul":
            data = open(p, "rb").read().replace(b"evil-nul.txt", b"evil\x00ul.txt")
            open(p, "wb").write(data)
        write_spec(os.path.join(out, "unsafe-%s.spec" % label), p, files)

    # Oversize claims need no large payload: alter the directory metadata.
    huge = bytearray(valid)
    at = huge.find(b"PK\x01\x02")
    at = huge.find(b"PK\x01\x02", at + 4)  # first file, not the directory entry
    assert at >= 0
    struct.pack_into("<I", huge, at + 24, 64 * 1024 * 1024 + 1)
    p = os.path.join(out, "oversize.zip")
    open(p, "wb").write(huge)
    write_spec(os.path.join(out, "oversize.spec"), p, files)

    # Manual folders.  GOG-like: deeper, upper-case names.
    write_folder(os.path.join(out, "folder-plain"), files)
    write_folder(os.path.join(out, "folder-gog", "Tyrian 2000", "game"), files, case=str.upper)
    changed = dict(files)
    changed["tyrian.cdt"] = files["tyrian.cdt"] + b"gog-build"
    write_folder(os.path.join(out, "folder-noncanonical", "GOG Games", "Tyrian 2000"), changed)
    incomplete = {k: v for k, v in files.items() if k != "levels3.dat"}
    write_folder(os.path.join(out, "folder-incomplete"), incomplete)
    write_folder(os.path.join(out, "folder-empty"), {})
    write_folder(os.path.join(out, "folder-v21"), make_files(random.Random(21), SHAPE_BANKS_21))
    write_spec(os.path.join(out, "folders.spec"), None, files)

    # A spec with a cancel request, for the slow-curl test.
    with open(os.path.join(out, "cancel.spec"), "w") as f:
        f.write(open(os.path.join(out, "valid.spec")).read())
        f.write("cancel-after-ms 700\n")


if __name__ == "__main__":
    main()
