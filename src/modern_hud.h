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
#ifndef MODERN_HUD_H
#define MODERN_HUD_H

#include <SDL3/SDL.h>

#include <stdbool.h>

// Expanded side-panel HUD (Phase 1b, step 2).
//
// The relocated in-game HUD lives on the same two off-screen 8-bit surfaces as
// in step 1 (see src/modern.h: modern_hud_surface(), modern_hud_begin_frame()).
// This module owns the layout and every relocated element:
//
//   * ship status: name, extra lives, cash, superbombs and the special-weapon
//     icon, together with the shield/armor values and the three vertical bars
//     (shield, armor, power reserve);
//   * armament: front/rear weapon name, power pips and rear firing mode;
//     left/right sidekick name and ammo or charge gauge;
//   * the step-1 globals: boss bars, level timer and the cheat notice.
//
// Layouts:
//   * 1 player: the armament is the left panel; the ship status lives in the
//     right panel, where the original sidebar kept shield/armor/power.  Shield,
//     armor and power are vertical bars filling bottom-up.
//   * 2 players: each player gets their own panel (the compact layout, status
//     then armament, both with the vertical vitals); player 2's panel is
//     right-aligned toward the outer screen edge.
//
// Everything is on the original 320x200 logical grid, drawn with the game's own
// fonts/sprites and procedural frames; the code only reads game state.

// Draws the whole relocated HUD for the current frame.  No-op unless
// modern_hud_in_panels() is true.  Call once per gameplay frame, after
// modern_hud_begin_frame().  Reproduces the exact `tempW` writes of the
// original JE_inGameDisplays so the parallax pan and the state hashes are
// untouched.
void modern_hud_draw(void);

// Draws the level timer (label + value) in the armament panel (the left one in
// single player, player 1's panel in two player).  Called from the exact spot
// of the original level-timer draw so its condition and side effects stay where
// they were.
void modern_hud_draw_timer(const char *label, const char *value, int brightness);

// Prepares the message strip for the level intro: it draws the strip bar and
// the level name (the same top line modern_hud_draw() draws once gameplay
// starts), so the intro's level name is part of the level's palette fade.  The
// panels stay empty/cleared.  No-op unless modern_hud_in_panels().  Display-only;
// does not touch game state or tempW.
void modern_hud_show_intro(void);

// Where boss bar `bar` should be drawn in panel mode.  In 2-player bar b goes
// in panel b; in 1-player both bars are stacked at the bottom of the armament
// (left) panel.  Returns false (leaving the out parameters alone) outside panel
// mode.  `half_width` is the drawn half-width (the original bar is 25).
bool modern_hud_boss_target(int bar, bool two_player, SDL_Surface **surface, int *cx, int *y, int *half_width);

#endif /* MODERN_HUD_H */
