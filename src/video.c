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
#include "video.h"

#include "drawlist.h"
#include "keyboard.h"
#include "logging.h"
#include "modern.h"
#include "opentyr.h"
#include "regress.h"
#include "video_scale.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const char *const scaling_mode_names[ScalingMode_MAX] = {
	"Center",
	"Integer",
	"Fit 8:5",
	"Fit 4:3",
};

int fullscreen_display;
ScalingMode scaling_mode = SCALE_INTEGER;
static SDL_Rect last_output_rect = { 0, 0, vga_width, vga_height };
// Geometry of the last presented output, for mouse mapping.  Classic always
// presents the 320x200 8-bit frame (canvas == frame, offset 0); Modern can
// present a wider canvas with the game frame centered in it.
static int last_output_canvas_w = vga_width;
static int last_output_canvas_h = vga_height;
static int last_output_frame_x = 0;
static int last_output_frame_y = 0;
// Piecewise mapping for the widened pic-1 layout (see
// video_set_last_output_rect_split); false unless that call was the last one.
static bool last_output_split = false;
static int last_output_split_l = 0, last_output_split_r = 0;
static int last_output_insert_l = 0, last_output_insert_r = 0;

SDL_Surface *VGAScreen, *VGAScreenSeg;
SDL_Surface *VGAScreen2;
SDL_Surface *game_screen;

SDL_Window *main_window = NULL;
static SDL_Renderer *main_window_renderer = NULL;
const SDL_PixelFormatDetails *main_window_tex_format = NULL;
static SDL_Texture *main_window_texture = NULL;

static ScalerFunction scaler_function;

static void init_renderer(void);
static void deinit_renderer(void);
static void init_texture(void);
static void deinit_texture(void);

static SDL_DisplayID window_get_display(void);
static void window_center_in_display(SDL_DisplayID display_id);
static void calc_dst_render_rect(SDL_Surface *src_surface, SDL_Rect *dst_rect);
static void scale_and_flip(SDL_Surface *);

