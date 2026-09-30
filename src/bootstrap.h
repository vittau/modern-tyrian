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
#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include "game_variant.h"

#include <stddef.h>

typedef struct
{
	bool variant_explicit;
	GameVariant variant;
	const char *data_directory;
	bool regress, selftest;
	const char *regress_user_root;
	bool regress_user_files;
} GameBootstrapOptions;

bool gameBootstrapParse(int argc, char *argv[], GameBootstrapOptions *out,
                        char *error, size_t error_size);

#endif // BOOTSTRAP_H
