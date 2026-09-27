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
#include "episodes.h"
#include "joystick.h"
#include "logging.h"
#include "loudness.h"
#include "modern.h"
#include "mtrand.h"
#include "opentyr.h"
#include "palette.h"
#include "player.h"
#include "shots.h"
#include "tyrian2.h"
#include "varz.h"
#include "video.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// beginPlayDemo() reseeds with this constant; scenario mode matches it so the
// two modes share the same deterministic RNG stream shape.
static const unsigned long scenario_seed = 32402394;

int regress_demo = 0;
int regress_scenario_episode = 0;
int regress_scenario_level = 0;
int regress_frames = 0;
const char *regress_out_path = NULL;
int regress_detail = 2;
int regress_audio = 0;
int regress_modern = 0;
int regress_aspect = -1;
const char *regress_state_out_path = NULL;

// 64-bit FNV-1a.
static const Uint64 fnv_offset_basis = UINT64_C(14695981039346656037);
static const Uint64 fnv_prime        = UINT64_C(1099511628211);

// Virtual frame-pacing clock, UQ22.10 milliseconds.
static Uint32 regress_clock = 0;

static FILE *regress_out = NULL;
static FILE *regress_state_out = NULL;
static unsigned long regress_frame = 0;

bool regress_active(void)
{
	return regress_demo != 0 || regress_scenario_active();
}

bool regress_scenario_active(void)
{
	return regress_scenario_episode != 0;
}

bool regress_audio_active(void)
{
	return regress_audio != 0;
}

static bool arg_is_option(const char *arg, const char *option, size_t option_len)
{
	return strncmp(arg, option, option_len) == 0 &&
	       (arg[option_len] == '\0' || arg[option_len] == '=');
}

bool regress_scan_args(int argc, char *argv[])
{
	static const char *const demo_option     = "--regress-demo";
	static const char *const scenario_option = "--regress-level";
	static const char *const audio_option    = "--regress-audio";

	for (int i = 1; i < argc; ++i)
	{
		if (arg_is_option(argv[i], demo_option, strlen(demo_option)) ||
		    arg_is_option(argv[i], scenario_option, strlen(scenario_option)) ||
		    arg_is_option(argv[i], audio_option, strlen(audio_option)))
			return true;
	}

	return false;
}

