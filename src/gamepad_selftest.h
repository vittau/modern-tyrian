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
#ifndef GAMEPAD_SELFTEST_H
#define GAMEPAD_SELFTEST_H

#include <stdbool.h>

// Headless self-test for the gamepad/hot-plug input path (--selftest-gamepad).
//
// It attaches SDL virtual controllers, drives the loaded default mapping,
// exercises hot-unplug/replug, checks the config round-trip and confirms the
// legacy (non-gamepad) joystick path still works.  It needs no hardware.

// Set by --selftest-gamepad.
extern bool selftest_gamepad;

// Cheap argv scan, run before config loading so the user's config is skipped.
// Returns true when --selftest-gamepad is present.
bool gamepad_selftest_scan_args(int argc, char *argv[]);

// Runs the self-test to completion; returns EXIT_SUCCESS or EXIT_FAILURE.
int gamepad_selftest_run(void);

#endif /* GAMEPAD_SELFTEST_H */
