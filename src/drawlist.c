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
#include "drawlist.h"

#include "backgrnd.h"
#include "logging.h"
#include "vga256d.h"
#include "video.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Per-tick draw list, stages 1-2.  See drawlist.h for the contract.

// Command buffering.  Fixed capacity allocated once; no per-frame allocation.
#define DL_MAX_COMMANDS  (1u << 15)   // 32768 entries, comfortably above one tick
#define DL_PAYLOAD_BYTES (1u << 18)   // 256 KiB bulk payload arena per tick

enum
{
	DL_FILL_FULL = 0,
	DL_FILL_RECT,
	DL_RECT_OUTLINE,
	DL_BG_ROW,
	DL_BG_ROW_BLEND,
	DL_BLIT_SPRITE,
	DL_BLIT_SPRITE2,
	DL_DARKEN,
	DL_FILTER_SCREEN,
	DL_FILTER,
	DL_STARFIELD,
	DL_SUPERPIXELS,
	DL_KIND_COUNT,
};

typedef struct
{
	Uint16 kind;
	Uint8 variant;
	Uint8 surface;      // 1 = game_screen, 2 = VGAScreen2
	Uint8 src_surface;  // for DL_FILTER
	Uint8 obj_kind;
	Uint8 obj_id;
	Uint8 obj_sub;
	Uint8 color;
	Uint8 filter;
	Uint8 hue;
	Sint8 value;
	bool black;
	int x, y;           // destination / row origin
	int a, b, c, d;     // rect coords, table, index
	Uint8 **map;        // background row source
	Sprite2_array sheet;  // blit_sprite2 source
	Uint32 payload_off;
	Uint32 payload_len;
} DlCommand;

static DlCommand *dl_commands = NULL;
static Uint8 *dl_payload = NULL;
static Uint32 dl_count = 0;
static Uint32 dl_payload_used = 0;

static bool dl_enabled = false;
static bool dl_recording = false;
static bool dl_check = false;

static SDL_Surface *dl_scratch_game = NULL;
static SDL_Surface *dl_scratch_vga2 = NULL;

static int dl_context_kind = DL_OBJ_NONE;
static int dl_context_id = 0;
static int dl_context_sub = 0;

static unsigned long dl_checked = 0;
static unsigned long dl_mismatched = 0;
static char dl_first_mismatch[128] = "";
static char dl_first_mismatch_saved[128] = "";
static int dl_debug_diffs = 0;
static int dl_debug_total = 0;

bool drawlist_enabled(void)
{
	return dl_enabled;
}

bool drawlist_recording(void)
{
	return dl_recording;
}

void drawlist_set_enabled(bool enabled)
{
	dl_enabled = enabled;
}

void drawlist_set_check(bool check)
{
	dl_check = check;
}

static SDL_Surface *dl_scratch_for(int surface)
{
	switch (surface)
	{
	case 1:
		return dl_scratch_game;
	case 2:
		return dl_scratch_vga2;
	default:
		return NULL;
	}
}

static int dl_surface_of(const SDL_Surface *surface)
{
	if (surface == game_screen)
		return 1;
	if (surface == VGAScreen2)
		return 2;
	return 0;
}

void drawlist_init(void)
{
	if (dl_commands == NULL)
		dl_commands = malloc(sizeof(DlCommand) * DL_MAX_COMMANDS);
	if (dl_payload == NULL)
		dl_payload = malloc(DL_PAYLOAD_BYTES);

	if (dl_scratch_game == NULL)
		dl_scratch_game = SDL_CreateSurface(vga_width, vga_height, SDL_PIXELFORMAT_INDEX8);
	if (dl_scratch_vga2 == NULL)
		dl_scratch_vga2 = SDL_CreateSurface(vga_width, vga_height, SDL_PIXELFORMAT_INDEX8);

	// The scratch surfaces are written by code that assumes the engine pitch
	// (backgrnd.c/sprite.c), so they must match the real surfaces exactly.
	assert(dl_scratch_game != NULL && game_screen != NULL &&
	       dl_scratch_game->pitch == game_screen->pitch);
	assert(dl_scratch_vga2 != NULL && VGAScreen2 != NULL &&
	       dl_scratch_vga2->pitch == VGAScreen2->pitch);
}

