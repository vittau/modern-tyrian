# OpenTyrian2000 fork audit

## Provenance and scope

Our reference tree is `a2d82d82bb0ac83a2253b18fc5d9ee231a7f61b9` (branch `t2000p1`). Fork references below use [KScl/opentyrian2000 at `aad5aca01af139c0b089237c38ef765f7a84355d`](https://github.com/KScl/opentyrian2000/tree/aad5aca01af139c0b089237c38ef765f7a84355d); upstream was fetched at `5a9d8daa2811347bec3a0c42f8da87682378f3cb`. The merge base is **`967c12ed1db3644afe068ce83a8bec3d82744f0d`**.

The audited delta is `git diff --find-renames --unified=3 967c12ed1db3644afe068ce83a8bec3d82744f0d aad5aca01af139c0b089237c38ef765f7a84355d`, equivalently `git diff upstream/master...aad5aca`. It contains 50 file changes, 1,292 inserted and 476 deleted text lines. Upstream-only work after the base is excluded. Merge conflict resolutions that survive in this delta are included, even when their origin is a merge. This avoids counting the fork’s many upstream merges as new functionality.

## Classification ledger

There is one row per default three-context-line textual hunk, plus one row per binary addition. Ranges are the full new-side hunk ranges (including context); a removal points at its surrounding fork location. Mixed hunks have multiple classes so unrelated common fixes are not silently variant-gated. Globals/declarations point to their consuming function in our tree; functions absent here point to the existing integration owner, rather than pretending the fork helper exists.

| Code | Class | Default destination |
|---|---|---|
| C | Tyrian 2000 content/rules | Variant schema/table or UI policy |
| F | General fix valid for both variants | Common fix, after checking existing protection |
| R | Renderer | Variant asset/layout metadata through shared Modern renderer |
| I | Input | Common SDL3 action layer; variant menu metadata |
| A | Audio | Variant sound-ID/table metadata through common Nuked-OPL3/resampler |
| G | Gameplay | Variant rule hook/policy; preserve 2.1 |
| D | Fork architecture side effect | Drop fork structure/branding |

### .gitignore

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H001 | `fork:.gitignore:12–18` | D | Fork executable/install/docs branding or data-source wording | `.gitignore:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |

### Makefile

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H002 | `fork:Makefile:5–11` | D | Fork executable/install/docs branding or data-source wording | `Makefile:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |
| H003 | `fork:Makefile:31–37` | D | Fork executable/install/docs branding or data-source wording | `Makefile:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |
| H004 | `fork:Makefile:44–50` | D | Fork executable/install/docs branding or data-source wording | `Makefile:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |
| H005 | `fork:Makefile:124–148` | D | Fork executable/install/docs branding or data-source wording | `Makefile:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |

### README

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H006 | `fork:README:1–9` | D | Fork executable/install/docs branding or data-source wording | `README.md:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |
| H007 | `fork:README:14–22` | D | Fork executable/install/docs branding or data-source wording | `README.md:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |
| H008 | `fork:README:29–55` | D | Fork executable/install/docs branding or data-source wording | `README.md:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |

### linux/icons/tyrian2000-128.png

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H009 | `fork:linux/icons/tyrian2000-128.png` (binary) | D | Fork-specific icon asset | `visualc/resources.rc:1` / application packaging | low: one shared application identity | Drop; do not import data-derived icons |
### linux/icons/tyrian2000-22.png

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H010 | `fork:linux/icons/tyrian2000-22.png` (binary) | D | Fork-specific icon asset | `visualc/resources.rc:1` / application packaging | low: one shared application identity | Drop; do not import data-derived icons |
### linux/icons/tyrian2000-24.png

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H011 | `fork:linux/icons/tyrian2000-24.png` (binary) | D | Fork-specific icon asset | `visualc/resources.rc:1` / application packaging | low: one shared application identity | Drop; do not import data-derived icons |
### linux/icons/tyrian2000-32.png

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H012 | `fork:linux/icons/tyrian2000-32.png` (binary) | D | Fork-specific icon asset | `visualc/resources.rc:1` / application packaging | low: one shared application identity | Drop; do not import data-derived icons |
### linux/icons/tyrian2000-48.png

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H013 | `fork:linux/icons/tyrian2000-48.png` (binary) | D | Fork-specific icon asset | `visualc/resources.rc:1` / application packaging | low: one shared application identity | Drop; do not import data-derived icons |
### linux/man/opentyrian2000.6

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H014 | `fork:linux/man/opentyrian2000.6:1–15` | D | Fork executable/install/docs branding or data-source wording | `linux/man/opentyrian.6:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |
| H015 | `fork:linux/man/opentyrian2000.6:39–49` | D | Fork executable/install/docs branding or data-source wording | `linux/man/opentyrian.6:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |
| H016 | `fork:linux/man/opentyrian2000.6:73–79` | D | Fork executable/install/docs branding or data-source wording | `linux/man/opentyrian.6:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |

### linux/opentyrian2000.desktop

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H017 | `fork:linux/opentyrian2000.desktop:1–10` | D | Fork executable/install/docs branding or data-source wording | `linux/opentyrian.desktop:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |

### src/config.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H018 | `fork:src/config.c:53–63` | F | Initialize valid mouse defaults before a missing-config return | `loadOpenTyrianConfig()` — `src/config.c:253` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | common fix/check existing protection |
| H019 | `fork:src/config.c:82–106` | C/I | Replace hardcoded score/team defaults with data-backed arrays; introduce mouse binding names/values | `loadOpenTyrianConfig()` — `src/config.c:253` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant table/schema; common SDL3 action layer + menu descriptor |
| H020 | `fork:src/config.c:135–143` | G | Declare Timed Battle selection | `loadSaves()` — `src/config.c:888` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant rule hook |
| H021 | `fork:src/config.c:153–161` | I | Store configurable mouse bindings | `loadOpenTyrianConfig()` — `src/config.c:253` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | common SDL3 action layer + menu descriptor |
| H022 | `fork:src/config.c:166–172` | G | Add Timed Battle mode state | `JE_loadGame()` — `src/config.c:549` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant rule hook |
| H023 | `fork:src/config.c:190–196` | C | Document DOS config version 3 | `loadConfiguration()` — `src/config.c:749` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant table/schema |
| H024 | `fork:src/config.c:202–209` | C | Allocate 20 three-entry high-score boards | `loadSaves()` — `src/config.c:888` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant table/schema |
| H025 | `fork:src/config.c:214–227` | F/I | Reset mouse defaults before parsing, with whitespace cleanup | `loadOpenTyrianConfig()` — `src/config.c:253` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | common fix/check existing protection; common SDL3 action layer + menu descriptor |
| H026 | `fork:src/config.c:260–285` | I | Read optional mouse section with named binding values | `loadOpenTyrianConfig()` — `src/config.c:253` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | common SDL3 action layer + menu descriptor |
| H027 | `fork:src/config.c:318–336` | I | Write mouse bindings independently of DOS config, with whitespace cleanup | `saveOpenTyrianConfig()` — `src/config.c:400` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | common SDL3 action layer + menu descriptor |
| H028 | `fork:src/config.c:437–443` | G | Clear Timed Battle state when loading a normal save | `JE_loadGame()` — `src/config.c:549` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant rule hook |
| H029 | `fork:src/config.c:452–458` | C | Accept the two new super-arcade ship IDs | `JE_loadGame()` — `src/config.c:549` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant table/schema |
| H030 | `fork:src/config.c:747–760` | D | Rename user directory to opentyrian2000 | `userDirGet()` — `src/file.c:273` | low: one binary/provider namespace supersedes fork structure | drop |
| H031 | `fork:src/config.c:772–778` | F | Track whether configuration initialization completed | `loadConfiguration()` — `src/config.c:749` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | common fix/check existing protection |
| H032 | `fork:src/config.c:822–828` | C | Default missing DOS config to version 3 | `loadConfiguration()` — `src/config.c:749` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant table/schema |
| H033 | `fork:src/config.c:905–945` | C | Read unencrypted 2000 score suffix: ten 35-byte and ten 39-byte boards | `loadSaves()` — `src/config.c:888` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant table/schema |
| H034 | `fork:src/config.c:967–1005` | C | Initialize new score boards from data-backed defaults and variant RNG ranges | `loadSaves()` — `src/config.c:888` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant table/schema |
| H035 | `fork:src/config.c:1008–1017` | F | Prevent uninitialized configuration writes after failed data loading | `saveConfiguration()` — `src/config.c:837` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | common fix/check existing protection |
| H036 | `fork:src/config.c:1087–1129` | C | Write 2000 score suffix, including placeholder unknown field | `saveSaves()` — `src/config.c:989` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant table/schema |

### src/config.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H037 | `fork:src/config.h:70–77` | I | Declare three mouse assignments | `loadOpenTyrianConfig()` — `src/config.c:253` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | common SDL3 action layer + menu descriptor |
| H038 | `fork:src/config.h:106–128` | C/I | Declare T2K scores and data-backed default names plus mouse defaults | `loadSaves()` — `src/config.c:888` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant table/schema; common SDL3 action layer + menu descriptor |
| H039 | `fork:src/config.h:130–136` | G | Expose Timed Battle selection | `JE_loadGame()` — `src/config.c:549` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | variant rule hook |
| H040 | `fork:src/config.h:155–167` | I/G | Expose mouse assignments and Timed Battle mode | `JE_loadGame()` — `src/config.c:549` | high: config/save I/O was rewritten; preserve Deck, shared controls, and user-file suppression | common SDL3 action layer + menu descriptor; variant rule hook |

### src/destruct.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H041 | `fork:src/destruct.c:738–744` | C | Resolve Destruct name through variant super-arcade index | `JE_introScreen()` — `src/destruct.c:738` | low: small metadata/capacity change; shared backend remains intact | variant table/schema |

### src/episodes.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H042 | `fork:src/episodes.c:71–103` | C | Read two non-contiguous weapon ID banks without consuming holes | `JE_loadItemDat()` — `src/episodes.c:54` | medium: new schemas must coexist without changing 2.1 reads | variant table/schema |
| H043 | `fork:src/episodes.c:185–225` | C | Read second enemy bank without consuming ID hole | `JE_loadItemDat()` — `src/episodes.c:54` | medium: new schemas must coexist without changing 2.1 reads | variant table/schema |
| H044 | `fork:src/episodes.c:232–240` | F | Use byte printf format for episode filenames | `JE_initEpisode()` — `src/episodes.c:247` | medium: new schemas must coexist without changing 2.1 reads | common fix/check existing protection |

### src/episodes.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H045 | `fork:src/episodes.h:27–33` | C | Expose episode 5 as available | `JE_scanForEpisodes()` — `src/episodes.c:262` | medium: new schemas must coexist without changing 2.1 reads | variant table/schema |

### src/fonthand.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H046 | `fork:src/fonthand.c:54–60` | C | Allow 12 warning lines instead of 10 | `levelWarningText` — `src/fonthand.c:56` | low: small metadata/capacity change; shared backend remains intact | variant table/schema |

### src/fonthand.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H047 | `fork:src/fonthand.h:34–40` | C | Declare expanded warning-line capacity | `levelWarningText` — `src/fonthand.c:56` | low: small metadata/capacity change; shared backend remains intact | variant table/schema |

### src/game_menu.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H048 | `fork:src/game_menu.c:60–66` | I | Add mouse-config menu ID | `JE_itemScreen()` — `src/game_menu.c:272` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | common SDL3 action layer + menu descriptor |
| H049 | `fork:src/game_menu.c:93–99` | R | Add 150-frame weapon-preview text cycle state | `JE_weaponSimUpdate()` — `src/game_menu.c:3336` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant layout/asset metadata + shared renderer |
| H050 | `fork:src/game_menu.c:105–112` | C/I | Extend options/menu escape topology for mouse settings | `JE_itemScreen()` — `src/game_menu.c:272` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant table/schema; common SDL3 action layer + menu descriptor |
| H051 | `fork:src/game_menu.c:306–325` | I | Draw mouse menu choices and binding values | `JE_itemScreen()` — `src/game_menu.c:272` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | common SDL3 action layer + menu descriptor |
| H052 | `fork:src/game_menu.c:499–531` | G/F | Use actual rear None item rather than last-slot rule; guard selection before indexing | `JE_itemScreen()` — `src/game_menu.c:272` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant rule hook; common fix/check existing protection |
| H053 | `fork:src/game_menu.c:750–756` | F | Bounds-check portrait palette lookup for new faces | `JE_itemScreen()` — `src/game_menu.c:272` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | common fix/check existing protection |
| H054 | `fork:src/game_menu.c:1147–1153` | I | Add 24-pixel mouse-menu hit-test spacing | `JE_itemScreen()` — `src/game_menu.c:272` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | common SDL3 action layer + menu descriptor |
| H055 | `fork:src/game_menu.c:1672–1679` | R | Add big-ship illustration offsets for new ship graphics | `draw_ship_illustration()` — `src/game_menu.c:1810` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant layout/asset metadata + shared renderer |
| H056 | `fork:src/game_menu.c:1697–1721` | R | Expand front weapon illustration/position tables | `draw_ship_illustration()` — `src/game_menu.c:1810` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant layout/asset metadata + shared renderer |
| H057 | `fork:src/game_menu.c:1729–1741` | R | Expand rear weapon illustration table | `draw_ship_illustration()` — `src/game_menu.c:1810` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant layout/asset metadata + shared renderer |
| H058 | `fork:src/game_menu.c:1898–1907` | R | Draw ships with graphic IDs above 500 from added shape bank | `JE_drawItem()` — `src/game_menu.c:2032` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant layout/asset metadata + shared renderer |
| H059 | `fork:src/game_menu.c:1969–1979` | I | Space mouse-setting menu rows | `JE_drawMenuChoices()` — `src/game_menu.c:2096` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | common SDL3 action layer + menu descriptor |
| H060 | `fork:src/game_menu.c:2352–2358` | C | Include Super Tyrian/options help in common help predicate | `JE_drawMainMenuHelpText()` — `src/game_menu.c:2533` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant table/schema |
| H061 | `fork:src/game_menu.c:2378–2489` | C/I | Add save-overwrite confirmation using new strings and message-box sprite | `JE_quitRequest()` — `src/game_menu.c:2572` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant table/schema; common SDL3 action layer + menu descriptor |
| H062 | `fork:src/game_menu.c:2732–2738` | R | Reset preview text cycle upon weapon selection | `JE_menuFunction()` — `src/game_menu.c:2768` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant layout/asset metadata + shared renderer |
| H063 | `fork:src/game_menu.c:2762–2770` | I | Route added options row to mouse configuration | `JE_menuFunction()` — `src/game_menu.c:2768` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | common SDL3 action layer + menu descriptor |
| H064 | `fork:src/game_menu.c:3103–3126` | I | Cycle/reset five mouse actions and return from menu | `JE_menuFunction()` — `src/game_menu.c:2768` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | common SDL3 action layer + menu descriptor |
| H065 | `fork:src/game_menu.c:3180–3193` | R | Add ship-specs offsets for big graphics 45/46 | `JE_drawShipSpecs()` — `src/game_menu.c:3228` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant layout/asset metadata + shared renderer |
| H066 | `fork:src/game_menu.c:3239–3279` | G/R | Display power only when eligible; alternate rear-mode hint and costs | `JE_weaponSimUpdate()` — `src/game_menu.c:3336` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant rule hook; variant layout/asset metadata + shared renderer |
| H067 | `fork:src/game_menu.c:3282–3290` | G/R | Remove duplicate power flags/text from simulation rendering | `JE_weaponSimUpdate()` — `src/game_menu.c:3336` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant rule hook; variant layout/asset metadata + shared renderer |
| H068 | `fork:src/game_menu.c:3365–3382` | R | Draw two rear-mode indicator sprites | `JE_weaponViewFrame()` — `src/game_menu.c:3398` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant layout/asset metadata + shared renderer |

### src/game_menu.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H069 | `fork:src/game_menu.h:45–51` | C | Expose save confirmation callback | `JE_quitRequest()` — `src/game_menu.c:2572` | high: widened panels, modal centering, keyboard/gamepad remap and hit-test topology differ | variant table/schema |

### src/helptext.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H070 | `fork:src/helptext.c:33–39` | C/I | Add mouse-settings help index in options | `JE_loadHelpText()` — `src/helptext.c:165` | high: wrong section counts shift all menu/HUD labels and default-score initialization | variant table/schema; common SDL3 action layer + menu descriptor |
| H071 | `fork:src/helptext.c:41–51` | C/I | Add limited-options and mouse-menu help mapping | `JE_loadHelpText()` — `src/helptext.c:165` | high: wrong section counts shift all menu/HUD labels and default-score initialization | variant table/schema; common SDL3 action layer + menu descriptor |
| H072 | `fork:src/helptext.c:75–83` | C | Allocate licensing/ordering/Super Tyrian strings | `JE_loadHelpText()` — `src/helptext.c:165` | high: wrong section counts shift all menu/HUD labels and default-score initialization | variant table/schema |
| H073 | `fork:src/helptext.c:187–194` | C | Change section counts and define skipped setup section sizes | `JE_loadHelpText()` — `src/helptext.c:165` | high: wrong section counts shift all menu/HUD labels and default-score initialization | variant table/schema |
| H074 | `fork:src/helptext.c:229–239` | D | Move Setup override and shift title rows for fork menus | `titleScreen()` — `src/tyrian2.c:3340` | low: one binary/provider namespace supersedes fork structure | drop |
| H075 | `fork:src/helptext.c:393–453` | C | Read remaining T2K sections including timed names, mouse menu and score defaults | `JE_loadHelpText()` — `src/helptext.c:165` | high: wrong section counts shift all menu/HUD labels and default-score initialization | variant table/schema |

### src/helptext.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H076 | `fork:src/helptext.h:25–31` | C/I | Reserve one extra menu | `JE_loadHelpText()` — `src/helptext.c:165` | high: wrong section counts shift all menu/HUD labels and default-score initialization | variant table/schema; common SDL3 action layer + menu descriptor |
| H077 | `fork:src/helptext.h:34–49` | C | Expand string section counts, field sizes, special ships and ship descriptions | `JE_loadHelpText()` — `src/helptext.c:165` | high: wrong section counts shift all menu/HUD labels and default-score initialization | variant table/schema |
| H078 | `fork:src/helptext.h:67–75` | C | Expose extended string arrays | `JE_loadHelpText()` — `src/helptext.c:165` | high: wrong section counts shift all menu/HUD labels and default-score initialization | variant table/schema |

### src/keyboard.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H079 | `fork:src/keyboard.c:18–24` | I | Include binding configuration | `handleSdlEvents()` — `src/keyboard.c:176` | high: SDL3 events and deterministic input filtering replaced fork SDL2 paths | common SDL3 action layer + menu descriptor |
| H080 | `fork:src/keyboard.c:37–43` | I | Expand mouse action state from three to four actions | `handleSdlEvents()` — `src/keyboard.c:176` | high: SDL3 events and deterministic input filtering replaced fork SDL2 paths | common SDL3 action layer + menu descriptor |
| H081 | `fork:src/keyboard.c:232–266` | I | Map physical mouse buttons to configurable actions | `handleSdlEvents()` — `src/keyboard.c:176` | high: SDL3 events and deterministic input filtering replaced fork SDL2 paths | common SDL3 action layer + menu descriptor |

### src/keyboard.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H082 | `fork:src/keyboard.h:33–39` | I | Expose fourth mouse action | `handleSdlEvents()` — `src/keyboard.c:176` | high: SDL3 events and deterministic input filtering replaced fork SDL2 paths | common SDL3 action layer + menu descriptor |

### src/lvlmast.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H083 | `fork:src/lvlmast.c:20–27` | C | Add apostrophe and percent enemy shape-bank filenames | `JE_loadCompShapes()` — `src/sprite.c:588` | low: small metadata/capacity change; shared backend remains intact | variant table/schema |

### src/lvlmast.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H084 | `fork:src/lvlmast.h:23–45` | C | Expand weapon/enemy banks, ports, options, ships, shields and specials | `JE_loadItemDat()` — `src/episodes.c:54` | low: small metadata/capacity change; shared backend remains intact | variant table/schema |

### src/mainint.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H085 | `fork:src/mainint.c:25–31` | C | Include save-confirmation declaration | `JE_operation()` — `src/mainint.c:2799` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema |
| H086 | `fork:src/mainint.c:976–982` | G | Allow end-of-initial-episode high scores for Hazudra Fodder | `JE_nextEpisode()` — `src/mainint.c:961` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant rule hook |
| H087 | `fork:src/mainint.c:1064–1070` | G | Clear Timed Battle during new player initialization | `JE_initPlayerData()` — `src/mainint.c:1021` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant rule hook |
| H088 | `fork:src/mainint.c:1091–1117` | C | Sort all 20 T2K boards rather than six legacy boards | `JE_sortHighScores()` — `src/mainint.c:1070` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema |
| H089 | `fork:src/mainint.c:1123–1130` | C | Display five episode and three Timed Battle pages | `JE_highScoreScreen()` — `src/mainint.c:1082` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema |
| H090 | `fork:src/mainint.c:1134–1142` | C | Allocate T2K high-score heading/board selection state | `JE_highScoreScreen()` — `src/mainint.c:1082` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema |
| H091 | `fork:src/mainint.c:1151–1217` | C | Render separate one/two-player boards and Timed Battle pages | `JE_highScoreScreen()` — `src/mainint.c:1082` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema |
| H092 | `fork:src/mainint.c:2031–2052` | G | Select score board and use cash-only Timed Battle score | `JE_highScoreCheck()` — `src/mainint.c:1970` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant rule hook |
| H093 | `fork:src/mainint.c:2055–2072` | C | Insert and shift T2K high-score entries | `JE_highScoreCheck()` — `src/mainint.c:1970` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema |
| H094 | `fork:src/mainint.c:2074–2083` | A/G | Suppress high-score music switch for Timed Battle | `JE_highScoreCheck()` — `src/mainint.c:1970` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant sound map + common audio; variant rule hook |
| H095 | `fork:src/mainint.c:2090–2099` | R | Use pic 13 behind Timed Battle name entry | `JE_highScoreCheck()` — `src/mainint.c:1970` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant layout/asset metadata + shared renderer |
| H096 | `fork:src/mainint.c:2208–2238` | G/C | Ignore cancel for Timed Battle scores and draw selected board | `JE_highScoreCheck()` — `src/mainint.c:1970` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant rule hook; variant table/schema |
| H097 | `fork:src/mainint.c:2240–2254` | C | Glow T2K board entries by board-local rank | `JE_highScoreCheck()` — `src/mainint.c:1970` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema |
| H098 | `fork:src/mainint.c:2518–2526` | C | Remove obsolete legacy sorter; change credits line count to 126 | `JE_playCredits()` — `src/mainint.c:2413` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema |
| H099 | `fork:src/mainint.c:2760–2773` | G | Add truncated tenths time bonus to cash | `JE_endLevelAni()` — `src/mainint.c:2607` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant rule hook |
| H100 | `fork:src/mainint.c:2775–2821` | G/R | Add remaining-lives bonus and icon animation | `JE_endLevelAni()` — `src/mainint.c:2607` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant rule hook; variant layout/asset metadata + shared renderer |
| H101 | `fork:src/mainint.c:3014–3021` | C/I | Confirm mouse-triggered save before writing | `JE_operation()` — `src/mainint.c:2799` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema; common SDL3 action layer + menu descriptor |
| H102 | `fork:src/mainint.c:3056–3063` | C/I | Confirm keyboard-triggered save before writing | `JE_operation()` — `src/mainint.c:2799` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema; common SDL3 action layer + menu descriptor |
| H103 | `fork:src/mainint.c:3077–3087` | R | Brighten cash under obscured-vision smoothie | `JE_inGameDisplays()` — `src/mainint.c:2934` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant layout/asset metadata + shared renderer |
| H104 | `fork:src/mainint.c:3715–3722` | I | Use independent right sidekick and rear-mode mouse actions | `JE_playerMovement()` — `src/mainint.c:3381` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | common SDL3 action layer + menu descriptor |
| H105 | `fork:src/mainint.c:4292–4298` | C | Allow new super-arcade ship special selection | `JE_playerMovement()` — `src/mainint.c:3381` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant table/schema |
| H106 | `fork:src/mainint.c:4599–4606` | G | Charging infinite-ammo sidekicks ignore main-fire auto trigger | `JE_playerMovement()` — `src/mainint.c:3381` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | variant rule hook |
| H107 | `fork:src/mainint.c:4802–4812` | F | Bound pickup notification formatting (including cosmetic sizeof syntax) | `JE_playerCollide()` — `src/mainint.c:4697` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | common fix/check existing protection |
| H108 | `fork:src/mainint.c:4818–4826` | F | Bound pickup notification formatting (including cosmetic sizeof syntax) | `JE_playerCollide()` — `src/mainint.c:4697` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | common fix/check existing protection |
| H109 | `fork:src/mainint.c:4846–4852` | F | Bound pickup notification formatting (including cosmetic sizeof syntax) | `JE_playerCollide()` — `src/mainint.c:4697` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | common fix/check existing protection |
| H110 | `fork:src/mainint.c:4866–4874` | F | Bound pickup notification formatting (including cosmetic sizeof syntax) | `JE_playerCollide()` — `src/mainint.c:4697` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | common fix/check existing protection |
| H111 | `fork:src/mainint.c:4877–4883` | F | Bound pickup notification formatting (including cosmetic sizeof syntax) | `JE_playerCollide()` — `src/mainint.c:4697` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | common fix/check existing protection |
| H112 | `fork:src/mainint.c:4893–4901` | F | Bound pickup notification formatting (including cosmetic sizeof syntax) | `JE_playerCollide()` — `src/mainint.c:4697` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | common fix/check existing protection |
| H113 | `fork:src/mainint.c:4904–4910` | F | Bound pickup notification formatting (including cosmetic sizeof syntax) | `JE_playerCollide()` — `src/mainint.c:4697` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | common fix/check existing protection |
| H114 | `fork:src/mainint.c:4974–4982` | F | Bound pickup notification formatting (including cosmetic sizeof syntax) | `JE_playerCollide()` — `src/mainint.c:4697` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | common fix/check existing protection |
| H115 | `fork:src/mainint.c:4987–4995` | F | Bound pickup notification formatting (including cosmetic sizeof syntax) | `JE_playerCollide()` — `src/mainint.c:4697` | high: Modern HUD, draw identity, credits canvas and SDL3 input already diverged | common fix/check existing protection |

### src/menus.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H116 | `fork:src/menus.c:36–43` | C | Extend gameplay-name array and add Timed Battle planet names | `gameplaySelect()` — `src/menus.c:38` | high: Modern centered layouts and input mapping require reconstruction | variant table/schema |
| H117 | `fork:src/menus.c:45–51` | G | Insert Timed Battle option before two-player mode | `gameplaySelect()` — `src/menus.c:38` | high: Modern centered layouts and input mapping require reconstruction | variant rule hook |
| H118 | `fork:src/menus.c:213–219` | G | Permit Timed Battle selection action | `gameplaySelect()` — `src/menus.c:38` | high: Modern centered layouts and input mapping require reconstruction | variant rule hook |
| H119 | `fork:src/menus.c:221–227` | G | Set Timed Battle mode when selected | `gameplaySelect()` — `src/menus.c:38` | high: Modern centered layouts and input mapping require reconstruction | variant rule hook |
| H120 | `fork:src/menus.c:674–858` | G/I | Implement three-choice Timed Battle menu; first choice E1, others E5 | `episodeSelect()` — `src/menus.c:229` | high: Modern centered layouts and input mapping require reconstruction | variant rule hook; common SDL3 action layer + menu descriptor |

### src/menus.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H121 | `fork:src/menus.h:21–35` | C/G | Declare expanded gameplay names and Timed Battle entry point | `gameplaySelect()` — `src/menus.c:38` | high: Modern centered layouts and input mapping require reconstruction | variant table/schema; variant rule hook |

### src/mouse.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H122 | `fork:src/mouse.c:29–34` | I | Remove three-button fallback flag in favor of action mapping | `handleSdlEvents()` — `src/keyboard.c:176` | low: small metadata/capacity change; shared backend remains intact | common SDL3 action layer + menu descriptor |

### src/mouse.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H123 | `fork:src/mouse.h:33–38` | I | Remove obsolete three-button flag declaration | `handleSdlEvents()` — `src/keyboard.c:176` | low: small metadata/capacity change; shared backend remains intact | common SDL3 action layer + menu descriptor |

### src/opentyr.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H124 | `fork:src/opentyr.c:56–62` | D | Rename fork application identity | `main()` — `src/opentyr.c:974` | low: one binary/provider namespace supersedes fork structure | drop |
| H125 | `fork:src/opentyr.c:751–758` | D | Add fork copyright banner | `main()` — `src/opentyr.c:974` | low: one binary/provider namespace supersedes fork structure | drop |
| H126 | `fork:src/opentyr.c:764–783` | C/F | Load strings before config so score defaults exist; resolve explicit xmas override | `main()` — `src/opentyr.c:974` | high: variant/provider/save selection must precede current initialization ordering | variant table/schema; common fix/check existing protection |
| H127 | `fork:src/opentyr.c:795–801` | F | Bypass automatic Christmas prompt for explicit mode | `main()` — `src/opentyr.c:974` | high: variant/provider/save selection must precede current initialization ordering | common fix/check existing protection |
| H128 | `fork:src/opentyr.c:828–833` | C | Remove late duplicate help-text load | `main()` — `src/opentyr.c:974` | high: variant/provider/save selection must precede current initialization ordering | variant table/schema |

### src/opentyr.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H129 | `fork:src/opentyr.h:51–57` | C | Change displayed historical version to 2000 | `titleScreen()` — `src/tyrian2.c:3340` | high: variant/provider/save selection must precede current initialization ordering | variant table/schema |

### src/palette.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H130 | `fork:src/palette.c:27–33` | R | Load 24 palettes instead of 23 | `loadPals()` — `src/palette.c:42` | low: small metadata/capacity change; shared backend remains intact | variant layout/asset metadata + shared renderer |

### src/params.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H131 | `fork:src/params.c:113–119` | F | Remember explicit short xmas option | `JE_paramCheck()` — `src/params.c:51` | low: small metadata/capacity change; shared backend remains intact | common fix/check existing protection |
| H132 | `fork:src/params.c:191–197` | F | Remember explicit long xmas option | `JE_paramCheck()` — `src/params.c:51` | low: small metadata/capacity change; shared backend remains intact | common fix/check existing protection |
| H133 | `fork:src/params.c:252–262` | F | Remember explicit legacy YESXMAS/NOXMAS options | `JE_paramCheck()` — `src/params.c:51` | low: small metadata/capacity change; shared backend remains intact | common fix/check existing protection |

### src/pcxmast.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H134 | `fork:src/pcxmast.c:21–27` | R | Add palette 23 for the fourteenth picture | `JE_loadPic()` — `src/picload.c:34` | low: small metadata/capacity change; shared backend remains intact | variant layout/asset metadata + shared renderer |

### src/pcxmast.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H135 | `fork:src/pcxmast.h:21–27` | R | Allow fourteen pictures | `JE_loadPic()` — `src/picload.c:34` | low: small metadata/capacity change; shared backend remains intact | variant layout/asset metadata + shared renderer |

### src/shots.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H136 | `fork:src/shots.c:228–234` | G/R | Recognize trail 198 alongside trail 98 | `player_shot_move_and_draw()` — `src/shots.c:188` | high: trails create real explosions inside the fixed-tick drawlist path | variant rule hook; variant layout/asset metadata + shared renderer |
| H137 | `fork:src/shots.c:382–394` | G/R | Limit Flying Punch trail to first/center multi-shot tile | `player_shot_create()` — `src/shots.c:336` | high: trails create real explosions inside the fixed-tick drawlist path | variant rule hook; variant layout/asset metadata + shared renderer |

### src/sndmast.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H138 | `fork:src/sndmast.c:51–58` | A | Add names for two new effects | `loadSndFile()` — `src/nortsong.c:213` | low: small metadata/capacity change; shared backend remains intact | variant sound map + common audio |

### src/sndmast.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H139 | `fork:src/sndmast.h:21–27` | A | Expand SFX count to 31 | `loadSndFile()` — `src/nortsong.c:213` | low: small metadata/capacity change; shared backend remains intact | variant sound map + common audio |
| H140 | `fork:src/sndmast.h:58–74` | A | Assign two added effects and shift nine voice IDs by two | `loadSndFile()` — `src/nortsong.c:213` | low: small metadata/capacity change; shared backend remains intact | variant sound map + common audio |

### src/sprite.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H141 | `fork:src/sprite.c:42–48` | R | Declare added compressed ship shape bank | `JE_loadMainShapeTables()` — `src/sprite.c:977` | medium: keep drawlist-recording blitters and bank ownership/lifetime correct | variant layout/asset metadata + shared renderer |
| H142 | `fork:src/sprite.c:794–800` | R | Read 13 banks instead of 12 | `JE_loadMainShapeTables()` — `src/sprite.c:977` | medium: keep drawlist-recording blitters and bank ownership/lifetime correct | variant layout/asset metadata + shared renderer |
| H143 | `fork:src/sprite.c:841–851` | R | Load final 2000 ship bank | `JE_loadMainShapeTables()` — `src/sprite.c:977` | medium: keep drawlist-recording blitters and bank ownership/lifetime correct | variant layout/asset metadata + shared renderer |

### src/sprite.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H144 | `fork:src/sprite.h:36–42` | R | Allow 152 sprites in generic tables | `load_sprites()` — `src/sprite.c:70` | medium: keep drawlist-recording blitters and bank ownership/lifetime correct | variant layout/asset metadata + shared renderer |
| H145 | `fork:src/sprite.h:112–118` | R | Expose new compressed ship bank | `JE_loadMainShapeTables()` — `src/sprite.c:977` | medium: keep drawlist-recording blitters and bank ownership/lifetime correct | variant layout/asset metadata + shared renderer |

### src/tyrian2.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H146 | `fork:src/tyrian2.c:670–676` | R | Restore title-logo animation after level end | `JE_main()` — `src/tyrian2.c:589` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant layout/asset metadata + shared renderer |
| H147 | `fork:src/tyrian2.c:694–705` | G | Return to title on Timed Battle game over | `JE_main()` — `src/tyrian2.c:589` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H148 | `fork:src/tyrian2.c:931–937` | G | Do not autosave Timed Battle level start | `JE_main()` — `src/tyrian2.c:589` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H149 | `fork:src/tyrian2.c:2130–2140` | A/G | Suppress countdown warning sounds in Timed Battle | `JE_main()` — `src/tyrian2.c:589` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant sound map + common audio; variant rule hook |
| H150 | `fork:src/tyrian2.c:2148–2155` | F | Format level timer with integer truncated tenths | `JE_main()` — `src/tyrian2.c:589` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | common fix/check existing protection |
| H151 | `fork:src/tyrian2.c:2701–2707` | C | Use relocated Destruct name index in episode script | `JE_main()` — `src/tyrian2.c:589` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant table/schema |
| H152 | `fork:src/tyrian2.c:3004–3028` | G | Handle ]T selection jumps and ]q battle high-score exit | `JE_main()` — `src/tyrian2.c:589` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H153 | `fork:src/tyrian2.c:3293–3298` | D | Remove old Setup override after moving it to help loader | `titleScreen()` — `src/tyrian2.c:3340` | low: one binary/provider namespace supersedes fork structure | drop |
| H154 | `fork:src/tyrian2.c:3321–3346` | R | Draw/animate 2000 logo sprite with Tyrian title | `titleScreen()` — `src/tyrian2.c:3340` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant layout/asset metadata + shared renderer |
| H155 | `fork:src/tyrian2.c:3518–3532` | C/G | Change cheat announcement voice and handle cancelled Super Tyrian selection | `titleScreen()` — `src/tyrian2.c:3340` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant table/schema; variant rule hook |
| H156 | `fork:src/tyrian2.c:3626–3638` | G | Run Timed Battle selection before difficulty | `newGame()` — `src/tyrian2.c:3674` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H157 | `fork:src/tyrian2.c:3663–3669` | C | Give E5 smuggled arcade ship 20000 starting cash | `newGame()` — `src/tyrian2.c:3674` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant table/schema |
| H158 | `fork:src/tyrian2.c:3685–3693` | R | Render new super-arcade ship from 2000 bank | `newSuperArcadeGame()` — `src/tyrian2.c:3719` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant layout/asset metadata + shared renderer |
| H159 | `fork:src/tyrian2.c:3698–3704` | G | Reset Timed Battle when starting super-arcade | `newSuperArcadeGame()` — `src/tyrian2.c:3719` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H160 | `fork:src/tyrian2.c:3718–3746` | C/A | Use data-backed Super Tyrian intro strings and alternate voice | `newSuperTyrianGame()` — `src/tyrian2.c:3761` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant table/schema; variant sound map + common audio |
| H161 | `fork:src/tyrian2.c:3748–3777` | G | Allow Super Tyrian starting episode selection and cancellation | `newSuperTyrianGame()` — `src/tyrian2.c:3761` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H162 | `fork:src/tyrian2.c:3950–3967` | G | Keep full launch IDs in second enemy bank rather than modulo 1000 | `JE_makeEnemy()` — `src/tyrian2.c:3930` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H163 | `fork:src/tyrian2.c:4210–4222` | G | Interpret enemy spawn x=-200 as RNG position 24..231 | `JE_createNewEventEnemy()` — `src/tyrian2.c:4215` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H164 | `fork:src/tyrian2.c:4343–4350` | G | Treat event 83 as map-stop alias of 4 | `JE_eventSystem()` — `src/tyrian2.c:4325` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H165 | `fork:src/tyrian2.c:4956–4995` | G | Implement event 58 launch and 59/68 replacement using same 25-slot enemy group | `JE_eventSystem()` — `src/tyrian2.c:4325` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H166 | `fork:src/tyrian2.c:5042–5047` | G | Remove 2.1 event 68 random-explosion semantics | `JE_eventSystem()` — `src/tyrian2.c:4325` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H167 | `fork:src/tyrian2.c:5184–5215` | G | Implement 84/85 Timed Battle timer/drop rules and event 99 random explosions | `JE_eventSystem()` — `src/tyrian2.c:4325` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |
| H168 | `fork:src/tyrian2.c:5338–5347` | R | Move Classic boss bars below active timer | `draw_boss_bar()` — `src/tyrian2.c:5268` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant layout/asset metadata + shared renderer |

