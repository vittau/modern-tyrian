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
#include "backgrnd.h"

#include "config.h"
#include "drawlist.h"
#include "modern.h"
#include "mtrand.h"
#include "opentyr.h"
#include "video.h"

#include <assert.h>
#include <string.h>

/*Special Background 2 and Background 3*/

/*Back Pos 3*/
JE_word backPos, backPos2, backPos3;
JE_word backMove, backMove2, backMove3;

/*Main Maps*/
JE_word mapX, mapY, mapX2, mapX3, mapY2, mapY3;
JE_byte **mapYPos, **mapY2Pos, **mapY3Pos;
JE_word mapXPos, oldMapXOfs, mapXOfs, mapX2Ofs, mapX2Pos, mapX3Pos, oldMapX3Ofs, mapX3Ofs, tempMapXOfs;
intptr_t mapXbpPos, mapX2bpPos, mapX3bpPos;
JE_byte map1YDelay, map1YDelayMax, map2YDelay, map2YDelayMax;

JE_boolean  anySmoothies;
JE_byte     smoothie_data[9]; /* [1..9] */

void JE_darkenBackground(JE_word neat)  /* wild detail level */
{
	drawlist_record_darken(VGAScreen, neat);
	drawlist_apply_darken(VGAScreen, neat);
}

void drawlist_apply_darken(SDL_Surface *surface, JE_word neat)
{
	Uint8 *s = surface->pixels; /* screen pointer, 8-bit specific */
	int x, y;
	
	s += 24;
	
	for (y = 184; y; y--)
	{
		for (x = 264; x; x--)
		{
			*s = ((((*s & 0x0f) << 4) - (*s & 0x0f) + ((((x - neat - y) >> 2) + *(s-2) + (y == 184 ? 0 : *(s-(surface->pitch-1)))) & 0x0f)) >> 4) | (*s & 0xf0);
			s++;
		}
		s += surface->pitch - 264;
	}
}

void blit_background_row(SDL_Surface *surface, int x, int y, Uint8 **map)
{
	drawlist_record_bg_row(surface, x, y, map, false);

	assert(SDL_BITSPERPIXEL(surface->format) == 8);
	
	Uint8 *pixels = (Uint8 *)surface->pixels + (y * surface->pitch) + x,
	      *pixels_ll = (Uint8 *)surface->pixels,  // lower limit
	      *pixels_ul = (Uint8 *)surface->pixels + (surface->h * surface->pitch);  // upper limit
	
	for (int y = 0; y < 28; y++)
	{
		// not drawing on screen yet; skip y
		if ((pixels + (12 * 24)) < pixels_ll)
		{
			pixels += surface->pitch;
			continue;
		}
		
		for (int tile = 0; tile < 12; tile++)
		{
			Uint8 *data = *(map + tile);
			
			// no tile; skip tile
			if (data == NULL)
			{
				pixels += 24;
				continue;
			}
			
			data += y * 24;
			
			for (int x = 24; x; x--)
			{
				if (pixels >= pixels_ul)
					return;
				if (pixels >= pixels_ll && *data != 0)
					*pixels = *data;
				
				pixels++;
				data++;
			}
		}
		
		pixels += surface->pitch - 12 * 24;
	}
}

void blit_background_row_blend(SDL_Surface *surface, int x, int y, Uint8 **map)
{
	drawlist_record_bg_row(surface, x, y, map, true);

	assert(SDL_BITSPERPIXEL(surface->format) == 8);
	
	Uint8 *pixels = (Uint8 *)surface->pixels + (y * surface->pitch) + x,
	      *pixels_ll = (Uint8 *)surface->pixels,  // lower limit
	      *pixels_ul = (Uint8 *)surface->pixels + (surface->h * surface->pitch);  // upper limit
	
	for (int y = 0; y < 28; y++)
	{
		// not drawing on screen yet; skip y
		if ((pixels + (12 * 24)) < pixels_ll)
		{
			pixels += surface->pitch;
			continue;
		}
		
		for (int tile = 0; tile < 12; tile++)
		{
			Uint8 *data = *(map + tile);
			
			// no tile; skip tile
			if (data == NULL)
			{
				pixels += 24;
				continue;
			}
			
			data += y * 24;
			
			for (int x = 24; x; x--)
			{
				if (pixels >= pixels_ul)
					return;
				if (pixels >= pixels_ll && *data != 0)
					*pixels = (*data & 0xf0) | (((*pixels & 0x0f) + (*data & 0x0f)) / 2);
				
				pixels++;
				data++;
			}
		}
		
		pixels += surface->pitch - 12 * 24;
	}
}

