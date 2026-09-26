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
#ifndef REGRESS_H
#define REGRESS_H

#include "SDL.h"

#include <stdbool.h>

// Headless demo-playback / synthetic-scenario regression harness.
//
// When --regress-demo=N is given, the game replays recorded demo N without a
// window or audio, pins every setting that can affect the 8-bit framebuffer or
// gameplay, and appends a hash of each presented frame to --regress-out=FILE.
//
// When --regress-level=EPISODE:LEVEL is given instead, the game starts that
// level directly (no demo recording, no input, fixed loadout and RNG seed),
// runs for --regress-frames frames, then exits.  This reaches render paths the
// recorded demos never exercise (the smoothie filters and the two
// JE_starShowVGA special codes).
//
// Outside regress mode every entry point below is inert.

extern int regress_demo;              // 0 = disabled, 1..5 = demo to replay
extern int regress_scenario_episode;  // 0 = disabled, 1..4 = episode to start
extern int regress_scenario_level;    // lvlFileNum within that episode (1-based)
extern int regress_frames;            // scenario length cap; 0 = run to the end
extern const char *regress_out_path;  // where frame hashes are written
extern int regress_detail;            // processorType to use (1..6)

// True when regress mode was requested.
bool regress_active(void);

// True when a synthetic level scenario was requested.
bool regress_scenario_active(void);

// Cheap argv scan for "--regress-demo"/"--regress-level" only, run before config
// loading so the user's config/save files can be skipped.  Does not set state.
bool regress_scan_args(int argc, char *argv[]);

// Set up headless drivers, disable audio/joystick input, pin the configuration
// values that affect output or gameplay, and open the output file.
void regress_init(void);

// Set up the fixed state (RNG seed, episode, loadout, level number) for a
// synthetic level scenario.  Called from JE_loadMap() in place of
// beginPlayDemo() when regress_scenario_active() is true.
void regress_begin_scenario(void);

// Flush and close the output file.
void regress_finish(void);

// Deterministic substitute for the frame-pacing clock.  In normal mode this is
// SDL_GetTicks() << 10; in regress mode it is a virtual UQ22.10 ms clock that
// only moves when the game asks to wait, so playback runs as fast as possible
// while staying independent of wall-clock time.
Uint32 regress_clock_ticks10bit(void);
void regress_clock_advance_to(Uint32 target);

// Append one "<frame_index> <hash>" line for a presented 320x200 8-bit surface.
void regress_capture_frame(SDL_Surface *surface);

#endif /* REGRESS_H */
