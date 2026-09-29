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
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "file.h"

#include "game_data.h"
#include "game_variant.h"
#include "logging.h"
#include "opentyr.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#if !defined(S_ISDIR) && defined(_S_IFMT) && defined(_S_IFDIR)
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif

const char *customDataDirPath = NULL;

bool steamDeck(void)
{
	// Steam sets SteamDeck=1 for the games it launches on a Deck, in Game Mode
	// and in Desktop Mode.  A build run from a plain Desktop-Mode terminal does
	// not get it, so fall back to the DMI identity, which is the reliable one:
	// the stable board_name is "Jupiter" on the LCD Deck and "Galileo" on the
	// OLED (product_name carries the same string).
	const char *steam_deck_env = getenv("SteamDeck");
	if (steam_deck_env != NULL && strcmp(steam_deck_env, "1") == 0)
		return true;

#ifdef __linux__
	static const char *const dmi_files[] =
	{
		"/sys/devices/virtual/dmi/id/board_name",
		"/sys/devices/virtual/dmi/id/product_name",
	};

	for (size_t i = 0; i < COUNTOF(dmi_files); ++i)
	{
		FILE *f = fopen(dmi_files[i], "r");
		if (f == NULL)
			continue;

		char name[64];
		const char *line = fgets(name, sizeof(name), f);
		fclose(f);

		if (line != NULL && (strstr(name, "Jupiter") != NULL || strstr(name, "Galileo") != NULL))
			return true;
	}
#endif

	return false;
}

enum
{
	ERRNUM_EOF = -1,
};

static char *userDirPath = NULL;
static size_t userDirPathLen = 0;
static bool userFilesDisabled = false;
static bool legacySaveReadOnly = false;

static bool fileExists(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f != NULL)
		fclose(f);
	return f != NULL;
}

static File fileOpen(const char *path, const char *mode)
{
	errno = 0;  // fopen might not set errno
	FILE *f = fopen(path, mode);
	return (File) { f, errno, f == NULL };
}

bool dataFileExists(const char *filename)
{
	File file = dataFileOpen(filename, "rb");

	bool result = !file.error;

	fileClose(&file);

	return result;
}

bool userFileExists(const char *filename)
{
	File file = userFileOpen(filename, "rb");

	bool result = !file.error;

	fileClose(&file);

	return result;
}

File dataFileOpen(const char *filename, const char *mode)
{
	if (strcmp(mode, "rb") != 0)
		return (File) { NULL, EACCES, true };
	if (gameDataCurrent() == NULL)
		findDataFiles();
	return gameDataOpen(gameDataCurrent(), filename);
}

static void determineUserDirPath(void)
{
	if (userDirPathLen != 0)
	{
		free(userDirPath);
		userDirPathLen = 0;
	}

	const char *basePath = SDL_GetBasePath();
	if (basePath != NULL)
	{
		// If a certain file exists in the base path, store user files there.
		const char *const filename = "opentyrian.cfg";

		size_t filePathSize = strlen(basePath) + strlen(filename) + 1;
		char *filePath = malloc(filePathSize);
		snprintf(filePath, filePathSize, "%s%s", basePath, filename);

		bool portable = fileExists(filePath);
		
		free(filePath);

		if (portable)
		{
			userDirPathLen = strlen(basePath) - 1;  // Trim trailing slash.
			size_t userDirPathSize = userDirPathLen + 1;
			userDirPath = malloc(userDirPathSize);
			snprintf(userDirPath, userDirPathSize, "%s", basePath);
		}

		if (portable)
			return;
	}

#ifdef TARGET_WIN32
	const char *appData = getenv("APPDATA");
	if (appData != NULL)
	{
		userDirPathLen = strlen(appData) + strlen("/OpenTyrian");
		size_t userDirPathSize = userDirPathLen + 1;
		userDirPath = malloc(userDirPathSize);
		snprintf(userDirPath, userDirPathSize, "%s/OpenTyrian", appData);
		return;
	}
#else
	const char *xdgConfigHome = getenv("XDG_CONFIG_HOME");
	if (xdgConfigHome != NULL)
	{
		userDirPathLen = strlen(xdgConfigHome) + strlen("/opentyrian");
		size_t userDirPathSize = userDirPathLen + 1;
		userDirPath = malloc(userDirPathSize);
		snprintf(userDirPath, userDirPathSize, "%s/opentyrian", xdgConfigHome);
		return;
	}

	const char *home = getenv("HOME");
	if (home != NULL)
	{
		userDirPathLen = strlen(home) + strlen("/.config/opentyrian");
		size_t userDirPathSize = userDirPathLen + 1;
		userDirPath = malloc(userDirPathSize);
		snprintf(userDirPath, userDirPathSize, "%s/.config/opentyrian", home);
		return;
	}
#endif

	userDirPath = "";
	userDirPathLen = 0;
}

