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
#include "high_scores.h"

#include "config.h"
#include "game_schema.h"

// Legacy layout: six boards of three slots per three episodes.
#define LEGACY_EPISODES 3
#define LEGACY_SLOTS_PER_EPISODE 6

// Suffix layout: after the Timed Battle boards, an episode owns two boards.
static bool hasSuffixBoards(void)
{
	return gameSchema()->save_suffix_main_boards > 0;
}

static size_t suffixMainBase(void)
{
	return gameSchema()->save_suffix_timed_boards;
}

size_t highScorePageCount(void)
{
	return hasSuffixBoards() ? gameSchema()->save_suffix_main_boards / 2 : LEGACY_EPISODES;
}

HighScoreRow highScoreRow(size_t page, bool two_player, size_t entry)
{
	if (hasSuffixBoards())
	{
		const VariantHighScore *score = &variantHighScores[suffixMainBase() + page * 2 + (two_player ? 1 : 0)][entry];
		return (HighScoreRow){ score->score, score->playerName, score->difficulty };
	}

	const JE_SaveFileType *save = &saveFiles[page * LEGACY_SLOTS_PER_EPISODE + (two_player ? 3 : 0) + entry];
	return (HighScoreRow){ save->highScore1, save->highScoreName, save->highScoreDiff };
}

void highScoresSortSuffix(void)
{
	if (!hasSuffixBoards())
		return;

	const size_t boards = (size_t)gameSchema()->save_suffix_timed_boards + gameSchema()->save_suffix_main_boards;

	for (size_t board = 0; board < boards; ++board)
	{
		VariantHighScore *entries = variantHighScores[board];

		// Three entries per board: the same passes as the legacy sort.
		for (size_t pass = 0; pass < 2; ++pass)
		{
			for (size_t i = 0; i + 1 < gameSchema()->save_suffix_boards_per; ++i)
			{
				if (entries[i + 1].score > entries[i].score)
				{
					const VariantHighScore swap = entries[i];
					entries[i] = entries[i + 1];
					entries[i + 1] = swap;
				}
			}
		}
	}
}
