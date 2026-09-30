# CRT filters in Modern Tyrian

Modern presentation offers **Off**, **Scanlines**, **NTSC**, and **Scanl + NTSC**
in *Setup → Graphics → CRT Filter*. The full combined label, “Scanlines + NTSC”,
measures 117 pixels in the normal menu font; the 95-pixel value column therefore
uses “Scanl + NTSC” (90 pixels). The font’s blank `+` placeholder is drawn
as a small cross within its existing measured advance. The row is disabled in Classic. The launcher
never passes through this filter.

The shared `opentyrian.cfg` stores `crt_filter` beside `smooth_motion`, using
`off`, `scanlines`, `ntsc`, or `scanlines+ntsc`. Its default is `off`, and unknown
values also select Off. Changes take effect on the next presented frame.

## Why composite looks different

An NTSC television receives an analogue signal rather than an RGB pixel grid.
Brightness is carried as luma (Y); colour is carried as two chroma components
(I and Q) modulated onto a colour subcarrier. A colourburst provides the phase
reference. High-frequency brightness transitions can leak into chroma, producing
rainbow fringes and dot crawl, while limited chroma bandwidth bleeds colours
sideways. Limited luma bandwidth softens sharp edges.

The filter is a stylised SNES composite model, rather than an emulation of a
particular PC display adapter. It is the same scalar C99 port used in Deadly
Dave, adapted here to Tyrian's XRGB8888 presentation buffer.

## Blargg NTSC

`src/ntsc.c` ports Shay Green's **snes_ntsc 0.2.2**, retaining his copyright and
**LGPL 2.1 or later** license. The implementation keeps the composite preset;
there are no alternative S-video/RGB presets or field merging.

The model converts RGB to YIQ, applies a sharpened sinc luma filter and a
Gaussian chroma filter, mixes encoder artifacts/fringing, resamples horizontally,
and converts YIQ back through the decoder matrix. Because these operations are
linear, it precomputes each colour's contribution against black and sums those
kernels while blitting. Six neighbouring pixel kernels contribute to each
output pixel, followed by a packed RGB clamp.

Input is quantised to **RGB555** (32768 colours). Each colour has kernels for
three alignments and three burst phases, occupying approximately **16 MiB**.
The table is built lazily on the first NTSC frame, reused across mode changes,
and freed at video shutdown. RGB555 scratch storage grows only when needed.
`NTSC_RGB_BITS` stays **8**: half-range kernels without a compensating brightness
stage would darken the entire picture.

The blitter processes three input columns into seven output columns:

```
output_width(w) = ((w - 1) / 3 + 1) * 7
```

It primes the sliding window with black, sums/clamps the kernels while consuming
input, and flushes the final partial chunk against black. The last one or two
input pixels are retained even when the width is not a multiple of three plus
one. Burst phase advances between source rows and cycles through 0, 1, 2 once
per presented frame. Smooth-motion frames therefore advance it independently
of the game's logic ticks. Regression resets the starting phase to zero.

## Half-row scanlines

The scanlines follow Deadly Dave's luminance-weighted CPU pass. Their dark band
is the **lower half of every game row**, as if a 200-row picture were displayed
on 400 rows. They are not alternating full game rows at normal window sizes.
For a pixel's RGB channels:

```
lum = min((77*R + 150*G + 29*B) >> 8, 216)
dark(channel) = ((channel >> 1) * (255 - lum) + channel * lum) >> 8
```

Bright pixels lose less light, modelling the beam spreading into the gap.
The luminance weight saturates at **216**, so white retains a faint trace.
The upper X byte is preserved by this pass.

`crt_filter_output_height(src_h, dst_h)` chooses the uploaded texture height:

- If the destination is shorter than twice the source height, keep `src_h`
  and use the reference's alternating-row fallback.
- If the destination is a whole multiple of `2 * src_h`, use `2 * src_h`.
- Otherwise use `dst_h`, one output row per native destination pixel. This
  includes odd integer scales: 200 source rows into 600 destination rows
  produces a 600-row texture, avoiding a fractional 1.5× GPU stretch.

For each output row, the pass computes its source row and the fraction of its
vertical interval that overlaps the dark lower half. A row wholly in the bright
half is copied; one wholly in the dark half is dimmed; a straddling row blends
in proportion to its overlap using 16-bit fixed-point coverage. This coverage
is computed once per row. In combined mode, NTSC runs at the **source height**
first, then scanline resampling expands that output from bottom to top in place.
The NTSC blit never repeats work at the window's vertical resolution.

## Wiring into Tyrian

```
Modern canvas: palette, lighting, bloom, effects, HUD, modals
                      |
          regress_capture_modern_frame()
                      |
          modern_present_frame(): optional CRT pass
                      |
              streaming XRGB8888 texture
                      |
       original Fit rectangle and sharp-bilinear presentation
```

Every Modern presentation, including interpolation and modal frames, uses this
path. The pass reads the finished canvas without changing it, game state, or
RNG order. Blur backdrops and other canvas readers see the unfiltered picture.
Off retains the original canvas texture and upload path. Filtered output has
its own buffer and streaming texture, replaced only when output dimensions
change. Allocation failure falls back to the complete original canvas.

`video_fit_rect()` is shared with the presenter and measures the window in
**native pixels**, including HiDPI. The filter uses this rectangle's height for
scanline sampling. Content aspect always comes from the **canvas**, divided by
`MODERN_ORIGINAL_PIXEL_ASPECT`; NTSC's expanded texture cannot change it. Mouse
mapping also keeps the original canvas dimensions and split layout coordinates.

## Verification and measurement

Existing regressions pin CRT Off. `--regress-crt=MODE` opts a Modern run into
hashing the exact filtered buffer uploaded by the presenter and prints a
`CRT coverage:` line proving execution. `--regress-crt-height=N` supplies a
fixed destination height for synthetic output hashes.

`--regress-crt-check` runs release-safe synthetic assertions for identity,
pitches, output dimensions, half-row coverage at 400/600/1200/1080/2160 and odd 1079/2161 rows,
the 150-row fallback, white's faint trace, input immutability, phase reset, and
unknown config values, and partial NTSC chunks at widths 1 through 7. Its `CRT measurement:` lines report each mode at 1080
and 2160 destination rows and the lazy table build separately. These are
CPU-filter measurements; texture uploads/GPU presentation are excluded.

`tools/check_display.sh` checks runtime mode/size/aspect/fullscreen changes,
output reuse, Classic bypass, modal mouse mapping in all four modes and both
variants, the real menu/picker, shared config roundtrips, and identical game-state/RNG hashes.

For review captures, regress-only `--regress-crt-window=WIDTHxHEIGHT` controls
the dummy/native window and `--regress-present-png=FRAME:FILE` captures the
renderer output immediately before presentation. PNG capture is restricted to
Tyrian 2.1. Keep the frame cap greater than the requested capture frame plus
one, since the harness exits at its capture checkpoint on the final frame.

## Credits

- Shay Green (Blargg), [snes_ntsc 0.2.2](http://www.slack.net/~ant/),
  Copyright © 2006–2007, LGPL 2.1 or later.
- Deadly Dave: scalar C99 NTSC port and half-row scanline implementation,
  adapted for Modern Tyrian's presentation, geometry and configuration.
