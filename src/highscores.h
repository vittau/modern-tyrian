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
#ifndef HIGHSCORES_H
#define HIGHSCORES_H

#include "opentyr.h"

#include <stdbool.h>

// Episode high-score boards.  Each episode has one board for one player and one
// for two, and each board ranks three entries.  Where the boards are stored is
// the variant's business (the save slots in 2.1, the score suffix of the save
// file in Tyrian 2000, see GameDataSchema); the callers only see boards.
#define HIGH_SCORE_ENTRIES 3

// Episodes that have boards.
unsigned int highScoreEpisodes(void);

// The board of an episode (1-based) for one or two players.
unsigned int highScoreBoard(unsigned int episode, bool twoPlayer);

JE_longint highScoreValue(unsigned int board, unsigned int rank);
const char *highScoreName(unsigned int board, unsigned int rank);
JE_byte highScoreDifficulty(unsigned int board, unsigned int rank);

// Makes room for a new entry at rank by moving the ones below it down one place.
// The entry at rank keeps its old contents until highScoreSet.
void highScoreShiftDown(unsigned int board, unsigned int rank);
void highScoreSet(unsigned int board, unsigned int rank, JE_longint score, const char *name, JE_byte difficulty);

// Puts every board in descending order of score.
void highScoreSortAll(void);

#endif // HIGHSCORES_H
