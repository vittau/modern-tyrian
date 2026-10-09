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
/* Synthetic coverage uses explicit checks in release builds too. */
#include "regress.h"

#include "drawlist.h"
#include "modern_bloom.h"
#include "modern_depth.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The fixture runs the shadow core on hand-built layer buffers: a canvas with a
// margin (so a write outside the playfield is caught), a flat grey colour, and a
// rank table where each layer was first drawn in enum order.

#define DF_W MODERN_PLAYFIELD_W
#define DF_H MODERN_PLAYFIELD_H
#define DF_X0 5                       // playfield origin on the canvas
#define DF_PITCH (DF_X0 + DF_W + 7)   // pixels per canvas row
#define DF_ROWS (DF_H + 4)
#define DF_GREY 0x00c0c0c0u

static Uint8 df_layers[DF_W * DF_H];
static Uint8 df_rank[DL_LAYER_COUNT];
static Uint32 df_canvas[DF_PITCH * DF_ROWS];

static void df_reset(int base_layer)
{
	memset(df_layers, base_layer, sizeof df_layers);
	// Drawing order = enum order, so a later enum value is "above".
	for (int i = 0; i < DL_LAYER_COUNT; ++i)
		df_rank[i] = (Uint8)i;
	for (int i = 0; i < DF_PITCH * DF_ROWS; ++i)
		df_canvas[i] = DF_GREY;
}

static void df_block(int x0, int y0, int x1, int y1, int value)
{
	for (int y = y0; y <= y1; ++y)
		for (int x = x0; x <= x1; ++x)
			df_layers[y * DF_W + x] = (Uint8)value;
}

static Uint32 df_px(int x, int y)
{
	return df_canvas[y * DF_PITCH + DF_X0 + x];
}

static unsigned df_run(ModernQuality quality, ModernDepthShadowStats *stats)
{
	return (unsigned)modern_depth_shadow_apply(df_canvas + DF_X0, DF_PITCH, df_layers, df_rank, quality, stats);
}

// Number of playfield pixels that are not the flat grey.
static int df_changed(void)
{
	int n = 0;
	for (int y = 0; y < DF_H; ++y)
		for (int x = 0; x < DF_W; ++x)
			n += df_px(x, y) != DF_GREY;
	return n;
}

// The margin around the playfield (columns left/right of it and the rows below)
// must be untouched.
static bool df_margin_clean(void)
{
	for (int y = 0; y < DF_ROWS; ++y)
		for (int x = 0; x < DF_PITCH; ++x)
		{
			const bool inside = y < DF_H && x >= DF_X0 && x < DF_X0 + DF_W;
			if (!inside && df_canvas[y * DF_PITCH + x] != DF_GREY)
				return false;
		}
	return true;
}