Uint32 regress_clock_ticks10bit(void)
{
	if (!regress_active())
		return (Uint32)SDL_GetTicks() << 10;

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

Uint64 regress_fnv1a(const void *data, size_t size)
{
	Uint64 hash = fnv_offset_basis;

	hash_bytes(&hash, data, size);

	return hash;
}

FILE *regress_output_file(void)
{
	return regress_out;
}

// Hash of the whole gameplay state, presentation-independent.  Deliberately
// skips the pointer fields of the enemy structs (their addresses depend on the
// process layout) and hashes everything else field by field so no struct
// padding can leak in.
static Uint64 regress_state_hash(void)
{
	Uint64 hash = fnv_offset_basis;

	const unsigned long long rng = mt_rand_state_hash();
	hash_bytes(&hash, (const Uint8 *)&rng, sizeof rng);

	for (int i = 0; i < 2; ++i)
	{
		const Player *p = &player[i];

		hash_bytes(&hash, (const Uint8 *)&p->cash, sizeof p->cash);
		hash_bytes(&hash, (const Uint8 *)&p->items, sizeof p->items);
		hash_bytes(&hash, (const Uint8 *)&p->last_items, sizeof p->last_items);
		hash_bytes(&hash, (const Uint8 *)&p->is_dragonwing, sizeof p->is_dragonwing);
		hash_bytes(&hash, (const Uint8 *)&p->shield_max, sizeof p->shield_max);
		hash_bytes(&hash, (const Uint8 *)&p->initial_armor, sizeof p->initial_armor);
		hash_bytes(&hash, (const Uint8 *)&p->shot_hit_area_x, sizeof p->shot_hit_area_x);
		hash_bytes(&hash, (const Uint8 *)&p->shot_hit_area_y, sizeof p->shot_hit_area_y);
		hash_bytes(&hash, (const Uint8 *)&p->is_alive, sizeof p->is_alive);
		hash_bytes(&hash, (const Uint8 *)&p->invulnerable_ticks, sizeof p->invulnerable_ticks);
		hash_bytes(&hash, (const Uint8 *)&p->exploding_ticks, sizeof p->exploding_ticks);
		hash_bytes(&hash, (const Uint8 *)&p->shield, sizeof p->shield);
		hash_bytes(&hash, (const Uint8 *)&p->armor, sizeof p->armor);
		hash_bytes(&hash, (const Uint8 *)&p->weapon_mode, sizeof p->weapon_mode);
		hash_bytes(&hash, (const Uint8 *)&p->superbombs, sizeof p->superbombs);
		hash_bytes(&hash, (const Uint8 *)&p->purple_balls_needed, sizeof p->purple_balls_needed);
		hash_bytes(&hash, (const Uint8 *)&p->mouseX, sizeof p->mouseX);
		hash_bytes(&hash, (const Uint8 *)&p->mouseY, sizeof p->mouseY);
		hash_bytes(&hash, (const Uint8 *)&p->x, sizeof p->x);
		hash_bytes(&hash, (const Uint8 *)&p->y, sizeof p->y);
		hash_bytes(&hash, (const Uint8 *)p->old_x, sizeof p->old_x);
		hash_bytes(&hash, (const Uint8 *)p->old_y, sizeof p->old_y);
		hash_bytes(&hash, (const Uint8 *)&p->x_velocity, sizeof p->x_velocity);
		hash_bytes(&hash, (const Uint8 *)&p->y_velocity, sizeof p->y_velocity);
		hash_bytes(&hash, (const Uint8 *)&p->x_friction_ticks, sizeof p->x_friction_ticks);
		hash_bytes(&hash, (const Uint8 *)&p->y_friction_ticks, sizeof p->y_friction_ticks);
		hash_bytes(&hash, (const Uint8 *)&p->delta_x_shot_move, sizeof p->delta_x_shot_move);
		hash_bytes(&hash, (const Uint8 *)&p->delta_y_shot_move, sizeof p->delta_y_shot_move);
		hash_bytes(&hash, (const Uint8 *)&p->last_x_shot_move, sizeof p->last_x_shot_move);
		hash_bytes(&hash, (const Uint8 *)&p->last_y_shot_move, sizeof p->last_y_shot_move);
		hash_bytes(&hash, (const Uint8 *)&p->last_x_explosion_follow, sizeof p->last_x_explosion_follow);
		hash_bytes(&hash, (const Uint8 *)&p->last_y_explosion_follow, sizeof p->last_y_explosion_follow);
		hash_bytes(&hash, (const Uint8 *)p->sidekick, sizeof p->sidekick);

		const Uint8 lives = (p->lives != NULL) ? *p->lives : 0;
		hash_bytes(&hash, &lives, sizeof lives);
	}

	// Enemies: every field except the two pointers (sprite2s, enemydatofs),
	// whose addresses vary with the process layout.
	for (unsigned int i = 0; i < COUNTOF(enemy); ++i)
	{
		const Uint8 *base = (const Uint8 *)&enemy[i];
		hash_bytes(&hash, base, offsetof(struct JE_SingleEnemyType, sprite2s));
		hash_bytes(&hash, base + offsetof(struct JE_SingleEnemyType, exrev),
		                 offsetof(struct JE_SingleEnemyType, enemydatofs) - offsetof(struct JE_SingleEnemyType, exrev));
		hash_bytes(&hash, base + offsetof(struct JE_SingleEnemyType, edamaged),
		                 sizeof(struct JE_SingleEnemyType) - offsetof(struct JE_SingleEnemyType, edamaged));
	}
	hash_bytes(&hash, (const Uint8 *)enemyAvail, sizeof enemyAvail);
	hash_bytes(&hash, (const Uint8 *)enemyShot, sizeof enemyShot);
	hash_bytes(&hash, (const Uint8 *)enemyShotAvail, sizeof enemyShotAvail);
	hash_bytes(&hash, (const Uint8 *)playerShotData, sizeof playerShotData);
	hash_bytes(&hash, (const Uint8 *)shotAvail, sizeof shotAvail);
	hash_bytes(&hash, (const Uint8 *)boss_bar, sizeof boss_bar);
	hash_bytes(&hash, (const Uint8 *)&tempW, sizeof tempW);
	hash_bytes(&hash, (const Uint8 *)&eventLoc, sizeof eventLoc);
	hash_bytes(&hash, (const Uint8 *)&curLoc, sizeof curLoc);
	hash_bytes(&hash, (const Uint8 *)&levelTimer, sizeof levelTimer);
	hash_bytes(&hash, (const Uint8 *)&levelTimerCountdown, sizeof levelTimerCountdown);
	hash_bytes(&hash, (const Uint8 *)explosions, sizeof explosions);
	hash_bytes(&hash, (const Uint8 *)superpixels, sizeof superpixels);
	hash_bytes(&hash, (const Uint8 *)rep_explosions, sizeof rep_explosions);

	return hash;
}

// Writes the frame and/or state record(s) for the current frame and honors the
// scenario frame cap.  Either stream may be absent; at least one is open.
static void regress_emit_records(bool write_frame, Uint64 frame_hash)
{
	if (write_frame)
		fprintf(regress_out, "%lu %016" PRIx64 "\n", regress_frame, frame_hash);

	if (regress_state_out != NULL)
		fprintf(regress_state_out, "%lu %016" PRIx64 "\n", regress_frame, regress_state_hash());

	regress_frame++;

	if (regress_frames > 0 && regress_frame >= (unsigned long)regress_frames)
	{
		// Scenario length cap.  This is the last frame we want, so flush and
		// leave; there is no clean way to unwind the original in-level loop and
		// every frame we care about has already been captured.
		regress_finish();
		exit(EXIT_SUCCESS);
	}
}

void regress_capture_frame(SDL_Surface *surface)
{
	if ((regress_out == NULL && regress_state_out == NULL) || surface == NULL)
		return;

	const bool write_frame = regress_out != NULL;
	Uint64 hash = fnv_offset_basis;

	if (write_frame)
	{
		assert(SDL_BITSPERPIXEL(surface->format) == 8);

		// Hash only the visible 320 bytes of each row, honoring the pitch.
		const Uint8 *pixels = surface->pixels;
		const size_t row_size = MIN((size_t)surface->w, (size_t)vga_width);
		for (int y = 0; y < surface->h; ++y)
		{
			hash_bytes(&hash, pixels, row_size);
			pixels += surface->pitch;
		}

		// Hash the palette that is currently being presented.
		const SDL_Color *palette = get_active_palette();
		for (size_t i = 0; i < 256; ++i)
		{
			const Uint8 rgb[3] = { palette[i].r, palette[i].g, palette[i].b };
			hash_bytes(&hash, rgb, sizeof rgb);
		}
	}

	regress_emit_records(write_frame, hash);
}

void regress_capture_modern_frame(void)
{
	if (regress_out == NULL && regress_state_out == NULL)
		return;

	const ModernFrame *frame = modern_current_frame();
	if (frame == NULL || frame->pixels == NULL)
		return;

	const bool write_frame = regress_out != NULL;
	Uint64 hash = fnv_offset_basis;

	if (write_frame)
	{
		// Hash the visible XRGB bytes of each row, honoring the canvas pitch.
		// The palette is already applied to the canvas, so it is not hashed
		// separately.
		const Uint8 *pixels = (const Uint8 *)frame->pixels;
		const size_t row_size = (size_t)frame->w * sizeof(Uint32);
		for (int y = 0; y < frame->h; ++y)
		{
			hash_bytes(&hash, pixels, row_size);
			pixels += frame->pitch;
		}
	}

	regress_emit_records(write_frame, hash);
}

void regress_begin_scenario(void)
{
	assert(regress_scenario_active());

	// Mirror beginPlayDemo(): restart the RNG at its fixed seed and force the
	// difficulty, so scenario playback is as reproducible as demo playback.
	mt_srand(scenario_seed);
	difficultyLevel = DIFFICULTY_NORMAL;

	// Scenario mode intentionally cannot die before the smoothie event fires.
	// youAreCheating is the engine's own invincibility flag (the one the
	// F2+F3+F6 cheat toggles); it only suppresses player death, and only
	// scenario mode turns it on.  It is set here, not in regress_init(), because
	// main() resets it to false later; demo mode never reaches here.
	youAreCheating = true;

	// This selects tyrianN.lvl and loads the item tables (needed for ships[],
	// shields[] and the weapon fire code).
	JE_initEpisode(regress_scenario_episode);

	// Fixed level identity.  lvlFileNum is the 1-based index into tyrianN.lvl
	// (the same field the episode script's "L" line and the demo header set).
	// levelName/levelSong are arbitrary but must be constant.
	memset(levelName, 0, sizeof levelName);
	strncpy(levelName, "SCENARIO", sizeof levelName - 1);
	lvlFileNum = (JE_byte)regress_scenario_level;
	initial_episode_num = (JE_byte)regress_scenario_episode;
	levelSong = 1;

	// Fixed new-game loadout.  This keeps the frame hashes a function of the
	// engine and the level data only (no user config, no save file, no demo).
	player[0].items.ship = 1;                     // USP Talon
	player[0].items.generator = 2;                // Advanced MR-12
	player[0].items.shield = 4;                   // Gencore High Energy Shield
	player[0].items.weapon[FRONT_WEAPON].id = 1;  // Pulse Cannon
	player[0].items.weapon[FRONT_WEAPON].power = 1;
	player[0].items.weapon[REAR_WEAPON].id = 0;   // None
	player[0].items.weapon[REAR_WEAPON].power = 1;
	player[0].items.sidekick[LEFT_SIDEKICK] = 0;  // None
	player[0].items.sidekick[RIGHT_SIDEKICK] = 0;
	player[0].items.special = 0;                  // None
	player[0].items.sidekick_series = 0;
	player[0].items.sidekick_level = 0;
	player[0].items.super_arcade_mode = 0;
	player[0].last_items = player[0].items;
}

void regress_init(void)
{
	// Headless by default.  Respect a driver the caller set explicitly.
	if (SDL_getenv("SDL_VIDEO_DRIVER") == NULL)
		SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
	if (SDL_getenv("SDL_AUDIO_DRIVER") == NULL)
		SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);

	// Regress playback needs neither audio nor joystick input, except the
	// offline audio regression, which drives the mixer directly.
	audio_disabled = !regress_audio_active();
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

	// Pin the presentation mode so the harness hashes the right buffer, without
	// the user's config leaking in (loadConfiguration() is skipped in regress
	// mode).  The Modern geometry is pinned too: the historic modern-* baselines
	// are 4:3 + original, and --regress-aspect opts into a wider canvas.
	presentation = regress_modern ? PRESENTATION_MODERN : PRESENTATION_CLASSIC;
	modern_aspect = regress_aspect >= 0 ? (ModernAspect)regress_aspect : MODERN_ASPECT_4_3;
	modern_pixel_aspect = PIXEL_ASPECT_ORIGINAL;

	JE_initProcessorType();

	// Start the virtual clock at a fixed value; setFrameSpeed() re-anchors the
	// frame counters from it, so the absolute value is irrelevant.
	regress_clock = 0;
	regress_frame = 0;

	if (regress_out_path == NULL && regress_state_out_path == NULL)
	{
		logFatal("--regress-demo/--regress-level/--regress-audio require --regress-out=FILE or --regress-state-out=FILE.");
		exit(EXIT_FAILURE);
	}

	if (regress_audio_active() && regress_out_path == NULL)
	{
		// The audio baseline is the frame output stream; there is no state
		// stream for it.
		logFatal("--regress-audio requires --regress-out=FILE.");
		exit(EXIT_FAILURE);
	}

	if (regress_out_path != NULL)
	{
		regress_out = fopen(regress_out_path, "wb");
		if (regress_out == NULL)
		{
			logFatal("Failed to open regression output '%s'.", regress_out_path);
			exit(EXIT_FAILURE);
		}
	}

	if (regress_state_out_path != NULL)
	{
		regress_state_out = fopen(regress_state_out_path, "wb");
		if (regress_state_out == NULL)
		{
			logFatal("Failed to open regression state output '%s'.", regress_state_out_path);
			exit(EXIT_FAILURE);
		}
	}
}

void regress_finish(void)
{
	if (regress_out == NULL && regress_state_out == NULL)
		return;

	bool close_ok = true;

	if (regress_out != NULL)
	{
		if (fclose(regress_out) != 0)
		{
			logError("Failed to close regression output '%s'.", regress_out_path);
			close_ok = false;
		}
		regress_out = NULL;
	}

	if (regress_state_out != NULL)
	{
		if (fclose(regress_state_out) != 0)
		{
			logError("Failed to close regression state output '%s'.", regress_state_out_path);
			close_ok = false;
		}
		regress_state_out = NULL;
	}

	if (!close_ok)
		return;

	if (regress_audio_active())
		logInfo("Regression: wrote audio baseline to '%s'.", regress_out_path);
	else
	{
		if (regress_out_path != NULL)
			logInfo("Regression: wrote %lu frames to '%s'.", regress_frame, regress_out_path);
		if (regress_state_out_path != NULL)
			logInfo("Regression: wrote %lu state records to '%s'.", regress_frame, regress_state_out_path);
	}
}
