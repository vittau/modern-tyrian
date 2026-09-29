# Tyrian 2000 integration design

## Scope and acceptance

This is a C99 proposal against our `a2d82d82bb0ac83a2253b18fc5d9ee231a7f61b9` tree. Phase 2 implements the abstraction with **2.1 as its only usable variant**, before Phase 3 ports 2000 content/rules. One engine, renderer, SDL3 input path, audio path and binary serve both. The launcher is an in-binary Modern screen on every normal start, including Deck; an explicit `--variant=2.1|2000` bypass is for automation. Presentation/controls are shared; saves, high scores and progress are separated. No fork SDL2 code or original assets are copied.

The fork delta and detailed loader schemas are in [fork-diff.md](fork-diff.md) and [data-formats.md](data-formats.md). Existing 2.1 baselines are a gate, never regenerated merely to accommodate the architecture. The unmodified tree built with GCC-16 C99 warnings-as-errors and passed **all 164 regression cases** during this research. A headless probe with verified 2000 data exited 1 at the intentional shape-header rejection (`src/opentyr.c:1107`). It did not exercise 2000 gameplay.

## Integration points and Modern risks

| Our file / owner | Required integration | Specific risk and validation |
|---|---|---|
| `src/opentyr.c:974`, `main` | Early variant/data/bootstrap parsing; split shared config from variant saves; validate provider before variant assets/default scores | **High:** current config/saves load before full parsing, and 2000 HDT supplies score defaults. Preserve regress/selftest user-file suppression, Deck log setup, command-line presentation precedence and missing-data failure safety |
| `src/params.c:51`, `JE_paramCheck` | Add `--variant=` and let bootstrap share recognized data/variant values | **High:** parsing only here is too late. Repeated parsing must not reset or reinterpret chosen provider. Variant bounds also govern demo, port and episode arguments |
| `src/file.c:102`, `findDataFiles`; `:179`, `dataFileOpen` | Provider-backed root resolution and immutable installation validation | **High:** current discovery accepts a directory from only `tyrian1.lvl`; do not accidentally mix 2.1 assets into a partial 2000 installation. Keep error-state `File` semantics |
| `src/file.c:203`, `determineUserDirPath`; `:312`, `userFileOpen` | Explicit shared vs variant file category and non-destructive migration | **High:** portable base-path detection and Deck defaults must remain stable; failed migration must not create blank saves |
| `src/config.c:253,400`, modern config load/save | Shared presentation, scale, gain, keyboard/mouse/gamepad/analog configuration | **High:** fork config is SDL2-era and lacks Modern settings. Keep `processorTypeChoice` plus Modern effective Pentium detail, Deck logging and analog/remap values |
| `src/config.c:749,837,888,989`, legacy config/save I/O | Variant `tyrian.cfg` version and prefix/suffix save codec | **High:** exact 2.1 bytes and prefix integrity must remain unchanged; 2000 suffix and boards cannot be written by 2.1 |
| `src/episodes.c:54,247,264` | Schema-selected item-bank loops, episode cap and resource names | **Medium:** engine arrays may reserve the larger maximum but serialized loops and ID holes are variant-specific. Existing five-slot episode storage is not complete E5 support |
| `src/lvllib.c:34`, `analyzeLevel`; `src/tyrian2.c:2433`, `JE_loadMap` | Keep shared offset/event/map decoding, validate selected schema IDs | **High:** malformed/new bank IDs and event maxima feed unchecked arrays; distinguish physical levels from episode script sections |
| `src/helptext.c:165`, `JE_loadHelpText`; `src/helptext.h:33` | Variant string section descriptors and semantic aliases | **High:** the earliest misc count difference shifts everything. Raw menu/HUD indices cannot be assumed after a partial load |
| `src/lvlmast.c:23`, shape-file table; `src/lvlmast.h:26` | Variant bank filename/ID tables and maximum capacities | **Medium:** never consume serialized holes or map ID 1000 as an enemy when fork starts second enemies at 1001 |
| `src/sprite.c:977`, `JE_loadMainShapeTables`; `:1044`, `free_main_shape_tables` | 12/13 banks, generic sprite cap, added ship bank and cleanup | **Medium:** retain all drawlist-recording blitters. Added bank must be owned/freed once, never outlive commands referencing it |
| `src/varz.c:317`, `JE_getShipInfo`; `src/game_menu.c`, `JE_drawItem` | Resolve `(sheet,index)` from ship graphic ID centrally | **High:** fork adjusts mainly player one; verify player two, custom ship >90, shop preview, death/respawn and Super Tyrian |
| `src/picload.c:34`, `JE_loadPic`; `src/pcxmast.c:23`; `src/palette.c:42`, `loadPals` | Variant counts/palette mapping and per-provider cached-offset lifetime | **Medium:** static first-load picture offsets cannot survive switching providers; larger arrays must not alter 2.1 reads |
| `src/modern.c:1317`, `modern_pic1_widens`; `:1419`, `modern_compose_vert` | Backdrop layout descriptor and 2000 per-screen verification | **High:** fixed pic-1 split/header rows and pic-2 credit palette range may misidentify original art as menu elements. Preserve inverse mouse mapping |
| `src/modern_hud.c:496,553,666,776` | Semantic player/timer labels and expanded weapon/sidekick names | **High:** longer strings must fit narrow 2P panels; Timed Battle timer/bonus must not collide with boss bars or duplicate Classic UI |
| `src/mainint.c:2417`, `JE_playCredits` | Variant record count, retain canvas-wide procedural credit renderer | **High:** requesting 131 records from 126 hits EOF; do not copy the fork’s narrow Classic-only loop over our widened grid/starfield |
| `src/drawlist.h:47`; `src/drawlist.c:344,571,643,1349` | Classify added sprites by object role; retain per-sprite palette-derived emission and interpolation keys | **High:** wrong context can light ships/HUD or blend replacement enemies across identities. No asset-specific colour table is required by default |
| `src/interp.c`, gameplay present/reset; `src/tyrian2.c:702,1210` | Keep fixed tick and same replay/interpolation capture for added objects/events | **High:** battle timer is a logic counter, not real time. Reset history on levels/provider changes; preserve alpha=1 replay identity |
| `src/mainint.c:3381`, `JE_playerMovement`; `src/keyboard.c`, `handleSdlEvents`; `src/gamepad.c` | Sidekick main-fire policy and mouse actions through common input | **High:** fork mouse state is logical actions, ours tracks physical input; do not break analog fractional movement, remap or real-input suppression in regress |
| `src/game_menu.c`, `JE_itemScreen`, `JE_weaponSimUpdate`, `JE_drawShipSpecs`, modal functions | Variant rows/help/eligibility and common new save-confirmation modal | **High:** current widened pic-1 maps, custom joystick rows and centered quit box override fork topology. New text alters element extents and cursor hit tests |
| `src/menus.c`, `gameplaySelect`, `episodeSelect`; `src/tyrian2.c`, `titleScreen`, `newGame`, `newSuperTyrianGame` | Descriptor-backed mode list, battle choice, new logo and Super Tyrian episode selection | **High:** preserve modern backdrop tracking/center offsets, A/B/Esc cancellation and no stale battle mode after load |
| `src/tyrian2.c:4325`, `JE_eventSystem`; `JE_makeEnemy`, `JE_createNewEventEnemy` | Event dispatch table/hook for 58/59/68/83/84/85/99, launch decoder and spawn sentinel | **High:** 68 changes meaning; RNG and allocation order affect state and all future ticks. Reference approximation must not silently become a common fix |
| `src/shots.c`, `player_shot_create/move_and_draw`; `src/varz.c`, `JE_setupExplosion`, `JE_playerDamage` | Flying Punch center trail, explosion 54 and battle death policy | **High:** these are gameplay allocations, not cosmetic VFX. Retain contexts and secondary particle RNG separation |
| `src/nortsong.c:137,213`; `src/loudness.c:274`; `src/sndmast.h:24` | Runtime sample counts, variant semantic sound IDs; reusable music and resampler | **Medium:** 31 effects shift all voice IDs; cached song offsets/counts must reset on data change. Keep Nuked-OPL3 loudness and sample-rate tests |
| `src/demo.c:55`; `src/params.c:343` | Variant demo list/count; retain decoder and deterministic seed | **Low:** all five 2000 demos are identical bytes to 2.1; maps/rules still require different baselines |
| `src/xmas.c:35`; `src/params.c`; `src/opentyr.c:1131` | Explicit override vs seasonal prompt, preserve optional-asset checks | **Medium:** regress forcibly disables Christmas now; new test-only asset-profile option is needed to cover it deterministically |
| `src/regress.c:382,733`; `src/regress_screen.c:111,139,161`; `src/regress_audio.c:65` | Variant state additions and stable, data-independent score/save seeds | **High:** preserve existing 2.1 hash stream and fixtures exactly. New mode/table data needs a versioned 2000 state extension |
| `tools/regress.sh:85,90,101,179–250`; `test/regress/data-manifest.txt:1`; `Makefile:232` | Separate baseline/manifest/suite routing for `make regress-2000` | **High:** never update 2.1 manifest using 2000 data; no implicit downloader fallback to 2.1 |
| `.github/workflows/linux.yml`, `macos.yml`, `windows.yml`, `regress-full.yml` | Verified external data acquisition and isolated 2000 checks | **Medium:** release archive staging must stay 2.1-only; failure artifacts must exclude original assets |

