#ifndef RESAMPLER_H
#define RESAMPLER_H

#include <stddef.h>
#include <stdint.h>

/*
 * Convert `src_frames` signed 8-bit mono samples at `in_rate` Hz to signed
 * 16-bit mono at `out_rate` Hz with a band-limited polyphase windowed-sinc
 * filter.
 *
 * Returns a malloc'd buffer and its length in bytes, or NULL with *out_bytes
 * set to 0 when the input is empty or allocation fails.  The output is
 * identical on every platform and compiler: the filter table is generated
 * once (tools/gen_resampler_table.py) and the runtime is integer-only.
 */
int16_t *resampler_convert(const uint8_t *src, size_t src_frames,
                           int in_rate, int out_rate, size_t *out_bytes);

/*
 * Streaming form of the same converter, for a continuous source such as the
 * OPL chip.  The kernel/table are shared with resampler_convert(); the phase,
 * the input history and the next output index are kept across calls, so any
 * chunking of the input or the output produces the same samples.  Everything
 * is integer-only.  Initialised once; no allocation after init, so it is safe
 * to call from the audio thread.
 *
 * Usage: resampler_stream_push() input frames as the source produces them,
 * resampler_stream_pull() output frames on demand.  A pull consumes only the
 * input it uses and keeps the tail the next output still needs.
 */
typedef struct ResamplerStream ResamplerStream;

ResamplerStream *resampler_stream_init(int in_rate, int out_rate);
void resampler_stream_free(ResamplerStream *stream);
void resampler_stream_reset(ResamplerStream *stream);
void resampler_stream_push(ResamplerStream *stream, const int16_t *frames, size_t count);
size_t resampler_stream_pull(ResamplerStream *stream, int16_t *frames, size_t max_frames);

#endif
