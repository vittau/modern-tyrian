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
#include "modern_depth.h"

#include "config.h"
#include "logging.h"
#include "modern.h"
#include "modern_bloom.h"
#include "modern_held.h"
#include "opentyr.h"

#include <string.h>

// Depth setting (Setup -> Graphics -> Depth:).  Modern only; Off keeps the layer
// buffer from being stamped at all (modern_depth_layers_wanted()).
ModernQuality modern_depth_quality = MODERN_QUALITY_LOW;

// Presented layer buffer (playfield window) and rank table.  Fixed-capacity
// statics: no per-frame allocation.
static Uint8 md_layer[MODERN_PLAYFIELD_W * MODERN_PLAYFIELD_H];
static Uint8 md_rank[DL_LAYER_COUNT];
static bool md_valid = false;
static bool md_fresh = false;      // a frame was copied since the pass last ran
static bool md_requested = false;  // regress / debug request, independent of the setting

// Snapshot of the last really presented frame's buffers, reused by held in-level
// screens (modern_held.c).
static Uint8 md_snap_layer[MODERN_PLAYFIELD_W * MODERN_PLAYFIELD_H];
static Uint8 md_snap_rank[DL_LAYER_COUNT];
static bool md_snap_valid = false;

static unsigned long md_frames = 0;
static unsigned long md_frames_interpolated = 0;
static unsigned long md_frames_flipped = 0;

bool modern_depth_layers_wanted(void)
{
	return presentation == PRESENTATION_MODERN &&
	       (md_requested || modern_depth_quality != MODERN_QUALITY_OFF);
}

void modern_depth_set_requested(bool requested)
{
	md_requested = requested;
}

void modern_depth_begin(void)
{
	md_fresh = false;
	if (!modern_depth_layers_wanted())
	{
		md_valid = false;
		return;
	}

	memset(md_layer, DL_LAYER_NONE, sizeof md_layer);
	memset(md_rank, 0, sizeof md_rank);
	md_valid = true;
	md_fresh = true;
}

void modern_depth_from_game(SDL_Surface *game, bool flip)
{
	if (!md_valid)
		return;

	int pitch = 0;
	const Uint8 *layer = drawlist_layer_for_surface(game, &pitch, NULL, NULL);
	const Uint8 *rank = drawlist_layer_ranks(game);
	if (layer == NULL)
		return;

	// The playfield copy always starts at game x = 24 (both the normal and the
	// spotlight special code read the same columns); the vertical-flip special
	// code reverses the rows.  Same mapping as modern_bloom_tag_from_game().
	for (int y = 0; y < MODERN_PLAYFIELD_H; ++y)
	{
		const int sy = flip ? (MODERN_PLAYFIELD_H - 1 - y) : y;
		memcpy(md_layer + (size_t)y * MODERN_PLAYFIELD_W,
		       layer + (size_t)sy * (size_t)pitch + 24, MODERN_PLAYFIELD_W);
	}

	if (rank != NULL)
		memcpy(md_rank, rank, sizeof md_rank);

	md_frames++;
	if (game == drawlist_interpolated_game())
		md_frames_interpolated++;
	if (flip)
		md_frames_flipped++;
}

bool modern_depth_held_save(void)
{
	md_snap_valid = md_valid;
	if (!md_valid)
		return false;

	memcpy(md_snap_layer, md_layer, sizeof md_layer);
	memcpy(md_snap_rank, md_rank, sizeof md_rank);
	return true;
}

void modern_depth_held_restore(const Uint8 *overlay)
{
	md_fresh = false;
	if (!md_snap_valid || !modern_depth_layers_wanted())
		return;

	memcpy(md_layer, md_snap_layer, sizeof md_layer);
	memcpy(md_rank, md_snap_rank, sizeof md_rank);
	for (size_t i = 0; i < sizeof md_layer; ++i)
		if (overlay[i])
			md_layer[i] = DL_LAYER_OTHER;

	md_valid = true;
	md_fresh = true;
}

void modern_depth_held_forget(void)
{
	md_snap_valid = false;
}

void modern_depth_mark_vfx(int x, int y)
{
	if (!md_valid)
		return;
	if ((unsigned)x >= MODERN_PLAYFIELD_W || (unsigned)y >= MODERN_PLAYFIELD_H)
		return;

	// A flag, not a layer: the pixel keeps the layer it was painted over, so it
	// still receives shadows as that layer but never casts one.
	md_layer[(size_t)y * MODERN_PLAYFIELD_W + (size_t)x] |= DL_LAYER_VFX_FLAG;
}