### src/tyrian2.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H169 | `fork:src/tyrian2.h:61–67` | G | Return success/cancel from Super Tyrian start | `newSuperTyrianGame()` — `src/tyrian2.c:3761` | high: fixed-tick loop, drawlist, VFX and interpolation hooks surround changed rules | variant rule hook |

### src/varz.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H170 | `fork:src/varz.c:38–47` | C | Extend arcade progression/ship/special tables for Dragon and Pretzel Pete | `JE_getShipInfo()` — `src/varz.c:317` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant table/schema |
| H171 | `fork:src/varz.c:50–58` | C | Add weapon loadouts for the new arcade ships | `JE_getShipInfo()` — `src/varz.c:317` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant table/schema |
| H172 | `fork:src/varz.c:150–156` | C | Expand per-ship twiddle selection table | `JE_getShipInfo()` — `src/varz.c:317` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant table/schema |
| H173 | `fork:src/varz.c:165–176` | C | Add five ship combo selection rows | `JE_getShipInfo()` — `src/varz.c:317` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant table/schema |
| H174 | `fork:src/varz.c:331–337` | R | Choose added shape bank for ship graphics above 500 | `JE_getShipInfo()` — `src/varz.c:317` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant layout/asset metadata + shared renderer |
| H175 | `fork:src/varz.c:345–351` | R | Normalize graphic ID by subtracting bank offset | `JE_getShipInfo()` — `src/varz.c:317` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant layout/asset metadata + shared renderer |
| H176 | `fork:src/varz.c:493–502` | D | Change quit-message Christmas/version text | `JE_tyrianHalt()` — `src/varz.c:436` | low: one binary/provider namespace supersedes fork structure | drop |
| H177 | `fork:src/varz.c:882–888` | G/R | Extend explosion table to 54 entries | `JE_setupExplosion()` — `src/varz.c:843` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant rule hook; variant layout/asset metadata + shared renderer |
| H178 | `fork:src/varz.c:935–942` | G/R | Add explosion type 54 mapping | `JE_setupExplosion()` — `src/varz.c:843` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant rule hook; variant layout/asset metadata + shared renderer |
| H179 | `fork:src/varz.c:952–958` | G/R | Recognize Flying Punch trail 198 as explosion 98 | `JE_setupExplosion()` — `src/varz.c:843` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant rule hook; variant layout/asset metadata + shared renderer |
| H180 | `fork:src/varz.c:1062–1069` | G | Keep Timed Battle timer enabled when player dies | `JE_playerDamage()` — `src/varz.c:1015` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant rule hook |

