# README screenshots

These are captures of the freeware **Tyrian 2.1** running in this port. No game
data is stored here — only the images below, all produced by the game's own
headless snapshot support (`--regress-snapshot=FRAME:FILE`, which writes a BMP).

| File | What it shows |
| --- | --- |
| `hero.png` | Modern 16:9 gameplay, lighting High, VFX High — the glass side HUD, enemy shots and an explosion |
| `menu.png` | The in-game **Game Menu**, widened to the Modern canvas |
| `classic-vs-modern.png` | The same frame in Classic (original 4:3 with the sidebar HUD) and Modern 16:9 |

## Regenerate

From the repository root, with `./opentyrian` built and `./data` present:

```bash
make
./get_data.sh
./docs/screenshots/capture.sh
```

`capture.sh` runs the four snapshot commands and then `render.py`. The hero and
comparison frames come from **episode 4, level 16 (SAVARA LV)** at frame **500**,
started through the episode script (`--regress-script=E:L`) so `playDemo` is
false and no `INSERT COIN` is drawn. `--regress-seed` pins the RNG, and
`--constant` runs the engine's built-in constant-play (a hidden test flag, not
listed in `--help`) so the ship fires and enemies die; the chosen frame has full
shield/armor (10/10) and shows enemy shots and an explosion. The four commands:

```sh
# hero — 4:16 frame 500, Modern 16:9, lighting High, VFX High
./opentyrian --regress-script=4:16 --regress-seed=32402394 --regress-frames=520 \
    --constant --regress-modern --regress-aspect=16:9 \
    --regress-lighting=high --regress-vfx=high \
    --regress-snapshot=500:hero.bmp --no-sound

# menu — the in-game Game Menu, Modern 16:9
./opentyrian --regress-screen=game-menu --regress-modern --regress-aspect=16:9 \
    --regress-lighting=low --regress-vfx=low --regress-frames=90 \
    --regress-snapshot=80:menu.bmp --no-sound

# comparison — Classic (raw 8-bit frame) and Modern of the same 4:16 frame
./opentyrian --regress-script=4:16 --regress-seed=32402394 --regress-frames=520 \
    --constant --regress-snapshot=500:classic.bmp --no-sound
./opentyrian --regress-script=4:16 --regress-seed=32402394 --regress-frames=520 \
    --constant --regress-modern --regress-aspect=16:9 \
    --regress-lighting=low --regress-vfx=low \
    --regress-snapshot=500:modern.bmp --no-sound
```

`render.py` upscales with nearest-neighbour and the original **1.2 pixel aspect**
(each source pixel is 1.2× taller than wide, as on a 4:3 CRT), and composes the
Classic/Modern pair at the same display scale. Standard library only.

The capture is deterministic: the regression modes use a fixed clock, and
`--regress-seed` plus `--constant` reproduce the same BMPs byte for byte.
