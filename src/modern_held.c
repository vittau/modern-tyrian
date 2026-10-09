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
#include "modern_held.h"

#include "logging.h"
#include "modern_bloom.h"
#include "modern_depth.h"

#include <string.h>

#define MH_W MODERN_PLAYFIELD_W
#define MH_H MODERN_PLAYFIELD_H

// Fixed-capacity statics: no per-frame allocation.
static Uint8 mh_snap_idx[MH_W * MH_H];   // playfield indices of the last really presented frame
static bool mh_snap_valid = false;
static bool mh_held = false;             // a held in-level screen is up

static Uint8 mh_overlay[MH_W * MH_H];    // 1 = overlay pixel of the held frame being built
static bool mh_overlay_active = false;   // this frame reuses the snapshot
static unsigned long mh_overlay_px = 0;  // overlay pixels of this frame

static Uint32 mh_pre[MH_W * MH_H];       // canvas playfield before the passes (held frames)

// --regress-held-check: the post-pass playfield of the last live frame.
static bool mh_check = false;
static bool mh_capture_pending = false;
static bool mh_live_valid = false;
static Uint32 mh_live[MH_W * MH_H];
static Uint8 mh_dist[MH_W * MH_H];       // Chebyshev distance to the nearest overlay pixel (check only)

static unsigned long mh_frames = 0;
static unsigned long mh_overlay_total = 0;
static unsigned long mh_overlay_changed = 0;
static unsigned long mh_compared = 0;
static unsigned long mh_mismatch = 0;
static int mh_mismatch_dist = 0;         // farthest mismatch from the overlay
static unsigned long mh_shadowed = 0;
static unsigned long mh_space_frames = 0;
static unsigned long mh_emitters = 0;
static unsigned long mh_lit = 0;

void modern_held_set(bool held)
{
	mh_held = held;
}

void modern_held_reset(void)
{
	mh_snap_valid = false;
	mh_live_valid = false;
	mh_capture_pending = false;
	mh_overlay_active = false;
	modern_depth_held_forget();
	modern_bloom_held_forget();
}

void modern_held_set_check(bool check)
{
	mh_check = check;
}

void modern_held_capture(const SDL_Surface *playfield)
{
	mh_capture_pending = false;

	const bool depth = modern_depth_held_save();
	const bool light = modern_bloom_held_save();
	mh_snap_valid = (depth || light) && playfield != NULL &&
	                playfield->w >= MH_W && playfield->h >= MH_H;
	if (!mh_snap_valid)
		return;

	for (int y = 0; y < MH_H; ++y)
		memcpy(mh_snap_idx + (size_t)y * MH_W,
		       (const Uint8 *)playfield->pixels + (size_t)y * (size_t)playfield->pitch, MH_W);

	mh_capture_pending = mh_check;
}

void modern_held_prepare(const ModernFrame *frame, bool plain_playfield)
{
	mh_overlay_active = false;
	mh_overlay_px = 0;

	if (!mh_held || !mh_snap_valid || !plain_playfield || !frame->gameplay || frame->src == NULL)
		return;

	const int playfield_x = frame->content_offset_x;
	if (playfield_x < 0 || playfield_x + MH_W > frame->w || MH_H > frame->h)
		return;

	// Overlay = every playfield pixel whose palette index is not the one the
	// last real frame presented there.
	unsigned long overlay = 0;
	for (int y = 0; y < MH_H; ++y)
	{
		const Uint8 *src = frame->src + (size_t)y * (size_t)frame->src_pitch;
		const Uint8 *snap = mh_snap_idx + (size_t)y * MH_W;
		Uint8 *out = mh_overlay + (size_t)y * MH_W;

		for (int x = 0; x < MH_W; ++x)
		{
			const Uint8 changed = src[x] != snap[x];
			out[x] = changed;
			overlay += changed;
		}
	}
	mh_overlay_px = overlay;

	modern_depth_held_restore(mh_overlay);
	modern_bloom_held_restore(mh_overlay);

	for (int y = 0; y < MH_H; ++y)
		memcpy(mh_pre + (size_t)y * MH_W, frame->pixels + (size_t)y * (size_t)frame->w + playfield_x,
		       MH_W * sizeof(Uint32));

	mh_overlay_active = true;
}

const Uint8 *modern_held_overlay(void)
{
	return mh_overlay_active ? mh_overlay : NULL;
}