### Pic-1 widening, pic-2 credits and new menu rows

Our widening has concrete 2.1 assumptions: split minimum 170, split maximum `MODERN_PIC1_SPLIT_MAX`, help band starts at y=184, header handling ends at y=33 and repeats the header interior through x=310 (`src/modern.c:219–237`). `modern_pic1_widens` scans drawn element differences and picks a free column; a new mouse menu, longer 2000 help or right-aligned episode-5 label can change the chosen split and inverse mouse mapping. Keep the established algorithm, supply a `BackdropLayout` descriptor, and verify the actual 2000 pic-1 artwork/each affected menu before retaining those constants for it.

Pic-2 Vert- crops and re-composites credit pixels at rows 192–198 with palette indices 35–39 (`src/modern.c:239–244,1456–1474`). These are artwork heuristics, not format guarantees. Verify title, mode/episode/battle/difficulty, high scores and name entry at 16:9, 21:9 and 32:9; compare element bounding boxes and reverse hit tests. The added title badge must be drawn as an element after backdrop capture so its movement stays sharp. Credits’ scrolling-text record count belongs to string schema; its widened canvas is a shared renderer feature, separate from pic-2’s baked footer.

### HUD labels, tags and interpolation

The HUD reads `miscText[48]`, `[49]` for player names (`src/modern_hud.c:503,934`) and `[66]` for timer fit (`:780`). Introduce semantic label IDs resolved by the selected string schema; the 2.1 mappings remain exactly those indices. Expanded port/sidekick/special names come from item data, pass through existing truncation and panel width checks. Cash under obscured vision needs Classic verification; Modern cash already lives outside the playfield. The timer currently participates in panel/boss placement; the fork’s y=15 Classic boss bars must not override Modern recorded boss gauges.

