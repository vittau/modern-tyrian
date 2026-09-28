#!/usr/bin/env python3
"""Generate the README's banner and section-header SVGs.

Run:  python3 docs/readme/generate.py

Renders `banner.svg` and the `h-*.svg` headers in Tyrian's palette, with the
Press Start 2P font (OFL, `docs/readme/fonts/`) embedded as base64 so GitHub,
which loads SVGs as <img> and cannot fetch web fonts, still shows it.

Palette, sampled from the game's own UI (title/Game Menu screens and the
Modern HUD) so the art matches what the game draws:

  Space       #05070f  #0a1322  #132238  #1d3450   backgrounds, steel panels
  Steel hi    #2b4767  #40618c                      panel edges, grid
  Gold        #9a4914  #c9924e  #e6c884  #f4d79b   "Game Menu" title lettering
  Amber       #eb8a18  #f3ae24                     HUD power bar
  Danger      #d31400  #ff5a2b                     enemy fire, explosions
  Cyan        #53a8d8  #7dbee4                     shield bar / HUD accents
  Green       #598641  #7d9e59                     armour bar / HUD accents
  Ink         #eef3fb  #c3cb9a                     text

The output is deterministic: the starfields use a seeded LCG, so re-running
the script does not change the files.
"""
import base64
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = base64.b64encode(
    open(os.path.join(HERE, "fonts", "press-start-2p.woff2"), "rb").read()
).decode("ascii")

# --- palette ---------------------------------------------------------------
BG_DEEP = "#05070f"
BG = "#0a1322"
STEEL = "#132238"
STEEL_HI = "#2b4767"
STEEL_EDGE = "#40618c"
GOLD_DARK = "#9a4914"
GOLD = "#c9924e"
GOLD_HI = "#e6c884"
GOLD_TOP = "#f4d79b"
AMBER = "#f3ae24"
DANGER = "#ff5a2b"
CYAN = "#53a8d8"
CYAN_HI = "#7dbee4"
GREEN = "#7d9e59"
INK = "#eef3fb"
INK_DIM = "#c3cb9a"

STYLE = (
    "<style>@font-face{font-family:PS2P;src:url(data:font/woff2;base64,"
    + FONT
    + ") format('woff2')}text{font-family:PS2P,monospace}</style>"
)


def rng(seed):
    """Deterministic LCG so re-running the script does not churn the files."""
    s = seed

    def nxt():
        nonlocal s
        s = (s * 1664525 + 1013904223) >> 0
        s &= 0xFFFFFFFF
        return s / 2 ** 32

    return nxt


def glow(ident, blur):
    return (
        f'<filter id="{ident}" x="-50%" y="-50%" width="200%" height="200%">'
        f'<feGaussianBlur stdDeviation="{blur}" result="b"/>'
        '<feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>'
        "</filter>"
    )


def stars(width, height, count, seed, top=0, dim=0.25):
    r = rng(seed)
    out = []
    for _ in range(count):
        x = r() * width
        y = top + r() * (height - top)
        rad = 0.5 + r() * 1.1
        op = dim + r() * (1 - dim) * 0.8
        out.append(
            f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{rad:.2f}" '
            f'fill="{INK}" opacity="{op:.2f}"/>'
        )
    return "".join(out)


def gold_grad(ident, y1=0, y2=1):
    return (
        f'<linearGradient id="{ident}" x1="0" y1="{y1}" x2="0" y2="{y2}">'
        f'<stop offset="0" stop-color="{GOLD_TOP}"/>'
        f'<stop offset="0.42" stop-color="{GOLD}"/>'
        f'<stop offset="0.52" stop-color="{GOLD_HI}"/>'
        f'<stop offset="1" stop-color="{GOLD_DARK}"/>'
        "</linearGradient>"
    )


# --- little vector pieces --------------------------------------------------

def ship(x, y, s, flip=False):
    """A top-down fighter, gold body and steel wings, pointing up."""
    k = s / 40.0
    f = -1 if flip else 1
    return (
        f'<g transform="translate({x} {y}) scale({f * k:.4f} {k:.4f})" '
        'filter="url(#glow)">'
        f'<path d="M0,-22 L5,-14 L5,-2 L18,4 L18,9 L5,7 L5,13 L8,17 L8,21 '
        f'L-8,21 L-8,17 L-5,13 L-5,7 L-18,9 L-18,4 L-5,-2 L-5,-14 Z" '
        f'fill="url(#gShip)" stroke="{BG_DEEP}" stroke-width="1.1"/>'
        f'<path d="M0,-16 L3,-10 L3,6 L-3,6 L-3,-10 Z" fill="{CYAN_HI}"/>'
        f'<path d="M0,-13 L2,-9 L2,4 L-2,4 L-2,-9 Z" fill="{BG}"/>'
        f'<rect x="-2.4" y="21" width="4.8" height="9" rx="1.6" fill="{AMBER}"/>'
        f'<rect x="-1" y="30" width="2" height="7" rx="1" fill="{GOLD_TOP}"/>'
        "</g>"
    )


