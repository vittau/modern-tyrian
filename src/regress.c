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
#include "drawlist.h"
#include "episodes.h"
#include "interp.h"
#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "loudness.h"
#include "modern.h"
#include "modern_bloom.h"
#include "mtrand.h"
#include "opentyr.h"
#include "palette.h"
#include "player.h"
#include "shots.h"
#include "tyrian2.h"
#include "varz.h"
#include "vfx.h"
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

int regress_seed_set = 0;
unsigned long regress_seed = 0;
int regress_demo = 0;
int regress_scenario_episode = 0;
int regress_scenario_level = 0;
int regress_script = 0;
int regress_frames = 0;
const char *regress_out_path = NULL;
int regress_detail = 2;
int regress_audio = 0;
int regress_modern = 0;
int regress_aspect = -1;
int regress_vfx = -1;
const char *regress_state_out_path = NULL;
int regress_players = 1;
int regress_arcade = 0;
const char *regress_screen = NULL;
int regress_replay_check = 0;
int regress_interp_check = 0;
int regress_interp_smoothness = 0;
int regress_smooth_alphas = 5;
int regress_gameplay_check = 0;
int regress_parallax_check = 0;
int regress_realtime = 0;
double regress_bench_seconds = 20.0;
int regress_bloom_quality = -1;
int regress_lighting_quality = -1;

// Snapshot requests (--regress-snapshot=FRAME:FILE), repeatable.  Fixed size:
// a run needs only a handful and parsing must not allocate per frame.
#define REGRESS_MAX_SNAPSHOTS 16

typedef struct
{
	unsigned long frame;
	const char *path;
} RegressSnapshot;

static RegressSnapshot regress_snapshots[REGRESS_MAX_SNAPSHOTS];
static int regress_snapshot_count = 0;

// 64-bit FNV-1a.
static const Uint64 fnv_offset_basis = UINT64_C(14695981039346656037);
static const Uint64 fnv_prime        = UINT64_C(1099511628211);

// Virtual frame-pacing clock, UQ22.10 milliseconds.
static Uint32 regress_clock = 0;

static FILE *regress_out = NULL;
static FILE *regress_state_out = NULL;
static unsigned long regress_frame = 0;

// Smoothness diagnostics: log the presented frame of the first few ticks whose
// interpolation showed non-monotonic motion.
static unsigned long regress_smooth_seen_events = 0;
static unsigned regress_smooth_log_count = 0;

// Gameplay-composition assertion (--regress-gameplay-check): while a level is
// being presented in Modern, every frame must drop the classic sidebar, and
// every frame in a level brightness *fade ramp* must mirror that brightness on
// the HUD (otherwise the panels flash at full brightness over the darkened
// playfield).  A colour override or a static brightness offset is not mirrored.
static unsigned long regress_gameplay_frames = 0;
static unsigned long regress_gameplay_missing = 0;
static char regress_gameplay_first[128] = "";
static unsigned long regress_gameplay_filter_frames = 0;
static unsigned long regress_gameplay_filter_missing = 0;
static char regress_gameplay_filter_first[128] = "";

static void regress_check_gameplay_composition(void)
{
	if (!regress_gameplay_check || !modern_in_level_period() || !modern_hud_in_panels())
		return;

	regress_gameplay_frames++;
	if (!modern_last_frame_gameplay_panels())
	{
		if (regress_gameplay_missing == 0)
			snprintf(regress_gameplay_first, sizeof regress_gameplay_first,
			         "frame %lu: in-level frame used the full-frame composition "
			         "(gameplay=%d)", regress_frame, modern_last_frame_gameplay() ? 1 : 0);
		regress_gameplay_missing++;
	}

	// The engine darkens the playfield in place while a brightness ramp is
	// running (the level-start fade-in and filterFade events); the HUD panels
	// and message strip must mirror exactly that brightness ramp, or they show
	// at full brightness over a dark playfield (the level-start flash).  A
	// colour override or a static offset is deliberately NOT mirrored.
	const bool level_fade = levelBrightness != -99 && explosionTransparent &&
	                        filterFade && levelBrightnessChg != 0;
	if (level_fade)
	{
		regress_gameplay_filter_frames++;
		if (!modern_last_frame_hud_filtered())
		{
			if (regress_gameplay_filter_missing == 0)
				snprintf(regress_gameplay_filter_first, sizeof regress_gameplay_filter_first,
				         "frame %lu: brightness fade active (int=%d) but the HUD "
				         "did not follow it", regress_frame, levelBrightness);
			regress_gameplay_filter_missing++;
		}
	}
}

