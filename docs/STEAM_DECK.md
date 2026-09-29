# OpenTyrian on the Steam Deck

OpenTyrian ships for Linux as a plain `.tar.gz` with a self-contained binary —
no AppImage, Flatpak, Snap or distro package.  It is statically linked against
SDL3 and SDL3_net, so the only thing it needs from SteamOS is glibc and libm;
the video (Wayland/X11), audio (PipeWire/PulseAudio/ALSA) and controller
(udev/HIDAPI) backends are loaded from the system at run time.  Nothing has to
be installed or unlocked, which also makes it a good fit for SteamOS's
read-only root filesystem.

The build is made on Ubuntu 22.04 (glibc 2.35), so it runs on SteamOS 3.5 and
newer (glibc 2.37+), and on current Arch/Fedora/Debian.

## 1. Download and extract

In **Desktop Mode**:

1. Open a browser and download the Linux build from the
   [latest release](https://github.com/vittau/modern-tyrian/releases/latest):
   `opentyrian-<version>-linux-x86_64.tar.gz`.
2. Extract it into a folder in your home directory, for example:

       mkdir -p ~/Games
       tar -xzf ~/Downloads/opentyrian-<version>-linux-x86_64.tar.gz -C ~/Games

   You should end up with `~/Games/opentyrian/opentyrian` (the executable),
   `~/Games/opentyrian/data/` (the freeware Tyrian 2.1 data), and the license
   files.
3. If the browser or a FAT-formatted USB drive dropped the executable bit,
   restore it once:

       chmod +x ~/Games/opentyrian/opentyrian

4. Optional sanity check from a terminal — this prints the version and exits:

       ~/Games/opentyrian/opentyrian --help

## 2. Install

Nothing to install: the game runs from wherever you extracted it and finds its
`data/` directory next to the executable, regardless of the working directory.

## 3. Add it to Steam

1. In Desktop Mode, open **Steam**.
2. Bottom-left: **Add a Game** → **Add a Non-Steam Game…**
3. **Browse…**, change the file filter to *All Files* if needed, and select
   `~/Games/opentyrian/opentyrian`.  (You can also type the full path into the
   file name box.)
4. **Add Selected Programs**.
5. Optional: in the game's **Properties → Shortcut**, rename it to "OpenTyrian"
   and give it the `linux/icons/tyrian-128.png` icon from the source tree.
6. **Launch options: leave empty.**  The binary finds its data directory by
   itself; fullscreen and the 16:10 canvas are the defaults on a Deck.
7. Switch back to **Game Mode**; OpenTyrian is now in your library.

To see the log while diagnosing a problem, add this launch option:

    --log-file=/home/deck/opentyrian.log

(The game also writes `~/.config/opentyrian/opentyrian.log` automatically on a
Deck, so this is only needed if you want the log somewhere else.)

The built-in launcher opens on **every start**, including in Game Mode. Its
painted panels render at the actual display resolution and fit the Deck's
1280×800 screen. Use the d-pad or left stick to select **Tyrian 2.1** or
**Tyrian 2000**, A to confirm, and B to exit. The last choice only preselects
the panel; it never skips this screen. Down selects the About/Exit bar; About
shows version, credits, licences, and the data paths in use.

The package includes only the freeware 2.1 data. Until a valid 2000 installation
is found, its button reads **INSTALL** and explains manual installation. Place
your 2000 data in `~/Games/opentyrian/tyrian2000/`, beside the executable, or
set the `TYRIAN2000_DATA` environment variable to its directory. Saves and
scores are separate in `tyrian21/` and `tyrian2000/` under the shared user root.
For automation, `--variant=2.1|2000` skips the launcher; leave normal Steam
launch options empty to use the selector.

## 4. Controller

The built-in Steam Deck controls are ordinary SDL gamepads, and OpenTyrian uses
SDL's Gamepad API, so **no Steam Input template is required**.

* Leave the controller layout at Steam's default (*Gamepad*); do **not** apply a
  Keyboard/Mouse template for this game.
* The left stick is proportional (the ship speed ramps up with the tilt); the
  right trackpad works as the mouse.
* All buttons can be remapped in the in-game **Setup → Joystick** screen, and the
  mapping is saved in `opentyrian.cfg`.

## 5. Video and fullscreen

On a Deck the game starts **fullscreen**, in the **Modern** presentation, with
the aspect set to **auto**.  At 1280×800 auto resolves to the 16:10 canvas, so
the picture fills the screen with the 16:10 HUD geometry and no window frame or
mouse cursor.

* **Alt+Enter** toggles fullscreen at any time.
* **Setup → Graphics** changes the presentation (Classic/Modern), the aspect
  (4:3, 16:10, 16:9, 21:9, 32:9, auto) and the pixel aspect.
* Docked to a 16:9 TV, auto follows the display and still fills the screen; keep
  it on auto unless you specifically want a fixed canvas.
* Prefer the original 4:3 look?  Set presentation *Classic*; the game then shows
  the 320×200 frame with pillarboxing.

## 6. Saves, configuration and logs

User files live in the standard XDG config directory:

    $XDG_CONFIG_HOME/opentyrian/     # or ~/.config/opentyrian/

| File | Contents |
|---|---|
| `opentyrian.cfg` | video/presentation/input settings |
| `tyrian.cfg` | in-game options and key/button bindings |
| `tyrian.sav` | saved games |
| `opentyrian.log` | log, written automatically on a Deck |

If you create `opentyrian.cfg` **next to the executable** instead, all of these
files are stored there (portable mode).  Each release archive ships without a
configuration file, so the default is `~/.config/opentyrian/`.

## 7. Performance

The game logic keeps the original fixed tick (~35 Hz); the renderer draws at the
display refresh with interpolated motion (*smooth motion*, on by default), so on
a Deck it runs at the panel's 60 Hz (LCD) or up to 90 Hz (OLED) without touching
gameplay.

A quick check, using one of the recorded demos:

    ~/Games/opentyrian/opentyrian --regress-demo=1 --regress-realtime --bench-seconds=20

This opens a window, replays the demo against the wall clock and logs the
presented frames per second.  `--smooth-motion=off` falls back to the classic
35 Hz cadence if you prefer the original timing.

Suggested in-game settings on the Deck: Modern presentation, aspect *auto*,
lighting *low* (the default).  Turn lighting to *off* for the longest battery
life and the steadiest 90 Hz.

## 8. Troubleshooting

* **Nothing happens when launched.**  Run it from a Desktop Mode terminal to see
  the text output, and read `~/.config/opentyrian/opentyrian.log`.
* **Black screen or no window.**  Force a video backend from a terminal:
  `SDL_VIDEODRIVER=wayland ./opentyrian` or `SDL_VIDEODRIVER=x11 ./opentyrian`.
  Game Mode uses gamescope (Wayland) with XWayland available.
* **No sound.**  SDL tries PipeWire, then PulseAudio, then ALSA.  Force one with
  `SDL_AUDIODRIVER=pipewire ./opentyrian`.
* **Controller not detected.**  The log records the joystick enumeration; check
  that no Keyboard/Mouse Steam Input template is applied and that the layout is
  *Gamepad*.
* **Game data not found.**  The data directory is looked up next to the
  executable first; pass `--data=/path/to/data` if you keep it elsewhere.

## Verified on the Deck

What could be checked without physical hardware is covered by CI (the package is
run in a clean Debian container) and by the regression suite.  A real Steam Deck
(or SteamOS VM/HoloISO) is still needed to confirm: fullscreen presentation
through gamescope at 1280×800, the built-in controller with the default Steam
Input layout, the on-screen 60/90 Hz behaviour, and the docked 16:9 case.
