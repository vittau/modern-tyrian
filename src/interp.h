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
#ifndef INTERP_H
#define INTERP_H

#include <SDL3/SDL.h>

#include <stdbool.h>

// Decoupled high-refresh presentation (Fase 2, stage 3).
//
// The game logic keeps its fixed tick.  In Modern gameplay, while the tick is
// waiting for its deadline, this module presents extra frames whose moving
// objects are linearly interpolated between the previous and the current tick's
// draw lists.  Everything here is presentation only: it never touches gameplay
// state, RNG or timing, and in Classic mode it reproduces the original path.

// "Smooth motion" setting (Modern only, default on).  Display-only.
extern bool interp_smooth_motion;

// Parses "on"/"off" (also true/false, 1/0).  Returns false for anything else.
bool set_smooth_motion_by_name(const char *name);

// True when the smooth presentation loop applies to the current gameplay frame:
// Modern mode, smooth motion on, the level's smooth scroll timing, not end-of-
// level and not netplay.  Regress runs keep the deterministic single frame
// unless the real-time benchmark explicitly asks for the loop.
bool interp_active(void);

// Presents the current level gameplay frame.  Replaces the original
// delayUntilElapsed() + playfield-copy + JE_showVGA() sequence in
// JE_starShowVGA() and runs the interpolated presentation loop when active.
void interp_present_gameplay(void);

// --regress-interp-alpha=A: present the frame interpolated at alpha A (0..1)
// instead of the live framebuffer.  -1 disables.  Used with --regress-snapshot
// to capture mid-tick frames headless.
void interp_set_regress_alpha(double alpha);
bool interp_regress_alpha_active(void);

// Present the current gameplay frame from the live framebuffer, without the
// interpolation machinery (the original path).  Also used by the real-time
// benchmark's fallback.
void interp_present_live_frame(void);

// Real-time pacing benchmark (--regress-realtime): after `seconds` of presented
// frames, logs average/percentile presented fps and logic tick rate, then exits.
void interp_bench_start(double seconds);

#endif // INTERP_H
