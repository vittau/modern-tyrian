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
#ifndef FILE_H
#define FILE_H

#include <SDL3/SDL_endian.h>

#include <stdbool.h>
#include <stdio.h>

extern const char *customDataDirPath;

typedef struct File
{
	FILE *f;
	int errnum;
	bool error;  // Indicates that an operation failed and no further operations (except close) will be performed.
} File;

bool findDataFiles(void);

// True on a Steam Deck: the SteamDeck=1 environment variable, or the DMI
// board/product name "Jupiter" (LCD) / "Galileo" (OLED).  Always false on
// non-Linux platforms.
bool steamDeck(void);

// The directory user files (opentyrian.cfg, saves, logs) go in, resolving it
// on first use.  Empty string when there is nowhere to write.
const char *userDirGet(void);

// Resolve the user directory and create it if needed.  Returns false when
// there is no user directory (userFileOpen() then falls back to the cwd).
bool userDirPrepare(void);

bool dataFileExists(const char *filename);
bool userFileExists(const char *filename);

File dataFileOpen(const char *filename, const char *mode);
File userFileOpen(const char *filename, const char *mode);

// Cuts the process off from the user's directory: every later userFileOpen()
// fails and userFilesEnabled() returns false.  Regress and selftest runs call
// it so they can never overwrite the player's config or saved games.
void userFilesDisable(void);
bool userFilesEnabled(void);

void fileSetPosition(File *file, long position);
long fileGetPosition(File *file);
long fileGetLength(File *file);

size_t fileReadAtMost(File *file, void *data, size_t size);
void fileReadExactly(File *file, void *data, size_t size);

static inline uint8_t fileReadU8(File *file)
{
	Uint8 value;
	fileReadExactly(file, &value, sizeof value);
	return value;
}

static inline void fileReadU8Array(File *file, uint8_t *values, size_t count)
{
	fileReadExactly(file, values, sizeof *values * count);
}

static inline uint16_t fileReadU16(File *file)
{
	Uint16 value;
	fileReadExactly(file, &value, sizeof value);
	return SDL_Swap16LE(value);
}

static inline void fileReadU16Array(File *file, uint16_t *values, size_t count)
{
	fileReadExactly(file, values, sizeof *values * count);
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	for (size_t i = 0; i < count; ++i)
		values[i] = SDL_Swap16LE(values[i]);
#endif
}

static inline uint16_t fileReadU16BE(File *file)
{
	Uint16 value;
	fileReadExactly(file, &value, sizeof value);
	return SDL_Swap16BE(value);
}

static inline void fileReadU16BEArray(File *file, uint16_t *values, size_t count)
{
	fileReadExactly(file, values, sizeof *values * count);
#if SDL_BYTEORDER != SDL_BIG_ENDIAN
	for (size_t i = 0; i < count; ++i)
		values[i] = SDL_Swap16BE(values[i]);
#endif
}

static inline uint32_t fileReadU32(File *file)
{
	Uint32 value;
	fileReadExactly(file, &value, sizeof value);
	return SDL_Swap32LE(value);
}

static inline void fileReadU32Array(File *file, uint32_t *values, size_t count)
{
	fileReadExactly(file, values, sizeof *values * count);
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	for (size_t i = 0; i < count; ++i)
		values[i] = SDL_Swap32LE(values[i]);
#endif
}

static inline bool fileReadBool(File *file)
{
	return fileReadU8(file) != 0;
}

static inline void fileReadBoolArray(File *file, bool *values, size_t count)
{
	for (size_t i = 0; i < count; ++i)
		values[i] = fileReadBool(file);
}

static inline char fileReadChar(File *file)
{
	return fileReadU8(file);
}

static inline void fileReadCharArray(File *file, char *values, size_t count)
{
	fileReadExactly(file, values, count);
}

static inline int8_t fileReadS8(File *file)
{
	return fileReadU8(file);
}

static inline void fileReadS8Array(File *file, int8_t *values, size_t count)
{
	fileReadU8Array(file, (uint8_t *)values, count);
}

static inline int16_t fileReadS16(File *file)
{
	return fileReadU16(file);
}

static inline void fileReadS16Array(File *file, int16_t *values, size_t count)
{
	fileReadU16Array(file, (uint16_t *)values, count);
}

static inline int32_t fileReadS32(File *file)
{
	return fileReadU32(file);
}

static inline void fileReadS32Array(File *file, int32_t *values, size_t count)
{
	fileReadU32Array(file, (uint32_t *)values, count);
}

void fileWrite(File *file, const void *data, size_t size);

static inline void fileWriteU8(File *file, uint8_t value)
{
	fileWrite(file, &value, sizeof value);
}

static inline void fileWriteU8Array(File *file, uint8_t *values, size_t count)
{
	fileWrite(file, values, sizeof *values * count);
}

static inline void fileWriteCharArray(File *file, char *values, size_t count)
{
	fileWrite(file, values, sizeof *values * count);
}

void fileFlush(File *file);

void fileClose(File *file);

const char *fileGetError(File *file);

#endif // FILE_H