void drawlist_shutdown(void)
{
	free(dl_commands);
	dl_commands = NULL;
	free(dl_payload);
	dl_payload = NULL;

	if (dl_scratch_game != NULL)
		SDL_DestroySurface(dl_scratch_game);
	dl_scratch_game = NULL;
	if (dl_scratch_vga2 != NULL)
		SDL_DestroySurface(dl_scratch_vga2);
	dl_scratch_vga2 = NULL;
}

void drawlist_set_context(int obj_kind, int obj_id, int obj_sub)
{
	dl_context_kind = obj_kind;
	dl_context_id = obj_id;
	dl_context_sub = obj_sub;
}

static void dl_copy_surface(SDL_Surface *dst, const SDL_Surface *src)
{
	const int row = MIN(dst->w, src->w);
	for (int y = 0; y < MIN(dst->h, src->h); ++y)
		memcpy((Uint8 *)dst->pixels + (size_t)y * dst->pitch,
		       (const Uint8 *)src->pixels + (size_t)y * src->pitch, (size_t)row);
}

void drawlist_frame_begin(void)
{
	if (!dl_enabled)
		return;

	// Deferred allocation: the scratch surfaces can only be sized once the
	// engine's surfaces exist (init_video runs after regress_init).
	if (dl_commands == NULL)
		drawlist_init();

	dl_count = 0;
	dl_payload_used = 0;
	dl_context_kind = DL_OBJ_NONE;
	dl_context_id = 0;
	dl_context_sub = 0;
	dl_recording = true;

	// Stateful filters (iced_blur_filter, blur_filter) blend with the current
	// destination contents, so the scratch must start from the same state the
	// engine's framebuffer has.  VGAScreen2 is always fully cleared before it is
	// read, but seed it too so replay never depends on recycling.
	if (game_screen != NULL && dl_scratch_game != NULL)
		dl_copy_surface(dl_scratch_game, game_screen);
	if (VGAScreen2 != NULL && dl_scratch_vga2 != NULL)
		dl_copy_surface(dl_scratch_vga2, VGAScreen2);
}

static DlCommand *dl_push(Uint16 kind)
{
	if (dl_count >= DL_MAX_COMMANDS)
	{
		// Should never happen; dropping commands keeps the run alive and the
		// replay check will report the resulting mismatch.
		return NULL;
	}

	DlCommand *c = &dl_commands[dl_count++];
	memset(c, 0, sizeof *c);
	c->kind = kind;
	c->obj_kind = (Uint8)dl_context_kind;
	c->obj_id = (Uint8)dl_context_id;
	c->obj_sub = (Uint8)dl_context_sub;
	return c;
}

static bool dl_push_payload(Uint32 *out_off, Uint32 *out_len, const void *data, size_t bytes)
{
	const Uint32 off = dl_payload_used;
	if (off + bytes > DL_PAYLOAD_BYTES || dl_payload == NULL)
		return false;
	memcpy(dl_payload + off, data, bytes);
	dl_payload_used += (Uint32)bytes;
	*out_off = off;
	*out_len = (Uint32)bytes;
	return true;
}

void drawlist_record_fill_full(SDL_Surface *surface)
{
	if (!dl_recording)
		return;
	const int sid = dl_surface_of(surface);
	if (sid == 0)
		return;

	DlCommand *c = dl_push(DL_FILL_FULL);
	if (c != NULL)
	{
		c->surface = (Uint8)sid;
		c->obj_kind = DL_OBJ_NONE;
	}
}

void drawlist_record_fill_rect(SDL_Surface *surface, int x, int y, int x2, int y2, Uint8 color)
{
	if (!dl_recording)
		return;
	const int sid = dl_surface_of(surface);
	if (sid == 0)
		return;

	DlCommand *c = dl_push(DL_FILL_RECT);
	if (c != NULL)
	{
		c->surface = (Uint8)sid;
		c->x = x; c->y = y; c->a = x2; c->b = y2;
		c->color = color;
		c->obj_kind = DL_OBJ_NONE;
	}
}