### src/varz.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H181 | `fork:src/varz.h:26–42` | C | Expand arcade ship count and relocate Destruct/Engage sentinel IDs | `JE_getShipInfo()` — `src/varz.c:317` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant table/schema |
| H182 | `fork:src/varz.h:221–227` | C | Declare extended ship combo selection table | `JE_getShipInfo()` — `src/varz.c:317` | high: ship IDs, trails and explosion creation feed motion identity and VFX | variant table/schema |

### src/video.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H183 | `fork:src/video.c:88–94` | D | Use fork application identity for SDL window title | `init_video()` — `src/video.c:81` | low: one binary/provider namespace supersedes fork structure | drop |

### src/xmas.c

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H184 | `fork:src/xmas.c:31–37` | F | Store explicit xmas override independently from seasonal auto-detection | `xmas_time()` — `src/xmas.c:35` | low: small metadata/capacity change; shared backend remains intact | common fix/check existing protection |

### src/xmas.h

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H185 | `fork:src/xmas.h:22–28` | F | Expose explicit xmas override flag | `xmas_time()` — `src/xmas.c:35` | low: small metadata/capacity change; shared backend remains intact | common fix/check existing protection |

### visualc/resources.rc

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H186 | `fork:visualc/resources.rc:1–1` | D | Fork executable/install/docs branding or data-source wording | `visualc/resources.rc:1` (packaging/document metadata; no function) | low: one binary/provider namespace supersedes fork structure | drop |