void init_video(void)
{
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
	{
		logFatal("Failed to initialize SDL video: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// Create the software surfaces that the game renders to. These are all 320x200x8 regardless
	// of the window size or monitor resolution.
	VGAScreen = VGAScreenSeg = SDL_CreateSurface(vga_width, vga_height, SDL_PIXELFORMAT_INDEX8);
	VGAScreen2 = SDL_CreateSurface(vga_width, vga_height, SDL_PIXELFORMAT_INDEX8);
	game_screen = SDL_CreateSurface(vga_width, vga_height, SDL_PIXELFORMAT_INDEX8);

	// The game code writes to surface->pixels directly without locking, so make sure that we
	// indeed created software surfaces that support this.
	assert(!SDL_MUSTLOCK(VGAScreen));
	assert(!SDL_MUSTLOCK(VGAScreen2));
	assert(!SDL_MUSTLOCK(game_screen));

	JE_clr256(VGAScreen);

	// Create the window with a temporary initial size, hidden until we set up the
	// scaler and find the true window size
	main_window = SDL_CreateWindow("OpenTyrian",
		vga_width, vga_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);

	if (main_window == NULL)
	{
		logFatal("Failed to create window: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (regress_active())
		logInfo("Regression: SDL video driver is '%s'.", SDL_GetCurrentVideoDriver());

	reinit_fullscreen(fullscreen_display);
	init_renderer();
	init_texture();
	init_scaler(scaler);
	modern_init();

	SDL_ShowWindow(main_window);

	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
	SDL_RenderClear(main_window_renderer);
	SDL_RenderPresent(main_window_renderer);
}

void deinit_video(void)
{
	modern_deinit();
	deinit_texture();
	deinit_renderer();

	SDL_DestroyWindow(main_window);

	SDL_DestroySurface(VGAScreenSeg);
	SDL_DestroySurface(VGAScreen2);
	SDL_DestroySurface(game_screen);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void init_renderer(void)
{
	main_window_renderer = SDL_CreateRenderer(main_window, NULL);

	if (main_window_renderer == NULL)
	{
		logFatal("Failed to create renderer: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}
}

static void deinit_renderer(void)
{
	if (main_window_renderer != NULL)
	{
		SDL_DestroyRenderer(main_window_renderer);
		main_window_renderer = NULL;
	}
}

static void init_texture(void)
{
	assert(main_window_renderer != NULL);

	SDL_PixelFormat format = SDL_PIXELFORMAT_XRGB8888;
	int scaler_w = scalers[scaler].width;
	int scaler_h = scalers[scaler].height;

	main_window_tex_format = SDL_GetPixelFormatDetails(format);

	main_window_texture = SDL_CreateTexture(main_window_renderer, format, SDL_TEXTUREACCESS_STREAMING, scaler_w, scaler_h);

	if (main_window_texture == NULL)
	{
		logFatal("Failed to create scaler texture (%dx%dx%s): %s", scaler_w, scaler_h, SDL_GetPixelFormatName(format), SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// SDL2 defaulted to nearest-neighbour sampling; SDL3 defaults to linear.
	// The pixel-art presentation must stay nearest-neighbour.
	SDL_SetTextureScaleMode(main_window_texture, SDL_SCALEMODE_NEAREST);
}

static void deinit_texture(void)
{
	if (main_window_texture != NULL)
	{
		SDL_DestroyTexture(main_window_texture);
		main_window_texture = NULL;
	}

	main_window_tex_format = NULL;
}

static SDL_DisplayID window_get_display(void)
{
	return SDL_GetDisplayForWindow(main_window);
}

static void window_center_in_display(SDL_DisplayID display_id)
{
	int win_w, win_h;
	SDL_GetWindowSize(main_window, &win_w, &win_h);

	SDL_Rect bounds;
	SDL_GetDisplayBounds(display_id, &bounds);

	SDL_SetWindowPosition(main_window, bounds.x + (bounds.w - win_w) / 2, bounds.y + (bounds.h - win_h) / 2);
}

// The windowed size for the current mode.  Classic uses the configured
// software scaler's output size.  Modern ignores the scalers and instead opens
// a window shaped like the chosen on-screen aspect at the largest integer
// multiple of the 200 logical rows that fits in ~80% of the usable desktop;
// the repo stores no window size, so this is also Modern's startup default.
static void windowed_size_for_mode(int *out_w, int *out_h)
{
	if (presentation == PRESENTATION_MODERN)
	{
		SDL_Rect usable;
		if (SDL_GetDisplayUsableBounds(window_get_display(), &usable) && usable.w > 0 && usable.h > 0)
		{
			// "auto" follows the display's own aspect; a fixed setting uses its
			// ratio.  The content aspect is independent of the pixel aspect.
			const float aspect = modern_aspect == MODERN_ASPECT_AUTO
				? (float)usable.w / (float)usable.h
				: modern_aspect_ratio();

			const float fill = 0.8f;
			const float max_w = (float)usable.w * fill;
			const float max_h = (float)usable.h * fill;

			int scale = (int)floorf(max_h / (float)vga_height);
			const int width_scale = (int)floorf(max_w / (aspect * (float)vga_height));
			if (width_scale < scale)
				scale = width_scale;
			if (scale < 1)
				scale = 1;

			*out_w = (int)lroundf(aspect * (float)vga_height * (float)scale);
			*out_h = vga_height * scale;
			return;
		}
	}

	*out_w = scalers[scaler].width;
	*out_h = scalers[scaler].height;
}

static void set_windowed_size_for_mode(void)
{
	int w, h;
	windowed_size_for_mode(&w, &h);
	SDL_SetWindowSize(main_window, w, h);
	window_center_in_display(window_get_display());
}

void video_apply_display_settings(void)
{
	// Called when the presentation/aspect/pixel-aspect settings change at
	// runtime.  Refit the windowed window to the new mode, then let the Modern
	// canvas follow the new window and geometry.  Classically this is a no-op
	// beyond re-centering at the scaler size.
	if (fullscreen_display == -1)
		set_windowed_size_for_mode();

	modern_update_canvas_size();
}

void reinit_fullscreen(int new_display)
{
	int display_count = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&display_count);

	fullscreen_display = new_display;

	if (displays == NULL)
	{
		fullscreen_display = -1;
	}
	else if (fullscreen_display >= display_count)
	{
		fullscreen_display = 0;
	}

	SDL_SetWindowFullscreen(main_window, false);
	{
		int w, h;
		windowed_size_for_mode(&w, &h);
		SDL_SetWindowSize(main_window, w, h);
	}

	if (fullscreen_display == -1)
	{
		window_center_in_display(window_get_display());
	}
	else
	{
		window_center_in_display(displays[fullscreen_display]);

		// SDL3 has no SDL_WINDOW_FULLSCREEN_DESKTOP flag: leaving the window's
		// fullscreen mode unset (NULL) selects the borderless desktop mode.
		if (!SDL_SetWindowFullscreen(main_window, true))
		{
			SDL_free(displays);
			reinit_fullscreen(-1);
			return;
		}
	}

	SDL_free(displays);

	// The window size just changed; the Modern canvas width follows it.
	modern_update_canvas_size();
}

void video_on_win_resize(void)
{
	int w, h;
	int scaler_w, scaler_h;

	// Tell video to reinit if the window was manually resized by the user.
	// Also enforce a minimum size on the window.

	SDL_GetWindowSize(main_window, &w, &h);
	scaler_w = scalers[scaler].width;
	scaler_h = scalers[scaler].height;

	if (w < scaler_w || h < scaler_h)
	{
		w = w < scaler_w ? scaler_w : w;
		h = h < scaler_h ? scaler_h : h;

		SDL_SetWindowSize(main_window, w, h);
	}

	// "auto" derives the Modern canvas width from the window size.
	modern_update_canvas_size();
}

void toggle_fullscreen(void)
{
	if (fullscreen_display != -1)
	{
		reinit_fullscreen(-1);
	}
	else
	{
		// Go fullscreen on the display the window is currently on.  The config
		// stores an index into the display list, so map the window's display ID
		// back to its index.
		int display_count = 0;
		SDL_DisplayID *displays = SDL_GetDisplays(&display_count);
		SDL_DisplayID current = window_get_display();

		int index = 0;
		for (int i = 0; displays != NULL && i < display_count; ++i)
		{
			if (displays[i] == current)
			{
				index = i;
				break;
			}
		}

		SDL_free(displays);

		reinit_fullscreen(index);
	}
}

bool init_scaler(unsigned int new_scaler)
{
	int w = scalers[new_scaler].width,
	    h = scalers[new_scaler].height;
	int bpp = main_window_tex_format->bits_per_pixel;

	scaler = new_scaler;

	deinit_texture();
	init_texture();

	if (fullscreen_display == -1)
	{
		// Changing scalers, when not in fullscreen mode, forces the window
		// to resize to exactly match the scaler's output dimensions.  Modern
		// ignores the software scalers, so its window keeps the Modern size.
		if (presentation != PRESENTATION_MODERN)
		{
			SDL_SetWindowSize(main_window, w, h);
			window_center_in_display(window_get_display());
		}
	}

	switch (bpp)
	{
	case 32:
		scaler_function = scalers[scaler].scaler32;
		break;
	case 16:
		scaler_function = scalers[scaler].scaler16;
		break;
	default:
		scaler_function = NULL;
		break;
	}

	if (scaler_function == NULL)
	{
		assert(false);
		return false;
	}

	// Changing the scaler windowed resizes the window; the Modern canvas width
	// follows the window size.
	modern_update_canvas_size();

	return true;
}

bool set_scaling_mode_by_name(const char *name)
{
	for (int i = 0; i < ScalingMode_MAX; ++i)
	{
		 if (strcmp(name, scaling_mode_names[i]) == 0)
		 {
			 scaling_mode = i;
			 return true;
		 }
	}
	return false;
}

void JE_clr256(SDL_Surface *screen)
{
	drawlist_record_fill_full(screen);
	SDL_FillSurfaceRect(screen, NULL, 0);

	// A cleared presented screen no longer shows the tracked backdrop; the
	// compositor falls back to the flat-edge / blurred fill until the next
	// JE_loadPic.  Temp buffers (ship specs) do not affect it.
	if (screen == VGAScreen)
		modern_backdrop_clear();
}

void JE_showVGA(void) 
{ 
	scale_and_flip(VGAScreen); 
}

SDL_Renderer *video_renderer(void)
{
	return main_window_renderer;
}

void video_set_last_output_rect(const SDL_Rect *rect)
{
	last_output_rect = *rect;
	last_output_canvas_w = vga_width;
	last_output_canvas_h = vga_height;
	last_output_frame_x = 0;
	last_output_frame_y = 0;
	last_output_split = false;
}

void video_set_last_output_rect_ex(const SDL_Rect *rect, int canvas_w, int canvas_h, int frame_x, int frame_y)
{
	last_output_rect = *rect;
	last_output_canvas_w = canvas_w;
	last_output_canvas_h = canvas_h;
	last_output_frame_x = frame_x;
	last_output_frame_y = frame_y;
	last_output_split = false;
}

void video_set_last_output_rect_split(const SDL_Rect *rect, int canvas_w, int canvas_h,
                                      int split_l, int split_r, int insert_l, int insert_r)
{
	last_output_rect = *rect;
	last_output_canvas_w = canvas_w;
	last_output_canvas_h = canvas_h;
	last_output_frame_x = 0;
	last_output_frame_y = 0;
	last_output_split = true;
	last_output_split_l = split_l;
	last_output_split_r = split_r;
	last_output_insert_l = insert_l;
	last_output_insert_r = insert_r;
}

// Canvas x -> game x for the widened layout: the frame is at the canvas left
// edge and the extra columns are the bands [split_l, split_l+insert_l) and
// [split_r+insert_l, split_r+insert_l+insert_r).  A point inside a band maps to
// its split column.
static Sint32 split_canvas_to_game_x(Sint32 cx)
{
	if (cx < last_output_split_l)
		return cx;

	if (cx < last_output_split_l + last_output_insert_l)
		return last_output_split_l;

	cx -= last_output_insert_l;

	if (cx < last_output_split_r)
		return cx;

	if (cx < last_output_split_r + last_output_insert_r)
		return last_output_split_r;

	return cx - last_output_insert_r;
}

// Game x -> canvas x, the inverse of split_canvas_to_game_x (game columns at or
// right of a split move with the columns inserted before them).
static Sint32 split_game_to_canvas_x(Sint32 gx)
{
	if (gx < last_output_split_l)
		return gx;

	if (gx < last_output_split_r)
		return gx + last_output_insert_l;

	return gx + last_output_insert_l + last_output_insert_r;
}

static void calc_dst_render_rect(SDL_Surface *const src_surface, SDL_Rect *const dst_rect)
{
	video_calc_dst_render_rect(src_surface->w, src_surface->h, main_window_texture, dst_rect);
}

void video_calc_dst_render_rect(int src_w, int src_h, SDL_Texture *texture, SDL_Rect *const dst_rect)
{
	// Decides how the logical output texture (after software scaling applied) will fit
	// in the window.  `src_w` x `src_h` is the logical surface size that integer
	// scaling counts in multiples of (the 320x200 game frame, or the modern canvas).

	int win_w, win_h;
	SDL_GetWindowSize(main_window, &win_w, &win_h);

	int maxh_width, maxw_height;

	switch (scaling_mode)
	{
	case SCALE_CENTER:
	{
		float tex_w, tex_h;
		SDL_GetTextureSize(texture, &tex_w, &tex_h);
		dst_rect->w = (int)tex_w;
		dst_rect->h = (int)tex_h;
		break;
	}
	case SCALE_INTEGER:
		dst_rect->w = src_w;
		dst_rect->h = src_h;
		while (dst_rect->w + src_w <= win_w && dst_rect->h + src_h <= win_h)
		{
			dst_rect->w += src_w;
			dst_rect->h += src_h;
		}
		break;
	case SCALE_ASPECT_8_5:
		maxh_width = win_h * (8.f / 5.f);
		maxw_height = win_w * (5.f / 8.f);

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = maxw_height;
		}
		else
		{
			dst_rect->w = maxh_width;
			dst_rect->h = win_h;
		}
		break;
	case SCALE_ASPECT_4_3:
		maxh_width = win_h * (4.f / 3.f);
		maxw_height = win_w * (3.f / 4.f);

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = maxw_height;
		}
		else
		{
			dst_rect->w = maxh_width;
			dst_rect->h = win_h;
		}
		break;
	case ScalingMode_MAX:
		assert(false);
		break;
	}

	dst_rect->x = (win_w - dst_rect->w) / 2;
	dst_rect->y = (win_h - dst_rect->h) / 2;
}

static void scale_and_flip(SDL_Surface *src_surface)
{
	assert(SDL_BITSPERPIXEL(src_surface->format) == 8);

	if (presentation == PRESENTATION_MODERN)
	{
		// CPU-composited canvas at the logical resolution: convert, run the
		// effect passes, then upload and present.  The software scalers are
		// Classic-only and are ignored here.
		modern_build_frame(src_surface);

		if (regress_active())
			regress_capture_modern_frame();

		modern_present_frame();
		return;
	}

	if (regress_active())
		regress_capture_frame(src_surface);

	// Do software scaling
	assert(scaler_function != NULL);
	scaler_function(src_surface, main_window_texture);

	SDL_Rect dst_rect;
	calc_dst_render_rect(src_surface, &dst_rect);

	// Clear the window and blit the output texture to it
	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
	SDL_RenderClear(main_window_renderer);
	const SDL_FRect dst_frect = { (float)dst_rect.x, (float)dst_rect.y, (float)dst_rect.w, (float)dst_rect.h };
	SDL_RenderTexture(main_window_renderer, main_window_texture, NULL, &dst_frect);
	SDL_RenderPresent(main_window_renderer);

	// Save output rect to be used by mouse functions
	last_output_rect = dst_rect;
}

/** Maps a specified point in game screen coordinates to window coordinates. */
void mapScreenPointToWindow(Sint32 *const inout_x, Sint32 *const inout_y)
{
	// The game frame is `last_output_frame_x/y` pixels into the presented
	// canvas, or its columns are the source of the widened canvas's pieces.
	Sint32 cx = last_output_split ? split_game_to_canvas_x(*inout_x)
	                              : *inout_x + last_output_frame_x;

	*inout_x = (2 * cx + 1) * last_output_rect.w / (2 * last_output_canvas_w) + last_output_rect.x;
	*inout_y = (2 * (*inout_y + last_output_frame_y) + 1) * last_output_rect.h / (2 * last_output_canvas_h) + last_output_rect.y;
}

/** Maps a specified point in window coordinates to game screen coordinates. */
void mapWindowPointToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	Sint32 cx = (2 * (*inout_x - last_output_rect.x) + 1) * last_output_canvas_w / (2 * last_output_rect.w);

	*inout_x = last_output_split ? split_canvas_to_game_x(cx) : cx - last_output_frame_x;
	*inout_y = (2 * (*inout_y - last_output_rect.y) + 1) * last_output_canvas_h / (2 * last_output_rect.h) - last_output_frame_y;
}

/** Scales a distance in window coordinates to game screen coordinates. */
void scaleWindowDistanceToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * *inout_x + 1) * last_output_canvas_w / (2 * last_output_rect.w);
	*inout_y = (2 * *inout_y + 1) * last_output_canvas_h / (2 * last_output_rect.h);
}
