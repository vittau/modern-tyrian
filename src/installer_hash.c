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
#include "installer_hash.h"

#include <string.h>

static const uint32_t sha256K[64] =
{
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256Block(Sha256 *sha, const uint8_t *p)
{
	uint32_t w[64];
	for (int i = 0; i < 16; ++i)
		w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
	for (int i = 16; i < 64; ++i)
	{
		uint32_t s0 = ROR32(w[i - 15], 7) ^ ROR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
		uint32_t s1 = ROR32(w[i - 2], 17) ^ ROR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	uint32_t a = sha->state[0], b = sha->state[1], c = sha->state[2], d = sha->state[3];
	uint32_t e = sha->state[4], f = sha->state[5], g = sha->state[6], h = sha->state[7];
	for (int i = 0; i < 64; ++i)
	{
		uint32_t s1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
		uint32_t ch = (e & f) ^ (~e & g);
		uint32_t t1 = h + s1 + ch + sha256K[i] + w[i];
		uint32_t s0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
		uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
		uint32_t t2 = s0 + maj;
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}
	sha->state[0] += a; sha->state[1] += b; sha->state[2] += c; sha->state[3] += d;
	sha->state[4] += e; sha->state[5] += f; sha->state[6] += g; sha->state[7] += h;
}

void sha256Init(Sha256 *sha)
{
	static const uint32_t initial[8] =
	{
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};
	memcpy(sha->state, initial, sizeof initial);
	sha->length = 0;
	sha->blockLength = 0;
}

void sha256Update(Sha256 *sha, const void *data, size_t size)
{
	const uint8_t *p = data;
	sha->length += size;
	while (size > 0)
	{
		size_t take = 64 - sha->blockLength;
		if (take > size)
			take = size;
		memcpy(sha->block + sha->blockLength, p, take);
		sha->blockLength += take;
		p += take;
		size -= take;
		if (sha->blockLength == 64)
		{
			sha256Block(sha, sha->block);
			sha->blockLength = 0;
		}
	}
}

void sha256FinishHex(Sha256 *sha, char hex[65])
{
	static const char digits[] = "0123456789abcdef";
	const uint64_t bits = sha->length * 8;

	uint8_t pad[72];
	size_t padLength = (sha->blockLength < 56 ? 56 : 120) - sha->blockLength;
	memset(pad, 0, sizeof pad);
	pad[0] = 0x80;
	for (int i = 0; i < 8; ++i)
		pad[padLength + i] = (uint8_t)(bits >> (56 - i * 8));
	sha256Update(sha, pad, padLength + 8);

	for (int i = 0; i < 8; ++i)
		for (int j = 0; j < 4; ++j)
		{
			uint8_t byte = (uint8_t)(sha->state[i] >> (24 - j * 8));
			hex[i * 8 + j * 2] = digits[byte >> 4];
			hex[i * 8 + j * 2 + 1] = digits[byte & 15];
		}
	hex[64] = '\0';
}

static uint32_t cksumTable[256];
static bool cksumTableReady = false;

static void cksumBuildTable(void)
{
	for (uint32_t i = 0; i < 256; ++i)
	{
		uint32_t value = i << 24;
		for (int bit = 0; bit < 8; ++bit)
			value = (value & 0x80000000u) != 0 ? (value << 1) ^ 0x04C11DB7u : value << 1;
		cksumTable[i] = value;
	}
	cksumTableReady = true;
}

void posixCksumInit(PosixCksum *sum)
{
	if (!cksumTableReady)
		cksumBuildTable();
	sum->crc = 0;
	sum->length = 0;
}

void posixCksumUpdate(PosixCksum *sum, const void *data, size_t size)
{
	const uint8_t *p = data;
	uint32_t crc = sum->crc;
	for (size_t i = 0; i < size; ++i)
		crc = (crc << 8) ^ cksumTable[(crc >> 24) ^ p[i]];
	sum->crc = crc;
	sum->length += size;
}

uint32_t posixCksumFinish(const PosixCksum *sum)
{
	uint32_t crc = sum->crc;
	for (uint64_t length = sum->length; length != 0; length >>= 8)
		crc = (crc << 8) ^ cksumTable[(crc >> 24) ^ (uint32_t)(length & 0xff)];
	return ~crc;
}

bool installerHashSelfTest(void)
{
	static const struct { const char *text; const char *hex; } vectors[] =
	{
		{ "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
		{ "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
		{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		  "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" }
	};
	for (size_t i = 0; i < sizeof vectors / sizeof *vectors; ++i)
	{
		Sha256 sha;
		char hex[65];
		sha256Init(&sha);
		sha256Update(&sha, vectors[i].text, strlen(vectors[i].text));
		sha256FinishHex(&sha, hex);
		if (strcmp(hex, vectors[i].hex) != 0)
			return false;
	}

	// One million 'a', fed in odd-sized pieces to exercise the block buffering.
	Sha256 sha;
	char hex[65];
	char chunk[997];
	memset(chunk, 'a', sizeof chunk);
	sha256Init(&sha);
	size_t remaining = 1000000;
	while (remaining > 0)
	{
		size_t take = remaining < sizeof chunk ? remaining : sizeof chunk;
		sha256Update(&sha, chunk, take);
		remaining -= take;
	}
	sha256FinishHex(&sha, hex);
	if (strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") != 0)
		return false;

	PosixCksum sum;
	posixCksumInit(&sum);
	posixCksumUpdate(&sum, "123456789", 9);
	return posixCksumFinish(&sum) == 930766865u;
}
