/* OpenTyrian gameplay regression fixtures, GPL-2.0-or-later.
 * These construct their own events/input; no original level records or assets.
 */
#include "regress_rules.h"
#include "config.h"
#include "demo.h"
#include "episodes.h"
#include "game_rules.h"
#include "game_schema.h"
#include "keyboard.h"
#include "joystick.h"
#include "logging.h"
#include "mainint.h"
#include "mtrand.h"
#include "player.h"
#include "regress.h"
#include "shots.h"
#include "tyrian2.h"
#include "video.h"

#include <stdlib.h>
#include <string.h>

extern struct JE_EventRecType eventRec[];
const char *regress_rule_fixture;

static void require(bool ok, const char *what)
{
	if (!ok)
	{
		logError("Rule fixture failed: %s", what);
		exit(EXIT_FAILURE);
	}
}

static void event(unsigned int type, int data, unsigned int link)
{
	memset(&eventRec[0], 0, sizeof eventRec[0]);
	eventRec[0].eventtype = type;
	eventRec[0].eventdat = data;
	eventRec[0].eventdat4 = link;
	eventLoc = 1;
	JE_eventSystem();
}

static void events_fixture(void)
{
	// 2.1 is tested with the same engine entry point, not just the dispatch map.
	if (gameEventCase(68) == 68)
	{
		event(68, 1, 0);
		require(randomExplosions, "2.1 event 68 enables random explosions");
		logInfo("Rule coverage: 2.1 event 68 random explosions ran.");
		return;
	}

	memset(enemy, 0, sizeof enemy);
	memset(enemyAvail, 1, sizeof enemyAvail);
	enemy[0].linknum = 7;
	enemy[99].linknum = 7;
	event(58, 1234, 7);
	require(enemy[0].launchtype == 1234 && enemy[99].launchtype == 1234 && enemy[1].launchtype == 0,
	        "58 matches links, including free slots and slot 99");
	event(58, 4321, 99);
	for (int i = 0; i < 100; ++i)
		require(enemy[i].launchtype == 4321, "58 wildcard touches all 100 slots");

	// A code-owned enemy with no graphics/RNG dependencies keeps allocation
	// assertions independent of which art bank the chosen real level loads.
	unsigned char saved[sizeof enemyDat[1]];
	memcpy(saved, &enemyDat[1], sizeof saved);
	memset(&enemyDat[1], 0, sizeof enemyDat[1]);
	enemyDat[1].armor = 1;
	memset(enemy, 0, sizeof enemy);
	memset(enemyAvail, 0, sizeof enemyAvail);
	enemy[30].linknum = 7;
	enemy[30].ex = 77; enemy[30].ey = 88;
	enemyAvail[26] = 1;
	event(68, 1, 7);
	require(enemyAvail[30] == 1 && enemyAvail[26] != 1 && enemy[26].ex == 77 && enemy[26].ey == 88,
	        "68 allocates in source group and copies position");
	const unsigned int before = totalEnemy;
	event(59, 1, 7);
	require(totalEnemy == before + 1 && enemyAvail[30] == 1,
	        "replacement processes a matching free source slot too");
	// No room: the source still gets freed.
	memset(enemyAvail, 0, sizeof enemyAvail);
	enemy[30].linknum = 7;
	event(59, 1, 7);
	require(enemyAvail[30] == 1, "59 frees source after allocation failure");
	memset(enemy, 0, sizeof enemy);
	memset(enemyAvail, 0, sizeof enemyAvail);
	event(59, 1, 0);
	require(enemyAvail[24] == 1 && enemyAvail[49] == 1 && enemyAvail[74] == 1 && enemyAvail[99] == 1,
	        "replacement wildcard 0 processes every group");
	memcpy(&enemyDat[1], saved, sizeof saved);
	memset(enemyAvail, 1, sizeof enemyAvail);
	memset(enemy, 0, sizeof enemy);
	unsigned char saved_high[sizeof enemyDat[1001]];
	memcpy(saved_high, &enemyDat[1001], sizeof saved_high);
	memset(&enemyDat[1001], 0, sizeof enemyDat[1001]);
	enemyDat[1001].armor = 1;
	enemyDat[1001].elaunchtype = 1234;
	struct JE_SingleEnemyType probe;
	JE_makeEnemy(&probe, 1001, 0);
	require(probe.launchtype == 1234 && probe.launchspecial == 0,
	        "second-bank enemy creation retains full launch ID");
	memcpy(&enemyDat[1001], saved_high, sizeof saved_high);
	logInfo("Rule coverage: second-bank enemy launch ran.");

	event(83, 1, 0);
	require(stopBackgrounds && stopBackgroundNum == 1, "83 aliases map stop");
	stopBackgrounds = false;
	stopBackgroundNum = 0;
	levelTimer = false; levelTimerCountdown = 123; levelTimerJumpTo = 456;
	event(84, 1, 0);
	require(!levelTimer && levelTimerCountdown == 123 && levelTimerJumpTo == 456,
	        "84 is inert outside Timed Battle");
	enemy[0].linknum = 7; enemy[0].enemydie = 42;
	event(85, 55, 7);
	require(enemy[0].enemydie == 42, "85 is inert outside Timed Battle");
	event(99, 1, 0);
	require(randomExplosions, "99 enables random explosions");
	event(99, 0, 0);
	require(!randomExplosions, "99 disables random explosions");
	event(99, 1, 0); // leave enabled so the following real ticks exercise it
	logInfo("Rule coverage: event 99 random explosions ran.");
}