static void regress_note_smoothness(void)
{
	if (!drawlist_smoothness_enabled())
		return;

	const unsigned long events = drawlist_smoothness_events();
	if (events > regress_smooth_seen_events && regress_smooth_log_count < 8)
	{
		logError("Smoothness: %lu event(s) by presented frame %lu.",
		         events - regress_smooth_seen_events, regress_frame);
		regress_smooth_log_count++;
	}
	regress_smooth_seen_events = events;
}

bool regress_active(void)
{
	return regress_demo != 0 || regress_scenario_active() || regress_screen_active() || regress_script_active();
}

bool regress_realtime_active(void)
{
	return regress_realtime != 0;
}

bool regress_scenario_active(void)
{
	// --regress-script uses the same episode/level but reaches the level through
	// the episode script, so it is not the synthetic-scenario path.
	return regress_scenario_episode != 0 && !regress_script;
}

bool regress_script_active(void)
{
	return regress_script && regress_scenario_episode != 0;
}

bool regress_screen_active(void)
{
	return regress_screen != NULL;
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
	static const char *const screen_option   = "--regress-screen";
	static const char *const script_option   = "--regress-script";

	for (int i = 1; i < argc; ++i)
	{
		if (arg_is_option(argv[i], demo_option, strlen(demo_option)) ||
		    arg_is_option(argv[i], scenario_option, strlen(scenario_option)) ||
		    arg_is_option(argv[i], audio_option, strlen(audio_option)) ||
		    arg_is_option(argv[i], screen_option, strlen(screen_option)) ||
		    arg_is_option(argv[i], script_option, strlen(script_option)))
			return true;
	}

	return false;
}

