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
#include "vfx_ambient.h"

#include "config.h"
#include "backgrnd.h"
#include "vfx.h"

#include <stddef.h>
#include <string.h>

// Ambient atmosphere.  See vfx_ambient.h for the contract.
//
// ---------------------------------------------------------------------------
// Style selection (read-only)
// ---------------------------------------------------------------------------
//
// The level already says what it is through the settings the game keeps for
// itself: the nine smoothie bits (JE_checkSmoothies / event type 64) pick the
// lava, water, iced-blur and blur full-screen filters, and starActive says
// whether the starfield runs.  The ambient style is a small priority read of
// exactly those flags, once per tick:
//
//   lava    (smoothies[0])                     -> EMBERS  (warm, rising)
//   iced    (smoothies[2] or smoothies[4])     -> SNOW   (cold flakes, falling)
//   water   (smoothies[1]) or blur (smoothies[3]) -> MIST (cool, near-still haze)
//   starfield active (starActive)              -> SPACE  (fine dust drifting with the stars)
//   otherwise                                  -> DUST   (sparse neutral motes)
//
// The priorities put fire above the cold filters (a level that turns lava on
// over an iced background should read as fire), and water/blur share MIST
// because both are "cool haze" filters.  The two full-screen combat filters
// (levelFilter/levelBrightness, used by the flare weapon and the level fades)
// are deliberately NOT inputs: they are transient weapon/fade tints, not level
// identity, and keying ambient off them would spawn embers every time the
// player fires a flare.  The background palette hue is not read either: the
// playfield mixes three parallax layers plus sprites, so there is no single
// background hue to sample without walking the tile map, and the smoothie bits
// already carry the level's intent at a fraction of the cost.
//
// A level can switch effects part-way through (E4:L9 is water, then lava), so
// the style is re-read every tick; particles already in flight keep their own
// style until they age out, which cross-fades the two looks instead of popping.

// ---------------------------------------------------------------------------
// Own deterministic RNG (xorshift32), separate from vfx.c so the ambient
// stream can never perturb the event-particle stream.
// ---------------------------------------------------------------------------

static Uint32 ambient_rng = 0x2545F491u;
static Uint32 ambient_serial = 0;   // increments per level, so each level's seed differs

static Uint32 ambient_rand(void)
{
	Uint32 x = ambient_rng;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	ambient_rng = x;
	return x;
}

static int ambient_rand_range(int n)
{
	return n > 0 ? (int)(ambient_rand() % (Uint32)n) : 0;
}

// Random in [-n, n].
static int ambient_rand_bipolar(int n)
{
	return n > 0 ? ambient_rand_range(2 * n + 1) - n : 0;
}

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------

// The presented playfield, the same 264x184 window vfx.c draws into.
#define AMB_W 264
#define AMB_H 184

// 16.16 helpers.  VFX_FP() multiplies instead of shifting so a negative value
// stays well defined in C99.
#define AMB_FP(n) ((Sint32)(n) * 65536)
#define AMB_HALF  (1 << 15)

// A hard cap keeps the worst case bounded; the per-style targets below are far
// smaller and are the real density control.
#define AMB_MAX 48

enum
{
	AMB_NONE = 0,
	AMB_DUST,     // default ground: sparse neutral motes
	AMB_SPACE,    // starfield/space: fine dust drifting with the stars
	AMB_EMBERS,   // lava: warm embers rising off the ground
	AMB_MIST,     // water/blur: cool, near-still haze
	AMB_SNOW      // iced/blur: light snow falling
};

typedef struct
{
	Sint32 x, y;      // 16.16 current position
	Sint32 px, py;    // 16.16 position at the previous tick
	Sint32 vx, vy;    // 16.16 per-tick velocity
	Uint8 life;
	Uint8 max_life;
	Uint8 style;
	Uint8 value;      // brightness amount (lighten) or palette value (blend)
	Uint8 hue;        // blend hue, fixed at spawn (mist uses the liquid's block)
	Uint8 phase;      // sway/flicker counter
} AmbientParticle;

