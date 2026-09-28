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
#include "config.h"
#include "logging.h"
#include "modern_bloom.h"
#include "player.h"
#include "vga256d.h"
#include "video.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Per-tick draw list, stages 1-3.  See drawlist.h for the contract.

// Command buffering.  Fixed capacity allocated once; no per-frame allocation.
#define DL_MAX_COMMANDS  (1u << 15)   // 32768 entries, comfortably above one tick
#define DL_PAYLOAD_BYTES (1u << 18)   // 256 KiB bulk payload arena per tick

// Emission tag buffer geometry (one byte per pixel of a gameplay surface).
#define DL_TAG_W 320
#define DL_TAG_H 200

// A position jump larger than this between the two ticks snaps instead of
// interpolating: it is a teleport, or a reused slot whose new occupant spawned
// far from the old one.  The largest legitimate per-tick movement (a fast enemy
// shot) is well below it.
#define DL_INTERP_JUMP 64

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
	intptr_t row_key;   // background: map pointer with the horizontal pan removed
	int bg_pan;         // background: horizontal pan in whole tiles (mapX*bpPos family)
	Sprite2_array sheet;  // blit_sprite2 source
	Uint32 payload_off;
	Uint32 payload_len;
} DlCommand;

// One recorded tick: its commands, its payload arena and the player position at
// the tick's end (for the spotlight special code).  Two sets are kept so the
// previous tick survives while the current one is recorded.
typedef struct
{
	DlCommand *commands;
	Uint8 *payload;
	Uint32 count;
	Uint32 payload_used;
	int player_x, player_y;
} DlSet;

static DlSet dl_sets[2];
static int dl_cur = 0;    // set being recorded
static int dl_last = -1;  // last completed set (-1 before the first frame_end)

static DlCommand *dl_commands = NULL;  // == dl_sets[dl_cur].commands
static Uint8 *dl_payload = NULL;       // == dl_sets[dl_cur].payload
static Uint32 dl_count = 0;
static Uint32 dl_payload_used = 0;

static bool dl_enabled = false;
static bool dl_recording = false;
static bool dl_check = false;
static bool dl_interp_check = false;
static bool dl_have_prev = false;   // a previous tick has been recorded

static SDL_Surface *dl_scratch_game = NULL;
static SDL_Surface *dl_scratch_vga2 = NULL;
// The previous presented frame the scratch starts every interpolated replay
// from.  Filters (iced/blur) genuinely blend with the previous frame, so the
// renderer owns this reference instead of seeding from the live framebuffer.
static SDL_Surface *dl_ref_game = NULL;
static SDL_Surface *dl_ref_vga2 = NULL;
static bool dl_ref_valid = false;

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

// Recording is requested independently by the regress harness and by the smooth
// presentation loop; it runs while either wants it.
static bool dl_want_regress = false;
static bool dl_want_smooth = false;

static void dl_update_enabled(void)
{
	const bool enabled = dl_want_regress || dl_want_smooth;
	if (enabled == dl_enabled)
		return;

	dl_enabled = enabled;

	// (Re)start the recorded history: the first tick seeds its reference frame
	// from the live framebuffer.
	if (enabled)
	{
		dl_ref_valid = false;
		dl_have_prev = false;
		dl_last = -1;
	}
}

void drawlist_set_enabled(bool enabled)
{
	dl_want_regress = enabled;
	dl_update_enabled();
}

void drawlist_set_smooth_enabled(bool enabled)
{
	dl_want_smooth = enabled;
	dl_update_enabled();
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
	for (int i = 0; i < 2; ++i)
	{
		if (dl_sets[i].commands == NULL)
			dl_sets[i].commands = malloc(sizeof(DlCommand) * DL_MAX_COMMANDS);
		if (dl_sets[i].payload == NULL)
			dl_sets[i].payload = malloc(DL_PAYLOAD_BYTES);
	}
	if (dl_commands == NULL)
	{
		dl_cur = 0;
		dl_commands = dl_sets[dl_cur].commands;
		dl_payload = dl_sets[dl_cur].payload;
	}

	if (dl_scratch_game == NULL)
		dl_scratch_game = SDL_CreateSurface(vga_width, vga_height, SDL_PIXELFORMAT_INDEX8);
	if (dl_scratch_vga2 == NULL)
		dl_scratch_vga2 = SDL_CreateSurface(vga_width, vga_height, SDL_PIXELFORMAT_INDEX8);
	if (dl_ref_game == NULL)
		dl_ref_game = SDL_CreateSurface(vga_width, vga_height, SDL_PIXELFORMAT_INDEX8);
	if (dl_ref_vga2 == NULL)
		dl_ref_vga2 = SDL_CreateSurface(vga_width, vga_height, SDL_PIXELFORMAT_INDEX8);

	// The scratch surfaces are written by code that assumes the engine pitch
	// (backgrnd.c/sprite.c), so they must match the real surfaces exactly.
	assert(dl_scratch_game != NULL && game_screen != NULL &&
	       dl_scratch_game->pitch == game_screen->pitch);
	assert(dl_scratch_vga2 != NULL && VGAScreen2 != NULL &&
	       dl_scratch_vga2->pitch == VGAScreen2->pitch);
	assert(dl_ref_game != NULL && dl_ref_game->pitch == game_screen->pitch);
	assert(dl_ref_vga2 != NULL && dl_ref_vga2->pitch == VGAScreen2->pitch);

	// The tag buffers are fixed-capacity 320x200; their stamp functions assume
	// the game surfaces have exactly that pitch.
	assert(game_screen->pitch == DL_TAG_W && VGAScreen2->pitch == DL_TAG_W);
}

