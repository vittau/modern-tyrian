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
	// An enemy that is a pickup (armour 0 and a non-zero value: a data cube, a
	// weapon/armour/option power-up or cash).  It is still an enemy for motion
	// but is tagged emissive by the lighting pass.
	DL_OBJ_ITEM,
};

// Emission tag classes for the Modern bloom/lighting pass.  Every blit to a
// gameplay surface stamps one byte per pixel into a parallel tag buffer (the
// "tag buffer" of the plan): DL_TAG_NONE for ships, HUD/text, backgrounds and
// starfield, one of the emissive classes for the objects the user asked to
// light.  The pass only takes light from a pixel whose tag is non-zero, so a
// bright ship window or a HUD glyph can never emit even though its palette
// entry is bright.
enum
{
	DL_TAG_NONE = 0,
	DL_TAG_PLAYER_SHOT,
	DL_TAG_ENEMY_SHOT,
	DL_TAG_EXPLOSION,
	DL_TAG_ITEM,
	DL_TAG_SUPERPIXEL,
	DL_TAG_VFX,
	DL_TAG_MAX
};

// The tag byte packs the class in its low 3 bits and a 5-bit quantised
// per-sprite emissive-footprint code (bright pixels) in the high bits.  The
// pass uses it to cap the light one big player-shot sprite emits (a high-power
// Mega Cannon would otherwise flood the field); a small sprite is below the
// cap and unchanged.  Footprint 0 is the default: no scaling (superpixels,
// VFX, and any sprite with too few bright pixels to matter).
#define DL_TAG_CLASS_MASK  0x07
#define DL_TAG_FOOT_SHIFT  3
#define DL_TAG_FOOT_STEPS  32
#define DL_TAG_FOOT_PER_STEP 8  // bright pixels represented by one footprint step

static inline Uint8 dl_tag_pack(int cls, unsigned int bright_pixels)
{
	unsigned int step = bright_pixels / DL_TAG_FOOT_PER_STEP;
	if (step > DL_TAG_FOOT_STEPS - 1)
		step = DL_TAG_FOOT_STEPS - 1;
	return (Uint8)((cls & DL_TAG_CLASS_MASK) | (step << DL_TAG_FOOT_SHIFT));
}

// --- depth layer buffer -------------------------------------------------------
//
// Beside the emission tag, every gameplay surface has a second parallel buffer:
// one byte per pixel saying which layer painted that pixel LAST.  It is the
// foundation of the Modern depth pass (soft cast shadows between layers) and
// changes no presented pixel.
//
// Encoding of one byte:
//   bits 0-4  layer id, one of DL_LAYER_* (0 .. DL_LAYER_COUNT - 1, max 31)
//   bit 5     DL_LAYER_VFX_FLAG: a VFX / ambient-particle pixel.  It only tints the
//             pixel and keeps the layer id of whatever was under it, so a VFX
//             pixel never casts a shadow but still receives as its underlying
//             layer.  Only ever set on the presented copy (modern_depth.c).
//   bit 6     reserved, always 0
//   bit 7     DL_LAYER_BLEND: the pixel was written by the BLENDED bg2 row
//             (blit_background_row_blend), i.e. it is a translucent mix of bg2
//             over whatever was below, not an opaque bg2 pixel.  Only ever set
//             together with DL_LAYER_BG2.
//
// The layer of an enemy comes from its slot range, because the slot range is
// what decides where in the frame it is drawn: 0-24 sky, 25-49 and 75-99 ground,
// 50-74 top.  A pickup keeps the layer of the slot it lives in.
enum
{
	DL_LAYER_NONE = 0,     // cleared by a fill, or never painted this tick
	DL_LAYER_BG1,
	DL_LAYER_STARFIELD,
	DL_LAYER_BG2,
	DL_LAYER_GROUND_ENEMY,
	DL_LAYER_SKY_ENEMY,
	DL_LAYER_BG3,
	DL_LAYER_TOP_ENEMY,
	DL_LAYER_PLAYER,
	DL_LAYER_SIDEKICK,
	DL_LAYER_PLAYER_SHOT,
	DL_LAYER_ENEMY_SHOT,
	DL_LAYER_EXPLOSION,
	DL_LAYER_SUPERPIXEL,
	DL_LAYER_HUD,
	DL_LAYER_OTHER,        // a sprite drawn with no object context
	DL_LAYER_COUNT
};

#define DL_LAYER_ID_MASK 0x1f
#define DL_LAYER_VFX_FLAG 0x20
#define DL_LAYER_BLEND   0x80

// True while the layer buffer is being stamped this tick (Modern presentation and
// depth requested; see modern_depth_layers_wanted()).
bool drawlist_layers_active(void);

// Stamps one pixel of a surface by its byte offset (the starfield addresses its
// pixels that way, including the row-wrapping neighbours).  No-op unless layers
// are active and `surface` is a tracked gameplay surface.
void drawlist_layer_stamp_offset(SDL_Surface *surface, size_t offset, int layer);

