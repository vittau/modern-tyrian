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
#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "file.h"
#include "game_variant.h"

typedef enum
{
	GAME_DATA_OK,
	GAME_DATA_NOT_FOUND,
	GAME_DATA_WRONG_VARIANT,
	GAME_DATA_MISSING_FILE,
	GAME_DATA_BAD_SIZE,
	GAME_DATA_BAD_HASH,
	GAME_DATA_IO_ERROR,
	GAME_DATA_UNAVAILABLE
} GameDataStatus;

typedef struct
{
	GameDataStatus status;
	char filename[64];
	char detail[256];
} GameDataError;

typedef struct
{
	const char *override_directory;
	const char *installed_directory;
	const char *package_directory;
} GameDataSearch;

typedef struct GameDataProvider GameDataProvider;

GameDataStatus gameDataLocate(const GameVariantDef *variant, const GameDataSearch *search,
                             GameDataProvider **out, GameDataError *error);
GameDataStatus gameDataValidate(GameDataProvider *provider, GameDataError *error);
File gameDataOpen(const GameDataProvider *provider, const char *filename);
bool gameDataExists(const GameDataProvider *provider, const char *filename);
const char *gameDataDirectory(const GameDataProvider *provider);
const GameVariantDef *gameDataVariant(const GameDataProvider *provider);
const char *gameDataStatusName(GameDataStatus status);
void gameDataClose(GameDataProvider *provider);
// Selected by findDataFiles(); all legacy data-file helpers use this provider.
GameDataProvider *gameDataCurrent(void);

// Locates and validates the data of the selected variant once, logging the
// result; on failure logs the reason (fatal) and returns false.  Calling it
// again after success does nothing.
bool gameDataPrepare(void);

#endif // GAME_DATA_H