### visualc/tyrian2000.ico

| ID | Fork range | Class | What changes | Our integration owner | Modern conflict risk and reason | Proposed destination |
|---|---|---|---|---|---|---|
| H187 | `fork:visualc/tyrian2000.ico` (binary) | D | Fork-specific icon asset | `visualc/resources.rc:1` / application packaging | low: one shared application identity | Drop; do not import data-derived icons |
## Per-item integration notes and behaviour evidence

The ledger covers **181 text hunks and six binary additions (187 rows)**. The six binary assets are five Linux icons and one Windows icon. None is needed for a single Modern Tyrian application. Packaging hunks also change a man-page assertion about bundled data: drop that assertion, retain only the verified external data source in future documentation. Do not import these icons as an alternate way to ship original art.

### Timed Battle (2000-only)

`832f2d0` introduces the mode, menu, high-score tables and script handling. `7a3ff18` replaces an early direct-end countdown with the existing event-jump timer: `fork:src/tyrian2.c:2119–2155` decrements once per logic tick, jumps to `levelTimerJumpTo` at zero and suppresses warning samples only in Timed Battle. This is the same numeric countdown convention as event 67; it is **not a new wall-clock timer**. `83812e0` replaces floating rounding with truncated integer tenths (also useful as a common timer-format fix, but 2.1 framebuffer changes need explicit review).

