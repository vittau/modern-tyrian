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
#include "modern_bloom.h"

#include "drawlist.h"
#include "opentyr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const modern_quality_names[MODERN_QUALITY_MAX] =
{
	"off",
	"low",
	"high",
};

// Modern users get the effects on by default (Low); Classic never reaches this
// code (the Modern pass list is only run by modern_build_frame()).  Regression
// mode pins both OFF unless --regress-lighting asks otherwise.
ModernQuality modern_bloom_quality = MODERN_QUALITY_LOW;
ModernQuality modern_lighting_quality = MODERN_QUALITY_LOW;

bool set_modern_quality_by_name(const char *name, ModernQuality *quality)
{
	// "medium" is the pre-merge spelling of today's High; keep accepting it from
	// old configs and command lines.
	if (strcmp(name, "medium") == 0)
	{
		*quality = MODERN_QUALITY_HIGH;
		return true;
	}

	for (int i = 0; i < MODERN_QUALITY_MAX; ++i)
	{
		if (strcmp(name, modern_quality_names[i]) == 0)
		{
			*quality = (ModernQuality)i;
			return true;
		}
	}
	return false;
}

bool modern_lighting_tags_wanted(void)
{
	return presentation == PRESENTATION_MODERN &&
	       (modern_bloom_quality != MODERN_QUALITY_OFF ||
	        modern_lighting_quality != MODERN_QUALITY_OFF);
}

// --- Emission tag (playfield) -------------------------------------------------
//
// The 264x184 tag that the pass consumes.  It is filled per presented frame by
// interp.c (from the game's tag buffer produced by drawlist.c) plus the VFX
// renderer.  Beside it, the per-pixel object-light palette index says which
// colour the tagged object's light should take.  Fixed-capacity statics: no
// per-frame allocation.

static Uint8 mb_tag[MODERN_PLAYFIELD_W * MODERN_PLAYFIELD_H];
static Uint8 mb_lcol[MODERN_PLAYFIELD_W * MODERN_PLAYFIELD_H];  // 0 = use the pixel's own colour
static bool mb_tag_valid = false;

void modern_bloom_tag_begin(void)
{
	if (!modern_lighting_tags_wanted())
	{
		mb_tag_valid = false;
		return;
	}

	memset(mb_tag, DL_TAG_NONE, sizeof mb_tag);
	memset(mb_lcol, 0, sizeof mb_lcol);
	mb_tag_valid = true;
}

void modern_bloom_tag_from_game(const Uint8 *game_tag, int game_pitch, bool flip)
{
	if (!mb_tag_valid)
		return;

	for (int y = 0; y < MODERN_PLAYFIELD_H; ++y)
	{
		Uint8 *dst = mb_tag + (size_t)y * MODERN_PLAYFIELD_W;

		if (game_tag == NULL)
		{
			memset(dst, DL_TAG_NONE, MODERN_PLAYFIELD_W);
			continue;
		}

		// The playfield copy always starts at game x = 24 (both the normal and
		// the spotlight special code read the same columns); the vertical-flip
		// special code reverses the rows.
		const int sy = flip ? (MODERN_PLAYFIELD_H - 1 - y) : y;
		memcpy(dst, game_tag + (size_t)sy * (size_t)game_pitch + 24, MODERN_PLAYFIELD_W);
	}
}

void modern_bloom_lightcol_from_game(const Uint8 *game_lcol, int game_pitch, bool flip)
{
	if (!mb_tag_valid)
		return;

	for (int y = 0; y < MODERN_PLAYFIELD_H; ++y)
	{
		Uint8 *dst = mb_lcol + (size_t)y * MODERN_PLAYFIELD_W;

		if (game_lcol == NULL)
		{
			memset(dst, 0, MODERN_PLAYFIELD_W);
			continue;
		}

		const int sy = flip ? (MODERN_PLAYFIELD_H - 1 - y) : y;
		memcpy(dst, game_lcol + (size_t)sy * (size_t)game_pitch + 24, MODERN_PLAYFIELD_W);
	}
}

void modern_bloom_tag_pixel(int x, int y)
{
	if (!mb_tag_valid)
		return;
	if ((unsigned)x >= MODERN_PLAYFIELD_W || (unsigned)y >= MODERN_PLAYFIELD_H)
		return;

	mb_tag[(size_t)y * MODERN_PLAYFIELD_W + (size_t)x] = DL_TAG_VFX;
	// VFX are drawn with their own palette index and emit their own colour.
	mb_lcol[(size_t)y * MODERN_PLAYFIELD_W + (size_t)x] = 0;
}

// --- Emission statistics (--light-tag-stats) ----------------------------------
//
// Debug-only per-class counters of the playfield pixels that pass the emissive
// threshold.  Used to prove that no DL_TAG_NONE (ship/HUD/background/text)
// pixel contributes to the light.  Never read by the pass.

static int mb_threshold_override = -1;

void modern_bloom_set_threshold(int threshold)
{
	mb_threshold_override = threshold;
}

static bool mb_stats_enabled = false;
static bool mb_stats_registered = false;
static unsigned long mb_stat_emissive[DL_TAG_MAX];
static unsigned long mb_stat_tagged[DL_TAG_MAX];
static unsigned long mb_stat_untagged = 0;
static unsigned long mb_stat_frames = 0;

