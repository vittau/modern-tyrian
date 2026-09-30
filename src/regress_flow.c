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
#include "regress_flow.h"

#include "config.h"
#include "destruct.h"
#include "demo.h"
#include "episodes.h"
#include "file.h"
#include "game_menu.h"
#include "game_rules.h"
#include "game_schema.h"
#include "helptext.h"
#include "keyboard.h"
#include "logging.h"
#include "mainint.h"
#include "mtrand.h"
#include "opentyr.h"
#include "player.h"
#include "regress.h"
#include "tyrian2.h"
#include "varz.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *regress_flow = NULL;

// The flow's own keyboard.  Each entry is one key press the input layer hands
// out when the game waits for input and none is queued.
#define FLOW_KEYS_MAX 256
#define FLOW_FILL_LIMIT 4000

// The row of "Next Level" in the shop's main menu (rows start at 2).
#define NEXT_LEVEL_ROW 6

static KeyboardInput flowKeys[FLOW_KEYS_MAX];
static size_t flowKeyCount, flowKeyNext;

// After the scripted keys: a key to keep pressing (Enter, to leave shops, maps
// and story screens), or none, which ends the run.
static bool flowFillEnter;
static unsigned long flowFilled;

// Per-level effects.
static int flowLevelTicks;       // ends the level as completed after this many ticks; 0 = never
static int flowDieTicks;         // kills player 1 after this many ticks; 0 = never
static int flowCash;             // cash added to player 1 when a level begins
static int flowEpisode, flowLastEpisode;
static int flowTick;             // ticks since the level began
static int flowLevelsStop;       // stop once this many levels have begun and run a few ticks; 0 = never
static int flowLevels;           // levels begun so far
static bool flowFeedPolls;       // hand out a key at every poll of the keyboard (Destruct)
static int flowCode;             // secret code typed at the title: FLOW_CODE_*
static int flowShip;             // arcade ship (1-based) for FLOW_CODE_SHIP
static int flowPickEpisode;      // episode picked on the Super Tyrian menu

enum { FLOW_CODE_NONE, FLOW_CODE_SHIP, FLOW_CODE_SUPER_TYRIAN, FLOW_CODE_DESTRUCT };

static int flowParam(const char *key, int fallback)
{
	const char *params = strchr(regress_flow, ':');
	const size_t keyLen = strlen(key);

	while (params != NULL)
	{
		++params;
		if (strncmp(params, key, keyLen) == 0 && params[keyLen] == '=')
			return atoi(params + keyLen + 1);
		params = strchr(params, ',');
	}
	return fallback;
}

static bool flowIs(const char *name)
{
	const size_t len = strlen(name);
	return strncmp(regress_flow, name, len) == 0 && (regress_flow[len] == '\0' || regress_flow[len] == ':');
}

bool regress_flow_active(void)
{
	return regress_flow != NULL;
}

// --- The keyboard -----------------------------------------------------------

static void pushKey(SDL_Scancode scancode)
{
	if (flowKeyCount < FLOW_KEYS_MAX)
	{
		KeyboardInput *const key = &flowKeys[flowKeyCount++];
		key->sym = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
		key->scancode = (Uint16)scancode;
		key->mod = SDL_KMOD_NONE;
		key->ch = 0;
	}
}

static void pushKeys(SDL_Scancode scancode, int count)
{
	for (int i = 0; i < count; ++i)
		pushKey(scancode);
}

// Characters as a player types them: the key press first (the title screen's
// secret codes read it), then the text of a name entry.
static void pushText(const char *text)
{
	for (; *text != '\0' && flowKeyCount + 1 < FLOW_KEYS_MAX; ++text)
	{
		KeyboardInput *const key = &flowKeys[flowKeyCount++];
		key->sym = -1;
		key->scancode = (Uint16)-1;
		key->mod = SDL_KMOD_NONE;
		key->ch = (Uint8)*text;
	}
}

static void pushTyped(const char *text)
{
	for (; *text != '\0' && flowKeyCount < FLOW_KEYS_MAX; ++text)
	{
		KeyboardInput *const key = &flowKeys[flowKeyCount++];
		key->sym = (SDL_Keycode)(*text >= 'A' && *text <= 'Z' ? *text - 'A' + 'a' : *text);
		key->scancode = (Uint16)SDL_GetScancodeFromKey(key->sym, NULL);
		key->mod = SDL_KMOD_NONE;
		key->ch = 0;
	}
}

