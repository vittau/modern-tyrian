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

// The configured on-screen aspect as a ratio.  For MODERN_ASPECT_AUTO this
// returns the 4:3 placeholder, since only the window knows the auto aspect;
// callers that care must special-case auto themselves.
float modern_aspect_ratio(void);

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

// Holds every presented frame to the gameplay composition until cleared.  The
// level intro (and its palette fade) and the end-of-level animation present
// through JE_showVGA directly rather than JE_starShowVGA, so they bracket their
// presentation block with this.  The pause and in-game menus are outside those
// blocks and stay non-gameplay.  Consumed by modern_build_frame().
void modern_set_gameplay_hold(bool hold);

// Marks the level's presentation period (level setup .. the next level load).
// Only the regression harness reads it; it does not select the composition.
void modern_set_in_level(bool in_level);
bool modern_in_level_period(void);

// What the last modern_build_frame() did, for the harness assertion:
// `gameplay` is true when the frame was requested as a gameplay frame, and
// `gameplay_panels` when it additionally used the playfield + side-panel
// composition (i.e. the classic sidebar was dropped).
bool modern_last_frame_gameplay(void);
bool modern_last_frame_gameplay_panels(void);

// Clears the relocated HUD panels and the message strip at a level start so the
// first level frames cannot show the previous level's HUD.  No allocation.
void modern_level_reset(void);

// --- Relocated in-game HUD (Phase 1b) ---------------------------------------
//
// In Modern mode, when both side panels are at least MODERN_HUD_MIN_PANEL_WIDTH
// logical pixels wide, the compositor copies only the 264x184 playfield out of
// the 320x200 frame and gives the freed side columns to the new HUD, so the
// original sidebar and bottom strip never appear next to it.  The in-game HUD
// that used to live inside the playfield (cash, lives and player name,
// superbombs, the special-weapon icon), plus the boss bars, the level timer and
// the cheat notice, is drawn into two off-screen 8-bit surfaces and composited
// over the ambilight side panels by modern_build_frame().  Index 0 in those
// surfaces means "keep the background".
//
// The panels are measured against the playfield layout (both centred in the
// canvas): panel width = (canvas_w - 264) / 2.  With the default `original`
// (1.2) pixel aspect 16:10 gives 60 px, 16:9 gives 81/82 px, 21:9 gives 148 px
// and 32:9 more; 4:3 gives 28 px and falls back.  The minimum is exactly the
// boss bar's width, so a panel that qualifies can hold every relocated element.
//
// When the panels are narrower (or in Classic mode) nothing changes: the full
// 320x200 frame is centred and the HUD stays inside it, exactly as before.
#define MODERN_PLAYFIELD_W 264
#define MODERN_PLAYFIELD_H 184
// The rows freed below the playfield, which carry the message strip.
#define MODERN_MESSAGE_H 16
#define MODERN_HUD_MIN_PANEL_WIDTH 51

// Room past the visible panel so text that runs long cannot wrap into the row
// below (the surfaces are always this much wider than the panel; only the
// first panel-width columns are composited).
#define MODERN_HUD_PANEL_PAD 80

// The layout of the relocated HUD and every element drawn into these surfaces
// lives in src/modern_hud.c/.h.

// True when the current Modern canvas has side panels wide enough for the
// relocated HUD.  False for Classic, Modern 4:3 and any narrow-panel geometry.
bool modern_hud_in_panels(void);

// Width in logical pixels of the narrower side panel (0 when there is none).
int modern_side_panel_width(void);

// The off-screen 8-bit surface for side panel `player` (0 = left/P1,
// 1 = right/P2), index 0 transparent, or NULL when modern_hud_in_panels() is
// false.  Owned by modern.c; the game draws into it with the usual 8-bit
// primitives.  modern_hud_begin_frame() must clear it once per gameplay frame.
SDL_Surface *modern_hud_surface(int player);

