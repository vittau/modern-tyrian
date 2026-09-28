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
#ifndef STARLIB_H
#define STARLIB_H

#include "opentyr.h"

#include "keyboard.h"

bool starLibMain(KeyboardInput *out_keyboardInput);
void JE_wackyCol(void);
void JE_starlib_init(void);
void JE_resetValues(void);
void JE_changeSetup(JE_byte setupType);
void JE_newStar(void);

// Projects the current star state onto `surface` centred at (surface->w / 2,
// 100), read-only (it does not advance the star positions or draw the game's
// 320-wide frame).  The jukebox uses it in Modern mode to fill the whole canvas
// width with the same starfield the game drew into the 320x200 frame.  The
// projection matches starLibMain()'s draw, so the centred 320 columns are the
// same stars.
void starLib_paint(SDL_Surface *surface);

#endif /* STARLIB_H */