// The steps down the gameplay menu to the entry that does `choice`.
static int gameplayMenuSteps(GameplayChoice choice)
{
	const GameStringSchema *strings = gameStrings();

	for (size_t i = 0; i + 1 < strings->gameplay_name; ++i)
	{
		if (strings->gameplay_choices[i] == choice)
			return (int)i;
	}
	logFatal("The gameplay menu has no such entry.");
	exit(EXIT_FAILURE);
}

bool regress_flow_feeds_polls(void)
{
	return flowFeedPolls;
}

bool regress_flow_presses_through(void)
{
	return regress_flow_active() && flowFillEnter;
}

void regress_flow_supply_input(void)
{
	if (flowKeyNext < flowKeyCount)
	{
		keyboardPushInput(&flowKeys[flowKeyNext++]);
		return;
	}

	if (flowFillEnter && flowFilled < FLOW_FILL_LIMIT)
	{
		// Keys for a player who wants the next level: Enter leaves story screens,
		// and the shop is left through "Next Level" and the first destination on the
		// map, backing out of whatever else is open.
		SDL_Scancode scancode = SDL_SCANCODE_RETURN;
		int menu, sel;

		if (JE_itemScreenState(&menu, &sel))
		{
			if (menu == MENU_FULL_GAME)
				scancode = sel < NEXT_LEVEL_ROW ? SDL_SCANCODE_DOWN : SDL_SCANCODE_RETURN;
			else if (menu == MENU_1_PLAYER_ARCADE || menu == MENU_2_PLAYER_ARCADE || menu == MENU_SUPER_TYRIAN)
				scancode = SDL_SCANCODE_RETURN;   // the first row starts the level
			else if (menu != MENU_PLAY_NEXT_LEVEL)
				scancode = SDL_SCANCODE_ESCAPE;
		}

		KeyboardInput key = { SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false), (Uint16)scancode, SDL_KMOD_NONE, 0 };

		++flowFilled;
		keyboardPushInput(&key);
		return;
	}

	// The flow is over: the game is waiting for a key nobody will press.
	if (flowFillEnter)
	{
		logError("Flow did not finish within %d filler keys.", FLOW_FILL_LIMIT);
		regress_finish();
		exit(EXIT_FAILURE);
	}
	regress_flow_coverage("script complete (%lu keys).", (unsigned long)flowKeyCount);
	regress_finish();
	exit(EXIT_SUCCESS);
}

// --- Setup --------------------------------------------------------------------