const Uint8 *modern_depth_layer_buffer(void)
{
	return md_valid ? md_layer : NULL;
}

const Uint8 *modern_depth_rank_table(void)
{
	return md_valid ? md_rank : NULL;
}

// --- soft cast shadows (stage 2) ---------------------------------------------------
//
// Every layer casts a soft shadow onto the layers BELOW it, as if lit by one fixed
// light from the upper left: the shadow of a pixel lands down-right of it, farther
// the higher its layer.  Heights are in quarter units (Q2) so the table can hold
// the half step between the sky enemies and the ship.
//
// A shadow pixel is darkened only when the receiver pixel's layer is BELOW the
// caster's in both senses: painted earlier this tick (the rank table, which
// follows background2over / skyEnemyOverAll / topEnemyOver) and lower in the
// height table.  So a layer never shadows itself, bg2 and the ground enemies
// (equal height) do not shadow each other, and a level that paints bg3 under the
// ground enemies simply gets no bg3 shadow on them.

typedef struct
{
	Sint8 dx, dy;    // shadow offset, screen space (down-right)
	Uint8 height;    // Q2 height; also the receiver height of the same layer
	Uint8 weight;    // Q8 coverage the layer casts at; 0 = does not cast
	bool receives;
} MdLayerRule;

#define MD_BLEND_WEIGHT 80  // a translucent bg2 pixel casts weakly (Q8)

static const MdLayerRule md_rules[DL_LAYER_COUNT] =
{
	[DL_LAYER_NONE]         = { 0, 0,  0,   0, false },
	[DL_LAYER_BG1]          = { 0, 0,  0,   0, true  },
	[DL_LAYER_STARFIELD]    = { 0, 0,  0,   0, false },
	[DL_LAYER_BG2]          = { 2, 3,  4, 255, true  },
	[DL_LAYER_GROUND_ENEMY] = { 2, 2,  4, 255, true  },
	[DL_LAYER_SKY_ENEMY]    = { 6, 9, 12, 255, true  },
	[DL_LAYER_BG3]          = { 8, 12, 16, 255, true },
	[DL_LAYER_TOP_ENEMY]    = { 9, 13, 20, 255, true },
	[DL_LAYER_PLAYER]       = { 7, 10, 14, 255, true },
	[DL_LAYER_SIDEKICK]     = { 7, 10, 14, 255, true },
	[DL_LAYER_PLAYER_SHOT]  = { 0, 0,  0,   0, false },
	[DL_LAYER_ENEMY_SHOT]   = { 0, 0,  0,   0, false },
	[DL_LAYER_EXPLOSION]    = { 0, 0,  0,   0, false },
	[DL_LAYER_SUPERPIXEL]   = { 0, 0,  0,   0, false },
	[DL_LAYER_HUD]          = { 0, 0,  0,   0, false },
	[DL_LAYER_OTHER]        = { 0, 0,  0,   0, false },
};

typedef struct
{
	int strength;  // Q8 darkening at full coverage
	int radius;    // box-blur radius
	int passes;    // box-blur passes per axis
} MdShadowParams;

static const MdShadowParams md_params[MODERN_QUALITY_MAX] =
{
	[MODERN_QUALITY_OFF]  = { 0,   0, 0 },
	[MODERN_QUALITY_LOW]  = { 72,  1, 2 },
	[MODERN_QUALITY_HIGH] = { 108, 2, 2 },
};

#define MD_W MODERN_PLAYFIELD_W
#define MD_H MODERN_PLAYFIELD_H

// Coverage (Q8) and coverage*height planes, ping-ponged through the blur.  The
// blur is UNNORMALISED (plain window sums, edge-replicated), so both planes stay
// exact integers: coverage = cov / norm and the mean caster height cov-weighted
// = hgt / cov, with no rounding to make a pixel at exactly the caster's own
// height look slightly lower.  Every plane is all-zero between calls; a call
// clears the rows it touched before it returns.
static Uint32 md_cov[2][MD_W * MD_H];
static Uint32 md_hgt[2][MD_W * MD_H];

static unsigned long md_stat_frames = 0;
static unsigned long md_stat_shadowed = 0;
static unsigned long md_stat_space = 0;
static unsigned long md_stat_casters[DL_LAYER_COUNT];
static unsigned long md_stat_blend_casters = 0;
static Uint64 md_stat_ticks = 0;          // SDL performance-counter ticks spent in the shadow core
static unsigned long md_stat_timed = 0;   // frames that ran the core

