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

static void modern_fill_side_panels(ModernFrame *frame, int frame_x, bool gameplay);
static void modern_composite_hud(ModernFrame *frame, int frame_x);
static void modern_set_hud_surface(SDL_Surface **surface, int *stored_w, int panel_w);

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

	// Side-panel HUD surfaces.  The frame is centered, so the two panels are
	// frame_x and w - frame_x - vga_width wide; only their common (narrower)
	// width matters for the layout checks.  With no panels this frees nothing
	// but makes modern_hud_in_panels() false.
	const int frame_x = (w - vga_width) / 2;
	const int left_w = frame_x;
	const int right_w = w - frame_x - vga_width;

	modern_hud_panel_w = (left_w > 0 && right_w > 0) ? MIN(left_w, right_w) : 0;

	modern_set_hud_surface(&modern_hud_surfaces[0], &modern_hud_surface_w[0], left_w);
	modern_set_hud_surface(&modern_hud_surfaces[1], &modern_hud_surface_w[1], right_w);
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
}

void modern_build_frame(SDL_Surface *src_surface)
{
	assert(SDL_BITSPERPIXEL(src_surface->format) == 8);

	ModernFrame *frame = &modern_frame_state;
	assert(frame->pixels != NULL);

	frame->src = src_surface->pixels;
	frame->src_pitch = src_surface->pitch;
	frame->palette = get_active_palette();

	// Convert the 8-bit frame through the very same XRGB words the Classic
	// "None" software scaler writes.  The frame is placed centered horizontally
	// in the canvas; the side panels fill the rest.  With a 320-wide canvas
	// (the default 4:3 / original combination) this is pixel-for-pixel identical
	// to the previous Modern output.
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

	modern_fill_side_panels(frame, offset_x, modern_gameplay_frame);

	// Gameplay frames in panel mode also carry the relocated HUD in the two
	// off-screen surfaces; composite it over the ambilight (index 0 stays).
	if (modern_gameplay_frame)
		modern_composite_hud(frame, offset_x);

	modern_gameplay_frame = false;

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

	const int right_x = frame_x + vga_width;

	modern_blit_hud_surface(frame, modern_hud_surfaces[0], 0, frame_x);
	modern_blit_hud_surface(frame, modern_hud_surfaces[1], right_x, frame->w - right_x);
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
// already-converted canvas: the left panel samples the frame's column 0, the
// right panel column 319 (or the playfield's column 263 during gameplay, which
// skips the HUD sidebar).  It never reads game_screen, so it cannot reveal the
// off-screen margins.
static void modern_fill_side_panels(ModernFrame *frame, int frame_x, bool gameplay)
{
	const int w = frame->w, h = frame->h;
	const int left_width = frame_x;
	const int right_x = frame_x + vga_width;
	const int right_width = w - right_x;

	if (left_width <= 0 && right_width <= 0)
		return;

	const int left_edge = frame_x;
	const int right_edge = gameplay ? frame_x + (vga_width - 1 - 56)  // playfield column 263
	                                : frame_x + (vga_width - 1);

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

	// The game frame sits centered in the wider canvas, so mouse mapping needs
	// the canvas size and the frame offset, not the canvas alone.
	video_set_last_output_rect_ex(&dst_rect, frame->w, frame->h, (frame->w - vga_width) / 2, 0);
}

const ModernFrame *modern_current_frame(void)
{
	return &modern_frame_state;
}
