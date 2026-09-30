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
#ifndef REGRESS_FLOW_H
#define REGRESS_FLOW_H

#include <stdbool.h>

// --regress-flow=NAME[:key=value,...]: drive the game through its own menus, the
// way a player does, with a scripted keyboard, and check that the parts of the
// game we care about ran.  Unlike --regress-script (one level of one episode)
// and --regress-screen (one screen), a flow starts at the title screen, walks
// the real menus (new game, gameplay, episode or battle, difficulty), plays the
// levels and ends at a well-defined point.  The keys come from the flow, one
// per blocking wait for input; nothing real ever reaches the game in regress
// mode (see handleSdlEvents).  Everything here is inert outside a flow.
//
//   battle:sel=N[,cash=C][,ticks=T][,die=T]
//        Timed Battle N (1-based) from the title: the battle menu, the timer,
//        the score board.  ticks=T ends the level as completed after T ticks
//        (time bonus), die=T kills the player after T ticks (game over),
//        cash=C adds cash at the start of the level so the score qualifies.
//   episode:ep=E[,ticks=T][,to=L]
//        Full Game from episode E: every level ends as completed after T ticks
//        (default 4) and every shop, map and story screen is left with Enter,
//        until the end of episode E has run and the next episode is set up (or,
//        for the last episode, the credits have played).  to=L stops a
//        multi-episode run at the end of episode L.
//   single-arcade:ep=E[,ticks=T]
//        One-player Arcade through the normal gameplay menu (not a secret ship).
//        ticks=0 keeps the level running for --regress-frames/snapshot captures.
//   save:ep=E[,ticks=T]
//        Complete one level, save slot 1 through shop Options, quit to the
//        title, Load slot 1, compare the persisted state, continue 40 ticks.
//        Requires --regress-user-root; saves restart levels, not live fights.
//   mouse:ep=E
//        Exercise the three Mouse actions, Reset and Done in the real shop.
//   --regress-gamepad routes navigation through push_joysticks_as_keyboard.
//   --regress-handoff=2.1|2000 enters the real launcher selection controller
//        before provider/assets/saves and verifies last_variant preselection.
//   --regress-data-audit=ROOT checks/logs resolved data opens from bootstrap on.
//   --regress-boss accelerates early event waves, frees early enemy slots, then
//        runs a loaded event-79 boss group for 60 ticks (use --regress-level).
//   list-levels:ep=E
//        Print the sections of episode E that play a level, one per line, and exit.
//
// The flow logs "Flow coverage: ..." lines for each step that ran; the harness
// script requires them.
extern const char *regress_flow;

bool regress_flow_active(void);

// Validates the flow after option parsing and, for a flow that runs, gives it a
// frame cap.  Exits on an unknown flow.
void regress_flow_init(void);

// Runs the flow: never returns.
void regress_flow_run(void);

// True for a flow that presses a key on every screen, timed ones included (the
// story, the warnings, the level-end tally).
bool regress_flow_presses_through(void);

// True while a flow feeds a key at every poll of the event queue, for the part
// of the game (Destruct) that reads the keyboard without waiting for it.
bool regress_flow_feeds_polls(void);

// Called by the input layer when a blocking wait finds no input: pushes the
// flow's next key.  Ends the run successfully when the flow is over.
void regress_flow_supply_input(void);

// Level hooks, called from the level loop.  Level begin resets the per-level
// tick count; the tick hook returns true when the level must end as completed
// now (the same state as the end-level event) and applies the flow's other
// per-level effects (cash, invulnerability, death).
void regress_flow_level_begin(void);
bool regress_flow_level_tick(void);

// Called when the episode script reaches the end of an episode and again once
// the transition to the next one has run.
void regress_flow_episode_end(unsigned int episode);
void regress_flow_episode_next(unsigned int from, unsigned int to);

// Logs "Flow coverage: <text>" (a no-op outside a flow).
void regress_flow_coverage(const char *format, ...);

#endif // REGRESS_FLOW_H