void drawlist_record_rect_outline(SDL_Surface *surface, int x, int y, int x2, int y2, Uint8 color)
{
	if (!dl_recording)
		return;
	const int sid = dl_surface_of(surface);
	if (sid == 0)
		return;

	DlCommand *c = dl_push(DL_RECT_OUTLINE);
	if (c != NULL)
	{
		c->surface = (Uint8)sid;
		c->x = x; c->y = y; c->a = x2; c->b = y2;
		c->color = color;
		c->obj_kind = DL_OBJ_NONE;
	}
}

void drawlist_record_bg_row(SDL_Surface *surface, int x, int y, Uint8 **map, bool blend)
{
	if (!dl_recording)
		return;
	const int sid = dl_surface_of(surface);
	if (sid == 0)
		return;

	DlCommand *c = dl_push(blend ? DL_BG_ROW_BLEND : DL_BG_ROW);
	if (c != NULL)
	{
		c->surface = (Uint8)sid;
		c->x = x; c->y = y;
		c->map = map;
	}
}

void drawlist_record_blit_sprite(SDL_Surface *surface, int x, int y,
                                 unsigned int table, unsigned int index,
                                 int variant, Uint8 hue, Sint8 value, bool black)
{
	if (!dl_recording)
		return;
	const int sid = dl_surface_of(surface);
	if (sid == 0)
		return;

	DlCommand *c = dl_push(DL_BLIT_SPRITE);
	if (c != NULL)
	{
		c->surface = (Uint8)sid;
		c->variant = (Uint8)variant;
		c->x = x; c->y = y;
		c->a = (int)table; c->b = (int)index;
		c->hue = hue; c->value = value; c->black = black;
	}
}

void drawlist_record_blit_sprite2(SDL_Surface *surface, int x, int y,
                                  Sprite2_array sheet, unsigned int index,
                                  int variant, Uint8 filter)
{
	if (!dl_recording)
		return;
	const int sid = dl_surface_of(surface);
	if (sid == 0)
		return;

	DlCommand *c = dl_push(DL_BLIT_SPRITE2);
	if (c != NULL)
	{
		c->surface = (Uint8)sid;
		c->variant = (Uint8)variant;
		c->x = x; c->y = y;
		c->sheet = sheet;
		c->b = (int)index;
		c->filter = filter;
	}
}

void drawlist_record_darken(SDL_Surface *surface, JE_word neat)
{
	if (!dl_recording)
		return;
	const int sid = dl_surface_of(surface);
	if (sid == 0)
		return;

	DlCommand *c = dl_push(DL_DARKEN);
	if (c != NULL)
	{
		c->surface = (Uint8)sid;
		c->a = neat;
		c->obj_kind = DL_OBJ_NONE;
	}
}

void drawlist_record_filter_screen(SDL_Surface *surface, JE_shortint col, JE_shortint int_)
{
	if (!dl_recording)
		return;
	const int sid = dl_surface_of(surface);
	if (sid == 0)
		return;

	DlCommand *c = dl_push(DL_FILTER_SCREEN);
	if (c != NULL)
	{
		c->surface = (Uint8)sid;
		c->a = col; c->b = int_;
		c->obj_kind = DL_OBJ_NONE;
	}
}

void drawlist_record_filter(SDL_Surface *dst, SDL_Surface *src, int kind)
{
	if (!dl_recording)
		return;
	const int dst_sid = dl_surface_of(dst);
	const int src_sid = dl_surface_of(src);
	if (dst_sid == 0 || src_sid == 0)
		return;

	DlCommand *c = dl_push(DL_FILTER);
	if (c != NULL)
	{
		c->surface = (Uint8)dst_sid;
		c->src_surface = (Uint8)src_sid;
		c->variant = (Uint8)kind;
		c->obj_kind = DL_OBJ_NONE;
	}
}