static void md_blur_h(const Uint32 *src, Uint32 *dst, int y0, int y1, int radius)
{
	for (int y = y0; y <= y1; ++y)
	{
		const Uint32 *in = src + (size_t)y * MD_W;
		Uint32 *out = dst + (size_t)y * MD_W;

		// Edge-replicating window [x - radius, x + radius].
		Uint32 sum = 0;
		for (int i = -radius; i <= radius; ++i)
			sum += in[i < 0 ? 0 : (i >= MD_W ? MD_W - 1 : i)];

		for (int x = 0; x < MD_W; ++x)
		{
			out[x] = sum;
			const int add = x + radius + 1, sub = x - radius;
			sum += in[add >= MD_W ? MD_W - 1 : add];
			sum -= in[sub < 0 ? 0 : sub];
		}
	}
}

static void md_blur_v(const Uint32 *src, Uint32 *dst, int y0, int y1, int radius)
{
	Uint32 col[MD_W];

	for (int x = 0; x < MD_W; ++x)
	{
		Uint32 sum = 0;
		for (int i = -radius; i <= radius; ++i)
		{
			const int yy = y0 + i;
			sum += src[(size_t)(yy < 0 ? 0 : (yy >= MD_H ? MD_H - 1 : yy)) * MD_W + x];
		}
		col[x] = sum;
	}

	for (int y = y0; y <= y1; ++y)
	{
		Uint32 *out = dst + (size_t)y * MD_W;
		const int add = y + radius + 1, sub = y - radius;
		const Uint32 *row_add = src + (size_t)(add >= MD_H ? MD_H - 1 : add) * MD_W;
		const Uint32 *row_sub = src + (size_t)(sub < 0 ? 0 : sub) * MD_W;
		for (int x = 0; x < MD_W; ++x)
		{
			out[x] = col[x];
			col[x] += row_add[x];
			col[x] -= row_sub[x];
		}
	}
}

static void md_clear_rows(int y0, int y1)
{
	const size_t offset = (size_t)y0 * MD_W, bytes = (size_t)(y1 - y0 + 1) * MD_W * sizeof(Uint32);
	memset(md_cov[0] + offset, 0, bytes);
	memset(md_cov[1] + offset, 0, bytes);
	memset(md_hgt[0] + offset, 0, bytes);
	memset(md_hgt[1] + offset, 0, bytes);
}

