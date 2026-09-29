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
#ifndef VFX_AMBIENT_H
#define VFX_AMBIENT_H

#include <SDL3/SDL.h>

// Ambient atmosphere: a handful of very faint particles drifting over the
// playfield so a level feels alive.  It is part of the Fase 2 VFX module and
// shares its rules:
//
//   * Modern only, and only with the Effects level on (Off = none, Low/High =
//     the matching density).  Classic never sees it.
//   * simulated once per logic tick, drawn interpolated with the same 16.16
//     alpha as the other particles;
//   * a separate, deterministic xorshift RNG seeded per level; it never calls
//     mt_rand()/rand() and never touches game state;
//   * drawn into the 8-bit playfield as palette indices, but NOT tagged as a
//     light emitter: ambient dust is atmosphere, not a light source (the one
//     possible exception, faint embers, is deliberately kept untagged so the
//     parallel coloured-lighting pass cannot mistake drifting motes for lights).
//
// The level picks the style read-only from the state the game already has:
// the lava/water/iced/blur smoothies it enables and whether the starfield is
// active.  See vfx_ambient.c for the mapping.

// Drops every ambient particle and reseeds the module's RNG for the new level.
void vfx_ambient_reset(void);

// Advances the ambient simulation one logic tick (spawns, drifts, ages out).
// A no-op that clears when ambient is not enabled.
void vfx_ambient_tick(void);

// Draws the interpolated ambient particles into the 8-bit playfield of
// `surface`, `alpha_fx16` is 16.16 (65536 = the current tick).
void vfx_ambient_render(SDL_Surface *surface, Uint32 alpha_fx16);

#endif /* VFX_AMBIENT_H */
