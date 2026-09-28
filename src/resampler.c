#include "resampler.h"

#include "resampler_table.h"

#include <stdlib.h>

/*
 * In-tree sample-rate converter for the game's 8-bit mono sound effects.
 *
 * SDL_ConvertAudioSamples() runs through SDL's float resampler, whose filter
 * coefficients are built with the platform libm (SDL_sinf/SDL_sqrtf).  Those
 * differ by an ulp between Apple's and glibc's libm, which made the offline
 * audio baseline differ between macOS and Linux.  Doing the conversion here,
 * with a generated integer filter table and integer arithmetic, makes the
 * whole audio path identical everywhere.
 *
 * The filter is the polyphase form of the interpolation kernel
 *
 *     h(u) = A * sinc(A*u) * kaiser(u / HALF, BETA),   |u| <= HALF
 *
 * stored in resampler_table.h at 1/SUBPHASES input samples and Q20 scaled.
 * For output sample n the continuous input position is t = n * in/out; with
 * g = gcd(in,out), fi = in/g, fo = out/g, write n = q*fo + p, so t = q*fi +
 * frac_p with frac_p = p*fi/fo.  Tap i covers input q*fi + i - (HALF-1) and
 * uses h(frac_p - (i - (HALF-1))).  The taps for a phase are computed once,
 * normalised so they sum to exactly Q (DC gain 1), and then applied with
 * 64-bit integer accumulation, round-to-nearest and clamping.
 *
 * For upsampling the kernel is used as generated, with its cutoff below the
 * input Nyquist so the first image band is suppressed; for downsampling the
 * kernel is stretched to the output Nyquist and scaled to keep DC gain 1.
 */

#define Q RESAMPLER_KERNEL_SCALE

static uint32_t gcd_u32(uint32_t a, uint32_t b)
{
	while (b != 0)
	{
		uint32_t t = a % b;
		a = b;
		b = t;
	}
	return a;
}

/* Round num/den to nearest; den > 0. */
static int64_t div_round(int64_t num, int64_t den)
{
	if (num >= 0)
		return (num + den / 2) / den;
	return -((-num + den / 2) / den);
}

/* h(|v|) in Q, v = index_num / index_den input samples, linearly interpolated
   from the generated table. */
static int64_t kernel_at(int64_t index_num, int64_t index_den)
{
	const int last = RESAMPLER_KERNEL_HALF * RESAMPLER_KERNEL_SUBPHASES;
	const int64_t q = index_num / index_den;
	if (q >= last)
		return resampler_kernel[last];

	const int64_t r = index_num - q * index_den;
	const int64_t k0 = resampler_kernel[q];
	const int64_t k1 = resampler_kernel[q + 1];
	return k0 + div_round((k1 - k0) * r, index_den);
}

int16_t *resampler_convert(const uint8_t *src, size_t src_frames,
                           int in_rate, int out_rate, size_t *out_bytes)
{
	*out_bytes = 0;
	if (in_rate <= 0 || out_rate <= 0 || src_frames == 0)
		return NULL;

	const uint64_t out_frames = (uint64_t)src_frames * (uint64_t)out_rate / (uint64_t)in_rate;
	if (out_frames == 0)
		return NULL;

	int16_t *dst = malloc((size_t)out_frames * sizeof *dst);
	if (dst == NULL)
		return NULL;

	const uint32_t g = gcd_u32((uint32_t)in_rate, (uint32_t)out_rate);
	const uint32_t fi = (uint32_t)in_rate / g;   /* input advance per group */
	const uint32_t fo = (uint32_t)out_rate / g;  /* number of phases */
	const int taps = 2 * RESAMPLER_KERNEL_HALF;
	const int offset = RESAMPLER_KERNEL_HALF - 1;

	/* Kernel stretch/scale: 1 for upsampling, out/in for downsampling. */
	const int64_t fc_num = (out_rate < in_rate) ? (int64_t)out_rate : 1;
	const int64_t fc_den = (out_rate < in_rate) ? (int64_t)in_rate : 1;

	int64_t *coeff = malloc((size_t)fo * (size_t)taps * sizeof *coeff);
	if (coeff == NULL)
	{
		free(dst);
		return NULL;
	}

	for (uint32_t p = 0; p < fo; ++p)
	{
		int64_t *c = &coeff[(size_t)p * (size_t)taps];
		int64_t sum = 0;

		for (int i = 0; i < taps; ++i)
		{
			/* u * fo, with u = frac_p - (i - offset). */
			const int64_t U = (int64_t)p * fi - (int64_t)(i - offset) * fo;
			const int64_t absU = (U < 0) ? -U : U;

			/* |v| * SUBPHASES, with v = fc * u. */
			const int64_t value = kernel_at(absU * RESAMPLER_KERNEL_SUBPHASES * fc_num,
			                                (int64_t)fo * fc_den);

			c[i] = value * fc_num / fc_den;
			sum += c[i];
		}

		if (sum != 0)
			for (int i = 0; i < taps; ++i)
				c[i] = div_round(c[i] * (int64_t)Q, sum);
	}

	for (uint64_t n = 0; n < out_frames; ++n)
	{
		const uint64_t q = n / fo;
		const uint32_t p = (uint32_t)(n % fo);
		const int64_t base = (int64_t)q * fi;
		const int64_t *c = &coeff[(size_t)p * (size_t)taps];

		/* Clip the tap range to the input, so the inner loop needs no
		   per-tap bounds test. */
		const int64_t k0 = base - offset;
		int lo = 0, hi = taps;
		if (k0 < 0)
			lo = (int)-k0;
		if (k0 + hi > (int64_t)src_frames)
			hi = (int)((int64_t)src_frames - k0);

		int64_t acc = 0;
		for (int i = lo; i < hi; ++i)
			acc += c[i] * ((int64_t)(int8_t)src[k0 + i] << 8);

		int64_t out = div_round(acc, Q);
		if (out > 32767)
			out = 32767;
		else if (out < -32768)
			out = -32768;

		dst[n] = (int16_t)out;
	}

	free(coeff);
	*out_bytes = (size_t)out_frames * sizeof *dst;
	return dst;
}
