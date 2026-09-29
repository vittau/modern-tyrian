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
#include "game_data.h"

#include "opentyr.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct GameDataProvider
{
	const GameVariantDef *variant;
	char *directory;
	GameDataStatus status;
	GameDataError error;
};

static GameDataProvider *currentProvider = NULL;

static GameDataStatus dataError(GameDataProvider *provider, GameDataError *error,
                                GameDataStatus status, const char *filename, const char *detail)
{
	GameDataError result = { status, "", "" };
	snprintf(result.filename, sizeof result.filename, "%s", filename);
	snprintf(result.detail, sizeof result.detail, "%s", detail);
	if (provider != NULL)
	{
		provider->status = status;
		provider->error = result;
	}
	if (error != NULL)
		*error = result;
	return status;
}

static bool setDirectory(GameDataProvider *provider, const char *directory)
{
	// Freeze a relative root against the current cwd without resolving symlinks.
	bool absolute = directory[0] == '/' || directory[0] == '\\' ||
	                (isalpha((unsigned char)directory[0]) && directory[1] == ':');
	char *cwd = absolute ? NULL : SDL_GetCurrentDirectory();
	if (!absolute && cwd == NULL)
		return false;
	size_t size = (cwd != NULL ? strlen(cwd) : 0) + strlen(directory) + 1;
	char *copy = malloc(size);
	if (copy == NULL)
	{
		SDL_free(cwd);
		return false;
	}
	snprintf(copy, size, "%s%s", cwd != NULL ? cwd : "", directory);
	SDL_free(cwd);
	free(provider->directory);
	provider->directory = copy;
	return true;
}

GameDataStatus gameDataLocate(const GameVariantDef *variant, const GameDataSearch *search,
                             GameDataProvider **out, GameDataError *error)
{
	*out = NULL;
	if (variant == NULL || variant->id != VARIANT_TYRIAN21)
		return dataError(NULL, error, GAME_DATA_UNAVAILABLE, "", "Tyrian 2000 is not available yet.");

	GameDataProvider *provider = calloc(1, sizeof *provider);
	if (provider == NULL)
		return dataError(NULL, error, GAME_DATA_IO_ERROR, "", "Could not allocate data provider.");
	provider->variant = variant;
	*out = provider;

	// An explicit directory is authoritative, even when it is incomplete.
	if (search->override_directory != NULL)
	{
		if (!setDirectory(provider, search->override_directory))
			return dataError(provider, error, GAME_DATA_IO_ERROR, "", "Could not allocate data directory.");
		if (gameDataExists(provider, "tyrian1.lvl"))
			return dataError(provider, error, GAME_DATA_OK, "", "");
		return dataError(provider, error, GAME_DATA_NOT_FOUND, "tyrian1.lvl", "The Tyrian data files were not found.  OpenTyrian requires the Tyrian v2.0/v2.1 data files.");
	}

	// Match the legacy executable/package, compiled system path, cwd order.
	const char *basePath = SDL_GetBasePath();
	char *baseData = NULL;
	if (basePath != NULL)
	{
		size_t size = strlen(basePath) + sizeof "data";
		baseData = malloc(size);
		if (baseData != NULL)
			snprintf(baseData, size, "%sdata", basePath);
	}
	const char *directories[] =
	{
		search->package_directory != NULL ? search->package_directory : baseData,
#ifdef TYRIAN_DIR
		TYRIAN_DIR,
#endif
		""
	};
	GameDataStatus status = GAME_DATA_NOT_FOUND;
	for (size_t i = 0; i < COUNTOF(directories); ++i)
	{
		if (directories[i] == NULL)
			continue;
		if (!setDirectory(provider, directories[i]))
		{
			status = GAME_DATA_IO_ERROR;
			break;
		}
		if (gameDataExists(provider, "tyrian1.lvl"))
		{
			status = GAME_DATA_OK;
			break;
		}
	}
	free(baseData);
	return dataError(provider, error, status, status == GAME_DATA_OK ? "" : "tyrian1.lvl",
	                 status == GAME_DATA_OK ? "" : "The Tyrian data files were not found.  OpenTyrian requires the Tyrian v2.0/v2.1 data files.");
}