// Returns the layer buffer matching `surface` (320x200, pitch 320), or NULL when
// layers are off / the surface is not tracked.
const Uint8 *drawlist_layer_for_surface(SDL_Surface *surface, int *out_pitch, int *out_w, int *out_h);

// Returns the DL_LAYER_COUNT-byte table of the order in which each layer was
// FIRST drawn on `surface`'s tick (1.. in drawing order; 0 = not drawn), or NULL
// when layers are off.  The order changes per level (background2over,
// background3over, skyEnemyOverAll, topEnemyOver), so a later pass uses it to
// decide which layer is "below" which.  The live surfaces and the interpolated
// scratch have a table each.
const Uint8 *drawlist_layer_ranks(SDL_Surface *surface);

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
// This is the regress request; drawlist_set_smooth_enabled() is the gameplay
// request (Modern smooth motion).  Recording is on when either asks for it.
void drawlist_set_enabled(bool enabled);
bool drawlist_enabled(void);

// Asks for recording because the smooth presentation loop needs the two most
// recent tick lists.  Separate from the regress request so Classic regress
// checks keep recording even though smooth motion is off there.
void drawlist_set_smooth_enabled(bool enabled);

// Marks the boundaries of one recorded level tick.  frame_begin switches to the
// next command set and, on the first tick after a level reset, seeds the
// reference frame from the live framebuffer; frame_end finalizes the set and,
// when a check is armed, replays or interpolates the tick and compares it with
// the real frame.  Both are no-ops when recording is not enabled.
void drawlist_frame_begin(void);
void drawlist_frame_end(void);

// Drops the recorded history at a level boundary so a new level never
// interpolates against (or blends filters with) the previous level's frame.
void drawlist_level_reset(void);

// Sets the identity metadata copied into subsequently recorded entries.  Pass
// DL_OBJ_NONE to clear it.
void drawlist_set_context(int obj_kind, int obj_id, int obj_sub);

// Reads the current context back, so a caller that draws something of its own
// (text) can restore what the code after it relied on.  Any pointer may be NULL.
void drawlist_get_context(int *obj_kind, int *obj_id, int *obj_sub);

// --- emission tag buffer ------------------------------------------------------
//
// The Modern bloom/lighting pass reads an 8-bit emission tag alongside the
// 8-bit playfield.  The tag is written by the sprite blits themselves (through
// their drawlist_record_blit_sprite* entry points, which are called whether or
// not recording is enabled), so it follows the object wherever it is drawn:
// the live tick surface and the interpolated replay scratch both get their own
// tag, and an object's tag therefore moves with the interpolated sprite.
//
// Only gameplay surfaces (game_screen, VGAScreen2 and the two replay scratch
// copies) are tagged.  The tag is derived from the current object identity:
// player/enemy shots, explosions and pickups are emissive; every other class
// (ships, HUD, backgrounds, starfield) is stamped DL_TAG_NONE, which also lets
// an opaque ship correctly occlude a shot it is drawn over.
//
// The buffers are fixed-capacity statics; begin is a no-op (and the blits skip
// the work) unless Modern lighting is on, so Classic and lighting-off runs pay
// nothing.

// Starts a tagging session for one tick: clears the live tag buffers and arms
// the blits.  No-op when Modern lighting is off.  Call at tick begin.
void drawlist_tag_begin(void);

// Sets one tag pixel of the game surface `surface` (used for the superpixels,
// which are drawn a pixel at a time and not through a sprite blit).  No-op
// unless tagging is armed and `surface` is a tracked gameplay surface.
void drawlist_tag_pixel(SDL_Surface *surface, int x, int y, int tag);

// Returns the tag buffer matching `surface` (its own pitch and size), or NULL
// when tagging is off / the surface is not tagged.  px_pitch/w/h optional.
const Uint8 *drawlist_tag_for_surface(SDL_Surface *surface, int *out_pitch, int *out_w, int *out_h);

// Returns the parallel per-pixel object-light palette index buffer matching
// `surface` (same geometry as the tag), or NULL when tagging is off.  Every
// pixel a tagged blit draws holds the representative palette index of the
// object's own colour (its dominant saturated shade); the lighting pass uses it
// in place of the pixel's own, often white-hot, colour.  0 means "use the
// pixel's own colour" (VFX, superpixels and clears).
const Uint8 *drawlist_lightcol_for_surface(SDL_Surface *surface, int *out_pitch, int *out_w, int *out_h);

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

// Interpolated versions used by the stage-3 renderer.  `pre` is the captured
// pre-step state; the element is drawn at the position partway (alpha_fx16 in
// 16.16) between the previous frame's position and this tick's advanced
// position.  A wrapped star snaps to its advanced position.
void drawlist_draw_starfield_interp(SDL_Surface *surface, int move_speed, const void *pre, size_t bytes, Uint32 alpha_fx16);
void drawlist_draw_superpixels_interp(SDL_Surface *surface, const void *pre, size_t bytes, Uint32 alpha_fx16);

