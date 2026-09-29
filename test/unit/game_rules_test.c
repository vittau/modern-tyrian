/*
 * Checks of the per-variant rule tables (src/game_rules.c) and of the high-score
 * boards (src/highscores.c) that need no game data.  Built and run by
 * tools/check_game_rules.sh, which regress.sh calls.
 */
#include "../../src/config.h"
#include "../../src/game_rules.h"
#include "../../src/game_schema.h"
#include "../../src/highscores.h"
#include "../../src/sndmast.h"

#include <stdio.h>
#include <string.h>

// What highscores.c reads of config.c.
JE_SaveFilesType saveFiles;
VariantHighScore variantHighScores[VARIANT_SCORE_BOARDS][VARIANT_SCORE_ENTRIES];

#define KEYBOARD_COMBOS 26  // rows of keyboardCombos in varz.c
#define NORTSHIPZ 7         // SA_NORTSHIPZ in varz.h: the arcade ship whose code ends the chain

static int failures = 0;

static void check(int ok, const char *what)
{
	if (!ok)
	{
		printf("FAIL %s: %s\n", gameVariantCurrent()->display_name, what);
		++failures;
	}
}

static void check_events(void)
{
	const GameRules *rules = gameRules();

	for (unsigned int type = 0; type < 256; ++type)
	{
		unsigned int mapped = 0;

		for (size_t i = 0; i < rules->event_count; ++i)
			if (rules->events[i].type == type)
				mapped = rules->events[i].action;

		check(gameEventCase(type) == (mapped != 0 ? mapped : type), "event case is the mapped action, or the type itself");
	}

	for (size_t i = 0; i < rules->event_count; ++i)
	{
		for (size_t j = i + 1; j < rules->event_count; ++j)
			check(rules->events[i].type != rules->events[j].type, "an event type is mapped once");
		check(rules->events[i].action < 256 || rules->events[i].action >= GAME_EVENT_SET_ENEMY_LAUNCH,
		      "an event action is a type or a GameEventAction");
	}
}

static void check_launch(void)
{
	uint16_t type;
	uint8_t special;

	// 2.1: always split.  2000: whole value above 1000 only (1000 itself splits).
	const int full = gameVariantCurrent()->id == VARIANT_TYRIAN2000;

	gameEnemyLaunch(1001, 1234, &type, &special);
	check(full ? (type == 1234 && special == 0) : (type == 234 && special == 1), "launch of a high enemy ID");
	gameEnemyLaunch(1000, 3456, &type, &special);
	check(type == 456 && special == 3, "launch of enemy 1000 is split");
	gameEnemyLaunch(0, 7, &type, &special);
	check(type == 7 && special == 0, "a small launch type is unchanged");
	gameEnemyLaunch(65535, 2005, &type, &special);
	check(full ? (type == 2005 && special == 0) : (type == 5 && special == 2), "launch of the last ID");
}

static void check_arcade(void)
{
	const GameDataSchema *schema = gameSchema();
	const GameStringSchema *strings = gameStrings();
	const GameArcadeRules *arcade = gameRules()->arcade;
	const unsigned int ships = arcade->ship_count;

	check(ships >= 7 && ships <= ARCADE_SHIPS_MAX, "arcade ship count within capacity");
	check(arcade->destruct_code == ships + 1 && arcade->engage_code == ships + 2, "the title codes follow the ships");
	check(arcade->engage_code <= strings->special_name, "every title code has a name");
	check(strings->super_ships >= ships + 4, "superShips holds the header, one name per ship and three labels");
	check(arcade->super_tyrian_win_state <= ships + 1, "the Super Tyrian win state is a state of the table");
	check(arcade->super_tyrian_win_state == 8, "the fork's Zinglon win state is preserved");

	for (unsigned int state = 0; state < arcade->engage_code; ++state)
	{
		const unsigned int next = arcade->next_ship[state];

		check(next >= 1 && next <= arcade->engage_code, "next ship is a ship or a title code");
	}

	for (unsigned int i = 0; i < ships; ++i)
	{
		check(arcade->ship[i] <= schema->ship_max, "arcade ship exists");
		check(arcade->special[i] <= schema->special_max, "arcade special exists");
		check(arcade->special_b[i] <= schema->special_max, "arcade alternate special exists");

		for (unsigned int w = 0; w < 5; ++w)
			check(gameWeaponValid(arcade->weapon[i][w]), "arcade weapon exists in the item data");
	}

	check(arcade->engage_voice < VOICE_COUNT && arcade->super_tyrian_voice < VOICE_COUNT, "voices exist");
}