// Emitted-colour statistics: for every pixel that passes the *light* threshold
// and carries a tag, accumulate the weighted light colour the mask would get,
// its palette hue block (index >> 4) and whether the pixel is near-white.  The
// accumulation is weighted by the emission strength (the max channel of the
// weighted light colour), so it reports the colour of the emitted light energy,
// not of the pixel count.  This is what tells us whether the light reads white
// (a desaturated energy-weighted hue) or the object's colour.  Debug only.
static unsigned long mb_stat_rgb[DL_TAG_MAX][3];   // sum of weighted colour
static unsigned long mb_stat_energy[DL_TAG_MAX];   // sum of max channel
static unsigned long mb_stat_hue[DL_TAG_MAX][16];  // energy per palette hue block
static unsigned long mb_stat_white[DL_TAG_MAX];    // energy from near-white pixels

static void mb_stats_print(void)
{
	static const char *const names[DL_TAG_MAX] =
	{
		"none", "player-shot", "enemy-shot", "explosion", "item", "superpixel", "vfx",
	};
	unsigned long total = 0;

	printf("light tag stats: %lu gameplay frames\n", mb_stat_frames);
	for (int i = 0; i < DL_TAG_MAX; ++i)
	{
		printf("  %-12s emit %lu   tagged %lu", names[i], mb_stat_emissive[i], mb_stat_tagged[i]);
		const unsigned long e = mb_stat_energy[i];
		if (e > 0)
		{
			const double r = (double)mb_stat_rgb[i][0] / e,
			             g = (double)mb_stat_rgb[i][1] / e,
			             b = (double)mb_stat_rgb[i][2] / e;
			const double mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
			const double mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
			const double sat = mx > 0.0 ? (mx - mn) / mx : 0.0;
			printf("   light chroma (%3.0f,%3.0f,%3.0f) sat %.2f  white-energy %.0f%%",
			       r * 255.0 / mx, g * 255.0 / mx, b * 255.0 / mx, sat,
			       100.0 * (double)mb_stat_white[i] / e);
		}
		printf("\n");
		if (e > 0)
		{
			printf("               hue (energy %%):");
			for (int h = 0; h < 16; ++h)
				printf(" %lu", mb_stat_hue[i][h] * 100 / e);
			printf("\n");
		}
		total += mb_stat_emissive[i];
	}
	printf("  %-12s %lu  (bright but untagged: excluded)\n", "emissive", total);
	printf("  %-12s %lu\n", "untagged", mb_stat_untagged);
}

void modern_bloom_set_stats(bool enabled)
{
	mb_stats_enabled = enabled;
	if (enabled && !mb_stats_registered)
	{
		atexit(mb_stats_print);
		mb_stats_registered = true;
	}
}

// --- Emissive detection -----------------------------------------------------
//
// Tyrian's palette is a 16x16 hue x brightness grid: index = hue * 16 +
// brightness, and each hue ramps from black to its brightest shade.  We use
// the *active* palette's largest channel as the emissive strength rather than
// the brightness nibble of the source index.  Reasons:
//
//   * It tracks the palette as displayed.  Palette fades are common in Tyrian
//     (level start/end, explosions, the white flash); using RGB means a
//     fade-to-black stops emitting light and a fade-to-white blooms, which is
//     what a player expects.  The raw index nibble would keep "bright" pixels
//     emitting while the screen is black.
//   * It is perceptually correct across hues.  The low-brightness end of a
//     saturated blue (e.g. index 157 = (22,39,59)) has a high nibble (13) but
//     is dark; max-channel correctly rejects it, while a grey at the same
//     nibble (index 13 = (51,51,51)) is accepted.
//
// The threshold is what keeps mid-bright backgrounds from blooming.  Without a
// per-pixel tag buffer (a later phase) bright backdrops such as white clouds
// or light rock can still glow; the tuned thresholds keep that from
// dominating.
//
// Per-class emission weight (Q8: 256 = full).  Explosions and the wide VFX are
// the reference; a pickup or a player shot contributes less so the frequent
// small objects do not outshine an explosion, and overlapping shots add less.
// Indexed by the DL_TAG_* class; DL_TAG_NONE is 0, so the tag-0 table is all
// zero and the mask code needs no per-pixel branch.
static const Uint16 mb_tag_weight_q8[DL_TAG_MAX] =
{
	0,     // DL_TAG_NONE
	190,   // DL_TAG_PLAYER_SHOT
	224,   // DL_TAG_ENEMY_SHOT
	256,   // DL_TAG_EXPLOSION
	170,   // DL_TAG_ITEM
	200,   // DL_TAG_SUPERPIXEL
	224,   // DL_TAG_VFX
};

// Per-class table of the object light colour: palette[index] scaled by the
// class weight (Q8).  The mask loop looks the object's representative index up
// here, then multiplies the result by the pixel's own emission weight
// (mb_bloom_w/mb_light_w), so the *hue* comes from the object's dominant colour
// while the *shape* still comes from the pixel.  DL_TAG_NONE's table is all
// zero, so a non-emissive pixel contributes nothing even if it is bright.
static Uint8 mb_rep[DL_TAG_MAX][256 * 3];

// Per-palette-index emission weight (Q8: 0 = below threshold), one table per
// effect: (max channel - threshold) rescaled to 0..255.
static Uint16 mb_bloom_w[256];
static Uint16 mb_light_w[256];

// Per-hue-family fallback colour: the brightest, most saturated shade of each
// hue block of the active palette.  Used as the light colour for a pixel with
// no object colour (VFX, superpixels and anything not stamped by a sprite
// blit), so even those emit their hue's saturated shade rather than their own
// near-white core.  This is the "derive the light from the hue block's
// saturated shade" half of the design; the per-object representative below is
// the other half.
static Uint8 mb_hue_rep[16];

