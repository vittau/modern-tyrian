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
#ifndef LOGGING_H
#define LOGGING_H

#include <SDL3/SDL.h>

#define logDebug(...) SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define logInfo(...) SDL_Log(__VA_ARGS__)
#define logWarn(...) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define logError(...) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

void logFatal(SDL_PRINTF_FORMAT_STRING const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(1);

// Mirror every log message to `path` (truncating it) while still sending it to
// the default destination (stderr).  Returns false if the file can't be opened.
bool logOpenFile(const char *path);
bool logFileIsOpen(void);

// The path given by --log-file=PATH (or --log-file PATH) on the command line,
// else NULL.  Scanned before SDL_Init()/loadConfiguration() so the log starts
// before anything is configured.
const char *logFileFromArgs(int argc, char *argv[]);

#endif /* LOGGING_H */
