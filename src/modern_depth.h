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
#ifndef MODERN_DEPTH_H
#define MODERN_DEPTH_H

#include "drawlist.h"
#include "modern.h"
#include "modern_bloom.h"

#include <SDL3/SDL.h>

#include <stdbool.h>

// Modern depth layers, stage 1: the per-pixel LAYER buffer (infrastructure only).
//
// drawlist.c stamps, for every pixel of a gameplay surface, which layer painted
// it last (DL_LAYER_*, see drawlist.h).  This module owns the presentation side:
// the 264x184 playfield window of that buffer, copied out of the presented
// surface with the same row/flip mapping as the emission tag, plus the table of
// the order in which the layers were first drawn that tick.  Stage 2 (soft cast
// shadows) reads both through the accessors below.
//
// Nothing here writes game state, calls mt_rand*, or changes a presented pixel.

// True only in the Modern presentation and when depth is requested.  When it is
// false every stamp path in drawlist.c returns immediately, like the tag.
bool modern_depth_layers_wanted(void);

// The `Depth:` setting (Setup -> Graphics, config key modern_depth).  Any level
// above Off opens the layer-buffer gate and enables the shadow pass.  Default
// On; regress runs pin it Off unless --regress-depth asks otherwise.
typedef enum { MODERN_DEPTH_OFF, MODERN_DEPTH_ON, MODERN_DEPTH_MAX } ModernDepth;
extern ModernDepth modern_depth_quality;
extern const char *const modern_depth_names[MODERN_DEPTH_MAX];
// Legacy low/high names both select On; invalid names leave the value unchanged.
bool set_modern_depth_by_name(const char *name, ModernDepth *depth);

// Requests the layer buffer independently of the setting (the regress / debug
// flags --regress-layer-check and --regress-layer-png).
void modern_depth_set_requested(bool requested);

// Clears the presented layer buffer for the frame about to be presented and
// marks it valid.  No-op (and the accessors then return NULL) when layers are
// not wanted.  Called by interp.c right before the copy.
void modern_depth_begin(void);

// Copies the `flip`-aware 264x184 window (source x 24..287) of `game`'s layer
// buffer, and its first-drawn rank table, into the presented buffer.  `game` is
// the presented surface (the live frame or the interpolated scratch).
void modern_depth_from_game(SDL_Surface *game, bool flip);

// Flags one playfield pixel as drawn by the VFX renderer (and the ambient
// particles).  Called wherever modern_bloom_tag_pixel() is.  The pixel KEEPS the
// layer it was painted over and gains DL_LAYER_VFX_FLAG: it never casts a shadow
// (ambient dust must not punch holes in them) but still receives as its layer.
void modern_depth_mark_vfx(int x, int y);

// Read-only access for the stage-2 pass.  Both return NULL unless a frame was
// presented through modern_depth_begin()/modern_depth_from_game().
//
// The layer buffer is MODERN_PLAYFIELD_W x MODERN_PLAYFIELD_H bytes, row-major,
// in presented (post-flip) coordinates.  Each byte is a DL_LAYER_* id, with
// DL_LAYER_BLEND set on a pixel written by the blended bg2 row.
const Uint8 *modern_depth_layer_buffer(void);

// DL_LAYER_COUNT bytes: 1.. = the order in which each layer was first drawn this
// tick (lower = painted earlier = lower), 0 = not drawn this tick.
const Uint8 *modern_depth_rank_table(void);

// --- soft cast shadows ------------------------------------------------------------

// Counters of one modern_depth_shadow_apply() call: caster pixels that landed a
// shadow, per caster layer, and how many of them were blended bg2 pixels.
typedef struct
{
	unsigned long casters[DL_LAYER_COUNT];
	unsigned long blend_casters;
} ModernDepthShadowStats;

// The shadow core, free of globals except its scratch planes.  `canvas` points at
// the playfield origin of an XRGB8888 canvas with `canvas_pitch_px` pixels per
// row; `layers` is the MODERN_PLAYFIELD_W x MODERN_PLAYFIELD_H layer buffer and
// `rank` the DL_LAYER_COUNT first-drawn table.  Darkens the canvas in place and
// returns the number of pixels darkened.  Quality Off is the identity.  Used by
// the pass and by the synthetic fixture (--regress-depth-check).
unsigned long modern_depth_shadow_apply(Uint32 *canvas, int canvas_pitch_px,
                                        const Uint8 *layers, const Uint8 *rank,
                                        ModernDepth quality, ModernDepthShadowStats *stats);

