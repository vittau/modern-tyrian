# AGENTS.md — Modern Tyrian

A modernized OpenTyrian (C99, SDL3) that plays two games from one binary:

- **Tyrian 2.1**, the freeware release. Its data ships in `./data`.
- **Tyrian 2000**, ported from the KScl/opentyrian2000 fork at `aad5aca` (GPL-2.0). Its data is **never** in this repo; the user installs it.

Plans and journals are in Portuguese, in `MODERNIZATION_PLAN.md` (the general modernization) and `MODERNIZATION_PLAN_2000.md` (the Tyrian 2000 track: architecture §5, test matrix §7, rules §9–10, user decisions §12, diary §13). Research lives in `docs/t2000/`:

- `fork-diff.md`: the fork ledger;
- `data-formats.md`: file formats, counts and the save layout;
- `integration.md`: the integration points.

## Hard rules

- **2.1 must not regress.** Classic mode is byte-exact with upstream: the same 8-bit framebuffer and the same RNG order. Every 2.1 baseline change needs an explicit reason.
- **The Modern layer is presentation only.** It never writes game state and never calls `mt_rand*`. Extra effects never go through `JE_setupExplosion`/`JE_doSP`.
- **Tyrian 2000 data never goes into git, releases, LFS, caches, logs or any project host.** That covers the zip, any file from it, and any extracted sprite, string, map or screenshot.
  - Tests commit only hashes and metadata (name, size, CRC).
  - `tools/check_no_t2000_data.sh` enforces this in `make regress` and before packaging.
- **Variant differences live in tables and hooks**, never in scattered `if (variant)`. Nothing is removed from 2.1 to fit 2000.
- **Port fork logic by understanding it.** Never paste fork files; cite the fork commit when porting.
- **Portability:** no absolute user paths in committed files. Regression outputs are opened with `"wb"` (Windows CRLF broke hashes before). New `.c/.h` files also go into `visualc/opentyrian.vcxproj` and `.filters` (the `.vcxproj` is CRLF).
- **Audit every touched `.c`:** `gcc-16 -std=iso9899:1999 -pedantic -Wall -Wextra -Wno-format-truncation -Wno-missing-field-initializers -Werror -fsyntax-only -DTARGET_UNIX -DWITH_NETWORK -I/opt/homebrew/include <file>`.
  - MinGW rejects `%zu` in the `logWarn`-family printf checks; cast to `unsigned long` and use `%lu`.

## Build and test

```sh
make                         # release;  `make debug` = -O0 -Werror, asserts on (run `make clean` between them)
make regress                 # 2.1: 172 cases + guards, ~1–3 min
make regress-2000 TYRIAN2000_DATA=<dir>   # 2000: ~150 cases, needs the 2000 data (clear error if missing)
tools/regress.sh --update                 # regenerate 2.1 baselines (intentional output changes only)
tools/regress-2000.sh --update-case=<label>   # regenerate one 2000 baseline; --only-launcher for launcher hashes
```

- **Harness:** headless (`SDL_VIDEO_DRIVER=dummy`) with a virtual clock. Cases compare per-tick frame/state hashes. Coverage lines in the log, such as `Rule coverage: …`, `Rule fixture PASS: …` and `boss N active …`, prove that a path really ran; a hash alone does not.
- **Useful `--regress-*` options:**
  - scenarios: `demo=N`, `level=E:L`, `script=E:L`, `screen=<name>`, `modern`, `aspect=`, `detail=`, `frames=N`, `players=N`, `seed=`;
  - `flow=` is a virtual keyboard that walks the real menus: Full Game, Timed Battle, arcade, Super Tyrian, Destruct;
  - `rules=<fixture>`, `items-new`, `fire`, `demo-hud-check`, `gameplay-check`, `data-audit`, `boss`, `handoff`, `gamepad`, `xmas`, `user-root=`/`user-files`.
  - CRT: `crt=off|scanlines|ntsc|scanlines+ntsc` (Modern filtered-output hashes), `crt-check` (synthetic fixtures + measurements), `crt-height=N`, `crt-window=WxH`, `present-png=FRAME:FILE` (2.1 only).
  - Launcher-only flags are parsed before `params.c`: `--regress-launcher=WxH,installed|missing,1|2[,about|message|…]`, `--launcher-png=`, `--launcher-flow=`.
- **`params.c` option-id ranges.** Pick a new range; beware getopt prefix matching (`--regress-loadout-new` broke `--regress-loadout`).

  | Range | Owner |
  |---|---|
  | 313 | `--variant` |
  | 314–315 | user paths |
  | 320–329 | 3b rules |
  | 330–339 | installer |
  | 350–359 | 3c |
  | 360–369 | Phase 4 |
  | 370–379 | Phase 7 |
  | 380–399 | Phase 8 |
  | 400–409 | display |
  | 410–419 | CRT filter |
  | 420–429 | progress |

- **Guards run by `make regress`:**
  - `check_no_t2000_data.sh`, `check_variant_bootstrap.sh`, `check_user_paths.sh`, `check_game_rules.sh`;
  - `check_installer.sh` (synthetic zips plus `tools/curl_stub.c` named by absolute path in a test spec; it can never reach the network);
  - `check_final_regression.sh` (the §7 matrix: data-open audit, save namespaces, UI save/load, bosses, pause, gamepad, launcher handoff);
  - `check_display.sh` (runtime display changes, modal centring, launcher first frame).
