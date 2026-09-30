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
#ifndef GAME_VARIANT_H
#define GAME_VARIANT_H

#include <stdbool.h>
#include <stdint.h>

struct GameDataSchema;
struct GameStringSchema;
struct GameRules;

typedef enum
{
	VARIANT_TYRIAN21,
	VARIANT_TYRIAN2000
} GameVariant;

typedef struct
{
	GameVariant id;
	const char *cli_name;
	const char *display_name;
	const char *log_label;
	const char *save_namespace;
	uint8_t episode_count, demo_count;
	const struct GameDataSchema *data_schema;
	const struct GameStringSchema *string_schema;
	const struct GameRules *rules;
	// UI tables arrive in Phase 3c.
} GameVariantDef;

typedef enum
{
	GAME_VARIANT_OK,
	GAME_VARIANT_UNAVAILABLE,
	GAME_VARIANT_INVALID
} GameVariantStatus;

const GameVariantDef *gameVariantGet(GameVariant variant);
const GameVariantDef *gameVariantCurrent(void);
bool gameVariantParse(const char *name, GameVariant *out);
// Select only before initialization or after full session teardown.
GameVariantStatus gameVariantSelect(GameVariant variant);

#endif // GAME_VARIANT_H