GameDataStatus gameDataValidate(GameDataProvider *provider, GameDataError *error)
{
	if (provider == NULL)
		return dataError(NULL, error, GAME_DATA_NOT_FOUND, "", "The Tyrian data files were not found.  OpenTyrian requires the Tyrian v2.0/v2.1 data files.");
	if (provider->status != GAME_DATA_OK)
	{
		if (error != NULL)
			*error = provider->error;
		return provider->status;
	}

	// Compatible 2.0/2.1 profile, not the harness's strict canonical manifest.
	// Episodes, custom ships and Christmas resources retain their old checks.
	File file = gameDataOpen(provider, "tyrian.shp");
	if (file.error)
		return dataError(provider, error, file.errnum == ENOENT ? GAME_DATA_MISSING_FILE : GAME_DATA_IO_ERROR,
		                 "tyrian.shp", "The Tyrian shape data file could not be opened.");
	uint16_t count = fileReadU16(&file);
	fileClose(&file);
	if (file.error)
		return dataError(provider, error, GAME_DATA_IO_ERROR, "tyrian.shp", "The Tyrian shape data header could not be read.");
	if (count == 11)
		return dataError(provider, error, GAME_DATA_WRONG_VARIANT, "tyrian.shp", "The Tyrian v1.0/v1.1 data files were found.  OpenTyrian requires the Tyrian v2.0/v2.1 data files.");
	if (count == 13)
		return dataError(provider, error, GAME_DATA_WRONG_VARIANT, "tyrian.shp", "The Tyrian 2000 data files were found.  OpenTyrian requires the Tyrian v2.0/v2.1 data files.");
	return dataError(provider, error, GAME_DATA_OK, "", "");
}

File gameDataOpen(const GameDataProvider *provider, const char *filename)
{
	// Asset names are basenames. No writes, absolute paths or traversal.
	if (provider == NULL || provider->directory == NULL || filename[0] == '\0' ||
	    strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0 ||
	    strpbrk(filename, "/\\:") != NULL)
		return (File) { NULL, EACCES, true };
#ifndef NDEBUG
	for (size_t i = 0; filename[i] != '\0'; ++i)
		assert(!isupper((unsigned char)filename[i]));
#endif
	const char *directory = provider->directory;
	size_t size = strlen(directory) + 1 + strlen(filename) + 1;
	char *path = malloc(size);
	if (path == NULL)
		return (File) { NULL, ENOMEM, true };
	if (directory[0] == '\0')
		snprintf(path, size, "%s", filename);
	else
		snprintf(path, size, "%s/%s", directory, filename);
	errno = 0;
	FILE *f = fopen(path, "rb");
	File file = { f, errno, f == NULL };
	free(path);
	return file;
}

bool gameDataExists(const GameDataProvider *provider, const char *filename)
{
	File file = gameDataOpen(provider, filename);
	bool exists = !file.error;
	fileClose(&file);
	return exists;
}

const char *gameDataDirectory(const GameDataProvider *provider)
{
	return provider != NULL && provider->directory != NULL && provider->directory[0] != '\0' ? provider->directory : ".";
}

const GameVariantDef *gameDataVariant(const GameDataProvider *provider)
{
	return provider != NULL ? provider->variant : NULL;
}

const char *gameDataStatusName(GameDataStatus status)
{
	static const char *const names[] =
	{
		"ok", "not-found", "wrong-variant", "missing-file", "bad-size", "bad-hash", "io-error", "unavailable"
	};
	return (unsigned)status < COUNTOF(names) ? names[status] : "unknown";
}

void gameDataClose(GameDataProvider *provider)
{
	if (provider == NULL)
		return;
	if (currentProvider == provider)
		currentProvider = NULL;
	free(provider->directory);
	free(provider);
}

GameDataProvider *gameDataCurrent(void)
{
	return currentProvider;
}

bool findDataFiles(void)
{
	gameDataClose(currentProvider);
	GameDataSearch search = { customDataDirPath, NULL, NULL };
	GameDataStatus status = gameDataLocate(gameVariantCurrent(), &search, &currentProvider, NULL);
	return status == GAME_DATA_OK;
}
