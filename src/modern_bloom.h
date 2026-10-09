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
#ifndef MODERN_BLOOM_H
#define MODERN_BLOOM_H

#include "modern.h"

#include <stdbool.h>

// Modern lighting quality, shared by the two effects of this module:
//
//   * `modern_bloom`    — a tight additive glow around the brightest pixels
//     (shots, lasers, explosions, engine flames).  This is Phase 1 "Bloom".
//   * `modern_lighting` — a wide coloured illumination map: the emissive
//     pixels spread the colour of the object that emits them (its dominant
//     saturated shade, not the white-hot core of its brightest pixels) over
//     the surrounding playfield, so terrain, enemies and the ship read as lit
//     by nearby fire (Phase 2 "Luzes dinâmicas").
//
// Three levels: OFF, LOW and HIGH.  HIGH is the pre-merge LOW; LOW is half of
// it.  The default outside Modern is OFF (pinned in regression mode); a Modern
// user gets LOW and can pick a level from the in-game settings, the config file
// or the command line.
typedef enum
{
	MODERN_QUALITY_OFF = 0,
	MODERN_QUALITY_LOW,
	MODERN_QUALITY_HIGH,
	MODERN_QUALITY_MAX
} ModernQuality;

extern const char *const modern_quality_names[MODERN_QUALITY_MAX];

extern ModernQuality modern_bloom_quality;
extern ModernQuality modern_lighting_quality;

// Parses "off", "low" or "high" into `*quality`; "medium" is accepted as the
// pre-merge name of HIGH.  Returns false (leaving it unchanged) for any other
// value.
bool set_modern_quality_by_name(const char *name, ModernQuality *quality);

// True when Modern is presenting and at least one of the two effects is on, so
// the emission tag buffer is worth building (and the blits should tag).  False
// for Classic, for both effects off, and before modern_init().
bool modern_lighting_tags_wanted(void);

// The effect pass.  Registered by modern_init() and run on every Modern
// frame; it does nothing unless the frame is a gameplay frame and at least
// one of the two effects is on.  It only reads `frame->src`/`frame->palette`
// and writes the playfield rectangle of `frame->pixels`, so it is
// deterministic, allocation-free and cannot touch game state.
void modern_bloom_pass(ModernFrame *frame);

// --- Emission tag (playfield) -------------------------------------------------
//
// The pass derives its light from the 8-bit playfield only where the per-pixel
// emission tag says so.  `interp.c` fills the playfield tag from the game's
// tag buffer (drawlist_tag_for_surface) right after it copies the playfield,
// and the VFX call modern_bloom_tag_pixel() as they draw.  The tag is cleared
// and marked valid by modern_bloom_tag_begin(), and the pass consumes it (a
// frame that never called begin does not emit).

// Clears the playfield tag for the frame about to be presented and marks it
// valid.  No-op (and the pass then emits nothing) when lighting is off.
void modern_bloom_tag_begin(void);

// Copies the `flip`-aware 264x184 playfield window (source x 24..287) out of a
// game tag buffer into the playfield tag.  `game_tag` may be NULL (then the
// tag is just cleared).  Called by interp.c after the playfield copy.
void modern_bloom_tag_from_game(const Uint8 *game_tag, int game_pitch, bool flip);

// Copies the matching window of the per-pixel object-light palette index
// buffer (drawlist_lightcol_for_surface) into the playfield light-colour buffer.
// `game_lcol` may be NULL (then the whole buffer is left at 0, meaning "use the
// pixel's own colour").  Called by interp.c right after modern_bloom_tag_from_game().
void modern_bloom_lightcol_from_game(const Uint8 *game_lcol, int game_pitch, bool flip);

// Held in-level screens (modern_held.c): snapshot of the presented frame's tag
// and light colours, and the re-arming of a held frame from it.  The pixels of
// `overlay` (MODERN_PLAYFIELD_W x MODERN_PLAYFIELD_H, nonzero = overlay) lose
// their tag, so nothing under the menu window or the PAUSED text emits.
// Save returns false, leaving no snapshot, when the frame has no tag.
bool modern_bloom_held_save(void);
void modern_bloom_held_restore(const Uint8 *overlay);
void modern_bloom_held_forget(void);

// Marks one playfield pixel as VFX emission.  Called by the VFX renderer.
void modern_bloom_tag_pixel(int x, int y);

// Enables the per-class emitting-pixel counters (--light-tag-stats); prints
// them at exit.  Debug only, never part of the pass output.
void modern_bloom_set_stats(bool enabled);

// Debug tuning override (--light-threshold=N): force both effect thresholds to
// N instead of the per-level values.  N < 0 restores the table.
void modern_bloom_set_threshold(int threshold);

// Tuning flags (--regress-light-scale=PERCENT, --regress-light-radius=N):
// scale the light gain / force its quarter-resolution blur radius.  < 0 = off.
void modern_bloom_set_light_scale(int percent);
void modern_bloom_set_light_radius(int radius);

// --- Per-object light sources (extension point) -----------------------------
//
// Today the light map is derived entirely from the emissive pixels of the
// 8-bit source.  The Phase 2 draw list being built elsewhere will eventually
// report exact per-object lights (a shot, an explosion, an engine flame) with
// a position and radius; those can be pushed here and will be merged into the
// same light map.
//
// The coordinate space is the logical playfield: x in [0, MODERN_PLAYFIELD_W),
// y in [0, MODERN_PLAYFIELD_H), with the pixel at that position receiving the
// light.  `radius` is in logical pixels.  `r`/`g`/`b` are the light colour.
//
// Sources are consumed and cleared by the next modern_bloom_pass(); they are
// additive with the pixel-derived lights.  Not used yet — nothing calls
// modern_lighting_add_source() in this task.
#define MODERN_LIGHT_MAX_SOURCES 64

void modern_lighting_reset_sources(void);
void modern_lighting_add_source(int x, int y, int radius, Uint8 r, Uint8 g, Uint8 b);

#endif /* MODERN_BLOOM_H */