`fork:src/menus.c:677–858` selects three battles, initializing episode 1 for selection 1 and episode 5 for selections 2/3. `fork:src/tyrian2.c:3008–3025` recognizes `]T` jump selection and `]q` battle termination/high-score check. `fork:src/tyrian2.c:3629–3635` makes it one-player arcade before difficulty selection. The exact chapter targets are supplied by the episode command stream, not a second level implementation. Game over returns to title (`fork:src/tyrian2.c:697–702`).

`85a5a22` fixes the end-level bonus to `(levelTimerCountdown / 10) * 100`, i.e. integer tenths ×100 cash; remaining lives contribute 1,000 each (`fork:src/mainint.c:2763–2820`). `3e395df` resets Timed Battle state on normal saves/new games, leaves current music playing during battle score entry, draws its special backdrop and makes score cancellation ineffective. `c954184` excludes battle start from backup saves (`fork:src/tyrian2.c:934`). Player death preserves the timer (`fork:src/varz.c:1064–1067`). The score is cash only, assigned to `timeBattleSelection - 1` (`fork:src/mainint.c:2034–2040`), rather than normal episode score calculation.

Use a mode enum/session field and a variant `TimedBattleRules` descriptor; timers, event jumps, player death and end-level screens remain common. Keep all mode resets together so loading a 2.1-style save cannot retain battle state. The fork declares ten Timed Battle score tables but exposes only three selections; serialize all ten for compatibility, rather than shrinking the suffix to three.

