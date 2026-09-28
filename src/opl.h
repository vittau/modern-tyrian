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
#ifndef OPL_H
#define OPL_H

#include "loudness.h"

#include <stdint.h>

/*
 * AdLib/OPL2 music interface used by the LDS player (src/lds_play.c).
 *
 * The implementation (src/opl.c) drives the vendored Nuked-OPL3 emulator in
 * OPL2-compatible mode at the chip's native rate (49716 Hz) and converts its
 * output to the mixer's rate with the streaming band-limited resampler
 * (src/resampler.c).
 */

typedef uintptr_t	Bitu;
typedef intptr_t	Bits;
typedef uint32_t	Bit32u;
typedef int32_t		Bit32s;
typedef uint16_t	Bit16u;
typedef int16_t		Bit16s;
typedef uint8_t		Bit8u;
typedef int8_t		Bit8s;

// general functions
void adlib_init(Bit32u samplerate);
void adlib_write(Bitu idx, Bit8u val);
void adlib_getsample(Bit16s* sndptr, Bits numsamples);

#define opl_init() adlib_init(audioSampleRate)
#define opl_write(reg, val) adlib_write(reg, val)
#define opl_update(buf, num) adlib_getsample(buf, num)

#endif /* OPL_H */
