# Tyrian 2.1 and Tyrian 2000 data formats

## Evidence and compatibility boundary

The 2000 archive was fetched from [the source linked by OpenTyrian2000](https://www.camanis.net/tyrian/tyrian2000.zip), verified at **5,051,363 bytes** and SHA-256 **`348bc76e73514e452279b8730cf217daf0f70a282f07b6b94af653d87e921667`**, and extracted only into external research scratch storage. It has **100 ZIP entries: one directory and 99 files**, totaling **12,337,252 uncompressed file bytes**. Measurements below compare it with the existing 2.1 data whose consumed files match `test/regress/data-manifest.txt:1`. Documents contain metadata and format descriptions only; no original strings, maps, sprites, sounds or demo streams are reproduced.

Source anchors use fork `aad5aca01af139c0b089237c38ef765f7a84355d` and our `a2d82d82bb0ac83a2253b18fc5d9ee231a7f61b9`. These are **loader contracts**, not an assertion that all DOS-private fields have been reverse engineered. Multi-byte fields below are little-endian unless specified otherwise.

### Measured file inventory

| File | 2.1 bytes | 2000 bytes | Relevant structure |
|---|---:|---:|---|
| `tyrian.hdt` | 153,657 | 295,069 | Encrypted string sections followed by item data |
| `tyrian.shp` | 443,871 | 505,983 | 12 vs 13 shape banks |
| `tyrianc.shp` | 444,862 | 516,757 | Christmas version of same bank schema |
| `tyrian.pic` | 365,969 | 367,161 | 13 vs 14 compressed pictures |
| `palette.dat` | 17,664 | 18,432 | 23 vs 24 palettes, 768 bytes each |
| `tyrian.cdt` | 1,125 | 1,078 | 131 vs 126 encrypted Pascal credit records |
| `music.mus` | 153,482 | 153,482 | 41 songs, byte-identical |
| `tyrian.snd` | 264,512 | 271,689 | 29 vs 31 effects |
| `voices.snd` | 132,767 | 132,767 | Nine voices, byte-identical |
| `voicesc.snd` | 188,275 | 188,275 | Nine Christmas voices, byte-identical |
| `tyrian1.lvl` | 538,262 | 538,856 | 37 offsets: 18 physical levels and terminal offset |
| `tyrian2.lvl` | 381,242 | 381,187 | 25 offsets: 12 physical levels and terminal offset |
| `tyrian3.lvl` | 393,398 | 393,938 | 25 offsets: 12 physical levels and terminal offset |
| `tyrian4.lvl` | 800,006 | 937,303 | 41 offsets: 20 physical levels plus item block |
| `tyrian5.lvl` | absent | 521,137 | 17 offsets: eight physical levels plus item block |
| `levels1.dat` | 9,359 | 9,600 | Episode command stream; adds Timed Battle routing |
| `levels2.dat` | 5,534 | 5,534 | Same size but different contents |
| `levels3.dat` | 6,052 | 6,052 | Byte-identical |
| `levels4.dat` | 11,103 | 11,116 | Expanded episode command content |
| `levels5.dat` | absent | 4,257 | Episode-5 command stream |
| `cubetxt1.dat` | 36,169 | 35,780 | Cube records, same encoding |
| `cubetxt2.dat` | 8,108 | 8,109 | Cube records, same encoding |
| `cubetxt3.dat` | 12,294 | 12,277 | Cube records, same encoding |
| `cubetxt4.dat` | 62,424 | 61,763 | Cube records, same encoding |
| `cubetxt5.dat` | absent | 7,024 | Episode-5 cube records |
| `estsc.shp` | 115,338 | 115,338 | Byte-identical credits illustrations |

Both packages lack `tyrian.sav` and `tyrian.cfg`: they are created by the game. Save sizes below are derived from serializers, not a packaged DOS save. File sizes alone are not a validator: several files differ without changing size.

## Episode 5 and the level containers

`src/lvllib.c:34` (`analyzeLevel`) reads a 16-bit offset count followed by that many 32-bit offsets. This container is unchanged in the fork. Offset entries occur in pairs for each physical level, and the final entry is EOF for episodes 1–3 or the item-data block for episodes 4/5. Episode 5 has eight physical level records, not five or one; the story’s logical sections are distinct from physical levels. `src/episodes.c:247` (`JE_initEpisode`) already constructs `tyrian5.lvl`, `cubetxt5.dat`, `levels5.dat` filenames and `src/episodes.h:29` already reserves five episode slots. Only `EPISODE_AVAILABLE` remains four.

At the start of each physical level: one map-bank selector byte, one shape-bank selector byte, three 16-bit map X positions, a 16-bit count and list of 16-bit enemy type IDs, then a 16-bit event count. Each event is **11 bytes**: time u16, type u8, two s16 parameters, three s8 parameters and one u8 link parameter. The fork changes event semantics, not event byte layout. Existing background shape/map decoding stays common (`JE_loadMap`, `src/tyrian2.c`; parsing also documented by `tools/scan_smoothies.py:61`). Expanded enemy/shape IDs require variant bounds.

`levels5.dat` uses the same encrypted length-prefixed command records consumed by the episode interpreter (`src/tyrian2.c:2421` onward). Added commands `]T` and `]q` select battle sections and terminate battles (`fork:src/tyrian2.c:3008–3025`). `cubetxt5.dat` uses the same cube parser (`load_cubes`, `src/game_menu.c:1904`): title/content/face metadata remain the same, but more face sprites require bounds-safe palette selection (`fork:src/game_menu.c:750`). No parallel E5 parser is needed.

The item block is read from the header offset in `tyrian.hdt` for episodes 1–3 and the final level offset for episodes 4/5 (`fork:src/episodes.c:54–72`; `src/episodes.c:54–96`). Its location is 16,465 in 2.1 HDT and 21,174 in 2000 HDT. Each 2000 item block occupies **273,895 bytes** in HDT/E4/E5; E4 2.1 occupies **137,192 bytes**.

### Item tables and banks

The seven 16-bit header fields are maximum IDs for weapons, ports, generators, ships, sidekicks, shields and first-bank enemies. They do **not** fully describe special weapons or the second banks. The fork still uses explicit constants. The maximum ID and the number of serialized records must be separate fields.

| Table | Bytes per record | 2.1 serialized IDs | 2000 serialized IDs read by fork |
|---|---:|---|---|
| Weapons | 80 | 0–780 | 0–818 and 1000–1818; gaps not present in stream |
| Ports | 82 | 0–42 | 0–60 |
| Specials | 37 | 0–46 | 0–54; not represented in seven-field header |
| Generators | 37 | 0–6 | 0–6 |
| Ships | 41 | 0–13 | 0–18 |
| Sidekicks | 86 | 0–30 | 0–37 |
| Shields | 37 | 0–10 | 0–11 |
| Enemies | 77 | 0–850 | 0–850 and 1001–1850; gaps not present in stream |

Counts and loops: `fork:src/lvlmast.h:26–42`; `fork:src/episodes.c:74–232`; our `src/lvlmast.h:26–36`, `JE_loadItemDat`, `src/episodes.c:54`. Embedded item names use an 8-bit length plus a **fixed 30-byte name field**; they are not the variable-length encrypted strings of HDT.

**Unconsumed trailer:** the fork’s table loops account for 273,818 bytes including the 14-byte header, leaving **77 bytes** at the end of each measured 2000 item block. That is one enemy-record-sized remainder. This research does not assign it an enemy ID or silently change the fork’s 1001 starting ID. Preserve known bank indexing for the initial port; investigate the remaining record against DOS behaviour before declaring a fully validated item schema. A validator must not mistake this known remainder for truncation, nor accept arbitrary trailing data.

### Static event coverage for regression planning

| Container/physical levels | Added/reinterpreted event IDs present |
|---|---|
| E1/L5 | 84 |
| E4/L5, L11 | 68 |
| E4/L15, L18 | 99 |
| E5/L3 | 84, 85 |
| E5/L4 | 83, 84 |
| E5/L6 | 58 |
| E5/L7 | 58, 59, 68 |
| E5/L8 | 58 |

This is a structural scan, not runtime reachability: conditional jumps and battle gating can skip records. E5/L1–2 and L5 still need ordinary gameplay coverage. See [fork-diff.md](fork-diff.md) for semantics and the fork’s explicit accuracy caveats.

## HDT strings and “read ALL strings”

The format remains a 32-bit item-data offset followed by sections of encrypted Pascal strings. Each string has a one-byte length and that many encrypted bytes; section marker records are consumed with the same mechanism. Decryption is the existing backwards XOR-chain (`src/helptext.c:70–106`); the byte format and key do not change. The fork’s `73eab84` expands counts and reads/skips remaining sections rather than changing encryption.

| Section/capacity | 2.1 | 2000 | Reference |
|---|---:|---:|---|
| Misc text | 68 | 72 | `fork:src/helptext.h:37` |
| Little misc text | 5 ×11-byte C buffers | 8 ×12 | `fork:src/helptext.h:38–39` |
| Title menu string buffer | 21 | 29 | `fork:src/helptext.h:40` |
| Main menu help | 34 | 37 | `fork:src/helptext.h:41` |
| Network text | 4 ×22 | 5 ×33 | `fork:src/helptext.h:42–43` |
| Gameplay names | 5 | 6 | `fork:src/menus.h:24` |
| Super ships / special names | 11 / 9 | 13 / 11 | `fork:src/helptext.h:44–45` |
| Ship descriptions | 13 pairs | 20 pairs | `fork:src/helptext.h:46` |
| Menu 3 / menu 12 / menu 15 entries | 8 / 6 / absent | 9 / 7 / 6 | `fork:src/helptext.c:190–191` |
| Timed Battle names | absent | 4 | `fork:src/helptext.c:403–408` |
| Default player/team names | hardcoded 34 / 22 | data-backed 39 / 10 | `fork:src/helptext.c:429–441` |

After menu 14, the fork reads Timed Battle names, skips ten unused setup blocks (counts 10,5,4,4,5,7,7,21,3,3), reads mouse menu 15, licensing info (three strings), default names, ordering info (six), and Super Tyrian intro (six). See `fork:src/helptext.c:396–452`. “ALL” includes **structurally skipping** setup records that the port does not use; it is not a requirement to show every DOS setup option.

Our loader reads 68 misc strings, so on 2000 it consumes the next misc record as the section-end marker and immediately shifts subsequent arrays (`src/helptext.c:196–205`). It cannot safely reuse 2.1 counts. Debug builds can assert when a shifted string is larger than a destination; release builds truncate/copy wrong labels and later misparse sections. Appending only the new tail sections does not fix the earlier count shifts. Default score names must be ready before generating missing-save tables, while shared presentation/input configuration can load without HDT.

Existing HUD indices 48/49 (player labels) and 66 (timer) are retained by the fork; new records append to misc text rather than redefining those known uses. Preserve semantic label mapping and verify actual 2000 strings through the loader without copying them into source or tests.

## Shapes, ships, pictures and palettes

Main shape files begin with u16 bank count and u32 offsets. Banks 0–6 contain generic sprite tables; the remaining banks use the compressed `Sprite2` representation already decoded by common blitters. In 2000 the bank count is 13 and the new final compressed bank contains additional ships (`fork:src/sprite.c:797–848`). Normal and Christmas files both use this expanded schema.

| Generic bank index | 2.1 sprite count | 2000 sprite count |
|---:|---:|---:|
| 0 | 60 | 60 |
| 1 | 85 | 85 |
| 2 | 127 | 127 |
| 3 | 151 | 152 |
| 4 | 12 | 18 |
| 5 | 45 | 47 |
| 6 | 22 | 22 |

The fork raises generic table capacity to 152 (`fork:src/sprite.h:39`), routes ship graphic IDs above 500 to the added bank and subtracts 500 (`fork:src/varz.c:334,348`; `fork:src/game_menu.c:1901–1904`). Main ship paths and menu previews must share that resolution. Also inspect the second-player path, which the fork does not extend symmetrically; do not assume new ships work in every two-player context just because player one renders correctly. New large illustrations need placement metadata for graphics 45/46; planet bank adds the 2000 title mark.

Enemy shape-file selection grows from 34 to 36 entries, adding apostrophe and percent (`fork:src/lvlmast.c:23–27`). Additional archive shape filenames include `newsh@.shp` (16,498 bytes), `newsh'.shp` (35,108), `newsh$.shp` (18,016), `newsh%.shp` (40,244), and `newsh(.shp` (32,060). File presence and bank requirements should come from the variant manifest, not from an allowlist built only from existing 2.1 demo use.

`tyrian.pic` is the same count/offset container and PCX-style RLE payload at 320×200, now fourteen rather than thirteen pictures (`fork:src/pcxmast.h:24`). `pcxpal` adds palette 23 at its final position (`fork:src/pcxmast.c:24`); palettes remain 256 RGB triplets with six-bit components, with 24 palettes instead of 23 (`fork:src/palette.c:30`). The Timed Battle score screen calls picture 13 in the fork; the new fourteenth picture is not synonymous with that call.

Our generic sprite loader asserts the old capacity; the main-bank loader asserts twelve and then caps reads (`src/sprite.c:977`). With assertions removed, the last old bank is sized to EOF and absorbs the added bank instead of being delimited by its own next offset. Our picture loader similarly assumes thirteen (`src/picload.c:51`) and cannot select picture fourteen. Existing early startup rejection prevents these bad paths today. The credits parser requests 131 records from a 126-record file and would hit EOF (`src/mainint.c:2417–2440`).

## Music and sound effects

`music.mus` has u16 song count and u32 offsets followed by existing LDS song data. Both archives have 41 songs and are byte-identical. Our `load_song`, `src/loudness.c:274`, already reads the dynamic song count; Nuked-OPL3, gain normalization and resampling can stay shared. Its static cached offsets and `song_playing` state must reset if the launcher can change providers in-process.

`tyrian.snd` has the same u16 count/u32 offset structure and signed 8-bit mono sample payload at 11,025 Hz. 2000 adds two effects, so there are **31 effects + nine voices = 40 sound slots**, instead of 29+9=38. `fork:src/sndmast.h:24,61–80` inserts effect IDs 30/31 and shifts voice IDs from 30–38 to 32–40. `voices.snd` and `voicesc.snd` are unchanged bytes. The sound offset namespace is **variant dependent even when voice bytes are identical**.

`loadSounds`, `src/nortsong.c:137–164`, asserts count equality then caps to the expected count. Under 2.1 release constants, loading the 31-effect file reads only 29 offsets, treats EOF as the end of effect 29, and includes remaining effects in that sample; voice offsets are then assigned two slots too early. Debug asserts first. Correct both sample capacity/count and semantic voice mapping together, through the shared resampler/mixer. Retain the existing 100-byte voice-tail trimming policy; no new audio backend appears in this fork.

## Demos

The verified archive contains **`demo.1` through `demo.5`**, and all five are byte-identical to our current 2.1 copies. Their sizes are respectively **2,745, 2,193, 1,974, 699 and 1,122 bytes**. They all select episode 1, with physical levels **9,12,13,7,5**. This corrects the plan’s “demo.1–4” inventory. They remain useful for integration consistency, but they contain no episode-5/new-ship coverage.

The unchanged layout is a **31-byte header**, then a big-endian 16-bit initial key wait and repeated **key-mask u8 + big-endian wait u16** records. Header fields are episode, ten-character level name, physical level, twelve item/loadout bytes, two weapon powers, three unused bytes and song. End-of-stream handling is unchanged (`src/demo.c:55–159`). The fork changes no `demo.c` hunk. Our playback loops through five demos (`src/demo.c:59`) and needs no format rewrite; a variant demo-count field prevents a future incomplete installation from being treated as valid.

Because maps, item tables and rules change, identical input streams **do not imply identical framebuffer/state hashes** across variants. Baselines must remain separate. The synthetic E5 and battle cases must operate on the installed data or constructed code-owned event fixtures; do not check demo bytes or extracted script records into Git.

## Save layout and namespaces

Our serializers (`loadSaves`, `src/config.c:888`; `saveSaves`, `src/config.c:989`) and fork prefix (`fork:src/config.h:27–35`, `fork:src/config.c:834–906`) use the same encrypted save block. Do not serialize C structs directly: padding and host endian are not the format.

| File region | Offset | Length | Both variants |
|---|---:|---:|---|
| 22 save slots | 0 | 2,398 | 109 bytes each, encrypted |
| Editor/item availability | 2,398 | 100 | Includes editor level in final two bytes; encrypted |
| Integrity bytes | 2,498 | 4 | Unencrypted sum, subtractive sum, multiplicative accumulator, XOR |
| 2.1 EOF | 2,502 | — | Total **2,502 bytes** |
| 2000 Timed Battle boards | 2,502 | 1,050 | 10 boards ×3 entries ×35 bytes; unencrypted |
| 2000 main-game boards | 3,552 | 1,170 | 10 boards ×3 entries ×39 bytes; unencrypted |
| 2000 EOF | 4,722 | — | Total **4,722 bytes** |

The 109-byte slot is: encode u16; level u16; items[12]; two u32 scores; level-name length u8 plus nine bytes; save name[14]; cube count u8; two power bytes; episode u8; lastItems[12]; difficulty, secret hint, input1, input2, repeated-game flag, initial difficulty (six bytes); two s32 legacy high scores; high-score-name length u8 plus 29 bytes; score difficulty u8. The 2.1 slots also carry the legacy three-episode high scores.

A 35-byte suffix entry is s32 score, length u8, name[29], difficulty u8. A 39-byte main-game entry additionally stores an unknown four-byte field between score and name length. The fork reads/skips it, writes a placeholder, and does not preserve it (`fork:src/config.c:908–942,1087–1127`). It treats names as up to 29 characters but does not clamp the file’s length before writing its terminator: our reader must reject/clamp malformed lengths safely. The newly randomized difficulty fields are not explicitly assigned in the suffix initialization loop; zero-initialize them deliberately without adding RNG draws.

Crypto covers only the first 2,498 bytes using the existing ten-byte key and chained XOR (`src/config.c:1051–1121`); appended scores are not covered by the four prefix checks. Validate exact length and all suffix fields separately. Our loader today reads 2,502 bytes then closes the file, so a 2000 save’s appended boards would be silently ignored; saving afterward truncates it to 2.1 length. The fork expects a complete suffix and aborts its fatal reads on a 2.1-length file. Separate namespaces are mandatory even though the prefix is structurally compatible.

`tyrian.cfg` remains a **28-byte DOS configuration layout**. The fork uses version byte 3 and keeps mouse bindings in textual `opentyrian.cfg`; our reader currently forces version 2 (`src/config.c:800`). User-approved shared presentation/controls mean that a shared modern config must not duplicate mouse/gamepad state into two conflicting variant configs. Variant legacy config and save files can remain under their own subdirectory.

## Current-build observation and loader failure checklist

A GCC-16 C99 warnings-as-errors build succeeded. Invoking the unmodified executable with `--data=<external-2000-directory> --regress-demo=1 --regress-out=<external-output>` returned **exit 1** and logged: “The Tyrian 2000 data files were found. OpenTyrian requires the Tyrian v2.0/v2.1 data files.” The header discriminator in `src/opentyr.c:1107–1112` fires before video/audio/full asset loading. Regression mode disables user files before the probe; no original user saves/configuration were opened for writing.

If that guard were simply removed, failures would include:

| Loader | Current mismatch | Consequence |
|---|---|---|
| `JE_loadItemDat`, `src/episodes.c:90` | 2.1 maxima and contiguous weapon/enemy loops | Debug count assertions; release desynchronizes every following item table |
| `JE_loadHelpText`, `src/helptext.c:196` | Wrong early string counts, widths, omitted tail | Wrong strings/section positions; debug bounds asserts |
| `JE_loadMainShapeTables`, `src/sprite.c:977` | Twelve banks, old generic capacity | Debug asserts; added bank unavailable and last bank mis-sized |
| `JE_loadPic`, `src/picload.c:51` / `loadPals`, `src/palette.c:42` | Thirteen pictures /23 palettes | Extra picture rejected, palette 24 unavailable |
| `JE_playCredits`, `src/mainint.c:2417` | Requests 131 records | EOF after 126 records |
| `loadSounds`, `src/nortsong.c:150` | 29 effects with 31-effect input | Bad last sample boundary, shifted voices, capacity mismatch |
| Episode script/event system, `src/tyrian2.c:2475,4325` | Missing ]T/]q and T2K event semantics | Wrong routing, ignored events, event 68 executes wrong rule |
| `loadSaves/saveSaves`, `src/config.c:888,989` | No 2000 suffix | New scores lost/truncated if namespaces mix |
| `tools/regress.sh:101` | 2.1-only consumed-data manifest | Correctly refuses 2000 data; never bypass this check |

Level-container parsing, encrypted-string decoding, compressed shape primitives, LDS decoding, resampling and demo decoding are reusable once counts/IDs/schema selection are explicit. These are static failure predictions beyond the observed early rejection; no experimental bypass was made.