// --- Per-sprite emissive-footprint cap for player shots ----------------------
//
// The pixel-derived light is a blurred sum of every tagged pixel's emission, so
// a sprite's contribution grows with its emissive *area*.  A large, dense
// player shot (the Mega Cannon at power 6 is the reported case; the high-power
// Laser and Zica are the same kind) then floods the field with a wide, bright
// halo, while the small shots (Pulse-Cannon, Vulcan, Lightning) sit well below
// it.  Each sprite blit stamps its bright-pixel count (its emissive footprint,
// drawlist.c) into the tag; a player-shot pixel's emission is scaled by
// MB_OBJ_FOOT_REF / footprint once the sprite has more than MB_OBJ_FOOT_REF
// bright pixels, i.e. one sprite never emits more light than a reference-sized
// one however big it is.  Sprites at or below the reference are untouched (the
// Pulse-Cannon, Multi-Cannon, Vulcan and Protron shots are all under 30 bright
// pixels; the Mega Cannon orb's four sprites are 70-95 each).
//
// Only the player-shot class is capped.  The flood comes from the sustained
// fire the player holds down, while explosions (up to ~110 bright pixels) and
// pickups are brief and are meant to flash, so they keep their full emission.
#define MB_OBJ_FOOT_REF 32
static Uint16 mb_oscale[DL_TAG_FOOT_STEPS];  // Q8: 256 = unchanged

static void mb_build_oscale(void)
{
	mb_oscale[0] = 256;  // no object footprint: unchanged
	for (int i = 1; i < DL_TAG_FOOT_STEPS; ++i)
	{
		// The step's centre (a step covers DL_TAG_FOOT_PER_STEP bright pixels).
		const int foot = i * DL_TAG_FOOT_PER_STEP + DL_TAG_FOOT_PER_STEP / 2;
		mb_oscale[i] = foot <= MB_OBJ_FOOT_REF
			? 256
			: (Uint16)((Uint32)MB_OBJ_FOOT_REF * 256u / (Uint32)foot);
	}
}

// The Q8 footprint scale of a tag byte: the cap above for player shots, none
// (256) for every other class.
static inline Uint16 mb_tag_oscale(Uint8 tag)
{
	return (tag & DL_TAG_CLASS_MASK) == DL_TAG_PLAYER_SHOT
		? mb_oscale[tag >> DL_TAG_FOOT_SHIFT]
		: 256;
}

// Ceiling on the combined per-pixel glow before it is screen-blended.  Keeps a
// dense volley of overlapping shots from saturating into a solid coloured blob;
// the base pixel keeps its own detail above it.  Round 3 raised the gains by
// 1.3x and re-checked the cap on the busiest scene (busy-f530, the densest
// player-shot volley of the demos): building with the cap at 255 produced
// byte-identical output, i.e. the combined glow never reaches 216 in these
// scenes, so the cap does not clip the increase and was left unchanged (raising
// it would only weaken the flood guard).
#define MB_GLOW_CAP 216

static void mb_build_tables(const SDL_Color *palette, int bloom_threshold, int light_threshold)
{
	mb_build_oscale();

	for (int i = 0; i < 256; ++i)
	{
		const int r = palette[i].r, g = palette[i].g, b = palette[i].b;
		int m = r > g ? r : g;
		if (b > m)
			m = b;

		int bw = 0, lw = 0;
		if (bloom_threshold < 255 && m > bloom_threshold)
			bw = (m - bloom_threshold) * 255 / (255 - bloom_threshold);
		if (light_threshold < 255 && m > light_threshold)
			lw = (m - light_threshold) * 255 / (255 - light_threshold);

		mb_bloom_w[i] = (Uint16)bw;
		mb_light_w[i] = (Uint16)lw;
	}

	// Expand palette colour * class weight once per class.
	for (int c = 0; c < DL_TAG_MAX; ++c)
	{
		const int w = mb_tag_weight_q8[c];
		for (int i = 0; i < 256; ++i)
		{
			mb_rep[c][i * 3 + 0] = (Uint8)(palette[i].r * w / 256);
			mb_rep[c][i * 3 + 1] = (Uint8)(palette[i].g * w / 256);
			mb_rep[c][i * 3 + 2] = (Uint8)(palette[i].b * w / 256);
		}
	}

	// Per-hue-family saturated representative (the no-object-colour fallback).
	for (int h = 0; h < 16; ++h)
	{
		int best = h * 16;
		int best_score = -1, best_max = -1;
		for (int b = 0; b < 16; ++b)
		{
			const int i = h * 16 + b;
			const int r = palette[i].r, g = palette[i].g, bl = palette[i].b;
			const int mx = MAX(r, MAX(g, bl));
			const int mn = MIN(r, MIN(g, bl));
			const int score = (mx - mn) * mx;
			if (score > best_score || (score == best_score && mx > best_max))
			{
				best_score = score;
				best_max = mx;
				best = i;
			}
		}
		mb_hue_rep[h] = (Uint8)best;
	}
}

