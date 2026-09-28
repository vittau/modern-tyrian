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
#include "modern_bloom.h"
#include "opentyr.h"
#include "video.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

const char *const presentation_names[PRESENTATION_MAX] =
{
	"classic",
	"modern",
};

Presentation presentation = PRESENTATION_CLASSIC;

const char *const modern_aspect_names[MODERN_ASPECT_MAX] =
{
	"4:3",
	"16:10",
	"16:9",
	"21:9",
	"32:9",
	"auto",
};

const char *const modern_pixel_aspect_names[PIXEL_ASPECT_MAX] =
{
	"original",
	"square",
};

ModernAspect modern_aspect = MODERN_ASPECT_4_3;
ModernPixelAspect modern_pixel_aspect = PIXEL_ASPECT_ORIGINAL;

// Aspect ratios for the fixed values; "auto" is resolved from the window.
static const float modern_aspect_ratios[MODERN_ASPECT_MAX] =
{
	4.0f / 3.0f,
	16.0f / 10.0f,
	16.0f / 9.0f,
	21.0f / 9.0f,
	32.0f / 9.0f,
	4.0f / 3.0f,  // auto: replaced by the window aspect
};

// Ordered effect passes.  Empty in this task; see modern.h for the contract.
#define MODERN_MAX_PASSES 16
static ModernPassFunction modern_passes[MODERN_MAX_PASSES];
static size_t modern_passes_count = 0;

// The canvas.  Allocated/reallocated only by modern_set_canvas_size(), never in
// the per-frame path.
static ModernFrame modern_frame_state;

// The canvas texture.  Created/recreated only by modern_set_canvas_size().
static SDL_Texture *modern_texture = NULL;

// True once modern_init() ran and the renderer exists.  The resize entry points
// that the window/fullscreen code calls become no-ops until then.
static bool modern_ready = false;

// One-shot flag set by JE_starShowVGA() just before it presents.
static bool modern_gameplay_frame = false;

// Row colours for the side panels: [left pre][right pre][left blur][right blur],
// 3 bytes each.  Allocated by modern_set_canvas_size() for the canvas height,
// never in the per-frame path.
static Uint8 *modern_panel_scratch = NULL;

// Precomputed per-column fade factors for the side panels (Q32 fixed point),
// sized to the canvas width.  Allocated by modern_set_canvas_size().
static Uint64 *modern_panel_scale_scratch = NULL;

// Off-screen 8-bit surfaces the game draws the relocated in-game HUD into (one
// per side panel), plus their allocated widths.  Index 0 is transparent.  They
// are (re)allocated only by modern_set_canvas_size(), never per frame.
static SDL_Surface *modern_hud_surfaces[2] = { NULL, NULL };
static int modern_hud_surface_w[2] = { 0, 0 };

// Width of the narrower side panel for the current canvas (the left one; the two
// differ by at most one pixel).  Kept in sync by modern_set_canvas_size().
static int modern_hud_panel_w = 0;

// The off-screen 8-bit surface for the message strip under the playfield
// (MODERN_PLAYFIELD_W wide by MODERN_MESSAGE_H rows), index 0 transparent.
// (Re)allocated only by modern_set_canvas_size(), never per frame.
static SDL_Surface *modern_message_surface = NULL;
static int modern_message_surface_w = 0;

// The current in-game message (set by JE_drawTextWindow through
// modern_message_set).  Display-only; copied into a fixed buffer.
static char modern_message[64] = "";

// Offset of the content the mouse maps to in the last built frame: for gameplay
// frames in panel mode this is the playfield offset, otherwise the 320x200
// frame offset.  Recorded by modern_build_frame() and used by
// modern_present_frame() for mouse mapping.
static int modern_frame_offset_x = 0;
static int modern_frame_offset_y = 0;

// Scratch for the blurred background fill of non-gameplay frames: the 320x200
// frame downsampled to MODERN_BLUR_LOW_W x MODERN_BLUR_LOW_H RGB, plus the
// per-canvas-column source map.  Allocated by modern_set_canvas_size().
#define MODERN_BLUR_LOW_W 40
#define MODERN_BLUR_LOW_H 25
static Uint8 *modern_blur_low = NULL;
static Uint32 *modern_blur_xmap = NULL;

static void modern_fill_side_panels(ModernFrame *frame, int left_edge, int right_edge,
                                    int left_width, int right_x, int right_width);
