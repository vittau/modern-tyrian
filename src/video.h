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
	SCALE_ASPECT_8_5,
	SCALE_ASPECT_4_3,
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
bool init_scaler(unsigned int new_scaler);
bool set_scaling_mode_by_name(const char *name);

// Applies a runtime change to the presentation/aspect/pixel-aspect settings:
// refits the windowed window to the new mode's default size and recomputes the
// Modern canvas.  Display-only; safe to call from a menu.
void video_apply_display_settings(void);

// Shared presentation helpers, used by both the Classic and Modern paths.
SDL_Renderer *video_renderer(void);
// Computes the destination rectangle for a logical surface of src_w x src_h
// pixels uploaded to `texture`, honoring the current scaling mode.  Classic
// passes the 8-bit surface size and its software-scaled texture; Modern passes
// the canvas size and its canvas texture.
void video_calc_dst_render_rect(int src_w, int src_h, SDL_Texture *texture, SDL_Rect *dst_rect);
// Records the presented output rectangle for mouse mapping.
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
