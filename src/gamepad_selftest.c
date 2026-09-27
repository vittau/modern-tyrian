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
#include "gamepad_selftest.h"

#include "config.h"
#include "config_file.h"
#include "joystick.h"
#include "keyboard.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool selftest_gamepad = false;

static int selftest_failures = 0;

static void selftest_check(bool ok, const char *what, const char *detail)
{
	if (ok)
	{
		printf("PASS  %s\n", what);
	}
	else
	{
		printf("FAIL  %s: %s\n", what, detail);
		selftest_failures++;
	}
}

static void selftest_expect_int(long actual, long expected, const char *what)
{
	char detail[96];
	snprintf(detail, sizeof detail, "expected %ld, got %ld", expected, actual);
	selftest_check(actual == expected, what, detail);
}

static bool selftest_arg_is(const char *arg, const char *option)
{
	size_t len = strlen(option);
	return strncmp(arg, option, len) == 0 && (arg[len] == '\0' || arg[len] == '=');
}

bool gamepad_selftest_scan_args(int argc, char *argv[])
{
	for (int i = 1; i < argc; ++i)
	{
		if (selftest_arg_is(argv[i], "--selftest-gamepad"))
		{
			selftest_gamepad = true;
			return true;
		}
	}

	return false;
}

static SDL_JoystickID attach_virtual(SDL_JoystickType type, const char *name, int naxes, int nbuttons, int nhats)
{
	SDL_VirtualJoystickDesc desc;
	SDL_INIT_INTERFACE(&desc);
	desc.type = type;
	desc.naxes = (Uint16)naxes;
	desc.nbuttons = (Uint16)nbuttons;
	desc.nhats = (Uint16)nhats;
	desc.button_mask = 0xffffffffu;
	desc.axis_mask = 0xffffffffu;
	desc.name = name;

	return SDL_AttachVirtualJoystick(&desc);
}

// Delivers pending SDL events so the hot-plug handlers run.
static void selftest_pump(void)
{
	SDL_UpdateJoysticks();
	SDL_PumpEvents();
	handleSdlEvents();
}

static void selftest_poll(void)
{
	if (joysticks > 0)
		poll_joysticks();
}

static void release_gamepad(SDL_Joystick *feed)
{
	for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
		SDL_SetJoystickVirtualButton(feed, i, false);
	for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i)
		SDL_SetJoystickVirtualAxis(feed, i, 0);

	SDL_UpdateJoysticks();
	selftest_poll();
}

static void release_stick(SDL_Joystick *feed, int axes, int buttons, int hats)
{
	for (int i = 0; i < buttons; ++i)
		SDL_SetJoystickVirtualButton(feed, i, false);
	for (int i = 0; i < axes; ++i)
		SDL_SetJoystickVirtualAxis(feed, i, 0);
	for (int i = 0; i < hats; ++i)
		SDL_SetJoystickVirtualHat(feed, i, SDL_HAT_CENTERED);

	SDL_UpdateJoysticks();
	selftest_poll();
}

// Presses a gamepad button/axis and refreshes the game's view of it.
static void gamepad_press_button(SDL_Joystick *feed, SDL_GamepadButton button)
{
	release_gamepad(feed);
	SDL_SetJoystickVirtualButton(feed, button, true);
	SDL_UpdateJoysticks();
	selftest_poll();
}

// Presses a gamepad button and lets push_joysticks_as_keyboard() translate it,
// which is what the menus observe.
static void gamepad_press_button_for_key(SDL_Joystick *feed, SDL_GamepadButton button)
{
	release_gamepad(feed);
	SDL_SetJoystickVirtualButton(feed, button, true);
	SDL_UpdateJoysticks();
	push_joysticks_as_keyboard();
}

static void gamepad_set_axis(SDL_Joystick *feed, SDL_GamepadAxis axis, Sint16 value)
{
	release_gamepad(feed);
	SDL_SetJoystickVirtualAxis(feed, axis, value);
	SDL_UpdateJoysticks();
	selftest_poll();
}

// Counts pushed KEY_DOWN events for a scancode, draining the queue.
static int drain_key_down(SDL_Scancode scancode)
{
	SDL_Event ev;
	int count = 0;

	while (SDL_PollEvent(&ev))
	{
		if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == scancode)
			count++;
	}

	return count;
}