// Chebyshev distance of every pixel to the nearest overlay pixel (saturating at
// 255), two chamfer passes.  A mismatch with the live frame can only be the
// fringe of an effect whose source sits under the overlay, so it must be near it.
static void mh_build_distance(void)
{
	for (int i = 0; i < MH_W * MH_H; ++i)
		mh_dist[i] = mh_overlay[i] ? 0 : 255;

	for (int y = 0; y < MH_H; ++y)
	{
		for (int x = 0; x < MH_W; ++x)
		{
			int d = mh_dist[y * MH_W + x];
			if (x > 0 && mh_dist[y * MH_W + x - 1] + 1 < d) d = mh_dist[y * MH_W + x - 1] + 1;
			if (y > 0)
			{
				if (mh_dist[(y - 1) * MH_W + x] + 1 < d) d = mh_dist[(y - 1) * MH_W + x] + 1;
				if (x > 0 && mh_dist[(y - 1) * MH_W + x - 1] + 1 < d) d = mh_dist[(y - 1) * MH_W + x - 1] + 1;
				if (x + 1 < MH_W && mh_dist[(y - 1) * MH_W + x + 1] + 1 < d) d = mh_dist[(y - 1) * MH_W + x + 1] + 1;
			}
			mh_dist[y * MH_W + x] = (Uint8)MIN(d, 255);
		}
	}

	for (int y = MH_H - 1; y >= 0; --y)
	{
		for (int x = MH_W - 1; x >= 0; --x)
		{
			int d = mh_dist[y * MH_W + x];
			if (x + 1 < MH_W && mh_dist[y * MH_W + x + 1] + 1 < d) d = mh_dist[y * MH_W + x + 1] + 1;
			if (y + 1 < MH_H)
			{
				if (mh_dist[(y + 1) * MH_W + x] + 1 < d) d = mh_dist[(y + 1) * MH_W + x] + 1;
				if (x > 0 && mh_dist[(y + 1) * MH_W + x - 1] + 1 < d) d = mh_dist[(y + 1) * MH_W + x - 1] + 1;
				if (x + 1 < MH_W && mh_dist[(y + 1) * MH_W + x + 1] + 1 < d) d = mh_dist[(y + 1) * MH_W + x + 1] + 1;
			}
			mh_dist[y * MH_W + x] = (Uint8)MIN(d, 255);
		}
	}
}

void modern_held_finish(const ModernFrame *frame)
{
	const int playfield_x = frame->content_offset_x;

	if (mh_overlay_active)
	{
		mh_frames++;
		mh_overlay_total += mh_overlay_px;

		const bool compare = mh_check && mh_live_valid;
		if (compare && mh_overlay_px > 0)
			mh_build_distance();
		for (int y = 0; y < MH_H; ++y)
		{
			const Uint32 *post = frame->pixels + (size_t)y * (size_t)frame->w + playfield_x;
			const Uint32 *pre = mh_pre + (size_t)y * MH_W;
			const Uint32 *live = mh_live + (size_t)y * MH_W;
			const Uint8 *overlay = mh_overlay + (size_t)y * MH_W;

			for (int x = 0; x < MH_W; ++x)
			{
				if (overlay[x])
				{
					if (post[x] != pre[x])
						mh_overlay_changed++;
				}
				else if (compare)
				{
					mh_compared++;
					if (post[x] != live[x])
					{
						mh_mismatch++;
						const int d = mh_overlay_px > 0 ? mh_dist[y * MH_W + x] : 0;
						if (d > mh_mismatch_dist)
							mh_mismatch_dist = d;
					}
				}
			}
		}

		mh_overlay_active = false;
	}
	else if (mh_capture_pending && mh_check)
	{
		// The post-pass playfield of the live frame the snapshot belongs to.
		if (playfield_x >= 0 && playfield_x + MH_W <= frame->w && MH_H <= frame->h)
		{
			for (int y = 0; y < MH_H; ++y)
				memcpy(mh_live + (size_t)y * MH_W,
				       frame->pixels + (size_t)y * (size_t)frame->w + playfield_x,
				       MH_W * sizeof(Uint32));
			mh_live_valid = true;
		}
	}

	mh_capture_pending = false;
}

void modern_held_note_shadow(unsigned long shadowed, bool space)
{
	mh_shadowed += shadowed;
	if (space)
		mh_space_frames++;
}

void modern_held_note_light(unsigned long emitters, unsigned long lit)
{
	mh_emitters += emitters;
	mh_lit += lit;
}

void modern_held_log_stats(void)
{
	logInfo("Depth held: frames=%lu shadowed_px=%lu space_frames=%lu overlay_px=%lu overlay_changed_px=%lu",
	        mh_frames, mh_shadowed, mh_space_frames, mh_overlay_total, mh_overlay_changed);
	logInfo("Light held: frames=%lu emitter_px=%lu lit_px=%lu overlay_px=%lu overlay_changed_px=%lu",
	        mh_frames, mh_emitters, mh_lit, mh_overlay_total, mh_overlay_changed);
	if (mh_check)
		logInfo("Held check: frames=%lu compared_px=%lu mismatch_px=%lu mismatch_max_dist=%d",
		        mh_frames, mh_compared, mh_mismatch, mh_mismatch_dist);
}
