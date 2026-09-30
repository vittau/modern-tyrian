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
#ifndef VIDEO_H
#define VIDEO_H

#include <SDL3/SDL.h>

#include <stdbool.h>

#define vga_width 320
#define vga_height 200

typedef enum {
	SCALE_CENTER,
	SCALE_INTEGER,
	SCALE_FIT,
	ScalingMode_MAX
} ScalingMode;

extern const char *const scaling_mode_names[ScalingMode_MAX];

extern int fullscreen_display; // -1 means windowed
extern ScalingMode scaling_mode;

extern SDL_Surface *VGAScreen, *VGAScreenSeg;
extern SDL_Surface *game_screen;
extern SDL_Surface *VGAScreen2;

extern SDL_Window *main_window;
extern const SDL_PixelFormatDetails *main_window_tex_format;

void init_video(void);

void video_on_win_resize(void);
void reinit_fullscreen(int new_display);
void toggle_fullscreen(void);
bool set_scaling_mode_by_name(const char *name);

// Applies a runtime change to the presentation/aspect/pixel-aspect settings:
// refits the windowed window to the new mode's default size and recomputes the
// Modern canvas.  Display-only; safe to call from a menu.
void video_apply_display_settings(void);

// Launcher: shape the windowed window as a 16:9 screen (fullscreen is left as
// it is). Returns false if SDL cannot settle the resize before drawing.
// video_apply_display_settings() gives the game's own size back.
bool video_fit_launcher_window(void);

// Shared presentation helpers, used by both the Classic and Modern paths.
SDL_Renderer *video_renderer(void);
// Presents `texture` (src_w x src_h logical pixels) in the window and returns
// the destination rectangle, in the window's native pixels (also used for mouse
// mapping, which converts SDL's window points to pixels as needed).
// `content_aspect` is the on-screen width/height of the whole image: Classic
// passes the 4:3 or 8:5 frame selected by the pixel aspect, Modern passes the
// canvas display aspect.  `mode` is the fit strategy; Modern always passes
// SCALE_FIT.  Center and Integer draw with nearest-neighbour; Fit uses the
// sharp-bilinear path.
SDL_Rect video_fit_rect(float content_aspect);
SDL_Rect video_present_texture(SDL_Texture *texture, int src_w, int src_h, float content_aspect, ScalingMode mode);
// Records the presented output rectangle (native pixels) for mouse mapping.
void video_set_last_output_rect(const SDL_Rect *rect);
// Modern variant: also records the canvas size and the offset of the 320x200
// game frame inside the canvas, so window points still map to game coordinates
// when the canvas is wider than the frame.
void video_set_last_output_rect_ex(const SDL_Rect *rect, int canvas_w, int canvas_h, int frame_x, int frame_y);

// Modern variant for the widened pic-1 layout: the canvas starts with the game
// frame at x=0 and inserts `insert_l` extra columns at game column `split_l`
// and `insert_r` at `split_r`.  The mapping is then piecewise: a window point
// inside an inserted band maps to the split column, and game x continues across
// the bands.
void video_set_last_output_rect_split(const SDL_Rect *rect, int canvas_w, int canvas_h,
                                      int split_l, int split_r, int insert_l, int insert_r);

void deinit_video(void);

void JE_clr256(SDL_Surface *);
void JE_showVGA(void);

void mapScreenPointToWindow(Sint32 *inout_x, Sint32 *inout_y);
void mapWindowPointToScreen(Sint32 *inout_x, Sint32 *inout_y);
void scaleWindowDistanceToScreen(Sint32 *inout_x, Sint32 *inout_y);

#endif /* VIDEO_H */