static void spawn_fixture(void)
{
	require(gameRules()->spawn_random_x, "spawn fixture requires sentinel policy");
	unsigned char saved[sizeof enemyDat[1]];
	memcpy(saved, &enemyDat[1], sizeof saved);
	memset(&enemyDat[1], 0, sizeof enemyDat[1]);
	enemyDat[1].armor = 1;
	memset(&eventRec[0], 0, sizeof eventRec[0]);
	eventRec[0].eventdat = 1;
	eventRec[0].eventdat2 = -200;
	eventLoc = 1;
	memset(enemyAvail, 1, sizeof enemyAvail);
	mt_srand(919);
	struct JE_SingleEnemyType probe;
	JE_makeEnemy(&probe, 1, 0);
	const unsigned int expected = mt_rand() % 208 + 24;
	const unsigned long long expected_rng = mt_rand_state_hash();
	mt_srand(919);
	JE_createNewEventEnemy(0, 0, 0);
	require(b == 1 && eventRec[0].eventdat2 == (int)expected && mt_rand_state_hash() == expected_rng,
	        "-200 mutates event with exactly one draw after enemy creation");
	// Exhaustion must neither mutate the record nor draw RNG.
	memset(enemyAvail, 0, sizeof enemyAvail);
	eventRec[0].eventdat2 = -200;
	JE_createNewEventEnemy(0, 0, 0);
	require(b == 0 && eventRec[0].eventdat2 == -200 && mt_rand_state_hash() == expected_rng,
	        "failed allocation never draws spawn X");
	memcpy(&enemyDat[1], saved, sizeof saved);
	memset(enemyAvail, 1, sizeof enemyAvail);
	memset(enemy, 0, sizeof enemy);
}

static void movement(void)
{
	power = 900;
	JE_playerMovement(&player[0], 1, 1, shipGr, shipGrPtr, &player[0].mouseX, &player[0].mouseY);
}

static void analog_movement(void)
{
	power = 900;
	JE_playerMovement(&player[0], 3, 1, shipGr, shipGrPtr, &player[0].mouseX, &player[0].mouseY);
}

