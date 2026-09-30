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
#include "highscores.h"

#include "config.h"
#include "game_rules.h"
#include "game_schema.h"

#include <assert.h>
#include <string.h>

// Tyrian 2.1 keeps the boards in the save slots: six per episode, three for
// one player then three for two, for the first three episodes.  Tyrian 2000
// appends ten main-game boards (five episodes, one and two players) to the save
// file after ten Timed Battle boards (data-formats.md, "Save layout").  A
// variant that has the suffix boards uses them.
#define SAVE_SLOT_EPISODES 3

static bool usesVariantBoards(void)
{
	return gameSchema()->save_suffix_main_boards > 0;
}

// A board number indexes the variant table directly: the Timed Battle boards
// come first, then the main boards (highScoreBoard() and highScoreTimedBoard()
// return numbers of that kind).
static VariantHighScore *variantEntry(unsigned int board, unsigned int rank)
{
	assert(board < VARIANT_SCORE_BOARDS && rank < HIGH_SCORE_ENTRIES);
	return &variantHighScores[board][rank];
}

static JE_SaveFileType *slotEntry(unsigned int board, unsigned int rank)
{
	const unsigned int index = board * HIGH_SCORE_ENTRIES + rank;

	assert(index < SAVE_SLOT_EPISODES * 6 && rank < HIGH_SCORE_ENTRIES);
	return &saveFiles[index];
}

// Names are at most 28 characters when typed; a longer one is cut, never overrun.
static void copyName(char *to, size_t size, const char *name)
{
	strncpy(to, name, size - 1);
	to[size - 1] = '\0';
}

unsigned int highScoreEpisodes(void)
{
	return usesVariantBoards() ? gameSchema()->save_suffix_main_boards / 2 : SAVE_SLOT_EPISODES;
}

unsigned int highScoreBoard(unsigned int episode, bool twoPlayer)
{
	assert(episode >= 1 && episode <= highScoreEpisodes());
	const unsigned int board = (episode - 1) * 2 + (twoPlayer ? 1 : 0);

	return usesVariantBoards() ? gameSchema()->save_suffix_timed_boards + board : board;
}

unsigned int highScoreTimedBattles(void)
{
	return gameRules()->timed_battle != NULL && usesVariantBoards() ? gameRules()->timed_battle->battle_count : 0;
}

unsigned int highScoreTimedBoard(unsigned int battle)
{
	assert(battle >= 1 && battle <= highScoreTimedBattles());
	return battle - 1;
}

JE_longint highScoreValue(unsigned int board, unsigned int rank)
{
	return usesVariantBoards() ? variantEntry(board, rank)->score : slotEntry(board, rank)->highScore1;
}

const char *highScoreName(unsigned int board, unsigned int rank)
{
	return usesVariantBoards() ? variantEntry(board, rank)->playerName : slotEntry(board, rank)->highScoreName;
}

JE_byte highScoreDifficulty(unsigned int board, unsigned int rank)
{
	return usesVariantBoards() ? variantEntry(board, rank)->difficulty : slotEntry(board, rank)->highScoreDiff;
}

void highScoreShiftDown(unsigned int board, unsigned int rank)
{
	for (unsigned int i = HIGH_SCORE_ENTRIES - 1; i > rank; --i)
	{
		if (usesVariantBoards())
		{
			// The whole record moves, including the field the file keeps but this
			// game does not use.
			*variantEntry(board, i) = *variantEntry(board, i - 1);
		}
		else
		{
			// Score and name only: the difficulty stays with its slot.
			JE_SaveFileType *to = slotEntry(board, i), *from = slotEntry(board, i - 1);

			to->highScore1 = from->highScore1;
			strcpy(to->highScoreName, from->highScoreName);
		}
	}
}

void highScoreSet(unsigned int board, unsigned int rank, JE_longint score, const char *name, JE_byte difficulty)
{
	if (usesVariantBoards())
	{
		VariantHighScore *entry = variantEntry(board, rank);

		entry->score = score;
		copyName(entry->playerName, sizeof entry->playerName, name);
		entry->difficulty = difficulty;
	}
	else
	{
		JE_SaveFileType *entry = slotEntry(board, rank);

		entry->highScore1 = score;
		copyName(entry->highScoreName, sizeof entry->highScoreName, name);
		entry->highScoreDiff = difficulty;
	}
}

// Exchange sort, in the order the original game used, on one board.
static void sortSlotBoard(unsigned int board)
{
	for (unsigned int a = 0; a < HIGH_SCORE_ENTRIES - 1; ++a)
	{
		for (unsigned int b = a + 1; b < HIGH_SCORE_ENTRIES; ++b)
		{
			JE_SaveFileType *x = slotEntry(board, a), *y = slotEntry(board, b);

			if (x->highScore1 < y->highScore1)
			{
				JE_longint tempScore = x->highScore1;
				x->highScore1 = y->highScore1;
				y->highScore1 = tempScore;

				char tempName[sizeof x->highScoreName];
				strcpy(tempName, x->highScoreName);
				strcpy(x->highScoreName, y->highScoreName);
				strcpy(y->highScoreName, tempName);

				JE_byte tempDiff = x->highScoreDiff;
				x->highScoreDiff = y->highScoreDiff;
				y->highScoreDiff = tempDiff;
			}
		}
	}
}

// Same rule on the variant table, where a whole record swaps.
static void sortVariantBoard(unsigned int index)
{
	VariantHighScore *board = variantHighScores[index];

	for (unsigned int a = 0; a < HIGH_SCORE_ENTRIES - 1; ++a)
	{
		for (unsigned int b = a + 1; b < HIGH_SCORE_ENTRIES; ++b)
		{
			if (board[a].score < board[b].score)
			{
				VariantHighScore temp = board[a];
				board[a] = board[b];
				board[b] = temp;
			}
		}
	}
}

void highScoreSortAll(void)
{
	if (usesVariantBoards())
	{
		// Timed Battle boards too: they are part of the same table.
		for (unsigned int i = 0; i < VARIANT_SCORE_BOARDS; ++i)
			sortVariantBoard(i);
	}
	else
	{
		for (unsigned int board = 0; board < SAVE_SLOT_EPISODES * 2; ++board)
			sortSlotBoard(board);
	}
}
