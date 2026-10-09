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
#include "modern_depth.h"

#include "modern.h"

#include <string.h>

// Presented layer buffer (playfield window) and rank table.  Fixed-capacity
// statics: no per-frame allocation.
static Uint8 md_layer[MODERN_PLAYFIELD_W * MODERN_PLAYFIELD_H];
static Uint8 md_rank[DL_LAYER_COUNT];
static bool md_valid = false;
static bool md_requested = false;

static unsigned long md_frames = 0;
static unsigned long md_frames_interpolated = 0;
static unsigned long md_frames_flipped = 0;

bool modern_depth_layers_wanted(void)
{
	return presentation == PRESENTATION_MODERN && md_requested;
}

void modern_depth_set_requested(bool requested)
{
	md_requested = requested;
}

void modern_depth_begin(void)
{
	if (!modern_depth_layers_wanted())
	{
		md_valid = false;
		return;
	}

	memset(md_layer, DL_LAYER_NONE, sizeof md_layer);
	memset(md_rank, 0, sizeof md_rank);
	md_valid = true;
}

void modern_depth_from_game(SDL_Surface *game, bool flip)
{
	if (!md_valid)
		return;

	int pitch = 0;
	const Uint8 *layer = drawlist_layer_for_surface(game, &pitch, NULL, NULL);
	const Uint8 *rank = drawlist_layer_ranks(game);
	if (layer == NULL)
		return;

	// The playfield copy always starts at game x = 24 (both the normal and the
	// spotlight special code read the same columns); the vertical-flip special
	// code reverses the rows.  Same mapping as modern_bloom_tag_from_game().
	for (int y = 0; y < MODERN_PLAYFIELD_H; ++y)
	{
		const int sy = flip ? (MODERN_PLAYFIELD_H - 1 - y) : y;
		memcpy(md_layer + (size_t)y * MODERN_PLAYFIELD_W,
		       layer + (size_t)sy * (size_t)pitch + 24, MODERN_PLAYFIELD_W);
	}

	if (rank != NULL)
		memcpy(md_rank, rank, sizeof md_rank);

	md_frames++;
	if (game == drawlist_interpolated_game())
		md_frames_interpolated++;
	if (flip)
		md_frames_flipped++;
}

void modern_depth_mark_vfx(int x, int y)
{
	if (!md_valid)
		return;
	if ((unsigned)x >= MODERN_PLAYFIELD_W || (unsigned)y >= MODERN_PLAYFIELD_H)
		return;

	md_layer[(size_t)y * MODERN_PLAYFIELD_W + (size_t)x] = DL_LAYER_VFX;
}

const Uint8 *modern_depth_layer_buffer(void)
{
	return md_valid ? md_layer : NULL;
}

const Uint8 *modern_depth_rank_table(void)
{
	return md_valid ? md_rank : NULL;
}

unsigned long modern_depth_presented_frames(void)       { return md_frames; }
unsigned long modern_depth_presented_interpolated(void) { return md_frames_interpolated; }
unsigned long modern_depth_presented_flipped(void)      { return md_frames_flipped; }

bool modern_depth_save_png(const char *path)
{
	if (!md_valid || path == NULL)
		return false;

	// One distinct colour per layer; a pixel of the blended bg2 row is lightened
	// so the flag is visible next to the opaque bg2.
	static const Uint8 colors[DL_LAYER_COUNT][3] =
	{
		{   0,   0,   0 },  // none
		{  40,  60, 170 },  // bg1
		{ 255, 255, 255 },  // starfield
		{  40, 170,  60 },  // bg2
		{ 200, 120,  40 },  // ground enemy
		{ 240, 220,  60 },  // sky enemy
		{ 170,  60, 200 },  // bg3
		{ 230,  60,  60 },  // top enemy
		{   0, 230, 230 },  // player
		{   0, 140, 140 },  // sidekick
		{ 130, 255, 130 },  // player shot
		{ 255, 130, 200 },  // enemy shot
		{ 255, 170,   0 },  // explosion
		{ 255, 255, 150 },  // superpixel
		{ 150, 150, 150 },  // vfx
		{  80,  80,  80 },  // hud
		{ 255,   0, 255 },  // other
	};

	SDL_Surface *surface = SDL_CreateSurface(MODERN_PLAYFIELD_W, MODERN_PLAYFIELD_H, SDL_PIXELFORMAT_RGBA32);
	if (surface == NULL)
		return false;

	for (int y = 0; y < MODERN_PLAYFIELD_H; ++y)
	{
		Uint8 *out = (Uint8 *)surface->pixels + (size_t)y * (size_t)surface->pitch;
		for (int x = 0; x < MODERN_PLAYFIELD_W; ++x)
		{
			const Uint8 v = md_layer[(size_t)y * MODERN_PLAYFIELD_W + (size_t)x];
			unsigned id = v & DL_LAYER_ID_MASK;
			if (id >= DL_LAYER_COUNT)
				id = DL_LAYER_OTHER;

			for (int c = 0; c < 3; ++c)
			{
				unsigned value = colors[id][c];
				if (v & DL_LAYER_BLEND)
					value = (value + 255) / 2;
				out[x * 4 + c] = (Uint8)value;
			}
			out[x * 4 + 3] = 255;
		}
	}

	const bool saved = SDL_SavePNG(surface, path);
	SDL_DestroySurface(surface);
	return saved;
}
