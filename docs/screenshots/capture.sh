#!/bin/sh
# Regenerate the README screenshots with the game's own headless snapshots.
#
#   ./docs/screenshots/capture.sh        # from the repository root
#
# Needs ./opentyrian built (`make`) and the game data in ./data
# (`./get_data.sh`).  Writes the BMPs to a temporary directory, then renders
# them into the PNGs next to this script with render.py.  No game data is
# committed; the screenshots are of the freeware Tyrian 2.1 running in this
# port.
#
# The hero and comparison frames come from episode 4, level 16 (SAVARA LV),
# started through the episode script (`--regress-script`) so `playDemo` is
# false and no INSERT COIN is drawn.  `--constant` runs the engine's built-in
# constant-play (a hidden test flag, not in --help) so the ship fires and
# enemies die; `--regress-seed` makes the run reproducible.  Frame 500 is a
# busy moment (enemy shots and an explosion) with full shield/armor.
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
out="${TMPDIR:-/tmp}/opentyrian-screenshots"
mkdir -p "$out"
cd "$root"

# 1. Hero: Modern 16:9, lighting and VFX at High, frame 500.
./opentyrian --regress-script=4:16 --regress-seed=32402394 --regress-frames=520 \
    --constant --regress-modern --regress-aspect=16:9 \
    --regress-lighting=high --regress-vfx=high \
    --regress-snapshot=500:"$out/hero.bmp" --no-sound

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

python3 "$root/docs/screenshots/render.py" "$out"