// --- Separable box blur on the half-resolution grid -------------------------
//
// One line (row or column) of an RGB box blur, using a sliding sum so the cost
// is independent of the radius.  `stride` is the distance between consecutive
// samples in units of Uint8 (3 for a row, w*3 for a column); `recip` is
// round(65536 / (2*radius+1)) so the per-sample division becomes a multiply
// and a shift.  Edges clamp to the border sample.
static void mb_blur_line(const Uint8 *src, Uint8 *dst, int base, int stride, int count,
                         int radius, Uint32 recip)
{
	int sr = 0, sg = 0, sb = 0;

	for (int i = -radius; i <= radius; ++i)
	{
		const int j = MIN(MAX(i, 0), count - 1);
		const Uint8 *p = src + base + j * stride;
		sr += p[0];
		sg += p[1];
		sb += p[2];
	}

	for (int i = 0; i < count; ++i)
	{
		Uint8 *d = dst + base + i * stride;
		d[0] = (Uint8)(((Uint32)sr * recip + 0x8000u) >> 16);
		d[1] = (Uint8)(((Uint32)sg * recip + 0x8000u) >> 16);
		d[2] = (Uint8)(((Uint32)sb * recip + 0x8000u) >> 16);

		const int jo = MIN(MAX(i - radius, 0), count - 1);
		const int ji = MIN(MAX(i + radius + 1, 0), count - 1);
		const Uint8 *po = src + base + jo * stride;
		const Uint8 *pi = src + base + ji * stride;
		sr += pi[0] - po[0];
		sg += pi[1] - po[1];
		sb += pi[2] - po[2];
	}
}

// Blurs `w` x `h` RGB into `a`, using `b` as scratch.  The horizontal pass
// writes b, the vertical pass writes back into a, so the result is in a.
static void mb_blur(Uint8 *a, Uint8 *b, int w, int h, int radius, int iterations)
{
	const Uint32 recip = 65536u / (Uint32)(2 * radius + 1);

	for (int it = 0; it < iterations; ++it)
	{
		for (int y = 0; y < h; ++y)
			mb_blur_line(a, b, y * w * 3, 3, w, radius, recip);

		for (int x = 0; x < w; ++x)
			mb_blur_line(b, a, x * 3, w * 3, h, radius, recip);
	}
}

// --- Buffers ----------------------------------------------------------------
//
// The tight bloom is computed at half the logical resolution; the wide light
// map, which has no detail finer than its 20-60 px radius, is computed at a
// quarter.  Both are bilinearly upsampled.  They are inherently low-frequency,
// so this is visually indistinguishable while making the blurs 4x/16x cheaper.
// The buffers are fixed-capacity (the largest playfield the compositor ever
// produces, 264x184), so the per-frame path never allocates.
#define MB_W MODERN_PLAYFIELD_W
#define MB_H MODERN_PLAYFIELD_H

#define MB_LW (MB_W / 2)
#define MB_LH (MB_H / 2)
#define MB_LPIX (MB_LW * MB_LH)

#define MB_QW (MB_W / 4)
#define MB_QH (MB_H / 4)
#define MB_QPIX (MB_QW * MB_QH)

static Uint8 mb_bloom[MB_LPIX * 3];
static Uint8 mb_bloom_scratch[MB_LPIX * 3];
static Uint8 mb_light[MB_QPIX * 3];
static Uint8 mb_light_scratch[MB_QPIX * 3];
static Uint8 mb_light_half[MB_LPIX * 3];  // quarter light upsampled to half

// Per-quality tuning.  Threshold is the palette max-channel above which a pixel
// emits; radius is the blur radius in buffer pixels (half-resolution for bloom,
// quarter-resolution for light); gain is Q7 (the combined glow is scaled by
// gain / 128); ambient is Q8 (the base is scaled by ambient / 256 so unlit
// areas read slightly deeper while lit areas gain).
//
// Every level uses three box iterations.  Three separable boxes approximate a
// Gaussian closely enough that the isophotes are round (a 3-box kernel is
// radially symmetric to within ~4% at the half maximum; see the roundness note
// in the report), which two boxes were not: two left visible square/diamond
// halos.  The radius stays small because each extra iteration widens the
// support by roughly the radius again.
typedef struct
{
	Uint8 threshold;
	Uint8 radius;
	Uint16 gain;
	Uint16 ambient;
	Uint8 iterations;
} MbParams;

// Three levels (round 3, "light-plus30").  Thresholds stay at the round-2
// pre-tag values (224/216): only the bright cores of an emitter feed the glow,
// so a cube or a shot keeps its shape and the halo stays a soft rim.  The round
// 2 gains measured ~0.7x the pre-tag High/Low and the user found High a little
// soft once only a few object classes emit, so round 3 raises every gain by
// about 1.3x (bloom 108->140 / 180->234, light 380->494 / 640->832).  Low is
// still 0.6x High.  MB_GLOW_CAP was re-checked against the higher gains and does
// not clip them (see the cap comment), so it is unchanged.  The ambient is
// unchanged (a step closer to 256 than before, since with backgrounds excluded
// the unlit field would otherwise read darker).
static const MbParams mb_bloom_params[MODERN_QUALITY_MAX] =
{
	{   0,  0,     0, 256, 0 },  // off
	{ 224,  2,   140, 256, 3 },  // low  (0.6x high's gain; the screen blend and
	{ 224,  2,   234, 256, 3 },  // high  lighter ambient put the result ~0.5x)
};

static const MbParams mb_light_params[MODERN_QUALITY_MAX] =
{
	{   0,  0,     0, 256, 0 },  // off
	{ 216,  2,   494, 252, 3 },  // low  (0.6x high's gain, see above)
	{ 216,  2,   832, 248, 3 },  // high
};

// --- Explicit light sources (extension point) -------------------------------

typedef struct
{
	int x, y, radius;
	Uint8 r, g, b;
} MbSource;

static MbSource mb_sources[MODERN_LIGHT_MAX_SOURCES];
static int mb_source_count = 0;

