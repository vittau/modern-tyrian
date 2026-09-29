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

#include "logging.h"
#include "loudness.h"
#include "nortsong.h"
#include "opentyr.h"
#include "sndmast.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Offline audio regression (--regress-audio).
//
// The harness renders the game's own audio pipeline to a fixed output format
// without ever opening an SDL audio device:
//
//   * loadSndFile() converts tyrian.snd/voices.snd through the same in-tree
//     band-limited resampler the game uses (S8, 11025 Hz, mono ->
//     S16, REGRESS_AUDIO_SAMPLE_RATE, mono) and each converted sample
//     is hashed:              sfx <index> <length_bytes> <fnv1a64>
//   * every song in music.mus is started with load_song() and rendered for
//     REGRESS_AUDIO_MUSIC_SECONDS seconds by the same mixer the SDL callback
//     calls, hashed once per second:  music <song> <second> <fnv1a64>
//   * a fixed sequence of sound effects is played over one song at fixed
//     volumes, hashed once per second:  mix <second> <fnv1a64>
//
// init_audio() asks the device for 11025 * OUTPUT_QUALITY (OUTPUT_QUALITY = 4
// in loudness.c) = 44100 Hz, AUDIO_S16SYS, mono.  The harness pins that exact
// request instead of the rate a device might grant.  The OPL music is rendered
// by the integer-only Nuked-OPL3 emulator, so no RNG seeding is needed.

#define REGRESS_AUDIO_SAMPLE_RATE 44100  // 11025 * OUTPUT_QUALITY
#define REGRESS_AUDIO_MUSIC_SECONDS 10
#define REGRESS_AUDIO_MIX_SECONDS 4
#define REGRESS_AUDIO_MIX_SONG 0

// Fixed mixer volumes for the "mix" section (0..255, game range).
#define REGRESS_AUDIO_MUSIC_VOLUME 128
#define REGRESS_AUDIO_SAMPLE_VOLUME 255

static void write_sfx_baseline(void)
{
	FILE *const out = regress_output_file();

	for (size_t i = 0; i < gameSoundCount(); ++i)
	{
		const size_t length_bytes = soundSampleCount[i] * sizeof (Sint16);
		const Uint64 hash = regress_fnv1a(soundSamples[i], length_bytes);

		fprintf(out, "sfx %zu %zu %016" PRIx64 "\n", i, length_bytes, hash);
	}
}

static Sint16 *alloc_buffer(void)
{
	Sint16 *buffer = malloc(REGRESS_AUDIO_SAMPLE_RATE * sizeof (Sint16));
	if (buffer == NULL)
	{
		logFatal("Audio regression: out of memory.");
		exit(EXIT_FAILURE);
	}

	return buffer;
}

static void render_music(unsigned int song, Sint16 *buffer)
{
	audio_regress_play_song(song);
	audio_regress_set_volume(REGRESS_AUDIO_MUSIC_VOLUME, REGRESS_AUDIO_SAMPLE_VOLUME);

	for (int second = 0; second < REGRESS_AUDIO_MUSIC_SECONDS; ++second)
	{
		audio_regress_mix(buffer, REGRESS_AUDIO_SAMPLE_RATE);

		const Uint64 hash = regress_fnv1a(buffer, REGRESS_AUDIO_SAMPLE_RATE * sizeof (Sint16));
		fprintf(regress_output_file(), "music %u %d %016" PRIx64 "\n", song, second, hash);
	}
}

// One scheduled sound effect, at a fixed offset from the start of the mix.
typedef struct
{
	int ms;       // start offset in milliseconds
	size_t sfx;   // 0-based index into soundSamples[]
	Uint8 chan;   // mixer channel (0..CHANNEL_COUNT-1)
	Uint8 vol;    // mixer volume (0..CHANNEL_VOLUME_LEVELS-1)
} MixEvent;

// Fixed, documented sequence: a spread of weapons/explosions/UI sounds across
// several channels and volumes, so the mixing and volume-scaling paths (including
// channel replacement) are exercised.
static const MixEvent mix_events[] = {
	{    0, S_WEAPON_1     - 1, 0, 4 },
	{  300, S_EXPLOSION_4  - 1, 1, 7 },
	{  750, S_WEAPON_13    - 1, 2, 1 },
	{ 1250, S_SELECT       - 1, 3, 5 },
	{ 1800, S_WARNING      - 1, 0, 7 },
	{ 2400, S_CURSOR       - 1, 4, 2 },
	{ 3000, S_EXPLOSION_12 - 1, 5, 6 },
	{ 3500, S_HULL_HIT     - 1, 1, 3 },
};

static void render_mix(Sint16 *buffer)
{
	audio_regress_play_song(REGRESS_AUDIO_MIX_SONG);
	audio_regress_set_volume(REGRESS_AUDIO_MUSIC_VOLUME, REGRESS_AUDIO_SAMPLE_VOLUME);

	const size_t event_count = COUNTOF(mix_events);
	size_t next_event = 0;
	long position = 0;

	for (int second = 0; second < REGRESS_AUDIO_MIX_SECONDS; ++second)
	{
		int filled = 0;

		while (filled < REGRESS_AUDIO_SAMPLE_RATE)
		{
			// Fire every effect that starts at the current sample position.
			while (next_event < event_count &&
			       (long)mix_events[next_event].ms * REGRESS_AUDIO_SAMPLE_RATE / 1000 == position)
			{
				const MixEvent *const event = &mix_events[next_event];

				if (soundSamples[event->sfx] != NULL)
					audio_regress_play_sample(soundSamples[event->sfx], soundSampleCount[event->sfx],
					                          event->chan, event->vol);

				next_event++;
			}

			// Render up to the end of the second, but stop exactly on the next
			// scheduled effect so the sample position stays aligned.
			int chunk = REGRESS_AUDIO_SAMPLE_RATE - filled;
			if (next_event < event_count)
			{
				const long next = (long)mix_events[next_event].ms * REGRESS_AUDIO_SAMPLE_RATE / 1000;
				if (next > position && next - position < chunk)
					chunk = (int)(next - position);
			}

			audio_regress_mix(buffer + filled, chunk);

			filled += chunk;
			position += chunk;
		}

		const Uint64 hash = regress_fnv1a(buffer, REGRESS_AUDIO_SAMPLE_RATE * sizeof (Sint16));
		fprintf(regress_output_file(), "mix %d %016" PRIx64 "\n", second, hash);
	}
}

void regress_audio_run(void)
{
	assert(regress_audio_active());
	assert(regress_output_file() != NULL);

	audio_regress_init(REGRESS_AUDIO_SAMPLE_RATE);

	// Same conversion path, same output format and rate as the game.
	loadSndFile(false);

	write_sfx_baseline();

	Sint16 *const buffer = alloc_buffer();

	const unsigned int songs = audio_regress_song_count();
	for (unsigned int song = 0; song < songs; ++song)
		render_music(song, buffer);

	render_mix(buffer);

	free(buffer);

	logInfo("Regression: rendered %u songs.", songs);

	regress_finish();
}