### T2K events and spawn rules (2000-only)

| Evidence | Rule | Destination / remaining accuracy issue |
|---|---|---|
| `e376530`, `fork:src/tyrian2.c:3955–3967` | Enemy IDs above 1000 use full `elaunchtype`; older bank splits type modulo 1000 and special quotient | Variant enemy-launch decoder; retain old semantics in 2.1 |
| `c0782f0`, `fork:src/tyrian2.c:4213–4219` | Spawn X sentinel −200 is replaced with `mt_rand() % 208 + 24`; it mutates that event record | Variant spawn hook preserving mutation and RNG draw count; no renderer RNG |
| `832f2d0`, `fork:src/tyrian2.c:4959–4967` | Event 58 sets launch type on first 100 slots, link match or wildcard 99 | Variant event hook; fork comments say ArcTyr-derived and possibly inaccurate |
| `e376530`, `62f58dc`, `fork:src/tyrian2.c:4969–4992` | Events 59/68 replace matching enemies; wildcard is **0**; allocate in source slot’s 25-slot group, copy position when allocation succeeds, mark source free regardless | Variant event hook; preserve allocation order/failure behaviour; historical fidelity remains unproved |
| `10a5752`, `fork:src/tyrian2.c:4346–4350` | Event 83 aliases map-stop event 4 | Variant event alias table |
| `832f2d0`, refined `7a3ff18`, `fork:src/tyrian2.c:5187–5196` | Event 84 copies event 67 timer enable/countdown/jump parameters, only in battle mode | Variant event table and shared timer helper |
| `e376530`, `fork:src/tyrian2.c:5197–5207` | Event 85 changes matched enemy death/drop type only in battle mode | Variant hook, first 100 slots |
| `e376530`, `fork:src/tyrian2.c:5211–5213` | Random explosions move from event 68 to 99 | Variant dispatch table: **68 must remain random explosions in 2.1** |