void modern_lighting_reset_sources(void)
{
	mb_source_count = 0;
}

void modern_lighting_add_source(int x, int y, int radius, Uint8 r, Uint8 g, Uint8 b)
{
	if (mb_source_count >= MODERN_LIGHT_MAX_SOURCES)
		return;

	MbSource *source = &mb_sources[mb_source_count++];
	source->x = x;
	source->y = y;
	source->radius = radius;
	source->r = r;
	source->g = g;
	source->b = b;
}

// Adds the queued explicit sources to the quarter-resolution light buffer with
// a smooth squared falloff.  They land after the blur, so an exact per-object
// light is not smeared by the pixel-derived blur.  Not reached in this task
// (no caller), but it is the hook the per-object light list will use.
static void mb_add_explicit_sources(void)
{
	for (int s = 0; s < mb_source_count; ++s)
	{
		const MbSource *src = &mb_sources[s];
		const int cx = src->x / 4, cy = src->y / 4;
		const int rad = MAX(1, src->radius / 4);
		const int r2 = rad * rad;

		for (int ly = MAX(0, cy - rad); ly <= MIN(MB_QH - 1, cy + rad); ++ly)
		{
			for (int lx = MAX(0, cx - rad); lx <= MIN(MB_QW - 1, cx + rad); ++lx)
			{
				const int dx = lx - cx, dy = ly - cy;
				const int d2 = dx * dx + dy * dy;
				if (d2 >= r2)
					continue;

				const int fall = (r2 - d2) * 255 / r2;
				Uint8 *p = mb_light + ((size_t)ly * MB_QW + lx) * 3;
				p[0] = (Uint8)MIN(255, p[0] + src->r * fall / 255);
				p[1] = (Uint8)MIN(255, p[1] + src->g * fall / 255);
				p[2] = (Uint8)MIN(255, p[2] + src->b * fall / 255);
			}
		}
	}
}

// Emitted colour of one playfield pixel: the object's representative colour
// (or, when the object buffer holds 0 --- VFX/superpixels and anything not
// stamped by a sprite blit, the saturated shade of the pixel's own hue family),
// scaled by the pixel's own emission weight and by the object's per-object
// footprint scale (Q8; player shots only).  Packed 0xRRGGBB.
static inline Uint32 mb_emit_rgb(const Uint8 *rep_class, Uint8 pi, Uint8 rep, Uint16 w, Uint16 oscale)
{
	const Uint8 idx = rep != 0 ? rep : mb_hue_rep[pi >> 4];
	const Uint8 *c = rep_class + idx * 3;
	const Uint32 s = (Uint32)w * (Uint32)oscale >> 8;  // emission weight x object scale, Q8
	const Uint32 r = (Uint32)((c[0] * s) >> 8);
	const Uint32 g = (Uint32)((c[1] * s) >> 8);
	const Uint32 b = (Uint32)((c[2] * s) >> 8);
	return (r << 16) | (g << 8) | b;
}

// Builds the bloom mask at half resolution (2x2 block average) and the light
// mask at quarter resolution (4x4 block average).  Each pixel's colour is the
// object light colour (its representative shade) times the pixel's emission
// weight; a pixel whose tag is NONE, or which is below the effect's threshold,
// contributes nothing.  Each half is skipped when its effect is off.
static void mb_build_masks(const ModernFrame *frame, bool do_bloom, bool do_light)
{
	if (do_bloom)
	{
		for (int ly = 0; ly < MB_LH; ++ly)
		{
			const Uint8 *row0 = frame->src + (size_t)(ly * 2) * frame->src_pitch;
			const Uint8 *row1 = row0 + frame->src_pitch;
			const Uint8 *tag0 = mb_tag + (size_t)(ly * 2) * MODERN_PLAYFIELD_W;
			const Uint8 *tag1 = tag0 + MODERN_PLAYFIELD_W;
			const Uint8 *lc0 = mb_lcol + (size_t)(ly * 2) * MODERN_PLAYFIELD_W;
			const Uint8 *lc1 = lc0 + MODERN_PLAYFIELD_W;
			Uint8 *bloom = mb_bloom + (size_t)ly * MB_LW * 3;

			for (int lx = 0; lx < MB_LW; ++lx)
			{
				const int x0 = lx * 2, x1 = x0 + 1;
				const Uint8 t[4]  = { tag0[x0], tag0[x1], tag1[x0], tag1[x1] };
				const Uint8 p[4]  = { row0[x0], row0[x1], row1[x0], row1[x1] };
				const Uint8 rr[4] = { lc0[x0],  lc0[x1],  lc1[x0],  lc1[x1]  };
				Uint32 sr = 0, sg = 0, sb = 0;

				for (int k = 0; k < 4; ++k)
				{
					if ((t[k] & DL_TAG_CLASS_MASK) == DL_TAG_NONE)
						continue;
					const Uint8 cls = t[k] & DL_TAG_CLASS_MASK;
					const Uint32 v = mb_emit_rgb(mb_rep[cls], p[k], rr[k], mb_bloom_w[p[k]],
					                             mb_tag_oscale(t[k]));
					sr += (v >> 16) & 0xff;
					sg += (v >> 8) & 0xff;
					sb += v & 0xff;
				}

				Uint8 *d = bloom + lx * 3;
				d[0] = (Uint8)(sr >> 2);
				d[1] = (Uint8)(sg >> 2);
				d[2] = (Uint8)(sb >> 2);
			}
		}
	}

	if (do_light)
	{
		for (int qy = 0; qy < MB_QH; ++qy)
		{
			Uint8 *d = mb_light + (size_t)qy * MB_QW * 3;

			for (int qx = 0; qx < MB_QW; ++qx)
			{
				Uint32 sr = 0, sg = 0, sb = 0;

				for (int k = 0; k < 4; ++k)
				{
					const Uint8 *row = frame->src + (size_t)(qy * 4 + k) * frame->src_pitch + qx * 4;
					const Uint8 *tag = mb_tag + (size_t)(qy * 4 + k) * MODERN_PLAYFIELD_W + qx * 4;
					const Uint8 *lcl = mb_lcol + (size_t)(qy * 4 + k) * MODERN_PLAYFIELD_W + qx * 4;

					for (int j = 0; j < 4; ++j)
					{
						if ((tag[j] & DL_TAG_CLASS_MASK) == DL_TAG_NONE)
							continue;
						const Uint8 cls = tag[j] & DL_TAG_CLASS_MASK;
						const Uint32 v = mb_emit_rgb(mb_rep[cls], row[j], lcl[j], mb_light_w[row[j]],
						                             mb_tag_oscale(tag[j]));
						sr += (v >> 16) & 0xff;
						sg += (v >> 8) & 0xff;
						sb += v & 0xff;
					}
				}

				d[qx * 3 + 0] = (Uint8)(sr >> 4);
				d[qx * 3 + 1] = (Uint8)(sg >> 4);
				d[qx * 3 + 2] = (Uint8)(sb >> 4);
			}
		}
	}
}

