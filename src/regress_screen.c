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
// --regress-screen: render one non-gameplay screen in a deterministic state,
// present REGRESS_SCREEN_FRAMES (or --regress-frames) frames and exit.  This is
// the regress-only guard for the widescreen non-gameplay compositions: it draws
// the screens with the game's own functions, so a change to the compositor or to
// a screen's layout shows up as a frame-hash difference.
//
// The screens themselves are interactive loops that normally block on input.
// In screen mode hasInput() reports input as available (src/keyboard.c), so the
// loops keep redrawing, and the --regress-frames cap ends the run.  No real
// input ever reaches the menu logic (handleSdlEvents() drops it in regress
// mode), so the rendered screen does not advance.
#include "regress.h"

#include "config.h"
#include "episodes.h"
#include "game_menu.h"
#include "jukebox.h"
#include "logging.h"
#include "mainint.h"
#include "menus.h"
#include "mtrand.h"
#include "opentyr.h"
#include "palette.h"
#include "picload.h"
#include "tyrian2.h"
#include "video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Representative, deterministic save data for the high-score screen (the real
// save files are not loaded in regress mode).
static void seed_high_scores(void)
{
	static const struct
	{
		const char *name;
		JE_longint score;
		JE_byte difficulty;
	} entries[6] =
	{
		{ "JACK", 123456, 0 },
		{ "JILL",  98765, 1 },
		{ "ZAN",   55555, 2 },
		{ "ACE",   43210, 0 },
		{ "BOB",   21000, 1 },
		{ "KAT",    9999, 2 },
	};

	for (size_t i = 0; i < 6; ++i)
	{
		snprintf(saveFiles[i].highScoreName, sizeof saveFiles[i].highScoreName, "%s", entries[i].name);
		saveFiles[i].highScore1 = entries[i].score;
		saveFiles[i].highScoreDiff = entries[i].difficulty;
	}
}

// A few populated save slots, so the load/save screen draws its per-slot name,
// level name and the episode text at x~297 (the element the S1 fallback
// analysis asks about) instead of only empty slots.
static void seed_save_slots(void)
{
	static const struct
	{
		const char *name;
		const char *level;
	} entries[3] =
	{
		{ "ALPHA", "TYRIAN 1" },
		{ "BETA",  "ASTEROID" },
		{ "GAMMA", "SAVARA" },
	};

	for (size_t i = 0; i < 3; ++i)
	{
		saveFiles[i].level = 1;
		saveFiles[i].episode = 1;
		snprintf(saveFiles[i].name, sizeof saveFiles[i].name, "%s", entries[i].name);
		snprintf(saveFiles[i].levelName, sizeof saveFiles[i].levelName, "%s", entries[i].level);
	}
}