def burst(x, y, s, seed=3):
    """A muzzle/explosion star: amber spikes over a hot core."""
    r = rng(seed)
    spikes = []
    n = 8
    for i in range(n):
        a = (i / n) * 6.28318
        r0 = s * (0.28 + r() * 0.12)
        r1 = s * (0.85 + r() * 0.35)
        x0, y0 = x + math.cos(a) * r0, y + math.sin(a) * r0
        x1, y1 = x + math.cos(a) * r1, y + math.sin(a) * r1
        w = 1.6
        spikes.append(
            f'<path d="M{x0:.1f},{y0:.1f} L{x1:.1f},{y1:.1f}" '
            f'stroke="{AMBER if i % 2 else DANGER}" stroke-width="{w + (i % 3)}" '
            'stroke-linecap="round"/>'
        )
    return (
        f'<g filter="url(#bigGlow)">{"".join(spikes)}'
        f'<circle cx="{x}" cy="{y}" r="{s * 0.42:.1f}" fill="{DANGER}" opacity="0.85"/>'
        f'<circle cx="{x}" cy="{y}" r="{s * 0.22:.1f}" fill="{GOLD_TOP}"/></g>'
    )


def bolt(x, y, h, color=CYAN_HI):
    return (
        f'<rect x="{x - 1.6}" y="{y}" width="3.2" height="{h}" rx="1.6" '
        f'fill="{color}" filter="url(#glow)"/>'
    )


def glass_panel(x, y, w, h, bars, seed=1):
    """A Modern-HUD side panel: dark glass, accent top, vertical value bars."""
    r = rng(seed)
    out = [
        f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="url(#gGlass)"/>',
        f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="none" '
        f'stroke="{STEEL_EDGE}" stroke-width="1.2"/>',
        f'<rect x="{x + 8}" y="{y + 9}" width="{w - 16}" height="2.4" fill="{CYAN}" opacity="0.8"/>',
        f'<rect x="{x + 8}" y="{y + 15}" width="{(w - 16) * 0.55:.0f}" height="1.6" fill="{INK}" opacity="0.28"/>',
    ]
    bw = 7
    gap = 5
    bx = x + 10
    by = y + 30
    avail = h - 56
    for i, (frac, color) in enumerate(bars):
        bh = max(8, avail * frac)
        out.append(
            f'<rect x="{bx + i * (bw + gap)}" y="{by + (avail - bh):.0f}" width="{bw}" '
            f'height="{bh:.0f}" rx="2" fill="{color}" opacity="0.92"/>'
        )
    # a few ticks below the bars
    ty = y + h - 12
    for i in range(3):
        ww = 10 + r() * 12
        out.append(
            f'<rect x="{x + 10}" y="{ty - i * 5}" width="{ww:.0f}" height="1.6" '
            f'fill="{INK}" opacity="0.22"/>'
        )
    return "".join(out)


# --- the banner ------------------------------------------------------------