static AmbientParticle ambient_particles[AMB_MAX];
static int ambient_count = 0;

// Defined with the renderer below; needed at spawn to freeze the haze colour.
static int ambient_mist_hue(void);

static int ambient_style(void)
{
	if (!vfx_enabled())
		return AMB_NONE;

	if (smoothies[0])
		return AMB_EMBERS;
	if (smoothies[2] || smoothies[4])
		return AMB_SNOW;
	if (smoothies[1] || smoothies[3])
		return AMB_MIST;
	if (starActive)
		return AMB_SPACE;

	return AMB_DUST;
}

// Per-style target count at the current Effects level, scaled by the
// accessibility intensity.  Low is deliberately very sparse; High is roughly
// double Low, never a wall of particles.
static int ambient_target(int style)
{
	const int hi = (vfx_level == VFX_HIGH);
	int n;

	switch (style)
	{
	case AMB_DUST:   n = hi ? 22 : 9;  break;
	case AMB_SPACE:  n = hi ? 26 : 11; break;
	case AMB_EMBERS: n = hi ? 24 : 9;  break;
	case AMB_MIST:   n = hi ? 12 : 5;  break;
	case AMB_SNOW:   n = hi ? 28 : 11; break;
	default:         return 0;
	}

	n = (n * vfx_intensity) / 256;
	return n < 0 ? 0 : n;
}

// The per-tick background scroll the particles share: the ground layers use
// backMove, the starfield its own speed.  Kept as a signed pixel count.
static int ambient_scroll(int style)
{
	if (style == AMB_SPACE)
		return starfield_speed;
	return (int)backMove;
}

static void ambient_spawn(int style)
{
	if (ambient_count >= AMB_MAX)
		return;

	AmbientParticle *p = &ambient_particles[ambient_count++];
	memset(p, 0, sizeof *p);

	const int scroll = ambient_scroll(style);

	switch (style)
	{
	case AMB_EMBERS:
		// Born low over the lava, rising against the scroll with a little sway.
		p->x = AMB_FP(ambient_rand_range(AMB_W));
		p->y = AMB_FP(AMB_H - 1 - ambient_rand_range(28));
		p->vx = AMB_FP(ambient_rand_bipolar(1));
		p->vy = -AMB_FP(1 + ambient_rand_range(2)) + AMB_FP(scroll) / 4;
		p->value = 4 + ambient_rand_range(4);
		p->max_life = p->life = (Uint8)(34 + ambient_rand_range(46));
		break;

	case AMB_SNOW:
		// Born at the top, falling with a slow zig-zag.
		p->x = AMB_FP(ambient_rand_range(AMB_W));
		p->y = AMB_FP(ambient_rand_range(28));
		p->vx = AMB_FP(ambient_rand_bipolar(1));
		p->vy = AMB_FP(1 + ambient_rand_range(2)) + AMB_FP(scroll) / 4;
		p->value = 3 + ambient_rand_range(4);
		p->max_life = p->life = (Uint8)(70 + ambient_rand_range(60));
		break;

	case AMB_MIST:
		// Anywhere, barely moving.
		p->x = AMB_FP(ambient_rand_range(AMB_W));
		p->y = AMB_FP(ambient_rand_range(AMB_H));
		p->vx = AMB_FP(ambient_rand_bipolar(1)) / 2;
		p->vy = AMB_FP(scroll) / 3 + AMB_HALF;
		p->value = 1 + ambient_rand_range(2);
		p->max_life = p->life = (Uint8)(90 + ambient_rand_range(80));
		break;

	case AMB_SPACE:
	case AMB_DUST:
	default:
		// Anywhere, drifting down with the background.
		p->x = AMB_FP(ambient_rand_range(AMB_W));
		p->y = AMB_FP(ambient_rand_range(AMB_H));
		p->vx = AMB_FP(ambient_rand_bipolar(1));
		p->vy = AMB_FP(scroll) + AMB_HALF;
		p->value = 1 + ambient_rand_range(2);
		p->max_life = p->life = (Uint8)(120 + ambient_rand_range(90));
		break;
	}

	p->px = p->x;
	p->py = p->y;
	p->style = (Uint8)style;
	p->hue = (Uint8)(style == AMB_MIST ? ambient_mist_hue() : (style == AMB_EMBERS ? 7 : 0));
}

