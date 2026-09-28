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
#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "config_file.h"

#include <SDL3/SDL.h>

typedef enum
{
	NONE,
	AXIS,
	BUTTON,
	HAT,
	GAMEPAD_BUTTON,
	GAMEPAD_AXIS
}
Joystick_assignment_types;

typedef struct
{
	Joystick_assignment_types type;
	int num;
	
	// if hat
	bool x_axis; // else y_axis
	
	// if hat or axis
	bool negative_axis; // else positive
}
Joystick_assignment;

typedef struct
{
	SDL_JoystickID id;
	SDL_Joystick *handle;    // underlying joystick, always valid when opened
	SDL_Gamepad *gamepad;    // non-NULL when opened through the Gamepad API
	bool is_gamepad;
	bool injected;           // synthetic stick created by --regress-stick
	
	Joystick_assignment assignment[10][2]; // 0-3: directions, 4-9: actions
	
	bool analog;
	int sensitivity, threshold;
	int deadzone;          // Modern radial dead zone, percent of full deflection (0..20)
	int analog_subpixel[2]; // Modern sub-pixel remainder, 1/1024 px (x, y)
	int velocity_target_frac[2]; // Modern momentum-target remainder, 1/1024 px/tick (x, y)
	
	signed int x, y;
	int analog_direction[4];
	bool direction[4], direction_pressed[4]; // up, right, down, left  (_pressed, for emulating key presses)
	
	bool confirm, cancel;
	bool action[6], action_pressed[6]; // fire, mode swap, left fire, right fire, menu, pause
	
	Uint32 joystick_delay;
	bool input_pressed;
}
Joystick;

extern int joystick_repeat_delay;
extern bool joydown;
extern bool ignore_joystick;
extern int joysticks;
extern Joystick *joystick;

// Modern analog radial dead zone, percent of full stick deflection.
#define JOYSTICK_DEADZONE_MIN 0
#define JOYSTICK_DEADZONE_MAX 20
#define JOYSTICK_DEADZONE_DEFAULT 10

// The original analog path clamps mouseXC to +/-30 and draws (mouseXC +/- 3) / 4
// pixels per tick, so this is the absolute per-tick ceiling it could reach.
#define JOYSTICK_ANALOG_MAX_STEP ((30 + 3) / 4)

// The original velocity model caps x_velocity/y_velocity at +/-4; the Modern
// curve scales that cap by s so the steady-state speed is progressive.
#define JOYSTICK_MODERN_VELOCITY_MAX 4

// >= 0 forces the dead zone of every joystick (for tests); -1 keeps the cfg.
extern int joystick_deadzone_override;

int joystick_axis_reduce(int j, int value);
bool joystick_analog_angle(int j, float *angle);

// Modern response curve: radial dead zone plus a linear ramp from the dead zone
// up to the 75% point.  Returns the speed fraction for a raw stick vector (x, y)
// in 1/1024 units (0..1024); 0 inside the dead zone and 1024 at 75% or more.
int joystick_modern_response(int x, int y, int deadzone_percent);
// Largest per-tick pixel step the Modern curve may emit: the joystick's current
// full-deflection analog speed, so 100% of the curve never beats the original.
int joystick_modern_max_step(int j);
// Modern analog movement for the joystick's current x/y in whole pixels per tick,
// keeping the sub-pixel remainder in the device so slow speeds still creep.
// The requested momentum is split per axis by the same projection as the step
// (target_x = target*x/mag, target_y = target*y/mag, each with its own sub-tick
// carry), so cross-axis noise inside or near the dead zone contributes ~0
// instead of the full target.  *velocity_target_x/_y receive the momentum target
// magnitude in whole px/tick (0..legacy 4); the axis sign comes from the raw
// joystick x/y.  Both are -1 when the stick is inside the dead zone (no momentum
// target).  A push within JOYSTICK_SNAP_* of an axis is snapped onto it so it
// moves perfectly straight.
void joystick_analog_movement(int j, int *dx, int *dy,
                              int *velocity_target_x, int *velocity_target_y);

void poll_joystick(int j);
void poll_joysticks(void);

// Regression harness: install one synthetic analog stick at a fixed raw axis
// position so JE_playerMovement can run the whole stick path headless.  Only
// called from regress_init(); inert outside regress mode.
void joystick_inject_stick(int x, int y);

void push_key(SDL_Scancode key);
void push_joysticks_as_keyboard(void);

void init_joysticks(void);
void deinit_joysticks(void);

// hot-plug handling; called from the SDL event pump
void joystick_device_added(SDL_JoystickID id);
void joystick_device_removed(SDL_JoystickID id);

void reset_joystick_assignments(int j);
bool load_joystick_assignments(Config* config, int j);
bool save_joystick_assignments(Config* config, int j);

void joystick_assignments_to_string(char *buffer, size_t buffer_len, const Joystick_assignment *assignments);

bool detect_joystick_assignment(int j, Joystick_assignment *assignment);
bool joystick_assignment_cmp(const Joystick_assignment *, const Joystick_assignment *);

#endif /* JOYSTICK_H */
