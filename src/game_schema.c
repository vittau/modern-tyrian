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
#include "game_schema.h"

#include "config.h"
#include "sndmast.h"

#include <stddef.h>

// Tyrian 2.1: exactly the counts and order the loaders used before schemas.
// Tyrian 2000 values follow data-formats.md; the ported logic is that of
// KScl/opentyrian2000 (GPL-2.0), reimplemented on this tree's loaders.

static const GameIdBank weaponBanks21[] = { { 0, 780 } };
static const GameIdBank enemyBanks21[] = { { 0, 850 } };

// The fork reads weapons 0..818 and 1000..1818, and enemies 0..850 and
// 1001..1850.  The IDs in between have no bytes in the stream.
static const GameIdBank weaponBanks2000[] = { { 0, 818 }, { 1000, 1818 } };
static const GameIdBank enemyBanks2000[] = { { 0, 850 }, { 1001, 1850 } };

// Enemy shape files: the character of newsh?.shp, by 1-based table number.
static const char enemyShapeFiles21[34] =
{
	'2', '4', '7', '8', 'A', 'B', 'C', 'D', 'E', 'F',
	'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', '5', '#', 'V', '0', '@',  // [25] should be '&' rather than '5'
	'3', '^', '5', '9'
};
static const char enemyShapeFiles2000[36] =
{
	'2', '4', '7', '8', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', '5', '#', 'V', '0', '@', '3', '^', '5', '9', '\'', '%'
};

// Palette of each picture (tyrian.pic).  2000 adds picture 14 with palette 23.
static const uint8_t picturePalette21[13] = { 0, 7, 5, 8, 10, 5, 18, 19, 19, 20, 21, 22, 5 };
static const uint8_t picturePalette2000[14] = { 0, 7, 5, 8, 10, 5, 18, 19, 19, 20, 21, 22, 5, 23 };

const GameDataSchema gameDataSchema21 =
{
	{ 780, 42, 6, 13, 30, 10, 850 },
	weaponBanks21, 1,
	42, 46, 6, 13, 30, 10,
	enemyBanks21, 1,
	0,
	780, 850,
	12, 151, 0, enemyShapeFiles21, sizeof enemyShapeFiles21, 10,
	13, 23, picturePalette21,
	29, VOICE_COUNT, soundTitle21,
	131,
	0, 0, 0
};

const GameDataSchema gameDataSchema2000 =
{
	{ 818, 60, 6, 18, 37, 11, 850 },
	weaponBanks2000, 2,
	60, 54, 6, 18, 37, 11,
	enemyBanks2000, 2,
	GAME_ITEM_TRAILER_2000,
	1818, 1850,
	13, 152, 500, enemyShapeFiles2000, sizeof enemyShapeFiles2000, 12,
	14, 24, picturePalette2000,
	31, VOICE_COUNT, soundTitle2000,
	126,
	10, 10, 3
};

// Unused setup blocks of the 2000 HDT: counts of strings, skipped structurally.
static const uint8_t setupSkip2000[] = { 10, 5, 4, 4, 5, 7, 7, 21, 3, 3 };

static const uint8_t gameplayChoices21[] =
{
	GAMEPLAY_FULL_GAME, GAMEPLAY_ARCADE, GAMEPLAY_ARCADE_2P, GAMEPLAY_NETWORK
};
static const uint8_t gameplayChoices2000[] =
{
	GAMEPLAY_FULL_GAME, GAMEPLAY_ARCADE, GAMEPLAY_TIMED_BATTLE, GAMEPLAY_ARCADE_2P, GAMEPLAY_NETWORK
};