void draw_background_1(SDL_Surface *surface)
{
	drawlist_record_fill_full(surface);
	SDL_FillSurfaceRect(surface, NULL, 0);
	
	Uint8 **map = (Uint8 **)mapYPos + mapXbpPos - 12;
	
	for (int i = -1; i < 7; i++)
	{
		drawlist_set_context(DL_OBJ_BACKGROUND, 1, i + 1);
		blit_background_row(surface, mapXPos, (i * 28) + backPos, map);
		
		map += 14;
	}
}

void draw_background_2(SDL_Surface *surface)
{
	if (map2YDelayMax > 1 && backMove2 < 2)
		backMove2 = (map2YDelay == 1) ? 1 : 0;
	
	if (background2 != 0)
	{
		// water effect combines background 1 and 2 by synchronizing the x coordinate
		int x = smoothies[1] ? mapXPos : mapX2Pos;
		
		Uint8 **map = (Uint8 **)mapY2Pos + (smoothies[1] ? mapXbpPos : mapX2bpPos) - 12;
		
		for (int i = -1; i < 7; i++)
		{
			drawlist_set_context(DL_OBJ_BACKGROUND, 2, i + 1);
			blit_background_row(surface, x, (i * 28) + backPos2, map);
			
			map += 14;
		}
	}
	
	/*Set Movement of background*/
	if (--map2YDelay == 0)
	{
		map2YDelay = map2YDelayMax;
		
		backPos2 += backMove2;
		
		if (backPos2 >  27)
		{
			backPos2 -= 28;
			mapY2--;
			mapY2Pos -= 14;  /*Map Width*/
		}
	}
}

void draw_background_2_blend(SDL_Surface *surface)
{
	if (map2YDelayMax > 1 && backMove2 < 2)
		backMove2 = (map2YDelay == 1) ? 1 : 0;
	
	Uint8 **map = (Uint8 **)mapY2Pos + mapX2bpPos - 12;
	
	for (int i = -1; i < 7; i++)
	{
		drawlist_set_context(DL_OBJ_BACKGROUND, 2, i + 1);
		blit_background_row_blend(surface, mapX2Pos, (i * 28) + backPos2, map);
		
		map += 14;
	}
	
	/*Set Movement of background*/
	if (--map2YDelay == 0)
	{
		map2YDelay = map2YDelayMax;
		
		backPos2 += backMove2;
		
		if (backPos2 >  27)
		{
			backPos2 -= 28;
			mapY2--;
			mapY2Pos -= 14;  /*Map Width*/
		}
	}
}

void draw_background_3(SDL_Surface *surface)
{
	/* Movement of background */
	backPos3 += backMove3;
	
	if (backPos3 > 27)
	{
		backPos3 -= 28;
		mapY3--;
		mapY3Pos -= 15;   /*Map Width*/
	}
	
	Uint8 **map = (Uint8 **)mapY3Pos + mapX3bpPos - 12;
	
	for (int i = -1; i < 7; i++)
	{
		drawlist_set_context(DL_OBJ_BACKGROUND, 3, i + 1);
		blit_background_row(surface, mapX3Pos, (i * 28) + backPos3, map);
		
		map += 15;
	}
}

void JE_filterScreen(JE_shortint col, JE_shortint int_)
{
	// Whether this applied offset is a step of a brightness ramp.  Captured
	// before the bookkeeping so the ramp's last step (which clears filterFade)
	// still counts.  Drives whether the Modern HUD mirrors the offset.
	modern_note_playfield_filter_fade(filterFade);

	if (filterFade)
	{
		levelBrightness += levelBrightnessChg;
		if ((filterFadeStart && levelBrightness < -14) || levelBrightness > 14)
		{
			levelBrightnessChg = -levelBrightnessChg;
			filterFadeStart = false;
			levelFilter = levelFilterNew;
		}
		if (!filterFadeStart && levelBrightness == 0)
		{
			filterFade = false;
			levelBrightness = -99;
		}
	}

	drawlist_record_filter_screen(VGAScreen, col, int_);
	drawlist_apply_filter_screen(VGAScreen, col, int_);
}

