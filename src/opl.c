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
#include "opl.h"

#include "nuked_opl3.h"
#include "resampler.h"

#include <stdint.h>

/*
 * OPL FM music backend.
 *
 * Tyrian's .mus files drive the AdLib/OPL2 chip.  This used to be a
 * DOSBox-derived OPL2 core; it is now the reference Nuked-OPL3 emulator
 * (src/nuked_opl3.c), kept in OPL2-compatible mode (the OPL3 "NEW" register,
 * 0x105, is never written, so the chip stays in OPL2 mode).  Nuked is
 * integer-only and has its own deterministic noise LFSR, so the rendered
 * music is identical on every platform and compiler.
 *
 * The chip runs at its native rate, 14318180 / 288 = 49716 Hz, and the
 * streaming band-limited resampler (src/resampler.c) converts that to the
 * mixer's output rate.  Nuked's own OPL3_GenerateResampled is deliberately NOT
 * used: it is linear interpolation, which leaves strong images above the
 * output Nyquist.
 *
 * The game is mono.  In OPL2 mode both output pairs of the chip carry the same
 * mono signal (pins 2/3 are silent), so pin 0 is taken as the mono sample;
 * no stereo is synthesized.
 */

#define OPL_NATIVE_CLOCK   14318180u
#define OPL_NATIVE_DIVISOR 288u
#define OPL_NATIVE_RATE    (OPL_NATIVE_CLOCK / OPL_NATIVE_DIVISOR)  /* 49716 Hz */

/* Native frames rendered per refill of the streaming resampler. */
#define OPL_NATIVE_BLOCK 512

static opl3_chip opl_chip;
static ResamplerStream *opl_resampler = NULL;
static int opl_output_rate = 0;
static int16_t opl_native[OPL_NATIVE_BLOCK];

void adlib_init(Bit32u samplerate)
{
	OPL3_Reset(&opl_chip, OPL_NATIVE_RATE);

	if (opl_resampler == NULL || opl_output_rate != (int)samplerate)
	{
		resampler_stream_free(opl_resampler);
		opl_resampler = resampler_stream_init(OPL_NATIVE_RATE, (int)samplerate);
		opl_output_rate = (int)samplerate;
	}
	else
	{
		/* Same rate: just restart the stream (new song).  No allocation. */
		resampler_stream_reset(opl_resampler);
	}
}

void adlib_write(Bitu idx, Bit8u val)
{
	/* Tyrian writes OPL2 registers only (addresses 0x01..0xf5).  Buffered
	   writes model the chip's register write FIFO, as the reference does. */
	OPL3_WriteRegBuffered(&opl_chip, (uint16_t)(idx & 0xffu), val);
}

void adlib_getsample(Bit16s *sndptr, Bits numsamples)
{
	if (opl_resampler == NULL)
	{
		for (Bits i = 0; i < numsamples; ++i)
			sndptr[i] = 0;
		return;
	}

	Bits done = 0;
	while (done < numsamples)
	{
		done += (Bits)resampler_stream_pull(opl_resampler, sndptr + done,
		                                    (size_t)(numsamples - done));

		if (done >= numsamples)
			break;

		/* Not enough chip output buffered yet: render one more block at the
		   native rate.  In OPL2 mode pin 0 is the mono signal. */
		for (int i = 0; i < OPL_NATIVE_BLOCK; ++i)
		{
			int16_t pair[2];
			OPL3_Generate(&opl_chip, pair);
			opl_native[i] = pair[0];
		}

		resampler_stream_push(opl_resampler, opl_native, OPL_NATIVE_BLOCK);
	}
}