unsigned long modern_depth_shadow_apply(Uint32 *canvas, int canvas_pitch_px,
                                        const Uint8 *layers, const Uint8 *rank,
                                        ModernQuality quality, ModernDepthShadowStats *stats)
{
	if (quality <= MODERN_QUALITY_OFF || quality >= MODERN_QUALITY_MAX ||
	    canvas == NULL || layers == NULL || rank == NULL)
		return 0;
	const MdShadowParams *params = &md_params[quality];

	// ok[caster][receiver]: the receiver is below the caster in drawing order and
	// in height.  Built once per frame from the rank table.
	bool ok[DL_LAYER_COUNT][DL_LAYER_COUNT];
	for (int c = 0; c < DL_LAYER_COUNT; ++c)
		for (int r = 0; r < DL_LAYER_COUNT; ++r)
			ok[c][r] = md_rules[c].weight != 0 && md_rules[r].receives &&
			           rank[r] != 0 && rank[c] > rank[r] &&
			           md_rules[c].height > md_rules[r].height;

	// Splat: every caster pixel marks the receiver pixel at its shadow offset.
	// The highest caster wins a pixel (a bg3 shadow beats a ground one).
	int ymin = MD_H, ymax = -1;
	unsigned long casters[DL_LAYER_COUNT] = { 0 };
	unsigned long blend_casters = 0;
	Uint32 *const cov0 = md_cov[0], *const hgt0 = md_hgt[0];
	for (int y = 0; y < MD_H; ++y)
	{
		const Uint8 *row = layers + (size_t)y * MD_W;
		for (int x = 0; x < MD_W; ++x)
		{
			const Uint8 v = row[x];
			if (v & DL_LAYER_VFX_FLAG)
				continue;
			const int id = v & DL_LAYER_ID_MASK;
			const MdLayerRule *rule = &md_rules[id];
			if (rule->weight == 0)
				continue;

			const int tx = x + rule->dx, ty = y + rule->dy;
			if (tx >= MD_W || ty >= MD_H)
				continue;
			const size_t t = (size_t)ty * MD_W + (size_t)tx;
			if (!ok[id][layers[t] & DL_LAYER_ID_MASK])
				continue;

			unsigned weight = rule->weight;
			if (v & DL_LAYER_BLEND)
			{
				weight = MD_BLEND_WEIGHT;
				blend_casters++;
			}
			casters[id]++;

			const Uint32 cur_h = cov0[t] != 0 ? hgt0[t] / cov0[t] : 0;
			if (rule->height > cur_h || (rule->height == cur_h && weight > cov0[t]))
			{
				cov0[t] = weight;
				hgt0[t] = weight * rule->height;
				if (ty < ymin) ymin = ty;
				if (ty > ymax) ymax = ty;
			}
		}
	}

	if (stats != NULL)
	{
		for (int i = 0; i < DL_LAYER_COUNT; ++i)
			stats->casters[i] += casters[i];
		stats->blend_casters += blend_casters;
	}
	if (ymax < 0)
		return 0;

	// Soften: a separable box blur repeated `passes` times, restricted to the
	// rows the shadows can reach.
	const int reach = params->radius * params->passes;
	const int y0 = MAX(0, ymin - reach), y1 = MIN(MD_H - 1, ymax + reach);
	int a = 0;
	for (int pass = 0; pass < params->passes; ++pass)
	{
		md_blur_h(md_cov[a], md_cov[a ^ 1], y0, y1, params->radius);
		md_blur_h(md_hgt[a], md_hgt[a ^ 1], y0, y1, params->radius);
		a ^= 1;
		md_blur_v(md_cov[a], md_cov[a ^ 1], y0, y1, params->radius);
		md_blur_v(md_hgt[a], md_hgt[a ^ 1], y0, y1, params->radius);
		a ^= 1;
	}

	// Darken.  A pixel is shadowed only if the mean caster height around it is
	// above its own layer's height, so the soft fringe never dirties a ship, a
	// shot or a structure that stands level with (or above) the caster.
	const Uint32 window = (Uint32)(2 * params->radius + 1);
	Uint32 norm = 1;
	for (int i = 0; i < 2 * params->passes; ++i)
		norm *= window;
	const Uint32 divisor = norm * 256;

	unsigned long shadowed = 0;
	for (int y = y0; y <= y1; ++y)
	{
		const Uint32 *cov = md_cov[a] + (size_t)y * MD_W;
		const Uint32 *hgt = md_hgt[a] + (size_t)y * MD_W;
		const Uint8 *row = layers + (size_t)y * MD_W;
		Uint32 *out = canvas + (size_t)y * (size_t)canvas_pitch_px;
		for (int x = 0; x < MD_W; ++x)
		{
			const Uint32 c = cov[x];
			if (c == 0)
				continue;
			const MdLayerRule *rule = &md_rules[row[x] & DL_LAYER_ID_MASK];
			if (!rule->receives || hgt[x] <= c * rule->height)
				continue;

			const Uint32 dark = c * (Uint32)params->strength / divisor;
			if (dark == 0)
				continue;
			const Uint32 mul = 256 - dark;
			const Uint32 p = out[x];
			out[x] = (((p & 0x00ff00ffu) * mul >> 8) & 0x00ff00ffu) |
			         (((p & 0x0000ff00u) * mul >> 8) & 0x0000ff00u);
			shadowed++;
		}
	}

	md_clear_rows(y0, y1);
	return shadowed;
}

void modern_depth_pass(ModernFrame *frame)
{
	// Consume the frame: a frame that never copied the layer buffer (the level
	// intro hold, a screen composed another way) gets no shadows rather than a
	// stale buffer's.
	const bool fresh = md_fresh && md_valid;
	md_fresh = false;

	if (!frame->gameplay || !fresh || modern_depth_quality == MODERN_QUALITY_OFF)
		return;

	const int playfield_x = frame->content_offset_x;
	if (playfield_x < 0 || playfield_x + MD_W > frame->w || MD_H > frame->h)
		return;

	// A held in-level screen reuses the last real frame's buffers (modern_held.c):
	// its counters stay apart from the live ones.
	const bool held = modern_held_overlay() != NULL;

	if (!held)
		md_stat_frames++;

	// Never cast a shadow onto the void: the starfield levels (the same test
	// vfx_ambient.c uses for its space style).
	if (starActive)
	{
		if (held)
			modern_held_note_shadow(0, true);
		else
			md_stat_space++;
		return;
	}

	ModernDepthShadowStats stats;
	memset(&stats, 0, sizeof stats);
	const Uint64 t0 = SDL_GetPerformanceCounter();
	const unsigned long shadowed = modern_depth_shadow_apply(frame->pixels + playfield_x, frame->w,
	                                                         md_layer, md_rank, modern_depth_quality, &stats);
	if (held)
	{
		modern_held_note_shadow(shadowed, false);
		return;
	}

	md_stat_shadowed += shadowed;
	md_stat_ticks += SDL_GetPerformanceCounter() - t0;
	md_stat_timed++;
	for (int i = 0; i < DL_LAYER_COUNT; ++i)
		md_stat_casters[i] += stats.casters[i];
	md_stat_blend_casters += stats.blend_casters;
}