void vfx_ambient_reset(void)
{
	ambient_count = 0;
	// A new level gets a new deterministic stream so two levels never share the
	// exact same mote pattern.
	ambient_rng = 0x2545F491u ^ (++ambient_serial * 0x9E3779B9u);
}

void vfx_ambient_tick(void)
{
	if (!vfx_enabled())
	{
		ambient_count = 0;
		return;
	}

	// Integrate and age out.  Off-screen particles are dropped so a drifting
	// one is recycled instead of sitting invisibly at the edge.
	int w = 0;
	for (int i = 0; i < ambient_count; ++i)
	{
		AmbientParticle *p = &ambient_particles[i];

		p->px = p->x;
		p->py = p->y;
		p->x += p->vx;
		p->y += p->vy;

		// Slow zig-zag for the falling/rising styles: flip the horizontal
		// drift every few ticks.  Position stays continuous between ticks.
		if ((p->style == AMB_SNOW || p->style == AMB_EMBERS) && (++p->phase & 7) == 0)
			p->vx = -p->vx;

		const Sint32 mx = p->x >> 16, my = p->y >> 16;
		if (--p->life == 0 || mx < -8 || mx > AMB_W + 8 || my < -8 || my > AMB_H + 8)
			continue;

		ambient_particles[w++] = *p;
	}
	ambient_count = w;

	// Refill towards the target, one particle per tick so the level does not
	// start with a burst.
	const int style = ambient_style();
	const int target = ambient_target(style);
	if (target > 0 && ambient_count < target)
		ambient_spawn(style);
}

// ---------------------------------------------------------------------------
// Rendering (Modern presentation only, into the 8-bit playfield)
// ---------------------------------------------------------------------------

// The engine's nibble blend, copied from vfx.c/blit_sprite2_blend.  Ambient
// uses it sparingly: it changes the destination hue, so it is reserved for the
// styles that need a colour (embers, mist, snow).
static void ambient_blend_at(Uint8 *base, int pitch, int x, int y, int hue, int value)
{
	if ((unsigned)x >= (unsigned)AMB_W || (unsigned)y >= (unsigned)AMB_H)
		return;

	Uint8 *s = base + (size_t)y * (size_t)pitch + (size_t)x;
	*s = (Uint8)((((*s & 0x0f) + (value & 0x0f)) / 2) | ((hue & 15) << 4));
}

// The most conservative mark: brighten the existing hue by one or two steps.
// It cannot recolour a ship or a shot, so it keeps them the clearest thing on
// screen.
static void ambient_lighten_at(Uint8 *base, int pitch, int x, int y, int amount)
{
	if ((unsigned)x >= (unsigned)AMB_W || (unsigned)y >= (unsigned)AMB_H)
		return;

	Uint8 *s = base + (size_t)y * (size_t)pitch + (size_t)x;
	int v = (*s & 0x0f) + amount;
	if (v > 15)
		v = 15;
	*s = (Uint8)((*s & 0xf0) | v);
}

// An opaque palette pixel; used only for the rare bright core of an ember.
static void ambient_put_at(Uint8 *base, int pitch, int x, int y, int hue, int value)
{
	if ((unsigned)x >= (unsigned)AMB_W || (unsigned)y >= (unsigned)AMB_H)
		return;

	Uint8 *s = base + (size_t)y * (size_t)pitch + (size_t)x;
	*s = (Uint8)(((hue & 15) << 4) | (value & 15));
}