// Debug-only: counts, per tag class, the playfield pixels whose palette entry
// is bright enough to emit.  Also counts bright pixels with tag NONE, which are
// exactly the ones the tag excludes (should stay 0 when the tag is built).  The
// emitted-colour statistics use the same colour the mask uses, so they report
// the tinted light.
static unsigned long mb_count_tags(const ModernFrame *frame, bool do_bloom, bool do_light)
{
	unsigned long item_tagged = 0;

	for (int y = 0; y < MODERN_PLAYFIELD_H; ++y)
	{
		const Uint8 *row = frame->src + (size_t)y * frame->src_pitch;
		const Uint8 *tag = mb_tag + (size_t)y * MODERN_PLAYFIELD_W;
		const Uint8 *lcl = mb_lcol + (size_t)y * MODERN_PLAYFIELD_W;

		for (int x = 0; x < MODERN_PLAYFIELD_W; ++x)
		{
			const Uint8 cls = tag[x] & DL_TAG_CLASS_MASK;
			if (cls != DL_TAG_NONE)
			{
				mb_stat_tagged[cls]++;
				if (cls == DL_TAG_ITEM)
					item_tagged++;
			}

			const bool bright =
				(do_bloom && mb_bloom_w[row[x]] != 0) ||
				(do_light && mb_light_w[row[x]] != 0);
			if (!bright)
				continue;

			if (cls != DL_TAG_NONE)
				mb_stat_emissive[cls]++;
			else
				mb_stat_untagged++;

			// Emitted-colour statistics (see mb_stats_print): only the light
			// threshold matters, and only the tag-carrying particles.  Weighted
			// by the emitted colour's max channel so the bright cores (which
			// dominate the blurred light) count more than the dim rims.
			if (do_light && mb_light_w[row[x]] != 0 && cls != DL_TAG_NONE)
			{
				const Uint32 v = mb_emit_rgb(mb_rep[cls], row[x], lcl[x], mb_light_w[row[x]],
				                             mb_tag_oscale(tag[x]));
				const int cr = (int)((v >> 16) & 0xff);
				const int cg = (int)((v >> 8) & 0xff);
				const int cb = (int)(v & 0xff);
				const int mx = MAX(cr, MAX(cg, cb));
				const int mn = MIN(cr, MIN(cg, cb));
				const Uint8 rep = lcl[x] != 0 ? lcl[x] : row[x];
				mb_stat_rgb[cls][0] += (unsigned long)cr;
				mb_stat_rgb[cls][1] += (unsigned long)cg;
				mb_stat_rgb[cls][2] += (unsigned long)cb;
				mb_stat_energy[cls] += (unsigned long)mx;
				mb_stat_hue[cls][rep >> 4] += (unsigned long)mx;
				if (mx > 0 && (mx - mn) * 4 < mx)
					mb_stat_white[cls] += (unsigned long)mx;
			}
		}
	}

	return item_tagged;
}

// Bilinearly upsamples the quarter-resolution light into mb_light_half, so it
// can be combined with the half-resolution bloom.
static void mb_upsample_light_half(void)
{
	for (int hy = 0; hy < MB_LH; ++hy)
	{
		const Uint32 qy = (Uint32)hy * 128 + 64;
		const int j0 = (int)(qy >> 8);
		const int j1 = MIN(j0 + 1, MB_QH - 1);
		const int wy0 = 256 - (int)(qy & 0xff), wy1 = (int)(qy & 0xff);
		const Uint8 *r0 = mb_light + (size_t)j0 * MB_QW * 3;
		const Uint8 *r1 = mb_light + (size_t)j1 * MB_QW * 3;
		Uint8 *d = mb_light_half + (size_t)hy * MB_LW * 3;

		for (int hx = 0; hx < MB_LW; ++hx)
		{
			const Uint32 qx = (Uint32)hx * 128 + 64;
			const int i0 = (int)(qx >> 8);
			const int i1 = MIN(i0 + 1, MB_QW - 1);
			const int wx0 = 256 - (int)(qx & 0xff), wx1 = (int)(qx & 0xff);
			const Uint8 *c00 = r0 + i0 * 3;
			const Uint8 *c10 = r0 + i1 * 3;
			const Uint8 *c01 = r1 + i0 * 3;
			const Uint8 *c11 = r1 + i1 * 3;
			const int w00 = wx0 * wy0, w10 = wx1 * wy0, w01 = wx0 * wy1, w11 = wx1 * wy1;

			d[hx * 3 + 0] = (Uint8)((c00[0] * w00 + c10[0] * w10 + c01[0] * w01 + c11[0] * w11) >> 16);
			d[hx * 3 + 1] = (Uint8)((c00[1] * w00 + c10[1] * w10 + c01[1] * w01 + c11[1] * w11) >> 16);
			d[hx * 3 + 2] = (Uint8)((c00[2] * w00 + c10[2] * w10 + c01[2] * w01 + c11[2] * w11) >> 16);
		}
	}
}