static void check_combos(void)
{
	const GameRules *rules = gameRules();
	const GameDataSchema *schema = gameSchema();

	check(rules->ship_combo_count == (size_t)schema->ship_max + 1u, "one twiddle row per ship");

	for (size_t ship = 0; ship < rules->ship_combo_count; ++ship)
		for (int i = 0; i < 3; ++i)
			check(rules->ship_combos[ship][i] <= KEYBOARD_COMBOS, "a twiddle names an existing combo");

	check(gameShipCombos((unsigned int)rules->ship_combo_count)[0] == 0 &&
	      gameShipCombos(100000)[2] == 0, "no twiddles past the last ship");
	check(gameShipCombos(1)[0] == 1 && gameShipCombos(1)[1] == 2, "USP Talon twiddles");
}

static void check_shots(void)
{
	const int t2k = gameVariantCurrent()->id == VARIANT_TYRIAN2000;

	check(gameIsSmokeTrail(98), "trail 98 is smoke");
	check(gameIsSmokeTrail(198) == t2k, "trail 198 is smoke only where the variant says");
	check(!gameIsSmokeTrail(6) && !gameIsSmokeTrail(-1), "ordinary/invalid trails are not smoke");

	check(gameShotTrail(198, 1) == 198, "the first tile keeps trail 198");
	check(gameShotTrail(198, 2) == (t2k ? 255 : 198), "later tiles lose trail 198 only where the variant says");
	check(gameShotTrail(98, 3) == 98 && gameShotTrail(255, 3) == 255, "other trails are untouched");

	check(gameSidekickMainFire(0), "a sidekick without charge stages fires on main fire");
	check(gameSidekickMainFire(3) == !t2k, "a charging sidekick fires on main fire only where the variant says");
	check(gameRules()->explosion_count == (t2k ? 54 : 53), "explosion table size");
}

static void check_cash(void)
{
	const int episodes = gameVariantCurrent()->episode_count;

	check(gameRules()->initial_cash[0] == 10000 && gameRules()->initial_cash[3] == 30000, "initial cash of episodes 1 and 4");
	check(episodes < 5 || gameRules()->initial_cash[4] == 20000, "initial cash of episode 5");
}

