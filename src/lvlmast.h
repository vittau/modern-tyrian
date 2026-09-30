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
#ifndef LVLMAST_H
#define LVLMAST_H

#include "opentyr.h"

#define EVENT_MAXIMUM 2500

// Array capacities: the largest of the Tyrian 2.1 and Tyrian 2000 tables.  The
// IDs that are valid, and the counts serialised in the data files, are those of
// the selected GameDataSchema (game_schema.h).
#define WEAP_NUM    1818
#define PORT_NUM    60
#define ARMOR_NUM   4
#define POWER_NUM   6
#define ENGINE_NUM  6
#define OPTION_NUM  37
#define SHIP_NUM    18
#define SHIELD_NUM  11
#define SPECIAL_NUM 54

#define ENEMY_NUM   1850

// The character of the enemy shape file (newsh?.shp) for a 1-based table number
// from the level file, or '\0' when the schema has no such table.
JE_char enemyShapeFileChar(unsigned int table);

#endif /* LVLMAST_H */
