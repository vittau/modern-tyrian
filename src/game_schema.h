/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef GAME_SCHEMA_H
#define GAME_SCHEMA_H

#include "game_variant.h"

#include <stddef.h>
#include <stdint.h>

// Per-variant data schemas.  Everything the loaders need to know about how a
// variant's data files are laid out lives in the two tables of game_schema.c;
// the loaders read counts from the selected schema and never test the variant.
//
// Array capacities in the engine are the maximum over all variants (see the
// *_MAX macros in lvlmast.h, helptext.h, ...).  The schema holds the counts
// that are actually serialised, so Tyrian 2.1 reads exactly the same bytes in
// the same order as before.

// Inclusive range of IDs that are present in the stream.  IDs between two banks
// (the holes) have no bytes and are never read.
typedef struct
{
	uint16_t first, last;
} GameIdBank;

// The 77 bytes that follow the enemy records of every Tyrian 2000 item block
// are not read (data-formats.md, "Unconsumed trailer").  They are one
// enemy-record-sized remainder that has no ID of its own.
#define GAME_ITEM_TRAILER_2000 77

typedef struct GameDataSchema
{
	// Item data (tyrian.hdt, and the level file of episodes 4 and 5).  The seven
	// header words are the last ID of weapons, ports, generators, ships,
	// sidekicks, shields and the first enemy bank.
	uint16_t item_header[7];
	const GameIdBank *weapon_banks;
	size_t weapon_bank_count;
	uint16_t port_max, special_max, generator_max;
	uint16_t ship_max, sidekick_max, shield_max;
	const GameIdBank *enemy_banks;
	size_t enemy_bank_count;
	uint16_t item_trailer_bytes;  // documented, never read

	// Highest valid ID of each table (weapons and enemies: of the last bank).
	uint16_t weapon_max, enemy_max;

	// Shapes.
	uint16_t main_shape_banks;    // banks in tyrian.shp / tyrianc.shp
	uint16_t sprite_table_max;    // sprites in a generic table
	uint16_t ship_bank2_base;     // ship graphic IDs above this live in the added ship bank (0: none)
	const char *enemy_shape_files;
	size_t enemy_shape_file_count;
	uint16_t warning_lines;       // lines of level warning text

	// Pictures and palettes.
	uint16_t picture_count, palette_count;
	const uint8_t *picture_palette;

	// Sound effects, voices and their names.  Voice IDs follow the effects.
	uint16_t sfx_count, voice_count;
	const char (*sound_titles)[9];

	// Credits (tyrian.cdt) record count.
	uint16_t credits_lines;

	// Save file: the encrypted prefix is common; 2000 appends unencrypted
	// high-score boards (data-formats.md, "Save layout and namespaces").
	uint16_t save_suffix_timed_boards, save_suffix_main_boards, save_suffix_boards_per;

	// Level events whose rule differs in this variant and is not implemented yet
	// (Phase 3b).  They are skipped, with a one-time log line, instead of running
	// the 2.1 rule (event 68) or being reported as invalid.
	const uint8_t *deferred_events;
	size_t deferred_event_count;
} GameDataSchema;

// Semantic misc-text labels.  The HUD and menus ask for these instead of raw
// indices, so every variant maps them through its own string schema.
typedef enum
{
	GAME_LABEL_PLAYER_1,
	GAME_LABEL_PLAYER_2,
	GAME_LABEL_TIMER,
	GAME_LABEL_REAR_MODE_HINT,  // Tyrian 2000 preview hint
	GAME_LABEL_COUNT
} GameLabel;

// What each entry of the gameplay-mode menu does, in menu order.
typedef enum
{
	GAMEPLAY_FULL_GAME,
	GAMEPLAY_ARCADE,
	GAMEPLAY_TIMED_BATTLE,  // Tyrian 2000 only; the mode itself is Phase 4
	GAMEPLAY_ARCADE_2P,
	GAMEPLAY_NETWORK
} GameplayChoice;

// Menu topology.  The item screen (game_menu.c) is one state machine for every
// variant; what differs is how many rows each menu has and where each row of the
// options menus sits.  Rows are the menu's selection numbers: the first entry is
// row 2 (row 1 is the title), the same numbering as curSel[] and the help rows.
typedef struct GameOptionsRows
{
	uint8_t load, save;        // 0: the menu has no such row
	uint8_t joystick, keyboard, mouse;
	uint8_t music, sound;      // volume rows; their bars sit at y = 6 + 16 * row
	uint8_t done;              // the last row, back to the previous menu
} GameOptionsRows;

// Number of help-line rows per menu (see menuHelp in the fork: one help string
// number per entry, 0 for none).
#define GAME_MENU_HELP_ROWS 11
#define GAME_MENU_COUNT 15

