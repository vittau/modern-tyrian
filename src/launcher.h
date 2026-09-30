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
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "game_data.h"
#include "game_variant.h"

#include <stdbool.h>
#include <stddef.h>

// The launcher is the first screen of a normal start: a 16:9 screen drawn at
// the window's own resolution, on which the player picks Tyrian 2.1 or
// Tyrian 2000.  It runs once per process.  The variant it returns is chosen for
// the whole run, because the loaders keep per-process state; there is no way
// back to the launcher and no switching in process.

// INSTALL on the Tyrian 2000 panel opens the install dialog (download from
// camanis.net, a local .zip, or an existing folder), which drives the
// non-blocking installer (installer.h) from the launcher's own event loop.  When
// the data validates the panel turns into PLAY.  Where no file dialog exists (the
// Steam Deck's Game Mode, or a system without a portal) the zip and folder
// choices explain where to put the files, and the launcher picks them up.

// Shows the launcher on the window created by init_video() and returns once the
// player has chosen a variant whose data is present and valid (true, *out set),
// or asked to quit (false).  `preselect` is only the panel that starts
// selected.  `data_override` is --data, applied to whichever variant is chosen.
// A variant that fails validation reports the error in the launcher and lets the
// player choose again. `initial_error` optionally shows a late validation error.
// The caller selects the variant and continues startup.
bool launcherChoose(GameVariant preselect, const char *data_override, const char *initial_error, GameVariant *out);

// --regress-launcher=WxH,installed|missing,1|2[,about|message|install|install-nodlg|
// progress|nocurl|success|manual] renders one
// launcher frame at a fixed size in software, with no window, clock, data or
// input, and writes a hash of the frame (to --regress-out=PATH, or stdout).
// --launcher-png=PATH also saves the frame.  Nothing else is initialised.
bool launcherRegressRequested(int argc, char *argv[]);
int launcherRegressMain(int argc, char *argv[]);

// --launcher-flow=REQUEST drives the launcher's install controller with no
// window, through the same functions the screen uses, and prints the result.
// REQUEST: download | zip=PATH | folder=PATH | scan | picker-zip=PATH |
// picker-folder=PATH | picker-cancel | picker-error (the last four feed the
// file-dialog callback directly).  --install-2000-spec=FILE applies as for
// --install-2000.  Exit 0 when an install request ends installed.
bool launcherFlowRequested(int argc, char *argv[]);
int launcherFlowMain(int argc, char *argv[]);

#endif // LAUNCHER_H