// Clears both HUD surfaces to index 0.  Call once per gameplay frame before the
// relocated HUD is drawn.  A no-op outside panel mode.  No allocation.
void modern_hud_begin_frame(void);

// The off-screen 8-bit surface for the message strip under the playfield (the
// 16 rows freed below the 264x184 playfield), index 0 transparent, or NULL
// outside panel mode.  The strip is MODERN_PLAYFIELD_W wide by 16 rows; it
// carries the level name and the in-game message.  Owned by modern.c; drawn by
// modern_hud.c and composited by modern_build_frame().  No allocation per frame.
SDL_Surface *modern_hud_message_surface(void);

// Stores/clears the current in-game message (the text JE_drawTextWindow draws).
// Display-only: the stored text is shown on the Modern message strip; the game's
// own drawing into the 8-bit frame is untouched.  `modern_message_set` copies
// the text into a fixed buffer, so it never allocates.
void modern_message_set(const char *text);
void modern_message_clear(void);
const char *modern_message_text(void);

// --- Non-gameplay backdrop composition (Phase 1, step S1) -------------------
//
// Every non-gameplay screen is a fixed 320x200 backdrop (a tyrian.pic picture)
// with code-drawn elements (text, lists, sprites, boxes) on top.  To extend
// those screens into the widescreen canvas without hand-drawn art, the
// compositor needs to tell backdrop from element.  `modern_backdrop_set()` is
// called (read-only) from JE_loadPic with the decoded picture; it keeps one
// pristine 320x200 8-bit copy of it.  A pixel of the presented frame whose
// index equals the pristine picture is backdrop; one that differs is an
// element.  `modern_backdrop_clear()` (JE_clr256) drops it.  The picture is
// only kept for ids the compositor knows how to extend (1, 2, 4, 5, 11); the
// match ratio is still checked per frame, so a screen that no longer resembles
// the picture falls back to the blurred fill.
#define MODERN_BACKDROP_W 320
#define MODERN_BACKDROP_H 200

void modern_backdrop_set(int pic_id, const Uint8 *pixels, int pitch);
void modern_backdrop_clear(void);

// Records where JE_mouseStart last blitted the mouse cursor (game coordinates;
// w/h <= 0 clears it).  The cursor is a code-drawn element that moves every
// frame, so the widening guard and the flat-edge test ignore its rectangle.
void modern_mouse_cursor_set(int x, int y, int w, int h);

// True when the last built frame used the widened pic-1 right-panel layout.
// When it did, the canvas-to-game x mapping is piecewise (the extra columns are
// inserted at a split column) and the mouse must go through the inserted band.
// The split is currently a single point, reported through `split_l`/`split_r`
// (equal) and `insert_l`/`insert_r` (insert_l == 0, insert_r == the extra width).
bool modern_frame_is_split(int *split_l, int *split_r, int *insert_l, int *insert_r);

// --- Canvas-wide procedural screens (Phase 1, step S2) ----------------------
//
// Some non-gameplay screens draw procedural content (starfields, grids) that can
// be extended across the whole Modern canvas instead of being cropped to the
// 320x200 frame.  These screens opt in by rendering their whole frame into a
// canvas-width 8-bit scratch (the same indexed palette) with the game's own
// drawing routines, then presenting as usual: modern_build_frame() sees the
// committed scratch and converts it 1:1.  Classic and Modern 4:3 never take this
// path, so the 8-bit screens and their baselines are unchanged, and no game/RNG
// state changes (the scratch is drawn from the same state the 320 frame already
// used).

// True when a screen should render into the canvas-wide scratch: Modern mode
// with a canvas wider than the 320x200 frame.
bool modern_screen_wide(void);

// Returns the canvas-wide 8-bit scratch (canvas_w x vga_height), cleared to
// index 0, or NULL when not wide (modern_screen_wide() is false).  The caller
// draws its whole frame into it with the usual 8-bit primitives; the next
// presented frame uses the scratch.  Allocates only on a canvas resize, never
// per frame.
SDL_Surface *modern_screen_begin(void);

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
