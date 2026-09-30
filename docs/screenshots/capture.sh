#!/bin/sh
# Regenerate the README screenshots with the game's own headless snapshots.
#
#   ./docs/screenshots/capture.sh        # from the repository root
#
# Needs ./opentyrian built (`make`) and the game data in ./data
# (`./get_data.sh`), plus cjpeg from libjpeg-turbo 3 or later (it reads PNG).
# Writes BMPs to a temporary directory for render.py; the CRT hero comes
# directly from the game's presenter as a PNG that cjpeg turns into hero.jpg
# at quality 95 with full-resolution chroma, so the NTSC colour fringes
# survive. No game data is committed; the screenshots are of the freeware
# Tyrian 2.1 running in this port.
#
# The hero and comparison frames come from episode 4, level 16 (SAVARA IV),
# started through the episode script (`--regress-script`) so `playDemo` is
# false and no INSERT COIN is drawn.  `--constant` runs the engine's built-in
# constant-play (a hidden test flag, not in --help) so the ship fires and
# enemies die; `--regress-seed` makes the run reproducible.  Frame 500 is a
# busy moment (enemy shots and an explosion) with full shield/armor.
set -eu
export SDL_VIDEO_DRIVER=dummy
export SDL_AUDIO_DRIVER=dummy

root=$(cd "$(dirname "$0")/../.." && pwd)
out="${TMPDIR:-/tmp}/opentyrian-screenshots"
mkdir -p "$out"
cd "$root"

# 1. Hero: Modern 16:9, lighting and VFX at High, CRT Both, frame 500.
./opentyrian --regress-script=4:16 --regress-seed=32402394 --regress-frames=520 \
    --constant --regress-modern --regress-aspect=16:9 \
    --regress-lighting=high --regress-vfx=high \
    --regress-crt=scanlines+ntsc --regress-crt-window=1280x720 \
    --regress-snapshot=500:"$out/hero.bmp" \
    --regress-present-png=500:"$out/hero.png" --no-sound
cjpeg -quality 95 -sample 1x1 -optimize \
    -outfile "$root/docs/screenshots/hero.jpg" "$out/hero.png"

# 2. Menu/shop screen: the in-game Game Menu, widened to the Modern canvas.
./opentyrian --regress-screen=game-menu --regress-modern --regress-aspect=16:9 \
    --regress-lighting=low --regress-vfx=low --regress-frames=90 \
    --regress-snapshot=80:"$out/menu.bmp" --no-sound

# 3. Classic (the raw 8-bit frame) and 4. Modern, the same 4:16 frame 500.
./opentyrian --regress-script=4:16 --regress-seed=32402394 --regress-frames=520 \
    --constant --regress-snapshot=500:"$out/classic.bmp" --no-sound
./opentyrian --regress-script=4:16 --regress-seed=32402394 --regress-frames=520 \
    --constant --regress-modern --regress-aspect=16:9 \
    --regress-lighting=low --regress-vfx=low \
    --regress-snapshot=500:"$out/modern.bmp" --no-sound

# Launcher artwork is generated for this port; no Tyrian 2000 data is loaded.
./opentyrian --regress-launcher=1280x720,missing,1 \
    --launcher-png="$root/docs/screenshots/launcher.png"

# TYRIAN, entered through the real Arcade menus, with its lives HUD.
./opentyrian --variant=2.1 --regress-flow=single-arcade:ep=1,ticks=0 \
    --regress-frames=520 --constant \
    --regress-modern --regress-aspect=16:9 \
    --regress-lighting=high --regress-vfx=high \
    --regress-snapshot=500:"$out/arcade.bmp" --no-sound

# Native Deck aspect: SAVARA IV with its narrower Modern HUD.
./opentyrian --variant=2.1 --regress-script=4:16 --regress-seed=32402394 \
    --regress-frames=520 --constant --regress-modern --regress-aspect=16:10 \
    --regress-lighting=high --regress-vfx=high \
    --regress-snapshot=500:"$out/deck.bmp" --no-sound

python3 "$root/docs/screenshots/render.py" "$out"
