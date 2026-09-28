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
#ifndef DRAW_LIST_H
#define DRAW_LIST_H

#include "opentyr.h"
#include "sprite.h"

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stddef.h>

// Per-tick draw list (Fase 2, stages 1-2).
//
// The level renderer builds its frame by writing to the software surfaces
// game_screen and VGAScreen2 through a small set of primitives.  While a frame
// is being recorded, every such primitive appends one entry here.  The list is
// observation-only: it never changes gameplay state, RNG or timing, and when
// recording is disabled every hook returns immediately.
//
// A separate replay renderer re-applies the list to scratch copies of the two
// surfaces.  Because the list is captured at the primitive level and every
// primitive is a pure function of (surface, arguments), replaying it in order
// reproduces the frame byte for byte; the filters that read the framebuffer are
// simply replayed in the same order.

// Object identity attached to an entry, for the stage-3 interpolation pass.
// It is pure metadata: recording never reads or writes gameplay state to fill
// it, and replay ignores it.
enum
{
	DL_OBJ_NONE = 0,
	DL_OBJ_ENEMY,
	DL_OBJ_PLAYER,
	DL_OBJ_SIDEKICK,
	DL_OBJ_PLAYER_SHOT,
	DL_OBJ_ENEMY_SHOT,
	DL_OBJ_EXPLOSION,
	DL_OBJ_BACKGROUND,
	DL_OBJ_HUD,
	DL_OBJ_SUPERPIXEL,
	DL_OBJ_STARFIELD,
};

// Which framebuffer-reading filter to replay.  The filters are pure w.r.t.
// gameplay state; only their source/destination surfaces matter.
enum
{
	DL_FILTER_LAVA = 0,
	DL_FILTER_WATER,
	DL_FILTER_ICED,
	DL_FILTER_BLUR,
};

// Blit variants of the sprite_table (1-bit) family.
enum
{
	DL_SPRITE_BLIT = 0,
	DL_SPRITE_BLEND,
	DL_SPRITE_HV_UNSAFE,
	DL_SPRITE_HV,
	DL_SPRITE_HV_BLEND,
	DL_SPRITE_DARK,
};

// Blit variants of the Sprite2 (compressed) family.
enum
{
	DL_SPRITE2_BLIT = 0,
	DL_SPRITE2_CLIP,
	DL_SPRITE2_BLEND,
	DL_SPRITE2_DARKEN,
	DL_SPRITE2_FILTER,
	DL_SPRITE2_FILTER_CLIP,
};

// --- lifecycle ----------------------------------------------------------------

// Allocates the fixed command/payload buffers.  Safe to call more than once.
void drawlist_init(void);
void drawlist_shutdown(void);

// True while the current level tick is being recorded.  Also false unless
// drawlist_set_enabled(true) was called.
bool drawlist_recording(void);

// Enables recording (and, with it, the replay-check when regress asks for it).
void drawlist_set_enabled(bool enabled);
bool drawlist_enabled(void);

// Marks the boundaries of one recorded level tick.  frame_begin snapshots the
// real surfaces into the scratch ones and resets the list; frame_end replays
// the list and, when the check is armed, compares the result with the real
// frame.  Both are no-ops when recording is not enabled.
void drawlist_frame_begin(void);
void drawlist_frame_end(void);

// Sets the identity metadata copied into subsequently recorded entries.  Pass
// DL_OBJ_NONE to clear it.
void drawlist_set_context(int obj_kind, int obj_id, int obj_sub);

// --- recording hooks ----------------------------------------------------------
//
// Each hook is called from exactly one drawing primitive.  They are safe to call
// unconditionally; when recording is off they return without touching anything.

void drawlist_record_fill_full(SDL_Surface *surface);
void drawlist_record_fill_rect(SDL_Surface *surface, int x, int y, int x2, int y2, Uint8 color);
void drawlist_record_rect_outline(SDL_Surface *surface, int x, int y, int x2, int y2, Uint8 color);

void drawlist_record_bg_row(SDL_Surface *surface, int x, int y, Uint8 **map, bool blend);

void drawlist_record_blit_sprite(SDL_Surface *surface, int x, int y,
                                 unsigned int table, unsigned int index,
                                 int variant, Uint8 hue, Sint8 value, bool black);
void drawlist_record_blit_sprite2(SDL_Surface *surface, int x, int y,
                                  Sprite2_array sheet, unsigned int index,
                                  int variant, Uint8 filter);

void drawlist_record_darken(SDL_Surface *surface, JE_word neat);
void drawlist_record_filter_screen(SDL_Surface *surface, JE_shortint col, JE_shortint int_);
void drawlist_record_filter(SDL_Surface *dst, SDL_Surface *src, int kind);

// Bulk, stateful operations.  The caller captures the pre-call state so replay
// can re-run the operation against its scratch surface without perturbing (or
// depending on) the live globals.
void drawlist_record_starfield(SDL_Surface *surface, int move_speed, const void *stars, size_t bytes);
void drawlist_record_superpixels(SDL_Surface *surface, const void *superpixels, size_t bytes);

// --- replay helpers (implemented next to the operation they replay) -----------
//
// These reproduce one stateful operation on a scratch surface.  They restore
// the operation's captured pre-state, run, and leave the live globals at the
// same post-state the real call produced.

void drawlist_replay_starfield(SDL_Surface *surface, int move_speed, const void *stars, size_t bytes);
void drawlist_replay_superpixels(SDL_Surface *surface, const void *superpixels, size_t bytes);

// Pure pixel-apply halves of the two global-surface operations.
void drawlist_apply_darken(SDL_Surface *surface, JE_word neat);
void drawlist_apply_filter_screen(SDL_Surface *surface, JE_shortint col, JE_shortint int_);

// --- replay-check -------------------------------------------------------------

// Arms the internal replay check (used by --regress-replay-check).  With it
// armed, drawlist_frame_end() replays every recorded tick and reports the first
// mismatching frame/pixel.
void drawlist_set_check(bool check);

// Summary after the run: frames checked and the first mismatch (if any).
unsigned long drawlist_checked_frames(void);
unsigned long drawlist_mismatched_frames(void);
const char *drawlist_first_mismatch(void);   // NULL when all matched

#endif // DRAW_LIST_H