static void test_gamepad_defaults_and_movement(SDL_Joystick *feed)
{
	printf("-- gamepad default mapping and movement --\n");

	selftest_check(joystick[0].analog, "gamepad defaults to analog", "analog is off");

	// Left stick, all four directions, proportional.
	gamepad_set_axis(feed, SDL_GAMEPAD_AXIS_LEFTX, -32000);
	selftest_check(joystick[0].direction[3] && !joystick[0].direction[1],
	               "stick left -> left direction", "direction flags wrong");
	selftest_check(joystick[0].x < 0 && joystick_axis_reduce(0, joystick[0].x) < 0,
	               "stick left -> negative movement delta", "movement delta not negative");

	gamepad_set_axis(feed, SDL_GAMEPAD_AXIS_LEFTX, 32000);
	selftest_check(joystick[0].direction[1] && !joystick[0].direction[3],
	               "stick right -> right direction", "direction flags wrong");
	selftest_check(joystick_axis_reduce(0, joystick[0].x) > 0,
	               "stick right -> positive movement delta", "movement delta not positive");

	gamepad_set_axis(feed, SDL_GAMEPAD_AXIS_LEFTY, -32000);
	selftest_check(joystick[0].direction[0] && !joystick[0].direction[2],
	               "stick up -> up direction", "direction flags wrong");
	selftest_check(joystick_axis_reduce(0, joystick[0].y) < 0,
	               "stick up -> negative movement delta", "movement delta not negative");

	gamepad_set_axis(feed, SDL_GAMEPAD_AXIS_LEFTY, 32000);
	selftest_check(joystick[0].direction[2] && !joystick[0].direction[0],
	               "stick down -> down direction", "direction flags wrong");
	selftest_check(joystick_axis_reduce(0, joystick[0].y) > 0,
	               "stick down -> positive movement delta", "movement delta not positive");

	// Proportionality: half deflection is slower than full deflection.
	gamepad_set_axis(feed, SDL_GAMEPAD_AXIS_LEFTX, 32000);
	int full = joystick_axis_reduce(0, joystick[0].x);
	gamepad_set_axis(feed, SDL_GAMEPAD_AXIS_LEFTX, 16000);
	int half = joystick_axis_reduce(0, joystick[0].x);
	selftest_check(full > 0 && half > 0 && half < full,
	               "analog movement is proportional", "half deflection is not slower than full");

	// Threshold: below the dead zone there is no movement.
	gamepad_set_axis(feed, SDL_GAMEPAD_AXIS_LEFTX, joystick[0].threshold * 1000 / 2);
	selftest_expect_int(joystick_axis_reduce(0, joystick[0].x), 0,
	                    "below threshold -> no movement");

	// Sensitivity scales the delta.
	gamepad_set_axis(feed, SDL_GAMEPAD_AXIS_LEFTX, 32000);
	int low_sensitivity = joystick_axis_reduce(0, joystick[0].x);
	joystick[0].sensitivity = 10;
	int high_sensitivity = joystick_axis_reduce(0, joystick[0].x);
	joystick[0].sensitivity = 5;
	selftest_check(high_sensitivity > low_sensitivity,
	               "sensitivity scales analog movement", "higher sensitivity is not faster");

	// D-pad: digital, full deflection.
	gamepad_press_button(feed, SDL_GAMEPAD_BUTTON_DPAD_UP);
	selftest_check(joystick[0].direction[0] && joystick_axis_reduce(0, joystick[0].y) < 0,
	               "D-pad up -> up direction at full speed", "D-pad up not detected");
	gamepad_press_button(feed, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
	selftest_check(joystick[0].direction[1] && joystick_axis_reduce(0, joystick[0].x) > 0,
	               "D-pad right -> right direction at full speed", "D-pad right not detected");
	gamepad_press_button(feed, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
	selftest_check(joystick[0].direction[2], "D-pad down -> down direction", "D-pad down not detected");
	gamepad_press_button(feed, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
	selftest_check(joystick[0].direction[3], "D-pad left -> left direction", "D-pad left not detected");

	// No other inputs are asserted.
	release_gamepad(feed);
	selftest_check(joystick[0].x == 0 && joystick[0].y == 0 && !joydown,
	               "neutral -> no movement", "neutral state is not neutral");
}

static void test_gamepad_actions(SDL_Joystick *feed)
{
	printf("-- gamepad action buttons --\n");

	// fire (south/A) is action[0] and confirm; mode (east/B) is action[1] and cancel.
	gamepad_press_button_for_key(feed, SDL_GAMEPAD_BUTTON_SOUTH);
	selftest_check(joystick[0].action[0] && joystick[0].action_pressed[0] && joystick[0].confirm,
	               "A -> fire/confirm", "A not mapped to fire");
	selftest_check(drain_key_down(SDL_SCANCODE_RETURN) == 1,
	               "A -> Enter in menus", "A did not push Enter");

	gamepad_press_button_for_key(feed, SDL_GAMEPAD_BUTTON_EAST);
	selftest_check(joystick[0].action[1] && joystick[0].cancel,
	               "B -> change fire/cancel", "B not mapped to change fire");
	selftest_check(drain_key_down(SDL_SCANCODE_ESCAPE) == 1,
	               "B -> Esc in menus", "B did not push Esc");

	gamepad_press_button(feed, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
	selftest_check(joystick[0].action[2], "left shoulder -> left sidekick", "left shoulder not detected");

	gamepad_press_button(feed, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
	selftest_check(joystick[0].action[3], "right shoulder -> right sidekick", "right shoulder not detected");

	gamepad_press_button_for_key(feed, SDL_GAMEPAD_BUTTON_BACK);
	selftest_check(joystick[0].action[4] && joystick[0].action_pressed[4],
	               "back -> in-game menu", "back not mapped to menu");
	selftest_check(drain_key_down(SDL_SCANCODE_RETURN) == 1,
	               "back -> Enter in menus", "back did not push Enter");

	gamepad_press_button_for_key(feed, SDL_GAMEPAD_BUTTON_START);
	selftest_check(joystick[0].action[5] && joystick[0].action_pressed[5],
	               "start -> pause", "start not mapped to pause");
	selftest_check(drain_key_down(SDL_SCANCODE_ESCAPE) == 1,
	               "start -> Esc in menus", "start did not push Esc");

	release_gamepad(feed);
}

// Writes a config with the game's writer and parses it back with the game's
// parser.  Uses $OPENTYRIAN_SELFTEST_CONFIG as the file when set (so a real
// opentyrian.cfg can be inspected), otherwise an anonymous temporary file.
static bool write_config_then_parse(const Config *config, Config *parsed, const char *env_var)
{
	const char *path = SDL_getenv(env_var);
	FILE *file;

	if (path != NULL && path[0] != '\0')
	{
		file = fopen(path, "wb");
		if (file == NULL)
			return false;
		config_write(config, file);
		if (fclose(file) != 0)
			return false;

		printf("      (wrote %s)\n", path);

		file = fopen(path, "rb");
		if (file == NULL)
			return false;
	}
	else
	{
		file = tmpfile();
		if (file == NULL)
			return false;
		config_write(config, file);
		fflush(file);
		rewind(file);
	}

	bool ok = config_parse(parsed, file);
	fclose(file);
	return ok;
}

// Config round-trip: a gamepad remap survives write + parse + load.
static void test_gamepad_config_roundtrip(void)
{
	printf("-- gamepad config round-trip --\n");

	joystick[0].assignment[4][0].type = GAMEPAD_BUTTON;
	joystick[0].assignment[4][0].num = SDL_GAMEPAD_BUTTON_EAST;
	joystick[0].assignment[4][0].negative_axis = false;
	joystick[0].sensitivity = 7;
	joystick[0].threshold = 3;
	joystick[0].analog = false;

	Config saved;
	config_init(&saved);
	save_joystick_assignments(&saved, 0);

	Config loaded;
	config_init(&loaded);
	if (!write_config_then_parse(&saved, &loaded, "OPENTYRIAN_SELFTEST_CONFIG"))
	{
		selftest_check(false, "gamepad config parses", "config write/parse failed");
		config_deinit(&loaded);
		config_deinit(&saved);
		return;
	}

	reset_joystick_assignments(0);
	load_joystick_assignments(&loaded, 0);

	selftest_check(joystick[0].assignment[4][0].type == GAMEPAD_BUTTON &&
	               joystick[0].assignment[4][0].num == SDL_GAMEPAD_BUTTON_EAST,
	               "gamepad remap survives config round-trip", "fire mapping did not round-trip");
	selftest_expect_int(joystick[0].sensitivity, 7, "sensitivity survives config round-trip");
	selftest_expect_int(joystick[0].threshold, 3, "threshold survives config round-trip");
	selftest_check(!joystick[0].analog, "analog flag survives config round-trip", "analog did not round-trip");

	// The default mapping must be restored for the later tests.
	reset_joystick_assignments(0);

	config_deinit(&loaded);
	config_deinit(&saved);
}

// A legacy config (raw joystick codes) still loads, for non-gamepads and old files.
static void test_legacy_config_load(Config *legacy)
{
	printf("-- legacy joystick config load --\n");

	Config serialized;
	config_init(&serialized);

	ConfigSection *section = config_find_or_add_section(&serialized, "joystick", SDL_GetJoystickName(joystick[0].handle));
	if (section != NULL)
	{
		config_set_string_option(section, "up", "AX 1-");
		config_set_string_option(section, "fire", "BTN 1");
		config_set_string_option(section, "pause", "H 1Y+");
	}

	if (!write_config_then_parse(&serialized, legacy, "OPENTYRIAN_SELFTEST_CONFIG_LEGACY"))
	{
		selftest_check(false, "legacy config parses", "config write/parse failed");
		config_deinit(&serialized);
		return;
	}

	reset_joystick_assignments(0);
	load_joystick_assignments(legacy, 0);

	selftest_check(joystick[0].assignment[0][0].type == AXIS &&
	               joystick[0].assignment[0][0].num == 0 &&
	               joystick[0].assignment[0][0].negative_axis,
	               "legacy 'AX 1-' loads", "legacy axis mapping did not load");
	selftest_check(joystick[0].assignment[4][0].type == BUTTON &&
	               joystick[0].assignment[4][0].num == 0,
	               "legacy 'BTN 1' loads", "legacy button mapping did not load");
	selftest_check(joystick[0].assignment[9][0].type == HAT &&
	               joystick[0].assignment[9][0].num == 0 &&
	               !joystick[0].assignment[9][0].x_axis &&
	               !joystick[0].assignment[9][0].negative_axis,
	               "legacy 'H 1Y+' loads", "legacy hat mapping did not load");

	config_deinit(&serialized);
}

// A virtual non-gamepad joystick must keep using the raw SDL_Joystick path.
static void test_legacy_joystick(void)
{
	printf("-- legacy (non-gamepad) joystick path --\n");

	SDL_JoystickID stick_id = attach_virtual(SDL_JOYSTICK_TYPE_UNKNOWN,
	                                         "OpenTyrian Selftest Stick", 4, 8, 1);
	selftest_check(stick_id != 0, "attach non-gamepad virtual joystick", "SDL_AttachVirtualJoystick failed");
	if (stick_id == 0)
		return;

	selftest_pump();

	selftest_expect_int(joysticks, 1, "non-gamepad joystick detected");
	selftest_check(joysticks == 1 && !joystick[0].is_gamepad,
	               "non-gamepad joystick uses the legacy path", "joystick was opened as a gamepad");

	if (joysticks != 1)
		return;

	// Guarantee the default raw mapping (an earlier config may have persisted).
	reset_joystick_assignments(0);

	SDL_Joystick *feed = joystick[0].handle;

	release_stick(feed, 4, 8, 1);

	// Legacy default: axis 1 negative is "up".
	SDL_SetJoystickVirtualAxis(feed, 1, -32000);
	SDL_UpdateJoysticks();
	selftest_poll();
	selftest_check(joystick[0].direction[0], "legacy stick up", "legacy axis-up not detected");

	release_stick(feed, 4, 8, 1);

	// Legacy default: button 0 is "fire".
	SDL_SetJoystickVirtualButton(feed, 0, true);
	SDL_UpdateJoysticks();
	selftest_poll();
	selftest_check(joystick[0].action[0], "legacy stick fire", "legacy button-fire not detected");

	release_stick(feed, 4, 8, 1);

	SDL_DetachVirtualJoystick(stick_id);
	selftest_pump();
	selftest_expect_int(joysticks, 0, "non-gamepad joystick unplugged");
}

int gamepad_selftest_run(void)
{
	// Headless; respect drivers the caller set explicitly.
	if (SDL_getenv("SDL_VIDEO_DRIVER") == NULL)
		SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
	if (SDL_getenv("SDL_AUDIO_DRIVER") == NULL)
		SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);

	printf("Gamepad self-test (SDL %d.%d.%d)\n",
	       SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);

	config_init(&opentyrian_config);

	ignore_joystick = false;
	init_joysticks();

	selftest_expect_int(joysticks, 0, "no controllers at startup");

	// --- virtual gamepad attach (hot-plug) ---
	SDL_JoystickID gamepad_id = attach_virtual(SDL_JOYSTICK_TYPE_GAMEPAD,
	                                           "OpenTyrian Selftest Gamepad",
	                                           SDL_GAMEPAD_AXIS_COUNT, SDL_GAMEPAD_BUTTON_COUNT, 0);
	selftest_check(gamepad_id != 0, "attach virtual gamepad", "SDL_AttachVirtualJoystick failed");

	if (gamepad_id == 0)
	{
		deinit_joysticks();
		config_deinit(&opentyrian_config);
		printf("Gamepad self-test FAILED (no virtual gamepad)\n");
		return EXIT_FAILURE;
	}

	selftest_pump();

	selftest_expect_int(joysticks, 1, "hot-plug detected the gamepad");
	selftest_check(joysticks == 1 && joystick[0].is_gamepad,
	               "device recognised and opened as a gamepad", "SDL_IsGamepad/OpenGamepad path not used");
	selftest_check(joysticks == 1 && joystick[0].gamepad != NULL,
	               "gamepad handle is open", "gamepad handle is NULL");

	if (joysticks == 1 && joystick[0].gamepad != NULL)
	{
		SDL_Joystick *feed = joystick[0].handle;

		test_gamepad_defaults_and_movement(feed);
		test_gamepad_actions(feed);
		test_gamepad_config_roundtrip();

		// --- hot-unplug / hot-replug a gamepad ---
		printf("-- hot-unplug / hot-replug --\n");
		SDL_DetachVirtualJoystick(gamepad_id);
		selftest_pump();
		selftest_expect_int(joysticks, 0, "hot-unplug released the gamepad");

		gamepad_id = attach_virtual(SDL_JOYSTICK_TYPE_GAMEPAD,
		                            "OpenTyrian Selftest Gamepad",
		                            SDL_GAMEPAD_AXIS_COUNT, SDL_GAMEPAD_BUTTON_COUNT, 0);
		selftest_pump();
		selftest_expect_int(joysticks, 1, "hot-replug reopened the gamepad");
		selftest_check(joysticks == 1 && joystick[0].is_gamepad,
		               "replugged device is a gamepad", "replug used the wrong path");

		if (joysticks == 1 && joystick[0].gamepad != NULL)
		{
			release_gamepad(joystick[0].handle);
			gamepad_set_axis(joystick[0].handle, SDL_GAMEPAD_AXIS_LEFTX, 32000);
			selftest_check(joystick[0].direction[1],
			               "replugged gamepad responds to input", "replugged gamepad is not polled");
		}

		SDL_DetachVirtualJoystick(gamepad_id);
		selftest_pump();
		selftest_expect_int(joysticks, 0, "gamepad cleaned up");

		// --- legacy config loads into a legacy joystick ---
		Config legacy;
		config_init(&legacy);
		SDL_JoystickID stick_id = attach_virtual(SDL_JOYSTICK_TYPE_UNKNOWN,
		                                         "OpenTyrian Selftest Stick", 4, 8, 1);
		selftest_pump();

		if (stick_id != 0 && joysticks == 1)
			test_legacy_config_load(&legacy);

		SDL_DetachVirtualJoystick(stick_id);
		selftest_pump();
		selftest_expect_int(joysticks, 0, "legacy joystick cleaned up");
		config_deinit(&legacy);

		test_legacy_joystick();
	}

	deinit_joysticks();
	config_deinit(&opentyrian_config);

	if (selftest_failures == 0)
	{
		printf("Gamepad self-test PASSED\n");
		return EXIT_SUCCESS;
	}

	printf("Gamepad self-test FAILED (%d check%s)\n", selftest_failures, selftest_failures == 1 ? "" : "s");
	return EXIT_FAILURE;
}