The plan describes class/colour tables, but current code uses **object contexts** for emission class (`drawlist.c:643`) and **per-sprite, per-palette representative colour/footprint analysis** (`:344–567`), with a cache cleared each tick (`:584–586`). New Dragon/Pretzel/Flying Punch assets should flow through those generic calculations. Ships, sidekicks, ordinary enemies, HUD and backgrounds stay non-emissive; shots, explosions, pickups and VFX receive their existing role tags. Check the new compressed bank and large shot brightness caps rather than adding an ID-based colour allowlist. This is a refinement of the plan, not a missing giant sprite table.

Interpolation keys currently mix object kind, slot ID and sub-ID (`src/drawlist.c:1365`), not a generation counter. A replacement in the same enemy slot could match the prior object and move smoothly between unrelated positions. Review existing primitive/sprite compatibility tests before adding an observed spawn/replacement epoch; if needed, add metadata that is absent from gameplay and 2.1 state hashes. New projectiles/trails/explosions must use recorded primitive blitters with correct contexts, not direct unrecorded surface writes. Run replay, alpha=1, smoothness and parallax checks on replacements and the Flying Punch. No extra `JE_setupExplosion` or RNG call is allowed from Modern presentation.

## Phase 2: concrete C99 API proposal

Use small internal headers, matching the existing `File`, explicit fixed-width types and `bool` conventions. The declarations below are proposed interfaces, not code implemented by this phase. Keep maximum allocation capacity separate from active serialized counts. No C++ classes, per-variant binary, or global `isTyrian2000` branch is needed.

### `src/game_variant.h`

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    VARIANT_TYRIAN21,
    VARIANT_TYRIAN2000
} GameVariant;

typedef struct {
    uint16_t first, last; /* Inclusive IDs; no bytes for holes. */
} GameIdBank;

typedef struct {
    const GameIdBank *weapon_banks;
    size_t weapon_bank_count;
    const GameIdBank *enemy_banks;
    size_t enemy_bank_count;
    uint16_t port_max, special_max, generator_max;
    uint16_t ship_max, sidekick_max, shield_max;
    uint16_t main_shape_banks, sprite_table_max;
    uint16_t picture_count, palette_count, warning_lines;
    uint16_t sfx_count, voice_count, credits_lines;
    const char *enemy_shape_files;
    size_t enemy_shape_file_count;
    const uint8_t *picture_palette;
} GameDataSchema;

/* Definitions live in dedicated internal schema/UI/rules headers. */
typedef struct GameStringSchema GameStringSchema;
typedef struct GameUiTables GameUiTables;
typedef struct GameRuleHooks GameRuleHooks;
typedef struct GameSaveCodec GameSaveCodec;
typedef struct GameDataManifest GameDataManifest;

typedef struct {
    GameVariant id;
    const char *cli_name;       /* "2.1" or "2000" */
    const char *display_name;   /* All variant labels derive here. */
    const char *log_suffix;
    const char *save_namespace; /* "tyrian21" or "tyrian2000" */
    uint8_t episode_count, demo_count, dos_config_version;
    const GameDataSchema *data_schema;
    const GameStringSchema *string_schema;
    const GameUiTables *ui;
    const GameRuleHooks *rules;
    const GameSaveCodec *save_codec;
    const GameDataManifest *manifest;
} GameVariantDef;

const GameVariantDef *gameVariantGet(GameVariant variant);
const GameVariantDef *gameVariantCurrent(void);
bool gameVariantParse(const char *name, GameVariant *out);
bool gameVariantSelect(GameVariant variant);
```

`gameVariantSelect` is valid only before variant initialization or after full teardown; callers cannot switch the immutable descriptor in the middle of a tick. Phase 2 exposes the 2000 enum/parser token but returns an explicit unavailable status for selection of the unimplemented provider/schema. Normal Phase-2 startup continues to select 2.1, until the real launcher exists. A reserved ID is not advertised as playable.

`GameStringSchema` holds section counts/field capacities, setup-skips, record destinations, semantic label mapping and source of default score names. `GameUiTables` holds available gameplay modes, title composition, menu/help topology, ship/weapon illustration offsets, `BackdropLayout`, Timed Battle destination policy, initial cash by episode, arcade progression/loadout/combo tables and score-board mappings. Keep existing storage shapes for Phase 2; later increase maximum capacities without increasing 2.1 reads.

`GameRuleHooks` should be narrow: a `bool handle_event(struct JE_EventRecType *event)` callback returning false to use common dispatch; enemy launch decode and spawn-X normalization; sidekick main-fire eligibility; rear-upgrade eligibility; score-board/completion policy; trail normalization; death timer policy. Shared event helpers implement timer/map stop/random explosions, selected through variant dispatch tables. Common functions own physics, tick sequencing, allocation and RNG. Use descriptive policy callbacks where inputs/outputs are sufficient; do not give renderer/provider objects access to gameplay mutation.

For Timed Battle use a common `GameMode` enum with full/arcade/two-player/Super Tyrian/timed values and a `TimedBattleRules` descriptor (selection-to-episode mapping, timer event, time/life bonus units, score policy). Phase 2 need not refactor all old mode booleans at once; initially expose a descriptor seam preserving old 2.1 call order. Phase 3 introduces battle session state behind that seam.

### `src/game_data.h`

```c
#include "file.h"
#include "game_variant.h"

