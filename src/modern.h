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
#ifndef MODERN_H
#define MODERN_H

#include "palette.h"

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stddef.h>

// Presentation modes.
//
// Classic is today's path: the 8-bit 320x200 frame is run through a software
// scaler (src/video_scale.c) into an XRGB8888 streaming texture and presented.
// The scalers (2x, Scale2x, hq2x, ...) are Classic-only.
//
// Modern replaces the software scalers with a CPU-composited XRGB8888 canvas
// at the logical resolution.  The 8-bit frame is converted through the active
// palette, an ordered list of effect passes runs on the canvas, and the canvas
// is uploaded to its own streaming texture and scaled with nearest-neighbour
// using the same fit logic as Classic.  All modern effects stay on the logical
// pixel grid; none is drawn at screen resolution.
typedef enum
{
	PRESENTATION_CLASSIC = 0,
	PRESENTATION_MODERN,
	PRESENTATION_MAX
} Presentation;

extern const char *const presentation_names[PRESENTATION_MAX];
extern Presentation presentation;

// Parses "classic" or "modern".  Returns false (leaving the setting unchanged)
// for any other value.
bool set_presentation_by_name(const char *name);

// Modern-mode widescreen geometry.
//
// `aspect` is the on-screen aspect the original 320x200 frame is presented at;
// it decides how wide the canvas is (the side panels fill the rest).  "auto"
// follows the current window aspect.
//
// `pixel_aspect` is the shape of one presented logical pixel.  The original DOS
// output was 320x200 on a 4:3 CRT, i.e. each pixel was 1.2x taller than wide;
// "square" presents pixels 1:1 instead.
typedef enum
{
	MODERN_ASPECT_4_3 = 0,
	MODERN_ASPECT_16_10,
	MODERN_ASPECT_16_9,
	MODERN_ASPECT_21_9,
	MODERN_ASPECT_32_9,
	MODERN_ASPECT_AUTO,
	MODERN_ASPECT_MAX
} ModernAspect;

typedef enum
{
	PIXEL_ASPECT_ORIGINAL = 0,  // 1.2 (height/width), the CRT look
	PIXEL_ASPECT_SQUARE,
	PIXEL_ASPECT_MAX
} ModernPixelAspect;

extern const char *const modern_aspect_names[MODERN_ASPECT_MAX];
extern const char *const modern_pixel_aspect_names[PIXEL_ASPECT_MAX];

extern ModernAspect modern_aspect;
extern ModernPixelAspect modern_pixel_aspect;

// Parse "4:3", "16:10", "16:9", "21:9", "32:9", "auto"; "original", "square".
// Return false (leaving the setting unchanged) for any other value.
bool set_modern_aspect_by_name(const char *name);
bool set_modern_pixel_aspect_by_name(const char *name);

// The pixel aspect as a scale factor (1.2 for original, 1.0 for square).
float modern_pixel_aspect_factor(void);

// The Modern canvas plus read-only access to the frame it was built from.
//
// `pixels` is an XRGB8888 canvas of `w` x `h` logical pixels, `pitch` bytes
// per row.  `h` is always the 200 logical rows of the 8-bit frame; `w` is at
// least 320 and grows with the configured aspect (the frame is centered
// horizontally and the side panels fill the rest).  `src` and `src_pitch` point
// at the source 8-bit indices and `palette` at the active palette for the frame
// currently being built.  Both are read-only and only valid during a pass; the
// conversion is what fills `pixels`.
typedef struct
{
	int w, h;              // canvas size in logical pixels
	int pitch;             // bytes per canvas row
	Uint32 *pixels;        // owned XRGB8888 canvas, `h` rows of `pitch` bytes
	const Uint8 *src;      // source 8-bit indices (read-only)
	int src_pitch;         // bytes per source row
	const SDL_Color *palette; // active palette (read-only)
} ModernFrame;

// An effect pass over the canvas.
//
// Contract:
//   * Passes run in registration order once per presented frame, on the canvas
//     at the logical resolution (one canvas pixel per game pixel).  They must
//     not assume any screen size.
//   * Passes must be deterministic: the same ModernFrame contents must always
//     produce the same result.  They must never call mt_rand()/mt_rand_1()/
//     mt_rand_lt1() or any other RNG.
//   * Passes are display-only: they must never read or write game state, call
//     game logic, or have side effects outside `frame`.
//   * Passes may read `frame->src`/`frame->palette` (read-only) but must only
//     write `frame->pixels`.
typedef void (*ModernPassFunction)(ModernFrame *frame);

// Registers a pass at the end of the ordered list.  Registration happens at
// startup, before the first frame; the list is empty in this task.
void modern_register_pass(ModernPassFunction pass);
size_t modern_pass_count(void);

// Creates the canvas and its streaming texture.  Call after the renderer
// exists (init_video()).
void modern_init(void);

// Destroys the canvas and its texture.  Call before the renderer is destroyed
// (deinit_video()).
void modern_deinit(void);

// Resizes the canvas (and recreates its texture at the new size).  Allocates;
// not for the per-frame path.  `modern_init()` sizes it to vga_width x
// vga_height.
void modern_set_canvas_size(int w, int h);

// Recomputes the canvas width from the current aspect/pixel-aspect settings and
// the current window size, then resizes if needed.  Allocates only when the
// size actually changes; call it on window resize, fullscreen toggle and
// setting changes, never per frame.
void modern_update_canvas_size(void);

// Marks the next presented frame as a gameplay frame.  JE_starShowVGA() calls
// this immediately before JE_showVGA() so the side panels sample the playfield
// edges (columns 0 and 263) instead of the HUD sidebar.  The flag is consumed
// by modern_build_frame(); it does not affect gameplay.
void modern_mark_gameplay_frame(void);

// Converts `src_surface` (8-bit indexed) through the active palette into the
// canvas and runs the registered passes, in order.  No allocation.
void modern_build_frame(SDL_Surface *src_surface);

// Uploads the canvas to its streaming texture and presents it on the window,
// honoring the current scaling mode.  Updates the mouse mapping rectangle.
// No allocation.
void modern_present_frame(void);

// The canvas built by the last modern_build_frame(), for the regression
// harness.  Never NULL once modern_init() has run.
const ModernFrame *modern_current_frame(void);

#endif /* MODERN_H */