static void modern_fill_blurred_background(ModernFrame *frame, int frame_x);
static void modern_composite_hud(ModernFrame *frame, int frame_x);
static void modern_composite_message(ModernFrame *frame, int frame_x);
static void modern_set_hud_surface(SDL_Surface **surface, int *stored_w, int panel_w);
static void modern_set_message_surface(int playfield_w);

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

bool set_modern_aspect_by_name(const char *name)
{
	for (int i = 0; i < MODERN_ASPECT_MAX; ++i)
	{
		if (strcmp(name, modern_aspect_names[i]) == 0)
		{
			modern_aspect = (ModernAspect)i;
			return true;
		}
	}
	return false;
}

bool set_modern_pixel_aspect_by_name(const char *name)
{
	for (int i = 0; i < PIXEL_ASPECT_MAX; ++i)
	{
		if (strcmp(name, modern_pixel_aspect_names[i]) == 0)
		{
			modern_pixel_aspect = (ModernPixelAspect)i;
			return true;
		}
	}
	return false;
}

float modern_pixel_aspect_factor(void)
{
	return modern_pixel_aspect == PIXEL_ASPECT_ORIGINAL ? 1.2f : 1.0f;
}

void modern_mark_gameplay_frame(void)
{
	modern_gameplay_frame = true;
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
	modern_frame_state.gameplay = false;
	modern_frame_state.content_offset_x = 0;

	// Scratch for the side panels' per-row colours (four h*3 buffers) and their
	// per-column fade factors (w Q32 values).  Grows only when the canvas is
	// resized.
	free(modern_panel_scratch);
	modern_panel_scratch = malloc((size_t)h * 3 * 4);
	if (modern_panel_scratch == NULL)
	{
		logFatal("Failed to allocate the modern panel scratch (%dx%d).", w, h);
		exit(EXIT_FAILURE);
	}

	free(modern_panel_scale_scratch);
	modern_panel_scale_scratch = malloc((size_t)w * sizeof(Uint64));
	if (modern_panel_scale_scratch == NULL)
	{
		logFatal("Failed to allocate the modern panel scale scratch (%dx%d).", w, h);
		exit(EXIT_FAILURE);
	}

	// Scratch for the blurred background fill (non-gameplay frames): the
	// downsampled frame and the per-column source map.
	free(modern_blur_low);
	modern_blur_low = malloc((size_t)MODERN_BLUR_LOW_W * MODERN_BLUR_LOW_H * 3);
	if (modern_blur_low == NULL)
	{
		logFatal("Failed to allocate the modern blur buffer (%dx%d).", w, h);
		exit(EXIT_FAILURE);
	}

	free(modern_blur_xmap);
	modern_blur_xmap = malloc((size_t)w * sizeof(Uint32));
	if (modern_blur_xmap == NULL)
	{
		logFatal("Failed to allocate the modern blur column map (%dx%d).", w, h);
		exit(EXIT_FAILURE);
	}

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

	// Side-panel HUD surfaces.  In panel mode the compositor copies only the
	// 264x184 playfield out of the 320x200 frame, so the two panels are the
	// columns left and right of the centred playfield:
	//   left_w = (w - 264) / 2, right_w = w - left_w - 264.
	// Only their common (narrower) width matters for the layout checks.  When
	// that is below MODERN_HUD_MIN_PANEL_WIDTH the game keeps its original
	// full-frame layout and modern_hud_in_panels() reports false.
	const int playfield_x = (w - MODERN_PLAYFIELD_W) / 2;
	const int left_w = playfield_x;
	const int right_w = w - playfield_x - MODERN_PLAYFIELD_W;

	modern_hud_panel_w = (left_w > 0 && right_w > 0) ? MIN(left_w, right_w) : 0;

	modern_set_hud_surface(&modern_hud_surfaces[0], &modern_hud_surface_w[0], left_w);
	modern_set_hud_surface(&modern_hud_surfaces[1], &modern_hud_surface_w[1], right_w);

	// The in-game message strip under the playfield only exists in panel mode.
	modern_set_message_surface(modern_hud_panel_w >= MODERN_HUD_MIN_PANEL_WIDTH ? MODERN_PLAYFIELD_W : 0);
}