// Combines the blurred bloom (half resolution) and the upsampled light (half
// resolution) into mb_bloom, scaled by their gains and clamped.  The explicit
// sources were already folded into mb_light.
static void mb_combine(const MbParams *bloom, const MbParams *light)
{
	const int bgain = bloom->gain;
	const int lgain = light->gain;

	for (int i = 0; i < MB_LPIX * 3; ++i)
	{
		int v = (mb_bloom[i] * bgain >> 7) + (mb_light_half[i] * lgain >> 7);
		if (v > MB_GLOW_CAP)
			v = MB_GLOW_CAP;
		mb_bloom[i] = (Uint8)v;
	}
}

// Per-column bilinear weights for the 2x upsample, in Q8.  A logical pixel at
// x maps to half-resolution coordinate x/2 + 0.25, so i0 = (x*128 + 64) >> 8
// and the two weights are (256 - fx) and fx.  Precomputing them once removes
// the per-row index/weight work; `mb_ly0`/`mb_ly1` are the equivalent per-row
// values, recomputed inside the loop.
static int mb_lx0[MB_W];
static int mb_lx1[MB_W];
static int mb_lwx0[MB_W];
static int mb_lwx1[MB_W];

static void mb_build_xmap(void)
{
	for (int x = 0; x < MB_W; ++x)
	{
		const int p = x * 128 + 64;
		const int i0 = p >> 8;
		mb_lx0[x] = i0;
		mb_lx1[x] = MIN(i0 + 1, MB_LW - 1);
		mb_lwx0[x] = 256 - (p & 0xff);
		mb_lwx1[x] = (p & 0xff);
	}
}

// Applies the combined half-resolution glow to the playfield.  The glow is
// bilinearly upsampled (fixed-point Q8 weights) so it has no 2-px blocks, and
// blended with a screen-style saturating add:
//
//   out = base * ambient/256 + glow * (255 - base * ambient/256) / 256
//
// so a glow can only push a channel towards 255, never past it, and an already
// bright pixel keeps its detail instead of flattening to white (the plain
// add-then-clamp the first round used did flatten sprites and the boss top).
static void mb_apply(ModernFrame *frame, int playfield_x, const MbParams *light)
{
	const int amb = light->ambient;

	mb_build_xmap();

	for (int y = 0; y < MB_H; ++y)
	{
		Uint32 *canvas = frame->pixels + (size_t)y * frame->w + playfield_x;
		const Uint32 q = (Uint32)y * 128 + 64;
		const int j0 = (int)(q >> 8);
		const int j1 = MIN(j0 + 1, MB_LH - 1);
		const int wy0 = 256 - (int)(q & 0xff);
		const int wy1 = (int)(q & 0xff);
		const Uint8 *lrow0 = mb_bloom + (size_t)j0 * MB_LW * 3;
		const Uint8 *lrow1 = mb_bloom + (size_t)j1 * MB_LW * 3;

		for (int x = 0; x < MB_W; ++x)
		{
			const Uint32 base = canvas[x];
			const int i0 = mb_lx0[x], i1 = mb_lx1[x];
			const int wx0 = mb_lwx0[x], wx1 = mb_lwx1[x];
			const Uint8 *c00 = lrow0 + i0 * 3;
			const Uint8 *c10 = lrow0 + i1 * 3;
			const Uint8 *c01 = lrow1 + i0 * 3;
			const Uint8 *c11 = lrow1 + i1 * 3;
			const int w00 = wx0 * wy0, w10 = wx1 * wy0;
			const int w01 = wx0 * wy1, w11 = wx1 * wy1;

			const int gr = (c00[0] * w00 + c10[0] * w10 + c01[0] * w01 + c11[0] * w11) >> 16;
			const int gg = (c00[1] * w00 + c10[1] * w10 + c01[1] * w01 + c11[1] * w11) >> 16;
			const int gb = (c00[2] * w00 + c10[2] * w10 + c01[2] * w01 + c11[2] * w11) >> 16;

			const int r0 = (int)((base >> 16) & 0xff) * amb >> 8;
			const int g0 = (int)((base >> 8) & 0xff) * amb >> 8;
			const int b0 = (int)(base & 0xff) * amb >> 8;

			int r = r0 + (gr * (255 - r0) >> 8);
			int g = g0 + (gg * (255 - g0) >> 8);
			int bl = b0 + (gb * (255 - b0) >> 8);
			if (r > 255) r = 255;
			if (g > 255) g = 255;
			if (bl > 255) bl = 255;

			canvas[x] = ((Uint32)(Uint8)r << 16) | ((Uint32)(Uint8)g << 8) | (Uint32)(Uint8)bl;
		}
	}
}