static void sidekicks_fixture(void)
{
	unsigned int charged = 0, plain = 0;
	for (unsigned int i = 1; i <= gameSchema()->sidekick_max; ++i)
	{
		if (options[i].wport == 0 || options[i].ammo != 0) continue;
		if (options[i].pwr > 0 && !charged) charged = i;
		if (options[i].pwr == 0 && !plain) plain = i;
	}
	require(charged && plain, "installed data has both sidekick classes");
	player[0].items.sidekick[0] = charged;
	player[0].items.sidekick[1] = plain;
	JE_drawOptions();
	const bool demo = playDemo;
	playDemo = false;
	// Exercise the actual remappable keyboard action path, with nondefault keys.
	keySettings[KEY_SETTING_FIRE] = SDL_SCANCODE_F10;
	keySettings[KEY_SETTING_LEFT_SIDEKICK] = SDL_SCANCODE_F11;
	keySettings[KEY_SETTING_RIGHT_SIDEKICK] = SDL_SCANCODE_F12;
	player[0].sidekick[0].charge = 1;
	shotRepeat[SHOT_LEFT_SIDEKICK] = shotRepeat[SHOT_RIGHT_SIDEKICK] = 0;
	keysactive[SDL_SCANCODE_F10] = true;
	movement();
	require(player[0].sidekick[0].charge == 1 && shotRepeat[SHOT_RIGHT_SIDEKICK] > 0,
	        "main fire preserves charged left and fires uncharged right");
	keysactive[SDL_SCANCODE_F10] = false;
	keysactive[SDL_SCANCODE_F11] = true;
	shotRepeat[SHOT_LEFT_SIDEKICK] = 0;
	movement();
	require(player[0].sidekick[0].charge == 0 && shotRepeat[SHOT_LEFT_SIDEKICK] > 0,
	        "remapped left action discharges charged sidekick");
	keysactive[SDL_SCANCODE_F11] = false;
	player[0].items.sidekick[1] = charged;
	JE_drawOptions();
	player[0].sidekick[1].charge = 1;
	shotRepeat[SHOT_RIGHT_SIDEKICK] = 0;
	keysactive[SDL_SCANCODE_F12] = true;
	movement();
	require(player[0].sidekick[1].charge == 0 && shotRepeat[SHOT_RIGHT_SIDEKICK] > 0,
	        "remapped right action discharges charged sidekick");
	keysactive[SDL_SCANCODE_F11] = true;
	player[0].sidekick[0].charge = player[0].sidekick[1].charge = 1;
	shotRepeat[SHOT_LEFT_SIDEKICK] = shotRepeat[SHOT_RIGHT_SIDEKICK] = 0;
	movement();
	require(player[0].sidekick[0].charge == 0 && player[0].sidekick[1].charge == 0,
	        "both sidekick actions discharge together");
	keysactive[SDL_SCANCODE_F11] = keysactive[SDL_SCANCODE_F12] = false;
	// The injected stick is hardware-free, but follows the analog movement and
	// logical controller action paths used by remapped gamepad buttons.
	joystick_inject_stick(12000, 0);
	joystick[0].action[0] = true;
	player[0].items.sidekick[1] = plain;
	JE_drawOptions();
	player[0].sidekick[0].charge = 1;
	shotRepeat[SHOT_LEFT_SIDEKICK] = shotRepeat[SHOT_RIGHT_SIDEKICK] = 0;
	analog_movement();
	require(player[0].sidekick[0].charge == 1 && shotRepeat[SHOT_RIGHT_SIDEKICK] > 0,
	        "analog main action preserves charged sidekick");
	joystick[0].action[0] = false;
	joystick[0].action[2] = true;
	shotRepeat[SHOT_LEFT_SIDEKICK] = 0;
	analog_movement();
	require(player[0].sidekick[0].charge == 0, "analog left action fires charged sidekick");
	joystick[0].action[2] = false;
	playDemo = demo;
	logInfo("Rule coverage: charged/plain sidekicks and remapped left/right/both actions ran.");
	logInfo("Rule coverage: analog sidekick actions ran.");
	player[0].items.sidekick[1] = charged;
	JE_drawOptions(); // leave both chargeable sidekicks for real Modern HUD ticks
}

