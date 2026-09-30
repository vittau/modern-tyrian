# README screenshots

Gameplay captures show the current bevelled glass HUD and use only the freeware
**Tyrian 2.1** data. The launcher uses
art generated for this port; it loads no Tyrian 2000 data.

| File | Capture |
| --- | --- |
| `launcher.png` | Launcher, 1280×720, Tyrian 2.1 selected, Tyrian 2000 not installed |
| `hero.jpg` | SAVARA IV, episode script section 4:16, frame 500, Modern 16:9, CRT Both (scanlines + NTSC), presented at 1280×720 |
| `arcade.png` | TYRIAN, episode 1 via the standard Arcade menu, frame 500, Modern 16:9 |
| `savara-deck-16x10.png` | SAVARA IV, frame 500, Modern 16:10, 1280×800 |
| `steam-deck.png` | Steam Deck mockup with the 16:10 capture inside the active screen; black bezels preserved |
| `menu.png` | Game Menu, Modern 16:9 |
| `classic-vs-modern.png` | SAVARA IV at the same tick in Classic and Modern |

From the repository root, after `make` and `./get_data.sh`:

```sh
./docs/screenshots/capture.sh
```

The script uses the game's headless snapshot support, a fixed seed and constant
play for reproducible captures. The hero uses the game's presented CRT output;
`render.py` scales the other gameplay images with nearest-neighbour and the
original 1.2 pixel aspect. The launcher exports its native PNG directly.

The Steam Deck mockup uses the [Valve frame supplied for this task](https://clan.fastly.steamstatic.com/images//39049601/e54b85b6e75bc7ec589372474ef1705b3471bb66.png)
and `savara-deck-16x10.png`. The final image is an exact composition, not an AI
redraw: `compose-deck.swift` changes only the active screen rectangle and
preserves the hardware, black bezels and screenshot layout.

On macOS, download the supplied frame and run:

```sh
swift docs/screenshots/compose-deck.swift FRAME.png \
    docs/screenshots/savara-deck-16x10.png docs/screenshots/steam-deck.png
```
