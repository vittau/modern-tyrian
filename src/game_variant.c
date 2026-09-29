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
#include "game_variant.h"

#include "game_schema.h"

#include <stddef.h>
#include <string.h>

static const GameVariantDef variants[] =
{
	{ VARIANT_TYRIAN21, "2.1", "Tyrian 2.1 Freeware", "Tyrian 2.1", "tyrian21", 4, 5,
	  &gameDataSchema21, &gameStringSchema21 },
	{ VARIANT_TYRIAN2000, "2000", "Tyrian 2000", "Tyrian 2000", "tyrian2000", 5, 5,
	  &gameDataSchema2000, &gameStringSchema2000 }
};

static const GameVariantDef *currentVariant = &variants[0];

const GameVariantDef *gameVariantGet(GameVariant variant)
{
	for (size_t i = 0; i < sizeof variants / sizeof *variants; ++i)
	{
		if (variants[i].id == variant)
			return &variants[i];
	}
	return NULL;
}

const GameVariantDef *gameVariantCurrent(void)
{
	return currentVariant;
}

bool gameVariantParse(const char *name, GameVariant *out)
{
	for (size_t i = 0; i < sizeof variants / sizeof *variants; ++i)
	{
		if (strcmp(name, variants[i].cli_name) == 0)
		{
			*out = variants[i].id;
			return true;
		}
	}
	return false;
}

GameVariantStatus gameVariantSelect(GameVariant variant)
{
	const GameVariantDef *def = gameVariantGet(variant);
	if (def == NULL)
		return GAME_VARIANT_INVALID;
	currentVariant = def;
	return GAME_VARIANT_OK;
}
