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
#ifndef VFX_H
#define VFX_H

#include <SDL3/SDL.h>

#include <stdbool.h>

// Fase 2 VFX: a read-only per-tick event queue plus a deterministic particle
// system on the logical 320x200 grid (Modern only).
//
// The gameplay code calls the vfx_event_*() hooks at the points where it
// already spawns explosions, superpixel bursts, shots, impacts and deaths.
// Those hooks are observation only: they never call mt_rand()/rand(), never
// write game state, and are no-ops in Classic (and during regress unless VFX
// are explicitly enabled).  Events land in a fixed-capacity ring buffer.
//
// Once per logic tick, vfx_tick_end() drains the queue and advances the
// particles one step with the module's own deterministic RNG (xorshift32, never
// mt_rand).  Each particle keeps its position for the previous and the current
// tick; at presentation time vfx_render_playfield() draws them into the 8-bit
// playfield of the presentation surface, interpolated by the same 16.16 alpha
// the draw-list interpolator uses.  Writing the VFX as palette indices before
// the Modern conversion means the (future) bloom/lighting pass, which reads the
// emissive palette luminance of the source frame, sees the bright particles.
//
// Colours use the game's own nibble convention: palette index = hue * 16 +
// value, where the high nibble selects one of the 16 palette blocks and the low
// nibble its lit amount.  That is exactly how superpixels and the blend
// primitives work, so the particles stay palette-consistent and follow the
// palette fades with the rest of the frame.

// --- intensity level (cfg [video] vfx, --vfx, Setup > Graphics) -------------

typedef enum
{
	VFX_OFF = 0,
	VFX_LOW,
	VFX_MEDIUM,
	VFX_HIGH,
	VFX_LEVEL_MAX
} VfxLevel;

extern const char *const vfx_level_names[VFX_LEVEL_MAX];
extern VfxLevel vfx_level;

// Parses "off"/"low"/"medium"/"high" (case-insensitive).  Returns false for
// anything else, leaving the setting unchanged.
bool set_vfx_by_name(const char *name);

// Accessibility hooks kept internal for now (the menu exposes only the level):
// a single intensity scale (256 = normal, lower spawns fewer/smaller effects)
// and a "reduce flashes" boolean that scales the flash-type effects down.
extern int vfx_intensity;
extern bool vfx_reduce_flashes;

// True when VFX should run: Modern presentation and a non-off level.
bool vfx_enabled(void);

// --- lifecycle ---------------------------------------------------------------

// Drops every particle and event; called when a level starts.
void vfx_reset(void);

// Advances the simulation one logic tick: rolls the previous/current positions,
// integrates velocities and lifetimes, then spawns the queued events.  A no-op
// (with a reset) when VFX are disabled.
void vfx_tick_end(void);

// Draws the interpolated particle state into the 8-bit playfield of `surface`
// (the VGAScreenSeg presented playfield, 264x184 at the top-left).  `alpha_fx16`
// is 16.16 (65536 = the current tick), matching src/interp.c.  A no-op when VFX
// are disabled.
void vfx_render_playfield(SDL_Surface *surface, Uint32 alpha_fx16);

// --- event hooks (observation only) -----------------------------------------
//
// Coordinates are the game's own (game_screen) coordinates, the same value the
// caller passes to the drawing primitives; the module converts them to
// playfield-local coordinates (x - 24).

// A standard explosion was created by JE_setupExplosion (type 0..52, ttl ticks).
void vfx_event_explosion(int x, int y, int ttl, int type);

// A large explosion was created by JE_setupExplosionLarge.  `power` is 1 for a
// normal one and 2 for a boss kill.
void vfx_event_explosion_large(int x, int y, bool ground, int power);

// A superpixel burst was created by JE_doSP (already drawn by the game; this
// only adds a few palette-matched sparks with the same colour block).
void vfx_event_superpixels(int x, int y, int num, int width, int colour);

// A shot was fired: `dirx`/`diry` are the shot's per-tick velocity (may be 0).
void vfx_event_shot(int x, int y, int dirx, int diry);
void vfx_event_enemy_shot(int x, int y, int dirx, int diry);

// An enemy was destroyed.  `linknum` is the enemy's link and `boss` (derived
// from it against boss_bar[]) adds the larger shockwave.  `ground` makes the
// debris scroll with the ground layer.
void vfx_event_enemy_death(int x, int y, bool big, bool ground, int linknum);

// An impact flash landed on an enemy (hue is the palette colour block).
void vfx_event_impact(int x, int y, int hue);

// A hit landed on a player or the player died.
void vfx_event_player_hit(int x, int y, int damage);
void vfx_event_player_death(int x, int y);

#endif /* VFX_H */