const char *userDirGet(void)
{
	if (userFilesDisabled)
		return "";
	if (userDirPath == NULL)
		determineUserDirPath();

	return userDirPath != NULL ? userDirPath : "";
}

bool userDirPrepare(void)
{
	if (userFilesDisabled)
		return false;
	if (userDirPath == NULL)
		determineUserDirPath();

	if (userDirPathLen == 0)
		return false;

	// Ignore the error, like the per-file open below always did: a failed
	// mkdir just means the later fopen() reports the real problem.
#ifdef _WIN32
	(void)_mkdir(userDirPath);
#else
	(void)mkdir(userDirPath, 0700);
#endif

	return true;
}

void userFilesDisable(void)
{
	userFilesDisabled = true;
}

bool userFilesEnabled(void)
{
	return !userFilesDisabled;
}

bool userFilesEnable(const char *root)
{
	if (root != NULL)
	{
		if (root[0] == '\0')
			return false;
		char *path = malloc(strlen(root) + 1);
		if (path == NULL)
			return false;
		strcpy(path, root);
		if (userDirPathLen != 0)
			free(userDirPath);
		userDirPath = path;
		userDirPathLen = strlen(path);
	}
	userFilesDisabled = false;
	legacySaveReadOnly = false;
	return true;
}

static char *userFilePath(UserFileKind kind, const char *filename)
{
	const char *root = userDirGet();
	if (root[0] == '\0')
		root = ".";
	const char *space = kind == USER_FILE_SHARED ? "" : gameVariantCurrent()->save_namespace;
	size_t size = strlen(root) + strlen(space) + strlen(filename) + 3;
	char *path = malloc(size);
	if (path != NULL)
		snprintf(path, size, "%s/%s%s%s", root, space, space[0] != '\0' ? "/" : "", filename);
	return path;
}

static void userNamespacePrepare(void)
{
	if (!userFilesEnabled())
		return;
	(void)userDirPrepare();
	char *path = userFilePath(USER_FILE_SHARED, gameVariantCurrent()->save_namespace);
	if (path != NULL)
	{
#ifdef _WIN32
		(void)_mkdir(path);
#else
		(void)mkdir(path, 0700);
#endif
		free(path);
	}
}

bool userSavesWritable(void)
{
	return userFilesEnabled() && !legacySaveReadOnly;
}

File userFileOpenKind(UserFileKind kind, const char *filename, const char *mode)
{
	if (!userFilesEnabled())
		return (File) { NULL, EACCES, true };
	bool legacy_read = kind == USER_FILE_VARIANT_SAVE && legacySaveReadOnly;
	if (legacy_read)
	{
		if (strcmp(mode, "rb") != 0)
			return (File) { NULL, EACCES, true };
		kind = USER_FILE_SHARED;
	}
	if (kind == USER_FILE_SHARED)
		(void)userDirPrepare();
	else
		userNamespacePrepare();
	char *path = userFilePath(kind, filename);
	if (path == NULL)
		return (File) { NULL, ENOMEM, true };
	File file = fileOpen(path, mode);
	free(path);
	// A failure before inspecting the source must not expose a 2000 prefix.
	if (legacy_read && !file.error && fileGetLength(&file) != 2502)
	{
		fileClose(&file);
		file.errnum = EINVAL;
		file.error = true;
	}
	return file;
}