// Pixel-apply half of JE_filterScreen: only the two in-place full-frame passes,
// with the fade bookkeeping left to the caller.  Replayed by the draw list so
// the replay does not advance levelBrightness/filterFade.
void drawlist_apply_filter_screen(SDL_Surface *surface, JE_shortint col, JE_shortint int_)
{
	Uint8 *s = NULL; /* screen pointer, 8-bit specific */
	int x, y;
	unsigned int temp;

	// The Modern compositor recomposes the HUD panels/message strip from
	// separate surfaces; report the brightness offset baked into this playfield
	// so it can mirror the fade on them.  Display-only.
	modern_note_playfield_filter(int_);

	if (col != -99 && filtrationAvail)
	{
		s = surface->pixels;
		s += 24;
		
		col <<= 4;
		
		for (y = 184; y; y--)
		{
			for (x = 264; x; x--)
			{
				*s = col | (*s & 0x0f);
				s++;
			}
			s += surface->pitch - 264;
		}
	}
	
	if (int_ != -99 && explosionTransparent)
	{
		s = surface->pixels;
		s += 24;
		
		for (y = 184; y; y--)
		{
			for (x = 264; x; x--)
			{
				temp = (*s & 0x0f) + int_;
				*s = (*s & 0xf0) | (temp >= 0x1f ? 0 : (temp >= 0x0f ? 0x0f : temp));
				s++;
			}
			s += surface->pitch - 264;
		}
	}
}

void JE_checkSmoothies(void)
{
	anySmoothies = (processorType > 2 && (smoothies[1-1] || smoothies[2-1])) || (processorType > 1 && (smoothies[3-1] || smoothies[4-1] || smoothies[5-1]));
}

void lava_filter(SDL_Surface *dst, SDL_Surface *src)
{
	drawlist_record_filter(dst, src, DL_FILTER_LAVA);

	assert(SDL_BITSPERPIXEL(src->format) == 8 && SDL_BITSPERPIXEL(dst->format) == 8);
	
	/* we don't need to check for over-reading the pixel surfaces since we only
	 * read from the top 185+1 scanlines, and there should be 320 */
	
	const int dst_pitch = dst->pitch;
	Uint8 *dst_pixel = (Uint8 *)dst->pixels + (185 * dst_pitch);
	const Uint8 * const dst_pixel_ll = (Uint8 *)dst->pixels;  // lower limit
	
	const int src_pitch = src->pitch;
	const Uint8 *src_pixel = (Uint8 *)src->pixels + (185 * src->pitch);
	const Uint8 * const src_pixel_ll = (Uint8 *)src->pixels;  // lower limit
	
	int w = 320 * 185 - 1;
	
	for (int y = 185 - 1; y >= 0; --y)
	{
		dst_pixel -= (dst_pitch - 320);  // in case pitch is not 320
		src_pixel -= (src_pitch - 320);  // in case pitch is not 320
		
		for (int x = 320 - 1; x >= 0; x -= 8)
		{
			int waver = abs(((w >> 9) & 0x0f) - 8) - 1;
			w -= 8;
			
			for (int xi = 8 - 1; xi >= 0; --xi)
			{
				--dst_pixel;
				--src_pixel;
				
				// value is average value of source pixel (2x), destination pixel above, and destination pixel below (all with waver)
				// hue is red
				Uint8 value = 0;
				
				if (src_pixel + waver >= src_pixel_ll)
					value += (*(src_pixel + waver) & 0x0f) * 2;
				value += *(dst_pixel + waver + dst_pitch) & 0x0f;
				if (dst_pixel + waver - dst_pitch >= dst_pixel_ll)
					value += *(dst_pixel + waver - dst_pitch) & 0x0f;
				
				*dst_pixel = (value / 4) | 0x70;
			}
		}
	}
}