void drawlist_shutdown(void)
{
	for (int i = 0; i < 2; ++i)
	{
		free(dl_sets[i].commands);
		dl_sets[i].commands = NULL;
		free(dl_sets[i].payload);
		dl_sets[i].payload = NULL;
	}
	dl_commands = NULL;
	dl_payload = NULL;

	if (dl_scratch_game != NULL)
		SDL_DestroySurface(dl_scratch_game);
	dl_scratch_game = NULL;
	if (dl_scratch_vga2 != NULL)
		SDL_DestroySurface(dl_scratch_vga2);
	dl_scratch_vga2 = NULL;
	if (dl_ref_game != NULL)
		SDL_DestroySurface(dl_ref_game);
	dl_ref_game = NULL;
	if (dl_ref_vga2 != NULL)
		SDL_DestroySurface(dl_ref_vga2);
	dl_ref_vga2 = NULL;

	dl_last = -1;
	dl_have_prev = false;
	dl_ref_valid = false;
}

void drawlist_set_context(int obj_kind, int obj_id, int obj_sub)
{
	dl_context_kind = obj_kind;
	dl_context_id = obj_id;
	dl_context_sub = obj_sub;
}

// --- emission tag buffer ------------------------------------------------------
//
// One byte per pixel, parallel to the 8-bit gameplay surfaces.  See drawlist.h
// for the contract.  The four buffers cover the live game_screen/VGAScreen2 and
// the two interpolated replay scratches; they are fixed-capacity statics, so
// the per-frame path never allocates.  A session is armed by
// drawlist_tag_begin() at tick begin and the buffers are cleared then.

static Uint8 dl_tag_game[DL_TAG_W * DL_TAG_H];
static Uint8 dl_tag_vga2[DL_TAG_W * DL_TAG_H];
static Uint8 dl_tag_scratch_game[DL_TAG_W * DL_TAG_H];
static Uint8 dl_tag_scratch_vga2[DL_TAG_W * DL_TAG_H];

static bool dl_tag_active = false;

static Uint8 *dl_tag_for_surface(SDL_Surface *surface)
{
	if (surface == NULL)
		return NULL;
	if (surface == game_screen)
		return dl_tag_game;
	if (surface == VGAScreen2)
		return dl_tag_vga2;
	if (surface == dl_scratch_game)
		return dl_tag_scratch_game;
	if (surface == dl_scratch_vga2)
		return dl_tag_scratch_vga2;
	return NULL;
}

void drawlist_tag_begin(void)
{
	// Tagging only exists for the Modern lighting pass; Classic, lighting off
	// and the menus pay nothing.
	dl_tag_active = modern_lighting_tags_wanted();
	if (!dl_tag_active)
		return;

	memset(dl_tag_game, DL_TAG_NONE, sizeof dl_tag_game);
	memset(dl_tag_vga2, DL_TAG_NONE, sizeof dl_tag_vga2);
}

void drawlist_tag_pixel(SDL_Surface *surface, int x, int y, int tag)
{
	if (!dl_tag_active)
		return;
	if ((unsigned)x >= DL_TAG_W || (unsigned)y >= DL_TAG_H)
		return;

	Uint8 *buf = dl_tag_for_surface(surface);
	if (buf != NULL)
		buf[(size_t)y * DL_TAG_W + (size_t)x] = (Uint8)tag;
}

const Uint8 *drawlist_tag_for_surface(SDL_Surface *surface, int *out_pitch, int *out_w, int *out_h)
{
	if (!dl_tag_active)
		return NULL;

	Uint8 *buf = dl_tag_for_surface(surface);
	if (buf == NULL)
		return NULL;

	if (out_pitch != NULL) *out_pitch = DL_TAG_W;
	if (out_w != NULL) *out_w = DL_TAG_W;
	if (out_h != NULL) *out_h = DL_TAG_H;
	return buf;
}

// The tag class of the object currently being drawn, from the context the game
// set (drawlist_set_context).  Anything that is not an explicit emitter is
// DL_TAG_NONE, so it clears the tag of the pixels it covers.
static Uint8 dl_tag_value(void)
{
	switch (dl_context_kind)
	{
	case DL_OBJ_PLAYER_SHOT: return DL_TAG_PLAYER_SHOT;
	case DL_OBJ_ENEMY_SHOT:  return DL_TAG_ENEMY_SHOT;
	case DL_OBJ_EXPLOSION:   return DL_TAG_EXPLOSION;
	case DL_OBJ_ITEM:        return DL_TAG_ITEM;
	default:                 return DL_TAG_NONE;
	}
}

// Stamps a compressed Sprite2 exactly the way blit_sprite2* walk it (12 px
// wide rows, nibble run lengths, `pitch` row advance).  `clip` mirrors the
// blit_sprite2_clip/filter_clip walk, which uses explicit x/y instead of a
// running pointer.
static void dl_tag_sprite2(Uint8 *buf, int x, int y, Sprite2_array sprite2s, unsigned int index,
                           Uint8 tag, bool clip)
{
	const Uint8 *data = sprite2s.data + SDL_Swap16LE(((Uint16 *)sprite2s.data)[index - 1]);

	if (clip)
	{
		for (; *data != 0x0f; ++data)
		{
			if (y >= DL_TAG_H)
				return;

			int skip = *data & 0x0f;
			int fill = (*data >> 4) & 0x0f;

			x += skip;

			if (fill == 0)
			{
				y += 1;
				x -= 12;
			}
			else if (y >= 0)
			{
				Uint8 *row = buf + (size_t)y * DL_TAG_W;
				do
				{
					++data;
					if (x >= 0 && x < DL_TAG_W)
						row[x] = tag;
					x += 1;
				} while (--fill);
			}
			else
			{
				data += fill;
				x += fill;
			}
		}
		return;
	}

	// Signed offset: y/x may be negative when the sprite is clipped at the
	// playfield edges.  The per-pixel guards below (pixels >= ll / >= ul) then
	// skip the writes exactly like the blit; an unsigned size_t offset would
	// instead wrap and is undefined behaviour.
	Uint8 *pixels = buf + (ptrdiff_t)y * DL_TAG_W + x;
	const Uint8 * const ll = buf;
	const Uint8 * const ul = buf + (size_t)DL_TAG_W * DL_TAG_H;

	for (; *data != 0x0f; ++data)
	{
		pixels += *data & 0x0f;
		unsigned int count = (*data & 0xf0) >> 4;

		if (count == 0)
		{
			pixels += DL_TAG_W - 12;
		}
		else
		{
			while (count--)
			{
				++data;

				if (pixels >= ul)
					return;
				if (pixels >= ll)
					*pixels = tag;

				++pixels;
			}
		}
	}
}

