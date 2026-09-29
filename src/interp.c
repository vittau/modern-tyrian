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
#include "interp.h"

#include "config.h"
#include "drawlist.h"
#include "keyboard.h"
#include "logging.h"
#include "nortsong.h"
#include "modern.h"
#include "modern_bloom.h"
#include "network.h"
#include "opentyr.h"
#include "palette.h"
#include "player.h"
#include "regress.h"
#include "tyrian2.h"
#include "varz.h"
#include "video.h"
#include "vfx.h"

#include <SDL3/SDL.h>

#include <stdlib.h>
#include <string.h>

// High-refresh presentation, stage 3.  See interp.h.

bool interp_smooth_motion = true;

// Whether we have touched the renderer's vsync and its current state, so the
// setting is only applied when it actually changes.
static bool interp_vsync_known = false;
static bool interp_vsync_on = false;

static double interp_regress_alpha = -1.0;

bool set_smooth_motion_by_name(const char *name)
{
	if (strcmp(name, "on") == 0 || strcmp(name, "true") == 0 || strcmp(name, "1") == 0)
	{
		interp_smooth_motion = true;
		return true;
	}
	if (strcmp(name, "off") == 0 || strcmp(name, "false") == 0 || strcmp(name, "0") == 0)
	{
		interp_smooth_motion = false;
		return true;
	}

	return false;
}

void interp_set_regress_alpha(double alpha)
{
	interp_regress_alpha = alpha;
}

bool interp_regress_alpha_active(void)
{
	return interp_regress_alpha >= 0.0;
}

bool interp_active(void)
{
	if (!interp_smooth_motion)
		return false;
	if (presentation != PRESENTATION_MODERN)
		return false;
	if (smoothScroll == 0)
		return false;
	if (playerEndLevel || skipStarShowVGA)
		return false;
	if (isNetworkGame)
		return false;
	if (regress_active() && !regress_realtime_active())
		return false;
	return true;
}

static void interp_ensure_vsync(bool on)
{
	if (interp_vsync_known && interp_vsync_on == on)
		return;

	SDL_Renderer *renderer = video_renderer();
	bool applied = false;
	if (renderer != NULL)
		applied = SDL_SetRenderVSync(renderer, on ? 1 : 0);

	interp_vsync_known = true;
	// If vsync could not be enabled the loop paces itself instead of spinning.
	interp_vsync_on = applied && on;
}

// The display refresh, in the UQ22.10 ms clock unit, used to pace when vsync is
// unavailable.  Never spins: the caller sleeps the remainder of the interval.
static Uint32 interp_frame_interval(void)
{
	int refresh = 0;

	SDL_DisplayID display = SDL_GetPrimaryDisplay();
	if (display != 0)
	{
		const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
		if (mode != NULL && mode->refresh_rate > 0.f)
			refresh = (int)(mode->refresh_rate + 0.5f);
	}

	if (refresh < 60)
		refresh = (refresh > 0) ? 60 : 120;

	return (Uint32)((1000ULL << 10) / (Uint32)refresh);
}

