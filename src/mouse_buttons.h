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
#ifndef MOUSE_BUTTONS_H
#define MOUSE_BUTTONS_H

#include "opentyr.h"

// What a mouse button does in a level.  The value order is the order of the
// button names in the help text (joyButtonNames) and of the Tyrian 2000 mouse
// settings menu; the action of a button is chosen there.  Only the variants
// whose UI tables enable the mouse menu let the player change it: the others
// always use their default (the historical mapping).
typedef enum
{
	MOUSE_ACTION_FIRE,
	MOUSE_ACTION_LEFT_SIDEKICK,
	MOUSE_ACTION_RIGHT_SIDEKICK,
	MOUSE_ACTION_BOTH_SIDEKICKS,
	MOUSE_ACTION_REAR_MODE,
	MOUSE_ACTION_COUNT
} MouseAction;

// Left, right and middle button.
#define MOUSE_BUTTON_COUNT 3

// The action of a button (0 left, 1 right, 2 middle).
MouseAction mouse_button_action(unsigned int button);

// Menu: advance a button to its next action (wrapping), or restore the defaults.
void mouse_button_action_next(unsigned int button);
void mouse_button_actions_reset(void);

// Configuration (opentyrian.cfg, section "mouse").  Only what the player chose
// is kept: someone who never opens the menu leaves the file untouched, and a
// variant without the menu writes back what another variant chose.
int mouse_button_config_action(unsigned int button);  // MouseAction, or -1 for the default
const char *mouse_action_config_name(MouseAction action);
bool mouse_button_action_set_by_name(unsigned int button, const char *name);
const char *mouse_button_config_key(unsigned int button);

// Gameplay: turn the held mouse buttons (SDL_BUTTON_*MASK) into the player
// buttons (fire, left sidekick, right sidekick, rear mode).
void mouse_buttons_to_player(Uint8 buttons_down, bool player_button[4]);

#endif /* MOUSE_BUTTONS_H */