Uint32 regress_clock_ticks10bit(void)
{
	if (!regress_active() || regress_realtime_active())
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

// Hash a scalar as explicit little-endian bytes of a fixed width, so the
// digest does not depend on the host's `long`/`int` widths or endianness.
// The widths match what the native-size hashing produced on the LP64
// baselines (macOS/Linux), so those baselines stay valid.
static void hash_u8(Uint64 *hash, Uint8 v)
{
	hash_bytes(hash, &v, 1);
}

static void hash_u16le(Uint64 *hash, Uint16 v)
{
	const Uint8 b[2] = { (Uint8)v, (Uint8)(v >> 8) };
	hash_bytes(hash, b, sizeof b);
}

static void hash_u32le(Uint64 *hash, Uint32 v)
{
	const Uint8 b[4] = { (Uint8)v, (Uint8)(v >> 8), (Uint8)(v >> 16), (Uint8)(v >> 24) };
	hash_bytes(hash, b, sizeof b);
}

static void hash_u64le(Uint64 *hash, Uint64 v)
{
	Uint8 b[8];
	for (int i = 0; i < 8; ++i)
		b[i] = (Uint8)(v >> (8 * i));
	hash_bytes(hash, b, sizeof b);
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

	hash_u64le(&hash, (Uint64)mt_rand_state_hash());

	for (int i = 0; i < 2; ++i)
	{
		const Player *p = &player[i];

		// Scalars are hashed as explicit little-endian fixed-width values so
		// the digest is identical on LP64 (macOS/Linux) and LLP64 (Windows,
		// where `unsigned long` is 4 bytes).  `cash` is an unsigned long that
		// fits in 32 bits, so zero-extending it to 64 bits reproduces the 8
		// bytes the LP64 baselines hash; every other scalar keeps its width.
		hash_u64le(&hash, (Uint64)p->cash);
		hash_bytes(&hash, (const Uint8 *)&p->items, sizeof p->items);
		hash_bytes(&hash, (const Uint8 *)&p->last_items, sizeof p->last_items);
		hash_u8(&hash, p->is_dragonwing ? 1 : 0);
		hash_u32le(&hash, (Uint32)p->shield_max);
		hash_u32le(&hash, (Uint32)p->initial_armor);
		hash_u32le(&hash, (Uint32)p->shot_hit_area_x);
		hash_u32le(&hash, (Uint32)p->shot_hit_area_y);
		hash_u8(&hash, p->is_alive ? 1 : 0);
		hash_u32le(&hash, (Uint32)p->invulnerable_ticks);
		hash_u32le(&hash, (Uint32)p->exploding_ticks);
		hash_u32le(&hash, (Uint32)p->shield);
		hash_u32le(&hash, (Uint32)p->armor);
		hash_u32le(&hash, (Uint32)p->weapon_mode);
		hash_u32le(&hash, (Uint32)p->superbombs);
		hash_u32le(&hash, (Uint32)p->purple_balls_needed);
		hash_u16le(&hash, p->mouseX);
		hash_u16le(&hash, p->mouseY);
		hash_u32le(&hash, (Uint32)p->x);
		hash_u32le(&hash, (Uint32)p->y);
		for (int j = 0; j < 20; ++j)
			hash_u32le(&hash, (Uint32)p->old_x[j]);
		for (int j = 0; j < 20; ++j)
			hash_u32le(&hash, (Uint32)p->old_y[j]);
		hash_u32le(&hash, (Uint32)p->x_velocity);
		hash_u32le(&hash, (Uint32)p->y_velocity);
		hash_u32le(&hash, (Uint32)p->x_friction_ticks);
		hash_u32le(&hash, (Uint32)p->y_friction_ticks);
		hash_u32le(&hash, (Uint32)p->delta_x_shot_move);
		hash_u32le(&hash, (Uint32)p->delta_y_shot_move);
		hash_u32le(&hash, (Uint32)p->last_x_shot_move);
		hash_u32le(&hash, (Uint32)p->last_y_shot_move);
		hash_u32le(&hash, (Uint32)p->last_x_explosion_follow);
		hash_u32le(&hash, (Uint32)p->last_y_explosion_follow);
		// sidekick/items are all fixed-width fields with no `long`, so their
		// raw bytes are the same on LP64 and LLP64.
		hash_bytes(&hash, (const Uint8 *)p->sidekick, sizeof p->sidekick);

		const Uint8 lives = (p->lives != NULL) ? *p->lives : 0;
		hash_u8(&hash, lives);
	}

	// Enemies: every field except the two pointers (sprite2s, enemydatofs),
	// whose addresses vary with the process layout.  The struct holds only
	// fixed-width fields plus those pointers, so the byte ranges (and the zero
	// padding) are the same on LP64 and LLP64.
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
	hash_u16le(&hash, tempW);
	hash_u16le(&hash, eventLoc);
	hash_u16le(&hash, curLoc);
	hash_u8(&hash, levelTimer ? 1 : 0);
	hash_u16le(&hash, levelTimerCountdown);
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

bool regress_add_snapshot(unsigned long frame, const char *path)
{
	if (regress_snapshot_count >= REGRESS_MAX_SNAPSHOTS)
		return false;

	regress_snapshots[regress_snapshot_count].frame = frame;
	regress_snapshots[regress_snapshot_count].path = path;
	++regress_snapshot_count;
	return true;
}

bool regress_has_snapshots(void)
{
	return regress_snapshot_count > 0;
}

// Saves `surface` (8-bit) to every snapshot requested for the current frame,
// attaching the active palette so the BMP is viewable.
static void regress_save_snapshots_8bit(const SDL_Surface *surface)
{
	for (int i = 0; i < regress_snapshot_count; ++i)
	{
		if (regress_snapshots[i].frame != regress_frame)
			continue;

		SDL_Surface *tmp = SDL_CreateSurface(surface->w, surface->h, SDL_PIXELFORMAT_INDEX8);
		if (tmp == NULL)
		{
			logError("Failed to create snapshot surface: %s", SDL_GetError());
			continue;
		}

		const int row = MIN(surface->w, tmp->w);
		for (int y = 0; y < surface->h && y < tmp->h; ++y)
			memcpy((Uint8 *)tmp->pixels + (size_t)y * tmp->pitch,
			       (const Uint8 *)surface->pixels + (size_t)y * surface->pitch, (size_t)row);

		SDL_Palette *palette = SDL_CreatePalette(256);
		if (palette != NULL)
		{
			SDL_SetPaletteColors(palette, get_active_palette(), 0, 256);
			SDL_SetSurfacePalette(tmp, palette);
		}

		if (!SDL_SaveBMP(tmp, regress_snapshots[i].path))
			logError("Failed to save snapshot '%s': %s", regress_snapshots[i].path, SDL_GetError());
		else
			logInfo("Regression: saved snapshot frame %lu to '%s'.", regress_frame, regress_snapshots[i].path);

		if (palette != NULL)
			SDL_DestroyPalette(palette);
		SDL_DestroySurface(tmp);
	}
}

// Saves the current Modern canvas (XRGB8888) to every requested snapshot.
static void regress_save_snapshots_modern(const ModernFrame *frame)
{
	for (int i = 0; i < regress_snapshot_count; ++i)
	{
		if (regress_snapshots[i].frame != regress_frame)
			continue;

		SDL_Surface *tmp = SDL_CreateSurfaceFrom(frame->w, frame->h, SDL_PIXELFORMAT_XRGB8888,
		                                         frame->pixels, frame->pitch);
		if (tmp == NULL)
		{
			logError("Failed to wrap the modern canvas for a snapshot: %s", SDL_GetError());
			continue;
		}

		if (!SDL_SaveBMP(tmp, regress_snapshots[i].path))
			logError("Failed to save snapshot '%s': %s", regress_snapshots[i].path, SDL_GetError());
		else
			logInfo("Regression: saved snapshot frame %lu to '%s'.", regress_frame, regress_snapshots[i].path);

		SDL_DestroySurface(tmp);
	}
}

void regress_capture_frame(SDL_Surface *surface)
{
	if (surface == NULL)
		return;
	if (regress_out == NULL && regress_state_out == NULL && !regress_has_snapshots())
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

	if (regress_has_snapshots())
		regress_save_snapshots_8bit(surface);

	regress_note_smoothness();
	regress_emit_records(write_frame, hash);
}

void regress_capture_modern_frame(void)
{
	if (regress_out == NULL && regress_state_out == NULL && !regress_has_snapshots())
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

	if (regress_has_snapshots())
		regress_save_snapshots_modern(frame);

	regress_check_gameplay_composition();
	regress_note_smoothness();
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

	// --regress-arcade turns the scenario into a 1-player arcade run (the same
	// `onePlayerAction` flag the 1-player arcade menu sets), so the arcade HUD
	// -- in particular the extra-lives row -- can be captured headless.
	if (regress_arcade)
	{
		onePlayerAction = true;
		player[0].items.weapon[FRONT_WEAPON].power = 6;  // extra lives (arcade)
		player[0].cash = 12345;
	}

	// --regress-players=2 turns the scenario into a deterministic two-player
	// run so the compact 2-player HUD can be captured headless.  Player 2 gets
	// a distinct loadout and some cash so the capture shows real data; player 1
	// gets extra lives (stored in the front weapon power field, as the engine
	// does) so the life icons are visible.
	if (regress_players == 2)
	{
		twoPlayerMode = true;

		player[0].items.weapon[FRONT_WEAPON].power = 6;
		player[0].items.weapon[REAR_WEAPON].id = 15;   // Vulcan Cannon
		player[0].items.weapon[REAR_WEAPON].power = 2;
		player[0].items.sidekick[LEFT_SIDEKICK] = 1;
		player[0].items.sidekick[RIGHT_SIDEKICK] = 2;
		player[0].weapon_mode = 1;
		player[0].cash = 2820;

		player[1].items = player[0].items;
		player[1].items.weapon[FRONT_WEAPON].id = 1;   // Pulse Cannon
		player[1].items.weapon[FRONT_WEAPON].power = 3;
		player[1].items.weapon[REAR_WEAPON].id = 15;   // Vulcan Cannon
		player[1].items.weapon[REAR_WEAPON].power = 2;
		player[1].items.sidekick[LEFT_SIDEKICK] = 2;
		player[1].items.sidekick[RIGHT_SIDEKICK] = 0;
		player[1].weapon_mode = 2;
		player[1].cash = 6789;
		player[1].last_items = player[1].items;
	}

	player[0].last_items = player[0].items;
}

void regress_init(void)
{
	// Headless by default.  Respect a driver the caller set explicitly.  The
	// real-time pacing benchmark opens a real window on purpose.
	if (!regress_realtime_active() && SDL_getenv("SDL_VIDEO_DRIVER") == NULL)
		SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
	if (SDL_getenv("SDL_AUDIO_DRIVER") == NULL)
		SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);

	// Regress playback needs neither audio nor joystick input, except the
	// offline audio regression, which drives the mixer directly.
	audio_disabled = !regress_audio_active();
	ignore_joystick = true;

	// No controller is opened and joystick hot-plug ADDED/REMOVED events are
	// ignored while ignore_joystick is set (see init_joysticks() and
	// joystick_device_added()/joystick_device_removed()).  handleSdlEvents()
	// additionally drops every input event in regress mode, so a device plugged
	// in mid-run cannot feed the game.
	//
	// Because that drop happens before any focus event is processed, redefine
	// the focused state directly: the release build auto-pauses while the window
	// is unfocused (see JE_main), and a regress run must never pause.
	windowHasFocus = true;

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
	presentation = (regress_modern || regress_realtime) ? PRESENTATION_MODERN : PRESENTATION_CLASSIC;
	if (regress_realtime_active())
		modern_aspect = regress_aspect >= 0 ? (ModernAspect)regress_aspect : MODERN_ASPECT_16_9;
	else
		modern_aspect = regress_aspect >= 0 ? (ModernAspect)regress_aspect : MODERN_ASPECT_4_3;
	modern_pixel_aspect = PIXEL_ASPECT_ORIGINAL;

	// VFX are pinned OFF for every existing baseline, so adding the particle
	// system does not change a single 8-bit frame; --regress-vfx opts a case
	// into a level (and the VFX cases run Modern 16:9, where they render).
	vfx_level = (regress_vfx >= 0) ? (VfxLevel)regress_vfx : VFX_OFF;

	// Record and replay-check every level tick.  This only observes: the draw
	// list is built from the existing drawing primitives and replayed into a
	// scratch surface; the real frame hash stream is unchanged.
	if (regress_replay_check)
	{
		drawlist_set_enabled(true);
		drawlist_set_check(true);
	}

	// Stage-3 proof: render each level tick's interpolated frame at alpha = 1
	// with the persistent renderer and compare it with the real frame.  Also
	// enable recording when a mid-tick snapshot is requested so there are two
	// lists to interpolate between.
	if (regress_interp_check || interp_regress_alpha_active())
		drawlist_set_enabled(true);
	if (regress_interp_check)
		drawlist_set_interp_check(true);

	// Stage-3 smoothness proof: re-derive the sub-frame positions for every
	// level tick and check they are monotonic.
	if (regress_interp_smoothness)
	{
		drawlist_set_enabled(true);
		drawlist_set_smoothness_check(true);
		drawlist_set_smoothness_alphas((unsigned)regress_smooth_alphas);
	}

	// Parallax guard: run the interpolated presentation every tick and require
	// it to leave the starfield and the background scroll untouched, so the
	// per-tick displacement is the same with the interpolation on or off.
	if (regress_parallax_check)
	{
		drawlist_set_enabled(true);
		drawlist_set_parallax_check(true);
	}

	// Real-time pacing benchmark: log presented-fps statistics and exit after
	// the requested duration.
	if (regress_realtime_active())
		interp_bench_start(regress_bench_seconds);

	// Bloom and dynamic lighting are pinned OFF for every existing case so the
	// baselines stay byte-for-byte unchanged; --regress-lighting opts a run into
	// the new look (it sets both effects, the way the "Lighting" picker does),
	// and --regress-bloom is an advanced override of bloom only.  The user's
	// config cannot leak in because loadConfiguration() is skipped in regress
	// mode.
	if (regress_lighting_quality >= 0)
	{
		modern_bloom_quality = (ModernQuality)regress_lighting_quality;
		modern_lighting_quality = (ModernQuality)regress_lighting_quality;
	}
	else
	{
		modern_bloom_quality = regress_bloom_quality >= 0 ? (ModernQuality)regress_bloom_quality : MODERN_QUALITY_OFF;
		modern_lighting_quality = MODERN_QUALITY_OFF;
	}

	// --regress-seed pins the RNG for a regress run.  Demo playback and the
	// synthetic scenarios have their own fixed streams, but a --regress-script
	// run otherwise inherits main()'s time(NULL) seed, which makes it
	// irreproducible; the option lets such a run (e.g. a scripted death that
	// reaches the !playDemo GAME OVER) be a stable baseline.  Regress-only and
	// opt-in: it never affects a normal game.
	if (regress_seed_set)
		mt_srand(regress_seed);

	JE_initProcessorType();

	// Start the virtual clock at a fixed value; setFrameSpeed() re-anchors the
	// frame counters from it, so the absolute value is irrelevant.
	regress_clock = 0;
	regress_frame = 0;

	// A screen run always has a built-in frame cap: the screen functions loop
	// forever waiting for input, so the cap is what ends the run.
	if (regress_screen_active() && regress_frames == 0)
		regress_frames = REGRESS_SCREEN_FRAMES;

	// The episode script can block on an input-driven screen (the item screen)
	// once the WARNING text is done, so give --regress-script a default cap too.
	if (regress_script_active() && regress_frames == 0)
		regress_frames = REGRESS_SCREEN_FRAMES;

	if (regress_out_path == NULL && regress_state_out_path == NULL && !regress_has_snapshots() &&
	    !regress_realtime_active())
	{
		logFatal("--regress-demo/--regress-level/--regress-audio/--regress-screen require --regress-out=FILE, --regress-state-out=FILE or --regress-snapshot=FRAME:FILE.");
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

void regress_frame_reset(void)
{
	regress_frame = 0;
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

	if (regress_interp_check)
	{
		logInfo("Interp check: %lu level frames interpolated at alpha=1, %lu mismatched.",
		        drawlist_checked_frames(), drawlist_mismatched_frames());
		logInfo("Interp stats: %lu matched; snaps new=%lu jump=%lu sheet=%lu; overshoots=%lu.",
		        drawlist_interp_matched(), drawlist_interp_snap_new(), drawlist_interp_snap_jump(),
		        drawlist_interp_snap_sheet(), drawlist_interp_overshoots());
		if (drawlist_mismatched_frames() != 0)
		{
			logError("Interp check FAILED: %s", drawlist_first_mismatch());
			exit(EXIT_FAILURE);
		}
	}
	else if (regress_replay_check)
	{
		logInfo("Replay check: %lu level frames replayed, %lu mismatched.",
		        drawlist_checked_frames(), drawlist_mismatched_frames());
		if (drawlist_mismatched_frames() != 0)
		{
			logError("Replay check FAILED: %s", drawlist_first_mismatch());
			exit(EXIT_FAILURE);
		}
	}

	if (regress_interp_smoothness)
	{
		logInfo("Smoothness: %lu level ticks, %lu bg layer checks, %lu object checks.",
		        drawlist_smoothness_ticks(), drawlist_smoothness_bg_checks(),
		        drawlist_smoothness_object_checks());
		logInfo("Smoothness: horizontal events %lu, vertical events %lu, object events %lu (%lu event frames).",
		        drawlist_smoothness_horizontal_events(), drawlist_smoothness_vertical_events(),
		        drawlist_smoothness_object_events(), drawlist_smoothness_frames());
		if (drawlist_smoothness_events() != 0)
		{
			logError("Smoothness check FAILED: %lu non-monotonic motion events.",
			         drawlist_smoothness_events());
			exit(EXIT_FAILURE);
		}
	}

	if (regress_gameplay_check)
	{
		logInfo("Gameplay composition check: %lu in-level Modern frames, %lu without the panels.",
		        regress_gameplay_frames, regress_gameplay_missing);
		logInfo("Gameplay fade check: %lu frames in a brightness fade, %lu where the HUD did not follow.",
		        regress_gameplay_filter_frames, regress_gameplay_filter_missing);
		if (regress_gameplay_missing != 0)
		{
			logError("Gameplay composition check FAILED: %s", regress_gameplay_first);
			exit(EXIT_FAILURE);
		}
		if (regress_gameplay_filter_missing != 0)
		{
			logError("Gameplay fade check FAILED: %s", regress_gameplay_filter_first);
			exit(EXIT_FAILURE);
		}
	}

	if (regress_parallax_check)
	{
		logInfo("Parallax check: %lu level ticks, %lu presentation state mutations, %lu double updates, %lu advance mismatches.",
		        drawlist_parallax_ticks(), drawlist_parallax_mutations(),
		        drawlist_parallax_double_updates(), drawlist_parallax_advance_mismatches());
		if (drawlist_parallax_mutations() != 0 ||
		    drawlist_parallax_double_updates() != 0 ||
		    drawlist_parallax_advance_mismatches() != 0)
		{
			logError("Parallax check FAILED: the starfield/background moved beyond one tick.");
			exit(EXIT_FAILURE);
		}
	}
}