// Allocates (or resizes) one HUD panel surface to `panel_w + MODERN_HUD_PANEL_PAD`
// x vga_height, zero-filled.  `panel_w <= 0` leaves the existing surface alone;
// modern_hud_in_panels() then reports false so it is never used.  Not called in
// the per-frame path.
static void modern_set_hud_surface(SDL_Surface **surface, int *stored_w, int panel_w)
{
	if (panel_w <= 0)
		return;

	const int w = panel_w + MODERN_HUD_PANEL_PAD;

	if (*surface != NULL && *stored_w == w)
		return;

	if (*surface != NULL)
		SDL_DestroySurface(*surface);

	*surface = SDL_CreateSurface(w, vga_height, SDL_PIXELFORMAT_INDEX8);
	if (*surface == NULL)
	{
		logFatal("Failed to allocate the modern HUD panel surface (%dx%d): %s", w, vga_height, SDL_GetError());
		exit(EXIT_FAILURE);
	}

	memset((*surface)->pixels, 0, (size_t)(*surface)->pitch * (size_t)(*surface)->h);
	*stored_w = w;
}

// Allocates (or frees) the message strip surface.  `playfield_w <= 0` frees it
// (no panel mode); otherwise it is playfield_w + MODERN_HUD_PANEL_PAD wide by
// MODERN_MESSAGE_H rows, zero-filled (index 0 transparent).  Not called in the
// per-frame path.
static void modern_set_message_surface(int playfield_w)
{
	if (playfield_w <= 0)
	{
		if (modern_message_surface != NULL)
		{
			SDL_DestroySurface(modern_message_surface);
			modern_message_surface = NULL;
		}
		modern_message_surface_w = 0;
		return;
	}

	const int w = playfield_w + MODERN_HUD_PANEL_PAD;

	if (modern_message_surface != NULL && modern_message_surface_w == w)
		return;

	if (modern_message_surface != NULL)
		SDL_DestroySurface(modern_message_surface);

	modern_message_surface = SDL_CreateSurface(w, MODERN_MESSAGE_H, SDL_PIXELFORMAT_INDEX8);
	if (modern_message_surface == NULL)
	{
		logFatal("Failed to allocate the modern message surface (%dx%d): %s", w, MODERN_MESSAGE_H, SDL_GetError());
		exit(EXIT_FAILURE);
	}

	memset(modern_message_surface->pixels, 0, (size_t)modern_message_surface->pitch * (size_t)modern_message_surface->h);
	modern_message_surface_w = w;
}

bool modern_hud_in_panels(void)
{
	return presentation == PRESENTATION_MODERN &&
	       modern_hud_panel_w >= MODERN_HUD_MIN_PANEL_WIDTH &&
	       modern_hud_surfaces[0] != NULL && modern_hud_surfaces[1] != NULL;
}

int modern_side_panel_width(void)
{
	return modern_hud_in_panels() ? modern_hud_panel_w : 0;
}

SDL_Surface *modern_hud_surface(int player)
{
	if (player < 0 || player >= 2 || !modern_hud_in_panels())
		return NULL;

	return modern_hud_surfaces[player];
}

SDL_Surface *modern_hud_message_surface(void)
{
	return modern_hud_in_panels() ? modern_message_surface : NULL;
}

void modern_message_set(const char *text)
{
	if (text == NULL)
	{
		modern_message[0] = '\0';
		return;
	}

	snprintf(modern_message, sizeof modern_message, "%s", text);
}

void modern_message_clear(void)
{
	modern_message[0] = '\0';
}

const char *modern_message_text(void)
{
	return modern_message;
}

void modern_hud_begin_frame(void)
{
	if (!modern_hud_in_panels())
		return;

	for (int i = 0; i < 2; ++i)
	{
		SDL_Surface *surface = modern_hud_surfaces[i];
		memset(surface->pixels, 0, (size_t)surface->pitch * (size_t)surface->h);
	}
}

void modern_init(void)
{
	// The bloom + dynamic-light pass lives in modern_bloom.c; it is the only
	// registered effect pass.  It reads the bloom/lighting settings itself.
	modern_register_pass(modern_bloom_pass);

	modern_ready = true;
	modern_update_canvas_size();
}

void modern_update_canvas_size(void)
{
	if (!modern_ready)
		return;

	int win_w = 0, win_h = 0;
	SDL_GetWindowSize(main_window, &win_w, &win_h);
	if (win_w <= 0 || win_h <= 0)
	{
		win_w = vga_width;
		win_h = vga_height;
	}

	// width = round(200 * pixel_aspect * target_aspect), never below 320 so the
	// frame always fits.  "auto" follows the window aspect.
	const float pixel_aspect = modern_pixel_aspect_factor();
	const float target_aspect = modern_aspect == MODERN_ASPECT_AUTO
		? (float)win_w / (float)win_h
		: modern_aspect_ratios[modern_aspect];

	int width = (int)lroundf((float)vga_height * pixel_aspect * target_aspect);
	if (width < vga_width)
		width = vga_width;

	modern_set_canvas_size(width, vga_height);
}