// Copies the 264x184 playfield out of `game` into VGAScreenSeg, applying the
// level's vertical-flip or player-spotlight special code.  This is the original
// presentation code moved out of JE_starShowVGA(); `px`/`py` are the player
// position that drives the spotlight (interpolated when appropriate).  The VFX
// are drawn into the presented playfield afterwards, interpolated at the same
// alpha, so the palette luminance they add reaches the (future) lighting pass.
static void interp_blit_playfield(SDL_Surface *game, int px, int py, Uint32 alpha_fx16,
                                  bool bar_interp)
{
	JE_byte *src;
	Uint8 *s = VGAScreenSeg->pixels;
	int x, y, lightx, lighty, lightdist;

	// Remember the level filter just applied to this playfield so the Modern
	// compositor can fade/tint the HUD panels and message strip with it.
	modern_capture_playfield_filter();

	src = game->pixels;
	src += 24;

	if (starShowVGASpecialCode == 1)
	{
		src += game->pitch * 183;
		for (y = 0; y < 184; y++)
		{
			memmove(s, src, 264);
			s += VGAScreenSeg->pitch;
			src -= game->pitch;
		}
	}
	else if (starShowVGASpecialCode == 2 && processorType >= 2)
	{
		lighty = 172 - py;
		lightx = 281 - px;

		for (y = 184; y; y--)
		{
			if (lighty > y)
			{
				for (x = 320 - 56; x; x--)
				{
					*s = (*src & 0xf0) | ((*src >> 2) & 0x03);
					s++;
					src++;
				}
			}
			else
			{
				for (x = 320 - 56; x; x--)
				{
					lightdist = abs(lightx - x) + lighty;
					if (lightdist < y)
						*s = *src;
					else if (lightdist - y <= 5)
						*s = (*src & 0xf0) | (((*src & 0x0f) + (3 * (5 - (lightdist - y)))) / 4);
					else
						*s = (*src & 0xf0) | ((*src & 0x0f) >> 2);
					s++;
					src++;
				}
			}
			s += 56 + VGAScreenSeg->pitch - 320;
			src += 56 + game->pitch - 320;
		}
	}
	else
	{
		for (y = 0; y < 184; y++)
		{
			memmove(s, src, 264);
			s += VGAScreenSeg->pitch;
			src += game->pitch;
		}
	}

	// Copy the emission tag of the same playfield window so the Modern
	// bloom/lighting pass only lights the tagged objects (shots, explosions,
	// pickups, superpixels).  The tag follows the same flip/spotlight mapping
	// as the pixels and, for an interpolated frame, the interpolated sprite
	// positions.
	int game_tag_pitch = 0;
	const Uint8 *game_tag = drawlist_tag_for_surface(game, &game_tag_pitch, NULL, NULL);
	modern_bloom_tag_begin();
	modern_bloom_tag_from_game(game_tag, game_tag != NULL ? game_tag_pitch : 0,
	                           starShowVGASpecialCode == 1);

	// Copy the matching object-light palette indices: the pass uses them to
	// light each object with its own colour rather than the white-hot core of
	// its brightest pixels.  Same flip/spotlight mapping as the tag.
	int game_lcol_pitch = 0;
	const Uint8 *game_lcol = drawlist_lightcol_for_surface(game, &game_lcol_pitch, NULL, NULL);
	modern_bloom_lightcol_from_game(game_lcol, game_lcol != NULL ? game_lcol_pitch : 0,
	                                starShowVGASpecialCode == 1);

	// Draw the interpolated VFX into the presented playfield (palette indices),
	// before the Modern conversion so the effects feed the lighting pass.
	vfx_render_playfield(VGAScreenSeg, alpha_fx16);

	// Let the compositor redraw the dynamic HUD bars at the same alpha.
	modern_set_bar_interp(bar_interp, alpha_fx16);
	modern_mark_gameplay_frame();
	JE_showVGA();
}

void interp_present_live_frame(void)
{
	interp_blit_playfield(game_screen, player[0].x, player[0].y, 65536u, false);
}

// --- palette fade interpolation (stage 4) ------------------------------------

static bool interp_fade_check = false;
static unsigned long interp_fade_frames_count = 0;
static unsigned long interp_fade_channels_count = 0;
static unsigned long interp_fade_bad_count = 0;

void interp_set_fade_check(bool check)
{
	interp_fade_check = check;
}

void interp_fade_reset(void)
{
	interp_fade_frames_count = 0;
	interp_fade_channels_count = 0;
	interp_fade_bad_count = 0;
}

unsigned long interp_fade_frames(void)   { return interp_fade_frames_count; }
unsigned long interp_fade_channels(void) { return interp_fade_channels_count; }
unsigned long interp_fade_bad(void)      { return interp_fade_bad_count; }

bool interp_fade_smooth_active(void)
{
	if (isNetworkGame)
		return false;
	if (presentation != PRESENTATION_MODERN)
		return false;
	if (!interp_smooth_motion)
		return false;
	// A regress run keeps the stepped palette unless a mid-fade capture (or the
	// effects check) explicitly asks for the interpolated one.
	if (regress_active() && !regress_realtime_active())
		return interp_regress_alpha_active();
	return true;
}