void drawlist_record_starfield(SDL_Surface *surface, int move_speed, const void *stars, size_t bytes)
{
	if (!dl_recording)
		return;
	const int sid = dl_surface_of(surface);
	if (sid == 0)
		return;

	DlCommand *c = dl_push(DL_STARFIELD);
	if (c == NULL)
		return;
	c->surface = (Uint8)sid;
	c->a = move_speed;
	c->obj_kind = DL_OBJ_STARFIELD;
	if (!dl_push_payload(&c->payload_off, &c->payload_len, stars, bytes))
		c->kind = DL_KIND_COUNT;  // mark unusable
}

void drawlist_record_superpixels(SDL_Surface *surface, const void *superpixels, size_t bytes)
{
	if (!dl_recording)
		return;
	const int sid = dl_surface_of(surface);
	if (sid == 0)
		return;

	DlCommand *c = dl_push(DL_SUPERPIXELS);
	if (c == NULL)
		return;
	c->surface = (Uint8)sid;
	c->obj_kind = DL_OBJ_SUPERPIXEL;
	if (!dl_push_payload(&c->payload_off, &c->payload_len, superpixels, bytes))
		c->kind = DL_KIND_COUNT;
}

static void dl_replay_command(const DlCommand *c)
{
	SDL_Surface *surface = dl_scratch_for(c->surface);
	if (surface == NULL)
		return;

	switch (c->kind)
	{
	case DL_FILL_FULL:
		SDL_FillSurfaceRect(surface, NULL, 0);
		break;

	case DL_FILL_RECT:
	{
		SDL_Rect rect = { c->x, c->y, c->a - c->x + 1, c->b - c->y + 1 };
		SDL_FillSurfaceRect(surface, &rect, c->color);
		break;
	}

	case DL_RECT_OUTLINE:
		JE_rectangle(surface, c->x, c->y, c->a, c->b, c->color);
		break;

	case DL_BG_ROW:
		blit_background_row(surface, c->x, c->y, c->map);
		break;
	case DL_BG_ROW_BLEND:
		blit_background_row_blend(surface, c->x, c->y, c->map);
		break;

	case DL_BLIT_SPRITE:
		switch (c->variant)
		{
		case DL_SPRITE_BLIT:
			blit_sprite(surface, c->x, c->y, c->a, c->b);
			break;
		case DL_SPRITE_BLEND:
			blit_sprite_blend(surface, c->x, c->y, c->a, c->b);
			break;
		case DL_SPRITE_HV_UNSAFE:
			blit_sprite_hv_unsafe(surface, c->x, c->y, c->a, c->b, c->hue, c->value);
			break;
		case DL_SPRITE_HV:
			blit_sprite_hv(surface, c->x, c->y, c->a, c->b, c->hue, c->value);
			break;
		case DL_SPRITE_HV_BLEND:
			blit_sprite_hv_blend(surface, c->x, c->y, c->a, c->b, c->hue, c->value);
			break;
		case DL_SPRITE_DARK:
			blit_sprite_dark(surface, c->x, c->y, c->a, c->b, c->black);
			break;
		default:
			break;
		}
		break;

	case DL_BLIT_SPRITE2:
		switch (c->variant)
		{
		case DL_SPRITE2_BLIT:
			blit_sprite2(surface, c->x, c->y, c->sheet, c->b);
			break;
		case DL_SPRITE2_CLIP:
			blit_sprite2_clip(surface, c->x, c->y, c->sheet, c->b);
			break;
		case DL_SPRITE2_BLEND:
			blit_sprite2_blend(surface, c->x, c->y, c->sheet, c->b);
			break;
		case DL_SPRITE2_DARKEN:
			blit_sprite2_darken(surface, c->x, c->y, c->sheet, c->b);
			break;
		case DL_SPRITE2_FILTER:
			blit_sprite2_filter(surface, c->x, c->y, c->sheet, c->b, c->filter);
			break;
		case DL_SPRITE2_FILTER_CLIP:
			blit_sprite2_filter_clip(surface, c->x, c->y, c->sheet, c->b, c->filter);
			break;
		default:
			break;
		}
		break;

	case DL_DARKEN:
		drawlist_apply_darken(surface, c->a);
		break;

	case DL_FILTER_SCREEN:
		drawlist_apply_filter_screen(surface, c->a, c->b);
		break;

	case DL_FILTER:
	{
		SDL_Surface *src = dl_scratch_for(c->src_surface);
		if (src == NULL)
			break;
		switch (c->variant)
		{
		case DL_FILTER_LAVA: lava_filter(surface, src);  break;
		case DL_FILTER_WATER: water_filter(surface, src); break;
		case DL_FILTER_ICED: iced_blur_filter(surface, src); break;
		case DL_FILTER_BLUR: blur_filter(surface, src);  break;
		default: break;
		}
		break;
	}

	case DL_STARFIELD:
		drawlist_replay_starfield(surface, c->a, dl_payload + c->payload_off, c->payload_len);
		break;

	case DL_SUPERPIXELS:
		drawlist_replay_superpixels(surface, dl_payload + c->payload_off, c->payload_len);
		break;

	default:
		break;
	}
}

