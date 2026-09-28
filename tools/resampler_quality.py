#!/usr/bin/env python3
"""Measure the quality of the in-tree resampler (src/resampler.c).

Compares, for the 11025 -> 44100 Hz case the game uses:

  * SDL3's resampler (the previous behaviour), via SDL_ConvertAudioSamples;
  * the discarded linear interpolation;
  * the new band-limited polyphase resampler in src/resampler.c.

It reports passband ripple, the -0.5/-1/-3 dB points, the stopband maximum
from the input Nyquist (5.5 kHz, where the first image starts) and the image
band maximum, from a centred impulse.  It also A/Bs a few real sound effects
against SDL's output (RMS difference and SNR) and times the conversion of all
38 sounds (load-time cost).

Run from the repository root:  python3 tools/resampler_quality.py
Requires the SDL3 shared library and a C compiler on PATH.
"""

import ctypes
import math
import os
import struct
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIN, FOUT = 11025, 44100
AUDIO_S8, AUDIO_S16 = 0x8008, 0x8010


class Spec(ctypes.Structure):
    _fields_ = [("format", ctypes.c_uint16), ("channels", ctypes.c_int), ("freq", ctypes.c_int)]


def load_sdl():
    for name in ("libSDL3.dylib", "libSDL3.so.0", "libSDL3.so", "SDL3.dll"):
        try:
            lib = ctypes.CDLL(name)
            break
        except OSError:
            continue
    else:
        raise RuntimeError("SDL3 shared library not found")
    lib.SDL_free.argtypes = [ctypes.c_void_p]
    lib.SDL_ConvertAudioSamples.restype = ctypes.c_bool
    lib.SDL_ConvertAudioSamples.argtypes = [
        ctypes.POINTER(Spec), ctypes.POINTER(ctypes.c_uint8), ctypes.c_int,
        ctypes.POINTER(Spec), ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_int)]
    return lib


def sdl_convert(lib, data, fin=FIN, fout=FOUT):
    src, dst = Spec(AUDIO_S8, 1, fin), Spec(AUDIO_S16, 1, fout)
    buf = (ctypes.c_uint8 * len(data))(*data)
    out, outlen = ctypes.c_void_p(), ctypes.c_int(0)
    if not lib.SDL_ConvertAudioSamples(ctypes.byref(src), buf, len(data),
                                       ctypes.byref(dst), ctypes.byref(out), ctypes.byref(outlen)):
        raise RuntimeError("SDL_ConvertAudioSamples failed")
    arr = [ctypes.cast(out, ctypes.POINTER(ctypes.c_int16))[i] for i in range(outlen.value // 2)]
    lib.SDL_free(out)
    return arr


def build_ours():
    tmp = tempfile.mkdtemp(prefix="resampler-quality-")
    libpath = os.path.join(tmp, "libresampler.dylib")
    subprocess.check_call(["cc", "-O2", "-dynamiclib", "-o", libpath,
                           os.path.join(ROOT, "src", "resampler.c")])
    lib = ctypes.CDLL(libpath)
    lib.resampler_convert.restype = ctypes.POINTER(ctypes.c_int16)
    lib.resampler_convert.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                                      ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_size_t)]
    libc = ctypes.CDLL(None)
    libc.free.argtypes = [ctypes.c_void_p]
    return lib, libc