// Pure pixel-apply halves of the two global-surface operations.
void drawlist_apply_darken(SDL_Surface *surface, JE_word neat);
void drawlist_apply_filter_screen(SDL_Surface *surface, JE_shortint col, JE_shortint int_);

// --- layer check (--regress-layer-check) --------------------------------------
//
// Per level tick: the interpolated frame rendered at alpha = 1 must carry
// exactly the layer buffer (and rank table) the live tick stamped, and the rank
// table must be a consistent permutation.  The totals also tell which layers and
// filters were really exercised.
typedef struct
{
	unsigned long ticks;          // level ticks checked
	unsigned long interp_ticks;   // of those, ticks compared against the interpolated renderer
	unsigned long mismatches;     // layer buffer differed between live and interpolated
	unsigned long rank_bad;       // rank table inconsistent, or live != interpolated
	unsigned long pixels[DL_LAYER_COUNT];  // playfield pixels per layer, summed over ticks
	unsigned long blend_pixels;   // of the BG2 pixels, those written by the blended row
	unsigned long filters[4];     // ticks that ran lava/water/iced/blur (DL_FILTER_*)
	unsigned long rank_orders;    // distinct rank orderings seen
	char first[128];              // first failure, "" when none
} DrawlistLayerStats;

void drawlist_set_layer_check(bool check);
bool drawlist_layer_check_enabled(void);
const DrawlistLayerStats *drawlist_layer_stats(void);

// --- replay-check -------------------------------------------------------------

// Arms the internal replay check (used by --regress-replay-check).  With it
// armed, drawlist_frame_end() replays every recorded tick and reports the first
// mismatching frame/pixel.
void drawlist_set_check(bool check);

// Summary after the run: frames checked and the first mismatch (if any).
unsigned long drawlist_checked_frames(void);
unsigned long drawlist_mismatched_frames(void);
const char *drawlist_first_mismatch(void);   // NULL when all matched

// --- stage 3: interpolation ---------------------------------------------------
//
// Recording keeps the two most recent tick lists (double buffered).  At any
// point after drawlist_frame_end(), the renderer can compose a frame that
// linearly interpolates the positions of the objects present in both ticks.
// The frame is rendered into drawlist's persistent scratch surfaces, which are
// the previous presented frame (so filters that blend with the destination see
// the previous frame, as iced/blur genuinely require).
//
// `alpha_fx16` is a 16.16 fixed-point blend factor: 0 = previous tick,
// 65536 = current tick.  Returns false when there is no usable previous list
// (the caller then presents the plain current tick).
bool drawlist_render_interpolated(Uint32 alpha_fx16);

// The scratch surface the last drawlist_render_interpolated() produced.
SDL_Surface *drawlist_interpolated_game(void);

// Player 0's interpolated position at the last alpha, for the presentation's
// spotlight special code.  Valid after drawlist_render_interpolated().
void drawlist_interpolated_player(int *x, int *y);

// True when a previous list exists to interpolate from.
bool drawlist_has_previous(void);

// --- interpolation diagnostics (--regress-interp-check) -----------------------

void drawlist_set_interp_check(bool check);

// Counts over the run: matched commands, snaps by reason (no previous
// identity / position jump / sprite-sheet change) and position overshoots
// detected by the internal alpha = 0.5 sanity pass.
unsigned long drawlist_interp_matched(void);
unsigned long drawlist_interp_snap_new(void);
unsigned long drawlist_interp_snap_jump(void);
unsigned long drawlist_interp_snap_sheet(void);
unsigned long drawlist_interp_overshoots(void);

// --- smoothness diagnostics (--regress-interp-smoothness) ---------------------
//
// Per level tick, re-derives the interpolated positions at N sub-frame alphas
// and checks that every background layer's presented offset and every matched
// object's position move monotonically between the two ticks (no backtracking,
// no overshoot).  See the implementation for the exact rule.
void drawlist_set_smoothness_check(bool check);
bool drawlist_smoothness_enabled(void);
void drawlist_set_smoothness_alphas(unsigned int count);

unsigned long drawlist_smoothness_ticks(void);
unsigned long drawlist_smoothness_bg_checks(void);
unsigned long drawlist_smoothness_object_checks(void);
unsigned long drawlist_smoothness_horizontal_events(void);
unsigned long drawlist_smoothness_vertical_events(void);
unsigned long drawlist_smoothness_object_events(void);
unsigned long drawlist_smoothness_frames(void);
unsigned long drawlist_smoothness_events(void);

// --- parallax guard (--regress-parallax-check) --------------------------------
//
// Per level tick, runs the interpolated presentation at both ends of the tick
// and requires it to leave the live starfield and the background scroll
// counters untouched, and checks the level logic advanced the starfield by
// exactly the recorded per-tick step.  Catches a starfield/background layer
// advanced per presented frame instead of per tick.
void drawlist_set_parallax_check(bool check);

unsigned long drawlist_parallax_ticks(void);
unsigned long drawlist_parallax_mutations(void);
unsigned long drawlist_parallax_double_updates(void);
unsigned long drawlist_parallax_advance_mismatches(void);

#endif // DRAW_LIST_H
