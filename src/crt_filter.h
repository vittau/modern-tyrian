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
#ifndef CRT_FILTER_H
#define CRT_FILTER_H

#include <stdbool.h>
#include <stdint.h>

#define CRT_FILTER_OFF 0
#define CRT_FILTER_SCANLINES 1
#define CRT_FILTER_NTSC 2
#define CRT_FILTER_BOTH 3
#define CRT_FILTER_MODE_COUNT 4

extern const char *const crt_filter_names[CRT_FILTER_MODE_COUNT];

// Selects a presentation filter; invalid modes select Off.
void crt_filter_set_mode(int mode);

// Returns the selected presentation filter.
int crt_filter_mode(void);

// Parses the canonical config names; unknown names select Off and return false.
bool crt_filter_set_by_name(const char *name);

// Returns the output width; NTSC expands three source columns into seven.
int crt_filter_output_width(int src_w);

// Returns 2*src_h for whole multiples of that height, dst_h otherwise, or src_h
// when dst_h is too short for half rows. Modes without scanlines keep src_h.
int crt_filter_output_height(int src_h, int dst_h);

// Filters read-only XRGB8888 source pixels into dst, with pitches in pixels.
// Advances NTSC burst once per call; call once per presented frame. Returns
// false on allocation failure so the presenter can use its original canvas.
bool crt_filter_render(const uint32_t *src, int src_pitch, int src_w,
                       uint32_t *dst, int dst_pitch, int src_h, int out_h);

// Resets the NTSC burst phase to zero for deterministic regression runs.
void crt_filter_reset_phase(void);

// Releases the lazy NTSC table and RGB555 scratch at video shutdown.
void crt_filter_quit(void);

// Returns the last lazy table build duration in milliseconds, for diagnostics.
double crt_filter_table_build_ms(void);

#endif /* CRT_FILTER_H */
