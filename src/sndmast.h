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
#ifndef SNDMAST_H
#define SNDMAST_H

#include "opentyr.h"

#include "game_schema.h"

// Capacities: the largest variant.  The counts actually loaded come from the
// selected GameDataSchema (sfx_count, voice_count).
#define SFX_COUNT_MAX 31
#define VOICE_COUNT 9
#define SOUND_COUNT (SFX_COUNT_MAX + VOICE_COUNT)

enum
{
	S_NONE             =  0,
	S_WEAPON_1         =  1,
	S_WEAPON_2         =  2,
	S_ENEMY_HIT        =  3,
	S_EXPLOSION_4      =  4,
	S_WEAPON_5         =  5,
	S_WEAPON_6         =  6,
	S_WEAPON_7         =  7,
	S_SELECT           =  8,
	S_EXPLOSION_8      =  8,
	S_EXPLOSION_9      =  9,
	S_WEAPON_10        = 10,
	S_EXPLOSION_11     = 11,
	S_EXPLOSION_12     = 12,
	S_WEAPON_13        = 13,
	S_WEAPON_14        = 14,
	S_WEAPON_15        = 15,
	S_SPRING           = 16,
	S_WARNING          = 17,
	S_ITEM             = 18,
	S_HULL_HIT         = 19,
	S_MACHINE_GUN      = 20,
	S_SOUL_OF_ZINGLON  = 21,
	S_EXPLOSION_22     = 22,
	S_CLINK            = 23,
	S_CLICK            = 24,
	S_WEAPON_25        = 25,
	S_WEAPON_26        = 26,
	S_SHIELD_HIT       = 27,
	S_CURSOR           = 28,
	S_POWERUP          = 29,
	// Tyrian 2000 inserts two more effects (30, 31) before the voices; those two
	// slots are voices in Tyrian 2.1, so name voices only through V_*.
};

// Voices, in the order of voices.snd.  Their sound IDs shift with the number of
// effects of the selected variant (29 + 1 + n in 2.1, 31 + 1 + n in 2000).
enum
{
	VOICE_CLEARED_PLATFORM,  // "Cleared enemy platform."
	VOICE_BOSS,              // "Large enemy approaching."
	VOICE_ENEMIES,           // "Enemies ahead."
	VOICE_GOOD_LUCK,         // "Good luck."
	VOICE_LEVEL_END,         // "Level completed."
	VOICE_DANGER,            // "Danger."
	VOICE_SPIKES,            // "Warning: spikes ahead."
	VOICE_DATA_CUBE,         // "Data acquired."
	VOICE_ACCELERATE         // "Unexplained speed increase."
};

#define V_CLEARED_PLATFORM gameVoiceSound(VOICE_CLEARED_PLATFORM)
#define V_BOSS             gameVoiceSound(VOICE_BOSS)
#define V_ENEMIES          gameVoiceSound(VOICE_ENEMIES)
#define V_GOOD_LUCK        gameVoiceSound(VOICE_GOOD_LUCK)
#define V_LEVEL_END        gameVoiceSound(VOICE_LEVEL_END)
#define V_DANGER           gameVoiceSound(VOICE_DANGER)
#define V_SPIKES           gameVoiceSound(VOICE_SPIKES)
#define V_DATA_CUBE        gameVoiceSound(VOICE_DATA_CUBE)
#define V_ACCELERATE       gameVoiceSound(VOICE_ACCELERATE)

extern const char soundTitle21[SFX_COUNT_MAX - 2 + VOICE_COUNT][9];
extern const char soundTitle2000[SOUND_COUNT][9];

// Sound ID (1-based) played for level warning text number id (1-based).
JE_byte windowTextSample(unsigned int id);

#endif /* SNDMAST_H */
