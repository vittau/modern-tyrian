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
#include "game_rules.h"

#include "sndmast.h"

#include <stdio.h>
#include <string.h>

// --- Arcade ships -----------------------------------------------------------

// Tyrian 2.1.  Ships: Stealth, StormWind, Techno, Enemy, Weird, Unknown, NortShip Z.
static const uint8_t arcadeNextShip21[] = { 3, 9, 6, 2, 5, 1, 4, 3, 7 };  // 0 -> 3 -> 2 -> 6 -> 4 -> 5 -> 1 -> 9 -> 7
static const uint16_t arcadeSpecial21[]  = { 7, 8, 9, 10, 11, 12, 13 };
static const uint16_t arcadeSpecialB21[] = { 37, 6, 15, 40, 16, 14, 41 };
static const uint8_t arcadeShip21[]      = { 3, 1, 5, 10, 2, 11, 12 };
static const uint16_t arcadeWeapon21[][5] =
{  /*  R  Bl  Bk  G   P */
	{  9, 31, 32, 33, 34 },  /* Stealth Ship */
	{ 19,  8, 22, 41, 34 },  /* StormWind    */
	{ 27,  5, 20, 42, 31 },  /* Techno       */
	{ 15,  3, 28, 22, 12 },  /* Enemy        */
	{ 23, 35, 25, 14,  6 },  /* Weird        */
	{  2,  5, 21,  4,  7 },  /* Unknown      */
	{ 40, 38, 37, 41, 36 }   /* NortShip Z   */
};

static const GameArcadeRules arcade21 =
{
	7, 8, 9, 8,
	arcadeNextShip21, arcadeSpecial21, arcadeSpecialB21, arcadeShip21, arcadeWeapon21,
	VOICE_DATA_CUBE, VOICE_DANGER, false, false
};

// Tyrian 2000 adds Dragon and Pretzel Pete after NortShip Z and moves the two
// title codes up by two (fork c0782f0, varz.c/varz.h).
static const uint8_t arcadeNextShip2000[] = { 3, 8, 6, 2, 5, 1, 4, 10, 9, 7, 3 };
static const uint16_t arcadeSpecial2000[]  = { 7, 8, 9, 10, 11, 12, 13, 48, 47 };
static const uint16_t arcadeSpecialB2000[] = { 37, 6, 15, 40, 16, 14, 41, 48, 47 };
static const uint8_t arcadeShip2000[]      = { 3, 1, 5, 10, 2, 11, 12, 15, 17 };
static const uint16_t arcadeWeapon2000[][5] =
{  /*  R  Bl  Bk  G   P */
	{  9, 31, 32, 33, 34 },  /* Stealth Ship */
	{ 19,  8, 22, 41, 34 },  /* StormWind    */
	{ 27,  5, 20, 42, 31 },  /* Techno       */
	{ 15,  3, 28, 22, 12 },  /* Enemy        */
	{ 23, 35, 25, 14,  6 },  /* Weird        */
	{  2,  5, 21,  4,  7 },  /* Unknown      */
	{ 40, 38, 37, 41, 36 },  /* NortShip Z   */
	{ 47, 45, 19, 33, 19 },  /* Dragon       */
	{ 44, 26, 46, 26,  1 }   /* Pretzel Pete */
};

// Preserve the fork's state 8 after a Zinglon Super Tyrian win, even though
// the extended progression table now makes that state offer Pretzel Pete.
static const GameArcadeRules arcade2000 =
{
	9, 10, 11, 8,
	arcadeNextShip2000, arcadeSpecial2000, arcadeSpecialB2000, arcadeShip2000, arcadeWeapon2000,
	VOICE_DANGER, VOICE_GOOD_LUCK, true, true
};

// --- Twiddles ---------------------------------------------------------------

// Per ship, the combos of keyboardCombos (varz.c) it can perform, by ship item.
static const uint8_t shipCombos21[14][3] =
{
	{ 5, 4, 7},  /*2nd Player ship*/
	{ 1, 2, 0},  /*USP Talon*/
	{14, 4, 0},  /*Super Carrot*/
	{ 4, 5, 0},  /*Gencore Phoenix*/
	{ 6, 5, 0},  /*Gencore Maelstrom*/
	{ 7, 8, 0},  /*MicroCorp Stalker*/
	{ 7, 9, 0},  /*MicroCorp Stalker-B*/
	{10, 3, 5},  /*Prototype Stalker-C*/
	{ 5, 8, 9},  /*Stalker*/
	{ 1, 3, 0},  /*USP Fang*/
	{ 7,16,17},  /*U-Ship*/
	{ 2,11,12},  /*1st Player ship*/
	{ 3, 8,10},  /*Nort ship*/
	{ 0, 0, 0}   // Dummy entry added for Stalker 21.126
};

