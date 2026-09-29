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
#ifndef HIGH_SCORES_H
#define HIGH_SCORES_H

#include "opentyr.h"

// The high-score screen shows one page per episode with a one-player and a
// two-player board.  Where the boards live depends on the variant's save:
//   Tyrian 2.1    the score fields of the encrypted save slots (3 episodes)
//   Tyrian 2000   the unencrypted boards after them (5 episodes)
// The schema says which; callers only ask for pages.  The Timed Battle boards
// of the 2000 save have no page until that mode exists.

typedef struct
{
	JE_longint  score;
	const char *name;
	JE_byte     difficulty;
} HighScoreRow;

// Pages the screen offers.
size_t highScorePageCount(void);

// Entry (0..2) of the one- or two-player board of a page.
HighScoreRow highScoreRow(size_t page, bool two_player, size_t entry);

// Puts the entries of every score board of the variant's save suffix in score
// order (the legacy slots are ordered by JE_sortHighScores).
void highScoresSortSuffix(void);

#endif /* HIGH_SCORES_H */