// Compares the replayed game_screen with the real one over every byte of every
// row.  Reports the first mismatching frame and pixel.
static bool dl_compare_frames(void)
{
	const Uint8 *real = (const Uint8 *)game_screen->pixels;
	const Uint8 *seen = (const Uint8 *)dl_scratch_game->pixels;

	dl_debug_diffs = 0;
	dl_debug_total = 0;

	if (game_screen->pitch != dl_scratch_game->pitch)
	{
		snprintf(dl_first_mismatch, sizeof dl_first_mismatch,
		         "frame %lu: pitch %d != %d", dl_checked, game_screen->pitch, dl_scratch_game->pitch);
		return false;
	}

	for (int y = 0; y < game_screen->h; ++y)
	{
		const Uint8 *r = real + (size_t)y * game_screen->pitch;
		const Uint8 *s = seen + (size_t)y * dl_scratch_game->pitch;
		for (int x = 0; x < game_screen->w; ++x)
		{
			if (r[x] != s[x])
			{
				if (dl_mismatched == 0 && dl_debug_diffs < 8)
				{
					logError("  diff (x=%d,y=%d) real=%u replayed=%u", x, y, (unsigned)r[x], (unsigned)s[x]);
					dl_debug_diffs++;
				}
				if (dl_debug_total == 0)
					snprintf(dl_first_mismatch, sizeof dl_first_mismatch,
					         "frame %lu: (x=%d,y=%d) real=%u replayed=%u",
					         dl_checked, x, y, (unsigned)r[x], (unsigned)s[x]);
				dl_debug_total++;
			}
		}
	}

	if (dl_mismatched == 0 && dl_debug_total > 0)
		logError("  (%d differing pixels in frame %lu)", dl_debug_total, dl_checked);

	return dl_debug_total == 0;
}

void drawlist_frame_end(void)
{
	if (!dl_recording)
		return;

	dl_recording = false;
	dl_context_kind = DL_OBJ_NONE;

	if (dl_check && dl_scratch_game != NULL && game_screen != NULL)
	{
		for (Uint32 i = 0; i < dl_count; ++i)
			dl_replay_command(&dl_commands[i]);

		dl_checked++;
		if (!dl_compare_frames())
		{
			if (dl_mismatched == 0)
				memcpy(dl_first_mismatch_saved, dl_first_mismatch, sizeof dl_first_mismatch_saved);
			dl_mismatched++;
			logError("Replay check: mismatch #%lu at %s.", dl_mismatched, dl_first_mismatch);
		}
	}

	dl_count = 0;
	dl_payload_used = 0;
}

unsigned long drawlist_checked_frames(void)
{
	return dl_checked;
}

unsigned long drawlist_mismatched_frames(void)
{
	return dl_mismatched;
}

const char *drawlist_first_mismatch(void)
{
	return dl_first_mismatch_saved[0] != '\0' ? dl_first_mismatch_saved : NULL;
}
