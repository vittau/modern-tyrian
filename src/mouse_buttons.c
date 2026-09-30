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
#include "mouse_buttons.h"

#include "game_schema.h"
#include "mouse.h"

#include <SDL3/SDL.h>
#include <string.h>

// The player's choice per button, plus one: 0 keeps the variant's default.
static Uint8 chosen[MOUSE_BUTTON_COUNT];

static const char *const actionNames[MOUSE_ACTION_COUNT] =
{
	"fire", "left sidekick", "right sidekick", "both sidekicks", "rear mode"
};

static const char *const buttonKeys[MOUSE_BUTTON_COUNT] = { "left", "right", "middle" };

static bool customisable(void)
{
	return gameUi()->mouse_menu != 0;
}

MouseAction mouse_button_action(unsigned int button)
{
	if (button >= MOUSE_BUTTON_COUNT)
		return MOUSE_ACTION_FIRE;
	if (customisable() && chosen[button] != 0)
		return (MouseAction)(chosen[button] - 1);
	return (MouseAction)gameUi()->default_mouse_actions[button];
}

void mouse_button_action_next(unsigned int button)
{
	if (button >= MOUSE_BUTTON_COUNT || !customisable())
		return;
	const unsigned int next = ((unsigned int)mouse_button_action(button) + 1) % MOUSE_ACTION_COUNT;
	chosen[button] = (Uint8)(next + 1);
}

void mouse_button_actions_reset(void)
{
	memset(chosen, 0, sizeof chosen);
}

int mouse_button_config_action(unsigned int button)
{
	return button < MOUSE_BUTTON_COUNT && chosen[button] != 0 ? chosen[button] - 1 : -1;
}

const char *mouse_action_config_name(MouseAction action)
{
	return action < MOUSE_ACTION_COUNT ? actionNames[action] : actionNames[0];
}

const char *mouse_button_config_key(unsigned int button)
{
	return button < MOUSE_BUTTON_COUNT ? buttonKeys[button] : buttonKeys[0];
}

bool mouse_button_action_set_by_name(unsigned int button, const char *name)
{
	if (button >= MOUSE_BUTTON_COUNT)
		return false;
	for (unsigned int i = 0; i < MOUSE_ACTION_COUNT; ++i)
	{
		if (strcmp(name, actionNames[i]) == 0)
		{
			chosen[button] = (Uint8)(i + 1);
			return true;
		}
	}
	return false;
}

void mouse_buttons_to_player(Uint8 buttons_down, bool player_button[4])
{
	// Without a middle button the right one stands in for it, as it always did.
	const bool held[MOUSE_BUTTON_COUNT] =
	{
		(buttons_down & SDL_BUTTON_LMASK) != 0,
		(buttons_down & SDL_BUTTON_RMASK) != 0,
		(buttons_down & (mouse_has_three_buttons ? SDL_BUTTON_MMASK : SDL_BUTTON_RMASK)) != 0,
	};

	for (unsigned int i = 0; i < MOUSE_BUTTON_COUNT; ++i)
	{
		if (!held[i])
			continue;
		switch (mouse_button_action(i))
		{
		case MOUSE_ACTION_FIRE:           player_button[0] = true; break;
		case MOUSE_ACTION_LEFT_SIDEKICK:  player_button[1] = true; break;
		case MOUSE_ACTION_RIGHT_SIDEKICK: player_button[2] = true; break;
		case MOUSE_ACTION_BOTH_SIDEKICKS: player_button[1] = player_button[2] = true; break;
		case MOUSE_ACTION_REAR_MODE:      player_button[3] = true; break;
		default: break;
		}
	}
}