The repeated `break` after replacement is dead code and should be dropped. The fork does not test `enemyAvail` before matching; replacing this with a cleaner active-enemies-only loop would change the reference behaviour. Confirm against DOS 2000 before correcting it. These source comments mean that “identical to the original” is a target, not a demonstrated result of this research.

### Twiddles and arcade ships (2000-only)

`e1b86fc`, `fork:src/varz.c:153–174`, extends `shipCombos` from 14 to 19 rows: Storm, Red Dragon, Gencore II, PeteZoomer and Rum Bottle receive added selection rows. The input recognizer `JE_SFCodes()` still consumes the existing `keyboardCombos` and `shipCombosB`; the delta changes ship-to-combo selection, **not the fundamental directional combo language**. `c0782f0`, `fork:src/varz.c:41–57`, extends the arcade ship/progression/special/loadout tables from seven to nine ships, adding Dragon and Pretzel Pete; Destruct and Engage sentinel values move to 10/11 (`fork:src/varz.h:29–40`). Save decoding and special switching use `SA_LASTSHIP` instead of the old NortShip limit.

Put these tables and semantic sentinel mapping in the variant descriptor. Never globally replace the 2.1 tables: both RNG order and stored arcade IDs matter. Newly available ports, ships, shields, sidekicks, specials and enemy/weapon banks are data schemas, not separate gameplay loops. See [data-formats.md](data-formats.md).