void modern_deinit(void)
{
	modern_ready = false;

	if (modern_texture != NULL)
	{
		SDL_DestroyTexture(modern_texture);
		modern_texture = NULL;
	}

	free(modern_panel_scratch);
	modern_panel_scratch = NULL;

	free(modern_panel_scale_scratch);
	modern_panel_scale_scratch = NULL;

	free(modern_blur_low);
	modern_blur_low = NULL;

	free(modern_blur_xmap);
	modern_blur_xmap = NULL;

	if (modern_message_surface != NULL)
	{
		SDL_DestroySurface(modern_message_surface);
		modern_message_surface = NULL;
	}
	modern_message_surface_w = 0;

	for (int i = 0; i < 2; ++i)
	{
		if (modern_hud_surfaces[i] != NULL)
		{
			SDL_DestroySurface(modern_hud_surfaces[i]);
			modern_hud_surfaces[i] = NULL;
		}
		modern_hud_surface_w[i] = 0;
	}
	modern_hud_panel_w = 0;

	free(modern_frame_state.pixels);
	modern_frame_state.pixels = NULL;
	modern_frame_state.w = 0;
	modern_frame_state.h = 0;
	modern_frame_state.pitch = 0;
	modern_frame_state.src = NULL;
	modern_frame_state.src_pitch = 0;
	modern_frame_state.palette = NULL;
	modern_frame_state.gameplay = false;
	modern_frame_state.content_offset_x = 0;
}

void modern_build_frame(SDL_Surface *src_surface)
{
	assert(SDL_BITSPERPIXEL(src_surface->format) == 8);

	ModernFrame *frame = &modern_frame_state;
	assert(frame->pixels != NULL);

	frame->src = src_surface->pixels;
	frame->src_pitch = src_surface->pitch;
	frame->palette = get_active_palette();

	const bool gameplay = modern_gameplay_frame;
	modern_gameplay_frame = false;

	if (gameplay && modern_hud_in_panels())
	{
		// Panel mode: copy only the playfield rectangle (the original sidebar and
		// bottom strip are dropped) and lay the HUD out in the freed columns and
		// the message strip below the playfield.
		const int playfield_x = (frame->w - MODERN_PLAYFIELD_W) / 2;
		const int copy_w = MIN((int)src_surface->w, MODERN_PLAYFIELD_W);
		const int copy_h = MIN(MIN((int)src_surface->h, frame->h), MODERN_PLAYFIELD_H);

		for (int y = 0; y < copy_h; ++y)
		{
			const Uint8 *src = frame->src + (size_t)y * frame->src_pitch;
			Uint32 *dst = frame->pixels + (size_t)y * frame->w + playfield_x;

			for (int x = 0; x < copy_w; ++x)
				dst[x] = rgb_palette[src[x]];
		}

		const int right_x = playfield_x + MODERN_PLAYFIELD_W;
		modern_fill_side_panels(frame, playfield_x, playfield_x + MODERN_PLAYFIELD_W - 1,
		                        playfield_x, right_x, frame->w - right_x);

		modern_composite_hud(frame, playfield_x);
		modern_composite_message(frame, playfield_x);

		// Mouse mapping follows the playfield on gameplay frames.
		modern_frame_offset_x = playfield_x;
		modern_frame_offset_y = 0;
	}
	else
	{
		// Fallback and non-gameplay frames: the full 320x200 frame stays centred.
		// Gameplay falls back to the ambilight (as before); non-gameplay frames
		// get the blurred background fill.
		const int copy_w = MIN((int)src_surface->w, frame->w);
		const int copy_h = MIN((int)src_surface->h, frame->h);
		const int offset_x = (frame->w - copy_w) / 2;

		for (int y = 0; y < copy_h; ++y)
		{
			const Uint8 *src = frame->src + (size_t)y * frame->src_pitch;
			Uint32 *dst = frame->pixels + (size_t)y * frame->w + offset_x;

			for (int x = 0; x < copy_w; ++x)
				dst[x] = rgb_palette[src[x]];
		}

		if (offset_x > 0)
		{
			const int right_x = offset_x + vga_width;
			const int right_width = frame->w - right_x;

			if (gameplay)
			{
				// Ambilight sampled from the playfield edges, skipping the HUD
				// sidebar (playfield column 263) as before.
				modern_fill_side_panels(frame, offset_x, offset_x + (vga_width - 1 - 56),
				                        offset_x, right_x, right_width);
			}
			else
			{
				modern_fill_blurred_background(frame, offset_x);
			}
		}

		modern_frame_offset_x = offset_x;
		modern_frame_offset_y = 0;
	}

	// Context for the effect passes: a gameplay frame exposes the playfield
	// rectangle (its left edge is the content offset on those frames).
	frame->gameplay = gameplay;
	frame->content_offset_x = modern_frame_offset_x;

	for (size_t i = 0; i < modern_passes_count; ++i)
		modern_passes[i](frame);
}