void water_filter(SDL_Surface *dst, SDL_Surface *src)
{
	drawlist_record_filter(dst, src, DL_FILTER_WATER);

	assert(SDL_BITSPERPIXEL(src->format) == 8 && SDL_BITSPERPIXEL(dst->format) == 8);
	
	Uint8 hue = smoothie_data[1] << 4;
	
	/* we don't need to check for over-reading the pixel surfaces since we only
	 * read from the top 185+1 scanlines, and there should be 320 */
	
	const int dst_pitch = dst->pitch;
	Uint8 *dst_pixel = (Uint8 *)dst->pixels + (185 * dst_pitch);
	
	const Uint8 *src_pixel = (Uint8 *)src->pixels + (185 * src->pitch);
	
	int w = 320 * 185 - 1;
	
	for (int y = 185 - 1; y >= 0; --y)
	{
		dst_pixel -= (dst_pitch - 320);  // in case pitch is not 320
		src_pixel -= (src->pitch - 320);  // in case pitch is not 320
		
		for (int x = 320 - 1; x >= 0; x -= 8)
		{
			int waver = abs(((w >> 10) & 0x07) - 4) - 1;
			w -= 8;
			
			for (int xi = 8 - 1; xi >= 0; --xi)
			{
				--dst_pixel;
				--src_pixel;
				
				// pixel is copied from source if not blue
				// otherwise, value is average of value of source pixel and destination pixel below (with waver)
				if ((*src_pixel & 0x30) == 0)
				{
					*dst_pixel = *src_pixel;
				}
				else
				{
					Uint8 value = *src_pixel & 0x0f;
					value += *(dst_pixel + waver + dst_pitch) & 0x0f;
					*dst_pixel = (value / 2) | hue;
				}
			}
		}
	}
}

void iced_blur_filter(SDL_Surface *dst, SDL_Surface *src)
{
	drawlist_record_filter(dst, src, DL_FILTER_ICED);

	assert(SDL_BITSPERPIXEL(src->format) == 8 && SDL_BITSPERPIXEL(dst->format) == 8);
	
	Uint8 *dst_pixel = dst->pixels;
	const Uint8 *src_pixel = src->pixels;
	
	for (int y = 0; y < 184; ++y)
	{
		for (int x = 0; x < 320; ++x)
		{
			// value is average value of source pixel and destination pixel
			// hue is icy blue
			
			const Uint8 value = (*src_pixel & 0x0f) + (*dst_pixel & 0x0f);
			*dst_pixel = (value / 2) | 0x80;
			
			++dst_pixel;
			++src_pixel;
		}
		
		dst_pixel += (dst->pitch - 320);  // in case pitch is not 320
		src_pixel += (src->pitch - 320);  // in case pitch is not 320
	}
}

void blur_filter(SDL_Surface *dst, SDL_Surface *src)
{
	drawlist_record_filter(dst, src, DL_FILTER_BLUR);

	assert(SDL_BITSPERPIXEL(src->format) == 8 && SDL_BITSPERPIXEL(dst->format) == 8);
	
	Uint8 *dst_pixel = dst->pixels;
	const Uint8 *src_pixel = src->pixels;
	
	for (int y = 0; y < 184; ++y)
	{
		for (int x = 0; x < 320; ++x)
		{
			// value is average value of source pixel and destination pixel
			// hue is source pixel hue
			
			const Uint8 value = (*src_pixel & 0x0f) + (*dst_pixel & 0x0f);
			*dst_pixel = (value / 2) | (*src_pixel & 0xf0);
			
			++dst_pixel;
			++src_pixel;
		}
		
		dst_pixel += (dst->pitch - 320);  // in case pitch is not 320
		src_pixel += (src->pitch - 320);  // in case pitch is not 320
	}
}

/* Background Starfield */
typedef struct
{
	Uint8 color;
	JE_word position; // relies on overflow wrap-around
	int speed;
} StarfieldStar;

#define MAX_STARS 100
#define STARFIELD_HUE 0x90
static StarfieldStar starfield_stars[MAX_STARS];
int starfield_speed;

void initialize_starfield(void)
{
	for (int i = MAX_STARS-1; i >= 0; --i)
	{
		starfield_stars[i].position = mt_rand() % 320 + mt_rand() % 200 * VGAScreen->pitch;
		starfield_stars[i].speed = mt_rand() % 3 + 2;
		starfield_stars[i].color = mt_rand() % 16 + STARFIELD_HUE;
	}
}

