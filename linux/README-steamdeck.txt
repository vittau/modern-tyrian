OpenTyrian - Steam Deck (Linux) quick start
===========================================

This archive is self-contained: the SDL3 and SDL3_net libraries are linked
into the binary, so there is nothing to install.  It needs only glibc, which
SteamOS 3.5+ provides.  Extract it anywhere in your home directory and run
./opentyrian.

Install
-------
1. In Desktop Mode, extract the archive, for example:

       mkdir -p ~/Games
       tar -xzf opentyrian-<version>-linux-x86_64.tar.gz -C ~/Games

2. If the executable bit was lost (FAT drive, some browsers):

       chmod +x ~/Games/opentyrian/opentyrian

Add to Steam
------------
1. Desktop Mode -> Steam -> bottom-left "Add a Game" -> "Add a Non-Steam Game".
2. "Browse..." (set the filter to All Files) and pick:
       ~/Games/opentyrian/opentyrian
3. "Add Selected Programs".  Launch options are not needed: the game finds
   data/ next to the executable, and on a Deck it starts fullscreen with the
   Modern 16:10 canvas.

Controller
----------
The built-in controls are standard SDL gamepads, so no Steam Input template is
needed.  Leave the controller layout at the default "Gamepad"; do not apply a
Keyboard/Mouse template.  Remap buttons in-game under Setup -> Joystick.

Saves, configuration and logs
-----------------------------
    ~/.config/opentyrian/
        opentyrian.cfg   settings (presentation, aspect, input)
        tyrian.cfg       in-game options
        tyrian.sav       saved games
        opentyrian.log   log, written automatically on a Deck

Add "--log-file=/home/deck/opentyrian.log" to the Steam launch options to put
the log somewhere else.

More
----
Full guide with troubleshooting: docs/STEAM_DECK.md in the OpenTyrian source
repository (https://github.com/vittau/modern-tyrian).

Licenses
--------
OpenTyrian is GPL-2.0-or-later (see COPYING).  The bundled Tyrian 2.1 data is
freeware (see licenses/Tyrian.txt).  SDL3 and SDL3_net licenses are in
licenses/.
