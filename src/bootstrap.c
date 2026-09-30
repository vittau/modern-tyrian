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
#include "bootstrap.h"

#include "gamepad_selftest.h"
#include "params.h"
#include "regress.h"

#include <stdio.h>
#include <string.h>

static bool bootstrapOption(int value, const char *arg, GameBootstrapOptions *out,
                            char *error, size_t error_size)
{
	if (value == PARAM_REGRESS_USER_FILES)
	{
		out->regress_user_files = true;
		out->regress = true;
		return true;
	}
	if (value != 't' && value != PARAM_VARIANT && value != PARAM_REGRESS_USER_ROOT)
		return true;
	if (arg == NULL)
	{
		snprintf(error, error_size, "Option --%s requires an argument.", value == 't' ? "data" : value == PARAM_VARIANT ? "variant" : "regress-user-root");
		return false;
	}
	if (value == PARAM_REGRESS_USER_ROOT)
	{
		if (arg[0] == '\0')
		{
			snprintf(error, error_size, "--regress-user-root requires a nonempty sandbox directory.");
			return false;
		}
		out->regress_user_root = arg;
		return true;
	}
	if (value == 't')
	{
		// Legacy --data precedence: the last occurrence wins.
		out->data_directory = arg;
		return true;
	}
	GameVariant variant;
	if (!gameVariantParse(arg, &variant))
	{
		snprintf(error, error_size, "Unknown game variant '%s'. Expected 2.1 or 2000.", arg);
		return false;
	}
	if (out->variant_explicit && out->variant != variant)
	{
		snprintf(error, error_size, "Conflicting --variant options.");
		return false;
	}
	out->variant = variant;
	out->variant_explicit = true;
	return true;
}

bool gameBootstrapParse(int argc, char *argv[], GameBootstrapOptions *out,
                        char *error, size_t error_size)
{
	*out = (GameBootstrapOptions) { false, VARIANT_TYRIAN21, NULL,
	                              regress_scan_args(argc, argv), gamepad_selftest_scan_args(argc, argv), NULL, false };
	error[0] = '\0';
	const Options *options = JE_paramOptions();
	for (int i = 1; i < argc; ++i)
	{
		const char *arg = argv[i];
		if (strcmp(arg, "--") == 0)
			break;
		if (strncmp(arg, "--", 2) == 0)
		{
			const char *name = arg + 2;
			const char *equals = strchr(name, '=');
			size_t length = equals != NULL ? (size_t)(equals - name) : strlen(name);
			const Options *match = NULL;
			bool ambiguous = false;
			for (const Options *opt = options; opt->long_opt != NULL || opt->short_opt != 0; ++opt)
			{
				if (opt->long_opt == NULL || strncmp(name, opt->long_opt, length) != 0)
					continue;
				if (match != NULL)
					ambiguous = true;
				match = opt;
				if (strlen(opt->long_opt) == length)
				{
					ambiguous = false;
					break;
				}
			}
			// Leave unrelated/invalid options to the full parser as before.
			if (match == NULL || ambiguous)
				continue;
			const char *value = equals != NULL ? equals + 1 : NULL;
			if (!match->has_arg && value != NULL)
			{
				if (match->value == PARAM_REGRESS_USER_FILES)
				{
					snprintf(error, error_size, "--regress-user-files does not accept an argument.");
					return false;
				}
				continue;
			}
			if (match->has_arg && value == NULL && i + 1 < argc)
				value = argv[++i];
			if (!bootstrapOption(match->value, value, out, error, error_size))
				return false;
		}
		else if (arg[0] == '-')
		{
			for (size_t j = 1; arg[j] != '\0'; ++j)
			{
				const Options *match = NULL;
				for (const Options *opt = options; opt->long_opt != NULL || opt->short_opt != 0; ++opt)
				{
					if (opt->short_opt == arg[j])
					{
						match = opt;
						break;
					}
				}
				if (match == NULL)
					break;
				if (!match->has_arg)
					continue;
				const char *value = arg[j + 1] != '\0' ? arg + j + 1 : (i + 1 < argc ? argv[++i] : NULL);
				if (!bootstrapOption(match->value, value, out, error, error_size))
					return false;
				break;
			}
		}
	}
	if ((out->regress_user_root != NULL && (!out->regress || out->selftest)) ||
	    (out->regress_user_files && out->regress_user_root == NULL))
	{
		snprintf(error, error_size, "User-file regression requires --regress-user-root and a regress mode, without selftest.");
		return false;
	}
	return true;
}