// UI policies adapted from KScl/opentyrian2000 aad5aca (GPL-2.0), including
// 26f40e7's mouse menu and rear-mode preview.  Keep our rebuilt joystick rows.
// Menu topology.  Row numbers are selection numbers (the first entry is row 2).
// Tyrian 2000 adds a Mouse row to both options menus and a mouse settings menu
// (menu 14), so its options menu has 9 rows, its limited options menu 7 and the
// mouse menu 6.  The fork keeps 6 rows for the limited menu, which hides "Done"
// and leaves the new row and the volume rows at each other's numbers; here every
// row is drawn and every label goes to its own action.
static const GameUiTables gameUiTables21 =
{
	// menu_choices
	{ 7, 9, 8, 0, 0, 11, SAVE_FILES_NUM / 2 + 2, 0, 0, 6, 4, 6, 7, 5, 0 },
	// menu_esc
	{ 0, 1, 1, 1, 2, 3, 3, 1, 8, 0, 0, 11, 3, 0, 0 },
	// mouse_row_height
	{ 16, 16, 16, 16, 26, 12, 11, 28, 0, 16, 16, 16, 8, 16, 0 },
	// menu_help
	{
		{  1, 34,  2,  3,  4,  5,  0, 0, 0, 0, 0 },
		{  6,  7,  8,  9, 10, 11, 11, 12, 0, 0, 0 },
		{ 13, 14, 15, 15, 16, 17, 12,  0, 0, 0, 0 },
		{ 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 },
		{  4, 30, 30,  3,  5,  0, 0, 0, 0, 0, 0 },
		{ 0 },
		{ 16, 17, 15, 15, 12,  0, 0, 0, 0, 0, 0 },
		{ 31, 31, 31, 31, 32, 12, 0, 0, 0, 0, 0 },
		{  4, 34,  3,  5,  0, 0, 0, 0, 0, 0, 0 },
		{ 0 }
	},
	// options, limited_options, mouse_menu
	{ 2, 3, 6, 7, 0, 4, 5, 8 },
	{ 0, 0, 2, 3, 0, 4, 5, 6 },
	0, { 0, 1, 2 },
	false, false,
	0, 0, 0, 0,
	50, 30
};

static const GameUiTables gameUiTables2000 =
{
	{ 7, 9, 9, 0, 0, 11, SAVE_FILES_NUM / 2 + 2, 0, 0, 6, 4, 7, 7, 5, 6 },
	{ 0, 1, 1, 1, 2, 3, 3, 1, 8, 0, 0, 11, 3, 0, 3 },
	{ 16, 16, 16, 16, 26, 12, 11, 28, 0, 16, 16, 16, 8, 16, 24 },
	{
		{  1, 34,  2,  3,  4,  5,  0, 0, 0, 0, 0 },
		{  6,  7,  8,  9, 10, 11, 11, 12, 0, 0, 0 },
		{ 13, 14, 15, 15, 16, 17, 35, 12, 0, 0, 0 },
		{ 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 },
		{  4, 30, 30,  3,  5,  0, 0, 0, 0, 0, 0 },
		{  4, 37, 12,  0, 0, 0, 0, 0, 0, 0, 0 },
		{ 16, 17, 35, 15, 15, 12, 0, 0, 0, 0, 0 },
		{ 31, 31, 31, 31, 32, 12, 0, 0, 0, 0, 0 },
		{  4, 34,  3,  5,  0, 0, 0, 0, 0, 0, 0 },
		{ 35, 35, 35, 36, 12,  0, 0, 0, 0, 0, 0 }
	},
	{ 2, 3, 6, 7, 8, 4, 5, 9 },
	{ 0, 0, 2, 3, 4, 5, 6, 7 },
	1, { 0, 3, 4 },  // fire, both sidekicks, rear mode
	true, true,
	// The 2000 mark under the logo's right half (planet shape 151).
	151, 155, 41, 45,
	50, 24
};

// Menu strings by menu number (menuInt[n]).  Menus with no strings, or that
// are read elsewhere in the HDT order, are zero.
const GameStringSchema gameStringSchema21 =
{
	68, 5, 34,
	4, 11, 9, 13,
	5, gameplayChoices21,
	{ 0, 7, 9, 8, 0, 0, 11, 0, 0, 0, 6, 4, 6, 7, 5, 0 },
	false, 0, NULL, 0, 0, 0, 0, 0, 0,
	false, false,
	{ 48, 49, 66, 0 },
	&gameUiTables21
};

// The 2000 HDT appends records to the misc text, so the labels the HUD knows
// keep their positions.  Menu 15 (mouse settings) has strings only in 2000.
const GameStringSchema gameStringSchema2000 =
{
	72, 8, 37,
	5, 13, 11, 20,
	6, gameplayChoices2000,
	{ 0, 7, 9, 9, 0, 0, 11, 0, 0, 0, 6, 4, 7, 7, 5, 6 },
	true, 4, setupSkip2000, sizeof setupSkip2000, 3, 39, 10, 6, 6,
	true, true,
	{ 48, 49, 66, 70 },
	&gameUiTables2000
};

const GameDataSchema *gameSchema(void)
{
	return gameVariantCurrent()->data_schema;
}

const GameStringSchema *gameStrings(void)
{
	return gameVariantCurrent()->string_schema;
}

const GameUiTables *gameUi(void)
{
	return gameStrings()->ui;
}
