# README screenshots

Gameplay captures use only the freeware **Tyrian 2.1** data. The launcher uses
art generated for this port; it loads no Tyrian 2000 data.

| File | Capture |
| --- | --- |
| `launcher.png` | Launcher, 1280×720, Tyrian 2.1 selected, Tyrian 2000 not installed |
| `hero.png` | SAVARA IV, episode script section 4:16, frame 500, Modern 16:9 |
| `arcade.png` | TYRIAN, episode 1 via the Arcade menus, frame 800, Modern 16:9 |
| `menu.png` | Game Menu, Modern 16:9 |
| `classic-vs-modern.png` | SAVARA IV at the same tick in Classic and Modern |

From the repository root, after `make` and `./get_data.sh`:

```sh
./docs/screenshots/capture.sh
```

The script uses the game's headless snapshot support, a fixed seed and constant
play for reproducible captures. `render.py` scales gameplay with nearest-neighbour
and the original 1.2 pixel aspect. The launcher exports its native PNG directly.
