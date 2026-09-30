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

#include "game_schema.h"
#include "installer.h"
#include "logging.h"
#include "regress.h"
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

// What identifies and completes a Tyrian 2000 installation.  Nothing here is
// taken from the 2.1 directory: an incomplete 2000 root is an error, never
// filled in per file from the 2.1 data.
static const char *const files2000[] =
{
	"tyrian.shp", "tyrian.hdt", "tyrian.pic", "palette.dat", "tyrian.snd", "voices.snd",
	"music.mus", "tyrian.cdt", "estsc.shp",
	"tyrian1.lvl", "tyrian2.lvl", "tyrian3.lvl", "tyrian4.lvl", "tyrian5.lvl",
	"levels1.dat", "levels2.dat", "levels3.dat", "levels4.dat", "levels5.dat",
	"cubetxt1.dat", "cubetxt2.dat", "cubetxt3.dat", "cubetxt4.dat", "cubetxt5.dat"
};

#define MESSAGE_NOT_FOUND_21 "The Tyrian data files were not found.  OpenTyrian requires the Tyrian v2.0/v2.1 data files."
#define MESSAGE_NOT_FOUND_2000 "The Tyrian 2000 data files were not found.  Tyrian 2000 requires its own data files; use --data=<directory> or TYRIAN2000_DATA."

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
	if (variant == NULL || (variant->id != VARIANT_TYRIAN21 && variant->id != VARIANT_TYRIAN2000))
		return dataError(NULL, error, GAME_DATA_UNAVAILABLE, "", "The game variant is not available.");
	const bool is2000 = variant->id == VARIANT_TYRIAN2000;
	const char *notFound = is2000 ? MESSAGE_NOT_FOUND_2000 : MESSAGE_NOT_FOUND_21;

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
		return dataError(provider, error, GAME_DATA_NOT_FOUND, "tyrian1.lvl", notFound);
	}

	if (is2000)
	{
		// The default search: TYRIAN2000_DATA, the installer's per-user location,
		// then a tyrian2000 directory beside the executable or in the working
		// directory.  Never ./data, which holds the Tyrian 2.1 files.
		char installed[INSTALLER_PATH_MAX];
		if (!installerInstallDirectory(installed, sizeof installed))
			installed[0] = '\0';
		const char *basePath = SDL_GetBasePath();
		char *baseData = NULL;
		if (basePath != NULL)
		{
			size_t size = strlen(basePath) + sizeof "tyrian2000";
			baseData = malloc(size);
			if (baseData != NULL)
				snprintf(baseData, size, "%styrian2000", basePath);
		}
		const char *directories[] = { search->installed_directory, getenv("TYRIAN2000_DATA"), installed, baseData, "tyrian2000" };
		GameDataStatus status = GAME_DATA_NOT_FOUND;
		for (size_t i = 0; i < COUNTOF(directories); ++i)
		{
			if (directories[i] == NULL || directories[i][0] == '\0')
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
		                 status == GAME_DATA_OK ? "" : notFound);
	}

	// Tyrian 2.1: match the legacy executable/package, compiled system path, cwd order.
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
		// A regress-only default-search decoy, so tests never write into the
		// actual system installation directory.
		regress_data_audit_root != NULL && getenv("TYRIAN_REGRESS_DEFAULT") != NULL
			? getenv("TYRIAN_REGRESS_DEFAULT") : TYRIAN_DIR,
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

// Reads the leading u16 count of a data file, or reports why it cannot.
static GameDataStatus readCount(GameDataProvider *provider, GameDataError *error,
                                const char *filename, uint16_t *count)
{
	File file = gameDataOpen(provider, filename);
	if (file.error)
		return dataError(provider, error, file.errnum == ENOENT ? GAME_DATA_MISSING_FILE : GAME_DATA_IO_ERROR,
		                 filename, "A required Tyrian 2000 data file could not be opened.");
	*count = fileReadU16(&file);
	bool failed = file.error;
	fileClose(&file);
	if (failed)
		return dataError(provider, error, GAME_DATA_IO_ERROR, filename, "A Tyrian 2000 data file header could not be read.");
	return GAME_DATA_OK;
}