// Copies the non-transparent pixels of one HUD panel surface over the canvas
// side region starting at `dst_x`.  `panel_w` is the visible panel width; the
// surface's extra MODERN_HUD_PANEL_PAD columns are never composited.
static void modern_blit_hud_surface(const ModernFrame *frame, const SDL_Surface *hud, int dst_x, int panel_w)
{
	const int rows = MIN(frame->h, hud->h);
	const int cols = MIN(panel_w, hud->w);

	for (int y = 0; y < rows; ++y)
	{
		const Uint8 *src = (const Uint8 *)hud->pixels + (size_t)y * hud->pitch;
		Uint32 *dst = frame->pixels + (size_t)y * frame->w + dst_x;

		for (int x = 0; x < cols; ++x)
		{
			if (src[x] != 0)
				dst[x] = rgb_palette[src[x]];
		}
	}
}

static void modern_composite_hud(ModernFrame *frame, int frame_x)
{
	if (!modern_hud_in_panels())
		return;

	const int right_x = frame_x + MODERN_PLAYFIELD_W;

	modern_blit_hud_surface(frame, modern_hud_surfaces[0], 0, frame_x);
	modern_blit_hud_surface(frame, modern_hud_surfaces[1], right_x, frame->w - right_x);
}

// Composites the opaque message strip (drawn by modern_hud.c) into the 16 rows
// under the playfield.  Unlike the HUD panels, the strip writes every pixel
// (its background is index 0), so it is an opaque info bar.
static void modern_composite_message(ModernFrame *frame, int frame_x)
{
	const SDL_Surface *strip = modern_hud_message_surface();
	if (strip == NULL)
		return;

	const int cols = MIN(MODERN_PLAYFIELD_W, strip->w);
	const int rows = MIN(MODERN_MESSAGE_H, MIN(strip->h, frame->h - MODERN_PLAYFIELD_H));

	for (int y = 0; y < rows; ++y)
	{
		const Uint8 *src = (const Uint8 *)strip->pixels + (size_t)y * strip->pitch;
		Uint32 *dst = frame->pixels + (size_t)(MODERN_PLAYFIELD_H + y) * frame->w + frame_x;

		for (int x = 0; x < cols; ++x)
			dst[x] = rgb_palette[src[x]];
	}
}

// One faded side-panel pixel.  `scale` is the precomputed Q32 fade factor:
// strongly darkened (40% peak at the playfield edge) with a quadratic falloff
// towards the outer edge.  Fixed point keeps it deterministic and division-free
// in the per-pixel path.
static Uint32 modern_panel_pixel(int r, int g, int b, Uint64 scale)
{
	r = (int)(((Uint64)r * scale) >> 32);
	g = (int)(((Uint64)g * scale) >> 32);
	b = (int)(((Uint64)b * scale) >> 32);

	return ((Uint32)(Uint8)r << 16) | ((Uint32)(Uint8)g << 8) | (Uint32)(Uint8)b;
}

// Q32 fade factor for a column at distance `d` from the playfield edge
// (d == 0 at the edge, panel_width - 1 at the outer edge).  The 40% peak is
// 2/5; the falloff is quadratic.
static Uint64 modern_panel_scale(int panel_width, int d)
{
	const Uint64 num = (Uint64)(2 * (panel_width - d) * (panel_width - d));
	const Uint64 den = (Uint64)(5 * panel_width * panel_width);

	return (num << 32) / den;
}