static const uint8_t shipCombos2000[19][3] =
{
	{ 5, 4, 7},  /*2nd Player ship*/
	{ 1, 2, 0},  /*USP Talon*/
	{14, 4, 0},  /*Super Carrot*/
	{ 4, 5, 0},  /*Gencore Phoenix*/
	{ 6, 5, 0},  /*Gencore Maelstrom*/
	{ 7, 8, 0},  /*MicroCorp Stalker*/
	{ 7, 9, 0},  /*MicroCorp Stalker-B*/
	{10, 3, 5},  /*Prototype Stalker-C*/
	{ 5, 8, 9},  /*Stalker*/
	{ 1, 3, 0},  /*USP Fang*/
	{ 7,16,17},  /*U-Ship*/
	{ 2,11,12},  /*1st Player ship*/
	{ 3, 8,10},  /*Nort ship*/
	{ 0, 0, 0},  // Dummy entry added for Stalker 21.126
	{ 1, 0, 0},  /*Storm*/
	{ 4, 0, 0},  /*Red Dragon*/
	{ 5, 9, 2},  /*Gencore II*/
	{ 0, 0, 0},  /*PeteZoomer*/
	{ 0, 0, 0}   /*Rum Bottle*/
};

// --- Level events -----------------------------------------------------------

// Event 68 was the random explosions of 2.1 and became "replace enemy" in 2000,
// which moved the explosions to event 99.  83 is a second map stop.  58 and 59
// are new.  84 and 85 only do anything in a Timed Battle.
static const GameEventRule events2000[] =
{
	{ 58, GAME_EVENT_SET_ENEMY_LAUNCH },
	{ 59, GAME_EVENT_REPLACE_ENEMY },
	{ 68, GAME_EVENT_REPLACE_ENEMY },
	{ 83, 4 },
	{ 84, GAME_EVENT_BATTLE_TIMER },
	{ 85, GAME_EVENT_BATTLE_ENEMY_DEATH },
	{ 99, 68 }
};

static const uint32_t initialCash21[]   = { 10000, 15000, 20000, 30000 };
static const uint32_t initialCash2000[] = { 10000, 15000, 20000, 30000, 20000 };

static const GameTimedBattleRules timedBattle2000 =
{
	3, { 1, 5, 5 }, 13, 100, 1000
};

const GameRules gameRules21 =
{
	NULL, 0,
	0xFFFF,
	false, 0, 0, 0,
	shipCombos21, sizeof shipCombos21 / sizeof *shipCombos21,
	&arcade21,
	false,
	53, -1, false,
	initialCash21, false,
	NULL, false
};

const GameRules gameRules2000 =
{
	events2000, sizeof events2000 / sizeof *events2000,
	1000,
	true, -200, 24, 208,
	shipCombos2000, sizeof shipCombos2000 / sizeof *shipCombos2000,
	&arcade2000,
	true,
	54, 198, true,
	initialCash2000, true,
	&timedBattle2000, true
};

const GameRules *gameRules(void)
{
	return gameVariantCurrent()->rules;
}

unsigned int gameEventCase(unsigned int eventType)
{
	const GameRules *rules = gameRules();

	for (size_t i = 0; i < rules->event_count; ++i)
	{
		if (rules->events[i].type == eventType)
			return rules->events[i].action;
	}
	return eventType;
}

void gameEnemyLaunch(unsigned int enemyId, uint16_t elaunchtype, uint16_t *type, uint8_t *special)
{
	if (enemyId > gameRules()->enemy_full_launch_above)
	{
		*type = elaunchtype;
		*special = 0;
	}
	else
	{
		*type = elaunchtype % 1000;
		*special = elaunchtype / 1000;
	}
}

const uint8_t *gameShipCombos(unsigned int ship)
{
	static const uint8_t none[3] = { 0, 0, 0 };
	const GameRules *rules = gameRules();

	return ship < rules->ship_combo_count ? rules->ship_combos[ship] : none;
}

bool gameSidekickMainFire(unsigned int pwr)
{
	return !(gameRules()->charging_sidekick_own_fire && pwr > 0);
}

uint8_t gameShotTrail(uint8_t weaponTrail, unsigned int multiPos)
{
	const GameRules *rules = gameRules();

	if (rules->trail_first_tile_only && weaponTrail == rules->alt_smoke_trail && multiPos > 1)
		return 255;
	return weaponTrail;
}

bool gameIsSmokeTrail(int trail)
{
	const int alternate = gameRules()->alt_smoke_trail;
	return trail == 98 || (alternate >= 0 && trail == alternate);
}

void gameFormatLevelTimer(char *buffer, size_t size, int countdown)
{
	if (gameRules()->timer_truncated_tenths)
		snprintf(buffer, size, "%d.%d", countdown / 100, (countdown / 10) % 10);
	else
		snprintf(buffer, size, "%.1f", countdown / 100.0f);
}