// Stamps a 1-bit sprite_table sprite the way blit_sprite and its variants walk
// it (explicit `width` rows with a transparent/row opcode stream).
static void dl_tag_sprite(Uint8 *buf, int x, int y, unsigned int table, unsigned int index, Uint8 tag)
{
	if (index >= sprite_table[table].count || !sprite_exists(table, index))
		return;

	const Sprite * const cur = sprite(table, index);
	const Uint8 *data = cur->data;
	const Uint8 * const data_ul = data + cur->size;

	const unsigned int width = cur->width;
	unsigned int x_offset = 0;

	// See dl_tag_sprite2(): a signed offset keeps a negative start position
	// (sprite clipped at the top/left) out of undefined pointer arithmetic.
	Uint8 *pixels = buf + (ptrdiff_t)y * DL_TAG_W + x;
	const Uint8 * const ll = buf;
	const Uint8 * const ul = buf + (size_t)DL_TAG_W * DL_TAG_H;

	for (; data < data_ul; ++data)
	{
		switch (*data)
		{
		case 255:
			data++;
			pixels += *data;
			x_offset += *data;
			break;

		case 254:
			pixels += width - x_offset;
			x_offset = width;
			break;

		case 253:
			pixels++;
			x_offset++;
			break;

		default:
			if (pixels >= ul)
				return;
			if (pixels >= ll)
				*pixels = tag;

			pixels++;
			x_offset++;
			break;
		}

		if (x_offset >= width)
		{
			pixels += DL_TAG_W - x_offset;
			x_offset = 0;
		}
	}
}

static void dl_copy_surface(SDL_Surface *dst, const SDL_Surface *src)
{
	const int row = MIN(dst->w, src->w);
	for (int y = 0; y < MIN(dst->h, src->h); ++y)
		memcpy((Uint8 *)dst->pixels + (size_t)y * dst->pitch,
		       (const Uint8 *)src->pixels + (size_t)y * src->pitch, (size_t)row);
}

// Discards the recorded history at a level boundary.  A new level's first tick
// must not interpolate against (or blend filters with) the previous level's
// frame.
void drawlist_level_reset(void)
{
	dl_have_prev = false;
	dl_ref_valid = false;
	dl_last = -1;
}

void drawlist_frame_begin(void)
{
	if (!dl_enabled)
		return;

	// Deferred allocation: the scratch surfaces can only be sized once the
	// engine's surfaces exist (init_video runs after regress_init).
	if (dl_commands == NULL)
		drawlist_init();

	// Alternate sets so the last completed tick stays available as "previous".
	dl_cur = (dl_last == 0) ? 1 : 0;
	dl_commands = dl_sets[dl_cur].commands;
	dl_payload = dl_sets[dl_cur].payload;
	dl_count = 0;
	dl_payload_used = 0;
	dl_context_kind = DL_OBJ_NONE;
	dl_context_id = 0;
	dl_context_sub = 0;
	dl_recording = true;

	// The first recorded tick of a level: the previous presented frame the
	// interpolation (and the destination-reading filters) must start from is the
	// live framebuffer.  This is the only time the renderer touches it.
	if (!dl_ref_valid && game_screen != NULL && dl_ref_game != NULL)
	{
		dl_copy_surface(dl_ref_game, game_screen);
		if (VGAScreen2 != NULL)
			dl_copy_surface(dl_ref_vga2, VGAScreen2);
	}
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
	// A whole-surface clear also clears the emission tag of that surface.
	if (dl_tag_active)
	{
		Uint8 *tagbuf = dl_tag_for_surface(surface);
		if (tagbuf != NULL)
			memset(tagbuf, DL_TAG_NONE, (size_t)DL_TAG_W * DL_TAG_H);
	}

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
	// An opaque fill also clears the emission tag under it (HUD bars and other
	// code-drawn rectangles must not inherit a shot's tag).
	if (dl_tag_active)
	{
		Uint8 *tagbuf = dl_tag_for_surface(surface);
		if (tagbuf != NULL)
		{
			const int cx0 = MAX(0, MIN(x, x2)), cx1 = MIN(DL_TAG_W - 1, MAX(x, x2));
			const int cy0 = MAX(0, MIN(y, y2)), cy1 = MIN(DL_TAG_H - 1, MAX(y, y2));
			for (int ty = cy0; ty <= cy1; ++ty)
				memset(tagbuf + (size_t)ty * DL_TAG_W + cx0, DL_TAG_NONE, (size_t)(cx1 - cx0 + 1));
		}
	}

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

		// The map pointer advances with the horizontal pan (mapXbpPos is the
		// pan in whole tiles), so it is not a stable identity for a row across
		// ticks.  Store the pointer with that pan removed; the vertical
		// position (which is what interpolation tracks) is unaffected, so a row
		// keeps the same key through a tile-row wrap and the scroll stays
		// smooth.  The layer is dl_context_id.
		intptr_t bp = 0;
		if (dl_context_kind == DL_OBJ_BACKGROUND)
		{
			if (dl_context_id == 1)
				bp = mapXbpPos;
			else if (dl_context_id == 2)
				bp = (blend || !smoothies[1]) ? mapX2bpPos : mapXbpPos;
			else if (dl_context_id == 3)
				bp = mapX3bpPos;
		}
		c->row_key = (intptr_t)map / (intptr_t)sizeof(Uint8 *) - bp;
		c->bg_pan = (int)bp;
	}
}

