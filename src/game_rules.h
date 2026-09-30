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
#ifndef GAME_RULES_H
#define GAME_RULES_H

#include "game_variant.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Per-variant gameplay rules.  game_schema.h says how a variant's data files are
// laid out; this says how the game behaves with them.  Every rule that differs
// between Tyrian 2.1 and Tyrian 2000 is a field or a table of the two rule sets
// in game_rules.c, and the engine asks for it here instead of testing the
// variant.  The 2.1 set reproduces the historical behaviour exactly.
//
// The Tyrian 2000 rules are ported from the OpenTyrian2000 fork (KScl,
// aad5aca, GPL-2.0); each field names the fork commit it comes from.  Several
// are the fork's approximations of the DOS game (see docs/t2000/fork-diff.md).

// --- Level events -----------------------------------------------------------
//
// The event system switches on an "event case": the event type, unless the
// variant re-maps that type.  Types the map does not mention keep their number,
// so 2.1 (an empty map) is unchanged and a type with no rule is still reported
// as invalid.  The actions below are numbered past any real type (0..255).
typedef enum
{
	GAME_EVENT_SET_ENEMY_LAUNCH = 0x100,   // 58: set the launch type of linked enemies
	GAME_EVENT_REPLACE_ENEMY,              // 59, 68: replace linked enemies with another type
	GAME_EVENT_BATTLE_TIMER,               // 84: level timer, Timed Battle only
	GAME_EVENT_BATTLE_ENEMY_DEATH          // 85: change death/drop type, Timed Battle only
} GameEventAction;

typedef struct
{
	uint8_t type;      // type in the level file
	uint16_t action;   // event case to run: a GameEventAction or another event type
} GameEventRule;

// --- Arcade and Super Tyrian ships ------------------------------------------

// Arcade ships are numbered from 1 (the value of superArcadeMode) in the order
// of the tables below.  Two more values are codes typed at the title screen.
#define ARCADE_SHIPS_MAX 9

typedef struct
{
	uint8_t ship_count;                       // arcade ships
	uint8_t destruct_code, engage_code;       // 1-based specialName entries: Destruct, Super Tyrian
	// superArcadeMode after a Super Tyrian win on Zinglon difficulty: the
	// end-of-game screen then shows the code that this state leads to.
	uint8_t super_tyrian_win_state;
	const uint8_t *next_ship;                 // [ship_count + 2] state -> ship the end screen offers
	const uint16_t *special, *special_b;      // [ship_count] special weapon and its alternate
	const uint8_t *ship;                      // [ship_count] ship item
	const uint16_t (*weapon)[5];              // [ship_count] R, Bl, Bk, G, P pick-ups

	// Super Tyrian, entered with the engage code (fork c0782f0).  The voice
	// numbers are VOICE_* (sndmast.h).  The intro text is either built in or the
	// tyrian.hdt section that the variant has; the 2000 game also lets the player
	// pick the starting episode, and backing out returns to the title.
	uint8_t engage_voice, super_tyrian_voice;
	bool super_tyrian_text_from_data;
	bool super_tyrian_pick_episode;
} GameArcadeRules;

// --- Timed Battle -----------------------------------------------------------

// The Tyrian 2000 mode: one player, an arcade ship, a level timer that decides
// when the battle ends, and a score that is only cash (fork 832f2d0, 7a3ff18,
// 85a5a22, 3e395df, c954184).  The battles are picked from a menu and each one
// belongs to an episode; the episode script then jumps to the battle's section
// (]T) and ends with a high-score check (]q).
#define TIMED_BATTLES_MAX 3

typedef struct
{
	uint8_t battle_count;                    // battles on the menu (boards 0..battle_count-1)
	uint8_t battle_episode[TIMED_BATTLES_MAX]; // episode each battle is played from
	uint8_t score_picture;                   // backdrop of the name entry (tyrian.pic)
	uint16_t time_bonus;                     // cash per full tenth of a second left on the timer
	uint16_t life_bonus;                     // cash per life left
} GameTimedBattleRules;

// --- The rule set -----------------------------------------------------------

typedef struct GameRules
{
	// Level events (fork e376530, 62f58dc, 10a5752, 832f2d0).
	const GameEventRule *events;
	size_t event_count;

	// Enemy launch decoding (fork e376530): enemy IDs above this hold the whole
	// value in elaunchtype; below it the type is elaunchtype % 1000 and the
	// special is elaunchtype / 1000.  0xFFFF: every ID uses the split encoding.
	uint16_t enemy_full_launch_above;

	// An event enemy whose X is spawn_random_x_sentinel gets a random X in
	// [spawn_random_x_min, spawn_random_x_min + spawn_random_x_span) (fork
	// c0782f0).  The choice is written back to the event.  Disabled: false.
	bool spawn_random_x;
	int16_t spawn_random_x_sentinel;
	uint16_t spawn_random_x_min, spawn_random_x_span;

	// Twiddles (fork e1b86fc): per ship, the (up to three) combos of the arcade
	// combo language that ship can perform.  Rows past the end have none.
	const uint8_t (*ship_combos)[3];
	size_t ship_combo_count;

	const GameArcadeRules *arcade;

	// Sidekicks that charge only fire on their own button, not on main fire, when
	// they have unlimited ammo (fork dfe1050).
	bool charging_sidekick_own_fire;

	// Explosion table (fork c0782f0): types 0..explosion_count-1 exist.  A trail
	// ID other than 98 that behaves like 98 (Flying Punch, fork 0bfca44); -1 for
	// none.  With trail_first_tile_only that trail exists only on the first tile
	// of a multi-shot volley.
	uint8_t explosion_count;
	int16_t alt_smoke_trail;
	bool trail_first_tile_only;

	// Starting cash of a full game begun with a smuggled arcade ship, by episode.
	const uint32_t *initial_cash;
	// Hazudra Fodder completion may check the final episode's main boards.
	bool final_episode_score;

	// Timed Battle; NULL where the variant does not have it.
	const GameTimedBattleRules *timed_battle;

	// The level timer counts down in hundredths and shows whole tenths.  2.1
	// prints a rounded float; Tyrian 2000 truncates (fork 83812e0).
	bool timer_truncated_tenths;
} GameRules;

extern const GameRules gameRules21, gameRules2000;

// The rules of the selected variant.
const GameRules *gameRules(void);

// The event case for a type read from a level file.
unsigned int gameEventCase(unsigned int eventType);

// Splits the launch type of enemy record enemyId into a launch type and special.
void gameEnemyLaunch(unsigned int enemyId, uint16_t elaunchtype, uint16_t *type, uint8_t *special);

// The combos of a ship (three entries, zero-filled past the last).
const uint8_t *gameShipCombos(unsigned int ship);

// Whether the main fire button fires a sidekick with unlimited ammo whose charge
// stages number pwr (0: it does not charge).
bool gameSidekickMainFire(unsigned int pwr);

// Formats the level timer (a countdown in hundredths of a second) for the HUD.
void gameFormatLevelTimer(char *buffer, size_t size, int countdown);

// The trail a player shot leaves: the weapon's trail, or 255 (none) where the
// variant limits it to the first tile of a multi-shot volley.  multiPos is the
// 1-based position of the shot in the volley.
uint8_t gameShotTrail(uint8_t weaponTrail, unsigned int multiPos);

// True for a trail or explosion ID that draws the white smoke trail (98, or the
// variant's alternate).
bool gameIsSmokeTrail(int trail);

#endif // GAME_RULES_H