typedef enum {
    GAME_DATA_OK,
    GAME_DATA_NOT_FOUND,
    GAME_DATA_WRONG_VARIANT,
    GAME_DATA_MISSING_FILE,
    GAME_DATA_BAD_SIZE,
    GAME_DATA_BAD_HASH,
    GAME_DATA_IO_ERROR,
    GAME_DATA_UNAVAILABLE
} GameDataStatus;

typedef struct {
    GameDataStatus status;
    char filename[64];
    char detail[256];
} GameDataError;

typedef struct {
    const char *override_directory; /* --data */
    const char *installed_directory;
    const char *package_directory;  /* 2.1 package only */
} GameDataSearch;

typedef struct GameDataProvider GameDataProvider;

GameDataStatus gameDataLocate(const GameVariantDef *variant,
                             const GameDataSearch *search,
                             GameDataProvider **out,
                             GameDataError *error);
GameDataStatus gameDataValidate(const GameDataProvider *provider,
                               GameDataError *error);
File gameDataOpen(const GameDataProvider *provider, const char *filename);
bool gameDataExists(const GameDataProvider *provider, const char *filename);
const char *gameDataDirectory(const GameDataProvider *provider);
const GameVariantDef *gameDataVariant(const GameDataProvider *provider);
void gameDataClose(GameDataProvider *provider);
```

`GameDataProvider` privately owns one normalized absolute root, a descriptor, validation result and explicit optional-asset profile. `gameDataOpen` is read-only (`rb`) and returns our checked `File`; it rejects traversal/absolute filenames. `dataFileOpen` can initially forward its existing read-only usages to the selected provider, minimizing loader churn. Provider construction locates a **whole installation**; it never falls back per missing file to the packaged 2.1 directory. An explicit bad `--data` path reports its error rather than silently choosing another installation.

2.1 discovery retains the current packaged/custom/system search order and regression strict manifest. Phase 2 records validation state without breaking legitimate existing 2.0/2.1 support or optional custom ship/Christmas files: define the exact canonical manifest as a strict regression profile, with the current compatible schema profile for normal 2.1 startup. Phase 3’s canonical 2000 provider requires its full approved runtime manifest and distinguishes wrong-variant header/counts; manually installed/GOG sets that differ from the canonical archive get a diagnostic and a separately reviewed compatibility profile, never automatic acceptance by filename alone.

Provider validation supplies schemas and diagnostics; loaders still own decoding. ZIP inspection/detection can return an install candidate, but extraction/HTTP belong to a separate installer in Phase 5. Do not make `gameDataOpen()` trigger downloads. Report variant, selected root, validation state and missing/invalid file names at startup. Add reset entry points for static picture/song offsets and all owned shape banks if returning to the launcher supports in-process reselection; alternatively fully tear down/reinitialize a game session through the same common init path.

### Bootstrap parsing and initialization order

Propose `src/bootstrap.h` with:

```c
typedef struct {
    bool variant_explicit;
    GameVariant variant;
    const char *data_directory;
    bool regress, selftest;
} GameBootstrapOptions;

bool gameBootstrapParse(int argc, char *argv[],
                        GameBootstrapOptions *out,
                        char *error, size_t error_size);
```

Call it at the top of `main`, before `loadConfiguration`, `loadSaves`, data discovery and episode scanning; ideally before `SDL_Init(0)` (`src/opentyr.c:992–1019`). It parses only startup ownership arguments (`--variant`, `--data`, regress/selftest), rejects unknown variant values and inconsistent duplicate ownership options, and shares those results with full `JE_paramCheck`. The full parser still validates all normal parameters, applies presentation overrides and sets gameplay options. Help/version exit before requiring game data; no user writes happen from an invalid variant argument.

The eventual normal startup sequence is:

1. Parse bootstrap and disable all user file operations for regress/selftest before paths or saves are loaded.
2. Resolve shared user root; load shared modern presentation/input settings needed by the launcher; preserve Deck log at the shared root. Initialize only shared SDL/video/controller resources for the in-binary launcher.
3. Choose variant through explicit automation selection or launcher. `last_variant` preselects a panel but never skips the launcher.
4. Select descriptor and user namespace, locate/validate provider; report errors before variant-dependent parsing or migration-dependent writes.
5. Load variant asset/string schema (and data-backed default names), then variant legacy config/save codec. Apply full CLI overrides and effective detail exactly as today for 2.1.
6. Scan available episodes within selected descriptor cap; initialize audio/shape/game state and enter the common loop.

Phase 2 has no launcher screen yet, so step 3 uses the 2.1 default or explicit supported override; retain the same 2.1 externally visible behaviour and stream. The interface prepares a launcher callback but does not silently implement a separate executable. Do not copy the fork’s entire startup reorder: it predates our upstream config split.

### User paths and migration

Propose `src/user_paths.h`:

```c
typedef enum {
    USER_FILE_SHARED_CONFIG,
    USER_FILE_SHARED_LOG,
    USER_FILE_VARIANT_CONFIG,
    USER_FILE_VARIANT_SAVE
} UserFileKind;

