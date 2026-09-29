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
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "installer.h"

#include "game_data.h"
#include "game_variant.h"
#include "installer_hash.h"
#include "installer_manifest.h"
#include "logging.h"
// Only inflate and stored entries are read.  These must match how miniz.c is
// built (Makefile, visualc/opentyrian.vcxproj), or the structures would differ.
#ifndef MINIZ_NO_STDIO
#define MINIZ_NO_STDIO
#endif
#ifndef MINIZ_NO_TIME
#define MINIZ_NO_TIME
#endif
#ifndef MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_DEFLATE_APIS
#endif
#ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#endif
#include "third_party/miniz/miniz.h"

#include <SDL3/SDL.h>

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TARGET_WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define COUNT_OF(array) (sizeof (array) / sizeof *(array))

// Limits on what a zip may claim.  The real archive has 100 entries and 12.3 MB.
#define ZIP_MAX_ENTRIES 1000
#define ZIP_MAX_ENTRY_SIZE (64u * 1024 * 1024)
#define ZIP_MAX_TOTAL_SIZE (256u * 1024 * 1024)
#define ZIP_MAX_ARCHIVE_SIZE (256u * 1024 * 1024)

// How far below a chosen folder the Tyrian 2000 files are looked for.
#define FOLDER_SEARCH_DEPTH 4
#define FOLDER_SEARCH_VISITS 400

#define INSTALL_DIRECTORY_NAME "data-tyrian2000"
#define TEMP_DIRECTORY_NAME INSTALL_DIRECTORY_NAME ".tmp"
#define OLD_DIRECTORY_NAME INSTALL_DIRECTORY_NAME ".old"
#define PART_FILE_NAME INSTALLER_EXPECTED_FILENAME ".part"

#define MESSAGE_MANUAL "Choose the Tyrian 2000 zip file or folder instead."

// ---- what the archive must be (replaceable only by the test spec) ----------------

typedef struct
{
	const InstallerManifestEntry *entries;
	size_t count;
	uint64_t archiveSize;
	char archiveSha256[65];
} Manifest;

static Manifest manifest =
{
	installerCanonicalManifest,
	COUNT_OF(installerCanonicalManifest),
	INSTALLER_EXPECTED_SIZE,
	INSTALLER_EXPECTED_SHA256
};

// Test spec only: makes the headless run cancel itself after this many ms.
static unsigned cliCancelAfterMs = 0;

bool installerLoadTestSpec(const char *path)
{
	SDL_IOStream *io = SDL_IOFromFile(path, "rb");
	if (io == NULL)
		return false;
	size_t size = 0;
	char *text = SDL_LoadFile_IO(io, &size, true);
	if (text == NULL)
		return false;

	InstallerManifestEntry *entries = calloc(256, sizeof *entries);
	Manifest spec = { entries, 0, manifest.archiveSize, "" };
	snprintf(spec.archiveSha256, sizeof spec.archiveSha256, "%s", manifest.archiveSha256);
	bool ok = entries != NULL;

	for (char *line = text; ok && line != NULL && *line != '\0';)
	{
		char *next = strchr(line, '\n');
		if (next != NULL)
			*next++ = '\0';
		size_t length = strlen(line);
		if (length > 0 && line[length - 1] == '\r')
			line[--length] = '\0';

		unsigned long long number;
		unsigned long crc;
		int nameAt = 0;
		char hex[80];
		if (line[0] == '#' || line[0] == '\0')
		{
		}
		else if (sscanf(line, "archive-size %llu", &number) == 1 && number > 0 && number <= ZIP_MAX_ARCHIVE_SIZE)
			spec.archiveSize = number;
		else if (sscanf(line, "cancel-after-ms %llu", &number) == 1)
			cliCancelAfterMs = (unsigned)number;
		else if (sscanf(line, "archive-sha256 %79s", hex) == 1 && strlen(hex) == 64 && strspn(hex, "0123456789abcdef") == 64)
			snprintf(spec.archiveSha256, sizeof spec.archiveSha256, "%s", hex);
		else if (sscanf(line, "%llu %lu %n", &number, &crc, &nameAt) >= 2 && nameAt > 0 && line[nameAt] != '\0' &&
		         spec.count < 256 && strlen(line + nameAt) < 64 && number <= ZIP_MAX_ENTRY_SIZE &&
		         crc <= UINT32_MAX && strpbrk(line + nameAt, "/\\:") == NULL && line[nameAt] != '.')
		{
			entries[spec.count].size = (unsigned long)number;
			entries[spec.count].crc = crc;
			entries[spec.count].name = SDL_strdup(line + nameAt);
			if (entries[spec.count].name == NULL)
				ok = false;
			for (size_t i = 0; i < spec.count; ++i)
				if (SDL_strcasecmp(entries[i].name, line + nameAt) == 0)
					ok = false;
			++spec.count;
		}
		else
			ok = false;
		line = next;
	}
	SDL_free(text);
	if (!ok || spec.count == 0)
	{
		for (size_t i = 0; i < spec.count; ++i)
			SDL_free((void *)entries[i].name);
		free(entries);
		return false;
	}
	manifest = spec;
	return true;
}

// ---- results and progress ------------------------------------------------------

typedef struct
{
	InstallerError error;
	bool cancelled;
	char message[INSTALLER_MESSAGE_MAX];
} Failure;

static bool failWith(Failure *failure, InstallerError error, const char *format, ...) SDL_PRINTF_VARARG_FUNC(3);
static bool failWith(Failure *failure, InstallerError error, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	failure->error = error;
	vsnprintf(failure->message, sizeof failure->message, format, args);
	va_end(args);
	return false;
}

typedef enum
{
	JOB_DOWNLOAD,
	JOB_ZIP,
	JOB_FOLDER
} JobKind;

static struct
{
	SDL_Mutex *mutex;
	SDL_Thread *thread;
	InstallerProgress progress;
	bool working;
	SDL_AtomicInt cancel;
	JobKind kind;
	char source[INSTALLER_PATH_MAX];
} installer;

static bool installerInit(void)
{
	if (installer.mutex == NULL)
		installer.mutex = SDL_CreateMutex();
	return installer.mutex != NULL;
}

static void progressSet(InstallerState state, const char *message, uint64_t done, uint64_t total)
{
	SDL_LockMutex(installer.mutex);
	installer.progress.state = state;
	installer.progress.bytesDone = done;
	installer.progress.bytesTotal = total;
	snprintf(installer.progress.message, sizeof installer.progress.message, "%s", message);
	SDL_UnlockMutex(installer.mutex);
}

static void progressBytes(uint64_t done, uint64_t total)
{
	SDL_LockMutex(installer.mutex);
	installer.progress.bytesDone = done;
	installer.progress.bytesTotal = total;
	SDL_UnlockMutex(installer.mutex);
}

static bool cancelRequested(Failure *failure)
{
	if (SDL_GetAtomicInt(&installer.cancel) == 0)
		return false;
	failure->cancelled = true;
	return true;
}

// ---- small file system helpers -------------------------------------------------

static bool joinPath(char *out, size_t size, const char *directory, const char *name)
{
	int written = snprintf(out, size, "%s/%s", directory, name);
	return written > 0 && (size_t)written < size;
}

static bool pathInfo(const char *path, SDL_PathInfo *info)
{
	SDL_PathInfo local;
	return SDL_GetPathInfo(path, info != NULL ? info : &local);
}