- **Never run the long `--interp-check`/`--smoothness-check` sweeps locally.** They run in the manual GitHub workflow `regress-full.yml`.
- **CI** (`.github/workflows/{linux,macos,windows}.yml`) runs on every push to any branch:
  - build, `make regress`, then `tools/fetch_t2000_data.sh` (camanis.net, exact size + SHA-256, into `$RUNNER_TEMP`, **no Actions cache**, because a public repo's cache is readable from fork PRs) and `make regress-2000`;
  - on failure it uploads only `*.txt`/`*.log`, and releases ship 2.1 data only.
  - Windows is MSYS2: no shell scripts through `CreateProcess`, MSYS vs Windows paths (use `cygpath`), and `python` must be installed.

## Architecture (startup order)

1. **`bootstrap.c`** (`gameBootstrapParse`), first in `main`: early flags (`--variant`, `--data`, regress/selftest). User files are disabled until known.
2. **`game_variant.[ch]`:** `GameVariantDef` for 2.1/2000 holds the names, `save_namespace` (`tyrian21`/`tyrian2000`), the episode count (4/5) and pointers to the schema, UI and rule tables. `gameVariantCurrent()`.
3. **`launcher.[ch]`:** the in-binary first screen, drawn with SDL at the native output size, with PNG art embedded via `tools/embed_assets.sh` → `obj/launcher_art.c` (VS: `visualc/embed_assets.ps1`).
   - It always opens on a normal start, including the Deck. `--variant=`, regress and selftest skip it.
   - `launcher/last_variant` in the shared `opentyrian.cfg` only preselects a panel.
   - Its INSTALL flow drives the installer, with a "place the files" screen when no file dialog exists (Deck Game Mode, `OPENTYRIAN_NO_DIALOGS`).
4. **`game_data.[ch]`:** `GameDataProvider` locates, validates (wrong-variant diagnostics both ways) and opens data read-only, with no per-file fallback between directories.
   - The 2000 search order is `--data`, `TYRIAN2000_DATA`, the installer location, `tyrian2000/` beside the exe, then `./tyrian2000`.
5. **`file.c`:** user root, per-variant namespaces and the one-time 2.1 migration (`userPathsMigrateLegacy21`), which runs before `loadSaves`. The configs (`opentyrian.cfg`, `tyrian.cfg`) are shared in the root.
6. **Loaders read `game_schema.[ch]`** (`gameSchema()`, `gameStrings()`, semantic labels such as `GAME_LABEL_*`) for item/enemy banks, string sections, shape banks (12/13), pictures and palettes, sfx and voices, credits and episodes.
   - Rules come from `game_rules.[ch]` (`gameRules()`: events 58/59/68/83/84/85/99, launch/spawn, twiddles, arcade ships, charging sidekicks, Flying Punch, cash, Timed Battle).
   - UI comes from `gameUi()` (menus, options, the Mouse menu, the title mark). `highscores.[ch]` holds both variants' boards.
7. **The Modern renderer** (`modern*.c`, `vfx*.c`, `drawlist.c`, `interp.c`) is variant-agnostic.
   - Light and tags come from the object context, not sprite ids.
   - The HUD uses semantic labels.
   - Attract demos follow the active mode's HUD.
   - Modals over the widened pic-1 layout use `modern_dialog_begin/end`.
   - **CRT filter** (`crt_filter.c`, `ntsc.c`): Modern-only, after canvas capture and all passes in `modern_present_frame`; read-only canvas, original Fit rect/mouse map. Output width follows NTSC, scanline height is `2*src` only for `dst % (2*src) == 0`, otherwise native Fit height (or `src` below `2*src`); buffer/texture resize only on dimension changes, ~16 MiB NTSC table built lazily and freed at shutdown. See `docs/CRT.md`.
   - Window changes must settle with `SDL_SyncWindow` before geometry is read, because Cocoa is asynchronous.

## Data, saves, installer

- **2000 archive:** `https://www.camanis.net/tyrian/tyrian2000.zip`, 5,051,363 bytes, SHA-256 `348bc76e…87e921667`. The constants are in `installer.h`, and the 79-file manifest is `test/regress-2000/data-manifest.txt` = `installer_manifest.h`.
- **`installer.[ch]`** is a non-blocking SDL-thread job.
  - It detects, downloads through the system `curl` (no shell, https only), verifies, extracts with the vendored `third_party/miniz` (MIT, inflate only), validates, and installs by atomic rename.
  - It also installs from a zip or folder (GOG): non-canonical but valid data is accepted and logged.
  - Headless CLI: `--install-2000=`.
- **Install locations:**

  | Platform | Location |
  |---|---|
  | Windows | `%APPDATA%\OpenTyrian\data-tyrian2000` |
  | macOS | `~/Library/Application Support/OpenTyrian/data-tyrian2000` |
  | Linux/SteamOS | `$XDG_DATA_HOME` or `~/.local/share/opentyrian/data-tyrian2000` |
  | Portable (`opentyrian.cfg` beside the exe) | beside the exe |

  The downloaded `.part` file is deleted afterwards. Steam's sniper runtime ships curl.
- **Saves:**
  - 2.1: `tyrian21/tyrian.sav`, 2,502 bytes.
  - 2000: `tyrian2000/tyrian.sav`, 4,722 bytes (the 2,502-byte prefix + an unencrypted high-score suffix; an unknown 4-byte field is preserved).
  - Each variant never reads the other's save. The root 2.1 save is copied, never moved.

## Open items

- **DOS checks:** the fork's approximations (events 58/59/68, trail 198, Timed Battle routing and bonuses, Super Tyrian state 8, the Pretzel Pete sprite size) are listed in the Phase 4 diary. They need the DOS original.
- **Manual only:** a physical Steam Deck, the native Windows file picker and download, and a physical ultrawide/HiDPI Cocoa.