bool userPathsSelect(const GameVariantDef *variant);
File userFileOpenKind(UserFileKind kind, const char *name, const char *mode);
bool userPathsMigrateLegacy21(GameDataError *error);
```

Layout:

```text
<current user root>/
    opentyrian.cfg              shared presentation/controls, last_variant
    opentyrian.log              shared Deck/default log
    tyrian21/
        tyrian.cfg             legacy binary compatibility/options
        tyrian.sav             2,502-byte save / legacy scores / editor progress
    tyrian2000/
        tyrian.cfg             variant legacy version/settings
        tyrian.sav             4,722-byte save / suffix scores / editor progress
```

Keep root resolution exactly as `src/file.c:203–272`: portable executable directory if root `opentyrian.cfg` exists, otherwise existing platform/XDG paths and Deck defaults. New child directories are relative to that selected root. Shared `opentyrian.cfg` remains at root, so portable detection does not fail after migration. Presentation and all controls are authoritative there; per-variant DOS compatibility fields can be mirrored at save time if needed, never override an existing shared preference on every variant switch. Do not copy the fork’s whole `opentyrian2000` user directory convention.

For 2.1 first run: validate the legacy root save/config, create `tyrian21`, copy each missing target through a temporary file and atomic rename, preserve the original bytes and originals, never overwrite an existing destination. Write/log a migration-completed marker only after successful copies; failure keeps originals and reports the fallback policy without generating blank replacement saves. Legacy root `opentyrian.cfg` is already shared; migrate missing presentation/control values from `tyrian.cfg` once only, preserving explicit shared values. The save copy must happen before default score generation; never interpret a 4,722-byte root save as 2.1 just because its prefix decrypts. Do not automatically import a user’s OpenTyrian2000 directory unless explicitly chosen later.

Required Phase-2 checks: all 164 current baselines unchanged; byte-identical load/save roundtrip for synthetic code-owned 2.1 saves; migration with missing/existing targets, partial failure, invalid legacy save and repeated execution; portable/XDG/Deck path selection; regress/selftest never creates migration directories; shared options survive switching namespaces. Phase 3 adds 2000 suffix roundtrips, all 20 score boards, malformed lengths/truncated suffix, preserved unknown fields and cross-variant refusal. Each new save codec must validate the complete expected length and serialize explicit endian fields, not C-struct layout.
## Proposed 2000 regression suite

### Routing, manifest and baselines

Add `make regress-2000 TYRIAN2000_DATA=<directory>` as a distinct entry point in Phase 3/4, eventually sharing harness helpers with `tools/regress.sh`. Local resolution is explicit `TYRIAN2000_DATA`, then the verified installed provider path; if neither exists, report missing data. The 2000 runner must **never** call `get_data.sh` or silently use `./data`. Every game invocation passes `--variant=2000 --data=<verified-root>`, and every negative variant/path case uses a temporary isolated root.

Use `test/regress-2000/data-manifest.txt` with the existing **`size POSIX-cksum-CRC filename`** format (POSIX `cksum`, not ZIP CRC32 or zlib CRC32). Place baseline hash text in `test/regress-2000/`, transient outputs under `test/regress-2000/actual/` (ignored), and code-owned scripts/fixture declarations in a separate test source directory. Preserve `test/regress/` and `make regress` unchanged. Manifest regeneration must require both variant identity and a verified canonical zip; ordinary `--update` must not rewrite a manifest.

The following proposed canonical manifest is measured from the pinned archive and covers **all archive runtime-format files**, including every episode/script/cube, enemy/main/background shape bank, credits/animation, five demos and Christmas assets. It intentionally goes beyond the 2.1 demo-only consumed set because 2000 must not depend on unvalidated files reached later in episode 5. Optional custom ship templates and alternate runtime banks are listed for canonical-suite completeness; a product installation profile can mark optional resources separately, while the canonical regression fixture requires the entire set. DOS executables, PIFs, icons, readme, editor PCX/box/overlay files and driver support are not engine runtime requirements.

```text
# Tyrian 2000 canonical runtime set; metadata only.
# size POSIX-cksum-CRC filename
35780 1209265539 cubetxt1.dat
8109 2712896192 cubetxt2.dat
12277 2914551291 cubetxt3.dat
61763 2766464770 cubetxt4.dat
7024 2416136283 cubetxt5.dat
2745 625760848 demo.1
2193 3214433957 demo.2
1974 623644273 demo.3
699 1914765014 demo.4
1122 2152709229 demo.5
119552 3388891273 estpa.shp
115338 1085736327 estsc.shp
9600 2858812964 levels1.dat
5534 1788176354 levels2.dat
6052 1789566389 levels3.dat
11116 3983064838 levels4.dat
4257 3415369915 levels5.dat
153482 842346037 music.mus
17750 2070673289 newsh#.shp
18016 86096781 newsh$.shp
40244 1307984159 newsh%.shp
35108 2957881770 newsh'.shp
32060 1579300666 newsh(.shp
14096 3328586459 newsh0.shp
29941 2874864033 newsh1.shp
35236 1993929893 newsh2.shp
37130 2811399401 newsh3.shp
27109 1902489147 newsh4.shp
35810 2529306122 newsh5.shp
17810 3585975344 newsh6.shp
44079 2593278867 newsh7.shp
35152 975069015 newsh8.shp
38831 3600755471 newsh9.shp
16498 2621933012 newsh@.shp
34888 711303051 newsh^.shp
25622 2511252582 newsha.shp
39025 2458773564 newshb.shp
40714 117528338 newshc.shp
42012 1017427299 newshd.shp
33563 3448371693 newshe.shp
33543 3376643067 newshf.shp
36643 3829556607 newshg.shp
46639 4102196915 newshh.shp
30860 2722307320 newshi.shp
43999 3409272313 newshj.shp
23085 301199774 newshk.shp
27587 4289035018 newshl.shp
42126 3407189224 newshm.shp
41201 1292696811 newshn.shp
24478 3002203261 newsho.shp
46204 1454869397 newshp.shp
28249 3612603756 newshr.shp
46566 2487630938 newshs.shp
31830 1799186985 newsht.shp
35657 1233745291 newshu.shp
24103 35713433 newshv.shp
14929 3947938427 newsh~.shp
18432 350947577 palette.dat
118720 2717272862 shapes).dat
172480 1648817559 shapesw.dat
217504 767763841 shapesx.dat
206752 1717005239 shapesy.dat
318976 959231650 shapesz.dat
3315848 3988034873 tyrend.anm
1078 4139414299 tyrian.cdt
295069 1136020222 tyrian.hdt
367161 501130314 tyrian.pic
505983 3647119929 tyrian.shp
271689 3556103808 tyrian.snd
538856 743471398 tyrian1.lvl
381187 2271820750 tyrian2.lvl
393938 478873825 tyrian3.lvl
937303 106289611 tyrian4.lvl
521137 2948367722 tyrian5.lvl
516757 447103242 tyrianc.shp
27674 491299863 user1.shp
27674 84075211 user2.shp
132767 1661905447 voices.snd
188275 2600234191 voicesc.snd
```

Phase 5 should add a separate whole-archive SHA-256 manifest for all 99 files, used to detect any original file in Git/release staging regardless of extension or renaming; that denylist includes executables and icons too. The runtime CRC manifest is a fast regression guard, not a cryptographic authenticity check. ZIP SHA-256 remains authoritative for downloads; extracted per-file SHA-256 supports manual installation validation. Future package/Git checks must exempt explicit metadata and shared code, not original assets whose bytes happen to equal 2.1: this archive includes many identical 2.1 files, which are already legally packaged there. A pure “reject every matching 2000 hash everywhere” rule would falsely reject existing 2.1 data. Track approved 2.1 package provenance and allowed paths, and prohibit additions sourced from 2000 rather than blanket banning shared hashes.

### Case matrix and concrete scripts

The names below define the initial suite contract; new fixture options are **proposed harness extensions**, not existing working CLI flags. Every case fixes seed, mode, loadout, input trace, detail, frame cap and expected coverage. State and framebuffer streams are distinct. A missing coverage marker is failure even if hashes happen to match. The five shipped demos are identical input recordings to 2.1, so do not duplicate their bytes in the test tree.

| Cases | Inputs / exact starting points | Outputs / intended coverage |
|---|---|---|
| `demo1..5-d1..6` (30) | Installed `demo.1–5`, current `--regress-demo`, details 1–6 | Classic frame hashes; confirms shared decoder and E1 differences |
| `state-demo1..5-d4` (5) | Same five recordings, fixed seed, `--regress-state-out` | Logic/RNG hashes; variant ID and state version in header only, preserve 2.1’s existing byte stream |
| `modern-demo1..5-d4` (5), `modern-demo1-d6` | Same demos, Modern/Pentium and retained SuperWild exception | Modern canvas hashes, HUD and detail pin |
| `modern-wide-demo1/3-d4` at 16:9,21:9,32:9 (6) | Existing aspect/pixel-aspect options | Widescreen panels, correct labels and camera crop |
| `e5-level1..8-d1/d4` (16) | `--regress-level=5:N`, physical N=1..8, seed 32402394, neutral input/invulnerability | Classic frames/state, cap initially 6,000 ticks per case; coverage IDs recorded; adjust cap by explicit coverage evidence |
| `modern-e5-level1..8-d4` (8) | Same physical levels at Modern 16:9; dedicated natural gameplay loadout after schema load | Added enemies/backgrounds/music, shapes, emission and ambient effects |
| `e4-replace-L5/L11`, `e4-random-L15/L18` (4) | `--regress-level=4:5/11/15/18`, detail4, fixed seed, initial cap6,000 | Real-data event68 replacement and event99 random explosions, with event-executed counters |
| `battle1..3-normal` (3) | Proposed `--regress-battle=1/2/3` using mode-selection path, seed and difficulty fixed, cap12,000 | E1 first battle and E5 other battles; ]T/84/85/83 routing, timer expiration jump, no warning SFX |
| `battle1..3-complete` (3) | Authored logical input traces plus a code-owned deterministic success fixture if ordinary input cannot reach completion | Time/lives bonuses, ]q score route, no backup-save intent, cash-only board |
| `battle-death`, `battle-load-reset` (2) | Authored death/load sequence in fixture mode; temporary user-file sink with write-intent log | Timer persists through battle death; ordinary load clears battle mode |
| `event58`, `event59`, `event68`, `event83`, `event84`, `event85`, `event99` (7) | Proposed `--regress-fixture=<name>` constructs minimal in-memory records in test code; no copied level records | Assert wildcard differences (99 for58,0 for59/68), 25-slot group, failed replacement allocation, map-stop alias, battle gates, 2.1 event68 contrast |
| `enemy-bank2-launch`, `random-spawn-x` (2) | Code-owned enemy/event fixture with explicit seed, boundary IDs and −200 sentinel | Full launch ID, no division special in bank2, one RNG draw and event mutation |
| `twiddle-storm/dragon/gencore/pete/rum` (5) | Authored direction/fire action traces, matching selected ship table | Combo selection and result; zero-combo ships must not read outside bounds |
| `sidekick-charged-main/manual-left/manual-right/both`, `sidekick-uncharged-main` (5) | Proposed loadout+input fixture selecting a validated charged/uncharged option; fixed held-action windows | No charged auto-fire on main, manual release/repeat/charge timing, unchanged ammo-limited branch |
| `rear-upgrade-none`, `rear-upgrade-without-none`, `front-none`, `upgrade-done` (4) | Authored menu interaction over code-owned availability lists | Historical eligibility and guarded Done/invalid selection; UBSan/ASan execution |
| `score-hazudra`, `score-battle-cancel`, `score-sort` (3) | Synthetic completion/name actions and seeded suffix boards | Final-episode eligibility, ignored battle cancellation, all 20 boards/sort/name bounds |
| `obscured-cash-classic/modern` (2) | Fixture activates smoothie6 or a verified naturally reached spotlight scenario | Readable cash in Classic and unchanged Modern panel treatment |
| `flying-punch-center`, `explosion54` (2) | Variant loadout + authored fire trace; assert trail198 only on center tile | Shot/trail/explosion state, emission and correct sprite bank |
| `xmas-shapes`, `xmas-audio`, `xmas-explicit-no-prompt` (3) | Proposed deterministic `--regress-assets=christmas`; fixed date-independent profile | 13-bank seasonal ships, identical voice bytes with shifted slots, forced choice semantics |
| `audio` | Existing offline mixer at 44,100 Hz, adapted runtime counts | 40 converted samples, 41 songs, voice semantic IDs, both new SFX; own baseline even if music subsection equals 2.1 |
| `save21-migration`, `save2000-roundtrip`, `save2000-invalid`, `namespace-isolation`, `shared-config` (5 families) | Code-owned binary fixtures/temporary roots, no real user files | 2,502/4,722 sizes, prefix integrity, suffix/name validation, unknown field preservation, non-destructive migration |
| `data-valid`, `data-wrong-variant`, `data-missing`, `data-corrupt`, `installer-interrupted` (5 families) | External canonical root + temporary mutated copies outside repo | Correct provider diagnostics, no fallback or partial installation; installer case arrives in Phase 5 |

For **UI screens**, adapt `src/regress_screen.c:161` to descriptor-selected fixtures and add `timed-select`, `save-confirm`, `mouse-config` and battle-score pages. Cover title/modes/episodes/difficulty/high scores/setup/keyboard/gamepad/item upgrades/save-load/nav map/ship specs/cubes/pause/quit/credits in Classic and Modern, plus the existing 16:9/21:9/32:9 menu geometries. Set code-owned deterministic names/scores rather than copying HDT default names into fixtures. Run new ships’ big illustrations 45/46, enlarged main menu strings, both rear-mode preview states and all five episode labels; check pixel edges and inverse mouse selection.

For **episode command screens**, author `e5-entry`, `e5-warning`, `e5-menu`, `e5-cubes`, `e5-end` logical scripts that invoke the existing episode interpreter (`--regress-script=5:<logical-section>`). Determine logical sections through the installed provider at runtime or a structural index generated outside Git; do not assume physical N and `mainLevel` are the same. Assert resolved physical level IDs before hashing. Captures/screenshots are local QA only; commit text hashes and structural expectations, not extracted screen images or text. Natural script and full menu progression must remain additional coverage beyond direct physical-level cases.

The Timed Battle fixture must set selection through the new common menu/mode entry API and let `]T` execute; setting `timedBattleMode=true` after an ordinary synthetic level start does not test battle routing. End-level/score fixtures need deterministic logical inputs; current `regress-script` only starts at a logical section and is not a general editable input-script interpreter. Extend the current harness input abstraction with code-owned traces, do not invent binary recordings extracted from the copyrighted files.

Run representative replay/alpha=1 identity checks on E5/L7 replacements, Flying Punch and battle mode; full sweeps on all physical E5 levels and demos should be available as `regress-2000-replay/interp/smooth/parallax`. Modern tick-zero/current-state hashes must equal corresponding Classic state hashes despite more display frames. State extension for 2000 includes mode, selection, timer jump, variant progression/board state and relevant command flow; the 2.1 serializer stays byte-for-byte unchanged. Keep RNG-order lint, real-input filtering, compiler/platform matrix and exact output-line/frame-cap checks.

Baseline creation uses the pinned fork/DOS comparison in Classic where possible, then the reviewed integrated common engine. Fork SDL2 rendering alone is not an oracle for Modern canvas output. Record baseline provenance (fork commit, archive SHA, engine commit, options, coverage counters) in metadata. Original-DOS comparisons remain manual evidence until a deterministic capture method is available; the approximate event/trail implementations must be labeled accordingly rather than certified by self-generated hashes.

### CI download, verification and cache

Use the same canonical URL, size and SHA as [data-formats.md](data-formats.md). A dedicated trusted 2000 job on Linux/macOS/Windows stages files under `$RUNNER_TEMP/tyrian2000`, **outside the checkout and release staging**. The job restores a hash-keyed archive cache or downloads into a temporary archive, verifies 5,051,363 bytes and the exact SHA-256 on every cache hit/miss, safely extracts only regular relative entries under `tyrian2000/`, then verifies the runtime manifest. Reject traversal, symlinks, duplicate normalized paths, unexpected sizes/overlarge extraction and CRC failures. Atomically rename a complete verified staging directory to its final temporary root; partial downloads/extractions are never cached as valid.

Only run `make regress-2000 TYRIAN2000_DATA=<verified-root>` after verification. Keep the 2.1 regression job required too. Never run the source archive’s DOS executables. Cache key: `tyrian2000-348bc76e73514e452279b8730cf217daf0f70a282f07b6b94af653d87e921667-<manifest-revision>-<cache-format-revision>`, with no broad restore prefix. Store the verified ZIP, not extracted assets in a cache beside build/release files. An absent network/cache is an explicit acquisition failure; it must not pass as a skipped suite or regenerate baselines.

**Correction to the plan’s privacy assumption:** an ordinary Actions cache is not inherently private to maintainers. GitHub documents that fork PR runs can restore base-branch caches and recommends not treating their contents as confidential. Current cache-mode controls allow reads/writes to be denied, but a same-job `if` around the cache action does not by itself make cache bytes private. See [GitHub cache access restrictions](https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching#restrictions-for-accessing-a-cache) and [cache-mode](https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching#controlling-cache-access-with-cache-mode).

To meet the approved **private cache** intent on a public repo, cache an **authenticated encrypted ZIP** with a dedicated maintainer-only repository/environment secret, using reviewed CI encryption/decryption helpers. Only trusted push/workflow-dispatch jobs receive the key; never PR/fork contexts or `pull_request_target` running untrusted code. Restore encrypted bytes, decrypt outside the checkout, then verify the canonical plaintext ZIP SHA and size as above; rotate cache-format/key IDs when the key changes. Plaintext lives only in the temporary test job and is erased afterward. A private repository with appropriate access limits is another option. A plaintext public-repository cache cannot be described as private simply because the project provides no download link; this needs coordinator/user policy review before CI implementation. No cache/downloader was changed in this research phase.

Failure uploads must contain only logs, hashes, coverage summaries and code-owned fixture diagnostics. Exclude verified zip/extracted files, screenshots, audio PCM, save dumps containing original default names, and core dumps that could contain asset bytes. Release jobs stage only the existing approved 2.1 data; installer constants/manifest/hash text may ship, but no 2000 payload. Inspect actual archive contents before publishing, not merely `.gitignore`. Use provenance-aware denylist checking as above for shared hashes.

**Implemented (Phase 5 CI, user decision: no cache).** `tools/fetch_t2000_data.sh DEST` downloads the archive in every run and applies the checks above; the encrypted-cache proposal is not used and no Actions cache is involved. The Linux, macOS and Windows workflows fetch into `$RUNNER_TEMP/tyrian2000` after the 2.1 suite, in the same job (so they reuse its binary), run `make regress-2000`, and on failure upload only `test/regress-2000/actual/*.txt` and `*.log`. A failed fetch fails the job. Release events skip both steps and package only `./data`.

## Review gates and remaining uncertainties

1. Phase 2: 2.1-only abstraction, namespace migration and early parse; GCC-16 warnings-as-errors and all 164 unchanged baselines. No 2000 code becomes “supported” merely by removing the startup guard.
2. Phase 3: expanded schemas plus narrow hooks; verify E1 startup, suffix codec and representative E5 events; keep 2.1 green after every structural change.
3. Phase 4: normal UI progression across all five episodes, Timed Battle and seasonal assets; compare replacement/launch/trail approximations against DOS 2000. Resolve the 77-byte item-block remainder without guessing an ID.
4. Phase 5/CI: review canonical manifest/product compatibility profiles and the private-cache/privacy correction; no original assets enter Git, release artifacts or failure uploads.
5. Phase 7: review each backdrop, HUD label, new shape emission and interpolation replacement identity with existing Modern algorithms; baseline hashes prove determinism, not historical fidelity by themselves.