static Uint8 interp_lerp_channel(Uint8 a, Uint8 b, Uint32 alpha_fx16)
{
	const Sint32 d = (Sint32)b - (Sint32)a;
	return (Uint8)((Sint32)a + ((d * (Sint32)alpha_fx16) >> 16));
}

// Sets the active palette (and the cached conversion palette) to the blend of
// `before` and `after` at `alpha_fx16`.
static void interp_apply_palette_lerp(SDL_Color *before, SDL_Color *after,
                                      unsigned first, unsigned last, Uint32 alpha_fx16)
{
	Palette blend;

	for (unsigned i = first; i <= last; ++i)
	{
		blend[i].r = interp_lerp_channel(before[i].r, after[i].r, alpha_fx16);
		blend[i].g = interp_lerp_channel(before[i].g, after[i].g, alpha_fx16);
		blend[i].b = interp_lerp_channel(before[i].b, after[i].b, alpha_fx16);
		blend[i].a = 255;
	}

	set_palette(blend, first, last);
}

// Diagnostic: a channel whose endpoints differ by at least two must present a
// value strictly between them; a one-step channel must stay within the
// endpoints.  Only the checkbox path counts, so a normal run pays nothing.
static void interp_fade_note(SDL_Color *before, SDL_Color *after,
                             unsigned first, unsigned last, Uint32 alpha_fx16)
{
	if (!interp_fade_check || alpha_fx16 == 0 || alpha_fx16 >= 65536)
		return;

	interp_fade_frames_count++;

	for (unsigned i = first; i <= last; ++i)
	{
		const Uint8 a[3] = { before[i].r, before[i].g, before[i].b };
		const Uint8 b[3] = { after[i].r, after[i].g, after[i].b };

		for (int c = 0; c < 3; ++c)
		{
			if (a[c] == b[c])
				continue;

			interp_fade_channels_count++;

			const Uint8 v = interp_lerp_channel(a[c], b[c], alpha_fx16);
			const Uint8 lo = MIN(a[c], b[c]);
			const Uint8 hi = MAX(a[c], b[c]);

			bool ok;
			if ((unsigned int)(hi - lo) >= 2)
				ok = (v > lo && v < hi);
			else
				ok = (v >= lo && v <= hi);

			if (!ok)
				interp_fade_bad_count++;
		}
	}
}

void interp_present_palette_fade(SDL_Color *before, SDL_Color *after,
                                 unsigned first, unsigned last)
{
	// Regress: present exactly one deterministic frame at the requested alpha,
	// then let the caller's waitUntilElapsed() pace the step.
	if (regress_active() && !regress_realtime_active() && interp_regress_alpha_active())
	{
		const Uint32 alpha_fx16 = (Uint32)(interp_regress_alpha * 65536.0 + 0.5);
		if (alpha_fx16 >= 65536u)
		{
			// alpha = 1: present the realised tick exactly, as the plain path.
			JE_showVGA();
			return;
		}
		interp_fade_note(before, after, first, last, alpha_fx16);
		interp_apply_palette_lerp(before, after, first, last, alpha_fx16);
		JE_showVGA();
		set_palette(after, first, last);
		return;
	}

	// The fade loop called setFrameCount(1) for this step, so the tick is one
	// frame period long.
	const Uint32 deadline = getFrameDeadlineTicks10();
	Uint32 period = getFramePeriodTicks10();
	if (period == 0)
		period = 1;
	const Uint32 start = deadline - period;

	interp_ensure_vsync(true);
	const bool paced_soft = !interp_vsync_on;
	const Uint32 interval = interp_frame_interval();

	for (;;)
	{
		Uint32 now = regress_clock_ticks10bit();
		const Sint32 elapsed = (Sint32)(now - start);
		Uint32 alpha_fx16;
		if (elapsed <= 0)
			alpha_fx16 = 0;
		else
			alpha_fx16 = (Uint32)(((Uint64)elapsed << 16) / period);
		if (alpha_fx16 > 65536u)
			alpha_fx16 = 65536u;

		interp_fade_note(before, after, first, last, alpha_fx16);
		interp_apply_palette_lerp(before, after, first, last, alpha_fx16);
		JE_showVGA();
		handleSdlEvents();

		now = regress_clock_ticks10bit();
		if ((Sint32)(now - deadline) >= 0)
			break;

		if (paced_soft)
		{
			const Uint32 remain = deadline - now;
			if (remain > interval)
				SDL_Delay((interval >> 10) + 1);
			else
				SDL_Delay((remain >> 10) + 1);
		}
	}

	set_palette(after, first, last);
}