#define DF_REQUIRE(condition) \
	do \
	{ \
		if (!(condition)) \
		{ \
			fprintf(stderr, "Depth fixture FAIL: line %d: %s\n", __LINE__, #condition); \
			return 1; \
		} \
	} while (0)

int regress_depth_selfcheck(void)
{
	ModernDepthShadowStats stats;

	// Off is the identity.
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG3);
	memset(&stats, 0, sizeof stats);
	DF_REQUIRE(df_run(MODERN_QUALITY_OFF, &stats) == 0 && df_changed() == 0 && df_margin_clean());
	DF_REQUIRE(stats.casters[DL_LAYER_BG3] == 0);
	puts("Depth fixture PASS: Off is the identity");

	// A higher layer above a lower one darkens it: the ground right of / below the
	// block is shadowed, the block itself, and the ground far away, are not.
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG2);
	memset(&stats, 0, sizeof stats);
	DF_REQUIRE(df_run(MODERN_QUALITY_LOW, &stats) > 0);
	DF_REQUIRE(stats.casters[DL_LAYER_BG2] > 0);
	DF_REQUIRE(df_px(122, 70) < DF_GREY);        // right of the block, inside the shadow
	DF_REQUIRE(df_px(110, 83) < DF_GREY);        // below the block, inside the shadow
	DF_REQUIRE(df_px(110, 70) == DF_GREY);       // the caster itself
	DF_REQUIRE(df_px(60, 70) == DF_GREY);        // left of it (the light side)
	DF_REQUIRE(df_px(110, 40) == DF_GREY);       // above it
	DF_REQUIRE(df_px(200, 150) == DF_GREY);      // far away
	DF_REQUIRE(df_margin_clean());
	puts("Depth fixture PASS: above + higher darkens; caster, light side and distance do not");

	// Softness: a row crossing the shadow edge falls off in several distinct steps
	// (no hard pixel edge), and the channels stay in step (a grey stays grey).
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG3);
	DF_REQUIRE(df_run(MODERN_QUALITY_LOW, NULL) > 0);
	{
		int levels = 0;
		Uint32 last = DF_GREY;
		for (int x = 121; x < 160; ++x)
		{
			const Uint32 p = df_px(x, 72);
			DF_REQUIRE(((p >> 16) & 0xff) == ((p >> 8) & 0xff) && ((p >> 8) & 0xff) == (p & 0xff));
			DF_REQUIRE(p >= last || x < 125);  // brightens monotonically past the shadow body
			if (p != last)
				levels++;
			last = p;
		}
		DF_REQUIRE(levels >= 3);
	}
	puts("Depth fixture PASS: shadow edges are soft and neutral");

	// A layer never shadows itself, equal heights do not shadow each other, and
	// a lower layer never shadows a higher one.
	df_reset(DL_LAYER_SKY_ENEMY);
	df_block(100, 60, 120, 80, DL_LAYER_SKY_ENEMY);
	DF_REQUIRE(df_run(MODERN_QUALITY_HIGH, NULL) == 0 && df_changed() == 0);
	df_reset(DL_LAYER_BG2);
	df_block(100, 60, 120, 80, DL_LAYER_GROUND_ENEMY);
	DF_REQUIRE(df_run(MODERN_QUALITY_HIGH, NULL) == 0 && df_changed() == 0);
	// A bg2 block on a sky-enemy background: the sky layer shadows the block (it is
	// higher), but the block shadows nothing of the layer above it.
	df_reset(DL_LAYER_SKY_ENEMY);
	df_block(100, 60, 120, 80, DL_LAYER_BG2);
	DF_REQUIRE(df_run(MODERN_QUALITY_HIGH, NULL) > 0);
	for (int y = 0; y < DF_H; ++y)
		for (int x = 0; x < DF_W; ++x)
			if (df_layers[y * DF_W + x] == DL_LAYER_SKY_ENEMY)
				DF_REQUIRE(df_px(x, y) == DF_GREY);
	puts("Depth fixture PASS: same layer, equal height and lower-over-higher do not darken");

	// Drawing order: a higher layer painted BEFORE the receiver (smaller rank) casts
	// nothing on it.
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG2);
	df_rank[DL_LAYER_BG2] = 1;
	df_rank[DL_LAYER_BG1] = 2;
	DF_REQUIRE(df_run(MODERN_QUALITY_HIGH, NULL) == 0 && df_changed() == 0);
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG2);
	df_rank[DL_LAYER_BG1] = 0;  // never drawn this tick
	DF_REQUIRE(df_run(MODERN_QUALITY_HIGH, NULL) == 0 && df_changed() == 0);
	puts("Depth fixture PASS: the rank table decides what is below");

	// Non-casters never cast and non-receivers never receive.
	static const int non_casters[] =
	{
		DL_LAYER_STARFIELD, DL_LAYER_PLAYER_SHOT, DL_LAYER_ENEMY_SHOT, DL_LAYER_EXPLOSION,
		DL_LAYER_SUPERPIXEL, DL_LAYER_HUD, DL_LAYER_OTHER, DL_LAYER_BG1, DL_LAYER_NONE,
	};
	for (size_t i = 0; i < sizeof non_casters / sizeof non_casters[0]; ++i)
	{
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 120, 80, non_casters[i]);
		DF_REQUIRE(df_run(MODERN_QUALITY_HIGH, NULL) == 0 && df_changed() == 0);
	}
	static const int non_receivers[] =
	{
		DL_LAYER_STARFIELD, DL_LAYER_PLAYER_SHOT, DL_LAYER_ENEMY_SHOT, DL_LAYER_EXPLOSION,
		DL_LAYER_SUPERPIXEL, DL_LAYER_HUD, DL_LAYER_OTHER, DL_LAYER_NONE,
	};
	for (size_t i = 0; i < sizeof non_receivers / sizeof non_receivers[0]; ++i)
	{
		df_reset(non_receivers[i]);
		df_block(100, 60, 120, 80, DL_LAYER_TOP_ENEMY);
		df_rank[non_receivers[i]] = 1;
		df_run(MODERN_QUALITY_HIGH, NULL);
		// Only the area outside the caster holds the non-receiver; none of it darkens.
		for (int y = 0; y < DF_H; ++y)
			for (int x = 0; x < DF_W; ++x)
				if (df_layers[y * DF_W + x] == non_receivers[i])
					DF_REQUIRE(df_px(x, y) == DF_GREY);
	}
	puts("Depth fixture PASS: shots, explosions, superpixels, starfield, HUD and text neither cast nor receive");

	// VFX: a flagged pixel never casts, and keeps its underlying layer as a receiver.
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG2 | DL_LAYER_VFX_FLAG);
	DF_REQUIRE(df_run(MODERN_QUALITY_HIGH, NULL) == 0 && df_changed() == 0);
	{
		static Uint32 plain[DF_PITCH * DF_ROWS];
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 120, 80, DL_LAYER_BG2);
		df_run(MODERN_QUALITY_LOW, NULL);
		memcpy(plain, df_canvas, sizeof plain);

		// Same scene with scattered VFX over the receiving ground (and one over the
		// block): the ground keeps its shadow, so the darkening does not change.
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 120, 80, DL_LAYER_BG2);
		for (int y = 60; y < 90; y += 3)
			for (int x = 100; x < 130; x += 3)
				if (df_layers[y * DF_W + x] == DL_LAYER_BG1)
					df_layers[y * DF_W + x] |= DL_LAYER_VFX_FLAG;
		df_run(MODERN_QUALITY_LOW, NULL);
		int differing = 0;
		for (int y = 0; y < DF_H; ++y)
			for (int x = 0; x < DF_W; ++x)
				differing += df_px(x, y) != plain[y * DF_PITCH + DF_X0 + x];
		DF_REQUIRE(differing == 0);
	}
	puts("Depth fixture PASS: VFX pixels do not cast and keep the receiver (no holes)");

	// A translucent bg2 casts weaker than an opaque one.
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG2);
	memset(&stats, 0, sizeof stats);
	df_run(MODERN_QUALITY_LOW, &stats);
	const Uint32 opaque = df_px(122, 70);
	DF_REQUIRE(stats.blend_casters == 0);
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG2 | DL_LAYER_BLEND);
	memset(&stats, 0, sizeof stats);
	df_run(MODERN_QUALITY_LOW, &stats);
	DF_REQUIRE(stats.blend_casters > 0);
	DF_REQUIRE((df_px(122, 70) & 0xff) > (opaque & 0xff) && df_px(122, 70) < DF_GREY);
	puts("Depth fixture PASS: blended bg2 casts weakly");

	// The soft fringe does not dirty a ship (higher than the ground enemy shadow).
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_GROUND_ENEMY);
	df_block(123, 60, 126, 80, DL_LAYER_PLAYER);
	df_run(MODERN_QUALITY_HIGH, NULL);
	for (int y = 60; y <= 80; ++y)
		for (int x = 123; x <= 126; ++x)
			DF_REQUIRE(df_px(x, y) == DF_GREY);
	puts("Depth fixture PASS: a higher layer next to a shadow keeps its colour");

	// Nothing leaks outside the playfield: casters packed against the right and
	// bottom edges, at every quality.
	for (int q = MODERN_QUALITY_LOW; q <= MODERN_QUALITY_HIGH; ++q)
	{
		df_reset(DL_LAYER_BG1);
		df_block(DF_W - 30, 0, DF_W - 1, DF_H - 1, DL_LAYER_TOP_ENEMY);
		df_block(0, DF_H - 30, DF_W - 1, DF_H - 1, DL_LAYER_BG3);
		df_run((ModernQuality)q, NULL);
		DF_REQUIRE(df_margin_clean());
	}
	puts("Depth fixture PASS: no darkening outside the playfield");

	// High is stronger than Low; the pass is deterministic.
	long low_sum = 0, high_sum = 0;
	for (int q = MODERN_QUALITY_LOW; q <= MODERN_QUALITY_HIGH; ++q)
	{
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 140, 100, DL_LAYER_TOP_ENEMY);
		df_run((ModernQuality)q, NULL);
		long sum = 0;
		for (int y = 0; y < DF_H; ++y)
			for (int x = 0; x < DF_W; ++x)
				sum += (long)(DF_GREY & 0xff) - (long)(df_px(x, y) & 0xff);
		if (q == MODERN_QUALITY_LOW) low_sum = sum; else high_sum = sum;
	}
	DF_REQUIRE(low_sum > 0 && high_sum > low_sum);
	{
		static Uint32 first[DF_PITCH * DF_ROWS];
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 140, 100, DL_LAYER_TOP_ENEMY);
		df_run(MODERN_QUALITY_HIGH, NULL);
		memcpy(first, df_canvas, sizeof first);
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 140, 100, DL_LAYER_TOP_ENEMY);
		df_run(MODERN_QUALITY_HIGH, NULL);
		DF_REQUIRE(memcmp(first, df_canvas, sizeof first) == 0);
	}
	printf("Depth fixture PASS: High darker than Low (%ld vs %ld), deterministic\n", high_sum, low_sum);

	puts("Depth fixture PASS: all rules");
	return 0;
}