static void check_highscores(void)
{
	const int t2k = gameVariantCurrent()->id == VARIANT_TYRIAN2000;
	const unsigned int episodes = highScoreEpisodes();

	check(episodes == (t2k ? 5u : 3u), "episodes with boards");
	check(gameRules()->final_episode_score == t2k, "only 2000 scores final-episode completion");

	memset(saveFiles, 0, sizeof saveFiles);
	memset(variantHighScores, 0, sizeof variantHighScores);

	// Every (episode, players) board is a distinct place in storage.
	for (unsigned int episode = 1; episode <= episodes; ++episode)
	{
		for (int two = 0; two < 2; ++two)
		{
			const unsigned int board = highScoreBoard(episode, two != 0);

			for (unsigned int rank = 0; rank < HIGH_SCORE_ENTRIES; ++rank)
				highScoreSet(board, rank, (JE_longint)(episode * 100000 + two * 10000 + rank), "name", (JE_byte)episode);
		}
	}
	for (unsigned int episode = 1; episode <= episodes; ++episode)
	{
		for (int two = 0; two < 2; ++two)
		{
			const unsigned int board = highScoreBoard(episode, two != 0);

			for (unsigned int rank = 0; rank < HIGH_SCORE_ENTRIES; ++rank)
			{
				check(highScoreValue(board, rank) == (JE_longint)(episode * 100000 + two * 10000 + rank), "board entries do not overlap");
				check(highScoreDifficulty(board, rank) == episode, "difficulty is stored");
			}
		}
	}

	// Sorting puts a board in descending order and moves names with scores.
	const unsigned int board = highScoreBoard(1, false);
	highScoreSet(board, 0, 5, "low", 1);
	highScoreSet(board, 1, 900, "high", 2);
	highScoreSet(board, 2, 70, "mid", 3);
	highScoreSortAll();
	check(highScoreValue(board, 0) == 900 && highScoreValue(board, 1) == 70 && highScoreValue(board, 2) == 5, "sorted by score");
	check(strcmp(highScoreName(board, 0), "high") == 0 && strcmp(highScoreName(board, 2), "low") == 0, "names follow scores");
	check(highScoreDifficulty(board, 0) == 2 && highScoreDifficulty(board, 1) == 3 && highScoreDifficulty(board, 2) == 1, "difficulties follow scores");

	// Making room moves entries down and drops the last one.  Tyrian 2.1 leaves
	// the difficulty where it is; Tyrian 2000 moves the whole record.
	highScoreShiftDown(board, 0);
	check(highScoreValue(board, 0) == 900 && highScoreValue(board, 1) == 900 && highScoreValue(board, 2) == 70, "shift down");
	check(strcmp(highScoreName(board, 2), "mid") == 0, "names shift with scores");
	check(highScoreDifficulty(board, 1) == (t2k ? 2 : 3), "difficulty shift depends on the variant");

	highScoreSet(board, 0, 1000, "new", 4);
	check(highScoreValue(board, 0) == 1000 && strcmp(highScoreName(board, 0), "new") == 0 && highScoreDifficulty(board, 0) == 4, "set");

	// A long name is cut, not overrun (both variants' boards hold 29 characters).
	highScoreSet(board, 1, 1, "0123456789012345678901234567890123456789", 1);
	check(strlen(highScoreName(board, 1)) == 29, "a long name is cut to the board's width");

	// Tyrian 2000's main boards live after its Timed Battle boards.
	if (t2k)
	{
		variantHighScores[0][0].score = 77;
		check(highScoreValue(highScoreBoard(1, false), 0) == 1000, "Timed Battle boards are not episode boards");
		highScoreSortAll();
		check(variantHighScores[0][0].score == 77, "a sorted board of equal scores stays");
	}
}

int main(void)
{
	static const GameVariant variants[] = { VARIANT_TYRIAN21, VARIANT_TYRIAN2000 };

	for (size_t i = 0; i < sizeof variants / sizeof *variants; ++i)
	{
		gameVariantSelect(variants[i]);

		check_events();
		check_launch();
		check_arcade();
		check_combos();
		check_shots();
		check_cash();
		check_highscores();
	}

	gameVariantSelect(VARIANT_TYRIAN21);
	// 2.1 changes nothing: an empty event map, split launches, no random X.
	check(gameRules()->event_count == 0 && !gameRules()->spawn_random_x, "2.1 keeps the historical event rules");
	check(gameEventCase(68) == 68 && gameEventCase(99) == 99 && gameEventCase(83) == 83, "2.1 event 68 is still random explosions");

	gameVariantSelect(VARIANT_TYRIAN2000);
	check(gameEventCase(68) == GAME_EVENT_REPLACE_ENEMY && gameEventCase(99) == 68 && gameEventCase(83) == 4, "2000 event 68, 99 and 83");
	check(gameRules()->spawn_random_x && gameRules()->spawn_random_x_sentinel == -200 &&
	      gameRules()->spawn_random_x_min == 24 && gameRules()->spawn_random_x_span == 208, "2000 random X range 24..231");

	if (failures != 0)
	{
		printf("%d game-rules check(s) failed.\n", failures);
		return 1;
	}
	printf("game rules: ok\n");
	return 0;
}