void regress_flow_init(void)
{
	flowKeyCount = flowKeyNext = 0;
	flowFillEnter = false;
	flowFilled = 0;
	flowLevelTicks = flowDieTicks = flowCash = 0;
	flowEpisode = flowLastEpisode = 0;
	flowLevelsStop = flowLevels = 0;
	flowCode = FLOW_CODE_NONE;

	if (flowIs("arcade") || flowIs("supertyrian") || flowIs("destruct"))
	{
		// The keys are built in regress_flow_run(): they spell names the game
		// data holds, which are not loaded yet.
		const GameArcadeRules *const arcade = gameRules()->arcade;

		flowLevelsStop = flowParam("levels", 1);
		flowLevelTicks = 0;
		flowPickEpisode = flowParam("ep", 1);
		flowShip = flowParam("ship", 1);
		flowCode = flowIs("arcade") ? FLOW_CODE_SHIP : flowIs("supertyrian") ? FLOW_CODE_SUPER_TYRIAN : FLOW_CODE_DESTRUCT;

		flowFillEnter = flowCode != FLOW_CODE_DESTRUCT;   // then Enter through the story and shops

		if (flowCode == FLOW_CODE_SHIP && (flowShip < 1 || flowShip > arcade->ship_count))
		{
			logFatal("--regress-flow=arcade needs ship=1..%d.", arcade->ship_count);
			exit(EXIT_FAILURE);
		}
	}
	else if (flowIs("battle"))
	{
		const int sel = flowParam("sel", 1);

		const GameTimedBattleRules *const battle = gameRules()->timed_battle;

		if (battle == NULL || sel < 1 || sel > battle->battle_count)
		{
			logFatal("--regress-flow=battle needs a Timed Battle sel=1..%d.", battle == NULL ? 0 : battle->battle_count);
			exit(EXIT_FAILURE);
		}
		flowLevelTicks = flowParam("ticks", 0);
		flowDieTicks = flowParam("die", 0);
		flowCash = flowParam("cash", 0);

		pushKey(SDL_SCANCODE_RETURN);                                  // title: Start New Game
		pushKeys(SDL_SCANCODE_DOWN, gameplayMenuSteps(GAMEPLAY_TIMED_BATTLE));
		pushKey(SDL_SCANCODE_RETURN);                                  // gameplay: Timed Battle
		pushKeys(SDL_SCANCODE_DOWN, sel - 1);
		pushKey(SDL_SCANCODE_RETURN);                                  // the battle
		pushKey(SDL_SCANCODE_RETURN);                                  // difficulty: the default
		pushKey(SDL_SCANCODE_RETURN);                                  // end of the level
		if (flowDieTicks == 0)
		{
			pushText("ACE");                                           // name for the board
			pushKey(SDL_SCANCODE_RETURN);
			pushKey(SDL_SCANCODE_RETURN);                              // the board
		}
	}
	else if (flowIs("episode"))
	{
		flowEpisode = flowParam("ep", 1);
		flowLastEpisode = flowParam("to", flowEpisode);
		flowLevelTicks = flowParam("ticks", 4);
		flowCash = flowParam("cash", 0);

		if (flowEpisode < 1 || flowEpisode > EPISODE_AVAILABLE || flowLastEpisode < flowEpisode || flowLastEpisode > EPISODE_AVAILABLE)
		{
			logFatal("--regress-flow=episode needs ep=1..%d.", EPISODE_AVAILABLE);
			exit(EXIT_FAILURE);
		}

		pushKey(SDL_SCANCODE_RETURN);                                  // title: Start New Game
		pushKeys(SDL_SCANCODE_DOWN, gameplayMenuSteps(GAMEPLAY_FULL_GAME));
		pushKey(SDL_SCANCODE_RETURN);                                  // gameplay: Full Game
		pushKeys(SDL_SCANCODE_DOWN, flowEpisode - 1);
		pushKey(SDL_SCANCODE_RETURN);                                  // the episode
		pushKey(SDL_SCANCODE_RETURN);                                  // difficulty: the default
		flowFillEnter = true;
	}
	else if (flowIs("list-levels"))
	{
		flowEpisode = flowParam("ep", 1);
	}
	else
	{
		logFatal("Unknown --regress-flow '%s'.", regress_flow);
		exit(EXIT_FAILURE);
	}

	// A flow ends on its own; the cap is a backstop against a flow that gets lost.
	if (regress_frames == 0)
		regress_frames = 400000;
}

// The keys that type a secret code at the title screen, then walk its menus.
static void buildCodeKeys(void)
{
	const GameArcadeRules *const arcade = gameRules()->arcade;

	switch (flowCode)
	{
	case FLOW_CODE_SHIP:
		pushTyped(specialName[flowShip - 1]);
		pushKey(SDL_SCANCODE_RETURN);                                  // the episode (first)
		pushKey(SDL_SCANCODE_RETURN);                                  // difficulty: the default
		pushKey(SDL_SCANCODE_RETURN);                                  // the ship's picture
		break;
	case FLOW_CODE_SUPER_TYRIAN:
		pushTyped(specialName[arcade->engage_code - 1]);
		pushKey(SDL_SCANCODE_RETURN);                                  // the introduction
		if (arcade->super_tyrian_pick_episode)
		{
			pushKeys(SDL_SCANCODE_DOWN, flowPickEpisode - 1);
			pushKey(SDL_SCANCODE_RETURN);                              // the starting episode
		}
		break;
	case FLOW_CODE_DESTRUCT:
		pushTyped(specialName[arcade->destruct_code - 1]);
		break;
	default:
		break;
	}
}

