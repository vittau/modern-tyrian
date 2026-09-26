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

// Headless demo-playback regression harness.
//
// When --regress-demo=N is given, the game replays recorded demo N without a
// window or audio, pins every setting that can affect the 8-bit framebuffer or
// gameplay, and appends a hash of each presented frame to --regress-out=FILE.
// Outside regress mode every entry point below is inert.

extern int regress_demo;              // 0 = disabled, 1..5 = demo to replay
extern const char *regress_out_path;  // where frame hashes are written
extern int regress_detail;            // processorType to use (1..6)

// True when regress mode was requested.
bool regress_active(void);

// Cheap argv scan for "--regress-demo" only, run before config loading so the
// user's config/save files can be skipped.  Does not set any state.
bool regress_scan_args(int argc, char *argv[]);

// Set up headless drivers, disable audio/joystick input, pin the configuration
// values that affect output or gameplay, and open the output file.
void regress_init(void);

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