// --- atmospheric fog on bg1 and per-layer light (stage 3) ---------------------------

// Counters of one modern_depth_fog_apply() call.
typedef struct
{
	unsigned long fogged;        // bg1 pixels blended toward the fog colour
	unsigned long blend_fogged;  // translucent bg2 pixels (bg1 shows through) fogged at half strength
} ModernDepthFogStats;

// Derives the fog colour of a frame from the UNFOGGED canvas: the mean colour of
// the bg1 pixels (sampled on a 2x2 grid), desaturated by half toward its luma and
// lightened by about a third toward white.  Returns false (leaving *rgb alone)
// when fewer than MODERN_DEPTH_FOG_MIN_SAMPLES bg1 pixels were sampled.
#define MODERN_DEPTH_FOG_MIN_SAMPLES 256
bool modern_depth_fog_colour(const Uint32 *canvas, int canvas_pitch_px, const Uint8 *layers, Uint32 *rgb);

// Blends every bg1 pixel (not a VFX pixel) of `canvas` toward `fog_rgb` by a small
// fixed amount; translucent bg2 pixels get half of it, everything else none.
// Quality Off is the identity.  Returns the number of pixels fogged.
unsigned long modern_depth_fog_apply(Uint32 *canvas, int canvas_pitch_px, const Uint8 *layers,
                                     ModernDepth quality, Uint32 fog_rgb, ModernDepthFogStats *stats);

// Q8 weight (256 = full) of the light a pixel receives from the lighting pass,
// by its layer byte.  A VFX-flagged pixel and any non-world layer take 256.
unsigned modern_depth_light_weight(Uint8 layer_byte);

// Tuning override (--regress-fog-strength=Q8): the bg1 fog blend strength; < 0 restores 30.
void modern_depth_set_fog_strength(int q8);

// The layer buffer the lighting pass may weight by, or NULL when this frame has
// no per-layer light (Depth Off, not a fresh gameplay frame, a starfield level).
// Valid only between modern_depth_pass() and the end of the same frame.
const Uint8 *modern_depth_light_layers(void);

// Called by the lighting pass with the number of lit pixels per receiving layer
// of this frame (DL_LAYER_COUNT entries), how many of them took a weight < 256 and the
// performance-counter ticks the split combine + layered apply took.
void modern_depth_note_light(const unsigned long *lit_by_layer, unsigned long reduced, Uint64 ticks);

// The registered Modern pass (before the bloom/lighting pass).  Gameplay frames
// only, and only when the layer buffer of this very frame is valid.
void modern_depth_pass(ModernFrame *frame);

// Logs the "Depth shadows:" coverage line.
void modern_depth_log_stats(void);

// --- held in-level screens (modern_held.c) --------------------------------------

// Snapshots the presented layer buffer and rank table (the frame that is about to
// be shown).  Returns false, leaving no snapshot, when layers are not wanted or
// the frame has no buffer.
bool modern_depth_held_save(void);

// Re-arms the layer buffer from the snapshot for a held frame, with every pixel
// of `overlay` (MODERN_PLAYFIELD_W x MODERN_PLAYFIELD_H, nonzero = overlay)
// turned into DL_LAYER_OTHER: it neither casts nor receives.  No-op, leaving the
// frame without shadows, when there is no snapshot or layers are not wanted.
void modern_depth_held_restore(const Uint8 *overlay);

// Drops the snapshot.
void modern_depth_held_forget(void);

// --- regress / debug ------------------------------------------------------------

// Counts the presented frames the buffer was copied for: total, from the
// interpolated surface, and with the vertical-flip mapping.
unsigned long modern_depth_presented_frames(void);
unsigned long modern_depth_presented_interpolated(void);
unsigned long modern_depth_presented_flipped(void);

// Writes the presented layer buffer as a false-colour PNG (one distinct colour
// per layer, DL_LAYER_BLEND tinted).  For local review only.  Returns false on
// failure or when no buffer was presented.
bool modern_depth_save_png(const char *path);

#endif // MODERN_DEPTH_H
