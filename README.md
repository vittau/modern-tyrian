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

Otherwise, run `./get_data.sh` (or download
[Tyrian v2.1](https://camanis.net/tyrian/tyrian21.zip) yourself) and extract
the archive so that the files (with lowercase filenames) are in one of the
following locations, searched in order:

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
    --presentation=MODE          Set presentation mode: classic or modern
    --aspect=RATIO               Modern aspect: 4:3, 16:10, 16:9, 21:9, 32:9, auto
    --pixel-aspect=SHAPE         Pixel aspect: original (1.2) or square
    --bloom=LEVEL                Modern bloom: off, low, medium or high
    --lighting=LEVEL             Modern dynamic lighting: off, low, medium or high
    --regress-demo=N             Replay recorded demo N (1-5) headless and exit
    --regress-level=E:L          Start level L of episode E headless and exit
    --regress-frames=N           Cap a --regress-level run at N frames
    --regress-out=FILE           Write per-frame hashes to FILE (regress modes)
    --regress-state-out=FILE     Write per-frame game-state hashes to FILE
    --regress-snapshot=F:FILE    Save the presented image of frame F to FILE (BMP)
                                 (repeatable; the Modern canvas with --regress-modern)
    --regress-players=N          Start a --regress-level scenario with N players (1 or 2)
    --regress-arcade             Start a --regress-level scenario in 1-player arcade mode
    --regress-screen=NAME        Render one non-gameplay screen headless and exit
                                 (title, episode-select, high-scores, game-menu, upgrade,
                                 purchase, options, cube-list, cube-reader, keyboard,
                                 joystick, load-save, solid, setup, nav-map, ship-specs,
                                 jukebox, weapon-sim, credits)
    --regress-replay-check       Record each level frame's draw list and replay it (proof)
    --regress-interp-check       Render each level frame interpolated at alpha=1 and
                                 compare it byte for byte with the real frame (proof)
    --regress-interp-alpha=A     Present the frame interpolated at alpha A (0..1)
                                 (with --regress-snapshot; Modern only)
    --regress-interp-smoothness  Per level tick, check that every background layer and
                                 matched object moves monotonically across the sub-frames
    --regress-smooth-alphas=N    Sub-frame samples for --regress-interp-smoothness (default 5)
    --regress-gameplay-check     Assert every in-level Modern frame uses the gameplay
                                 composition (drops the classic sidebar)
    --smooth-motion=on|off       Modern gameplay at the display refresh with interpolated
                                 motion (default on)
    --regress-realtime           Replay a demo in a real window with the wall clock and log
                                 presented-fps statistics (uses --regress-demo)
    --bench-seconds=N            Duration of --regress-realtime (default 20)
    --regress-detail=M           Pin processor detail level M (1-6, default 2)
    --regress-modern             Hash the Modern canvas in regress modes
    --regress-bloom=LEVEL        Pin Modern bloom in regress modes (default off)
    --regress-lighting=LEVEL     Pin Modern lighting in regress modes (default off)
    --regress-audio              Render the audio baselines to FILE and exit
    --selftest-gamepad           Run the virtual-controller input self-test and exit

The `presentation` setting is also stored in `opentyrian.cfg` (in the `video`
section) and defaults to `classic`.  Classic is the original path: the 8-bit
frame is run through a software scaler (`None`, `2x`, `Scale2x`, `hq2x`, ...).
Modern composes an XRGB8888 canvas on the CPU at the logical resolution, runs
its effect passes there, and scales it to the window with nearest-neighbour;
the software scalers are ignored in Modern.  The Graphics submenu of the
in-game Setup screen exposes Presentation, Aspect, Pixel Aspect and Smooth
Motion.  Presentation, Aspect and Smooth Motion are Modern-only: they are
greyed out and ignored while Classic is active.  Pixel Aspect applies to both
presentations.  All of them are saved through the existing configuration.

The `scaling_mode` key picks how the frame is fitted to the window: `Center`
draws it 1:1, `Integer` scales it by whole multiples, and `Fit` scales it
proportionally to fill the window.  `Integer` is the default.

The `pixel_aspect` key reproduces the non-square pixels of the original DOS
output: `original` draws each pixel 1.2x taller than wide (the 4:3 frame),
`square` draws them 1:1 (the 8:5 frame).  It applies to both presentations: in
Modern it also sets the canvas width, and in Classic it selects the frame that
`Fit` uses, so `Fit` + `original` reproduces the old "Fit 4:3" mode and `Fit` +
`square` the old "Fit 8:5".  Classic `Integer` and `Center` ignore the pixel
aspect, as before.

The Modern presentation is widescreen.  The original 320x200 frame keeps its
size and is centered horizontally in a wider canvas (height stays 200 rows), so
the playfield is never enlarged.  The `aspect` setting picks the on-screen
aspect (or `auto` follows the window); the canvas width is
`round(200 * pixel_aspect * aspect)`, never below 320.  The `aspect` and
`pixel_aspect` settings are stored in the `video` section and default to `4:3`
and `original`.

For compatibility, an existing config whose `scaling_mode` is the old
`Fit 8:5` or `Fit 4:3` loads as `Fit` with `pixel_aspect` `square` or
`original` respectively.  An explicit `pixel_aspect` in the same file wins over
the legacy name.

The `smooth_motion` key in the same section (`--smooth-motion=on|off`) makes
Modern gameplay present at the display refresh with interpolated motion while
the logic keeps its original fixed tick; it defaults to `on` and has no effect
in Classic.  In Modern, a new window also opens at the chosen aspect instead of
the Classic scaler size: the largest integer multiple of the 200 logical rows
that fits in about 80% of the usable desktop, centered on the display.

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

`--regress-screen=NAME` renders one non-gameplay screen in a deterministic
state, presents `--regress-frames` frames (default 90) and exits, so the
widescreen menu compositions can be hashed and snapshotted without a window.
`NAME` is one of `title`, `episode-select`, `high-scores`, `game-menu`,
`upgrade`, `purchase`, `options`, `cube-list`, `cube-reader`, `keyboard`,
`joystick`, `load-save`, `solid`, `setup`, `nav-map`, `ship-specs`,
`jukebox`, `weapon-sim` or `credits`; combine it with `--regress-modern`,
`--regress-aspect` and `--regress-out` as usual.

## Regression Testing

The regression harness replays the recorded demos and the synthetic level
scenarios without a window and compares a hash per frame against the baselines
in `test/regress/`.  The default flow downloads the reference data and runs the
suite:

    ./get_data.sh
    make regress

`./get_data.sh` fetches the official freeware Tyrian 2.1 release into `./data`,
which is the default data directory, and `make regress` verifies that copy
against the committed baselines.  To run against another copy of the data,
point `TYRIAN_DATA` at it:

    make regress TYRIAN_DATA=/path/to/Tyrian

There are two full proof sweeps: `make regress-replay` records each level
frame's draw list and replays it (the renderer reproduces every frame byte for
byte), and `make regress-interp` renders each frame interpolated at alpha = 1
and compares it byte for byte with the real frame.  `--update` regenerates the
baselines when a change to the output is intentional.

The game data comes from `TYRIAN_DATA` (default `./data`, fetched by
`./get_data.sh`).  The baselines are only valid for the exact freeware data
they were generated from, so the harness checks the files it reads against
`test/regress/data-manifest.txt` (sizes and POSIX `cksum` CRCs) and refuses a
different data directory, naming the mismatching file, instead of reporting a
false divergence.  Freeware mirrors are not guaranteed to be byte-identical,
and changing a single sprite moves every frame from the point where it appears.

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

## Credits

The AdLib/OPL2 FM music is emulated by [Nuked-OPL3](https://github.com/nukeykt/Nuked-OPL3)
by Nuke.YKT (commit `765ec962e473aeb767e4cba74ffdc8f588ffbfe8`), vendored under
`src/nuked_opl3.c` / `src/nuked_opl3.h` and used under the GNU Lesser General
Public License v2.1 or later; see `LICENSE-Nuked-OPL3` for the full text.

## Links

- project: <https://github.com/opentyrian/opentyrian>
- irc:     <ircs://irc.oftc.net/#opentyrian>
- forums:  <https://tyrian2k.proboards.com/board/5>