void modern_depth_log_stats(void)
{
	logInfo("Depth shadows: frames=%lu shadowed_px=%lu casters bg2=%lu ground=%lu sky=%lu player=%lu sidekick=%lu bg3=%lu top=%lu blend=%lu space_frames=%lu interpolated=%lu flipped=%lu",
	        md_stat_frames, md_stat_shadowed, md_stat_casters[DL_LAYER_BG2],
	        md_stat_casters[DL_LAYER_GROUND_ENEMY], md_stat_casters[DL_LAYER_SKY_ENEMY],
	        md_stat_casters[DL_LAYER_PLAYER], md_stat_casters[DL_LAYER_SIDEKICK],
	        md_stat_casters[DL_LAYER_BG3], md_stat_casters[DL_LAYER_TOP_ENEMY],
	        md_stat_blend_casters, md_stat_space,
	        md_frames_interpolated, md_frames_flipped);
	if (md_stat_timed > 0)
		logInfo("Depth cost: %.3f ms per presented frame over %lu frames (shadow core only).",
		        1000.0 * (double)md_stat_ticks / (double)SDL_GetPerformanceFrequency() / (double)md_stat_timed,
		        md_stat_timed);
}

unsigned long modern_depth_presented_frames(void)       { return md_frames; }
unsigned long modern_depth_presented_interpolated(void) { return md_frames_interpolated; }
unsigned long modern_depth_presented_flipped(void)      { return md_frames_flipped; }

bool modern_depth_save_png(const char *path)
{
	if (!md_valid || path == NULL)
		return false;

	// One distinct colour per layer; a pixel of the blended bg2 row is lightened
	// so the flag is visible next to the opaque bg2.
	static const Uint8 colors[DL_LAYER_COUNT][3] =
	{
		{   0,   0,   0 },  // none
		{  40,  60, 170 },  // bg1
		{ 255, 255, 255 },  // starfield
		{  40, 170,  60 },  // bg2
		{ 200, 120,  40 },  // ground enemy
		{ 240, 220,  60 },  // sky enemy
		{ 170,  60, 200 },  // bg3
		{ 230,  60,  60 },  // top enemy
		{   0, 230, 230 },  // player
		{   0, 140, 140 },  // sidekick
		{ 130, 255, 130 },  // player shot
		{ 255, 130, 200 },  // enemy shot
		{ 255, 170,   0 },  // explosion
		{ 255, 255, 150 },  // superpixel
		{  80,  80,  80 },  // hud
		{ 255,   0, 255 },  // other
	};

	SDL_Surface *surface = SDL_CreateSurface(MODERN_PLAYFIELD_W, MODERN_PLAYFIELD_H, SDL_PIXELFORMAT_RGBA32);
	if (surface == NULL)
		return false;

	for (int y = 0; y < MODERN_PLAYFIELD_H; ++y)
	{
		Uint8 *out = (Uint8 *)surface->pixels + (size_t)y * (size_t)surface->pitch;
		for (int x = 0; x < MODERN_PLAYFIELD_W; ++x)
		{
			const Uint8 v = md_layer[(size_t)y * MODERN_PLAYFIELD_W + (size_t)x];
			unsigned id = v & DL_LAYER_ID_MASK;
			if (id >= DL_LAYER_COUNT)
				id = DL_LAYER_OTHER;

			for (int c = 0; c < 3; ++c)
			{
				unsigned value = colors[id][c];
				if (v & DL_LAYER_BLEND)
					value = (value + 255) / 2;
				if (v & DL_LAYER_VFX_FLAG)  // VFX over this layer: greyed
					value = (value + 150) / 2;
				out[x * 4 + c] = (Uint8)value;
			}
			out[x * 4 + 3] = 255;
		}
	}

	const bool saved = SDL_SavePNG(surface, path);
	SDL_DestroySurface(surface);
	return saved;
}