void update_and_draw_starfield(SDL_Surface* surface, int move_speed)
{
	drawlist_record_starfield(surface, move_speed, starfield_stars, sizeof starfield_stars);

	Uint8* p = (Uint8*)surface->pixels;

	for (int i = MAX_STARS-1; i >= 0; --i)
	{
		StarfieldStar* star = &starfield_stars[i];

		star->position += (star->speed + move_speed) * surface->pitch;

		if (star->position < 177 * surface->pitch)
		{
			if (p[star->position] == 0)
			{
				p[star->position] = star->color;
			}

			// If star is bright enough, draw surrounding pixels
			if (star->color - 4 >= STARFIELD_HUE)
			{
				if (p[star->position + 1] == 0)
					p[star->position + 1] = star->color - 4;

				if (star->position > 0 && p[star->position - 1] == 0)
					p[star->position - 1] = star->color - 4;

				if (p[star->position + surface->pitch] == 0)
					p[star->position + surface->pitch] = star->color - 4;

				if (star->position >= surface->pitch && p[star->position - surface->pitch] == 0)
					p[star->position - surface->pitch] = star->color - 4;
			}
		}
	}
}

// Regress-only parallax guard (see drawlist_parallax_tick()): `pre` is the star
// state captured at the start of one tick (the recorded payload) and the live
// array is the state after the level logic advanced it once.  Returns how many
// stars did not advance by exactly (speed + move_speed) rows over the tick -- a
// starfield updated twice per tick, or at the wrong speed, shows up here.
int starfield_check_advance(const void *pre, size_t bytes, int move_speed, int pitch)
{
	if (bytes != sizeof starfield_stars)
		return 0;

	const StarfieldStar *before = pre;
	int bad = 0;

	for (int i = 0; i < MAX_STARS; ++i)
	{
		const Uint16 expected = (Uint16)(before[i].position + (before[i].speed + move_speed) * pitch);
		if (starfield_stars[i].position != expected)
			++bad;
	}

	return bad;
}

// Raw live star state, for the regress parallax guard to prove the interpolated
// presentation never advances it.
const void *starfield_state(void)
{
	return starfield_stars;
}

size_t starfield_state_size(void)
{
	return sizeof starfield_stars;
}

// Replays one recorded starfield step.  The caller captured the pre-step star
// state; restoring it and re-running the (pure) update lands the live array on
// exactly the post-step state the real call produced.
void drawlist_replay_starfield(SDL_Surface *surface, int move_speed, const void *stars, size_t bytes)
{
	if (bytes != sizeof starfield_stars)
		return;

	memcpy(starfield_stars, stars, sizeof starfield_stars);
	update_and_draw_starfield(surface, move_speed);
}

// Stage-3 interpolated starfield: draws each star partway (alpha_fx16, 16.16)
// between the previous frame's position and this tick's advanced position,
// without touching the live array.  A star whose position wrapped snaps to the
// advanced position.  At alpha = 1 this is byte-identical to the draw half of
// update_and_draw_starfield().
void drawlist_draw_starfield_interp(SDL_Surface *surface, int move_speed, const void *pre, size_t bytes, Uint32 alpha_fx16)
{
	if (bytes != sizeof starfield_stars)
		return;

	StarfieldStar stars[MAX_STARS];
	memcpy(stars, pre, sizeof starfield_stars);

	Uint8 *p = (Uint8 *)surface->pixels;

	for (int i = MAX_STARS - 1; i >= 0; --i)
	{
		const StarfieldStar *star = &stars[i];
		const Uint16 prev_pos = star->position;
		const Uint16 next_pos = (Uint16)(prev_pos + (star->speed + move_speed) * surface->pitch);

		Uint16 pos;
		if (next_pos < prev_pos)
			pos = next_pos;  // wrapped: snap
		else
			pos = (Uint16)(prev_pos + (Uint16)(((Sint32)(next_pos - prev_pos) * (Sint32)alpha_fx16) / 65536));

		if (pos < 177 * surface->pitch)
		{
			if (p[pos] == 0)
				p[pos] = star->color;

			if (star->color - 4 >= STARFIELD_HUE)
			{
				if (p[pos + 1] == 0)
					p[pos + 1] = star->color - 4;

				if (pos > 0 && p[pos - 1] == 0)
					p[pos - 1] = star->color - 4;

				if (p[pos + surface->pitch] == 0)
					p[pos + surface->pitch] = star->color - 4;

				if (pos >= surface->pitch && p[pos - surface->pitch] == 0)
					p[pos - surface->pitch] = star->color - 4;
			}
		}
	}
}
