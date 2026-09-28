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
#ifndef VIDEO_SCALE_H
#define VIDEO_SCALE_H

#include <SDL3/SDL.h>

// Converts the 8-bit 320x200 frame through the active palette into an
// XRGB8888 texture of the same size: a plain 1x conversion.  This is all that
// remains of the old software scalers (hq2x, Scale2x, ...); the GPU does every
// scaling and filtering step (see video_present_texture in video.h).
void video_convert_frame(SDL_Surface *src_surface, SDL_Texture *dst_texture);

#endif /* VIDEO_SCALE_H */
