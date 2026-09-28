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
 * g = gcd(in,out), fi = in/g, fo = out/g, write n = q*fo + p, so
 * t = (q*fo + p) * fi/fo = q*fi + (p*fi mod fo)/fo + (p*fi div fo).  The
 * integer part (p*fi div fo) belongs to the window base, and the fractional
 * part is rem_p / fo with rem_p = (p*fi) mod fo (0 <= rem_p < fo).  Tap i
 * covers input base + i - (HALF-1) and uses h(frac_p - (i - (HALF-1))), with
 * frac_p = rem_p/fo.  (Using p*fi/fo directly as the fraction, as an earlier
 * version did, is only correct when fi == 1; for downsampling it walks the
 * window away from the signal and the output collapses.)  The taps for a
 * phase are computed once, normalised so they sum to exactly Q (DC gain 1),
 * and then applied with 64-bit integer accumulation, round-to-nearest and
 * clamping.
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

/* Build the `fo` phase filters (`taps` coefficients each) from the generated
   kernel.  Shared by the one-shot and the streaming converter so both use the
   exact same filter.  fc_num/fc_den stretch the kernel when downsampling. */
static void build_coeff(int64_t *coeff, uint32_t fi, uint32_t fo,
                        int64_t fc_num, int64_t fc_den)
{
	const int taps = 2 * RESAMPLER_KERNEL_HALF;
	const int offset = RESAMPLER_KERNEL_HALF - 1;

	for (uint32_t p = 0; p < fo; ++p)
	{
		int64_t *c = &coeff[(size_t)p * (size_t)taps];
		int64_t sum = 0;

		/* Fractional part of the input position for this phase,
		   in units of 1/fo of an input sample. */
		const int64_t rem = (int64_t)(((uint64_t)p * fi) % fo);

		for (int i = 0; i < taps; ++i)
		{
			/* u * fo, with u = rem/fo - (i - offset). */
			const int64_t U = rem - (int64_t)(i - offset) * fo;
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

	build_coeff(coeff, fi, fo, fc_num, fc_den);

	for (uint64_t n = 0; n < out_frames; ++n)
	{
		const uint64_t nfi = n * (uint64_t)fi;         /* t in units of 1/fo */
		const uint32_t p = (uint32_t)(n % fo);          /* polyphase index */
		const int64_t base = (int64_t)(nfi / fo);       /* floor(t) */
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
			acc += c[i] * ((int64_t)(int8_t)src[k0 + i] * 256);

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

/* --- streaming converter ------------------------------------------------- */

/* Ring capacity in input frames.  The OPL glue pushes a few hundred frames
   between pulls and a pull consumes everything it can, so the live history
   stays near the 96-tap window.  8192 is comfortably above any backlog the
   caller can create (and must be a power of two: it is used as a mask). */
#define RESAMPLER_STREAM_RING 8192

struct ResamplerStream
{
	int taps, offset;
	uint32_t fi, fo;
	int64_t *coeff;     /* fo phase filters, taps coefficients each */

	int16_t *ring;
	int64_t first;      /* absolute input index stored at ring[head] */
	size_t head;        /* ring slot holding `first` */
	size_t avail;       /* input frames held (including the leading zeros) */
	uint64_t next_out;  /* index of the next output frame to produce */
};

ResamplerStream *resampler_stream_init(int in_rate, int out_rate)
{
	if (in_rate <= 0 || out_rate <= 0)
		return NULL;

	ResamplerStream *stream = malloc(sizeof *stream);
	if (stream == NULL)
		return NULL;

	const uint32_t g = gcd_u32((uint32_t)in_rate, (uint32_t)out_rate);
	stream->fi = (uint32_t)in_rate / g;   /* input advance per group */
	stream->fo = (uint32_t)out_rate / g;  /* number of phases */
	stream->taps = 2 * RESAMPLER_KERNEL_HALF;
	stream->offset = RESAMPLER_KERNEL_HALF - 1;

	stream->coeff = malloc((size_t)stream->fo * (size_t)stream->taps * sizeof *stream->coeff);
	stream->ring = malloc(RESAMPLER_STREAM_RING * sizeof *stream->ring);
	if (stream->coeff == NULL || stream->ring == NULL)
	{
		free(stream->coeff);
		free(stream->ring);
		free(stream);
		return NULL;
	}

	const int64_t fc_num = (out_rate < in_rate) ? (int64_t)out_rate : 1;
	const int64_t fc_den = (out_rate < in_rate) ? (int64_t)in_rate : 1;
	build_coeff(stream->coeff, stream->fi, stream->fo, fc_num, fc_den);

	resampler_stream_reset(stream);
	return stream;
}

void resampler_stream_free(ResamplerStream *stream)
{
	if (stream == NULL)
		return;

	free(stream->coeff);
	free(stream->ring);
	free(stream);
}

void resampler_stream_reset(ResamplerStream *stream)
{
	/* Prime with `offset` zeros so the first output sees the same missing
	   history the one-shot converter treats as zero. */
	for (int i = 0; i < stream->offset; ++i)
		stream->ring[i] = 0;

	stream->head = 0;
	stream->first = -(int64_t)stream->offset;
	stream->avail = (size_t)stream->offset;
	stream->next_out = 0;
}

void resampler_stream_push(ResamplerStream *stream, const int16_t *frames, size_t count)
{
	/* The caller must not push more than the free ring space; the OPL glue
	   keeps the backlog far below it. */
	const size_t mask = RESAMPLER_STREAM_RING - 1;
	size_t slot = (stream->head + stream->avail) & mask;

	for (size_t i = 0; i < count; ++i)
	{
		stream->ring[slot] = frames[i];
		slot = (slot + 1) & mask;
	}
	stream->avail += count;
}

size_t resampler_stream_pull(ResamplerStream *stream, int16_t *frames, size_t max_frames)
{
	const size_t mask = RESAMPLER_STREAM_RING - 1;
	size_t produced = 0;

	while (produced < max_frames)
	{
		const uint64_t n = stream->next_out;
		const uint64_t nfi = n * (uint64_t)stream->fi;
		const int64_t base = (int64_t)(nfi / stream->fo);
		const int64_t k0 = base - stream->offset;
		const int64_t drop = k0 - stream->first;

		/* `drop` is never negative: the history is trimmed to the window
		   the next output needs.  The second test means "not enough input
		   buffered yet", and the caller pushes more. */
		if (drop < 0 || (uint64_t)drop + (uint64_t)stream->taps > stream->avail)
			break;

		stream->head = (stream->head + (size_t)drop) & mask;
		stream->first = k0;
		stream->avail -= (size_t)drop;

		const int64_t *c = &stream->coeff[(size_t)(n % stream->fo) * (size_t)stream->taps];

		int64_t acc = 0;
		size_t slot = stream->head;
		for (int i = 0; i < stream->taps; ++i)
		{
			acc += c[i] * (int64_t)stream->ring[slot];
			slot = (slot + 1) & mask;
		}

		int64_t out = div_round(acc, Q);
		if (out > 32767)
			out = 32767;
		else if (out < -32768)
			out = -32768;

		frames[produced++] = (int16_t)out;
		stream->next_out = n + 1;
	}

	return produced;
}