// Procedural "ambilight" side panels.  Deterministic and read-only over the
// already-converted canvas: the left panel samples `left_edge`, the right panel
// `right_edge` (the playfield's column 263 during gameplay, which skips the HUD
// sidebar).  It never reads game_screen, so it cannot reveal the off-screen
// margins.  The left panel fills x[0..left_width); the right fills
// x[right_x..right_x+right_width).
static void modern_fill_side_panels(ModernFrame *frame, int left_edge, int right_edge,
                                    int left_width, int right_x, int right_width)
{
	const int w = frame->w, h = frame->h;

	if (left_width <= 0 && right_width <= 0)
		return;

	const int sample = 4;
	const int lx0 = left_edge;
	const int lx1 = MIN(left_edge + sample - 1, w - 1);
	const int rx0 = MAX(right_edge - (sample - 1), 0);
	const int rx1 = right_edge;
	const int lcount = lx1 - lx0 + 1;
	const int rcount = rx1 - rx0 + 1;

	Uint8 *left_pre   = modern_panel_scratch;
	Uint8 *right_pre  = modern_panel_scratch + (size_t)h * 3;
	Uint8 *left_blur  = modern_panel_scratch + (size_t)h * 6;
	Uint8 *right_blur = modern_panel_scratch + (size_t)h * 9;

	// Per-row average of the few sampled edge columns.
	for (int y = 0; y < h; ++y)
	{
		const Uint32 *row = frame->pixels + (size_t)y * frame->w;
		int lr = 0, lg = 0, lb = 0, rr = 0, rg = 0, rb = 0;

		for (int x = lx0; x <= lx1; ++x)
		{
			const Uint32 p = row[x];
			lr += (p >> 16) & 0xff;
			lg += (p >> 8) & 0xff;
			lb += p & 0xff;
		}
		for (int x = rx0; x <= rx1; ++x)
		{
			const Uint32 p = row[x];
			rr += (p >> 16) & 0xff;
			rg += (p >> 8) & 0xff;
			rb += p & 0xff;
		}

		left_pre[y * 3 + 0] = (Uint8)(lr / lcount);
		left_pre[y * 3 + 1] = (Uint8)(lg / lcount);
		left_pre[y * 3 + 2] = (Uint8)(lb / lcount);
		right_pre[y * 3 + 0] = (Uint8)(rr / rcount);
		right_pre[y * 3 + 1] = (Uint8)(rg / rcount);
		right_pre[y * 3 + 2] = (Uint8)(rb / rcount);
	}

	// Vertical box blur so the panels blend over a few rows.
	const int blur = 8;
	for (int y = 0; y < h; ++y)
	{
		const int y0 = MAX(0, y - blur);
		const int y1 = MIN(h - 1, y + blur);
		const int n = y1 - y0 + 1;
		int lr = 0, lg = 0, lb = 0, rr = 0, rg = 0, rb = 0;

		for (int k = y0; k <= y1; ++k)
		{
			lr += left_pre[k * 3 + 0];
			lg += left_pre[k * 3 + 1];
			lb += left_pre[k * 3 + 2];
			rr += right_pre[k * 3 + 0];
			rg += right_pre[k * 3 + 1];
			rb += right_pre[k * 3 + 2];
		}

		left_blur[y * 3 + 0] = (Uint8)(lr / n);
		left_blur[y * 3 + 1] = (Uint8)(lg / n);
		left_blur[y * 3 + 2] = (Uint8)(lb / n);
		right_blur[y * 3 + 0] = (Uint8)(rr / n);
		right_blur[y * 3 + 1] = (Uint8)(rg / n);
		right_blur[y * 3 + 2] = (Uint8)(rb / n);
	}

	// Fill the panels, fading towards their outer edges.  The per-column fade
	// factors are precomputed once, so the per-pixel work is multiply + shift.
	for (int x = 0; x < left_width; ++x)
		modern_panel_scale_scratch[x] = modern_panel_scale(left_width, left_width - 1 - x);

	for (int y = 0; y < h; ++y)
	{
		Uint32 *row = frame->pixels + (size_t)y * frame->w;
		const int lr = left_blur[y * 3 + 0],
		          lg = left_blur[y * 3 + 1],
		          lb = left_blur[y * 3 + 2];

		for (int x = 0; x < left_width; ++x)
			row[x] = modern_panel_pixel(lr, lg, lb, modern_panel_scale_scratch[x]);
	}

	for (int x = 0; x < right_width; ++x)
		modern_panel_scale_scratch[x] = modern_panel_scale(right_width, x);

	for (int y = 0; y < h; ++y)
	{
		Uint32 *row = frame->pixels + (size_t)y * frame->w;
		const int rr = right_blur[y * 3 + 0],
		          rg = right_blur[y * 3 + 1],
		          rb = right_blur[y * 3 + 2];

		for (int x = 0; x < right_width; ++x)
			row[right_x + x] = modern_panel_pixel(rr, rg, rb, modern_panel_scale_scratch[x]);
	}
}

