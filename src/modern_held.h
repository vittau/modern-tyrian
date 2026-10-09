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
#ifndef MODERN_HELD_H
#define MODERN_HELD_H

#include "modern.h"

#include <SDL3/SDL.h>

#include <stdbool.h>

// Modern depth shadows and bloom/lighting on HELD in-level frames.
//
// The pause screen, the in-game menu (and the network wait boxes it shows) and
// the in-game help present the frozen playfield straight through JE_showVGA(),
// not through interp_blit_playfield(), so no layer buffer or emission tag is
// produced for them.  Instead of dropping the shadows and the light the moment
// the screen opens, the last frame that WAS really presented leaves a snapshot
// (layer buffer, rank table, emission tag, light colours, and the playfield
// palette indices), and a held frame reuses it.
//
// The overlay (the menu window, the PAUSED text, any darkening, the mouse
// cursor) is found by comparing the held frame's playfield indices with the
// snapshot of the indices: a changed pixel is overlay and is excluded from
// everything: it neither casts nor receives a shadow, carries no emission tag,
// and is not lit.  Everything else is the frozen picture, drawn exactly as the
// last live frame was.
//
// Presentation only: no game state, no mt_rand*, fixed-capacity statics.

// Marks the in-level held screens (pause, in-game menu, in-game help).  Set
// next to modern_set_gameplay_hold(true/false) by those screens only: the level
// intro, the Warning and the end-of-level / death fades hold the gameplay
// composition too but never reuse a snapshot.  Turning it off drops no
// snapshot, the next real frame replaces it.
void modern_held_set(bool held);

// Forgets the snapshot (a new level load).
void modern_held_reset(void);

// Called by interp_blit_playfield() right before the frame is presented, after
// the VFX are drawn: snapshots the layer buffer, the emission tag and the
// playfield palette indices of the frame about to be shown.  `playfield` is the
// presented 8-bit surface (VGAScreenSeg).
void modern_held_capture(const SDL_Surface *playfield);

// Called by modern_build_frame() on every frame before the passes run.  When a
// held in-level screen is up and a snapshot exists, builds the overlay mask and
// re-arms the depth and bloom buffers from the snapshot (overlay cleared); the
// passes then run as on a live frame.  `plain_playfield` is false when the
// frame is not the plain playfield composition (a procedural screen or a modal
// dialog), in which case nothing is reused.
void modern_held_prepare(const ModernFrame *frame, bool plain_playfield);

// Called by modern_build_frame() after the passes: checks that overlay pixels
// of the canvas did not change and feeds the counters / --regress-held-check.
void modern_held_finish(const ModernFrame *frame);

// The overlay mask of the frame being built (MODERN_PLAYFIELD_W x
// MODERN_PLAYFIELD_H, 1 = overlay), or NULL unless this frame reuses a snapshot.
const Uint8 *modern_held_overlay(void);

// --- regress / debug ------------------------------------------------------------

// Enables --regress-held-check: remembers the post-pass playfield of the last
// live frame and compares every held frame against it outside the overlay.
void modern_held_set_check(bool check);

// Logs the "Depth held:", "Light held:" and "Held check:" coverage lines.
void modern_held_log_stats(void);

// Stats owned by the passes (added to the held counters): shadow pixels written
// on a held frame, and pixels lit by the bloom/lighting pass on a held frame.
void modern_held_note_shadow(unsigned long shadowed, bool space);
void modern_held_note_light(unsigned long emitters, unsigned long lit);
// Stage 3: bg1 pixels fogged on a held frame, and lit pixels that took a per-layer
// light weight below full on a held frame.
void modern_held_note_fog(unsigned long fogged);
void modern_held_note_light_layers(unsigned long reduced);

#endif // MODERN_HELD_H