def banner():
    W, H = 1280, 400
    fx, fy, fw, fh = 150, 58, 980, 262
    panel_w = 122
    px = fx + panel_w          # playfield left
    pw = fw - 2 * panel_w      # playfield width
    py = fy
    ph = fh
    cx = fx + fw / 2

    field = (
        f'<clipPath id="field"><rect x="{fx + 3}" y="{fy + 3}" '
        f'width="{fw - 6}" height="{fh - 6}" rx="8"/></clipPath>'
    )

    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="Modern Tyrian">
  {STYLE}
  <defs>
    <linearGradient id="sky" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{BG_DEEP}"/>
      <stop offset="0.55" stop-color="{BG}"/>
      <stop offset="1" stop-color="{STEEL}"/>
    </linearGradient>
    <linearGradient id="gShip" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{GOLD_HI}"/>
      <stop offset="0.5" stop-color="{GOLD}"/>
      <stop offset="1" stop-color="{GOLD_DARK}"/>
    </linearGradient>
    <linearGradient id="gGlass" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{STEEL_HI}" stop-opacity="0.92"/>
      <stop offset="1" stop-color="{BG_DEEP}" stop-opacity="0.96"/>
    </linearGradient>
    <linearGradient id="gField" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#081225"/>
      <stop offset="1" stop-color="#04060d"/>
    </linearGradient>
    <radialGradient id="planet" cx="0.66" cy="0.28" r="0.8">
      <stop offset="0" stop-color="#3a2a4a"/>
      <stop offset="0.55" stop-color="#221733"/>
      <stop offset="1" stop-color="#0b0d1c"/>
    </radialGradient>
    <linearGradient id="planetRim" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="{GOLD_HI}" stop-opacity="0"/>
      <stop offset="0.55" stop-color="{GOLD}" stop-opacity="0.9"/>
      <stop offset="1" stop-color="{DANGER}" stop-opacity="0.2"/>
    </linearGradient>
    <linearGradient id="rule" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="{GOLD}" stop-opacity="0"/>
      <stop offset="0.5" stop-color="{GOLD_HI}"/>
      <stop offset="1" stop-color="{GOLD}" stop-opacity="0"/>
    </linearGradient>
    {gold_grad("gGold")}
    {glow("glow", 1.6)}
    {glow("bigGlow", 7)}
    <pattern id="scan" width="4" height="4" patternUnits="userSpaceOnUse">
      <rect width="4" height="1.3" fill="black" opacity="0.20"/>
    </pattern>
    <radialGradient id="vig" cx="0.5" cy="0.5" r="0.75">
      <stop offset="0.6" stop-color="black" stop-opacity="0"/>
      <stop offset="1" stop-color="black" stop-opacity="0.5"/>
    </radialGradient>
    {field}
  </defs>

  <rect width="{W}" height="{H}" fill="url(#sky)"/>
  {stars(W, H, 120, 11)}

  <!-- distant planet, lower right -->
  <g clip-path="url(#field)">
    <circle cx="1090" cy="150" r="210" fill="url(#planet)"/>
    <circle cx="1090" cy="150" r="210" fill="none" stroke="url(#planetRim)" stroke-width="5"/>
    <rect width="{W}" height="{H}" fill="url(#gField)" opacity="0.08"/>
  </g>

  <!-- steel frame -->
  <rect x="{fx}" y="{fy}" width="{fw}" height="{fh}" rx="10" fill="url(#gGlass)"/>
  <rect x="{fx}" y="{fy}" width="{fw}" height="{fh}" rx="10" fill="none" stroke="{STEEL_EDGE}" stroke-width="2"/>
  <rect x="{fx + 3}" y="{fy + 3}" width="{fw - 6}" height="{fh - 6}" rx="8" fill="url(#gField)"/>

  <!-- center playfield -->
  <g clip-path="url(#field)">
    {stars(pw, ph, 70, 23, top=0)}
    <circle cx="{cx - 40}" cy="{py + 74}" r="150" fill="{CYAN}" opacity="0.05" filter="url(#bigGlow)"/>
    <circle cx="{cx + 150}" cy="{py + 200}" r="120" fill="{DANGER}" opacity="0.06" filter="url(#bigGlow)"/>
    {bolt(px + 40, py + 12, 40)}
    {bolt(px + 8, py + 34, 22, GOLD_TOP)}
    {bolt(px + 72, py + 30, 26, GOLD_TOP)}
    {ship(px + 40, py + 94, 44)}
    {burst(px + pw - 96, py + 66, 26, 5)}
    {burst(px + pw - 150, py + 104, 14, 9)}
  </g>

  <!-- glass HUD panels -->
  {glass_panel(fx + 14, fy + 16, panel_w - 26, fh - 32, [(0.9, CYAN), (0.62, GOLD_HI), (0.8, AMBER)], 2)}
  {glass_panel(fx + fw - panel_w + 12, fy + 16, panel_w - 26, fh - 32, [(0.55, GREEN), (0.95, INK), (0.7, GOLD)], 4)}

  <!-- title -->
  <g text-anchor="middle">
    <text x="{cx}" y="{py + 106}" font-size="15" fill="{CYAN_HI}" letter-spacing="6">OPEN TYRIAN</text>
    <text x="{cx + 3}" y="{py + 169}" font-size="47" fill="{BG_DEEP}" opacity="0.85">MODERN TYRIAN</text>
    <text x="{cx}" y="{py + 166}" font-size="47" fill="url(#gGold)" stroke="{GOLD_DARK}" stroke-width="1" filter="url(#glow)">MODERN TYRIAN</text>
    <text x="{cx}" y="{py + 202}" font-size="10" fill="{INK}" letter-spacing="2.4">THE CLASSIC ARCADE SHOOTER, REFITTED FOR MODERN DISPLAYS</text>
  </g>

  <rect x="{fx + 40}" y="{fy + fh + 34}" width="{fw - 80}" height="2" fill="url(#rule)"/>
  <rect width="{W}" height="{H}" fill="url(#scan)"/>
  <rect width="{W}" height="{H}" fill="url(#vig)"/>
