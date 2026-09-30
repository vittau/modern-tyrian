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
#ifndef CONFIG_H
#define CONFIG_H

#include "opentyr.h"
#include "config_file.h"

#include <SDL3/SDL.h>

#define SAVE_FILES_NUM (11 * 2)

enum
{
	DIFFICULTY_WIMP = 0,
	DIFFICULTY_EASY,
	DIFFICULTY_NORMAL,
	DIFFICULTY_HARD,
	DIFFICULTY_IMPOSSIBLE,
	DIFFICULTY_INSANITY,
	DIFFICULTY_SUICIDE,
	DIFFICULTY_MANIACAL,
	DIFFICULTY_ZINGLON,  // aka Lord of the Game
	DIFFICULTY_NORTANEOUS,
	DIFFICULTY_10,
};

// NOTE: Do not reorder.  This ordering corresponds to the keyboard
//       configuration menu and to the bits stored in demo files.
enum
{
	KEY_SETTING_UP,
	KEY_SETTING_DOWN,
	KEY_SETTING_LEFT,
	KEY_SETTING_RIGHT,
	KEY_SETTING_FIRE,
	KEY_SETTING_CHANGE_FIRE,
	KEY_SETTING_LEFT_SIDEKICK,
	KEY_SETTING_RIGHT_SIDEKICK,
};

typedef SDL_Scancode KeySettings[8];

typedef JE_byte JE_PItemsType[12]; /* [1..12] */

typedef JE_byte JE_EditorItemAvailType[100]; /* [1..100] */

typedef struct
{
	JE_word       encode;
	JE_word       level;
	JE_PItemsType items;
	JE_longint    score;
	JE_longint    score2;
	char          levelName[11]; /* string [9]; */ /* SYN: Added one more byte to match lastLevelName below */
	JE_char       name[15]; /* [1..14] */ /* SYN: Added extra byte for null */
	JE_byte       cubes;
	JE_byte       power[2]; /* [1..2] */
	JE_byte       episode;
	JE_PItemsType lastItems;
	JE_byte       difficulty;
	JE_byte       secretHint;
	JE_byte       input1;
	JE_byte       input2;
	JE_boolean    gameHasRepeated; /*See if you went from one episode to another*/
	JE_byte       initialDifficulty;

	/* High Scores - Each episode has both sets of 1&2 player selections - with 3 in each */
	JE_longint    highScore1;
	JE_longint    highScore2;  // unused
	char          highScoreName[30]; /* string [29] */
	JE_byte       highScoreDiff;
} JE_SaveFileType;

typedef JE_SaveFileType JE_SaveFilesType[SAVE_FILES_NUM]; /* [1..savefilesnum] */

// Tyrian 2000 appends unencrypted high-score boards to tyrian.sav: 10 Timed
// Battle boards, then 10 main-game boards (one per episode and player count),
// each with three entries.  Tyrian 2.1 has none.
#define VARIANT_SCORE_BOARDS 20
#define VARIANT_SCORE_TIMED_BOARDS 10
#define VARIANT_SCORE_ENTRIES 3
#define VARIANT_SCORE_NAME_MAX 29

typedef struct
{
	JE_longint score;
	// Main-game entries carry an unknown 4-byte field between score and name;
	// it is kept as read and written back (Timed Battle entries have none).
	Uint32     unknown;
	char       playerName[VARIANT_SCORE_NAME_MAX + 1];
	JE_byte    difficulty;
} VariantHighScore;

extern VariantHighScore variantHighScores[VARIANT_SCORE_BOARDS][VARIANT_SCORE_ENTRIES];

// GameVariant last started from the launcher (opentyrian.cfg), or -1.
extern int launcherLastVariant;

extern const KeySettings defaultKeySettings;
extern JE_boolean smoothies[9];
extern JE_byte starShowVGASpecialCode;
extern JE_word lastCubeMax, cubeMax;
extern JE_word cubeList[4];
extern JE_boolean gameHasRepeated;
extern JE_shortint difficultyLevel, oldDifficultyLevel, initialDifficulty;
extern uint power, lastPower, powerAdd;
extern JE_byte shieldWait, shieldT;

enum
{
	SHOT_FRONT,
	SHOT_REAR,
	SHOT_LEFT_SIDEKICK,
	SHOT_RIGHT_SIDEKICK,
	SHOT_MISC,
	SHOT_P2_CHARGE,
	SHOT_P1_SUPERBOMB,
	SHOT_P2_SUPERBOMB,
	SHOT_SPECIAL,
	SHOT_NORTSPARKS,
	SHOT_SPECIAL2
};

extern JE_byte shotRepeat[11], shotMultiPos[11];
extern JE_boolean portConfigChange, portConfigDone;
extern char lastLevelName[11], levelName[11];
extern JE_byte mainLevel, nextLevel, saveLevel;
extern KeySettings keySettings;
extern JE_shortint levelFilter, levelFilterNew, levelBrightness, levelBrightnessChg;
extern JE_boolean filtrationAvail, filterActive, filterFade, filterFadeStart;
extern JE_boolean gameJustLoaded;
extern JE_boolean galagaMode;
extern JE_boolean extraGame;
extern JE_boolean twoPlayerMode, twoPlayerLinked, onePlayerAction, superTyrian, trentWin;
extern JE_byte superArcadeMode;
extern bool timedBattleMode;  // Tyrian 2000 Timed Battle; false in every other mode
extern JE_byte timeBattleSelection;  // the battle picked from the menu, 1-based (meaningful in Timed Battle only)
extern JE_byte superArcadePowerUp;
extern JE_real linkGunDirec;
extern JE_byte inputDevice[2];
extern JE_byte secretHint;
extern JE_byte background3over;
extern JE_byte background2over;
extern JE_byte gammaCorrection;
extern JE_boolean superPause, explosionTransparent, youAreCheating, displayScore, background2, smoothScroll, wild, superWild, starActive, topEnemyOver, skyEnemyOverAll, background2notTransparent;
extern JE_byte fastPlay;
extern JE_boolean pentiumMode;
extern JE_byte gameSpeed;
extern JE_byte processorType;
// The player's stored detail-level choice, saved to and loaded from tyrian.cfg.
// Classic renders at exactly this level.  Modern ignores it and always renders
// at Pentium (4) -- or 6 if the SuperWild cheat is active -- so the detail can
// never be set below Pentium; processorType is the effective level actually
// used by the renderer.  See JE_applyProcessorType().
extern JE_byte processorTypeChoice;
extern JE_SaveFilesType saveFiles;
extern JE_EditorItemAvailType editorItemAvail;
extern JE_word editorLevel;

extern Config opentyrian_config;

void JE_initProcessorType(void);
// Recomputes processorType (the effective render detail) from the stored
// processorTypeChoice and the active presentation, then re-derives the
// detail-dependent flags via JE_initProcessorType().  In Modern the effective
// level is pinned to Pentium (4), or 6 while the SuperWild cheat is active, so
// Modern never renders below Pentium; Classic uses the player's choice
// unchanged.  Safe to call on every presentation/detail change and at startup.
void JE_applyProcessorType(void);
void JE_setNewGameSpeed(void);

void loadConfiguration(void);
void saveConfiguration(void);

void loadSaves(void);
void saveSaves(void);

void JE_saveGame(JE_byte slot, const char *name);
void JE_loadGame(JE_byte slot);

#endif /* CONFIG_H */