// Blurred, darkened background fill for non-gameplay frames (title, menus,
// story/text screens, the in-game Esc menu...).  The already-converted 320x200
// frame is box-averaged down to a small grid and bilinearly stretched back over
// the whole canvas width, then darkened, so the side regions read as a soft
// extension of the screen rather than a flat gradient.  Deterministic integer
// math; no allocation in the per-frame path; only used when the canvas is wider
// than 320 (4:3 has no side regions and is unchanged).
static void modern_fill_blurred_background(ModernFrame *frame, int frame_x)
{
	const int w = frame->w, h = frame->h;
	const int left_width = frame_x;
	const int right_x = frame_x + vga_width;
	const int right_width = w - right_x;

	if (left_width <= 0 && right_width <= 0)
		return;

	// Downsample the frame region [frame_x, frame_x + vga_width) x [0, h) into
	// the low-res grid.  Block averaging is itself a heavy box blur.
	for (int by = 0; by < MODERN_BLUR_LOW_H; ++by)
	{
		const int y0 = by * h / MODERN_BLUR_LOW_H;
		const int y1 = MAX(y0 + 1, (by + 1) * h / MODERN_BLUR_LOW_H);

		for (int bx = 0; bx < MODERN_BLUR_LOW_W; ++bx)
		{
			const int x0 = frame_x + bx * vga_width / MODERN_BLUR_LOW_W;
			const int x1 = MAX(x0 + 1, frame_x + (bx + 1) * vga_width / MODERN_BLUR_LOW_W);

			unsigned long r = 0, g = 0, b = 0, n = 0;
			for (int y = y0; y < y1; ++y)
			{
				const Uint32 *row = frame->pixels + (size_t)y * frame->w;
				for (int x = x0; x < x1; ++x)
				{
					const Uint32 p = row[x];
					r += (p >> 16) & 0xff;
					g += (p >> 8) & 0xff;
					b += p & 0xff;
					++n;
				}
			}

			Uint8 *cell = modern_blur_low + ((size_t)by * MODERN_BLUR_LOW_W + bx) * 3;
			cell[0] = (Uint8)(r / n);
			cell[1] = (Uint8)(g / n);
			cell[2] = (Uint8)(b / n);
		}
	}

	// Per-canvas-column source coordinate (Q16) for the horizontal stretch.
	for (int x = 0; x < w; ++x)
		modern_blur_xmap[x] = (Uint32)(((Uint64)x * (MODERN_BLUR_LOW_W - 1) * 65536) / (w > 1 ? w - 1 : 1));

	// Bilinear stretch + darken over the two side regions.  `dark` is 0.34.
	const int dark = 88;
	for (int y = 0; y < h; ++y)
	{
		const Uint32 uy = (Uint32)(((Uint64)y * (MODERN_BLUR_LOW_H - 1) * 65536) / (h > 1 ? h - 1 : 1));
		const int iy = (int)(uy >> 16);
		const int fy = (int)((uy >> 8) & 0xff);
		const Uint8 *row0 = modern_blur_low + (size_t)iy * MODERN_BLUR_LOW_W * 3;
		const Uint8 *row1 = modern_blur_low + (size_t)MIN(iy + 1, MODERN_BLUR_LOW_H - 1) * MODERN_BLUR_LOW_W * 3;
		Uint32 *dst = frame->pixels + (size_t)y * frame->w;

		for (int pass = 0; pass < 2; ++pass)
		{
			const int x_begin = (pass == 0) ? 0 : right_x;
			const int x_end   = (pass == 0) ? left_width : right_x + right_width;

			for (int x = x_begin; x < x_end; ++x)
			{
				const Uint32 ux = modern_blur_xmap[x];
				const int ix = (int)(ux >> 16);
				const int fx = (int)((ux >> 8) & 0xff);
				const int ix1 = MIN(ix + 1, MODERN_BLUR_LOW_W - 1);
				const Uint8 *c00 = row0 + (size_t)ix * 3;
				const Uint8 *c10 = row0 + (size_t)ix1 * 3;
				const Uint8 *c01 = row1 + (size_t)ix * 3;
				const Uint8 *c11 = row1 + (size_t)ix1 * 3;

				const int wx0 = 256 - fx, wy0 = 256 - fy;
				const int w00 = wx0 * wy0, w10 = fx * wy0, w01 = wx0 * fy, w11 = fx * fy;

				const int r = (((c00[0] * w00 + c10[0] * w10 + c01[0] * w01 + c11[0] * w11) >> 16) * dark) >> 8;
				const int g = (((c00[1] * w00 + c10[1] * w10 + c01[1] * w01 + c11[1] * w11) >> 16) * dark) >> 8;
				const int b = (((c00[2] * w00 + c10[2] * w10 + c01[2] * w01 + c11[2] * w11) >> 16) * dark) >> 8;

				dst[x] = ((Uint32)(Uint8)r << 16) | ((Uint32)(Uint8)g << 8) | (Uint32)(Uint8)b;
			}
		}
	}
}