// Interpolated integer position (same 16.16 alpha as vfx.c/interp.c).
static void ambient_interp(const AmbientParticle *p, Uint32 alpha, int *ox, int *oy)
{
	const Sint32 dx = p->x - p->px;
	const Sint32 dy = p->y - p->py;
	*ox = (int)((p->px + (Sint32)(((Sint64)dx * (Sint32)alpha) >> 16)) >> 16);
	*oy = (int)((p->py + (Sint32)(((Sint64)dy * (Sint32)alpha) >> 16)) >> 16);
}

// Life normalised to 0..16, with the last stretch fading so nothing pops.
static int ambient_fade(const AmbientParticle *p)
{
	int life = p->life;
	if (life > 8)
		return 16;
	return life * 2;
}

// Mist takes the colour of the level's own liquid when the water smoothie is
// on (water_filter tints with smoothie_data[1]); a generic blur has no colour,
// so it falls back to the engine's blue energy block.  Read at spawn and kept
// on the particle so a mid-level effect change cannot recolour it in flight.
static int ambient_mist_hue(void)
{
	if (smoothies[1])
		return smoothie_data[1] & 15;
	return 9;
}

static void ambient_draw(const AmbientParticle *p, Uint8 *base, int pitch, Uint32 alpha)
{
	int x, y;
	ambient_interp(p, alpha, &x, &y);

	const int fade = ambient_fade(p);

	switch (p->style)
	{
	case AMB_EMBERS:
	{
		// Rising embers: lift the lava towards orange, and let the brightest
		// few carry an opaque hot core.  No light tag: see vfx_ambient.h.
		const int value = p->value * fade / 16;
		if (value <= 0)
			break;
		ambient_lighten_at(base, pitch, x, y, value);
		if (p->value >= 6 && (p->phase & 4) != 0)
			ambient_put_at(base, pitch, x, y, 7, 10 + ((p->value + p->phase) & 3));
		break;
	}

	case AMB_MIST:
	{
		// A very faint haze disc in the level's own liquid colour: centre once,
		// an L-shaped rim dimmer.  Never more than a two-step lift.
		const int value = p->value * fade / 16;
		if (value <= 0)
			break;
		ambient_blend_at(base, pitch, x, y, p->hue, value);
		ambient_blend_at(base, pitch, x + 1, y, p->hue, value);
		ambient_blend_at(base, pitch, x, y + 1, p->hue, value);
		ambient_blend_at(base, pitch, x - 1, y + 1, p->hue, value / 2);
		break;
	}

	case AMB_SNOW:
	{
		// Light flakes: brighten the underlying hue so snow never turns into
		// dark specks on a bright surface; the rarest ones get a white pixel.
		const int value = p->value * fade / 16;
		if (value <= 0)
			break;
		ambient_lighten_at(base, pitch, x, y, value + 1);
		if (p->value >= 6 && (p->phase & 2) != 0)
			ambient_put_at(base, pitch, x, y, 0, 13);
		break;
	}

	case AMB_SPACE:
	{
		// Fine, colour-preserving dust: the tiniest lift, so it can pass for a
		// distant star without competing with the real ones.
		const int value = p->value * fade / 16;
		if (value <= 0)
			break;
		ambient_lighten_at(base, pitch, x, y, value);
		break;
	}

	case AMB_DUST:
	default:
	{
		// Sparse, neutral, colour-preserving motes.
		const int value = p->value * fade / 16;
		if (value <= 0)
			break;
		ambient_lighten_at(base, pitch, x, y, value);
		break;
	}
	}
}

void vfx_ambient_render(SDL_Surface *surface, Uint32 alpha_fx16)
{
	if (!vfx_enabled() || surface == NULL || surface->pixels == NULL)
		return;

	Uint8 *base = surface->pixels;
	const int pitch = surface->pitch;

	for (int i = 0; i < ambient_count; ++i)
		ambient_draw(&ambient_particles[i], base, pitch, alpha_fx16);
}
