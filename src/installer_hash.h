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
#ifndef INSTALLER_HASH_H
#define INSTALLER_HASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Checksums the Tyrian 2000 installer verifies with.  Standalone: no game state.

typedef struct
{
	uint32_t state[8];
	uint64_t length;      // bytes hashed so far
	uint8_t block[64];
	size_t blockLength;
} Sha256;

void sha256Init(Sha256 *sha);
void sha256Update(Sha256 *sha, const void *data, size_t size);
// Writes the 64 lowercase hex digits and a terminating NUL.
void sha256FinishHex(Sha256 *sha, char hex[65]);

// The POSIX cksum(1) CRC: CRC-32 with the 0x04C11DB7 polynomial, MSB first,
// followed by the length bytes and complemented.  It is what
// test/regress-2000/data-manifest.txt records.
typedef struct
{
	uint32_t crc;
	uint64_t length;
} PosixCksum;

void posixCksumInit(PosixCksum *sum);
void posixCksumUpdate(PosixCksum *sum, const void *data, size_t size);
uint32_t posixCksumFinish(const PosixCksum *sum);

// Known-answer tests for both (NIST vectors and the cksum of "123456789").
bool installerHashSelfTest(void);

#endif // INSTALLER_HASH_H
