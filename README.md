<img src="linux/icons/tyrian-128.png" width="128" height="128" align="right" alt="OpenTyrian icon">

# OpenTyrian

OpenTyrian is an open-source port of the DOS game Tyrian.

Tyrian is an arcade-style vertical scrolling shooter.  The story is set
in 20,031 where you play as Trent Hawkins, a skilled fighter-pilot employed
to fight MicroSol and save the galaxy.

Tyrian features a story mode, one- and two-player arcade modes, and networked
multiplayer.

## Downloads

Download the appropriate build for your platform from the
[latest release](https://github.com/opentyrian/opentyrian/releases/latest),
extract it, and run it:

| Platform | File | Run |
|---|---|---|
| Windows | `opentyrian-<version>-windows-<arch>.zip` | Unzip and run `opentyrian.exe` |
| macOS | `opentyrian-<version>-macos-universal.zip` | Unzip and open `OpenTyrian.app` |
| Linux | `opentyrian-<version>-linux-<arch>.tar.gz` | Extract and run `./opentyrian` |

These builds contain everything needed to run the game, including the freeware
Tyrian 2.1 data files.

The macOS app is not notarized.  If Gatekeeper refuses to open it, right-click
the app, choose *Open*, and confirm once.

Configuration and saved game files are kept in one of the following locations:

| Platform | Location |
|---|---|
| Windows | `%APPDATA%\OpenTyrian` |
| macOS / Linux | `$XDG_CONFIG_HOME/opentyrian` or `~/.config/opentyrian` |

On Windows and Linux, if `opentyrian.cfg` exists in the same directory as the
executable, the configuration and saved game files will be stored there instead.

## Game Data

If you download a release build of OpenTyrian, the freeware Tyrian 2.1 data
files are included and do not need to be downloaded separately.

Otherwise, download [Tyrian v2.1](https://camanis.net/tyrian/tyrian21.zip) and
extract the archive so that the files (with lowercase filenames) are in one of
the following locations, searched in order:

1. the directory given with `--data=DIR`
2. a `data` directory next to the executable (inside `Contents/Resources`
   for the macOS app)
3. the system directory the build was configured with
   (`/usr/local/share/games/tyrian` by default; `C:\TYRIAN` on Windows)

## Building

Requirements: a C99 compiler, GNU make, pkg-config, SDL3, and, for network
play, SDL3_net.

    make

Network play is enabled automatically when SDL3_net is found.

A Visual Studio solution is provided in `visualc/`.

## Command-Line Options

    -h, --help                   Show help about options
    -s, --no-sound               Disable audio
    -j, --no-joystick            Disable joystick/gamepad input
    -x, --no-xmas                Disable Christmas mode
    -t, --data=DIR               Set Tyrian data directory
    -n, --net=HOST[:PORT]        Start a networked game
    --net-player-name=NAME       Set local player name in a networked game
    --net-player-number=NUMBER   Set local player number in a networked game
                                 (1 or 2)
    -p, --net-port=PORT          Set local port to bind (default is 1333)
    -d, --net-delay=FRAMES       Set lag-compensation delay (default is 1)
    --presentation=MODE          Set the presentation mode: classic or modern
    --aspect=RATIO               Set the Modern aspect: 4:3, 16:10, 16:9, 21:9,
                                 32:9 or auto (default is 4:3)
    --pixel-aspect=SHAPE         Set the Modern pixel aspect: original (1.2, the
                                 CRT look) or square (default is original)
    --bloom=LEVEL                Set Modern bloom: off, low, medium or high
                                 (default is medium)
    --lighting=LEVEL             Set Modern dynamic lighting: off, low, medium
                                 or high (default is medium)
    --selftest-gamepad           Run the virtual-controller input self-test and exit

The `presentation` setting is also stored in `opentyrian.cfg` (in the `video`
section) and defaults to `classic`.  Classic is the original path: the 8-bit
frame is run through a software scaler (`None`, `2x`, `Scale2x`, `hq2x`, ...).
Modern composes an XRGB8888 canvas on the CPU at the logical resolution, runs
its effect passes there, and scales it to the window with nearest-neighbour;
the software scalers are ignored in Modern.

The Modern presentation is widescreen.  The original 320x200 frame keeps its
size and is centered horizontally in a wider canvas (height stays 200 rows), so
the playfield is never enlarged.  The `aspect` setting picks the on-screen
aspect (or `auto` follows the window); the canvas width is
`round(200 * pixel_aspect * aspect)`, never below 320.  The `pixel_aspect`
setting reproduces the non-square pixels of the original DOS output: `original`
draws each pixel 1.2x taller than wide, `square` draws them 1:1.  Both settings
are stored in the `video` section as `aspect` and `pixel_aspect`.

On non-gameplay frames (title/splash, menus, the shop, story/text screens and
the in-game Esc menu) the side space is filled with a copy of the frame scaled
to the canvas width, heavily blurred and darkened, so it reads as a soft
backdrop behind the sharp, centered frame.  It is all deterministic integer
math on the 320x200 grid, allocated only on resize (under 0.15 ms/frame at
16:9).  At 4:3 there is no side space and nothing changes.

During gameplay, when both side panels are at least 51 logical pixels wide
(exactly the width of a boss bar), the Modern layout drops the original sidebar
and bottom strip entirely: only the 264x184 playfield is copied out of the
320x200 frame, and the freed columns carry the new HUD.  The panels are
measured against the centered playfield (`(canvas_w - 264) / 2`), so 16:10
(60 px) now qualifies as well as 16:9 (81/82 px), 21:9 and 32:9; 4:3 (28 px) and
16:9 with square pixels (46 px) are too narrow and keep the original
full-frame layout exactly as before.  The 16 rows freed below the playfield are
a message strip: the level name on the top line and the in-game message
(`JE_drawTextWindow`) centered below it.

Single player uses both panels: the left one shows the ship status (name, extra
lives, cash, superbombs, shield and armor values with bars, generator name,
the **generator/weapon power reserve bar** -- the original sidebar's power bar,
which drains when firing -- plus the boss bars and level timer) and the right one
the armament (front and rear weapon name, power pips and rear firing mode; left
and right sidekick name, icon and ammo/charge gauge).  Two players get one
compact panel each (status then armament), player 2 right-aligned toward the
outer edge.  Everything is drawn on the original 320x200 logical grid with the
game's own fonts and sprites and procedural frames; the panels widen at 21:9
and 32:9 without changing the layout tiers.  The power reserve bar is shown in
every layout, including the compact ones.  Classic is always unchanged.

Modern gameplay also has two lighting effects, computed on the logical grid and
controlled by the `bloom` and `lighting` settings (CLI above, `video` section in
`opentyrian.cfg`, both defaulting to `medium`):

* **Bloom** is a tight additive glow around the brightest pixels -- shots,
  lasers, explosions and engine flames.  The emissive strength comes from the
  largest RGB channel of the active palette entry (so it follows palette fades
  and ignores dark saturated colours), thresholded and blurred with two
  separable box passes, then added back with a clamped add.
* **Dynamic lighting** is the wide illumination: the same emissive pixels cast
  their colour over the surrounding playfield, so nearby terrain, enemies and
  the ship are lit in the hue of the fire.  It is built at half the logical
  resolution, blurred with a large box radius and upsampled with a fixed 2x2
  box; the unlit base is scaled by a slight ambient factor (~0.92 at medium) so
  the light reads without making the game noticeably darker.

Both effects run only on gameplay frames, only on the 264x184 playfield (never
on the HUD panels, menus or title screen), are deterministic integer math, and
allocate nothing per frame.  `bloom off` is byte-for-byte the previous Modern
output, Classic never has either effect, and regression mode pins both off
unless `--regress-bloom`/`--regress-lighting` opt in.  Combined cost is under
0.4 ms/frame at 16:9 medium on the reference machine (under 0.6 ms at high).
A later phase will feed exact per-object lights (from the per-tick draw list)
through `modern_lighting_add_source()`, sharing this same light map.

The regression harness can save a presented frame to a BMP for headless visual
review: `--regress-snapshot=FRAME:FILE` (repeatable; the Modern canvas with
`--regress-modern`, the 8-bit frame otherwise), e.g. with `--regress-demo` or
`--regress-level`.

## Gamepads and Joysticks

OpenTyrian uses the SDL3 Gamepad API for devices SDL recognises as gamepads
(they are opened with `SDL_OpenGamepad`), and keeps the raw `SDL_Joystick` API
for everything else.  Gamepads get a sensible default mapping:

| Control | Action |
|---|---|
| Left stick | Movement (analog: proportional, honours the sensitivity/threshold settings) |
| D-pad | Movement (digital: on/off at full speed) |
| South / A | Fire (and Enter/confirm in menus) |
| East / B | Change rear-weapon mode (and Esc/cancel in menus) |
| Left shoulder (L1/LB) | Left sidekick |
| Right shoulder (R1/RB) | Right sidekick |
| Back / Select | In-game menu (the Esc menu) |
| Start | Pause (and Esc/back in menus) |

Controllers can be connected and disconnected while the game runs (hot-plug);
the game opens them as they arrive and releases them as they leave.  The
mapping is configurable on the joystick setup screen and stored in
`opentyrian.cfg` under a `joystick` section named after the device.  Gamepad
assignments are stored by name (`GB a`, `GB leftshoulder`, `GA lefty-`, ...),
while the raw joystick format (`AX 1-`, `BTN 1`, `H 1X+`, ...) is still written
and loaded for non-gamepad devices and old configuration files.

The `--no-joystick` (`-j`) option disables all controller input.

Since a physical controller is not always available, `--selftest-gamepad` runs
a headless self-test against SDL virtual controllers.  It exercises the default
mapping, hot-plug, the configuration round-trip and the legacy non-gamepad
path, then exits 0 on success or non-zero on failure.

## Network Multiplayer

Currently OpenTyrian does not have an arena; as such, networked games must be
initiated manually via the command line simultaneously by both players.

    opentyrian --net HOSTNAME --net-player-name NAME --net-player-number NUMBER

where `HOSTNAME` is the IP address of your opponent, `NUMBER` is either 1 or 2
depending on which ship you intend to pilot, and `NAME` is your alias.

OpenTyrian uses UDP port 1333 for multiplayer, but in most cases players will
not need to open any ports because OpenTyrian makes use of UDP hole punching.

## Links

- project: <https://github.com/opentyrian/opentyrian>
- irc:     <ircs://irc.oftc.net/#opentyrian>
- forums:  <https://tyrian2k.proboards.com/board/5>
