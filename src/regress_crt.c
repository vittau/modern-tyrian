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
/* Synthetic coverage uses explicit checks in release builds too. */
#include "regress.h"

#include "crt_filter.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRT_REQUIRE(condition) \
	do \
	{ \
		if (!(condition)) \
		{ \
			fprintf(stderr, "CRT fixture FAIL: line %d: %s\n", __LINE__, #condition); \
			free(source); \
			free(output); \
			crt_filter_quit(); \
			return 1; \
		} \
	} while (0)

int regress_crt_selfcheck(void)
{
	const int width = 427, height = 200;
	const int destinations[] = { 400, 600, 1200, 1080, 2160, 150, 1079, 2161 };
	const int heights[] = { 400, 600, 400, 1080, 2160, 200, 1079, 2161 };
	uint32_t *source = malloc((size_t)(width + 3) * height * 4);
	uint32_t *output = malloc((size_t)1001 * 2161 * 4);
	CRT_REQUIRE(source != NULL && output != NULL);
	for (int i = 0; i < (width + 3) * height; ++i)
		source[i] = 0x00808080;
	crt_filter_set_mode(CRT_FILTER_OFF);
	CRT_REQUIRE(crt_filter_output_width(width) == width);
	CRT_REQUIRE(crt_filter_output_height(height, 2160) == height);
	CRT_REQUIRE(crt_filter_render(source, width + 3, width, output, width + 5, height, height));
	for (int y = 0; y < height; ++y)
		CRT_REQUIRE(memcmp(source + y * (width + 3), output + y * (width + 5), (size_t)width * 4) == 0);
	puts("CRT fixture PASS: off identity, independent source/destination pitches");
	crt_filter_set_mode(CRT_FILTER_SCANLINES);
	for (unsigned i = 0; i < sizeof destinations / sizeof destinations[0]; ++i)
	{
		int out_h = crt_filter_output_height(height, destinations[i]);
		CRT_REQUIRE(out_h == heights[i]);
		CRT_REQUIRE(crt_filter_output_width(width) == width);
		CRT_REQUIRE(crt_filter_render(source, width + 3, width, output, width, height, out_h));
		for (int y = 0; y < out_h; ++y)
		{
			/* Independent interval integration in source-row coordinates. */
			double start = (double)y * height / out_h;
			double end = (double)(y + 1) * height / out_h;
			double overlap = fmax(0.0, fmin(end, floor(start) + 1.0) - fmax(start, floor(start) + 0.5));
			double coverage = out_h == height ? (double)(y & 1) : overlap / (end - start);
			int expected = (int)(128.0 * (1.0 - coverage) + 95.0 * coverage + 1e-8);
			int actual = (int)(output[y * width] & 255);
			CRT_REQUIRE(abs(actual - expected) <= 1);
			for (int x = 1; x < width; ++x)
				CRT_REQUIRE(output[y * width + x] == output[y * width]);
		}
		printf("CRT fixture PASS: dst=%d output=%dx%d half-row coverage and straddling rows\n", destinations[i], width, out_h);
	}
	for (int i = 0; i < (width + 3) * height; ++i)
		CRT_REQUIRE(source[i] == 0x00808080);
	source[0] = 0x00ffffff;
	CRT_REQUIRE(crt_filter_render(source, width + 3, width, output, width, height, 400));
	CRT_REQUIRE(output[0] == 0x00ffffff && output[width] == 0x00eaeaea);
	puts("CRT fixture PASS: white retains faint trace; source is read-only");
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
			source[y * (width + 3) + x] = ((uint32_t)(x * 47 & 255) << 16) | ((uint32_t)(y * 31 & 255) << 8) | (uint32_t)(x * 11 & 255);
	for (int mode = 0; mode < CRT_FILTER_MODE_COUNT; ++mode)
	{
		crt_filter_set_mode(mode);
		int out_w = crt_filter_output_width(width);
		CRT_REQUIRE(out_w == (mode >= CRT_FILTER_NTSC ? 1001 : width));
		for (int d = 1080; d <= 2160; d *= 2)
		{
			int out_h = crt_filter_output_height(height, d);
			Uint64 start = SDL_GetPerformanceCounter();
			CRT_REQUIRE(crt_filter_render(source, width + 3, width, output, out_w, height, out_h));
			double first_ms = (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency();
			start = SDL_GetPerformanceCounter();
			for (int n = 0; n < 60; ++n)
				CRT_REQUIRE(crt_filter_render(source, width + 3, width, output, out_w, height, out_h));
			double ms = (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency() / 60.0;
			printf("CRT measurement: mode=%s dst=%d output=%dx%d render=%.3f ms/frame first=%.3f ms\n", crt_filter_names[mode], d, out_w, out_h, ms, first_ms);
		}
	}
	printf("CRT measurement: lazy table build=%.3f ms, storage=16777216 bytes\n", crt_filter_table_build_ms());
	/* Burst phase restarts at zero and repeats after three presented frames. */
	crt_filter_set_mode(CRT_FILTER_NTSC);
	crt_filter_reset_phase();
	CRT_REQUIRE(crt_filter_render(source, width + 3, width, output, 1001, height, height));
	uint32_t sample = output[50 * 1001 + 90];
	for (int n = 0; n < 3; ++n)
		CRT_REQUIRE(crt_filter_render(source, width + 3, width, output, 1001, height, height));
	CRT_REQUIRE(output[50 * 1001 + 90] == sample);
	crt_filter_reset_phase();
	CRT_REQUIRE(crt_filter_render(source, width + 3, width, output, 1001, height, height));
	CRT_REQUIRE(output[50 * 1001 + 90] == sample);
	puts("CRT fixture PASS: NTSC three-phase cycle and deterministic reset, composite full range");
	for (int w = 1; w <= 7; ++w)
	{
		uint32_t tiny[7] = { 0 }, black[21], red[21];
		int out_w = crt_filter_output_width(w);
		crt_filter_reset_phase();
		CRT_REQUIRE(crt_filter_render(tiny, w, w, black, out_w, 1, 1));
		tiny[w - 1] = 0x00ff0000;
		crt_filter_reset_phase();
		CRT_REQUIRE(crt_filter_render(tiny, w, w, red, out_w, 1, 1));
		CRT_REQUIRE(memcmp(black, red, (size_t)out_w * 4) != 0);
	}
	puts("CRT fixture PASS: NTSC widths 1..7 retain partial final chunks");
	CRT_REQUIRE(!crt_filter_set_by_name("unknown") && crt_filter_mode() == CRT_FILTER_OFF);
	puts("CRT fixture PASS: canonical mode names, unknown config falls back off");
	free(source);
	free(output);
	crt_filter_quit();
	return 0;
}
