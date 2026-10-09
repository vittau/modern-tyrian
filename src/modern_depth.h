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

// Requests the layer buffer.  Stage 1 only has the regress/debug flags
// (--regress-layer-check, --regress-layer-png); stage 2 adds the `Depth:` setting.
void modern_depth_set_requested(bool requested);

// Clears the presented layer buffer for the frame about to be presented and
// marks it valid.  No-op (and the accessors then return NULL) when layers are
// not wanted.  Called by interp.c right before the copy.
void modern_depth_begin(void);

// Copies the `flip`-aware 264x184 window (source x 24..287) of `game`'s layer
// buffer, and its first-drawn rank table, into the presented buffer.  `game` is
// the presented surface (the live frame or the interpolated scratch).
void modern_depth_from_game(SDL_Surface *game, bool flip);

// Marks one playfield pixel as drawn by the VFX renderer (and the ambient
// particles).  Called wherever modern_bloom_tag_pixel() is: those pixels are
// not part of any gameplay layer and must cast no shadow.
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
