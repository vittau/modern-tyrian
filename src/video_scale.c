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
#include "video_scale.h"

#include "palette.h"
#include "video.h"

#include <assert.h>
#include <string.h>

// The 8-bit frame through the active palette, 1x.  The old enlarging scalers
// (hq2x/hq3x/hq4x, Scale2x/3x, plain nnNx) were removed: the engine renders at
// 320x200 (Classic) or at the Modern canvas size and the GPU does all scaling.
void video_convert_frame(SDL_Surface *src_surface, SDL_Texture *dst_texture)
{
	Uint8 *src = src_surface->pixels;
	Uint8 *dst;

	const int src_pitch = src_surface->pitch;
	int dst_pitch;

	const int dst_Bpp = 4;  // SDL_BYTESPERPIXEL(XRGB8888)
	float dst_width_f, dst_height_f;
	SDL_GetTextureSize(dst_texture, &dst_width_f, &dst_height_f);

	assert(src_surface->w == vga_width && src_surface->h == vga_height);
	assert((int)dst_width_f == vga_width && (int)dst_height_f == vga_height);

	void *tmp_ptr;
	SDL_LockTexture(dst_texture, NULL, &tmp_ptr, &dst_pitch);
	dst = tmp_ptr;

	for (int y = 0; y < vga_height; y++)
	{
		for (int x = 0; x < vga_width; x++)
		{
			*(Uint32 *)dst = rgb_palette[src[x]];
			dst += dst_Bpp;
		}

		src += src_pitch;
		dst += dst_pitch - vga_width * dst_Bpp;
	}

	SDL_UnlockTexture(dst_texture);
}