File userFileOpen(const char *filename, const char *mode)
{
	return userFileOpenKind(USER_FILE_SHARED, filename, mode);
}

bool userFileExistsKind(UserFileKind kind, const char *filename)
{
	File file = userFileOpenKind(kind, filename, "rb");
	bool exists = !file.error;
	fileClose(&file);
	return exists;
}

static File fileCreateExclusive(const char *path)
{
#ifdef _WIN32
	int fd = _open(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
	int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
	if (fd < 0)
		return (File) { NULL, errno, true };
#ifdef _WIN32
	FILE *f = _fdopen(fd, "wb");
#else
	FILE *f = fdopen(fd, "wb");
#endif
	if (f == NULL)
	{
		int error = errno;
#ifdef _WIN32
		_close(fd);
#else
		close(fd);
#endif
		(void)remove(path);
		return (File) { NULL, error, true };
	}
	return (File) { f, 0, false };
}

static int publishLegacySave(const char *temp, const char *destination)
{
#ifndef _WIN32
	// Atomic no-clobber publication, even when two starts race to migrate.
	if (link(temp, destination) == 0)
		return 0;
	int error = errno;
	if (error != EPERM && error != EXDEV && error != ENOTSUP && error != EOPNOTSUPP)
		return error;
	// FAT/exFAT has no hard links. Recheck before the portable rename fallback.
#endif
	struct stat info;
	if (stat(destination, &info) == 0)
		return EEXIST;
	if (errno != ENOENT)
		return errno;
	return rename(temp, destination) == 0 ? 0 : errno;
}

// Returns 0 when path is missing or a directory, otherwise the errno that
// blocks it.  POSIX reports a path below a regular file as ENOTDIR while
// Windows reports ENOENT, so a directory that is really a file is checked
// explicitly to behave the same everywhere.
static int dirObstruction(const char *path)
{
	struct stat info;
	if (stat(path, &info) != 0)
		return errno == ENOENT ? 0 : errno;
	return S_ISDIR(info.st_mode) ? 0 : ENOTDIR;
}

void userPathsMigrateLegacy21(void)
{
	if (!userFilesEnabled() || gameVariantCurrent()->id != VARIANT_TYRIAN21)
		return;

	char *destination = userFilePath(USER_FILE_VARIANT_SAVE, "tyrian.sav");
	char *temp = userFilePath(USER_FILE_VARIANT_SAVE, "tyrian.sav.tmp");
	char *space = userFilePath(USER_FILE_SHARED, gameVariantCurrent()->save_namespace);
	int error = 0;
	struct stat info;
	if (destination == NULL || temp == NULL || space == NULL)
		error = ENOMEM;
	else
	{
		const char *root = userDirGet();
		error = dirObstruction(root[0] != '\0' ? root : ".");
		if (error == 0)
			error = dirObstruction(space);
	}
	if (error != 0)
		goto failed;

	if (stat(destination, &info) == 0)
	{
		logInfo("Save migration: nothing to migrate (tyrian21/tyrian.sav already exists).");
		goto done;
	}
	if (errno != ENOENT)
	{
		error = errno;
		goto failed;
	}

	File source = userFileOpen("tyrian.sav", "rb");
	if (source.error)
	{
		error = source.errnum;
		if (error == ENOENT)
		{
			logInfo("Save migration: nothing to migrate (no root tyrian.sav).");
			goto done;
		}
		goto failed;
	}
	long length = fileGetLength(&source);
	if (!source.error && length != 2502)
	{
		fileClose(&source);
		logWarn("Save migration: skipped root tyrian.sav (length %ld, expected 2502).", length);
		goto done;
	}
	Uint8 data[2502];
	fileReadExactly(&source, data, sizeof data);
	fileClose(&source);
	if (source.error)
	{
		error = source.errnum;
		goto failed;
	}

	userNamespacePrepare();
	File copy = fileCreateExclusive(temp);
	if (copy.error)
	{
		error = copy.errnum;
		goto failed;
	}
	fileWrite(&copy, data, sizeof data);
	fileFlush(&copy);
	fileClose(&copy);
	if (copy.error)
		error = copy.errnum;
	else
		error = publishLegacySave(temp, destination);
	(void)remove(temp);
	if (error == EEXIST)
	{
		logInfo("Save migration: nothing to migrate (another start created the destination).");
		goto done;
	}
	if (error != 0 || copy.error)
		goto failed;
	logInfo("Save migration: migrated root tyrian.sav to tyrian21/tyrian.sav; original kept.");
	goto done;

failed:
	legacySaveReadOnly = true;
	logWarn("Save migration: skipped (%s); using root save read-only, saving disabled for this session.",
	        error != 0 ? strerror(error) : "copy failed");
done:
	free(space);
	free(temp);
	free(destination);
}

void fileSetPosition(File *file, long position)
{
	if (file->error)
		return;

	errno = 0;  // fseek might not set errno
	if (fseek(file->f, position, SEEK_SET) == 0)
		return;

	file->errnum = errno;
	file->error = true;
}

long fileGetPosition(File *file)
{
	if (file->error)
		return 0;

	errno = 0;  // ftell might not set errno
	long position = ftell(file->f);
	if (position >= 0)
		return position;

	file->errnum = errno;
	file->error = true;

	return 0;
}

long fileGetLength(File *file)
{
	if (file->error)
		return 0;

	errno = 0;  // fseek/ftell might not set errno
	long position = ftell(file->f);
	if (position >= 0 &&
	    fseek(file->f, 0, SEEK_END) == 0)
	{
		long length = ftell(file->f);
		if (length >= 0 &&
		    fseek(file->f, position, SEEK_SET) == 0)
		{
			return length;
		}
	}

	file->errnum = errno;
	file->error = true;

	return 0;
}

size_t fileReadAtMost(File *file, void *data, size_t size)
{
	if (file->error)
		return 0;

	errno = 0;  // fread might not set errno
	size_t read = fread(data, 1, size, file->f);
	if (read == size)
		return read;

	file->errnum = errno;
	file->error = ferror(file->f) != 0;
	assert(file->error || feof(file->f) != 0);

	return read;
}

void fileReadExactly(File *file, void *data, size_t size)
{
	if (file->error)
	{
		memset(data, 0, size);
		return;
	}

	errno = 0;  // fread might not set errno
	size_t read = fread(data, 1, size, file->f);
	if (read == size)
		return;

	file->errnum = errno;
	file->error = true;

	if (file->errnum == 0)
		file->errnum = ERRNUM_EOF;

	memset((uint8_t *)data + read, 0, size - read);
}

void fileWrite(File *file, const void *data, size_t size)
{
	if (file->error)
		return;

	errno = 0;  // fwrite might not set errno
	size_t written = fwrite(data, 1, size, file->f);
	if (written == size)
		return;

	file->errnum = errno;
	file->error = true;

	assert(written < size);
}

void fileFlush(File *file)
{
	if (file->error)
		return;

	errno = 0;  // fflush might not set errno
	if (fflush(file->f) == 0)
		return;

	file->errnum = errno;
	file->error = true;
}

void fileClose(File *file)
{
	if (file->f == NULL)
		return;

	errno = 0;  // fclose might not set errno
	int result = fclose(file->f);
	file->f = NULL;
	if (result == 0)
		return;

	file->errnum = errno;
	file->error = true;
}

const char *fileGetError(File *file)
{
	switch (file->errnum)
	{
	case ERRNUM_EOF:
		return "Unexpected end of file";
	case 0:
		if (file->error)
			return "Unknown error";
		// fall through
	default:
		return strerror(file->errnum);
	}
}