### Charging sidekicks (2000-only)

`dfe1050`, `fork:src/mainint.c:4601–4606`, changes only the **infinite-ammo** branch: fire when `(button[0] && !this_option->pwr) || button[1 + i]`. Main fire triggers a sidekick without charge stages; a charged sidekick requires its own sidekick action. Charge advancement, ammo-limited logic and repeat timing are unchanged by this hunk. Use `sidekick_fires_on_main(const JE_OptionType *)` or a rule flag inside the common movement path, preserving 2.1’s current condition. Validate manual left/right/both mappings and the HUD charge bar together.

### Rear “None” and upgrade eligibility (2000 historical change plus common safety fix)

`3896d6c` explicitly says this was a 2.1 bug fixed in 2000. It determines rear eligibility from the selected item ID, instead of assuming the final purchasable row is “None”; front “None” remains upgradeable as the fork says the original permits. The final combined hunk is `fork:src/game_menu.c:499–531`. Keep the 2.1 historical policy unless separately authorized to change it. `7bf22d9` wraps the item lookup in a valid front/rear/not-Done guard: that bounds-check principle is a common fix, even though it appears in the same hunk as variant eligibility. Our `JE_itemScreen()` must be checked at the actual rewritten selection block, not patched by fork line number.

Our current upstream-derived `JE_itemScreen`, `src/game_menu.c:649–666`, already checks front/rear/not-Done before indexing and tests the selected item ID rather than a rear last-slot assumption. Thus the old rear-slot bug and the unsafe pre-guard lookup are already absent here. The remaining difference in this block is the fork allowing front “None” power changes, while ours rejects item zero for either port; retain our current 2.1 behaviour and use the 2000 policy only for its descriptor. Do not reintroduce the old 2.1 fork-base bug as a supposed fidelity fix.

The preview uses the resulting `leftPower/rightPower` state, cycles the rear-mode hint over 150 preview ticks and draws two mode lights (`26f40e7`, `fork:src/game_menu.c:3239–3291,3365–3381`). These are presentation policies carried by the shared menu implementation.

### Hazudra Fodder high scores (2000-only policy)

`5e842da`, `fork:src/mainint.c:976–982`, removes the `episodeNum != EPISODE_AVAILABLE` condition at initial-episode completion, allowing the final episode’s score check. The 2000 save suffix has boards for all five episodes; 2.1’s save-backed high-score UI has only three episodes. Applying this predicate globally risks indexing nonexistent 2.1 boards and changing historical scoring. Expose board availability and completion-score policy through the variant.

### Obscured-vision score display (renderer; possible common accessibility fix)

`7afd981`, `fork:src/mainint.c:3077–3087`, draws cash with bank 8/brightness 8 when `smoothies[5]` is active, otherwise bank 2/brightness 4. There is no scoring change or variant test in the commit: this is a readability fix in the fork, not proof of a historical 2000 rule. Our Modern HUD already draws cash outside the obscured playfield (`src/modern_hud.c:553`); no need to recolor it blindly. Keep the 2.1 Classic baseline unless a common visual correction is approved, and verify a 2000 obscured-vision case.

### Flying Punch trail and explosion type 54 (2000-only semantics)

`0bfca44`, `fork:src/shots.c:231,385–391`, treats trail 198 like 98 in motion but disables it on multi-shot positions after the first, reproducing a center-tile trail. `fork:src/varz.c:955` recognizes the same trail in explosion setup. The commit’s comment states that the original mechanism is uncertain; the fork approximates its observed appearance. `c0782f0` also expands the explosion data to 54 entries (`fork:src/varz.c:885,938–939`). These are real engine explosion objects and must remain in logic, tagged by existing explosion draw contexts. Modern decorative trails are separate observations. Compare state/replay hashes so a visual port cannot add extra `mt_rand()` or explosion allocations.

### Christmas (general fix, data-dependent presentation)

`d9904c1`, `fork:src/params.c:116,194,255–259`, records explicit `--xmas`, `--no-xmas`, short equivalents and YESXMAS/NOXMAS choices. `fork:src/opentyr.c:773–774,798` runs seasonal detection/prompt only if there was no explicit choice. This fixes override ordering and prompt behaviour for either variant; Christmas itself already exists in 2.1. Our `main()` already sets `xmas_time()` **before** full parsing (`src/opentyr.c:1060`), so the ordering bug is absent; its unconditional Christmas prompt remains a separate candidate. Preserve asset-existence checks and the regression default `xmas=false`. Both supplied variants include `tyrianc.shp` and `voicesc.snd`; the 2000 shape version has the added bank too.

### Other differences that must not be lost

- `c0782f0`, later refined by `3e395df`, adds episode-5 starting cash 20,000 when converting an arcade ship into a full-game start (`fork:src/tyrian2.c:3666`). Episodes 1–4 retain their old cash table.
- `c0782f0`, `fork:src/tyrian2.c:3721–3773`, makes Super Tyrian choose its starting episode, support cancellation, use data-backed instructions and a different introduction voice. The title cheat announcement also changes voice (`fork:src/tyrian2.c:3521`). These are variant startup/UI policies.
- `e750f3d` adds save-name/original-slot confirmation before mouse or keyboard writes (`fork:src/game_menu.c:2381–2489`; `fork:src/mainint.c:3017–3019,3059–3061`). Reuse Modern modal centering and gamepad cancellation, preserving 2.1’s current flow.
- `26f40e7`/`d21207b` add mouse bindings, ship/weapon illustration offsets, rear-mode lights and help mapping. Input action configuration can be shared, while variant strings/row topology belong in descriptors. The fork’s physical SDL2 button switch must not replace SDL3 deterministic event filtering.
- `c0782f0` adds 12-line warnings, an extra generic planet/title sprite, 13-bank ships, fourteen pictures and palette 24; credits now have 126 rather than 131 records. These are asset schema/layout differences.
- `63956f0` adds a configuration-loaded guard. Our upstream-derived code already separates `loadConfiguration()` and `loadSaves()`, validates reads, and never registers the fork’s blanket save-on-halt pattern. Re-evaluate the specific late provider failure path rather than introducing a redundant global flag. `42ae4f2` initializes mouse defaults before a missing-file return; valid defaults remain necessary if shared mouse rebinding is added.
- `ef0bc8d` bounds pickup message formatting and adjusts episode filename formats; use our checked `File` API and bounded string helpers. Some `snprintf` lines already match our upstream-derived implementation. Cosmetic `sizeof` parenthesization is a drop/no-op, not a gameplay port.

## Limits of the evidence

This phase inspected source history, measured the verified archives, built our engine and checked existing regressions. It did **not** run DOS 2000 or prove the ArcTyr-derived replacement/launch rules, Timed Battle episode routing assumptions, or Flying Punch approximation against original gameplay. Those are explicit Phase 3/4 comparison obligations. The surviving delta contains no new audio emulator, renderer backend, network arena or alternate demo encoding; upstream changes outside the merge base must be evaluated separately.
