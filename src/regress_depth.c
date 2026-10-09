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

static unsigned df_run(ModernDepth quality, ModernDepthShadowStats *stats)
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
	DF_REQUIRE(df_run(MODERN_DEPTH_OFF, &stats) == 0 && df_changed() == 0 && df_margin_clean());
	DF_REQUIRE(stats.casters[DL_LAYER_BG3] == 0);
	puts("Depth fixture PASS: Off is the identity");

	// A higher layer above a lower one darkens it: the ground right of / below the
	// block is shadowed, the block itself, and the ground far away, are not.
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG2);
	memset(&stats, 0, sizeof stats);
	DF_REQUIRE(df_run(MODERN_DEPTH_ON, &stats) > 0);
	DF_REQUIRE(stats.casters[DL_LAYER_BG2] > 0);
	DF_REQUIRE(df_px(122, 70) < DF_GREY);        // right of the block, inside the shadow
	DF_REQUIRE(df_px(110, 83) < DF_GREY);        // below the block, inside the shadow
	DF_REQUIRE(df_px(110, 70) == DF_GREY);       // the caster itself
	DF_REQUIRE(df_px(60, 70) == DF_GREY);        // left of it (the light side)
	DF_REQUIRE(df_px(110, 40) == DF_GREY);       // above it
	DF_REQUIRE(df_px(200, 150) == DF_GREY);      // far away
	DF_REQUIRE(df_margin_clean());
	puts("Depth fixture PASS: above + higher darkens; caster, light side and distance do not");

	// No halo: the receiver touching the caster is darkest. Walk away along
	// both exposed edges and along the light direction (2,3); darkness may only
	// decrease. This fails with pre-blur receiver clipping, including wide blur.
	for (int layer = DL_LAYER_BG2; layer <= DL_LAYER_BG3; layer += DL_LAYER_BG3 - DL_LAYER_BG2)
	{
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 120, 80, layer);
		df_run(MODERN_DEPTH_ON, NULL);
		for (int y = 60; y <= 80; ++y)
			for (int x = 100; x <= 120; ++x)
				DF_REQUIRE(df_px(x, y) == DF_GREY);
		for (int ray = 0; ray < 3; ++ray)
		{
			Uint32 previous = 0;
			for (int d = 0; d < 20; ++d)
			{
				const int x = ray == 0 ? 121 + d : ray == 1 ? 110 : 121 + 2*d;
				const int y = ray == 0 ? 75 : ray == 1 ? 81 + d : 81 + 3*d;
				const Uint32 p = df_px(x, y);
				DF_REQUIRE(p >= previous);
				if (d == 0) DF_REQUIRE(p < DF_GREY);
				previous = p;
			}
			DF_REQUIRE(previous == DF_GREY);
		}
	}
	puts("Depth fixture PASS: no halo, adjacent receivers darkest, three rays fade monotonically; all caster pixels unchanged");

	// Softness: a row crossing the shadow edge falls off in several distinct steps
	// (no hard pixel edge), and the channels stay in step (a grey stays grey).
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG3);
	DF_REQUIRE(df_run(MODERN_DEPTH_ON, NULL) > 0);
	{
		int levels = 0;
		Uint32 last = DF_GREY;
		for (int x = 121; x < 160; ++x)
		{
			const Uint32 p = df_px(x, 72);
			DF_REQUIRE(((p >> 16) & 0xff) == ((p >> 8) & 0xff) && ((p >> 8) & 0xff) == (p & 0xff));
			DF_REQUIRE(p >= last || x == 121);  // brightens monotonically past the shadow body
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
	DF_REQUIRE(df_run(MODERN_DEPTH_ON, NULL) == 0 && df_changed() == 0);
	df_reset(DL_LAYER_BG2);
	df_block(100, 60, 120, 80, DL_LAYER_GROUND_ENEMY);
	DF_REQUIRE(df_run(MODERN_DEPTH_ON, NULL) == 0 && df_changed() == 0);
	// A bg2 block on a sky-enemy background: the sky layer shadows the block (it is
	// higher), but the block shadows nothing of the layer above it.
	df_reset(DL_LAYER_SKY_ENEMY);
	df_block(100, 60, 120, 80, DL_LAYER_BG2);
	DF_REQUIRE(df_run(MODERN_DEPTH_ON, NULL) > 0);
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
	DF_REQUIRE(df_run(MODERN_DEPTH_ON, NULL) == 0 && df_changed() == 0);
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG2);
	df_rank[DL_LAYER_BG1] = 0;  // never drawn this tick
	DF_REQUIRE(df_run(MODERN_DEPTH_ON, NULL) == 0 && df_changed() == 0);
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
		DF_REQUIRE(df_run(MODERN_DEPTH_ON, NULL) == 0 && df_changed() == 0);
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
		df_run(MODERN_DEPTH_ON, NULL);
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
	DF_REQUIRE(df_run(MODERN_DEPTH_ON, NULL) == 0 && df_changed() == 0);
	{
		static Uint32 plain[DF_PITCH * DF_ROWS];
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 120, 80, DL_LAYER_BG2);
		df_run(MODERN_DEPTH_ON, NULL);
		memcpy(plain, df_canvas, sizeof plain);

		// Same scene with scattered VFX over the receiving ground (and one over the
		// block): the ground keeps its shadow, so the darkening does not change.
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 120, 80, DL_LAYER_BG2);
		for (int y = 60; y < 90; y += 3)
			for (int x = 100; x < 130; x += 3)
				if (df_layers[y * DF_W + x] == DL_LAYER_BG1)
					df_layers[y * DF_W + x] |= DL_LAYER_VFX_FLAG;
		df_run(MODERN_DEPTH_ON, NULL);
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
	df_run(MODERN_DEPTH_ON, &stats);
	const Uint32 opaque = df_px(122, 70);
	DF_REQUIRE(stats.blend_casters == 0);
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_BG2 | DL_LAYER_BLEND);
	memset(&stats, 0, sizeof stats);
	df_run(MODERN_DEPTH_ON, &stats);
	DF_REQUIRE(stats.blend_casters > 0);
	DF_REQUIRE((df_px(122, 70) & 0xff) > (opaque & 0xff) && df_px(122, 70) < DF_GREY);
	puts("Depth fixture PASS: blended bg2 casts weakly");

	// The soft fringe does not dirty a ship (higher than the ground enemy shadow).
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 120, 80, DL_LAYER_GROUND_ENEMY);
	df_block(123, 60, 126, 80, DL_LAYER_PLAYER);
	df_run(MODERN_DEPTH_ON, NULL);
	for (int y = 60; y <= 80; ++y)
		for (int x = 123; x <= 126; ++x)
			DF_REQUIRE(df_px(x, y) == DF_GREY);
	puts("Depth fixture PASS: a higher layer next to a shadow keeps its colour");

	// Nothing leaks outside the playfield: casters packed against the right and
	// bottom edges, at every quality.
	for (int q = MODERN_DEPTH_ON; q <= MODERN_DEPTH_ON; ++q)
	{
		df_reset(DL_LAYER_BG1);
		df_block(DF_W - 30, 0, DF_W - 1, DF_H - 1, DL_LAYER_TOP_ENEMY);
		df_block(0, DF_H - 30, DF_W - 1, DF_H - 1, DL_LAYER_BG3);
		df_run((ModernDepth)q, NULL);
		DF_REQUIRE(df_margin_clean());
	}
	puts("Depth fixture PASS: no darkening outside the playfield");

	// On is deterministic and has measurable full-silhouette darkening.
	long on_sum = 0;
	df_reset(DL_LAYER_BG1);
	df_block(100, 60, 140, 100, DL_LAYER_TOP_ENEMY);
	df_run(MODERN_DEPTH_ON, NULL);
	for (int y = 0; y < DF_H; ++y)
		for (int x = 0; x < DF_W; ++x)
			on_sum += (long)(DF_GREY & 0xff) - (long)(df_px(x, y) & 0xff);
	DF_REQUIRE(on_sum > 0);
	{
		static Uint32 first[DF_PITCH * DF_ROWS];
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 140, 100, DL_LAYER_TOP_ENEMY);
		df_run(MODERN_DEPTH_ON, NULL);
		memcpy(first, df_canvas, sizeof first);
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 140, 100, DL_LAYER_TOP_ENEMY);
		df_run(MODERN_DEPTH_ON, NULL);
		DF_REQUIRE(memcmp(first, df_canvas, sizeof first) == 0);
	}
	printf("Depth fixture PASS: On darkness %ld, deterministic\n", on_sum);

	// --- stage 3: atmospheric fog on bg1 ----------------------------------------
	{
		static const Uint32 bg1_px = 0x00604020u;  // a brown terrain colour
		Uint32 fog = 0;
		ModernDepthFogStats fs;

		// The fog colour comes from the bg1 mean, desaturated and lightened.
		df_reset(DL_LAYER_BG1);
		for (int i = 0; i < DF_PITCH * DF_ROWS; ++i)
			df_canvas[i] = bg1_px;
		DF_REQUIRE(modern_depth_fog_colour(df_canvas + DF_X0, DF_PITCH, df_layers, &fog));
		{
			const int r = (int)((fog >> 16) & 0xff), g = (int)((fog >> 8) & 0xff), b = (int)(fog & 0xff);
			DF_REQUIRE(r > 0x60 && g > 0x40 && b > 0x20);                 // lightened
			DF_REQUIRE(r - b < 0x60 - 0x20 && r >= g && g >= b);          // hue kept, saturation halved
		}
		// Too few bg1 samples: no colour.
		df_reset(DL_LAYER_BG3);
		df_block(0, 0, 9, 9, DL_LAYER_BG1);
		DF_REQUIRE(!modern_depth_fog_colour(df_canvas + DF_X0, DF_PITCH, df_layers, &fog));
		fog = 0x00c0c0c0u;

		// Off is the identity; On fogs exactly the bg1 pixels, towards the fog colour.
		df_reset(DL_LAYER_BG1);
		df_block(100, 60, 120, 80, DL_LAYER_BG2);
		df_block(130, 60, 140, 80, DL_LAYER_BG2 | DL_LAYER_BLEND);
		df_block(150, 60, 160, 80, DL_LAYER_BG1 | DL_LAYER_VFX_FLAG);
		df_block(170, 60, 180, 80, DL_LAYER_BG3);
		for (int i = 0; i < DF_PITCH * DF_ROWS; ++i)
			df_canvas[i] = bg1_px;
		memset(&fs, 0, sizeof fs);
		DF_REQUIRE(modern_depth_fog_apply(df_canvas + DF_X0, DF_PITCH, df_layers, MODERN_DEPTH_OFF, fog, &fs) == 0);
		for (int i = 0; i < DF_PITCH * DF_ROWS; ++i)
			DF_REQUIRE(df_canvas[i] == bg1_px);
		const unsigned long applied = modern_depth_fog_apply(df_canvas + DF_X0, DF_PITCH, df_layers, MODERN_DEPTH_ON, fog, &fs);
		DF_REQUIRE(applied == fs.fogged + fs.blend_fogged && fs.fogged > 0 && fs.blend_fogged == 11u * 21u);
		DF_REQUIRE(df_px(10, 10) != bg1_px && df_px(10, 10) > bg1_px);      // bg1 moved toward the lighter fog
		DF_REQUIRE((df_px(10, 10) & 0xff) > 0x20 && (df_px(10, 10) & 0xff) < (fog & 0xff));  // partly, not fully
		DF_REQUIRE(df_px(110, 70) == bg1_px);                                // opaque bg2: none
		DF_REQUIRE(df_px(155, 70) == bg1_px);                                // VFX pixel: none
		DF_REQUIRE(df_px(175, 70) == bg1_px);                                // bg3: none
		{
			const int full = (int)(df_px(10, 10) & 0xff) - 0x20, half = (int)(df_px(135, 70) & 0xff) - 0x20;
			DF_REQUIRE(half > 0 && half * 2 <= full + 1 && half * 2 >= full - 1);  // translucent bg2: half
		}
		for (int y = 0; y < DF_ROWS; ++y)        // the margin (all bg1_px) was not fogged
			for (int x = 0; x < DF_PITCH; ++x)
				if (!(y < DF_H && x >= DF_X0 && x < DF_X0 + DF_W))
					DF_REQUIRE(df_canvas[y * DF_PITCH + x] == bg1_px);
		// Subtle: no channel moves by more than 1/5 of its distance to the fog colour.
		for (int c = 0; c < 3; ++c)
		{
			const int sh = 16 - 8 * c, from = (int)((bg1_px >> sh) & 0xff), to = (int)((fog >> sh) & 0xff);
			const int moved = (int)((df_px(10, 10) >> sh) & 0xff) - from;
			DF_REQUIRE(abs(moved) * 5 <= abs(to - from));
		}
	}
	puts("Depth fixture PASS: fog colour from the bg1 mean; only bg1 fogged (blend half, VFX/bg2/bg3 none), subtle, margin clean");

	// --- stage 3: per-layer light weights ---------------------------------------
	{
		static const int terrain[] = { DL_LAYER_BG1, DL_LAYER_BG2, DL_LAYER_GROUND_ENEMY };
		for (size_t i = 0; i < sizeof terrain / sizeof terrain[0]; ++i)
			DF_REQUIRE(modern_depth_light_weight((Uint8)terrain[i]) == 256);
		DF_REQUIRE(modern_depth_light_weight(DL_LAYER_BG2 | DL_LAYER_BLEND) == 256);
		const unsigned sky = modern_depth_light_weight(DL_LAYER_SKY_ENEMY);
		const unsigned player = modern_depth_light_weight(DL_LAYER_PLAYER);
		const unsigned bg3 = modern_depth_light_weight(DL_LAYER_BG3);
		DF_REQUIRE(sky < 256 && player < 256 && bg3 < player && bg3 < sky && bg3 * 8 <= 256);
		DF_REQUIRE(modern_depth_light_weight(DL_LAYER_SIDEKICK) == player);
		DF_REQUIRE(modern_depth_light_weight(DL_LAYER_TOP_ENEMY) < 256);
		static const int unweighted[] =
		{
			DL_LAYER_PLAYER_SHOT, DL_LAYER_ENEMY_SHOT, DL_LAYER_EXPLOSION, DL_LAYER_SUPERPIXEL,
			DL_LAYER_STARFIELD, DL_LAYER_HUD, DL_LAYER_OTHER, DL_LAYER_NONE,
		};
		for (size_t i = 0; i < sizeof unweighted / sizeof unweighted[0]; ++i)
			DF_REQUIRE(modern_depth_light_weight((Uint8)unweighted[i]) == 256);
		// A VFX pixel over bg3 or the ship is lit as VFX (full), not as its layer.
		DF_REQUIRE(modern_depth_light_weight(DL_LAYER_BG3 | DL_LAYER_VFX_FLAG) == 256);
		DF_REQUIRE(modern_depth_light_weight(DL_LAYER_PLAYER | DL_LAYER_VFX_FLAG) == 256);
		// An id outside the table cannot read out of it.
		DF_REQUIRE(modern_depth_light_weight(DL_LAYER_ID_MASK) == 256);
	}
	puts("Depth fixture PASS: light weights: terrain full, sky/ship reduced, bg3 almost none, shots/VFX full");

	puts("Depth fixture PASS: all rules");
	return 0;
}
