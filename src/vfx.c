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
#include "vfx.h"

#include "modern.h"
#include "modern_bloom.h"
#include "tyrian2.h"
#include "varz.h"

#include <SDL3/SDL.h>

#include <stddef.h>
#include <string.h>

// Fase 2 VFX.  See vfx.h for the contract.

const char *const vfx_level_names[VFX_LEVEL_MAX] =
{
	"off",
	"low",
	"high",
};

VfxLevel vfx_level = VFX_LOW;
int vfx_intensity = 256;         // 256 = normal; lower spawns fewer/smaller
bool vfx_reduce_flashes = false; // scale the flash-type effects down

// The game draws the 320x200 frame with the playfield at columns
// [24, 288) x [0, 184); the presentation copy (interp_blit_playfield) lifts the
// 264-wide playfield out to x = 0.  Events arrive in game coordinates and are
// converted to this playfield-local space.
#define VFX_PLAYFIELD_X 24
#define VFX_PLAYFIELD_W 264
#define VFX_PLAYFIELD_H 184

// Fixed point 16.16 helpers.  VFX_FP() multiplies instead of shifting left so a
// negative value is well defined in C99 (a left shift of a negative value is
// undefined); every caller passes a value well inside the 16.16 range.
#define VFX_FP(n) ((Sint32)(n) * 65536)
#define VFX_FPHALF (1 << 15)

// Palette colour blocks of the Tyrian VGA palette (index = block * 16 + value).
// The values match the engine's own nibble blending: the high nibble is the
// colour block, the low nibble its lit amount.  See the palette table.
#define VFX_HUE_FIRE   7   // dark red -> orange -> yellow -> white
#define VFX_HUE_SMOKE  0   // grayscale ramp
#define VFX_HUE_DEBRIS 1   // brown/gold ramp
#define VFX_HUE_ENERGY 9   // blue ramp
#define VFX_HUE_HIT    4   // red/pink ramp

// ---------------------------------------------------------------------------
// Own deterministic RNG (xorshift32).  Never mt_rand()/rand(); seeded with a
// fixed constant on reset so the whole effect stream is reproducible.
// ---------------------------------------------------------------------------

static Uint32 vfx_rng = 0x9E3779B9u;

static Uint32 vfx_rand(void)
{
	Uint32 x = vfx_rng;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	vfx_rng = x;
	return x;
}

static int vfx_rand_range(int n)
{
	return n > 0 ? (int)(vfx_rand() % (Uint32)n) : 0;
}

// Random in [-n, n].
static int vfx_rand_bipolar(int n)
{
	return n > 0 ? vfx_rand_range(2 * n + 1) - n : 0;
}

// 16 directions, cos/sin scaled by 127.
static const Sint8 vfx_dir_x[16] =
	{ 127, 117, 90, 49, 0, -49, -90, -117, -127, -117, -90, -49, 0, 49, 90, 117 };
static const Sint8 vfx_dir_y[16] =
	{ 0, 49, 90, 117, 127, 117, 90, 49, 0, -49, -90, -117, -127, -117, -90, -49 };

// ---------------------------------------------------------------------------
// Event queue (fixed capacity; no per-frame allocation)
// ---------------------------------------------------------------------------

enum
{
	VFX_EV_NONE = 0,
	VFX_EV_EXPLOSION,
	VFX_EV_EXPLOSION_LARGE,
	VFX_EV_SUPERPIXELS,
	VFX_EV_SHOT,
	VFX_EV_ENEMY_SHOT,
	VFX_EV_ENEMY_DEATH,
	VFX_EV_PLAYER_HIT,
	VFX_EV_PLAYER_DEATH,
	VFX_EV_IMPACT
};

#define VFX_EVENT_QUEUE 1024

typedef struct
{
	Sint16 x, y;    // playfield-local
	Sint16 vx, vy;  // per-tick direction hint
	Uint16 ttl;     // lifetime/intensity hint
	Uint8 type;
	Uint8 hue;
	Uint8 size;
	Uint8 flags;    // 1 = ground, 2 = big/boss
} VfxEvent;

static VfxEvent vfx_events[VFX_EVENT_QUEUE];
static int vfx_event_count = 0;

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------