static void twiddle_fixture(void)
{
	player[0].items.ship = 14; // Storm, a new ship, within the fork's ship<15 gate
	JE_getShipInfo();
	const unsigned int combo = gameShipCombos(14)[0];
	require(combo == 1, "Storm selects its new twiddle");
	for (int i = 0; i < 8; ++i)
	{
		const unsigned int key = keyboardCombos[combo - 1][i];
		if (key > 100)
		{
			require(SFExecuted[0] == key - 100, "Storm twiddle reaches the special");
			break;
		}
		button[0] = key >= 5 && key <= 8;
		const unsigned int dir = key == 9 ? 0 : (key - 1) % 4 + 1;
		JE_SFCodes(1, 100, 100, 100 + (dir == 3) - (dir == 4), 100 + (dir == 1) - (dir == 2));
	}
	require(SFExecuted[0] != 0, "Storm twiddle executed");
	button[0] = false;
	logInfo("Rule coverage: Storm twiddle executed.");
}

static void punch_fixture(void)
{
	unsigned int wp = 0;
	for (unsigned int i = 1; i <= gameSchema()->weapon_max; ++i)
		if (gameWeaponValid(i) && weapons[i].trail == 198 && weapons[i].multi > 1) { wp = i; break; }
	require(wp != 0, "installed Flying Punch volley exists");
	player_shot_create(0, SHOT_FRONT, 100, 100, 100, 100, wp, 1);
	unsigned int count = 0, trails = 0;
	for (int i = 0; i < MAX_PWEAPON; ++i)
	{
		if (!shotAvail[i]) continue;
		++count;
		if (playerShotData[i].shotTrail == 198) ++trails;
		else require(playerShotData[i].shotTrail == 255, "outer Punch shots have no trail");
	}
	require(count == weapons[wp].multi && trails == 1, "only centre shot keeps Punch trail");
	const unsigned long long rng = mt_rand_state_hash();
	bool special_shot;
	int x, y;
	JE_integer damage;
	JE_byte filter, chain, owner;
	JE_word rw, rh;
	const int old_x = playerShotData[0].shotX, old_y = playerShotData[0].shotY;
	require(player_shot_move_and_draw(0, &special_shot, &x, &y, &damage, &filter, &chain, &owner, &rw, &rh),
	        "centre Punch shot moves and draws");
	require(explosions[0].sprite == 133 && explosions[0].ttl == 7 && explosions[0].x == old_x && explosions[0].y == old_y,
	        "198 smoke uses 98 motion without the type-6 position offset");
	JE_setupExplosion(120, 100, 0, 53, false, false);
	require(explosions[1].sprite == 96 && explosions[1].ttl == 3, "54th explosion entry");
	require(mt_rand_state_hash() == rng, "trail and explosion setup draw no RNG");
	logInfo("Rule coverage: Flying Punch centre trail and explosion 54 ran.");
}

void regress_rules_run(void)
{
	if (!regress_rule_fixture) return;
	require(regress_scenario_active(), "rule fixture requires --regress-level");
	// Preserve the real event stream. Fixtures construct their own record only
	// while invoking the event system, then the loaded level continues normally.
	const struct JE_EventRecType saved = eventRec[0];
	const JE_word saved_loc = eventLoc;
	VGAScreen = game_screen;
	if (!strcmp(regress_rule_fixture, "events")) events_fixture();
	else if (!strcmp(regress_rule_fixture, "spawn")) spawn_fixture();
	else if (!strcmp(regress_rule_fixture, "sidekicks")) sidekicks_fixture();
	else if (!strcmp(regress_rule_fixture, "twiddle")) twiddle_fixture();
	else if (!strcmp(regress_rule_fixture, "punch")) punch_fixture();
	else require(false, "unknown rule fixture");
	eventRec[0] = saved;
	eventLoc = saved_loc;
	logInfo("Rule fixture PASS: %s", regress_rule_fixture);
}