// The sections of an episode that play a level.
static void listLevels(void)
{
	JE_initEpisode((JE_byte)flowEpisode);

	File file = dataFileOpen(episodeFilename, "rb");
	if (file.error)
	{
		logFatal("Failed to open file '%s': %s", episodeFilename, fileGetError(&file));
		exit(EXIT_FAILURE);
	}

	unsigned int section = 0;
	bool hasLevel = false;
	char line[256];

	for (; ; )
	{
		readEncryptedString(&file, line, sizeof line);
		if (file.error)
			break;

		if (line[0] == '*')
		{
			if (hasLevel)
				printf("%d:%u\n", flowEpisode, section);
			++section;
			hasLevel = false;
		}
		else if (line[0] == ']' && line[1] == 'L')
		{
			hasLevel = true;
		}
	}
	if (hasLevel)
		printf("%d:%u\n", flowEpisode, section);

	fileClose(&file);
}

void regress_flow_run(void)
{
	// Everything a flow does must be the same every time.
	mt_srand(0x2000);
	opentyrian_version = "regress";

	if (flowIs("list-levels"))
	{
		listLevels();
		regress_finish();
		exit(EXIT_SUCCESS);
	}

	// The main loop of main(), from the title screen on.
	regress_frame_reset();

	JE_initPlayerData();
	JE_sortHighScores();

	playDemo = false;
	stoppedDemo = false;

	gameLoaded = false;
	jumpSection = false;

	buildCodeKeys();

	if (!titleScreen())
	{
		logError("The flow left the title screen by quitting.");
		exit(EXIT_FAILURE);
	}
	if (loadDestruct && flowCode == FLOW_CODE_DESTRUCT)
	{
		// The intro screen wants a key, the mode menu backs out with Escape.
		regress_flow_coverage("Destruct started from the title.");
		pushKey(SDL_SCANCODE_RETURN);
		pushKey(SDL_SCANCODE_ESCAPE);
		flowFeedPolls = true;   // Destruct polls the keyboard itself, and its fades clear the queue
		JE_destructGame();
		regress_flow_coverage("Destruct ran to its mode menu and back.");
		regress_finish();
		exit(EXIT_SUCCESS);
	}
	if (loadDestruct || playDemo)
	{
		logError("The flow left the title screen for %s.", loadDestruct ? "Destruct" : "the demo");
		exit(EXIT_FAILURE);
	}

	JE_main();

	regress_flow_coverage("the game returned to the title (%lu of %lu keys used).", (unsigned long)flowKeyNext, (unsigned long)flowKeyCount);
	regress_finish();
	exit(EXIT_SUCCESS);
}

// --- In the level -------------------------------------------------------------

void regress_flow_level_begin(void)
{
	if (!regress_flow_active())
		return;

	flowTick = 0;
	++flowLevels;
	regress_flow_coverage("level begins: episode %u section %u.", (unsigned)episodeNum, (unsigned)mainLevel);

	if (flowCash != 0)
		player[0].cash += (unsigned long)flowCash;
	if (flowDieTicks == 0 && flowIs("battle"))
		youAreCheating = true;   // a battle that is not about dying does not end early
}

bool regress_flow_level_tick(void)
{
	if (!regress_flow_active())
		return false;

	++flowTick;

	if (flowDieTicks != 0 && flowTick == flowDieTicks)
	{
		regress_flow_coverage("player killed after %d ticks.", flowTick);
		youAreCheating = false;
		player[0].shield = 0;
		player[0].armor = 1;
		JE_playerDamage(255, &player[0]);
	}

	if (flowLevelsStop != 0 && flowLevels >= flowLevelsStop && flowTick == 40)
	{
		regress_flow_coverage("flow complete: level %d ran (arcade mode %u, ship %u).", flowLevels,
		                      (unsigned)superArcadeMode, (unsigned)player[0].items.ship);
		regress_finish();
		exit(EXIT_SUCCESS);
	}

	return flowLevelTicks != 0 && flowTick == flowLevelTicks;
}

void regress_flow_episode_end(unsigned int episode)
{
	regress_flow_coverage("episode %u end ran.", episode);
}

void regress_flow_episode_next(unsigned int from, unsigned int to)
{
	regress_flow_coverage("episode %u -> episode %u set up.", from, to);

	if (flowEpisode != 0 && (int)from >= flowLastEpisode)
	{
		regress_flow_coverage("flow complete.");
		regress_finish();
		exit(EXIT_SUCCESS);
	}
}

void regress_flow_coverage(const char *format, ...)
{
	if (!regress_flow_active())
		return;

	char text[200];
	va_list args;

	va_start(args, format);
	vsnprintf(text, sizeof text, format, args);
	va_end(args);

	logInfo("Flow coverage: %s", text);
}
