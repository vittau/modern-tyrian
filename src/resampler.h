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

#endif