void modern_bloom_pass(ModernFrame *frame)
{
	if (!frame->gameplay || frame->src == NULL || frame->palette == NULL)
	{
		modern_lighting_reset_sources();
		return;
	}

	const MbParams *bloom = &mb_bloom_params[modern_bloom_quality];
	const MbParams *light = &mb_light_params[modern_lighting_quality];

	if (bloom->gain == 0 && light->gain == 0)
	{
		modern_lighting_reset_sources();
		return;
	}

	// Every presented gameplay frame arms its own tag (interp.c).  A frame that
	// did not (the level intro/Warning hold, a screen composed another way)
	// emits nothing rather than reusing a stale tag.
	if (!mb_tag_valid)
	{
		modern_lighting_reset_sources();
		return;
	}
	mb_tag_valid = false;

	const int playfield_x = frame->content_offset_x;
	if (playfield_x < 0 || playfield_x + MB_W > frame->w || MB_H > frame->h)
	{
		modern_lighting_reset_sources();
		return;
	}

	const int bloom_threshold = mb_threshold_override >= 0 ? mb_threshold_override : bloom->threshold;
	const int light_threshold = mb_threshold_override >= 0 ? mb_threshold_override : light->threshold;
	mb_build_tables(frame->palette, bloom_threshold, light_threshold);
	mb_build_masks(frame, bloom->gain != 0, light->gain != 0);

	if (mb_stats_enabled)
	{
		mb_count_tags(frame, bloom->gain != 0, light->gain != 0);
		mb_stat_frames++;
	}

	if (bloom->gain != 0)
		mb_blur(mb_bloom, mb_bloom_scratch, MB_LW, MB_LH, bloom->radius, bloom->iterations);

	if (light->gain != 0)
	{
		mb_blur(mb_light, mb_light_scratch, MB_QW, MB_QH, light->radius, light->iterations);
		if (mb_source_count > 0)
			mb_add_explicit_sources();
		mb_upsample_light_half();
	}

	mb_combine(bloom, light);
	mb_apply(frame, playfield_x, light);

	modern_lighting_reset_sources();
}

// Temporary compile-time roundness check (build with -DMB_BLUR_SELFTEST and the
// file provides its own main).
//
// The blur is separable, so its 2D kernel is k(x)*k(y) for the 1D kernel k
// the real code produces.  This extracts that exact k (blurring a 1D impulse
// with mb_blur_line, three iterations) and measures how much further the
// half-maximum isophote reaches along the diagonal than along the axis:
// a circle gives 1.00, an axis-aligned square gives sqrt(2) = 1.414, a diamond
// gives 0.707.  Subpixel crossings are linearly interpolated.
#ifdef MB_BLUR_SELFTEST
#include <math.h>
#include <stdio.h>

#define RT_N 257
#define RT_C (RT_N / 2)

static double find_isophote(const double *k, int maxd, double thr, int diagonal)
{
	// value(t) on the axis is k(t)*k(0); on the diagonal it is k(t)^2.
	for (int t = 1; t <= maxd; ++t)
	{
		const double v1 = diagonal ? k[RT_C + t] * k[RT_C + t]
		                           : k[RT_C + t] * k[RT_C];
		const double v0 = diagonal ? k[RT_C + t - 1] * k[RT_C + t - 1]
		                           : k[RT_C + t - 1] * k[RT_C];
		if (v1 < thr)
		{
			const double frac = (v0 - thr) / (v0 - v1);
			return (t - 1) + frac;
		}
	}
	return maxd;
}

int main(void)
{
	static Uint8 a[RT_N * 3], b[RT_N * 3];
	static double k[RT_N];

	static const struct { const char *name; int radius; int iters; } tests[] =
	{
		{ "bloom-low", 2, 3 }, { "bloom-medium", 2, 3 }, { "bloom-high", 3, 3 },
		{ "light-low", 2, 3 }, { "light-medium", 3, 3 }, { "light-high", 5, 3 },
		{ "old-2box", 2, 2 },
	};

	printf("kernel roundness (diagonal isophote distance / axis isophote distance)\n");
	printf("%-14s r iters  axis   diag   ratio  (1.00 = round, 1.414 = square)\n", "name");

	for (size_t t = 0; t < COUNTOF(tests); ++t)
	{
		const int iters = tests[t].iters;
		memset(a, 0, sizeof a);
		a[RT_C * 3] = a[RT_C * 3 + 1] = a[RT_C * 3 + 2] = 255;

		// mb_blur with h=1: the vertical pass is the identity, so this is the
		// real 1D horizontal box-blur kernel repeated `iters` times.
		mb_blur(a, b, RT_N, 1, tests[t].radius, iters);

		for (int i = 0; i < RT_N; ++i)
			k[i] = a[i * 3];

		const double peak = k[RT_C] * k[RT_C];
		const double thr = peak * 0.5;
		const int maxd = MIN(RT_C - 1, tests[t].radius * 4 + 4);
		const double axis = find_isophote(k, maxd, thr, 0);
		const double diag = find_isophote(k, maxd, thr, 1);
		const double ratio = axis > 0.0 ? (diag * 1.41421356) / axis : 0.0;

		printf("%-14s %d %5d  %5.2f  %5.2f  %5.3f\n",
			tests[t].name, tests[t].radius, iters, axis, diag * 1.41421356, ratio);
	}

	return 0;
}
#endif