enum
{
	VFX_KIND_SPARK = 0,
	VFX_KIND_DEBRIS,
	VFX_KIND_SMOKE,
	VFX_KIND_RING,
	VFX_KIND_FLASH
};

typedef struct
{
	Sint32 x, y;      // 16.16 current position
	Sint32 px, py;    // 16.16 position at the previous tick
	Sint32 vx, vy;    // 16.16 per-tick velocity
	Sint32 rad;       // 16.16 ring/smoke radius
	Sint32 prad;      // 16.16 radius at the previous tick
	Sint32 grow;      // 16.16 radius growth per tick
	Uint8 life;
	Uint8 max_life;
	Uint8 kind;
	Uint8 hue;
	Uint8 value;
	Uint8 flags;
} VfxParticle;

#define VFX_MAX_PARTICLES 512
static VfxParticle vfx_particles[VFX_MAX_PARTICLES];
static int vfx_particle_count = 0;

bool vfx_enabled(void)
{
	return presentation == PRESENTATION_MODERN && vfx_level != VFX_OFF;
}

static void vfx_enqueue(VfxEvent *e)
{
	if (!vfx_enabled())
		return;
	if (vfx_event_count >= VFX_EVENT_QUEUE)
		return;

	vfx_events[vfx_event_count++] = *e;
}

// Fills an event's common fields from the game coordinate pair.
static void vfx_make_event(VfxEvent *e, int type, int x, int y)
{
	memset(e, 0, sizeof *e);
	e->type = (Uint8)type;
	e->x = (Sint16)(x - VFX_PLAYFIELD_X);
	e->y = (Sint16)y;
}

void vfx_event_explosion(int x, int y, int ttl, int type)
{
	if (!vfx_enabled())
		return;

	VfxEvent e;
	vfx_make_event(&e, VFX_EV_EXPLOSION, x, y);
	e.ttl = (Uint16)(ttl < 0 ? 0 : ttl);
	e.size = (Uint8)(type < 0 ? 0 : (type > 255 ? 255 : type));
	vfx_enqueue(&e);
}

void vfx_event_explosion_large(int x, int y, bool ground, int power)
{
	if (!vfx_enabled())
		return;

	VfxEvent e;
	vfx_make_event(&e, VFX_EV_EXPLOSION_LARGE, x, y);
	e.flags = (Uint8)((ground ? 1 : 0) | (power >= 2 ? 2 : 0));
	vfx_enqueue(&e);
}

void vfx_event_superpixels(int x, int y, int num, int width, int colour)
{
	if (!vfx_enabled())
		return;

	VfxEvent e;
	vfx_make_event(&e, VFX_EV_SUPERPIXELS, x, y);
	e.ttl = (Uint16)(num < 0 ? 0 : num);
	e.size = (Uint8)(width < 0 ? 0 : (width > 255 ? 255 : width));
	e.hue = (Uint8)((colour >> 4) & 15);
	vfx_enqueue(&e);
}

void vfx_event_shot(int x, int y, int dirx, int diry)
{
	if (!vfx_enabled())
		return;

	VfxEvent e;
	vfx_make_event(&e, VFX_EV_SHOT, x, y);
	e.vx = (Sint16)dirx;
	e.vy = (Sint16)diry;
	vfx_enqueue(&e);
}

void vfx_event_enemy_shot(int x, int y, int dirx, int diry)
{
	if (!vfx_enabled())
		return;

	VfxEvent e;
	vfx_make_event(&e, VFX_EV_ENEMY_SHOT, x, y);
	e.vx = (Sint16)dirx;
	e.vy = (Sint16)diry;
	vfx_enqueue(&e);
}

void vfx_event_enemy_death(int x, int y, bool big, bool ground, int linknum)
{
	if (!vfx_enabled())
		return;

	bool boss = false;
	if (linknum != 0)
	{
		for (unsigned int i = 0; i < COUNTOF(boss_bar); ++i)
			if (boss_bar[i].link_num != 0 && boss_bar[i].link_num == (Uint8)linknum)
				boss = true;
	}

	VfxEvent e;
	vfx_make_event(&e, VFX_EV_ENEMY_DEATH, x, y);
	e.flags = (Uint8)((ground ? 1 : 0) | (boss ? 2 : 0));
	e.size = (Uint8)(big || boss ? 1 : 0);
	vfx_enqueue(&e);
}