// Chooses where the canvas lands in the window, honoring the pixel aspect.
//
// The canvas' natural aspect already contains the pixel-aspect factor
// (width = 200 * pixel_aspect * target_aspect), so the on-screen aspect of the
// content is canvas_aspect / pixel_aspect; fitting the window at that aspect
// gives every canvas pixel the requested shape (sy/sx == pixel_aspect).
//  * Integer uses independent integer factors per axis: sy is the largest that
//    fits the window height, sx is the integer closest to sy / pixel_aspect
//    (at least 1).  Square keeps sx == sy.
//  * The Fit modes scale proportionally to fill the window at the content
//    aspect.
//  * Center stays the raw 1:1 canvas, as it was before this task.
static void modern_calc_dst_rect(const ModernFrame *frame, SDL_Rect *dst_rect)
{
	int win_w = 0, win_h = 0;
	SDL_GetWindowSize(main_window, &win_w, &win_h);
	if (win_w <= 0 || win_h <= 0)
	{
		win_w = vga_width;
		win_h = vga_height;
	}

	const float pixel_aspect = modern_pixel_aspect_factor();
	const float content_aspect = ((float)frame->w / (float)frame->h) / pixel_aspect;

	switch (scaling_mode)
	{
	case SCALE_CENTER:
		dst_rect->w = frame->w;
		dst_rect->h = frame->h;
		break;
	case SCALE_INTEGER:
	{
		int sx, sy;

		if (pixel_aspect == 1.0f)
		{
			sy = win_h / frame->h;
			sx = win_w / frame->w;
			if (sx < sy)
				sy = sx;
		}
		else
		{
			sy = win_h / frame->h;
			sx = (int)floorf((float)sy / pixel_aspect + 0.5f);
			if (sx < 1)
				sx = 1;

			// Keep the output inside the window when the window is narrower
			// than the content aspect (this only ever lowers sy).
			while (sy > 1 && frame->w * sx > win_w)
			{
				--sy;
				sx = (int)floorf((float)sy / pixel_aspect + 0.5f);
				if (sx < 1)
					sx = 1;
			}
		}

		if (sy < 1)
			sy = 1;

		dst_rect->w = frame->w * sx;
		dst_rect->h = frame->h * sy;
		break;
	}
	default:  // SCALE_ASPECT_8_5 / SCALE_ASPECT_4_3 (the Fit modes)
	{
		const float maxh_width = win_h * content_aspect;
		const float maxw_height = win_w / content_aspect;

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = (int)maxw_height;
		}
		else
		{
			dst_rect->w = (int)maxh_width;
			dst_rect->h = win_h;
		}
		break;
	}
	}

	dst_rect->x = (win_w - dst_rect->w) / 2;
	dst_rect->y = (win_h - dst_rect->h) / 2;
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
	modern_calc_dst_rect(frame, &dst_rect);

	SDL_Renderer *renderer = video_renderer();
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	const SDL_FRect dst_frect = { (float)dst_rect.x, (float)dst_rect.y, (float)dst_rect.w, (float)dst_rect.h };
	SDL_RenderTexture(renderer, modern_texture, NULL, &dst_frect);
	SDL_RenderPresent(renderer);

	// Mouse mapping needs the canvas size and the offset of the game content
	// inside it (the playfield offset on gameplay frames in panel mode, the
	// 320x200 frame offset otherwise); both are recorded by modern_build_frame.
	video_set_last_output_rect_ex(&dst_rect, frame->w, frame->h,
	                              modern_frame_offset_x, modern_frame_offset_y);
}

const ModernFrame *modern_current_frame(void)
{
	return &modern_frame_state;
}