void drawlist_record_blit_sprite(SDL_Surface *surface, int x, int y,
                                 unsigned int table, unsigned int index,
                                 int variant, Uint8 hue, Sint8 value, bool black)
{
	// Emission tagging runs whether or not the draw list is being recorded: it
	// is what lets the lighting pass follow each object's own pixels (and move
	// with an interpolated replay).
	if (dl_tag_active)
	{
		Uint8 *tagbuf = dl_tag_for_surface(surface);
		if (tagbuf != NULL)
			dl_tag_sprite(tagbuf, x, y, table, index, dl_tag_value());
	}

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
	// See drawlist_record_blit_sprite: the tag is written for every blit, so
	// the lighting pass can restrict emission per object.
	if (dl_tag_active)
	{
		Uint8 *tagbuf = dl_tag_for_surface(surface);
		if (tagbuf != NULL)
		{
			const bool clip = (variant == DL_SPRITE2_CLIP || variant == DL_SPRITE2_FILTER_CLIP);
			dl_tag_sprite2(tagbuf, x, y, sheet, index, dl_tag_value(), clip);
		}
	}

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

static void dl_replay_command(const DlCommand *c, int x, int y)
{
	SDL_Surface *surface = dl_scratch_for(c->surface);
	if (surface == NULL)
		return;

	// The sprite blits below stamp the emission tag from the current context;
	// restore the command's identity so an interpolated replay tags each object
	// at its interpolated position.
	drawlist_set_context(c->obj_kind, c->obj_id, c->obj_sub);

	switch (c->kind)
	{
	case DL_FILL_FULL:
		SDL_FillSurfaceRect(surface, NULL, 0);
		break;

	case DL_FILL_RECT:
	{
		SDL_Rect rect = { x, y, c->a - c->x + 1, c->b - c->y + 1 };
		SDL_FillSurfaceRect(surface, &rect, c->color);
		break;
	}

	case DL_RECT_OUTLINE:
		JE_rectangle(surface, x, y, c->a, c->b, c->color);
		break;

	case DL_BG_ROW:
		blit_background_row(surface, x, y, c->map);
		break;
	case DL_BG_ROW_BLEND:
		blit_background_row_blend(surface, x, y, c->map);
		break;

	case DL_BLIT_SPRITE:
		switch (c->variant)
		{
		case DL_SPRITE_BLIT:
			blit_sprite(surface, x, y, c->a, c->b);
			break;
		case DL_SPRITE_BLEND:
			blit_sprite_blend(surface, x, y, c->a, c->b);
			break;
		case DL_SPRITE_HV_UNSAFE:
			blit_sprite_hv_unsafe(surface, x, y, c->a, c->b, c->hue, c->value);
			break;
		case DL_SPRITE_HV:
			blit_sprite_hv(surface, x, y, c->a, c->b, c->hue, c->value);
			break;
		case DL_SPRITE_HV_BLEND:
			blit_sprite_hv_blend(surface, x, y, c->a, c->b, c->hue, c->value);
			break;
		case DL_SPRITE_DARK:
			blit_sprite_dark(surface, x, y, c->a, c->b, c->black);
			break;
		default:
			break;
		}
		break;

	case DL_BLIT_SPRITE2:
		switch (c->variant)
		{
		case DL_SPRITE2_BLIT:
			blit_sprite2(surface, x, y, c->sheet, c->b);
			break;
		case DL_SPRITE2_CLIP:
			blit_sprite2_clip(surface, x, y, c->sheet, c->b);
			break;
		case DL_SPRITE2_BLEND:
			blit_sprite2_blend(surface, x, y, c->sheet, c->b);
			break;
		case DL_SPRITE2_DARKEN:
			blit_sprite2_darken(surface, x, y, c->sheet, c->b);
			break;
		case DL_SPRITE2_FILTER:
			blit_sprite2_filter(surface, x, y, c->sheet, c->b, c->filter);
			break;
		case DL_SPRITE2_FILTER_CLIP:
			blit_sprite2_filter_clip(surface, x, y, c->sheet, c->b, c->filter);
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

// --- stage 3: identity matching -----------------------------------------------
//
// Commands that carry an object identity are paired across the two ticks so
// their positions can be interpolated.  The pairing is by identity plus the
// order of occurrence inside that identity (a 2x2 ship draws four commands with
// the same identity; they keep their relative order).  Background rows use the
// pan-normalised map pointer instead (c->row_key), which is unique per row.

#define DL_MATCH_BUCKETS 2048

typedef struct
{
	Uint64 key;
	bool used;
	Sint32 head, tail, cursor;
} DlMatchBucket;

static DlMatchBucket dl_buckets[DL_MATCH_BUCKETS];
static Sint32 dl_match_next[DL_MAX_COMMANDS];

static unsigned long dl_interp_matched_count = 0;
static unsigned long dl_interp_new_count = 0;
static unsigned long dl_interp_jump_count = 0;
static unsigned long dl_interp_sheet_count = 0;
static unsigned long dl_interp_overshoot_count = 0;

static int dl_interp_player_x = 0, dl_interp_player_y = 0;

static Uint64 dl_mix_key(Uint32 a, Uint32 b, Uint32 c, Uint64 d)
{
	Uint64 h = UINT64_C(1469598103934665603);
	h = (h ^ a) * UINT64_C(1099511628211);
	h = (h ^ b) * UINT64_C(1099511628211);
	h = (h ^ c) * UINT64_C(1099511628211);
	h = (h ^ d) * UINT64_C(1099511628211);
	return h;
}

static bool dl_interpolatable(const DlCommand *c)
{
	if (c->kind == DL_BG_ROW || c->kind == DL_BG_ROW_BLEND)
		return true;

	switch (c->obj_kind)
	{
	case DL_OBJ_ENEMY:
	case DL_OBJ_ITEM:
	case DL_OBJ_PLAYER:
	case DL_OBJ_SIDEKICK:
	case DL_OBJ_PLAYER_SHOT:
	case DL_OBJ_ENEMY_SHOT:
	case DL_OBJ_EXPLOSION:
		return true;
	default:
		return false;
	}
}

static Uint64 dl_key_of(const DlCommand *c)
{
	if (c->kind == DL_BG_ROW || c->kind == DL_BG_ROW_BLEND)
		return dl_mix_key(0xB6u, c->obj_id, 0, (Uint64)(intptr_t)c->row_key);

	return dl_mix_key(c->obj_kind, c->obj_id, c->obj_sub, 0);
}

static Uint32 dl_hash_key(Uint64 key)
{
	return (Uint32)(key ^ (key >> 32)) & (DL_MATCH_BUCKETS - 1);
}

// Indexes every interpolatable command of the previous tick, in order.
static void dl_match_build(const DlSet *prev)
{
	for (int i = 0; i < DL_MATCH_BUCKETS; ++i)
		dl_buckets[i].used = false;

	for (Uint32 i = 0; i < prev->count; ++i)
	{
		const DlCommand *c = &prev->commands[i];
		if (!dl_interpolatable(c))
			continue;

		const Uint64 key = dl_key_of(c);
		Uint32 b = dl_hash_key(key);
		while (dl_buckets[b].used && dl_buckets[b].key != key)
			b = (b + 1) & (DL_MATCH_BUCKETS - 1);

		if (!dl_buckets[b].used)
		{
			dl_buckets[b].used = true;
			dl_buckets[b].key = key;
			dl_buckets[b].head = -1;
			dl_buckets[b].tail = -1;
		}

		dl_match_next[i] = -1;
		if (dl_buckets[b].tail < 0)
			dl_buckets[b].head = (Sint32)i;
		else
			dl_match_next[dl_buckets[b].tail] = (Sint32)i;
		dl_buckets[b].tail = (Sint32)i;
	}
}

static void dl_match_reset_cursors(void)
{
	for (int i = 0; i < DL_MATCH_BUCKETS; ++i)
		if (dl_buckets[i].used)
			dl_buckets[i].cursor = dl_buckets[i].head;
}

// Returns the previous-tick command matching `c` (the next occurrence of the
// same identity), or -1 when there is none left.
static Sint32 dl_match_take(const DlCommand *c)
{
	if (!dl_interpolatable(c))
		return -1;

	const Uint64 key = dl_key_of(c);
	Uint32 b = dl_hash_key(key);
	while (dl_buckets[b].used && dl_buckets[b].key != key)
		b = (b + 1) & (DL_MATCH_BUCKETS - 1);

	if (!dl_buckets[b].used)
		return -1;

	const Sint32 idx = dl_buckets[b].cursor;
	if (idx >= 0)
		dl_buckets[b].cursor = dl_match_next[idx];
	return idx;
}

enum
{
	DL_SNAP_NONE = 0,
	DL_SNAP_NEW,
	DL_SNAP_JUMP,
	DL_SNAP_SHEET,
};

// Decides whether a pair snaps (drawn at the new position, no sliding) and why.
static int dl_snap_reason(const DlCommand *p, const DlCommand *c)
{
	if (p == NULL)
		return DL_SNAP_NEW;

	int dx, dy;

	// A background row's recorded x wraps at every tile crossing, so the jump
	// has to be measured on the continuous origin (see dl_bg_presented), not on
	// the raw x; a normal pan changes it by at most a few pixels per tick.
	if (c->kind == DL_BG_ROW || c->kind == DL_BG_ROW_BLEND)
		dx = (c->x - 24 * c->bg_pan) - (p->x - 24 * p->bg_pan);
	else
		dx = c->x - p->x;

	dy = c->y - p->y;

	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;
	if (dx > DL_INTERP_JUMP || dy > DL_INTERP_JUMP)
		return DL_SNAP_JUMP;

	// A different compressed sheet/size in the same slot is a reused slot (or
	// an enemy that morphed); its motion is discontinuous, so snap.  Animation
	// keeps the sheet and changes only the index, so this does not fire on it.
	if (p->kind == DL_BLIT_SPRITE2 && c->kind == DL_BLIT_SPRITE2 &&
	    (p->sheet.data != c->sheet.data || p->sheet.size != c->sheet.size))
		return DL_SNAP_SHEET;

	return DL_SNAP_NONE;
}

static int dl_lerp(int a, int b, Uint32 alpha_fx16)
{
	return a + (int)(((Sint64)(b - a) * (Sint32)alpha_fx16) / 65536);
}

// Presented (x, map) of one background row partway between the previous tick and
// the current one, plus the continuous content origin `x - 24*pan` that the
// pair actually draws.  A background row is recorded at x = mapX*Pos (0..23)
// and at a map pointer shifted by the pan in whole tiles; as the pan crosses a
// tile both wrap, so interpolating the recorded x directly (stage 3 before this
// fix) slid the whole layer ~23 px the wrong way on every crossing.  The fix
// interpolates the continuous origin instead and keeps the map pointer of the
// current command, so x = origin + 24*pan and alpha = 1 reproduces the tick.
static void dl_bg_presented(const DlCommand *p, const DlCommand *c, Uint32 alpha_fx16,
                            int *out_x, Uint8 ***out_map, int *out_origin)
{
	// Unwrap each endpoint with its own pan, interpolate the continuous origin,
	// then re-express it against the current command's map pointer so the blit
	// stays in the same tile range the real frame used.
	const int hp = p->x - 24 * p->bg_pan;
	const int hc = c->x - 24 * c->bg_pan;
	const int origin = dl_lerp(hp, hc, alpha_fx16);

	*out_map = c->map;
	*out_x = origin + 24 * c->bg_pan;
	*out_origin = origin;
}

bool drawlist_render_interpolated(Uint32 alpha_fx16)
{
	// The scratch and reference surfaces are created by drawlist_init(), which
	// frame_begin() calls before the first tick is recorded.  An empty previous
	// set is fine: every command then snaps to its current position.
	if (dl_last < 0 || dl_scratch_game == NULL || dl_ref_game == NULL)
		return false;

	const DlSet *prev = &dl_sets[1 - dl_last];
	const DlSet *cur = &dl_sets[dl_last];

	if (alpha_fx16 > 65536)
		alpha_fx16 = 65536;

	// Filters that read the destination blend with the previous presented
	// frame: the renderer owns that reference and seeds the scratch from it.
	dl_copy_surface(dl_scratch_game, dl_ref_game);
	dl_copy_surface(dl_scratch_vga2, dl_ref_vga2);

	// The interpolated tags describe exactly the commands redrawn below, so a
	// command that is gone this tick cannot keep emitting.
	if (dl_tag_active)
	{
		memset(dl_tag_scratch_game, DL_TAG_NONE, sizeof dl_tag_scratch_game);
		memset(dl_tag_scratch_vga2, DL_TAG_NONE, sizeof dl_tag_scratch_vga2);
	}

	dl_match_build(prev);
	dl_match_reset_cursors();

	for (Uint32 i = 0; i < cur->count; ++i)
	{
		const DlCommand *c = &cur->commands[i];
		int x = c->x, y = c->y;
		const DlCommand *p = NULL;
		int reason = DL_SNAP_NEW;

		if (dl_interpolatable(c))
		{
			const Sint32 pi = dl_match_take(c);
			if (pi >= 0)
				p = &prev->commands[pi];
			reason = dl_snap_reason(p, c);

			if (reason == DL_SNAP_NONE)
			{
				x = dl_lerp(p->x, c->x, alpha_fx16);
				y = dl_lerp(p->y, c->y, alpha_fx16);
				dl_interp_matched_count++;

				// Sanity: a mid-frame position must stay inside the endpoints.
				if (dl_interp_check && alpha_fx16 == 65536)
				{
					const int mx = dl_lerp(p->x, c->x, 32768);
					const int my = dl_lerp(p->y, c->y, 32768);
					if (mx < MIN(p->x, c->x) || mx > MAX(p->x, c->x) ||
					    my < MIN(p->y, c->y) || my > MAX(p->y, c->y))
						dl_interp_overshoot_count++;
				}
			}
			else if (reason == DL_SNAP_NEW)
				dl_interp_new_count++;
			else if (reason == DL_SNAP_JUMP)
				dl_interp_jump_count++;
			else
				dl_interp_sheet_count++;
		}

		SDL_Surface *surface = dl_scratch_for(c->surface);
		if (surface == NULL)
			continue;

		if (c->kind == DL_BG_ROW || c->kind == DL_BG_ROW_BLEND)
		{
			Uint8 **map = c->map;
			int origin;  // only the smoothness pass reads this
			if (reason == DL_SNAP_NONE)
				dl_bg_presented(p, c, alpha_fx16, &x, &map, &origin);
			else
			{
				x = c->x;
				y = c->y;
				map = c->map;
			}

			if (c->kind == DL_BG_ROW)
				blit_background_row(surface, x, y, map);
			else
				blit_background_row_blend(surface, x, y, map);
		}
		else if (c->kind == DL_STARFIELD)
			drawlist_draw_starfield_interp(surface, c->a, cur->payload + c->payload_off, c->payload_len, alpha_fx16);
		else if (c->kind == DL_SUPERPIXELS)
			drawlist_draw_superpixels_interp(surface, cur->payload + c->payload_off, c->payload_len, alpha_fx16);
		else
			dl_replay_command(c, x, y);
	}

	// The spotlight follows the player.  On the first tick of a level there is
	// no previous position to slide from, so snap.
	if (prev->count == 0)
	{
		dl_interp_player_x = cur->player_x;
		dl_interp_player_y = cur->player_y;
	}
	else
	{
		dl_interp_player_x = dl_lerp(prev->player_x, cur->player_x, alpha_fx16);
		dl_interp_player_y = dl_lerp(prev->player_y, cur->player_y, alpha_fx16);
	}

	// A full-alpha render realises the current tick exactly; keep it as the
	// reference the next tick interpolates from.
	if (alpha_fx16 == 65536)
	{
		dl_copy_surface(dl_ref_game, dl_scratch_game);
		dl_copy_surface(dl_ref_vga2, dl_scratch_vga2);
		dl_ref_valid = true;
	}

	return true;
}

SDL_Surface *drawlist_interpolated_game(void)
{
	return dl_scratch_game;
}

void drawlist_interpolated_player(int *x, int *y)
{
	if (x != NULL) *x = dl_interp_player_x;
	if (y != NULL) *y = dl_interp_player_y;
}

bool drawlist_has_previous(void)
{
	return dl_have_prev;
}

void drawlist_set_interp_check(bool check)
{
	dl_interp_check = check;
}

unsigned long drawlist_interp_matched(void)    { return dl_interp_matched_count; }
unsigned long drawlist_interp_snap_new(void)   { return dl_interp_new_count; }
unsigned long drawlist_interp_snap_jump(void)  { return dl_interp_jump_count; }
unsigned long drawlist_interp_snap_sheet(void) { return dl_interp_sheet_count; }
unsigned long drawlist_interp_overshoots(void) { return dl_interp_overshoot_count; }

// --- stage 3: smoothness proof ------------------------------------------------
//
// For every level tick this re-derives the interpolated positions at N
// sub-frame alphas (alpha 0 = previous tick, alpha 1 = current tick) and checks
// that what the renderer would present for each background layer and for each
// matched object moves monotonically from the previous tick's position to the
// current one, without backtracking or overshooting the tick's total motion.
//
// A background row is the case that matters: the recorded x (mapX*Pos, 0..23)
// and the map pointer both wrap as the pan crosses a tile, so a naive
// interpolation of the recorded x slides the layer ~23 px the wrong way once
// per tile crossing.  Comparing the presented continuous origin against the
// endpoints (each unwrapped with its own pan) makes that show up as a
// horizontal event; after the fix the presented origin equals the interpolated
// origin by construction.
#define DL_SMOOTH_MAX_ALPHAS 33

static bool dl_smoothness_check = false;
static unsigned dl_smooth_alpha_count = 5;

static unsigned long dl_smooth_ticks = 0;
static unsigned long dl_smooth_bg_checks = 0;
static unsigned long dl_smooth_obj_checks = 0;
static unsigned long dl_smooth_h_events = 0;
static unsigned long dl_smooth_v_events = 0;
static unsigned long dl_smooth_obj_events = 0;
static unsigned long dl_smooth_frames_with_events = 0;

void drawlist_set_smoothness_check(bool check)
{
	dl_smoothness_check = check;
}

bool drawlist_smoothness_enabled(void)
{
	return dl_smoothness_check;
}

void drawlist_set_smoothness_alphas(unsigned int count)
{
	if (count >= 2 && count <= DL_SMOOTH_MAX_ALPHAS)
		dl_smooth_alpha_count = count;
}

unsigned long drawlist_smoothness_ticks(void)             { return dl_smooth_ticks; }
unsigned long drawlist_smoothness_bg_checks(void)         { return dl_smooth_bg_checks; }
unsigned long drawlist_smoothness_object_checks(void)     { return dl_smooth_obj_checks; }
unsigned long drawlist_smoothness_horizontal_events(void) { return dl_smooth_h_events; }
unsigned long drawlist_smoothness_vertical_events(void)   { return dl_smooth_v_events; }
unsigned long drawlist_smoothness_object_events(void)     { return dl_smooth_obj_events; }
unsigned long drawlist_smoothness_frames(void)            { return dl_smooth_frames_with_events; }
unsigned long drawlist_smoothness_events(void)
{
	return dl_smooth_h_events + dl_smooth_v_events + dl_smooth_obj_events;
}

static void dl_smoothness_tick(void)
{
	if (dl_last < 0)
		return;

	const DlSet *prev = &dl_sets[1 - dl_last];
	const DlSet *cur = &dl_sets[dl_last];
	if (cur->count == 0)
		return;

	const unsigned n = dl_smooth_alpha_count;

	// True endpoints of each layer: the continuous origin of the pan computed
	// with each command's own pan (so it does not wrap), and the minimum y of
	// the matched rows.
	int layer_h[3][2];
	int layer_v[3][2];
	bool layer_seen[3] = { false, false, false };
	int hpos[3][DL_SMOOTH_MAX_ALPHAS];
	int vmin[3][DL_SMOOTH_MAX_ALPHAS];
	bool layer_ok[3][DL_SMOOTH_MAX_ALPHAS];

	memset(layer_ok, 0, sizeof layer_ok);

	dl_match_build(prev);

	dl_match_reset_cursors();
	for (Uint32 i = 0; i < cur->count; ++i)
	{
		const DlCommand *c = &cur->commands[i];
		if (!dl_interpolatable(c))
			continue;
		const Sint32 pi = dl_match_take(c);
		if (pi < 0)
			continue;
		const DlCommand *p = &prev->commands[pi];
		if (dl_snap_reason(p, c) != DL_SNAP_NONE)
			continue;
		if (c->kind != DL_BG_ROW && c->kind != DL_BG_ROW_BLEND)
			continue;

		const int layer = c->obj_id;
		if (layer < 1 || layer > 3)
			continue;

		if (!layer_seen[layer - 1])
		{
			layer_seen[layer - 1] = true;
			layer_h[layer - 1][0] = p->x - 24 * p->bg_pan;
			layer_h[layer - 1][1] = c->x - 24 * c->bg_pan;
			layer_v[layer - 1][0] = p->y;
			layer_v[layer - 1][1] = c->y;
		}
		else
		{
			layer_v[layer - 1][0] = MIN(layer_v[layer - 1][0], p->y);
			layer_v[layer - 1][1] = MIN(layer_v[layer - 1][1], c->y);
		}
	}

	unsigned long tick_events = 0;

	for (unsigned k = 0; k < n; ++k)
	{
		const Uint32 alpha = (Uint32)(((Uint64)k << 16) / (n - 1));

		dl_match_reset_cursors();
		for (Uint32 i = 0; i < cur->count; ++i)
		{
			const DlCommand *c = &cur->commands[i];
			if (!dl_interpolatable(c))
				continue;
			const Sint32 pi = dl_match_take(c);
			if (pi < 0)
				continue;
			const DlCommand *p = &prev->commands[pi];
			if (dl_snap_reason(p, c) != DL_SNAP_NONE)
				continue;

			if (c->kind == DL_BG_ROW || c->kind == DL_BG_ROW_BLEND)
			{
				const int layer = c->obj_id;
				if (layer < 1 || layer > 3)
					continue;

				int x, origin;
				Uint8 **map;
				dl_bg_presented(p, c, alpha, &x, &map, &origin);
				const int v = dl_lerp(p->y, c->y, alpha);

				if (!layer_ok[layer - 1][k])
				{
					layer_ok[layer - 1][k] = true;
					hpos[layer - 1][k] = origin;
					vmin[layer - 1][k] = v;
				}
				else
				{
					if (origin != hpos[layer - 1][k])
					{
						// Rows of one layer must agree on the pan; a
						// disagreement is itself a discontinuity.
						dl_smooth_h_events++;
						tick_events++;
						hpos[layer - 1][k] = origin;
					}
					vmin[layer - 1][k] = MIN(vmin[layer - 1][k], v);
				}
			}
			else
			{
				const int px = dl_lerp(p->x, c->x, alpha);
				const int py = dl_lerp(p->y, c->y, alpha);
				dl_smooth_obj_checks++;
				if (px < MIN(p->x, c->x) || px > MAX(p->x, c->x) ||
				    py < MIN(p->y, c->y) || py > MAX(p->y, c->y))
				{
					dl_smooth_obj_events++;
					tick_events++;
				}
			}
		}
	}

	for (int layer = 0; layer < 3; ++layer)
	{
		if (!layer_seen[layer])
			continue;

		bool complete = true;
		for (unsigned k = 0; k < n; ++k)
			if (!layer_ok[layer][k])
				complete = false;
		if (!complete)
			continue;

		dl_smooth_bg_checks++;

		const int hp = layer_h[layer][0], hc = layer_h[layer][1];
		const int hlo = MIN(hp, hc), hhi = MAX(hp, hc);
		bool h_bad = false;
		for (unsigned k = 0; k < n; ++k)
			if (hpos[layer][k] < hlo || hpos[layer][k] > hhi)
				h_bad = true;
		for (unsigned k = 1; k < n; ++k)
		{
			const int step = hpos[layer][k] - hpos[layer][k - 1];
			if ((hc > hp && step < 0) || (hc < hp && step > 0))
				h_bad = true;
		}
		if (h_bad)
		{
			dl_smooth_h_events++;
			tick_events++;
		}

		const int vp = layer_v[layer][0], vc = layer_v[layer][1];
		const int vlo = MIN(vp, vc), vhi = MAX(vp, vc);
		bool v_bad = false;
		for (unsigned k = 0; k < n; ++k)
			if (vmin[layer][k] < vlo || vmin[layer][k] > vhi)
				v_bad = true;
		for (unsigned k = 1; k < n; ++k)
		{
			const int step = vmin[layer][k] - vmin[layer][k - 1];
			if ((vc > vp && step < 0) || (vc < vp && step > 0))
				v_bad = true;
		}
		if (v_bad)
		{
			dl_smooth_v_events++;
			tick_events++;
		}
	}

	dl_smooth_ticks++;
	if (tick_events > 0)
		dl_smooth_frames_with_events++;
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

	// Persist this tick into its set before anything reads it back.
	dl_sets[dl_cur].count = dl_count;
	dl_sets[dl_cur].payload_used = dl_payload_used;
	dl_sets[dl_cur].player_x = player[0].x;
	dl_sets[dl_cur].player_y = player[0].y;
	dl_last = dl_cur;
	dl_have_prev = dl_sets[1 - dl_last].count > 0;

	// Stage 3 proof: the smoothness pass.  Runs before the byte-for-byte checks
	// because it only reads the two recorded lists (it does not draw).
	if (dl_smoothness_check)
		dl_smoothness_tick();

	// Stages 1-2 proof: replay this tick's list.  The destination-reading
	// filters need the frame the tick started from, which is the persistent
	// reference (the previous tick's realised frame), not the live post-tick
	// framebuffer.
	if (dl_check && !dl_interp_check && dl_scratch_game != NULL && dl_ref_game != NULL)
	{
		dl_copy_surface(dl_scratch_game, dl_ref_game);
		dl_copy_surface(dl_scratch_vga2, dl_ref_vga2);
		for (Uint32 i = 0; i < dl_count; ++i)
			dl_replay_command(&dl_commands[i], dl_commands[i].x, dl_commands[i].y);

		// Keep the reference in sync for the next tick.
		dl_copy_surface(dl_ref_game, dl_scratch_game);
		dl_copy_surface(dl_ref_vga2, dl_scratch_vga2);
		dl_ref_valid = true;

		dl_checked++;
		if (!dl_compare_frames())
		{
			if (dl_mismatched == 0)
				memcpy(dl_first_mismatch_saved, dl_first_mismatch, sizeof dl_first_mismatch_saved);
			dl_mismatched++;
			logError("Replay check: mismatch #%lu at %s.", dl_mismatched, dl_first_mismatch);
		}
	}

	// Stage 3 proof: build the interpolated frame at alpha = 1 with the
	// persistent renderer and compare it byte for byte with the real frame.
	if (dl_interp_check && game_screen != NULL && dl_scratch_game != NULL)
	{
		bool rendered = drawlist_render_interpolated(65536);
		if (!rendered)
		{
			// First tick of a level: no previous list yet.  Replay the current
			// list from the live frame so the tick is still covered.
			dl_copy_surface(dl_scratch_game, game_screen);
			dl_copy_surface(dl_scratch_vga2, VGAScreen2);
			for (Uint32 i = 0; i < dl_count; ++i)
				dl_replay_command(&dl_commands[i], dl_commands[i].x, dl_commands[i].y);
		}

		dl_checked++;
		if (!dl_compare_frames())
		{
			if (dl_mismatched == 0)
				memcpy(dl_first_mismatch_saved, dl_first_mismatch, sizeof dl_first_mismatch_saved);
			dl_mismatched++;
			logError("Interp check: mismatch #%lu at %s.", dl_mismatched, dl_first_mismatch);
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