def ours_convert(lib, libc, data, fin=FIN, fout=FOUT):
    buf = (ctypes.c_uint8 * len(data))(*data) if data else (ctypes.c_uint8 * 1)()
    n = ctypes.c_size_t(0)
    ptr = lib.resampler_convert(buf, len(data), fin, fout, ctypes.byref(n))
    if not ptr:
        return []
    arr = [ptr[i] for i in range(n.value // 2)]
    libc.free(ptr)
    return arr


def linear_convert(data, fin=FIN, fout=FOUT):
    out_frames = len(data) * fout // fin
    out = []
    for j in range(out_frames):
        pos = j * fin * 65536 // fout
        k, frac = pos >> 16, pos & 0xffff
        s0 = (data[k] - 256 if data[k] > 127 else data[k]) << 8
        s1 = s0 if k + 1 >= len(data) else ((data[k + 1] - 256 if data[k + 1] > 127 else data[k + 1]) << 8)
        out.append(s0 + ((s1 - s0) * frac >> 16))
    return out


def response(y, center, fout, f):
    p = min(len(y) // 2 - 2, center, len(y) - 1 - center)
    w = 2 * math.pi * f / fout
    re, im = float(y[center]), 0.0
    for m in range(1, p + 1):
        re += (y[center + m] + y[center - m]) * math.cos(w * m)
        im -= (y[center + m] - y[center - m]) * math.sin(w * m)
    return math.hypot(re, im)


def metrics(y, center, fout=FOUT):
    dc = response(y, center, fout, 0.0)

    def hn(f):
        return response(y, center, fout, f) / dc

    pb = [hn(f) for f in range(0, 4501, 25)]
    ripple = 20 * math.log10(max(pb)) - 20 * math.log10(min(pb))

    def edge(lim):
        for f in range(0, 8000, 5):
            if hn(f) < lim:
                return f
        return None

    stop = max(hn(f) for f in range(5513, 22051, 25))
    image = max(hn(f) for f in range(5513, 11026, 25))
    return ripple, edge(10 ** (-0.5 / 20)), edge(10 ** (-1 / 20)), edge(10 ** (-3 / 20)), \
        20 * math.log10(stop), 20 * math.log10(image)


def parse_snd(path, count, trim):
    data = open(path, "rb").read()
    n = min(struct.unpack_from("<H", data, 0)[0], count)
    pos = [struct.unpack_from("<I", data, 2 + 4 * i)[0] for i in range(n)]
    pos.append(len(data))
    sounds = []
    for i in range(n):
        size = pos[i + 1] - pos[i] if pos[i + 1] > pos[i] else 0
        if trim:
            size = size - 100 if size >= 100 else 0
        sounds.append(list(data[pos[i]:pos[i] + size]))
    return sounds


def design_taps(fin=FIN, fout=FOUT):
    """Replicate src/resampler.c's integer taps (parsed from the generated
    table) so the filter's own response can be measured without the 16-bit
    output quantisation."""
    import re
    txt = open(os.path.join(ROOT, "src", "resampler_table.h")).read()
    kernel = [int(x) for x in re.findall(r"-?\d+", txt.split("{", 1)[1].rsplit("}", 1)[0])]
    half, sub, scale = 48, 64, 1 << 20
    g = math.gcd(fin, fout)
    fi, fo = fin // g, fout // g
    taps, offset = 2 * half, half - 1
    phases = []
    for p in range(fo):
        c = []
        for i in range(taps):
            u = p * fi - (i - offset) * fo
            idx = abs(u) * sub
            q, r = idx // fo, idx % fo
            c.append(kernel[half * sub] if q >= half * sub else kernel[q] + (kernel[q + 1] - kernel[q]) * r // fo)
        s = sum(c)
        c = [(x * scale + (s // 2 if x >= 0 else -(s // 2))) // s for x in c]
        phases.append(c)
    return phases, taps, offset, fo, fi, scale


def design_impulse(phases, taps, offset, fo, fi, scale, m=4096):
    k = m // 2
    x = [0] * m
    x[k] = 1
    nout = m * FOUT // FIN
    y = []
    for n in range(nout):
        q, p = n // fo, n % fo
        base, acc = q * fi, 0
        c = phases[p]
        for i in range(taps):
            kk = base + i - offset
            if 0 <= kk < m:
                acc += c[i] * x[kk]
        y.append(acc / scale)
    return y, k * FOUT // FIN


def main():
    sdl = load_sdl()
    ours, libc = build_ours()

    m, k = 4096, 2048
    impulse = [0] * m
    impulse[k] = 127
    c = k * FOUT // FIN

    phases, taps, offset, fo, fi, scale = design_taps()
    design_y, design_c = design_impulse(phases, taps, offset, fo, fi, scale, m)

    print("== frequency response (11025 -> 44100 Hz, centred impulse) ==")
    print("%-16s %10s %8s %8s %8s %12s %14s" %
          ("filter", "ripple", "-0.5dB", "-1dB", "-3dB", "stop>5.5k", "image5.5-11k"))
    rows = (("SDL", sdl_convert(sdl, impulse), c),
            ("linear", linear_convert(impulse), c),
            ("new (S16 out)", ours_convert(ours, libc, impulse), c),
            ("new (filter taps)", design_y, design_c))
    for name, y, ctr in rows:
        r = metrics(y, ctr)
        print("%-16s %8.3f dB %7d %8d %8d %10.1f dB %11.1f dB" %
              (name, r[0], r[1], r[2], r[3], r[4], r[5]))
    print("  (the 'new (S16 out)' stopband is limited by the 16-bit output")
    print("   quantisation; 'new (filter taps)' is the filter itself.)")

    print()
    print("== A/B on real sound effects (vs SDL, 11025 -> 44100) ==")
    sfx = parse_snd(os.path.join(ROOT, "data", "tyrian.snd"), 29, False)
    voices = parse_snd(os.path.join(ROOT, "data", "voices.snd"), 9, True)
    for idx in (0, 5, 20):
        s = sfx[idx]
        a = sdl_convert(sdl, s)
        b = ours_convert(ours, libc, s)
        n = min(len(a), len(b))
        if n == 0:
            print("  sfx %-2d: empty" % idx)
            continue
        diff = [a[i] - b[i] for i in range(n)]
        rms_d = math.sqrt(sum(d * d for d in diff) / n)
        rms_a = math.sqrt(sum(x * x for x in a) / n)
        snr = 20 * math.log10(rms_a / rms_d) if rms_d > 0 else float("inf")
        print("  sfx %-2d: %d samples, RMS diff %.1f/32768 (%.2f%%), SNR %.1f dB"
              % (idx, n, rms_d, 100.0 * rms_d / 32768.0, snr))

    print()
    total = sum(len(s) for s in sfx) + sum(len(s) for s in voices)
    allb = sfx + voices
    bufs = [(ctypes.c_uint8 * len(s))(*s) for s in allb]
    n = ctypes.c_size_t(0)
    t0 = time.perf_counter()
    for b, s in zip(bufs, allb):
        ptr = ours.resampler_convert(b, len(s), FIN, FOUT, ctypes.byref(n))
        if ptr:
            libc.free(ptr)
    dt = time.perf_counter() - t0
    print("== load-time cost (conversion only, C) ==")
    print("  %d sounds, %d input bytes -> %d output samples in %.1f ms"
          % (len(allb), total, sum(len(s) * FOUT // FIN for s in allb), dt * 1000.0))


if __name__ == "__main__":
    sys.exit(main())
