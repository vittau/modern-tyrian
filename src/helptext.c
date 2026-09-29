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
#include "helptext.h"

#include "file.h"
#include "fonthand.h"
#include "game_data.h"
#include "game_schema.h"
#include "logging.h"
#include "menus.h"
#include "opentyr.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

const JE_byte menuHelp[MENU_MAX][11] = /* [1..maxmenu, 1..11] */
{
	{  1, 34,  2,  3,  4,  5,                  0, 0, 0, 0, 0 },
	{  6,  7,  8,  9, 10, 11, 11, 12,                0, 0, 0 },
	{ 13, 14, 15, 15, 16, 17, 12,                 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{  4, 30, 30,  3,  5,                   0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 16, 17, 15, 15, 12,                   0, 0, 0, 0, 0, 0 },
	{ 31, 31, 31, 31, 32, 12,                  0, 0, 0, 0, 0 },
	{  4, 34,  3,  5,                    0, 0, 0, 0, 0, 0, 0 },
	{ 35, 35, 35, 36, 12,                   0, 0, 0, 0, 0, 0 }  // Tyrian 2000 only (menu 15)
};

char helpTxt[39][231];                                                   /* [1..39] of string [230] */
char pName[21][16];                                                      /* [1..21] of string [15] */
char miscText[HELPTEXT_MISCTEXT_COUNT][42];                              /* [1..68] of string [41] */
char miscTextB[HELPTEXT_MISCTEXTB_COUNT][HELPTEXT_MISCTEXTB_SIZE];       /* [1..5] of string [10] */
char keyName[8][18];                                                     /* [1..8] of string [17] */
char menuText[7][HELPTEXT_MENUTEXT_SIZE];                                /* [1..7] of string [20] */
char outputs[9][31];                                                     /* [1..9] of string [30] */
char topicName[6][21];                                                   /* [1..6] of string [20] */
char mainMenuHelp[HELPTEXT_MAINMENUHELP_COUNT][66];                      /* [1..34] of string [65] */
char inGameText[6][21];                                                  /* [1..6] of string [20] */
char detailLevel[6][13];                                                 /* [1..6] of string [12] */
char gameSpeedText[5][13];                                               /* [1..5] of string [12] */
char inputDevices[3][13];                                                /* [1..3] of string [12] */
char networkText[HELPTEXT_NETWORKTEXT_COUNT][HELPTEXT_NETWORKTEXT_SIZE]; /* [1..4] of string [20] */
char difficultyNameB[11][21];                                            /* [0..9] of string [20] */
char joyButtonNames[5][21];                                              /* [1..5] of string [20] */
char superShips[HELPTEXT_SUPERSHIPS_COUNT][26];                          /* [0..10] of string [25] */
char specialName[HELPTEXT_SPECIALNAME_COUNT][10];                        /* [1..9] of string [9] */
char destructHelp[25][22];                                               /* [1..25] of string [21] */
char weaponNames[17][17];                                                /* [1..17] of string [16] */
char destructModeName[DESTRUCT_MODES][13];                               /* [1..destructmodes] of string [12] */
char shipInfo[HELPTEXT_SHIPINFO_COUNT][2][256];                          /* [1..13, 1..2] of string */
char timedBattleName[HELPTEXT_TIMED_BATTLE_COUNT][23];                   /* Tyrian 2000 */
char licensingInfo[HELPTEXT_LICENSING_COUNT][46];                        /* Tyrian 2000 */
char hdtHighScoreNames[HELPTEXT_HIGH_SCORE_NAMES_COUNT][23];             /* Tyrian 2000: default names */
char hdtTeamNames[HELPTEXT_TEAM_NAMES_COUNT][25];                        /* Tyrian 2000: default names */
char orderingInfo[HELPTEXT_ORDERING_COUNT][32];                          /* Tyrian 2000 */
char superTyrianText[HELPTEXT_SUPER_TYRIAN_COUNT][64];                   /* Tyrian 2000 */
char menuInt[MENU_MAX+1][11][18];                                        /* [0..15, 1..11] of string [17] */

static void decrypt_string(char *s, size_t len)
{
	static const unsigned char crypt_key[] = { 204, 129, 63, 255, 71, 19, 25, 62, 1, 99 };

	if (len == 0)
		return;

	for (size_t i = len - 1; ; --i)
	{
		s[i] ^= crypt_key[i % sizeof(crypt_key)];
		if (i == 0)
			break;
		s[i] ^= s[i - 1];
	}
}

void readEncryptedString(File *file, char *dst, size_t size)
{
	Uint8 buffer[255];

	Uint8 len = fileReadU8(file);
	fileReadExactly(file, buffer, len);

	if (size == 0)
		return;

	decrypt_string((char *)buffer, len);

	assert(len < size);
	len = MIN(len, size - 1);

	memcpy(dst, buffer, len);
	dst[len] = '\0';
}

void JE_helpBox(SDL_Surface *screen,  int x, int y, const char *message, JE_byte boxWidth, JE_byte verticalHeight, JE_byte color, JE_byte brightness, JE_byte shadeType)
{
	JE_byte startpos, endpos, pos;
	JE_boolean endstring;

	char substring[256];

	if (strlen(message) == 0)
	{
		return;
	}

	pos = 1;
	endpos = 0;
	endstring = false;

	do
	{
		startpos = endpos + 1;

		do
		{
			endpos = pos;
			do
			{
				pos++;
				if (pos == strlen(message))
				{
					endstring = true;
					if ((unsigned)(pos - startpos) < boxWidth)
					{
						endpos = pos + 1;
					}
				}

			} while (!(message[pos-1] == ' ' || endstring));

		} while (!((unsigned)(pos - startpos) > boxWidth || endstring));

		SDL_strlcpy(substring, message + startpos - 1, MIN((size_t)(endpos - startpos + 1), sizeof(substring)));
		JE_textShade(screen, x, y, substring, color, brightness, shadeType);

		y += verticalHeight;

	} while (!endstring);

	if (endpos != pos + 1)
	{
		JE_textShade(screen, x, y, message + endpos, color, brightness, shadeType);
	}
}

void JE_HBox(SDL_Surface *screen, int x, int y, JE_byte messageNum, JE_byte boxWidth, JE_byte verticalHeight, JE_byte color, JE_byte brightness)
{
	JE_helpBox(screen, x, y, helpTxt[messageNum-1], boxWidth, verticalHeight, color, brightness, FULL_SHADE);
}

void JE_loadHelpText(void)
{
	const GameStringSchema *strings = gameStrings();
	const uint8_t *menuInt_entries = strings->menu_entries;

	const char *filename = "tyrian.hdt";

	File file = dataFileOpen(filename, "rb");
	if (file.error)
	{
		logFatal("Failed to open file '%s': %s", filename, fileGetError(&file));
		exit(EXIT_FAILURE);
	}

	(void)fileReadU32(&file);  // Episode 1-3 item data position

	/*Online Help*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(helpTxt); ++i)
		readEncryptedString(&file, helpTxt[i], sizeof helpTxt[i]);
	readEncryptedString(&file, NULL, 0);

	/*Planet names*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(pName); ++i)
		readEncryptedString(&file, pName[i], sizeof pName[i]);
	readEncryptedString(&file, NULL, 0);

	/*Miscellaneous text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < strings->misc_text; ++i)
		readEncryptedString(&file, miscText[i], sizeof miscText[i]);
	readEncryptedString(&file, NULL, 0);

	/*Little Miscellaneous text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < strings->misc_text_b; ++i)
		readEncryptedString(&file, miscTextB[i], sizeof miscTextB[i]);
	readEncryptedString(&file, NULL, 0);

	/*Key names*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[6]; ++i)
		readEncryptedString(&file, menuInt[6][i], sizeof menuInt[6][i]);
	readEncryptedString(&file, NULL, 0);

	/*Main Menu*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(menuText); ++i)
		readEncryptedString(&file, menuText[i], sizeof menuText[i]);
	readEncryptedString(&file, NULL, 0);

	if (strings->title_menu_makes_room)
	{
		// The title screen puts "Setup" in entry 4 and keeps the rest of the
		// entries in the order of the actions (Demo, Quit).
		memcpy(menuText[6], menuText[5], sizeof menuText[6]);
		memcpy(menuText[5], menuText[4], sizeof menuText[5]);
	}

	/*Event text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(outputs); ++i)
		readEncryptedString(&file, outputs[i], sizeof outputs[i]);
	readEncryptedString(&file, NULL, 0);

	/*Help topics*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(topicName); ++i)
		readEncryptedString(&file, topicName[i], sizeof topicName[i]);
	readEncryptedString(&file, NULL, 0);

	/*Main Menu Help*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < strings->main_menu_help; ++i)
		readEncryptedString(&file, mainMenuHelp[i], sizeof mainMenuHelp[i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 1 - Main*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[1]; ++i)
		readEncryptedString(&file, menuInt[1][i], sizeof menuInt[1][i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 2 - Items*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[2]; ++i)
		readEncryptedString(&file, menuInt[2][i], sizeof menuInt[2][i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 3 - Options*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[3]; ++i)
		readEncryptedString(&file, menuInt[3][i], sizeof menuInt[3][i]);
	readEncryptedString(&file, NULL, 0);

	/*InGame Menu*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(inGameText); ++i)
		readEncryptedString(&file, inGameText[i], sizeof inGameText[i]);
	readEncryptedString(&file, NULL, 0);

	/*Detail Level*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(detailLevel); ++i)
		readEncryptedString(&file, detailLevel[i], sizeof detailLevel[i]);
	readEncryptedString(&file, NULL, 0);

	/*Game speed text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(gameSpeedText); ++i)
		readEncryptedString(&file, gameSpeedText[i], sizeof gameSpeedText[i]);
	readEncryptedString(&file, NULL, 0);

	// episode names
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(episode_name); ++i)
		readEncryptedString(&file, episode_name[i], sizeof episode_name[i]);
	readEncryptedString(&file, NULL, 0);

	// difficulty names
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(difficulty_name); ++i)
		readEncryptedString(&file, difficulty_name[i], sizeof difficulty_name[i]);
	readEncryptedString(&file, NULL, 0);

	// gameplay mode names
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < strings->gameplay_name; ++i)
		readEncryptedString(&file, gameplay_name[i], sizeof gameplay_name[i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 10 - 2Player Main*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[10]; ++i)
		readEncryptedString(&file, menuInt[10][i], sizeof menuInt[10][i]);
	readEncryptedString(&file, NULL, 0);

	/*Input Devices*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(inputDevices); ++i)
		readEncryptedString(&file, inputDevices[i], sizeof inputDevices[i]);
	readEncryptedString(&file, NULL, 0);

	/*Network text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < strings->network_text; ++i)
		readEncryptedString(&file, networkText[i], sizeof networkText[i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 11 - 2Player Network*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[11]; ++i)
		readEncryptedString(&file, menuInt[11][i], sizeof menuInt[11][i]);
	readEncryptedString(&file, NULL, 0);

	/*HighScore Difficulty Names*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(difficultyNameB); ++i)
		readEncryptedString(&file, difficultyNameB[i], sizeof difficultyNameB[i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 12 - Network Options*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[12]; ++i)
		readEncryptedString(&file, menuInt[12][i], sizeof menuInt[12][i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 13 - Joystick*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[13]; ++i)
		readEncryptedString(&file, menuInt[13][i], sizeof menuInt[13][i]);
	readEncryptedString(&file, NULL, 0);

	/*Joystick Button Assignments*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(joyButtonNames); ++i)
		readEncryptedString(&file, joyButtonNames[i], sizeof joyButtonNames[i]);
	readEncryptedString(&file, NULL, 0);

	/*SuperShips - For Super Arcade Mode*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < strings->super_ships; ++i)
		readEncryptedString(&file, superShips[i], sizeof superShips[i]);
	readEncryptedString(&file, NULL, 0);

	/*SuperShips - For Super Arcade Mode*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < strings->special_name; ++i)
		readEncryptedString(&file, specialName[i], sizeof specialName[i]);
	readEncryptedString(&file, NULL, 0);

	/*Secret DESTRUCT game*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(destructHelp); ++i)
		readEncryptedString(&file, destructHelp[i], sizeof destructHelp[i]);
	readEncryptedString(&file, NULL, 0);

	/*Secret DESTRUCT weapons*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(weaponNames); ++i)
		readEncryptedString(&file, weaponNames[i], sizeof weaponNames[i]);
	readEncryptedString(&file, NULL, 0);

	/*Secret DESTRUCT modes*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(destructModeName); ++i)
		readEncryptedString(&file, destructModeName[i], sizeof destructModeName[i]);
	readEncryptedString(&file, NULL, 0);

	/*NEW: Ship Info*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < strings->ship_info; ++i)
	{
		readEncryptedString(&file, shipInfo[i][0], sizeof shipInfo[i][0]);
		readEncryptedString(&file, shipInfo[i][1], sizeof shipInfo[i][1]);
	}
	readEncryptedString(&file, NULL, 0);

	/*Menu 14 - Super Tyrian*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[14]; ++i)
		readEncryptedString(&file, menuInt[14][i], sizeof menuInt[14][i]);

	if (strings->has_tail)
	{
		// The Tyrian 2000 HDT continues after menu 14 (the 2.1 file ends there).
		readEncryptedString(&file, NULL, 0);

		/*Timed Battle names*/
		readEncryptedString(&file, NULL, 0);
		for (size_t i = 0; i < strings->timed_battle_name; ++i)
			readEncryptedString(&file, timedBattleName[i], sizeof timedBattleName[i]);
		readEncryptedString(&file, NULL, 0);

		/*Setup blocks: not used by this engine, skipped structurally*/
		for (size_t block = 0; block < strings->setup_skip_count; ++block)
		{
			readEncryptedString(&file, NULL, 0);
			for (size_t i = 0; i < strings->setup_skip[block]; ++i)
				readEncryptedString(&file, NULL, 0);
			readEncryptedString(&file, NULL, 0);
		}

		/*Menu 15 - Mouse settings*/
		readEncryptedString(&file, NULL, 0);
		for (size_t i = 0; i < menuInt_entries[15]; ++i)
			readEncryptedString(&file, menuInt[15][i], sizeof menuInt[15][i]);
		readEncryptedString(&file, NULL, 0);

		/*Licensing info*/
		readEncryptedString(&file, NULL, 0);
		for (size_t i = 0; i < strings->licensing_info; ++i)
			readEncryptedString(&file, licensingInfo[i], sizeof licensingInfo[i]);
		readEncryptedString(&file, NULL, 0);

		/*Default high score names*/
		readEncryptedString(&file, NULL, 0);
		for (size_t i = 0; i < strings->high_score_names; ++i)
			readEncryptedString(&file, hdtHighScoreNames[i], sizeof hdtHighScoreNames[i]);
		readEncryptedString(&file, NULL, 0);

		/*Default team names*/
		readEncryptedString(&file, NULL, 0);
		for (size_t i = 0; i < strings->team_names; ++i)
		{
			// One name is longer than its 24-character field: keep the field,
			// truncating (as the fork does), so it also fits a score entry.
			char full[256];
			readEncryptedString(&file, full, sizeof full);
			SDL_strlcpy(hdtTeamNames[i], full, sizeof hdtTeamNames[i]);
		}
		readEncryptedString(&file, NULL, 0);

		/*Ordering info*/
		readEncryptedString(&file, NULL, 0);
		for (size_t i = 0; i < strings->ordering_info; ++i)
			readEncryptedString(&file, orderingInfo[i], sizeof orderingInfo[i]);
		readEncryptedString(&file, NULL, 0);

		/*Super Tyrian text*/
		readEncryptedString(&file, NULL, 0);
		for (size_t i = 0; i < strings->super_tyrian_text; ++i)
			readEncryptedString(&file, superTyrianText[i], sizeof superTyrianText[i]);
	}

	if (file.error)
	{
		logFatal("Failed to read from file '%s': %s", filename, fileGetError(&file));
		exit(EXIT_FAILURE);
	}

	fileClose(&file);
}

const char *helpLabelText(GameLabel label)
{
	return miscText[gameStrings()->label_misc_text[label]];
}

void helpTextEnsureLoaded(void)
{
	static bool loaded = false;
	if (loaded)
		return;

	if (!gameDataPrepare())
		exit(EXIT_FAILURE);
	JE_loadHelpText();
	loaded = true;
}