</svg>
"""


# --- section headers -------------------------------------------------------

ICON_COLOR = GOLD_HI


def icon(name, x, y, s):
    """A small pixel-art glyph for a section header, in a 40x40 box."""
    k = s / 40.0
    g = f'<g transform="translate({x} {y}) scale({k:.4f})" stroke-linecap="round" stroke-linejoin="round">'
    body = ""
    if name == "play":  # download arrow into a tray
        body = (
            f'<path d="M20 4 L20 22" stroke="{ICON_COLOR}" stroke-width="4"/>'
            f'<path d="M11 16 L20 25 L29 16" fill="none" stroke="{ICON_COLOR}" stroke-width="4"/>'
            f'<path d="M7 30 L7 35 L33 35 L33 30" fill="none" stroke="{CYAN_HI}" stroke-width="3.4"/>'
        )
    elif name == "deck":  # handheld: body, screen, two sticks
        body = (
            f'<rect x="6" y="7" width="28" height="26" rx="4" fill="none" stroke="{ICON_COLOR}" stroke-width="3"/>'
            f'<rect x="12" y="12" width="16" height="10" rx="1.5" fill="{CYAN}" opacity="0.85"/>'
            f'<circle cx="14" cy="27" r="2.6" fill="{GREEN}"/>'
            f'<circle cx="26" cy="27" r="2.6" fill="{AMBER}"/>'
        )
    elif name == "modern":  # widescreen canvas with side HUD panels
        body = (
            f'<rect x="3" y="9" width="34" height="22" rx="2.5" fill="none" stroke="{ICON_COLOR}" stroke-width="2.6"/>'
            f'<rect x="8" y="13" width="5" height="14" fill="{CYAN}" opacity="0.85"/>'
            f'<rect x="27" y="13" width="5" height="14" fill="{GOLD}" opacity="0.85"/>'
            f'<path d="M31 4 L32.6 7.4 L36 9 L32.6 10.6 L31 14 L29.4 10.6 L26 9 L29.4 7.4 Z" fill="{INK}"/>'
        )
    elif name == "controls":  # gamepad
        body = (
            f'<path d="M9 11 L31 11 Q37 11 36 20 Q35.5 28 31 30 Q28 31 26 28 L24 25 L16 25 L14 28 Q12 31 9 30 Q4.5 28 4 20 Q3 11 9 11 Z" '
            f'fill="none" stroke="{ICON_COLOR}" stroke-width="2.8"/>'
            f'<path d="M12 17 L12 21 M10 19 L14 19" stroke="{CYAN_HI}" stroke-width="2.4"/>'
            f'<circle cx="27" cy="17" r="2" fill="{DANGER}"/>'
            f'<circle cx="30" cy="21" r="2" fill="{AMBER}"/>'
            f'<rect x="18" y="18" width="5" height="2.4" rx="1.2" fill="{INK}" opacity="0.8"/>'
        )
    elif name == "graphics":  # monitor with a sun/slider
        body = (
            f'<rect x="4" y="7" width="32" height="22" rx="2.5" fill="none" stroke="{ICON_COLOR}" stroke-width="2.8"/>'
            f'<path d="M7 26 L14 15 L19 22 L24 18 L33 26 Z" fill="{AMBER}" opacity="0.85"/>'
            f'<circle cx="13" cy="13" r="2.6" fill="{GOLD_TOP}"/>'
            f'<path d="M16 33 L24 33" stroke="{CYAN_HI}" stroke-width="3"/>'
        )
    elif name == "build":  # hammer
        body = (
            f'<rect x="8" y="8" width="17" height="9" rx="2" fill="{GOLD}" stroke="{GOLD_DARK}" stroke-width="1.4"/>'
            f'<rect x="13" y="16" width="5" height="19" rx="2" fill="{CYAN}" opacity="0.9"/>'
            f'<path d="M25 6 L35 16" stroke="{INK}" stroke-width="3"/>'
        )
    elif name == "options":  # terminal
        body = (
            f'<rect x="4" y="7" width="32" height="26" rx="3" fill="none" stroke="{ICON_COLOR}" stroke-width="2.8"/>'
            f'<path d="M11 15 L16 19 L11 23" fill="none" stroke="{CYAN_HI}" stroke-width="2.8"/>'
            f'<rect x="19" y="22" width="11" height="3" rx="1.5" fill="{INK}" opacity="0.85"/>'
        )
    elif name == "regress":  # checklist shield
        body = (
            f'<path d="M20 4 L34 9 L34 20 Q34 31 20 36 Q6 31 6 20 L6 9 Z" fill="none" stroke="{ICON_COLOR}" stroke-width="2.8"/>'
            f'<path d="M13 19 L18 24 L28 13" fill="none" stroke="{GREEN}" stroke-width="3.2"/>'
        )
    elif name == "credits":  # star
        body = (
            f'<path d="M20 3 L24.5 14 L36 15 L27 23 L30 35 L20 28 L10 35 L13 23 L4 15 L15.5 14 Z" '
            f'fill="{GOLD}" stroke="{GOLD_HI}" stroke-width="1.4"/>'
        )
    return g + body + "</g>"


def header(title, name, idx):
    W, H = 880, 76
    seed = 100 + idx * 7
    t = title.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="{t}">
  {STYLE}
  <defs>
    <linearGradient id="h{idx}bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{BG_DEEP}"/>
      <stop offset="0.7" stop-color="{BG}"/>
      <stop offset="1" stop-color="{STEEL}"/>
    </linearGradient>
    <linearGradient id="h{idx}edge" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="{GOLD}"/>
      <stop offset="0.5" stop-color="{CYAN}"/>
      <stop offset="1" stop-color="{STEEL_EDGE}"/>
    </linearGradient>
    <linearGradient id="h{idx}rule" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="{CYAN}" stop-opacity="0"/>
      <stop offset="0.5" stop-color="{CYAN}" stop-opacity="0.7"/>
      <stop offset="1" stop-color="{CYAN}" stop-opacity="0"/>
    </linearGradient>
    {gold_grad(f"h{idx}gold")}
    {glow("hglow", 1.3)}
    <clipPath id="h{idx}clip"><rect x="1" y="1" width="{W - 2}" height="{H - 2}" rx="10"/></clipPath>
  </defs>
  <g clip-path="url(#h{idx}clip)">
    <rect width="{W}" height="{H}" fill="url(#h{idx}bg)"/>
    {stars(W - 260, H, 18, seed, top=6, dim=0.2)}
    <g stroke="{STEEL_HI}" stroke-width="1" opacity="0.5">
      <path d="M620 76 L720 0 M700 76 L800 0 M780 76 L880 0 M840 76 L920 0"/>
    </g>
    <rect x="0" y="0" width="6" height="{H}" fill="{GOLD}"/>
    <rect x="0" y="{H - 4}" width="{W}" height="4" fill="url(#h{idx}rule)"/>
  </g>
  <rect x="1" y="1" width="{W - 2}" height="{H - 2}" rx="10" fill="none" stroke="url(#h{idx}edge)" stroke-width="2"/>
  {icon(name, 20, 16, 44)}
  <text x="84" y="49" font-size="26" fill="url(#h{idx}gold)" stroke="{GOLD_DARK}" stroke-width="0.7" filter="url(#hglow)">{t}</text>
</svg>
"""


HEADERS = [
    ("play", "PLAY"),
    ("deck", "STEAM DECK"),
    ("modern", "WHAT MODERN MODE BRINGS"),
    ("controls", "CONTROLS"),
    ("graphics", "SETUP: GRAPHICS"),
    ("build", "BUILD IT"),
    ("options", "COMMAND-LINE OPTIONS"),
    ("regress", "REGRESSION TESTING"),
    ("credits", "CREDITS AND LICENCE"),
]


def write(name, text):
    path = os.path.join(HERE, name)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    print("%-22s %6d bytes" % (name, len(text.encode("utf-8"))))


if __name__ == "__main__":
    write("banner.svg", banner())
    for i, (name, title) in enumerate(HEADERS):
        write("h-%s.svg" % name, header(title, name, i))