// Renders the interpolated frame at `alpha_fx16` and presents it.  Falls back to
// the live tick frame when no previous list is available.
static void interp_render_and_present(Uint32 alpha_fx16)
{
	if (drawlist_render_interpolated(alpha_fx16))
	{
		int px, py;
		drawlist_interpolated_player(&px, &py);
		interp_blit_playfield(drawlist_interpolated_game(), px, py, alpha_fx16, true);
	}
	else
	{
		interp_present_live_frame();
	}
}

// --- real-time pacing benchmark ----------------------------------------------

#define BENCH_BUCKETS 200  // 0.5 ms wide, up to 100 ms

static bool bench_enabled = false;
static double bench_seconds = 20.0;
static Uint64 bench_start_ns = 0, bench_first_ns = 0, bench_last_ns = 0;
static unsigned long bench_presented = 0, bench_ticks = 0;
static Uint64 bench_sum_ns = 0, bench_max_ns = 0;
static unsigned long bench_hist[BENCH_BUCKETS];
static bool bench_refresh_logged = false;

void interp_bench_start(double seconds)
{
	if (seconds > 0.0)
		bench_seconds = seconds;
	bench_enabled = true;
	bench_presented = 0;
	bench_ticks = 0;
	bench_sum_ns = 0;
	bench_max_ns = 0;
	memset(bench_hist, 0, sizeof bench_hist);
	bench_start_ns = SDL_GetTicksNS();
	bench_first_ns = 0;
	bench_last_ns = 0;
	bench_refresh_logged = false;

	logInfo("Bench: presenting for %.1f s; leave the window focused.", bench_seconds);
}

static void interp_bench_report(void)
{
	const Uint64 now = SDL_GetTicksNS();
	const double wall_s = (now - bench_start_ns) / 1e9;
	const double active_s = (bench_presented > 1) ? (bench_last_ns - bench_first_ns) / 1e9 : 0.0;
	const unsigned long frames = bench_presented > 1 ? bench_presented - 1 : 1;
	const double mean_ms = bench_sum_ns / 1e6 / (double)frames;

	unsigned long target = (frames > 0) ? (unsigned long)(0.99 * (double)frames) : 0;
	unsigned long cum = 0;
	unsigned p99 = 0;
	for (unsigned i = 0; i < BENCH_BUCKETS; ++i)
	{
		cum += bench_hist[i];
		if (target > 0 && cum >= target)
		{
			p99 = i;
			break;
		}
	}

	logInfo("Bench result: wall %.1f s, active %.1f s, %lu presented frames.",
	        wall_s, active_s, bench_presented);
	logInfo("Bench result: fps active avg %.1f, instantaneous avg %.1f, min %.1f (max frame %.2f ms).",
	        active_s > 0.0 ? frames / active_s : 0.0,
	        bench_sum_ns > 0 ? 1e9 / (double)(bench_sum_ns / frames) : 0.0,
	        bench_max_ns > 0 ? 1e9 / (double)bench_max_ns : 0.0,
	        bench_max_ns / 1e6);
	logInfo("Bench result: frame time mean %.2f ms, max %.2f ms, p99 %.2f ms; logic %.1f ticks/s.",
	        mean_ms, bench_max_ns / 1e6, p99 * 0.5,
	        wall_s > 0.0 ? bench_ticks / wall_s : 0.0);

	bench_enabled = false;
	regress_finish();
	exit(EXIT_SUCCESS);
}