void vfx_event_player_hit(int x, int y, int damage)
{
	if (!vfx_enabled())
		return;

	VfxEvent e;
	vfx_make_event(&e, VFX_EV_PLAYER_HIT, x, y);
	e.ttl = (Uint16)(damage < 0 ? 0 : (damage > 255 ? 255 : damage));
	vfx_enqueue(&e);
}

void vfx_event_impact(int x, int y, int hue)
{
	if (!vfx_enabled())
		return;

	VfxEvent e;
	vfx_make_event(&e, VFX_EV_IMPACT, x, y);
	e.hue = (Uint8)(hue & 15);
	vfx_enqueue(&e);
}

void vfx_event_player_death(int x, int y)
{
	if (!vfx_enabled())
		return;

	VfxEvent e;
	vfx_make_event(&e, VFX_EV_PLAYER_DEATH, x, y);
	vfx_enqueue(&e);
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------

void vfx_reset(void)
{
	vfx_particle_count = 0;
	vfx_event_count = 0;
	vfx_rng = 0x9E3779B9u;
}

// Number of particles a base count maps to at the current level/intensity.
// High is roughly the old "low"; Low is about half of that.
static int vfx_count(int base)
{
	if (base <= 0)
		return 0;

	int n = (vfx_level == VFX_HIGH) ? base / 2 : base / 4;
	if (n < 1)
		n = 1;

	n = (n * vfx_intensity) / 256;
	return n < 0 ? 0 : n;
}

static int vfx_max_particles(void)
{
	switch (vfx_level)
	{
	case VFX_HIGH: return 320;
	default:       return 160;
	}
}

// Scaled lifetime.
static int vfx_life(int base)
{
	switch (vfx_level)
	{
	case VFX_HIGH: return (base * 2) / 3 + 1;
	default:       return base / 2 + 1;
	}
}

static VfxParticle *vfx_alloc(void)
{
	if (vfx_particle_count >= vfx_max_particles())
		return NULL;

	VfxParticle *p = &vfx_particles[vfx_particle_count++];
	memset(p, 0, sizeof *p);
	return p;
}

// Spawns one particle at playfield-local (x, y) in 16.16 with velocity (vx, vy).
static void vfx_spawn(int kind, Sint32 x, Sint32 y, Sint32 vx, Sint32 vy, int life, int hue, int value, Sint32 rad, Sint32 grow)
{
	VfxParticle *p = vfx_alloc();
	if (p == NULL)
		return;

	if (life < 1) life = 1;
	if (value < 0) value = 0;
	if (value > 15) value = 15;

	p->x = p->px = x;
	p->y = p->py = y;
	p->vx = vx;
	p->vy = vy;
	p->rad = p->prad = rad;
	p->grow = grow;
	p->life = p->max_life = (Uint8)life;
	p->kind = (Uint8)kind;
	p->hue = (Uint8)hue;
	p->value = (Uint8)value;
}

// One radial burst of sparks/debris/smoke/flash around a point.
static void vfx_burst(Sint32 cx, Sint32 cy, int sparks, int debris, int smoke, int flash, int scroll, bool bright)
{
	for (int i = 0; i < sparks; ++i)
	{
		const int d = vfx_rand_range(16);
		const int speed = 2 + vfx_rand_range(4);
		const int life = vfx_life(4 + vfx_rand_range(5));
		const int value = bright ? 15 : 12 + vfx_rand_range(4);
		const Sint32 vx = VFX_FP((vfx_dir_x[d] * speed) >> 7);
		const Sint32 vy = VFX_FP((vfx_dir_y[d] * speed) >> 7);
		vfx_spawn(VFX_KIND_SPARK, cx, cy, vx, vy + VFX_FP(scroll),
		          life, VFX_HUE_FIRE, value, 0, 0);
	}

	for (int i = 0; i < debris; ++i)
	{
		const int d = vfx_rand_range(16);
		const int speed = vfx_rand_range(3);
		const int life = vfx_life(12 + vfx_rand_range(14));
		const int value = 9 + vfx_rand_range(5);
		const Sint32 vx = VFX_FP((vfx_dir_x[d] * speed) >> 7);
		const Sint32 vy = VFX_FP((vfx_dir_y[d] * speed) >> 7);
		vfx_spawn(VFX_KIND_DEBRIS, cx, cy, vx, vy + VFX_FP(scroll),
		          life, VFX_HUE_DEBRIS, value, 0, 0);
	}

	for (int i = 0; i < smoke; ++i)
	{
		const int off = vfx_rand_bipolar(5);
		const int life = vfx_life(18 + vfx_rand_range(16));
		const int value = 3 + vfx_rand_range(4);
		const int rad = 2 + vfx_rand_range(3);
		vfx_spawn(VFX_KIND_SMOKE, cx + VFX_FP(off), cy + VFX_FP(off), 0, VFX_FP(scroll) + VFX_FPHALF,
		          life, VFX_HUE_SMOKE, value, VFX_FP(rad), VFX_FP(1) / 6);
	}

	for (int i = 0; i < flash; ++i)
	{
		const int rad = (vfx_reduce_flashes || vfx_level == VFX_LOW) ? 1 : 2;
		vfx_spawn(VFX_KIND_FLASH, cx, cy, 0, 0, 3, VFX_HUE_FIRE, 15, VFX_FP(rad), 0);
	}
}

static void vfx_ev_explosion(const VfxEvent *e)
{
	const Sint32 x = VFX_FP(e->x);
	const Sint32 y = VFX_FP(e->y);

	// Small, self-drawn-by-the-game explosion: a short spark puff and a little
	// debris and smoke.
	vfx_burst(x, y, vfx_count(3), vfx_count(2), vfx_count(1), 1, 0, false);
}

static void vfx_ev_explosion_large(const VfxEvent *e)
{
	const Sint32 x = VFX_FP(e->x);
	const Sint32 y = VFX_FP(e->y);
	const bool ground = (e->flags & 1) != 0;
	const bool boss = (e->flags & 2) != 0;

	// Ground explosions follow the ground scroll, exactly like the game's own
	// explosion sprites (explodeMove is the per-tick ground displacement).
	const int scroll = ground ? (int)explodeMove : 0;

	// Shockwave ring: a thin blended ring, smaller and dimmer at Low.
	const int radius = (vfx_level == VFX_HIGH) ? 24 : 14;
	const int value = boss ? 15 : ((vfx_level == VFX_HIGH) ? 13 : 11);
	const int rings = boss ? 2 : 1;
	for (int i = 0; i < rings; ++i)
	{
		vfx_spawn(VFX_KIND_RING, x, y, 0, 0,
		          vfx_life(8 + vfx_rand_range(5)), VFX_HUE_FIRE, value,
		          VFX_FP(2), VFX_FP(radius) / 12);
	}

	vfx_burst(x, y, vfx_count(boss ? 20 : 10), vfx_count(6), vfx_count(boss ? 6 : 3),
	          boss ? 2 : 1, scroll, true);
}

static void vfx_ev_superpixels(const VfxEvent *e)
{
	const Sint32 x = VFX_FP(e->x);
	const Sint32 y = VFX_FP(e->y);
	const int n = e->ttl < 8 ? (int)e->ttl : 8;

	for (int i = 0; i < vfx_count(n); ++i)
	{
		const int d = vfx_rand_range(16);
		const int speed = 1 + vfx_rand_range(3);
		const int life = vfx_life(4 + vfx_rand_range(4));
		const int value = 11 + vfx_rand_range(5);
		const Sint32 vx = VFX_FP((vfx_dir_x[d] * speed) >> 7);
		const Sint32 vy = VFX_FP((vfx_dir_y[d] * speed) >> 7);
		vfx_spawn(VFX_KIND_SPARK, x, y, vx, vy,
		          life, e->hue, value, 0, 0);
	}
}

static void vfx_ev_shot(const VfxEvent *e)
{
	const Sint32 x = VFX_FP(e->x);
	const Sint32 y = VFX_FP(e->y);


	// Muzzle flash at the shot spawn point plus a couple of sparks along the
	// shot.  The flash carries the shot's per-tick velocity so it stays on the
	// bullet: the player shot sprite is only drawn from the next tick, by which
	// point the bullet has already moved one step.
	vfx_spawn(VFX_KIND_FLASH, x, y, VFX_FP(e->vx), VFX_FP(e->vy), 2, VFX_HUE_FIRE, 15,
	          VFX_FP((vfx_reduce_flashes || vfx_level == VFX_LOW) ? 0 : 1), 0);

	int dirx = e->vx, diry = e->vy;
	const int mag = (dirx < 0 ? -dirx : dirx) + (diry < 0 ? -diry : diry);
	const int n = vfx_count(2);
	for (int i = 0; i < n; ++i)
	{
		Sint32 vx, vy;
		if (mag > 0)
		{
			vx = (Sint32)(dirx * 65536) / (mag > 4 ? 4 : mag);
			vy = (Sint32)(diry * 65536) / (mag > 4 ? 4 : mag);
		}
		else
		{
			vx = VFX_FP(vfx_rand_bipolar(1));
			vy = -VFX_FP(2);
		}
		vx += VFX_FP(vfx_rand_bipolar(1));
		vy += VFX_FP(vfx_rand_bipolar(1));
		const int life = vfx_life(3 + vfx_rand_range(3));
		const int value = 12 + vfx_rand_range(4);
		vfx_spawn(VFX_KIND_SPARK, x, y, vx, vy,
		          life, VFX_HUE_FIRE, value, 0, 0);
	}
}

static void vfx_ev_enemy_shot(const VfxEvent *e)
{
	const Sint32 x = VFX_FP(e->x);
	const Sint32 y = VFX_FP(e->y);

	vfx_spawn(VFX_KIND_FLASH, x, y, 0, 0, 2, VFX_HUE_HIT,
	          vfx_reduce_flashes ? 10 : 13, VFX_FP((vfx_reduce_flashes || vfx_level == VFX_LOW) ? 0 : 1), 0);

	if (vfx_count(1) > 0)
		vfx_spawn(VFX_KIND_SPARK, x, y, (Sint32)e->vx * 4096, (Sint32)e->vy * 4096,
		          vfx_life(3), VFX_HUE_HIT, 11, 0, 0);
}

static void vfx_ev_enemy_death(const VfxEvent *e)
{
	const bool boss = (e->flags & 2) != 0;
	const bool big = e->size != 0 || boss;

	if (big)
	{
		// e->flags already carries ground (1) and boss (2), which is exactly
		// what vfx_ev_explosion_large() reads.
		vfx_ev_explosion_large(e);
	}
	else
	{
		vfx_ev_explosion(e);
	}
}

static void vfx_ev_player_hit(const VfxEvent *e)
{
	const Sint32 x = VFX_FP(e->x);
	const Sint32 y = VFX_FP(e->y);

	vfx_spawn(VFX_KIND_FLASH, x, y, 0, 0, 3, VFX_HUE_FIRE,
	          vfx_reduce_flashes ? 11 : 14, VFX_FP((vfx_reduce_flashes || vfx_level == VFX_LOW) ? 1 : 2), 0);
	vfx_burst(x, y, vfx_count(3), vfx_count(2), 0, 0, 0, true);
}

static void vfx_ev_impact(const VfxEvent *e)
{
	const Sint32 x = VFX_FP(e->x);
	const Sint32 y = VFX_FP(e->y);

	vfx_spawn(VFX_KIND_FLASH, x, y, 0, 0, 3, e->hue,
	          vfx_reduce_flashes ? 11 : 15, VFX_FP((vfx_reduce_flashes || vfx_level == VFX_LOW) ? 1 : 2), 0);

	const int n = vfx_count(2);
	for (int i = 0; i < n; ++i)
	{
		const int d = vfx_rand_range(16);
		const int speed = 1 + vfx_rand_range(3);
		const int life = vfx_life(3 + vfx_rand_range(3));
		const int value = 12 + vfx_rand_range(4);
		vfx_spawn(VFX_KIND_SPARK, x, y,
		          VFX_FP((vfx_dir_x[d] * speed) >> 7),
		          VFX_FP((vfx_dir_y[d] * speed) >> 7),
		          life, e->hue, value, 0, 0);
	}
}

static void vfx_ev_player_death(const VfxEvent *e)
{
	VfxEvent large = *e;
	large.type = VFX_EV_EXPLOSION_LARGE;
	large.flags = 2;  // boss-scale burst
	vfx_ev_explosion_large(&large);
}

void vfx_tick_end(void)
{
	if (!vfx_enabled())
	{
		if (vfx_particle_count != 0 || vfx_event_count != 0)
			vfx_reset();
		return;
	}

	// Roll the previous position to this tick and integrate.
	int w = 0;
	for (int i = 0; i < vfx_particle_count; ++i)
	{
		VfxParticle *p = &vfx_particles[i];

		p->px = p->x;
		p->py = p->y;
		p->x += p->vx;
		p->y += p->vy;
		p->prad = p->rad;
		p->rad += p->grow;

		if (--p->life == 0)
			continue;

		vfx_particles[w++] = *p;
	}
	vfx_particle_count = w;

	// Spawn the effects for the events collected during this tick.
	for (int i = 0; i < vfx_event_count; ++i)
	{
		const VfxEvent *e = &vfx_events[i];
		switch (e->type)
		{
		case VFX_EV_EXPLOSION:       vfx_ev_explosion(e); break;
		case VFX_EV_EXPLOSION_LARGE: vfx_ev_explosion_large(e); break;
		case VFX_EV_SUPERPIXELS:     vfx_ev_superpixels(e); break;
		case VFX_EV_SHOT:            vfx_ev_shot(e); break;
		case VFX_EV_ENEMY_SHOT:      vfx_ev_enemy_shot(e); break;
		case VFX_EV_ENEMY_DEATH:     vfx_ev_enemy_death(e); break;
		case VFX_EV_PLAYER_HIT:      vfx_ev_player_hit(e); break;
		case VFX_EV_PLAYER_DEATH:    vfx_ev_player_death(e); break;
		case VFX_EV_IMPACT:          vfx_ev_impact(e); break;
		default: break;
		}
	}
	vfx_event_count = 0;
}

// ---------------------------------------------------------------------------
// Rendering (Modern presentation only, into the 8-bit playfield)
// ---------------------------------------------------------------------------

static Uint8 vfx_index(int hue, int value)
{
	if (value < 0) value = 0;
	if (value > 15) value = 15;
	return (Uint8)(((hue & 15) << 4) | value);
}

// The engine's own translucency, copied from blit_sprite2_blend: the source's
// high nibble is the hue block and the low nibbles are averaged, so the pixel
// reads as a tinted translucent overlay.  No ordered dither: softness comes
// from applying the blend more than once towards the centre.
static void vfx_blend(Uint8 *s, int hue, int value)
{
	*s = (Uint8)((((*s & 0x0f) + (value & 0x0f)) / 2) | ((hue & 15) << 4));
}

// The darken variant (blit_sprite2_darken): keeps the destination hue and
// halves its brightness.  Used for the faint rim of a smoke puff.
static void vfx_darken(Uint8 *s)
{
	*s = (Uint8)(((*s & 0x0f) / 2) + (*s & 0xf0));
}

static void vfx_put(Uint8 *base, int pitch, int x, int y, Uint8 index)
{
	if ((unsigned)x >= (unsigned)VFX_PLAYFIELD_W || (unsigned)y >= (unsigned)VFX_PLAYFIELD_H)
		return;

	base[(size_t)y * (size_t)pitch + (size_t)x] = index;
	modern_bloom_tag_pixel(x, y);  // VFX always emit; see modern_bloom.h
}

// Translucent blend at a playfield coordinate (bounds-checked).
static void vfx_blend_at(Uint8 *base, int pitch, int x, int y, int hue, int value)
{
	if ((unsigned)x >= (unsigned)VFX_PLAYFIELD_W || (unsigned)y >= (unsigned)VFX_PLAYFIELD_H)
		return;

	vfx_blend(base + (size_t)y * (size_t)pitch + (size_t)x, hue, value);
	modern_bloom_tag_pixel(x, y);  // VFX always emit; see modern_bloom.h
}

// Interpolated integer position of a particle at the given 16.16 alpha.
static void vfx_interp(const VfxParticle *p, Uint32 alpha, int *ox, int *oy)
{
	const Sint32 dx = p->x - p->px;
	const Sint32 dy = p->y - p->py;
	*ox = (int)((p->px + (Sint32)(((Sint64)dx * (Sint32)alpha) >> 16)) >> 16);
	*oy = (int)((p->py + (Sint32)(((Sint64)dy * (Sint32)alpha) >> 16)) >> 16);
}

// Interpolated radius (16.16) at the given 16.16 alpha.
static int vfx_interp_radius(const VfxParticle *p, Uint32 alpha)
{
	return (int)((p->prad + (Sint32)(((Sint64)(p->rad - p->prad) * (Sint32)alpha) >> 16)) >> 16);
}

static void vfx_draw_spark(const VfxParticle *p, Uint8 *base, int pitch, Uint32 alpha)
{
	int x0, y0, x1, y1;
	vfx_interp(p, alpha, &x1, &y1);
	x0 = (int)(p->px >> 16);
	y0 = (int)(p->py >> 16);

	const int dx = x1 - x0, dy = y1 - y0;
	int steps = (dx < 0 ? -dx : dx);
	if ((dy < 0 ? -dy : dy) > steps)
		steps = (dy < 0 ? -dy : dy);
	if (steps > 4)
		steps = 4;

	const int value = p->value * (int)p->life / (int)p->max_life;

	if (steps <= 0)
	{
		vfx_put(base, pitch, x1, y1, vfx_index(p->hue, value));
		return;
	}

	for (int t = 0; t <= steps; ++t)
	{
		const int x = x0 + dx * t / steps;
		const int y = y0 + dy * t / steps;
		vfx_put(base, pitch, x, y, vfx_index(p->hue, value));
	}
}

static void vfx_draw_debris(const VfxParticle *p, Uint8 *base, int pitch, Uint32 alpha)
{
	int x, y;
	vfx_interp(p, alpha, &x, &y);

	const int value = p->value * (int)p->life / (int)p->max_life;
	vfx_put(base, pitch, x, y, vfx_index(p->hue, value));
}

static void vfx_draw_smoke(const VfxParticle *p, Uint8 *base, int pitch, Uint32 alpha)
{
	int cx, cy;
	vfx_interp(p, alpha, &cx, &cy);

	int r = vfx_interp_radius(p, alpha);
	if (r < 1) r = 1;
	const int life = (int)p->life, max_life = (int)p->max_life;

	for (int dy = -r; dy <= r; ++dy)
	{
		for (int dx = -r; dx <= r; ++dx)
		{
			const int d2 = dx * dx + dy * dy;
			if (d2 > r * r)
				continue;

			const int x = cx + dx, y = cy + dy;
			if ((unsigned)x >= (unsigned)VFX_PLAYFIELD_W || (unsigned)y >= (unsigned)VFX_PLAYFIELD_H)
				continue;

			// Softness without dithering: the centre is blended twice, the mid
			// radius once, and the rim is only darkened (a smoke shadow).
			int cov = 255 - (d2 * 255) / (r * r + 1);
			cov = cov * life / max_life;

			Uint8 *s = base + (size_t)y * (size_t)pitch + (size_t)x;
			if (cov >= 150)
			{
				vfx_blend(s, p->hue, p->value);
				vfx_blend(s, p->hue, p->value);
			}
			else if (cov >= 70)
			{
				vfx_blend(s, p->hue, p->value);
			}
			else
			{
				vfx_darken(s);
			}
			modern_bloom_tag_pixel(x, y);
		}
	}
}

static void vfx_draw_ring(const VfxParticle *p, Uint8 *base, int pitch, Uint32 alpha)
{
	int cx, cy;
	vfx_interp(p, alpha, &cx, &cy);

	int r = vfx_interp_radius(p, alpha);
	if (r < 1) return;
	if (r > 80) r = 80;

	const int value = p->value * (int)p->life / (int)p->max_life;

	for (int dy = -r; dy <= r; ++dy)
	{
		for (int dx = -r; dx <= r; ++dx)
		{
			const int d2 = dx * dx + dy * dy;
			const int lo = (r - 1) * (r - 1), hi = (r + 1) * (r + 1);
			if (d2 < lo || d2 > hi)
				continue;

			const int x = cx + dx, y = cy + dy;
			if ((unsigned)x >= (unsigned)VFX_PLAYFIELD_W || (unsigned)y >= (unsigned)VFX_PLAYFIELD_H)
				continue;

			Uint8 *s = base + (size_t)y * (size_t)pitch + (size_t)x;
			vfx_blend(s, p->hue, value);
			modern_bloom_tag_pixel(x, y);
		}
	}
}

static void vfx_draw_flash(const VfxParticle *p, Uint8 *base, int pitch, Uint32 alpha)
{
	int cx, cy;
	vfx_interp(p, alpha, &cx, &cy);

	int r = (int)(p->rad >> 16);
	if (r < 0) r = 0;
	const int value = p->value * (int)p->life / (int)p->max_life;

	// Bright core so the flash still reads as a hot point...
	vfx_put(base, pitch, cx, cy, vfx_index(p->hue, value));

	// ...with four translucent arms that fade towards the tip.  Using the
	// engine's nibble blend keeps them soft; the old plus/cross wrote opaque
	// pixels and its 8-point case was a solid 3x3 white square.
	for (int i = 1; i <= r + 1; ++i)
	{
		const int arm = value * (r + 2 - i) / (r + 2);
		if (arm <= 0)
			continue;

		vfx_blend_at(base, pitch, cx - i, cy, p->hue, arm);
		vfx_blend_at(base, pitch, cx + i, cy, p->hue, arm);
		vfx_blend_at(base, pitch, cx, cy - i, p->hue, arm);
		vfx_blend_at(base, pitch, cx, cy + i, p->hue, arm);
	}

	// High adds short, dim diagonal tips so the burst becomes a star rather
	// than a square; Low stays a compact cross.
	if (r >= 2)
	{
		const int tip = value / 2;
		if (tip > 0)
		{
			vfx_blend_at(base, pitch, cx - 1, cy - 1, p->hue, tip);
			vfx_blend_at(base, pitch, cx + 1, cy - 1, p->hue, tip);
			vfx_blend_at(base, pitch, cx - 1, cy + 1, p->hue, tip);
			vfx_blend_at(base, pitch, cx + 1, cy + 1, p->hue, tip);
		}
	}
}

void vfx_render_playfield(SDL_Surface *surface, Uint32 alpha_fx16)
{
	if (!vfx_enabled() || surface == NULL || surface->pixels == NULL)
		return;

	Uint8 *base = surface->pixels;
	const int pitch = surface->pitch;

	for (int i = 0; i < vfx_particle_count; ++i)
	{
		const VfxParticle *p = &vfx_particles[i];
		switch (p->kind)
		{
		case VFX_KIND_SPARK:  vfx_draw_spark(p, base, pitch, alpha_fx16); break;
		case VFX_KIND_DEBRIS: vfx_draw_debris(p, base, pitch, alpha_fx16); break;
		case VFX_KIND_SMOKE:  vfx_draw_smoke(p, base, pitch, alpha_fx16); break;
		case VFX_KIND_RING:   vfx_draw_ring(p, base, pitch, alpha_fx16); break;
		case VFX_KIND_FLASH:  vfx_draw_flash(p, base, pitch, alpha_fx16); break;
		default: break;
		}
	}
}

bool set_vfx_by_name(const char *name)
{
	for (int i = 0; i < VFX_LEVEL_MAX; ++i)
	{
		if (SDL_strcasecmp(name, vfx_level_names[i]) == 0)
		{
			vfx_level = (VfxLevel)i;
			return true;
		}
	}

	// The old levels had a "medium"; map it to high so existing cfg files and
	// command lines keep working.
	if (SDL_strcasecmp(name, "medium") == 0)
	{
		vfx_level = VFX_HIGH;
		return true;
	}

	return false;
}