static bool isDirectory(const char *path)
{
	SDL_PathInfo info;
	return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

static bool isFile(const char *path)
{
	SDL_PathInfo info;
	return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

static bool isLink(const char *path)
{
#ifdef TARGET_WIN32
	wchar_t *wide = (wchar_t *)SDL_iconv_string("UTF-16LE", "UTF-8", path, strlen(path) + 1);
	if (wide == NULL)
		return true;
	DWORD attributes = GetFileAttributesW(wide);
	SDL_free(wide);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
	struct stat info;
	return lstat(path, &info) == 0 && S_ISLNK(info.st_mode);
#endif
}

typedef struct
{
	char **names;
	size_t count;
	bool failed;
} NameList;

static SDL_EnumerationResult SDLCALL collectName(void *userdata, const char *directory, const char *name)
{
	(void)directory;
	NameList *list = userdata;
	char **grown = realloc(list->names, (list->count + 1) * sizeof *grown);
	if (grown == NULL)
	{
		list->failed = true;
		return SDL_ENUM_FAILURE;
	}
	list->names = grown;
	list->names[list->count] = SDL_strdup(name);
	if (list->names[list->count] == NULL)
	{
		list->failed = true;
		return SDL_ENUM_FAILURE;
	}
	++list->count;
	return SDL_ENUM_CONTINUE;
}

static int compareNames(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

static bool listDirectory(const char *path, NameList *list)
{
	*list = (NameList) { NULL, 0, false };
	bool ok = SDL_EnumerateDirectory(path, collectName, list) && !list->failed;
	if (list->count > 1)
		qsort(list->names, list->count, sizeof *list->names, compareNames);
	return ok;
}

static void freeNames(NameList *list)
{
	for (size_t i = 0; i < list->count; ++i)
		SDL_free(list->names[i]);
	free(list->names);
	*list = (NameList) { NULL, 0, false };
}

// Removes a file, or a directory and everything below it.  A symbolic link is
// removed as the link, never followed.
static bool removeTree(const char *path)
{
	if (SDL_RemovePath(path))
		return true;
	if (isLink(path))
		return false;
	if (!pathInfo(path, NULL))
		return true;
	if (!isDirectory(path))
		return false;
	NameList list;
	bool ok = listDirectory(path, &list);
	for (size_t i = 0; i < list.count; ++i)
	{
		char child[INSTALLER_PATH_MAX];
		if (!joinPath(child, sizeof child, path, list.names[i]) || !removeTree(child))
			ok = false;
	}
	freeNames(&list);
	return ok && SDL_RemovePath(path);
}

// Freeze relative input paths and collapse separators and dot components
// before checking whether an input is inside our temporary space.
static bool absolutePath(const char *path, char *out, size_t size)
{
	bool absolute = path[0] == '/' || path[0] == '\\' ||
	                (isalpha((unsigned char)path[0]) && path[1] == ':');
	char *cwd = absolute ? NULL : SDL_GetCurrentDirectory();
	if (!absolute && cwd == NULL)
		return false;
	char combined[INSTALLER_PATH_MAX];
	int length = snprintf(combined, sizeof combined, "%s%s", cwd != NULL ? cwd : "", path);
	SDL_free(cwd);
	if (length <= 0 || (size_t)length >= sizeof combined || (size_t)length >= size)
		return false;
	for (int i = 0; i < length; ++i)
		if (combined[i] == '\\') combined[i] = '/';
	size_t used = 0, starts[INSTALLER_PATH_MAX / 2], components = 0;
	for (char *p = combined; *p != '\0';)
	{
		if (*p == '/') { out[used++] = *p++; continue; }
		char *end = strchr(p, '/');
		size_t n = end != NULL ? (size_t)(end - p) : strlen(p);
		if (n == 1 && p[0] == '.')
		{
		}
		else if (n == 2 && p[0] == '.' && p[1] == '.')
		{
			if (components > 0) used = starts[--components];
		}
		else
		{
			starts[components++] = used;
			memcpy(out + used, p, n); used += n;
			if (end != NULL) out[used++] = '/';
		}
		p += n;
		while (*p == '/') ++p;
	}
	while (used > 1 && out[used - 1] == '/') --used;
	out[used] = '\0';
	return true;
}

static bool sameOrInside(const char *path, const char *root)
{
	size_t length = strlen(root);
	while (length > 1 && (root[length - 1] == '/' || root[length - 1] == '\\'))
		--length;
	if (SDL_strncasecmp(path, root, length) != 0)
		return false;
	return path[length] == '\0' || path[length] == '/' || path[length] == '\\';
}

static bool namesMatch(const char *a, const char *b)
{
	return SDL_strcasecmp(a, b) == 0;
}

// Size and POSIX cksum CRC of a file, streamed.
static bool cksumFile(const char *path, uint64_t *size, uint32_t *crc)
{
	SDL_IOStream *io = SDL_IOFromFile(path, "rb");
	if (io == NULL)
		return false;
	PosixCksum sum;
	posixCksumInit(&sum);
	uint8_t buffer[65536];
	for (;;)
	{
		size_t got = SDL_ReadIO(io, buffer, sizeof buffer);
		if (got == 0)
			break;
		posixCksumUpdate(&sum, buffer, got);
	}
	bool ok = SDL_GetIOStatus(io) == SDL_IO_STATUS_EOF;
	SDL_CloseIO(io);
	*size = sum.length;
	*crc = posixCksumFinish(&sum);
	return ok;
}

static bool sha256File(const char *path, uint64_t *size, char hex[65])
{
	SDL_IOStream *io = SDL_IOFromFile(path, "rb");
	if (io == NULL)
		return false;
	Sha256 sha;
	sha256Init(&sha);
	uint8_t buffer[65536];
	for (;;)
	{
		size_t got = SDL_ReadIO(io, buffer, sizeof buffer);
		if (got == 0)
			break;
		sha256Update(&sha, buffer, got);
	}
	bool ok = SDL_GetIOStatus(io) == SDL_IO_STATUS_EOF;
	SDL_CloseIO(io);
	*size = sha.length;
	sha256FinishHex(&sha, hex);
	return ok;
}

// ---- where things live -----------------------------------------------------------

bool installerInstallDirectory(char *out, size_t size)
{
	if (out == NULL || size == 0)
		return false;
	out[0] = '\0';

	// Portable mode: opentyrian.cfg beside the executable keeps everything there.
	const char *base = SDL_GetBasePath();
	if (base != NULL)
	{
		char config[INSTALLER_PATH_MAX];
		snprintf(config, sizeof config, "%sopentyrian.cfg", base);
		if (isFile(config))
		{
			int written = snprintf(out, size, "%s" INSTALL_DIRECTORY_NAME, base);
			if (written > 0 && (size_t)written < size)
				return true;
			out[0] = '\0';
			return false;
		}
	}

	int written = 0;
#ifdef TARGET_WIN32
	const char *appData = getenv("APPDATA");
	if (appData == NULL || appData[0] == '\0')
		return false;
	written = snprintf(out, size, "%s/OpenTyrian/" INSTALL_DIRECTORY_NAME, appData);
#elif defined(__APPLE__)
	const char *home = getenv("HOME");
	if (home == NULL || home[0] == '\0')
		return false;
	written = snprintf(out, size, "%s/Library/Application Support/OpenTyrian/" INSTALL_DIRECTORY_NAME, home);
#else
	const char *dataHome = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");
	if (dataHome != NULL && dataHome[0] == '/')
		written = snprintf(out, size, "%s/opentyrian/" INSTALL_DIRECTORY_NAME, dataHome);
	else if (home != NULL && home[0] != '\0')
		written = snprintf(out, size, "%s/.local/share/opentyrian/" INSTALL_DIRECTORY_NAME, home);
	else
		return false;
#endif
	if (written <= 0 || (size_t)written >= size)
	{
		out[0] = '\0';
		return false;
	}
	return true;
}

// The working paths of one install, all beside the install location so the final
// rename never crosses a file system.
typedef struct
{
	char install[INSTALLER_PATH_MAX];
	char parent[INSTALLER_PATH_MAX];
	char part[INSTALLER_PATH_MAX];
	char temp[INSTALLER_PATH_MAX];
	char old[INSTALLER_PATH_MAX];
	char lock[INSTALLER_PATH_MAX];
} Paths;

// Advisory lock survives a crash as a harmless file, while the OS releases
// ownership. This keeps independent CLI/launcher processes out of one another's
// staging area. Windows denies sharing; Unix uses a nonblocking fcntl lock.
typedef struct
{
#ifdef TARGET_WIN32
	HANDLE handle;
#else
	int fd;
#endif
} InstallLock;

static bool lockInstall(const Paths *paths, InstallLock *lock, Failure *failure)
{
#ifdef TARGET_WIN32
	wchar_t *wide = (wchar_t *)SDL_iconv_string("UTF-16LE", "UTF-8", paths->lock, strlen(paths->lock) + 1);
	lock->handle = wide != NULL ? CreateFileW(wide, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
	                                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL) : INVALID_HANDLE_VALUE;
	SDL_free(wide);
	if (lock->handle != INVALID_HANDLE_VALUE) return true;
#else
	lock->fd = open(paths->lock, O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
	if (lock->fd >= 0)
	{
		struct flock request = { 0 };
		request.l_type = F_WRLCK;
		request.l_whence = SEEK_SET;
		if (fcntl(lock->fd, F_SETLK, &request) == 0) return true;
		close(lock->fd);
		lock->fd = -1;
	}
#endif
	return failWith(failure, INSTALLER_ERROR_BUSY, "The install location is busy or unavailable. Close other installers and check its permissions, then try again.");
}

static void unlockInstall(InstallLock *lock)
{
#ifdef TARGET_WIN32
	CloseHandle(lock->handle);
#else
	close(lock->fd);
#endif
}

static bool resolvePaths(Paths *paths, Failure *failure)
{
	if (!installerInstallDirectory(paths->install, sizeof paths->install))
		return failWith(failure, INSTALLER_ERROR_IO, "There is no place to install Tyrian 2000 data on this system.");
	char normalized[INSTALLER_PATH_MAX];
	if (!absolutePath(paths->install, normalized, sizeof normalized))
		return failWith(failure, INSTALLER_ERROR_IO, "The install location path is too long.");
	snprintf(paths->install, sizeof paths->install, "%s", normalized);

	snprintf(paths->parent, sizeof paths->parent, "%s", paths->install);
	char *slash = strrchr(paths->parent, '/');
	if (slash == NULL || slash == paths->parent)
		return failWith(failure, INSTALLER_ERROR_IO, "There is no place to install Tyrian 2000 data on this system.");
	*slash = '\0';

	if (!joinPath(paths->part, sizeof paths->part, paths->parent, PART_FILE_NAME) ||
	    !joinPath(paths->temp, sizeof paths->temp, paths->parent, TEMP_DIRECTORY_NAME) ||
	    !joinPath(paths->old, sizeof paths->old, paths->parent, OLD_DIRECTORY_NAME) ||
	    !joinPath(paths->lock, sizeof paths->lock, paths->parent, INSTALL_DIRECTORY_NAME ".lock"))
		return failWith(failure, INSTALLER_ERROR_IO, "The install location path is too long.");

	if (!SDL_CreateDirectory(paths->parent))
		return failWith(failure, INSTALLER_ERROR_IO, "Could not create %s. Check the permissions and free space, then try again.", paths->parent);
	return true;
}

// ---- validation ------------------------------------------------------------------

// The engine's own structural check of a Tyrian 2000 directory.
static bool validateStructure(const char *directory, Failure *failure)
{
	const GameVariantDef *variant = gameVariantGet(VARIANT_TYRIAN2000);
	GameDataSearch search = { directory, NULL, NULL };
	GameDataProvider *provider = NULL;
	GameDataError error = { GAME_DATA_OK, "", "" };

	GameDataStatus status = gameDataLocate(variant, &search, &provider, &error);
	if (status == GAME_DATA_OK)
		status = gameDataValidate(provider, &error);
	gameDataClose(provider);

	switch (status)
	{
	case GAME_DATA_OK:
		return true;
	case GAME_DATA_WRONG_VARIANT:
		return failWith(failure, INSTALLER_ERROR_WRONG_VARIANT, "%s", error.detail);
	case GAME_DATA_NOT_FOUND:
	case GAME_DATA_MISSING_FILE:
		return failWith(failure, INSTALLER_ERROR_INVALID_DATA, "The Tyrian 2000 data is incomplete: %s is missing.", error.filename);
	default:
		return failWith(failure, INSTALLER_ERROR_INVALID_DATA, "The Tyrian 2000 data is not usable: %s (%s).", error.detail, error.filename);
	}
}

typedef struct
{
	size_t missing, differing;
} ManifestReport;

// Compares every manifest file in the directory by size and CRC.  Only file
// names are logged.  False only when cancelled.
static bool compareManifest(const char *directory, ManifestReport *report, Failure *failure)
{
	*report = (ManifestReport) { 0, 0 };
	for (size_t i = 0; i < manifest.count; ++i)
	{
		if (cancelRequested(failure))
			return false;
		char path[INSTALLER_PATH_MAX];
		uint64_t size = 0;
		uint32_t crc = 0;
		if (!joinPath(path, sizeof path, directory, manifest.entries[i].name) || !isFile(path))
		{
			logWarn("Tyrian 2000 data: missing %s", manifest.entries[i].name);
			++report->missing;
		}
		else if (!cksumFile(path, &size, &crc) || size != manifest.entries[i].size || crc != manifest.entries[i].crc)
		{
			logWarn("Tyrian 2000 data: %s differs from the original release", manifest.entries[i].name);
			++report->differing;
		}
	}
	return true;
}

// ---- finding the files in a chosen folder ----------------------------------------

static bool hasFileNoCase(const NameList *list, const char *wanted)
{
	for (size_t i = 0; i < list->count; ++i)
		if (namesMatch(list->names[i], wanted))
			return true;
	return false;
}

static bool findRootAtDepth(const char *directory, int depth, int *visits, char *out, size_t outSize)
{
	if (*visits >= FOLDER_SEARCH_VISITS)
		return false;
	++*visits;

	NameList list;
	if (!listDirectory(directory, &list))
	{
		freeNames(&list);
		return false;
	}
	bool found = false;
	if (depth == 0)
	{
		if (hasFileNoCase(&list, "tyrian1.lvl"))
		{
			snprintf(out, outSize, "%s", directory);
			found = true;
		}
	}
	else
	{
		for (size_t i = 0; i < list.count && !found; ++i)
		{
			char child[INSTALLER_PATH_MAX];
			if (joinPath(child, sizeof child, directory, list.names[i]) && !isLink(child) && isDirectory(child))
				found = findRootAtDepth(child, depth - 1, visits, out, outSize);
		}
	}
	freeNames(&list);
	return found;
}

// The shallowest directory at or below `directory` that holds tyrian1.lvl.
static bool findDataRoot(const char *directory, char *out, size_t outSize)
{
	for (int depth = 0; depth <= FOLDER_SEARCH_DEPTH; ++depth)
	{
		int visits = 0;
		if (findRootAtDepth(directory, depth, &visits, out, outSize))
			return true;
	}
	return false;
}

// ---- copying from a folder -------------------------------------------------------

static bool copyFolder(const char *source, const char *temp, Failure *failure)
{
	char root[INSTALLER_PATH_MAX];
	if (!isDirectory(source))
		return failWith(failure, INSTALLER_ERROR_NOT_FOUND, "That folder does not exist.");
	if (!findDataRoot(source, root, sizeof root))
		return failWith(failure, INSTALLER_ERROR_NOT_FOUND,
		                "No Tyrian 2000 data files were found in that folder or the folders inside it. " MESSAGE_MANUAL);
	if (strcmp(root, source) != 0)
		logInfo("Tyrian 2000 data found in %s", root);

	NameList list;
	if (!listDirectory(root, &list))
	{
		freeNames(&list);
		return failWith(failure, INSTALLER_ERROR_IO, "Could not read that folder.");
	}
	if (!SDL_CreateDirectory(temp))
	{
		freeNames(&list);
		return failWith(failure, INSTALLER_ERROR_IO, "Could not create a temporary folder. Check the permissions and free space, then try again.");
	}

	// Only what the engine reads is copied, and always under the canonical lowercase name.
	uint64_t total = 0, done = 0;
	for (size_t i = 0; i < manifest.count; ++i)
		total += manifest.entries[i].size;
	progressSet(INSTALLER_EXTRACTING, "Copying the Tyrian 2000 data...", 0, total);

	bool ok = true;
	uint64_t copiedSize = 0;
	for (size_t i = 0; i < manifest.count && ok; ++i)
	{
		if (cancelRequested(failure))
		{
			ok = false;
			break;
		}
		for (size_t j = 0; j < list.count; ++j)
		{
			if (!namesMatch(list.names[j], manifest.entries[i].name))
				continue;
			char from[INSTALLER_PATH_MAX], to[INSTALLER_PATH_MAX];
			if (!joinPath(from, sizeof from, root, list.names[j]) || !isFile(from))
				continue;
			SDL_PathInfo info;
			if (!SDL_GetPathInfo(from, &info) || info.size > ZIP_MAX_ENTRY_SIZE ||
			    copiedSize + info.size > ZIP_MAX_TOTAL_SIZE)
			{
				ok = failWith(failure, INSTALLER_ERROR_INVALID_DATA, "The folder has data files that are too large. Choose another Tyrian 2000 folder.");
				break;
			}
			copiedSize += info.size;
			for (size_t k = j + 1; k < list.count; ++k)
				if (namesMatch(list.names[k], manifest.entries[i].name))
					ok = failWith(failure, INSTALLER_ERROR_INVALID_DATA, "The folder contains duplicate names for %s. Choose another Tyrian 2000 folder.", manifest.entries[i].name);
			if (!ok) break;
			if (!joinPath(to, sizeof to, temp, manifest.entries[i].name) || !SDL_CopyFile(from, to))
				ok = failWith(failure, INSTALLER_ERROR_IO, "Could not copy %s. Check the free space and permissions, then try again.", manifest.entries[i].name);
			done += manifest.entries[i].size;
			progressBytes(done < total ? done : total, total);
			break;
		}
	}
	freeNames(&list);
	return ok;
}

// ---- extracting from a zip -------------------------------------------------------

static size_t zipRead(void *opaque, mz_uint64 offset, void *buffer, size_t count)
{
	SDL_IOStream *io = opaque;
	if (SDL_SeekIO(io, (Sint64)offset, SDL_IO_SEEK_SET) < 0)
		return 0;
	return SDL_ReadIO(io, buffer, count);
}

typedef struct
{
	SDL_IOStream *io;
	uint64_t written, limit;
	bool failed;
} ZipSink;

static size_t zipWrite(void *opaque, mz_uint64 offset, const void *buffer, size_t count)
{
	(void)offset;
	ZipSink *sink = opaque;
	if (sink->written + count > sink->limit)
	{
		sink->failed = true;
		return 0;
	}
	// Unused DOS/support files are checked for CRC too, without being written.
	size_t wrote = sink->io != NULL ? SDL_WriteIO(sink->io, buffer, count) : count;
	sink->written += wrote;
	if (wrote != count)
		sink->failed = true;
	return wrote;
}

typedef struct
{
	bool directory;
	int components;
	const char *leaf;  // last component (NULL for a directory)
	size_t topLength;  // length of the first component
} EntryName;

// Splits and checks one zip entry name.  Rejects absolute paths, "..", empty
// components, backslashes, drive letters and control characters.
static bool parseEntryName(const char *name, EntryName *entry, const char **reason)
{
	*entry = (EntryName) { false, 0, NULL, 0 };
	size_t length = strlen(name);
	if (length == 0)
		return *reason = "an entry has no name", false;
	if (length >= MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE - 1)
		return *reason = "an entry name is too long", false;
	if (name[0] == '/')
		return *reason = "an entry has an absolute path", false;
	for (size_t i = 0; i < length; ++i)
	{
		unsigned char c = (unsigned char)name[i];
		if (c < 0x20 || c == 0x7f || c == '\\' || c == ':')
			return *reason = "an entry name has a forbidden character", false;
	}

	entry->directory = name[length - 1] == '/';
	const char *start = name;
	while (*start != '\0')
	{
		const char *end = strchr(start, '/');
		size_t part = end != NULL ? (size_t)(end - start) : strlen(start);
		if (part == 0)
			return *reason = "an entry has an empty path component", false;
		if ((part == 1 && start[0] == '.') || (part == 2 && start[0] == '.' && start[1] == '.'))
			return *reason = "an entry path escapes the archive", false;
		if (entry->components == 0)
			entry->topLength = part;
		++entry->components;
		entry->leaf = start;
		if (end == NULL)
			break;
		start = end + 1;
	}
	if (entry->directory)
		entry->leaf = NULL;
	return true;
}

// Every entry is checked before anything is written.  On success `top` is the
// single top-level directory name (may be empty for an empty archive).
static bool checkZip(mz_zip_archive *zip, char *top, size_t topSize, Failure *failure)
{
	const mz_uint count = mz_zip_reader_get_num_files(zip);
	if (count == 0)
		return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file is empty. " MESSAGE_MANUAL);
	if (count > ZIP_MAX_ENTRIES)
		return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file has too many entries to be Tyrian 2000. " MESSAGE_MANUAL);

	top[0] = '\0';
	uint64_t total = 0;
	for (mz_uint i = 0; i < count; ++i)
	{
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(zip, i, &stat))
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file is damaged. Download it again, or choose another. ");
		// miniz's stat name is bounded and NUL-terminated: reject truncation
		// and embedded NULs rather than validating a shortened name.
		if (mz_zip_reader_get_filename(zip, i, NULL, 0) != strlen(stat.m_filename) + 1)
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file has an invalid entry name, so it is not safe to unpack. Choose another file or a folder.");

		const char *reason = NULL;
		EntryName entry;
		if (!parseEntryName(stat.m_filename, &entry, &reason))
		{
			logWarn("Tyrian 2000 zip rejected: %s", reason);
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file is not safe to unpack (%s). Choose another file or a folder.", reason);
		}

		// Regular files and directories only: no links, devices or encrypted data.
		const bool unixHost = (stat.m_version_made_by >> 8) == 3;
		const unsigned mode = unixHost ? (stat.m_external_attr >> 16) & 0170000u : 0;
		if (mode != 0 && mode != 0100000u && mode != 0040000u)
		{
			logWarn("Tyrian 2000 zip rejected: an entry is a link or special file");
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file contains links or special files, so it is not safe to unpack. Choose another file or a folder.");
		}
		if ((stat.m_bit_flag & 1) != 0 || (stat.m_bit_flag & 64) != 0)
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file is encrypted. Choose another file or a folder.");
		if (!stat.m_is_directory && stat.m_method != 0 && stat.m_method != MZ_DEFLATED)
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file uses an unsupported compression method. Choose another file or a folder.");
		if (stat.m_is_directory != (mz_bool)entry.directory)
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file is damaged. Download it again, or choose another. ");
		if ((mode == 0040000u && !entry.directory) || (mode == 0100000u && entry.directory))
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file has inconsistent file types, so it is not safe to unpack. Choose another file or a folder.");
		if (stat.m_uncomp_size > ZIP_MAX_ENTRY_SIZE)
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file has an entry that is far too large to be Tyrian 2000. Choose another file or a folder.");
		total += stat.m_uncomp_size;
		if (total > ZIP_MAX_TOTAL_SIZE)
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file is far too large to be Tyrian 2000. Choose another file or a folder.");

		// Duplicates, ignoring case: they would collide on most file systems.
		for (mz_uint j = 0; j < i; ++j)
		{
			mz_zip_archive_file_stat other;
			if (!mz_zip_reader_file_stat(zip, j, &other))
				continue;
			size_t a = strlen(stat.m_filename), b = strlen(other.m_filename);
			while (a > 0 && stat.m_filename[a - 1] == '/')
				--a;
			while (b > 0 && other.m_filename[b - 1] == '/')
				--b;
			if (a == b && SDL_strncasecmp(stat.m_filename, other.m_filename, a) == 0)
			{
				logWarn("Tyrian 2000 zip rejected: a duplicate entry");
				return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file lists the same file twice, so it is not safe to unpack. Choose another file or a folder.");
			}
		}

		if (entry.components == 1 && !entry.directory)
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file has files outside a single top-level folder. " MESSAGE_MANUAL);
		if (top[0] == '\0')
		{
			if (entry.topLength >= topSize)
				return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file is not a Tyrian 2000 archive. " MESSAGE_MANUAL);
			memcpy(top, stat.m_filename, entry.topLength);
			top[entry.topLength] = '\0';
		}
		else if (strlen(top) != entry.topLength || strncmp(top, stat.m_filename, entry.topLength) != 0)
			return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file has more than one top-level folder. " MESSAGE_MANUAL);
	}
	return true;
}

static bool extractZip(const char *zipPath, const char *temp, Failure *failure)
{
	SDL_IOStream *io = SDL_IOFromFile(zipPath, "rb");
	if (io == NULL)
		return failWith(failure, INSTALLER_ERROR_NOT_FOUND, "The zip file could not be opened.");
	Sint64 archiveSize = SDL_GetIOSize(io);
	if (archiveSize <= 0 || (uint64_t)archiveSize > ZIP_MAX_ARCHIVE_SIZE)
	{
		SDL_CloseIO(io);
		return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "That file is not a Tyrian 2000 zip. " MESSAGE_MANUAL);
	}

	mz_zip_archive zip;
	memset(&zip, 0, sizeof zip);
	zip.m_pRead = zipRead;
	zip.m_pIO_opaque = io;
	if (!mz_zip_reader_init(&zip, (mz_uint64)archiveSize, 0))
	{
		logWarn("Tyrian 2000 zip rejected: %s", mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
		SDL_CloseIO(io);
		return failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "That file is not a valid zip archive, or it is damaged. Download it again, or choose another.");
	}

	char top[128];
	bool ok = checkZip(&zip, top, sizeof top, failure);

	uint64_t total = 0, done = 0;
	if (ok)
	{
		const mz_uint count = mz_zip_reader_get_num_files(&zip);
		for (mz_uint i = 0; i < count; ++i)
		{
			mz_zip_archive_file_stat stat;
			if (mz_zip_reader_file_stat(&zip, i, &stat) && !stat.m_is_directory)
				total += stat.m_uncomp_size;
		}
		ok = SDL_CreateDirectory(temp) ||
		     failWith(failure, INSTALLER_ERROR_IO, "Could not create a temporary folder. Check the permissions and free space, then try again.");
		progressSet(INSTALLER_EXTRACTING, "Unpacking the Tyrian 2000 data...", 0, total);
	}

	for (mz_uint i = 0; ok && i < mz_zip_reader_get_num_files(&zip); ++i)
	{
		if (cancelRequested(failure))
		{
			ok = false;
			break;
		}
		mz_zip_archive_file_stat stat;
		EntryName entry;
		const char *reason;
		if (!mz_zip_reader_file_stat(&zip, i, &stat) || !parseEntryName(stat.m_filename, &entry, &reason))
		{
			ok = failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file is damaged. Download it again, or choose another. ");
			break;
		}
		if (entry.directory)
			continue;

		// Only files the engine reads are unpacked, under their canonical names.
		const InstallerManifestEntry *wanted = NULL;
		for (size_t j = 0; j < manifest.count; ++j)
			if (entry.components == 2 && namesMatch(entry.leaf, manifest.entries[j].name))
			{
				wanted = &manifest.entries[j];
				break;
			}
		char path[INSTALLER_PATH_MAX];
		if (wanted != NULL && !joinPath(path, sizeof path, temp, wanted->name))
		{
			ok = failWith(failure, INSTALLER_ERROR_IO, "The install location path is too long.");
			break;
		}
		ZipSink sink = { wanted != NULL ? SDL_IOFromFile(path, "wb") : NULL, 0, stat.m_uncomp_size, false };
		if (wanted != NULL && sink.io == NULL)
		{
			ok = failWith(failure, INSTALLER_ERROR_IO, "Could not write %s. Check the permissions and free space, then try again.", wanted->name);
			break;
		}
		bool extracted = mz_zip_reader_extract_to_callback(&zip, i, zipWrite, &sink, 0) && !sink.failed &&
		                 sink.written == stat.m_uncomp_size;
		bool closed = sink.io == NULL || SDL_CloseIO(sink.io);
		if (!extracted)
		{
			logWarn("Tyrian 2000 zip: %s could not be extracted (%s)", entry.leaf,
			        mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
			ok = failWith(failure, INSTALLER_ERROR_BAD_ARCHIVE, "The zip file is damaged: %s failed its check. Download it again, or choose another.", entry.leaf);
		}
		else if (!closed)
			ok = failWith(failure, INSTALLER_ERROR_IO, "Could not write %s. Check the permissions and free space, then try again.", wanted->name);
		done += stat.m_uncomp_size;
		progressBytes(done < total ? done : total, total);
	}

	mz_zip_reader_end(&zip);
	SDL_CloseIO(io);
	return ok;
}

// ---- the download ------------------------------------------------------------------

static const char *curlFailureMessage(int exitCode)
{
	switch (exitCode)
	{
	case 6:
	case 7:
		return "Could not reach the download server. Check your internet connection and try again, or install the data manually.";
	case 28:
		return "The download timed out. Check your internet connection and try again, or install the data manually.";
	case 22:
		return "The download server refused the request. Try again later, or install the data manually.";
	case 23:
		return "Could not save the download. Check the free space and permissions, then try again.";
	default:
		return "The download failed. Check your internet connection and try again, or install the data manually.";
	}
}

static bool download(const Paths *paths, Failure *failure)
{
	progressSet(INSTALLER_DOWNLOADING, "Downloading the Tyrian 2000 data...", 0, manifest.archiveSize);

	const char *args[] =
	{
		"curl", "--disable", "--fail", "--location", "--silent", "--show-error",
		"--proto", "=https", "--proto-redir", "=https",
		"--connect-timeout", "20", "--speed-limit", "1000", "--speed-time", "30",
		"--output", paths->part, INSTALLER_DOWNLOAD_URL, NULL
	};
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
	SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
	SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
	SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_APP);
	SDL_Process *process = SDL_CreateProcessWithProperties(props);
	SDL_DestroyProperties(props);
	if (process == NULL)
	{
		logWarn("Could not start curl: %s", SDL_GetError());
		return failWith(failure, INSTALLER_ERROR_NO_CURL,
		                "The download needs the curl program, which was not found on this system. Install the data manually: " MESSAGE_MANUAL);
	}

	int exitCode = 0;
	bool oversize = false;
	for (;;)
	{
		if (SDL_WaitProcess(process, false, &exitCode))
			break;
		if (SDL_GetAtomicInt(&installer.cancel) != 0)
		{
			SDL_KillProcess(process, true);
			SDL_WaitProcess(process, true, &exitCode);
			SDL_DestroyProcess(process);
			failure->cancelled = true;
			return false;
		}

		SDL_PathInfo info;
		uint64_t size = SDL_GetPathInfo(paths->part, &info) ? (uint64_t)info.size : 0;
		progressBytes(size < manifest.archiveSize ? size : manifest.archiveSize, manifest.archiveSize);
		if (size > manifest.archiveSize)
		{
			// More than the archive can be: stop now instead of filling the disk.
			oversize = true;
			SDL_KillProcess(process, true);
			SDL_WaitProcess(process, true, &exitCode);
			break;
		}
		SDL_Delay(100);
	}

	char reported[256] = "";
	SDL_IOStream *errorStream = (SDL_IOStream *)SDL_GetPointerProperty(SDL_GetProcessProperties(process),
	                                                                    SDL_PROP_PROCESS_STDERR_POINTER, NULL);
	if (errorStream != NULL)
	{
		size_t got = SDL_ReadIO(errorStream, reported, sizeof reported - 1);
		reported[got] = '\0';
		for (char *c = reported; *c != '\0'; ++c)
			if (*c == '\r' || *c == '\n')
				*c = ' ';
	}
	SDL_DestroyProcess(process);

	if (oversize)
		return failWith(failure, INSTALLER_ERROR_VERIFY, INSTALLER_MESSAGE_VERIFY_FAILED);
	if (exitCode == 127 || (exitCode != 0 && strstr(reported, "not found") != NULL && strstr(reported, "curl") != NULL))
		return failWith(failure, INSTALLER_ERROR_NO_CURL,
		                "The download needs the curl program, which was not found on this system. Install the data manually: " MESSAGE_MANUAL);
	if (exitCode != 0)
	{
		logWarn("curl failed with exit code %d%s%s", exitCode, reported[0] != '\0' ? ": " : "", reported);
		return failWith(failure, INSTALLER_ERROR_NETWORK, "%s", curlFailureMessage(exitCode));
	}
	if (!isFile(paths->part))
		return failWith(failure, INSTALLER_ERROR_NETWORK, "The download failed. Check your internet connection and try again, or install the data manually.");
	progressBytes(manifest.archiveSize, manifest.archiveSize);
	return true;
}

// ---- installing ----------------------------------------------------------------------

// Swaps the validated temporary directory into the install location.  The old
// install, if any, is set aside first and put back if the rename fails.
static bool installTree(const Paths *paths, Failure *failure)
{
	if (!removeTree(paths->old))
		return failWith(failure, INSTALLER_ERROR_IO, "Could not remove the previous installation backup. Check its permissions and try again.");
	const bool hadOld = pathInfo(paths->install, NULL);
	if (hadOld && !SDL_RenamePath(paths->install, paths->old))
		return failWith(failure, INSTALLER_ERROR_IO, "Could not replace the existing Tyrian 2000 data. Close anything using it and try again.");
	if (!SDL_RenamePath(paths->temp, paths->install))
	{
		if (hadOld)
		{
			if (!SDL_RenamePath(paths->old, paths->install))
				return failWith(failure, INSTALLER_ERROR_IO, "Could not restore the previous install. Its complete data is in %s; move it back to %s.", paths->old, paths->install);
		}
		return failWith(failure, INSTALLER_ERROR_IO, "Could not install the Tyrian 2000 data. Check the permissions and free space, then try again.");
	}
	removeTree(paths->old);
	return true;
}

// ---- the worker ------------------------------------------------------------------------

typedef struct
{
	bool nonCanonical;
} Outcome;

static bool runJob(const Paths *paths, Failure *failure, Outcome *outcome)
{
	const bool fromZip = installer.kind != JOB_FOLDER;
	const char *zipPath = installer.kind == JOB_DOWNLOAD ? paths->part : installer.source;
	// A download must be exactly the archive; any failure after it is a failed verification.
	bool strict = installer.kind == JOB_DOWNLOAD;

	if (installer.kind != JOB_DOWNLOAD)
	{
		// Never work on something the installer itself is about to delete or replace.
		if (sameOrInside(installer.source, paths->temp) || sameOrInside(installer.source, paths->old) ||
		    sameOrInside(installer.source, paths->part))
			return failWith(failure, INSTALLER_ERROR_INVALID_DATA, "That location is the installer's own temporary space. Choose another.");
	}

	if (installer.kind == JOB_DOWNLOAD && !download(paths, failure))
		return false;
	if (cancelRequested(failure))
		return false;

	if (fromZip)
	{
		progressSet(INSTALLER_VERIFYING, installer.kind == JOB_DOWNLOAD ? "Verifying the download..." : "Checking the zip file...", 0, 0);
		uint64_t size = 0;
		char hex[65];
		if (!isFile(zipPath))
			return failWith(failure, INSTALLER_ERROR_NOT_FOUND, "The zip file does not exist.");
		SDL_PathInfo info;
		if (!SDL_GetPathInfo(zipPath, &info) || info.size != manifest.archiveSize)
			return failWith(failure, INSTALLER_ERROR_VERIFY, INSTALLER_MESSAGE_VERIFY_FAILED);
		if (!sha256File(zipPath, &size, hex))
			return failWith(failure, INSTALLER_ERROR_IO, "The zip file could not be read.");
		const bool canonicalArchive = size == manifest.archiveSize && strcmp(hex, manifest.archiveSha256) == 0;
		if (!canonicalArchive)
		{
			logWarn("Tyrian 2000 download rejected: size or SHA-256 mismatch");
			return failWith(failure, INSTALLER_ERROR_VERIFY, INSTALLER_MESSAGE_VERIFY_FAILED);
		}
		strict = true;
		if (cancelRequested(failure))
			return false;

		if (!extractZip(zipPath, paths->temp, failure))
		{
			if (installer.kind == JOB_DOWNLOAD && !failure->cancelled)
				failWith(failure, INSTALLER_ERROR_VERIFY, INSTALLER_MESSAGE_VERIFY_FAILED);
			return false;
		}
	}
	else if (!copyFolder(installer.source, paths->temp, failure))
		return false;
	if (cancelRequested(failure))
		return false;

	progressSet(INSTALLER_VALIDATING, "Checking the Tyrian 2000 data...", 0, 0);
	if (!validateStructure(paths->temp, failure))
	{
		if (strict)
		{
			logWarn("Tyrian 2000 data rejected: %s", failure->message);
			failWith(failure, INSTALLER_ERROR_VERIFY, INSTALLER_MESSAGE_VERIFY_FAILED);
		}
		return false;
	}
	ManifestReport report;
	if (!compareManifest(paths->temp, &report, failure))
		return false;
	if (report.missing + report.differing != 0)
	{
		if (strict)
			return failWith(failure, INSTALLER_ERROR_VERIFY, INSTALLER_MESSAGE_VERIFY_FAILED);
		// Structurally valid Tyrian 2000 data that is not the original release
		// (a GOG build, say): accepted, and marked so in the log.
		outcome->nonCanonical = true;
		logWarn("Tyrian 2000 data is non-canonical: %lu file(s) missing from the manifest, %lu differ from it",
		        (unsigned long)report.missing, (unsigned long)report.differing);
	}
	if (cancelRequested(failure))
		return false;

	progressSet(INSTALLER_INSTALLING, "Installing the Tyrian 2000 data...", 0, 0);
	return installTree(paths, failure);
}

static int SDLCALL workerMain(void *unused)
{
	(void)unused;
	Paths paths;
	Failure failure = { INSTALLER_OK, false, "" };
	Outcome outcome = { false };
	bool ok = resolvePaths(&paths, &failure);
	InstallLock lock;
	bool locked = ok && lockInstall(&paths, &lock, &failure);
	ok = ok && locked;

	if (ok)
	{
		// Check before cleanup: a chosen input must never be deleted as staging.
		if (installer.kind != JOB_DOWNLOAD &&
		    (sameOrInside(installer.source, paths.temp) || sameOrInside(installer.source, paths.old) ||
		     sameOrInside(installer.source, paths.part)))
			ok = failWith(&failure, INSTALLER_ERROR_INVALID_DATA, "That location is the installer's own temporary space. Choose another.");
		// A crash between the two renames left the complete old installation
		// in .old. Restore it before starting or discarding any staged files.
		if (ok && !pathInfo(paths.install, NULL) && pathInfo(paths.old, NULL) &&
		    !SDL_RenamePath(paths.old, paths.install))
			ok = failWith(&failure, INSTALLER_ERROR_IO, "Could not restore the previous installation backup. Check its permissions and try again.");
		if (ok)
		{
			if (!removeTree(paths.part) || !removeTree(paths.temp))
				ok = failWith(&failure, INSTALLER_ERROR_IO, "Could not clear the interrupted installation. Check its permissions and try again.");
			else
			{
				ok = runJob(&paths, &failure, &outcome);
				removeTree(paths.part);
				removeTree(paths.temp);
			}
		}
	}
	if (locked)
		unlockInstall(&lock);

	SDL_LockMutex(installer.mutex);
	InstallerProgress *progress = &installer.progress;
	progress->bytesDone = progress->bytesTotal = 0;
	if (ok)
	{
		progress->state = INSTALLER_DONE;
		progress->error = INSTALLER_OK;
		progress->nonCanonical = outcome.nonCanonical;
		snprintf(progress->installedPath, sizeof progress->installedPath, "%s", paths.install);
		if (outcome.nonCanonical)
			snprintf(progress->message, sizeof progress->message,
			         "Tyrian 2000 data installed. It is not the original release's files (non-canonical); see the log.");
		else
			snprintf(progress->message, sizeof progress->message, "Tyrian 2000 data installed.");
	}
	else if (failure.cancelled)
	{
		progress->state = INSTALLER_CANCELLED;
		progress->error = INSTALLER_OK;
		snprintf(progress->message, sizeof progress->message, "Installation cancelled. Nothing was installed.");
	}
	else
	{
		progress->state = INSTALLER_FAILED;
		progress->error = failure.error;
		snprintf(progress->message, sizeof progress->message, "%s", failure.message);
	}
	installer.working = false;
	SDL_UnlockMutex(installer.mutex);
	return 0;
}

// ---- the public API -------------------------------------------------------------------

static void reapThread(void)
{
	if (installer.thread != NULL)
	{
		SDL_WaitThread(installer.thread, NULL);
		installer.thread = NULL;
	}
}

static void detachFinishedThread(void)
{
	// The worker stops accessing installer state before publishing !working.
	// Detaching also reclaims an already finished thread without a UI-thread wait.
	if (installer.thread != NULL)
	{
		SDL_DetachThread(installer.thread);
		installer.thread = NULL;
	}
}

static bool begin(JobKind kind, const char *source)
{
	if (!installerInit())
		return false;
	SDL_LockMutex(installer.mutex);
	const bool busy = installer.working;
	SDL_UnlockMutex(installer.mutex);
	if (busy)
		return false;
	detachFinishedThread();

	if (source != NULL && (source[0] == '\0' || strlen(source) >= sizeof installer.source))
		return false;
	installer.kind = kind;
	if (source != NULL && !absolutePath(source, installer.source, sizeof installer.source))
		return false;
	if (source == NULL)
		installer.source[0] = '\0';
	// Normalize a trailing separator so a folder compares and joins cleanly.
	for (size_t length = strlen(installer.source); length > 1 &&
	     (installer.source[length - 1] == '/' || installer.source[length - 1] == '\\'); --length)
		installer.source[length - 1] = '\0';

	SDL_SetAtomicInt(&installer.cancel, 0);
	SDL_LockMutex(installer.mutex);
	memset(&installer.progress, 0, sizeof installer.progress);
	installer.progress.state = kind == JOB_DOWNLOAD ? INSTALLER_DOWNLOADING : INSTALLER_VERIFYING;
	snprintf(installer.progress.message, sizeof installer.progress.message, "%s",
	         kind == JOB_DOWNLOAD ? "Starting the download..." : "Starting the installation...");
	installer.working = true;
	SDL_UnlockMutex(installer.mutex);

	installer.thread = SDL_CreateThread(workerMain, "tyrian2000-installer", NULL);
	if (installer.thread == NULL)
	{
		SDL_LockMutex(installer.mutex);
		installer.working = false;
		installer.progress.state = INSTALLER_FAILED;
		installer.progress.error = INSTALLER_ERROR_IO;
		snprintf(installer.progress.message, sizeof installer.progress.message, "Could not start the installer.");
		SDL_UnlockMutex(installer.mutex);
		return false;
	}
	return true;
}

bool installerBeginDownload(void)
{
	return begin(JOB_DOWNLOAD, NULL);
}

bool installerBeginFromZip(const char *path)
{
	return path != NULL && begin(JOB_ZIP, path);
}

bool installerBeginFromFolder(const char *path)
{
	return path != NULL && begin(JOB_FOLDER, path);
}

bool installerPoll(InstallerProgress *progress)
{
	if (!installerInit())
	{
		memset(progress, 0, sizeof *progress);
		return false;
	}
	SDL_LockMutex(installer.mutex);
	*progress = installer.progress;
	const bool working = installer.working;
	SDL_UnlockMutex(installer.mutex);
	if (!working)
		detachFinishedThread();
	return working;
}

void installerCancel(void)
{
	SDL_SetAtomicInt(&installer.cancel, 1);
}

void installerShutdown(void)
{
	installerCancel();
	reapThread();
}

void installerDetect(InstallerStatus *status)
{
	memset(status, 0, sizeof *status);
	installerInstallDirectory(status->installDirectory, sizeof status->installDirectory);

	GameDataSearch search = { NULL, NULL, NULL };
	GameDataProvider *provider = NULL;
	GameDataError error = { GAME_DATA_OK, "", "" };
	GameDataStatus found = gameDataLocate(gameVariantGet(VARIANT_TYRIAN2000), &search, &provider, &error);
	if (found == GAME_DATA_OK)
		found = gameDataValidate(provider, &error);

	if (found == GAME_DATA_OK)
	{
		status->installed = true;
		snprintf(status->path, sizeof status->path, "%s", gameDataDirectory(provider));
		snprintf(status->message, sizeof status->message, "Tyrian 2000 data found in %s.", status->path);
	}
	else if (found == GAME_DATA_NOT_FOUND)
		snprintf(status->message, sizeof status->message, "Tyrian 2000 is not installed.");
	else
		snprintf(status->message, sizeof status->message, "Tyrian 2000 data found in %s is not usable: %s",
		         gameDataDirectory(provider), error.detail);
	gameDataClose(provider);
}

int installerSuggestFolders(InstallerSuggestion *out, int max)
{
	char candidates[16][INSTALLER_PATH_MAX];
	size_t count = 0;
#define ADD(...) do { if (count < COUNT_OF(candidates)) snprintf(candidates[count++], INSTALLER_PATH_MAX, __VA_ARGS__); } while (0)

	const char *home = getenv("HOME");
	if (home == NULL)
		home = "";
#ifdef TARGET_WIN32
	const char *programFiles86 = getenv("ProgramFiles(x86)");
	const char *programFiles = getenv("ProgramFiles");
	ADD("C:/GOG Games/Tyrian 2000");
	if (programFiles86 != NULL)
	{
		ADD("%s/GOG Galaxy/Games/Tyrian 2000", programFiles86);
		ADD("%s/Steam/steamapps/common/Tyrian 2000", programFiles86);
	}
	if (programFiles != NULL)
		ADD("%s/GOG Galaxy/Games/Tyrian 2000", programFiles);
	ADD("D:/GOG Games/Tyrian 2000");
#elif defined(__APPLE__)
	ADD("/Applications/Tyrian 2000.app/Contents/Resources/game");
	ADD("%s/Applications/Tyrian 2000.app/Contents/Resources/game", home);
	ADD("%s/GOG Games/Tyrian 2000", home);
	ADD("%s/Games/Tyrian 2000", home);
	ADD("%s/Library/Application Support/Steam/steamapps/common/Tyrian 2000", home);
#else
	ADD("%s/GOG Games/Tyrian 2000", home);
	ADD("%s/Games/gog/tyrian-2000", home);
	ADD("%s/Games/tyrian-2000", home);
	ADD("%s/Games/Tyrian 2000", home);
	ADD("%s/.local/share/Steam/steamapps/common/Tyrian 2000", home);
	ADD("%s/.steam/steam/steamapps/common/Tyrian 2000", home);
#endif
#undef ADD

	int written = 0;
	for (size_t i = 0; i < count && written < max; ++i)
	{
		if (!isDirectory(candidates[i]))
			continue;
		char root[INSTALLER_PATH_MAX];
		snprintf(out[written].path, sizeof out[written].path, "%s", candidates[i]);
		out[written].hasData = findDataRoot(candidates[i], root, sizeof root);
		++written;
	}
	return written;
}

// ---- headless ----------------------------------------------------------------------------

static const char *scanOption(int argc, char *argv[], const char *name)
{
	const size_t length = strlen(name);
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--") == 0)
			break;
		if (strncmp(argv[i], name, length) != 0)
			continue;
		if (argv[i][length] == '=')
			return argv[i] + length + 1;
		if (argv[i][length] == '\0' && i + 1 < argc)
			return argv[i + 1];
		if (argv[i][length] == '\0')
			return "";
	}
	return NULL;
}

const char *installerCliArgument(int argc, char *argv[])
{
	return scanOption(argc, argv, "--install-2000");
}

const char *installerCliSpecArgument(int argc, char *argv[])
{
	return scanOption(argc, argv, "--install-2000-spec");
}

bool installerRunCli(const char *request, const char *spec)
{
	if (spec != NULL)
	{
		if (!installerLoadTestSpec(spec))
		{
			logError("install: the test spec could not be read.");
			return false;
		}
		logWarn("install: using a test spec instead of the canonical archive and manifest.");
	}

	if (strcmp(request, "selftest") == 0)
	{
		const bool ok = installerHashSelfTest() && manifest.count > 0;
		logInfo("install: self-test %s", ok ? "passed" : "FAILED");
		return ok;
	}
	if (strcmp(request, "detect") == 0)
	{
		InstallerStatus status;
		installerDetect(&status);
		logInfo("install: %s", status.message);
		if (status.installDirectory[0] != '\0')
			logInfo("install: install location %s", status.installDirectory);
		return status.installed;
	}

	bool started;
	SDL_PathInfo info;
	if (strcmp(request, "download") == 0)
	{
		logInfo("install: downloading %s", INSTALLER_DOWNLOAD_URL);
		started = installerBeginDownload();
	}
	else if (SDL_GetPathInfo(request, &info) && info.type == SDL_PATHTYPE_DIRECTORY)
	{
		logInfo("install: from folder %s", request);
		started = installerBeginFromFolder(request);
	}
	else if (SDL_GetPathInfo(request, &info) && info.type == SDL_PATHTYPE_FILE)
	{
		logInfo("install: from zip %s", request);
		started = installerBeginFromZip(request);
	}
	else
	{
		logError("install: '%s' is not \"download\", \"detect\", a zip file or a folder.", request);
		return false;
	}
	if (!started)
	{
		logError("install: could not start.");
		return false;
	}

	InstallerProgress progress;
	InstallerState lastState = INSTALLER_IDLE;
	int lastPercent = -1;
	const Uint64 startTicks = SDL_GetTicks();
	bool cancelled = false;
	while (installerPoll(&progress))
	{
		if (cliCancelAfterMs != 0 && !cancelled && SDL_GetTicks() - startTicks >= cliCancelAfterMs)
		{
			logInfo("install: cancelling (test spec)");
			installerCancel();
			cancelled = true;
		}
		if (progress.state != lastState)
		{
			logInfo("install: %s", progress.message);
			lastState = progress.state;
			lastPercent = -1;
		}
		if (progress.bytesTotal > 0)
		{
			int percent = (int)(progress.bytesDone * 100 / progress.bytesTotal);
			if (percent / 10 != lastPercent / 10 || lastPercent < 0)
			{
				logInfo("install: %d%% (%llu of %llu bytes)", percent,
				        (unsigned long long)progress.bytesDone, (unsigned long long)progress.bytesTotal);
				lastPercent = percent;
			}
		}
		SDL_Delay(100);
	}

	if (progress.state == INSTALLER_DONE)
	{
		logInfo("install: %s", progress.message);
		logInfo("install: data is in %s%s", progress.installedPath, progress.nonCanonical ? " (non-canonical)" : "");
		return true;
	}
	logError("install: %s", progress.message);
	return false;
}