// A Tyrian 2000 installation: the shape file must be the 13-bank one, every
// file the engine reads must be present, and the counted containers must match
// the schema.  The exact size and checksum of the canonical archive are the
// regression suite's job (test/regress-2000/data-manifest.txt); a manually
// installed set may legitimately differ.
static GameDataStatus validate2000(GameDataProvider *provider, GameDataError *error)
{
	const GameDataSchema *schema = provider->variant->data_schema;

	uint16_t count;
	GameDataStatus status = readCount(provider, error, "tyrian.shp", &count);
	if (status != GAME_DATA_OK)
		return status;
	if (count == 12)
		return dataError(provider, error, GAME_DATA_WRONG_VARIANT, "tyrian.shp", "The Tyrian v2.0/v2.1 data files were found.  Tyrian 2000 requires the Tyrian 2000 data files.");
	if (count == 11)
		return dataError(provider, error, GAME_DATA_WRONG_VARIANT, "tyrian.shp", "The Tyrian v1.0/v1.1 data files were found.  Tyrian 2000 requires the Tyrian 2000 data files.");
	if (count != schema->main_shape_banks)
		return dataError(provider, error, GAME_DATA_BAD_SIZE, "tyrian.shp", "The Tyrian 2000 shape data file has an unexpected number of banks.");

	for (size_t i = 0; i < COUNTOF(files2000); ++i)
	{
		if (!gameDataExists(provider, files2000[i]))
			return dataError(provider, error, GAME_DATA_MISSING_FILE, files2000[i], "A required Tyrian 2000 data file is missing.");
	}

	status = readCount(provider, error, "tyrian.pic", &count);
	if (status != GAME_DATA_OK)
		return status;
	if (count != schema->picture_count)
		return dataError(provider, error, GAME_DATA_BAD_SIZE, "tyrian.pic", "The Tyrian 2000 picture file has an unexpected number of pictures.");

	status = readCount(provider, error, "tyrian.snd", &count);
	if (status != GAME_DATA_OK)
		return status;
	if (count != schema->sfx_count)
		return dataError(provider, error, GAME_DATA_BAD_SIZE, "tyrian.snd", "The Tyrian 2000 sound file has an unexpected number of effects.");

	File palettes = gameDataOpen(provider, "palette.dat");
	long paletteBytes = palettes.error ? -1 : fileGetLength(&palettes);
	fileClose(&palettes);
	if (paletteBytes != (long)schema->palette_count * 256 * 3)
		return dataError(provider, error, GAME_DATA_BAD_SIZE, "palette.dat", "The Tyrian 2000 palette file has an unexpected size.");

	return dataError(provider, error, GAME_DATA_OK, "", "");
}

GameDataStatus gameDataValidate(GameDataProvider *provider, GameDataError *error)
{
	if (provider == NULL)
		return dataError(NULL, error, GAME_DATA_NOT_FOUND, "", MESSAGE_NOT_FOUND_21);
	if (provider->status != GAME_DATA_OK)
	{
		if (error != NULL)
			*error = provider->error;
		return provider->status;
	}

	if (provider->variant->id == VARIANT_TYRIAN2000)
		return validate2000(provider, error);

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
	if (regress_data_audit_root != NULL)
		regress_audit_open(path);
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

static bool dataPrepared = false;

bool gameDataPrepare(void)
{
	if (dataPrepared)
		return true;

	GameDataError data_error;
	findDataFiles();
	GameDataStatus data_status = gameDataValidate(gameDataCurrent(), &data_error);
	logInfo("Game variant: %s; data root: %s; validation: %s%s%s.",
	        gameVariantCurrent()->log_label, gameDataDirectory(gameDataCurrent()),
	        gameDataStatusName(data_status), data_error.filename[0] != '\0' ? "; file: " : "",
	        data_error.filename);
	if (data_status != GAME_DATA_OK)
	{
		logFatal("%s", data_error.detail);
		return false;
	}

	dataPrepared = true;
	return true;
}

bool findDataFiles(void)
{
	gameDataClose(currentProvider);
	GameDataSearch search = { customDataDirPath, NULL, NULL };
	GameDataStatus status = gameDataLocate(gameVariantCurrent(), &search, &currentProvider, NULL);
	return status == GAME_DATA_OK;
}
