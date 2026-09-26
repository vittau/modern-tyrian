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
#include "regress.h"

#include "config.h"
#include "joystick.h"
#include "logging.h"
#include "loudness.h"
#include "opentyr.h"
#include "palette.h"
#include "video.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int regress_demo = 0;
const char *regress_out_path = NULL;
int regress_detail = 2;

// 64-bit FNV-1a.
static const Uint64 fnv_offset_basis = UINT64_C(14695981039346656037);
static const Uint64 fnv_prime        = UINT64_C(1099511628211);

// Virtual frame-pacing clock, UQ22.10 milliseconds.
static Uint32 regress_clock = 0;

static FILE *regress_out = NULL;
static unsigned long regress_frame = 0;

bool regress_active(void)
{
	return regress_demo != 0;
}

bool regress_scan_args(int argc, char *argv[])
{
	static const char *const option = "--regress-demo";
	const size_t option_len = strlen(option);

	for (int i = 1; i < argc; ++i)
	{
		if (strncmp(argv[i], option, option_len) == 0 &&
		    (argv[i][option_len] == '\0' || argv[i][option_len] == '='))
			return true;
	}

	return false;
}

Uint32 regress_clock_ticks10bit(void)
{
	if (!regress_active())
		return SDL_GetTicks() << 10;

	return regress_clock;
}

void regress_clock_advance_to(Uint32 target)
{
	if (target > regress_clock)
		regress_clock = target;
}

static void hash_bytes(Uint64 *hash, const Uint8 *data, size_t size)
{
	for (size_t i = 0; i < size; ++i)
	{
		*hash ^= data[i];
		*hash *= fnv_prime;
	}
}

void regress_capture_frame(SDL_Surface *surface)
{
	if (regress_out == NULL || surface == NULL)
		return;

	assert(surface->format->BitsPerPixel == 8);

	Uint64 hash = fnv_offset_basis;

	// Hash only the visible 320 bytes of each row, honoring the pitch.
	const Uint8 *pixels = surface->pixels;
	const size_t row_size = MIN((size_t)surface->w, (size_t)vga_width);
	for (int y = 0; y < surface->h; ++y)
	{
		hash_bytes(&hash, pixels, row_size);
		pixels += surface->pitch;
	}

	// Hash the palette that is currently being presented.
	const Palette *palette = get_active_palette();
	for (size_t i = 0; i < 256; ++i)
	{
		const Uint8 rgb[3] = { (*palette)[i].r, (*palette)[i].g, (*palette)[i].b };
		hash_bytes(&hash, rgb, sizeof rgb);
	}

	fprintf(regress_out, "%lu %016" PRIx64 "\n", regress_frame, hash);
	regress_frame++;
}

void regress_init(void)
{
	// Headless by default.  Respect a driver the caller set explicitly.
	if (SDL_getenv("SDL_VIDEODRIVER") == NULL)
		SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
	if (SDL_getenv("SDL_AUDIODRIVER") == NULL)
		SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);

	// Demo playback needs neither audio nor joystick input.
	audio_disabled = true;
	ignore_joystick = true;

	// Pin every setting that can change the 8-bit framebuffer or the gameplay.
	// These mirror the defaults the engine uses when no config file is present,
	// but are forced so the user's config and save files cannot leak in.
	// processorType is the swept detail level; JE_initProcessorType() derives
	// the detail-dependent flags (wild, superWild, smoothScroll,
	// explosionTransparent, filtrationAvail, background2, displayScore).
	gameSpeed           = 4;                       // Normal
	processorType       = regress_detail;          // 1..6, sweepable
	gammaCorrection     = 0;                       // no gamma remap
	difficultyLevel     = DIFFICULTY_NORMAL;       // beginPlayDemo sets this too
	initialDifficulty   = DIFFICULTY_WIMP;         // fresh-run value used by scripts
	fullscreen_display  = -1;                      // windowed; no display needed
	youAreCheating      = false;
	fastPlay            = 0;
	pentiumMode         = false;
	starActive          = true;
	filterActive        = true;

	JE_initProcessorType();

	// Start the virtual clock at a fixed value; setFrameSpeed() re-anchors the
	// frame counters from it, so the absolute value is irrelevant.
	regress_clock = 0;
	regress_frame = 0;

	if (regress_out_path == NULL)
	{
		logFatal("--regress-demo requires --regress-out=FILE.");
		exit(EXIT_FAILURE);
	}

	regress_out = fopen(regress_out_path, "wb");
	if (regress_out == NULL)
	{
		logFatal("Failed to open regression output '%s'.", regress_out_path);
		exit(EXIT_FAILURE);
	}
}

void regress_finish(void)
{
	if (regress_out == NULL)
		return;

	if (fclose(regress_out) != 0)
	{
		logError("Failed to close regression output '%s'.", regress_out_path);
		regress_out = NULL;
		return;
	}

	regress_out = NULL;

	logInfo("Regression: wrote %lu frames to '%s'.", regress_frame, regress_out_path);
}