void regress_screen_run(void)
{
	// main() seeded the RNG from the wall clock; pin it so JE_initPlayerData()'s
	// random draws cannot leak into the rendered frames.
	mt_srand(32402394);

	// The title screen draws the build version; pin it so the title baselines
	// do not change on every commit.
	opentyrian_version = "regress";

	// Loads the item tables and the cube filename the shop screens read.
	JE_initEpisode(1);

	JE_initPlayerData();
	JE_sortHighScores();

	// A little cash so the upgrade/purchase list shows real costs and the
	// affordability shading is exercised.
	player[0].cash = 12345;

	seed_high_scores();
	seed_save_slots();

	// The data-cube screens read cubeMax/cubeList (normally set from a save or
	// the level script).  load_cube() treats the entry as "skip this many marker
	// lines", so cube 1 (the first entry in cubetxt1.dat) is index 1.
	cubeMax = 1;
	cubeList[0] = 1;

	// Start this run's own frame timeline; snapshots count from 0.
	regress_frame_reset();

	const char *name = regress_screen;

	if (strcmp(name, "title") == 0)
	{
		titleScreen();
	}
	else if (strcmp(name, "episode-select") == 0)
	{
		episodeSelect();
	}
	else if (strcmp(name, "high-scores") == 0)
	{
		JE_highScoreScreen();
	}
	else if (strcmp(name, "game-menu") == 0)
	{
		JE_itemScreenStartAt(MENU_FULL_GAME, 0);
		JE_itemScreen();
	}
	else if (strcmp(name, "upgrade") == 0)
	{
		JE_itemScreenStartAt(MENU_UPGRADES, 0);
		JE_itemScreen();
	}
	else if (strcmp(name, "purchase") == 0)
	{
		// Category 3 is "front weapon" (see JE_menuFunction); the purchase list
		// then holds the available front weapons and their costs.
		JE_itemScreenStartAt(MENU_UPGRADE_SUB, 3);
		JE_itemScreen();
	}
	else if (strcmp(name, "options") == 0)
	{
		JE_itemScreenStartAt(MENU_OPTIONS, 0);
		JE_itemScreen();
	}
	else if (strcmp(name, "cube-list") == 0)
	{
		JE_itemScreenStartAt(MENU_DATA_CUBES, 0);
		JE_itemScreen();
	}
	else if (strcmp(name, "cube-reader") == 0)
	{
		JE_itemScreenStartAt(MENU_DATA_CUBE_SUB, 0);
		JE_itemScreen();
	}
	else if (strcmp(name, "keyboard") == 0)
	{
		JE_itemScreenStartAt(MENU_KEYBOARD_CONFIG, 0);
		JE_itemScreen();
	}
	else if (strcmp(name, "joystick") == 0)
	{
		JE_itemScreenStartAt(MENU_JOYSTICK_CONFIG, 0);
		JE_itemScreen();
	}
	else if (strcmp(name, "load-save") == 0)
	{
		// Menu 6 with performSave false is the load screen (its slot rows carry
		// the x~297 episode text the fallback analysis asks about).
		performSave = false;
		JE_itemScreenStartAt(MENU_LOAD_SAVE, 0);
		JE_itemScreen();
	}
	else if (strcmp(name, "solid") == 0)
	{
		// A story picture (pic 5, flat black edges): the compositor uses the
		// solid edge fill.  This mirrors the level event that shows it.
		JE_loadPic(VGAScreen, 5, true);
		for (;;)
			JE_showVGA();
	}
	else if (strcmp(name, "setup") == 0)
	{
		// The in-game Setup screen's Graphics submenu, which carries the
		// presentation/aspect/pixel-aspect/smooth-motion pickers.
		setupMenuStartAt(SETUP_MENU_GRAPHICS);
		setupMenu();
	}
	else if (strcmp(name, "nav-map") == 0)
	{
		// The short route and the palette are seeded inside JE_itemScreen()
		// (after its menu-choice and palette reset), so the frame matches the
		// real nav screen.
		JE_itemScreenStartAt(MENU_PLAY_NEXT_LEVEL, 0);
		JE_itemScreen();
	}
	else if (strcmp(name, "ship-specs") == 0)
	{
		// JE_doShipSpecs() is normally called from JE_itemScreen, which has
		// already loaded pic 1 and set its palette; do the same here.
		JE_loadPic(VGAScreen, 1, false);
		set_palette(colors, 0, 255);

		// In Classic JE_doShipSpecs() plays the zoom-in animation and presents
		// many frames; in Modern wide it draws one frame into the canvas-wide
		// scratch.  Loop until the frame cap exits.
		for (;;)
			JE_doShipSpecs();
	}
	else if (strcmp(name, "jukebox") == 0)
	{
		jukebox();
	}
	else if (strcmp(name, "weapon-sim") == 0)
	{
		// Category 3 (front weapon) opens the upgrade submenu with the weapon
		// simulator in the left window.
		JE_itemScreenStartAt(MENU_UPGRADE_SUB, 3);
		JE_itemScreen();
	}
	else if (strcmp(name, "credits") == 0)
	{
		// The credits loop breaks on input; in regress mode input is reported
		// immediately, so hold it back and let the frame cap end the run on a
		// deterministic mid-scroll frame.
		JE_playCreditsRegressHold(true);
		JE_playCredits();
	}
	else
	{
		logFatal("Unknown --regress-screen '%s'.", name);
		exit(EXIT_FAILURE);
	}

	// Every screen above loops until the --regress-frames cap ends the run.
	logFatal("Regression screen '%s' returned before its frame cap.", name);
	exit(EXIT_FAILURE);
}
