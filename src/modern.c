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
#include "modern.h"

#include "logging.h"
#include "opentyr.h"
#include "video.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

const char *const presentation_names[PRESENTATION_MAX] =
{
	"classic",
	"modern",
};

Presentation presentation = PRESENTATION_CLASSIC;

// Ordered effect passes.  Empty in this task; see modern.h for the contract.
#define MODERN_MAX_PASSES 16
static ModernPassFunction modern_passes[MODERN_MAX_PASSES];
static size_t modern_passes_count = 0;

// The canvas.  Allocated/reallocated only by modern_set_canvas_size(), never in
// the per-frame path.
static ModernFrame modern_frame_state;

// The canvas texture.  Created/recreated only by modern_set_canvas_size().
static SDL_Texture *modern_texture = NULL;

bool set_presentation_by_name(const char *name)
{
	for (int i = 0; i < PRESENTATION_MAX; ++i)
	{
		if (strcmp(name, presentation_names[i]) == 0)
		{
			presentation = (Presentation)i;
			return true;
		}
	}
	return false;
}

void modern_register_pass(ModernPassFunction pass)
{
	assert(pass != NULL);
	assert(modern_passes_count < COUNTOF(modern_passes));

	if (modern_passes_count < COUNTOF(modern_passes))
		modern_passes[modern_passes_count++] = pass;
}

size_t modern_pass_count(void)
{
	return modern_passes_count;
}

void modern_set_canvas_size(int w, int h)
{
	assert(w > 0 && h > 0);

	if (modern_texture != NULL && modern_frame_state.w == w && modern_frame_state.h == h)
		return;

	// (Re)allocate the canvas.  Only called at startup / on resize, never per
	// frame.
	free(modern_frame_state.pixels);
	modern_frame_state.pixels = malloc((size_t)w * (size_t)h * sizeof(Uint32));
	if (modern_frame_state.pixels == NULL)
	{
		logFatal("Failed to allocate the modern canvas (%dx%d).", w, h);
		exit(EXIT_FAILURE);
	}

	modern_frame_state.w = w;
	modern_frame_state.h = h;
	modern_frame_state.pitch = w * (int)sizeof(Uint32);
	modern_frame_state.src = NULL;
	modern_frame_state.src_pitch = 0;
	modern_frame_state.palette = NULL;

	// (Re)create the streaming texture at the canvas size, nearest-neighbour so
	// the logical pixels stay crisp when scaled.
	if (modern_texture != NULL)
	{
		SDL_DestroyTexture(modern_texture);
		modern_texture = NULL;
	}

	modern_texture = SDL_CreateTexture(video_renderer(), SDL_PIXELFORMAT_XRGB8888,
		SDL_TEXTUREACCESS_STREAMING, w, h);

	if (modern_texture == NULL)
	{
		logFatal("Failed to create the modern canvas texture (%dx%d): %s", w, h, SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_SetTextureScaleMode(modern_texture, SDL_SCALEMODE_NEAREST);
}

void modern_init(void)
{
	modern_set_canvas_size(vga_width, vga_height);
}

void modern_deinit(void)
{
	if (modern_texture != NULL)
	{
		SDL_DestroyTexture(modern_texture);
		modern_texture = NULL;
	}

	free(modern_frame_state.pixels);
	modern_frame_state.pixels = NULL;
	modern_frame_state.w = 0;
	modern_frame_state.h = 0;
	modern_frame_state.pitch = 0;
	modern_frame_state.src = NULL;
	modern_frame_state.src_pitch = 0;
	modern_frame_state.palette = NULL;
}

void modern_build_frame(SDL_Surface *src_surface)
{
	assert(SDL_BITSPERPIXEL(src_surface->format) == 8);

	ModernFrame *frame = &modern_frame_state;
	assert(frame->pixels != NULL);

	frame->src = src_surface->pixels;
	frame->src_pitch = src_surface->pitch;
	frame->palette = get_active_palette();

	// Convert through the very same XRGB words the Classic "None" software
	// scaler writes, so the canvas is pixel-for-pixel identical to Classic
	// with no passes registered.  Only the source region is converted; since
	// the canvas is currently the same size as the 8-bit frame it covers it
	// all.  A later task that widens the canvas owns the extra region.
	const int copy_w = MIN((int)src_surface->w, frame->w);
	const int copy_h = MIN((int)src_surface->h, frame->h);

	for (int y = 0; y < copy_h; ++y)
	{
		const Uint8 *src = frame->src + (size_t)y * frame->src_pitch;
		Uint32 *dst = frame->pixels + (size_t)y * frame->w;

		for (int x = 0; x < copy_w; ++x)
			dst[x] = rgb_palette[src[x]];
	}

	for (size_t i = 0; i < modern_passes_count; ++i)
		modern_passes[i](frame);
}

void modern_present_frame(void)
{
	ModernFrame *frame = &modern_frame_state;
	assert(modern_texture != NULL);
	assert(frame->pixels != NULL);

	// Upload the canvas to the streaming texture, honoring both pitches.
	void *texture_pixels;
	int texture_pitch;
	if (SDL_LockTexture(modern_texture, NULL, &texture_pixels, &texture_pitch))
	{
		const size_t row_bytes = (size_t)frame->w * sizeof(Uint32);
		for (int y = 0; y < frame->h; ++y)
		{
			memcpy((Uint8 *)texture_pixels + (size_t)y * texture_pitch,
			       (const Uint8 *)frame->pixels + (size_t)y * frame->pitch,
			       row_bytes);
		}
		SDL_UnlockTexture(modern_texture);
	}
	else
	{
		logError("Failed to lock the modern canvas texture: %s", SDL_GetError());
	}

	SDL_Rect dst_rect;
	video_calc_dst_render_rect(frame->w, frame->h, modern_texture, &dst_rect);

	SDL_Renderer *renderer = video_renderer();
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	const SDL_FRect dst_frect = { (float)dst_rect.x, (float)dst_rect.y, (float)dst_rect.w, (float)dst_rect.h };
	SDL_RenderTexture(renderer, modern_texture, NULL, &dst_frect);
	SDL_RenderPresent(renderer);

	video_set_last_output_rect(&dst_rect);
}

const ModernFrame *modern_current_frame(void)
{
	return &modern_frame_state;
}