static void interp_bench_sample(void)
{
	if (!bench_enabled)
		return;

	const Uint64 now = SDL_GetTicksNS();

	if (!bench_refresh_logged)
	{
		const Uint32 interval = interp_frame_interval();
		logInfo("Bench: pacing at %.1f fps (vsync %s).",
		        interval > 0 ? 1e6 / ((double)interval * 1000.0 / 1024.0) : 0.0,
		        interp_vsync_on ? "on" : "unavailable");
		bench_refresh_logged = true;
	}

	if (bench_presented == 0)
		bench_first_ns = now;

	if (bench_presented > 0)
	{
		const Uint64 dt = now - bench_last_ns;
		bench_sum_ns += dt;
		if (dt > bench_max_ns)
			bench_max_ns = dt;
		unsigned b = (unsigned)(dt / 500000ull);
		if (b >= BENCH_BUCKETS)
			b = BENCH_BUCKETS - 1;
		bench_hist[b]++;
	}
	bench_last_ns = now;
	bench_presented++;

	if (now - bench_start_ns >= (Uint64)(bench_seconds * 1e9))
		interp_bench_report();
}

void interp_present_gameplay(void)
{
	// Headless capture of a mid-tick frame (--regress-interp-alpha): render the
	// interpolated frame once and present it through the normal Modern path.
	if (interp_regress_alpha >= 0.0 && regress_active())
	{
		if (smoothScroll != 0)
		{
			delayUntilElapsed();
			setFrameCount(frameCountMax);
		}

		if (presentation == PRESENTATION_MODERN)
			interp_ensure_vsync(false);

		const Uint32 alpha_fx16 = (Uint32)(interp_regress_alpha * 65536.0 + 0.5);

		// Keep the persistent reference at the realised tick, then render the
		// requested in-between frame for presentation.
		(void)drawlist_render_interpolated(65536);

		if (drawlist_render_interpolated(alpha_fx16))
		{
			int px, py;
			drawlist_interpolated_player(&px, &py);
			interp_blit_playfield(drawlist_interpolated_game(), px, py, alpha_fx16, true);
		}
		else
		{
			interp_present_live_frame();
		}
		return;
	}

	if (!interp_active())
	{
		// The original single-frame path.  Modern gets vsync off so the tick's
		// own delay still paces it exactly as before this stage.
		if (presentation == PRESENTATION_MODERN)
			interp_ensure_vsync(false);

		if (smoothScroll != 0)
		{
			delayUntilElapsed();
			setFrameCount(frameCountMax);
		}

		interp_present_live_frame();
		return;
	}

	// Smooth path: present interpolated frames until the tick's deadline.
	const Uint32 deadline = getFrameDeadlineTicks10();
	Uint32 period = (Uint32)frameCountMax * getFramePeriodTicks10();
	if (period == 0)
		period = 1;
	const Uint32 start = deadline - period;

	interp_ensure_vsync(true);
	const bool paced_soft = !interp_vsync_on;
	const Uint32 interval = interp_frame_interval();

	bench_ticks++;

	for (;;)
	{
		Uint32 now = regress_clock_ticks10bit();
		Sint32 elapsed = (Sint32)(now - start);
		Uint32 alpha_fx16;
		if (elapsed <= 0)
			alpha_fx16 = 0;
		else
			alpha_fx16 = (Uint32)(((Uint64)elapsed << 16) / period);
		if (alpha_fx16 > 65536u)
			alpha_fx16 = 65536u;

		interp_render_and_present(alpha_fx16);
		interp_bench_sample();
		handleSdlEvents();

		now = regress_clock_ticks10bit();
		if ((Sint32)(now - deadline) >= 0)
			break;

		// Without vsync, pace to the display refresh instead of spinning.
		if (paced_soft)
		{
			const Uint32 remain = deadline - now;
			if (remain > interval)
				SDL_Delay((interval >> 10) + 1);
			else
				SDL_Delay((remain >> 10) + 1);
		}
	}

	// Leave the persistent reference (and scratch) at the realised tick frame
	// so the next tick, and any destination-reading filter, start from it.
	(void)drawlist_render_interpolated(65536);
	setFrameCount(frameCountMax);
}