typedef struct GameUiTables
{
	// Rows per menu (the loaders' string counts: title plus entries), the menu
	// that Esc goes back to (1-based, 0: quit request) and the height in pixels
	// of one mouse-selection row.
	uint8_t menu_choices[GAME_MENU_COUNT];
	uint8_t menu_esc[GAME_MENU_COUNT];
	uint8_t mouse_row_height[GAME_MENU_COUNT];

	// One help string number (mainMenuHelp, 1-based) per row of the simple menus.
	uint8_t menu_help[GAME_MENU_COUNT][GAME_MENU_HELP_ROWS];

	// Where the options menus keep their rows: the full options menu and the
	// limited one (no save/load) that network games use.
	GameOptionsRows options, limited_options;
	uint8_t mouse_menu;              // 1: the options menus lead to a mouse settings menu
	// What the left, right and middle mouse buttons do until the player changes
	// them (MouseAction values).  Tyrian 2.1 keeps its historical mapping.
	uint8_t default_mouse_actions[3];

	// Front "None" may be given power in the upgrade screen (Tyrian 2000).  The
	// rear port is never upgradeable without a weapon in either variant.
	bool front_none_takes_power;

	// The weapon preview shows the rear weapon mode as two lights beside it, and
	// alternates its power line with a mode hint for weapons that have two modes
	// (Tyrian 2000).  Purely presentation: 2.1 keeps its screens.
	bool rear_mode_preview;

	// The title screen's extra mark (a planet-shape sprite, 0 for none).  It
	// starts at (x, y_start) under the logo at y 62; while the logo rises by 2 per
	// step from y 60 the mark sinks by 1 per step from y_60 to its resting place.
	uint16_t title_mark_sprite;
	int16_t title_mark_x, title_mark_y_start, title_mark_y_60;

	// Episode rows share these positions with their mouse hit boxes.
	uint8_t episode_row_y, episode_row_step;
} GameUiTables;

// The mark's y for a logo at logo_y (62 is the frame before the logo rises).
static inline int gameTitleMarkY(const GameUiTables *ui, int logo_y)
{
	return logo_y >= 62 ? ui->title_mark_y_start : ui->title_mark_y_60 + (60 - logo_y) / 2;
}

// Sections of tyrian.hdt: encrypted Pascal strings, each section is a marker
// record, its strings and a marker record.  Counts are the serialised counts.
typedef struct GameStringSchema
{
	uint16_t misc_text, misc_text_b, main_menu_help;
	uint16_t network_text, super_ships, special_name, ship_info;
	uint16_t gameplay_name;                 // header plus one string per choice
	const uint8_t *gameplay_choices;        // GameplayChoice of each menu entry
	uint8_t menu_entries[16];         // strings of menuInt[n], by menu number

	// Sections that only the 2000 HDT has, read after menu 14.  The unused setup
	// blocks are skipped structurally (each is a marker, N strings and a marker).
	bool has_tail;
	uint16_t timed_battle_name;
	const uint8_t *setup_skip;
	size_t setup_skip_count;
	uint16_t licensing_info, high_score_names, team_names, ordering_info, super_tyrian_text;

	// Where the default high-score names come from: false uses the names built
	// into the engine, true the names read from the HDT, which must therefore be
	// loaded before default scores are generated.
	bool default_names_from_data;

	// The 2000 title menu lists Demo and Quit one entry earlier than 2.1 does
	// (entry 4 is "Demo", where 2.1 has the ordering-info entry that the title
	// screen replaces with Setup).  When set, entries 4 and 5 move to 5 and 6.
	bool title_menu_makes_room;

	// Semantic label -> misc text index (zero based).
	uint16_t label_misc_text[GAME_LABEL_COUNT];

	// Menu topology and screen policy of the variant.
	const GameUiTables *ui;
} GameStringSchema;

extern const GameDataSchema gameDataSchema21, gameDataSchema2000;
extern const GameStringSchema gameStringSchema21, gameStringSchema2000;

// The schemas of the selected variant.
const GameDataSchema *gameSchema(void);
const GameStringSchema *gameStrings(void);
const GameUiTables *gameUi(void);

// Sound IDs are 1-based slots in soundSamples[].  Effects come first, so the
// voices (index 0..8, the order of voices.snd) shift with the effect count.
static inline unsigned int gameVoiceSound(unsigned int voice)
{
	return gameSchema()->sfx_count + 1 + voice;
}

// True when the level event type is deferred to Phase 3b in this variant; the
// first time each type is met it is logged.
bool gameEventDeferred(unsigned int eventType);

// Sound slots in use: effects plus voices.
static inline unsigned int gameSoundCount(void)
{
	return gameSchema()->sfx_count + gameSchema()->voice_count;
}

// True when id lies in one of the banks.
static inline bool gameIdInBanks(const GameIdBank *banks, size_t count, unsigned int id)
{
	for (size_t i = 0; i < count; ++i)
	{
		if (id >= banks[i].first && id <= banks[i].last)
			return true;
	}
	return false;
}

static inline bool gameWeaponValid(unsigned int id)
{
	const GameDataSchema *schema = gameSchema();
	return gameIdInBanks(schema->weapon_banks, schema->weapon_bank_count, id);
}

static inline bool gameEnemyValid(unsigned int id)
{
	const GameDataSchema *schema = gameSchema();
	return gameIdInBanks(schema->enemy_banks, schema->enemy_bank_count, id);
}

#endif // GAME_SCHEMA_H
