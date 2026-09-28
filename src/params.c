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
#include "params.h"

#include "arg_parse.h"
#include "demo.h"
#include "episodes.h"
#include "file.h"
#include "gamepad_selftest.h"
#include "joystick.h"
#include "logging.h"
#include "loudness.h"
#include "modern.h"
#include "network.h"
#include "opentyr.h"
#include "regress.h"
#include "xmas.h"

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

JE_boolean richMode = false, constantPlay = false, constantDie = false;

/* YKS: Note: LOOT cheat had non letters removed. */
const char pars[][9] = {
	"LOOT", "RECORD", "NOJOY", "CONSTANT", "DEATH", "NOSOUND", "NOXMAS", "YESXMAS"
};

void JE_paramCheck(int argc, char *argv[])
{
	const Options options[] =
	{
		{ 'h', 'h', "help",              false },
		
		{ 's', 's', "no-sound",          false },
		{ 'j', 'j', "no-joystick",       false },
		{ 'x', 'x', "no-xmas",           false },
		
		{ 't', 't', "data",              true },
		
		{ 'n', 'n', "net",               true },
		{ 256, 0,   "net-player-name",   true }, // TODO: no short codes because there should
		{ 257, 0,   "net-player-number", true }, //       be a menu for entering these in the future
		{ 'p', 'p', "net-port",          true },
		{ 'd', 'd', "net-delay",         true },
		
		{ 'X', 'X', "xmas",              false },
		{ 'c', 'c', "constant",          false },
		{ 'k', 'k', "death",             false },
		{ 'r', 'r', "record",            false },
		{ 'l', 'l', "loot",              false },
		
		{ 258, 0,   "regress-demo",      true },
		{ 259, 0,   "regress-out",       true },
		{ 260, 0,   "regress-detail",    true },
		{ 261, 0,   "regress-level",     true },
		{ 262, 0,   "regress-frames",    true },
		{ 263, 0,   "regress-audio",     false },
		{ 264, 0,   "presentation",      true },
		{ 265, 0,   "regress-modern",    false },
		{ 266, 0,   "aspect",            true },
		{ 267, 0,   "pixel-aspect",      true },
		{ 268, 0,   "regress-aspect",    true },
		{ 269, 0,   "selftest-gamepad",  false },
		{ 270, 0,   "regress-state-out", true },
		{ 271, 0,   "regress-snapshot",  true },
		{ 272, 0,   "regress-players",   true },
		{ 273, 0,   "regress-arcade",    false },
		{ 274, 0,   "regress-replay-check", false },
		
		{ 0, 0, NULL, false }
	};
	
	Option option;
	
	for (; ; )
	{
		option = parse_args(argc, (const char **)argv, options);
		
		if (option.value == NOT_OPTION)
			break;
		
		switch (option.value)
		{
		case INVALID_OPTION:
		case AMBIGUOUS_OPTION:
		case OPTION_MISSING_ARG:
			logError("Try '%s --help' for more information.", argv[0]);
			exit(EXIT_FAILURE);
			break;
			
		case 'h':
			logInfo("Usage: %s [OPTION...]", argv[0]);
			logInfo("%s", "");
			logInfo("Options:");
			logInfo("  -h, --help                   Show help about options");
			logInfo("  -s, --no-sound               Disable audio");
			logInfo("  -j, --no-joystick            Disable joystick/gamepad input");
			logInfo("  -x, --no-xmas                Disable Christmas mode");
			logInfo("  -t, --data=DIR               Set Tyrian data directory");
			logInfo("  -n, --net=HOST[:PORT]        Start a networked game");
			logInfo("  --net-player-name=NAME       Set local player name in a networked game");
			logInfo("  --net-player-number=NUMBER   Set local player number in a networked game");
			logInfo("                               (1 or 2)");
			logInfo("  -p, --net-port=PORT          Set local port to bind (default is 1333)");
			logInfo("  -d, --net-delay=FRAMES       Set lag-compensation delay (default is 1)");
			logInfo("  --presentation=MODE          Set presentation mode: classic or modern");
			logInfo("  --aspect=RATIO               Modern aspect: 4:3, 16:10, 16:9, 21:9, 32:9, auto");
			logInfo("  --pixel-aspect=SHAPE         Modern pixel aspect: original (1.2) or square");
			logInfo("  --regress-demo=N             Replay recorded demo N (1-5) headless and exit");
			logInfo("  --regress-level=E:L          Start level L of episode E headless and exit");
			logInfo("  --regress-frames=N           Cap a --regress-level run at N frames");
			logInfo("  --regress-out=FILE           Write per-frame hashes to FILE (regress modes)");
			logInfo("  --regress-state-out=FILE     Write per-frame game-state hashes to FILE");
			logInfo("  --regress-snapshot=F:FILE    Save the presented image of frame F to FILE (BMP)");
			logInfo("                               (repeatable; the Modern canvas with --regress-modern)");
			logInfo("  --regress-players=N          Start a --regress-level scenario with N players (1 or 2)");
			logInfo("  --regress-arcade             Start a --regress-level scenario in 1-player arcade mode");
			logInfo("  --regress-replay-check       Record each level frame's draw list and replay it (proof)");
			logInfo("  --regress-detail=M           Pin processor detail level M (1-6, default 2)");
			logInfo("  --regress-modern             Hash the Modern canvas in regress modes");
			logInfo("  --regress-audio              Render the audio baselines to FILE and exit");
			logInfo("  --selftest-gamepad           Run the virtual-controller input self-test and exit");
			exit(EXIT_SUCCESS);
			break;
			
		case 's':
			// Disables sound/music usage
			audio_disabled = true;
			break;
			
		case 'j':
			// Disables joystick detection
			ignore_joystick = true;
			break;
			
		case 'x':
			xmas = false;
			break;
			
		// set custom Tyrian data directory
		case 't':
			customDataDirPath = option.arg;
			break;
			
		case 'n':
			isNetworkGame = true;
			
			intptr_t temp = (intptr_t)strchr(option.arg, ':');
			if (temp)
			{
				temp -= (intptr_t)option.arg;
				
				int temp_port = atoi(&option.arg[temp + 1]);
				if (temp_port > 0 && temp_port < 49152)
					network_opponent_port = temp_port;
				else
				{
					logError("%s: invalid network port number", argv[0]);
					exit(EXIT_FAILURE);
				}
				
				network_opponent_host = malloc(temp + 1);
				SDL_strlcpy(network_opponent_host, option.arg, temp + 1);
			}
			else
			{
				network_opponent_host = malloc(strlen(option.arg) + 1);
				strcpy(network_opponent_host, option.arg);
			}
			break;
			
		case 256: // --net-player-name
			network_player_name = malloc(strlen(option.arg) + 1);
			strcpy(network_player_name, option.arg);
			break;
			
		case 257: // --net-player-number
		{
			int temp = atoi(option.arg);
			if (temp >= 1 && temp <= 2)
				thisPlayerNum = temp;
			else
			{
				logError("%s: invalid network player number", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 'p':
		{
			int temp = atoi(option.arg);
			if (temp > 0 && temp < 49152)
				network_player_port = temp;
			else
			{
				logError("%s: invalid network port number", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 'd':
		{
			int temp;
			if (sscanf(option.arg, "%d", &temp) == 1)
				network_delay = 1 + temp;
			else
			{
				logError("%s: invalid network delay value", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 'X':
			xmas = true;
			break;
			
		case 'c':
			/* Constant play for testing purposes (C key activates invincibility)
			   This might be useful for publishers to see everything - especially
			   those who can't play it */
			constantPlay = true;
			break;
			
		case 'k':
			constantDie = true;
			break;
			
		case 'r':
			recordDemo = true;
			break;
			
		case 'l':
			// Gives you mucho bucks
			richMode = true;
			break;
			
		case 258: // --regress-demo
		{
			int temp = atoi(option.arg);
			if (temp >= 1 && temp <= 5)
				regress_demo = temp;
			else
			{
				logError("%s: regression demo number must be between 1 and 5", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 259: // --regress-out
			regress_out_path = option.arg;
			break;
		case 260: // --regress-detail
		{
			int temp = atoi(option.arg);
			if (temp >= 1 && temp <= 6)
				regress_detail = temp;
			else
			{
				logError("%s: regression detail level must be between 1 and 6", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 261: // --regress-level=EPISODE:LEVEL
		{
			int episode, level;
			if (sscanf(option.arg, "%d:%d", &episode, &level) == 2 &&
			    episode >= 1 && episode <= EPISODE_AVAILABLE &&
			    level >= 1)
			{
				regress_scenario_episode = episode;
				regress_scenario_level = level;
			}
			else
			{
				logError("%s: regression level must be EPISODE:LEVEL", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 262: // --regress-frames
		{
			int temp = atoi(option.arg);
			if (temp > 0)
				regress_frames = temp;
			else
			{
				logError("%s: regression frame cap must be positive", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 263: // --regress-audio
			regress_audio = 1;
			break;
		case 264: // --presentation=classic|modern
			if (!set_presentation_by_name(option.arg))
			{
				logError("%s: presentation mode must be 'classic' or 'modern'", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 265: // --regress-modern
			regress_modern = 1;
			break;
		case 266: // --aspect=4:3|16:10|16:9|21:9|32:9|auto
			if (!set_modern_aspect_by_name(option.arg))
			{
				logError("%s: aspect must be '4:3', '16:10', '16:9', '21:9', '32:9' or 'auto'", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 267: // --pixel-aspect=original|square
			if (!set_modern_pixel_aspect_by_name(option.arg))
			{
				logError("%s: pixel aspect must be 'original' or 'square'", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 268: // --regress-aspect=<aspect>
		{
			const ModernAspect before = modern_aspect;
			if (!set_modern_aspect_by_name(option.arg))
			{
				logError("%s: aspect must be '4:3', '16:10', '16:9', '21:9', '32:9' or 'auto'", argv[0]);
				exit(EXIT_FAILURE);
			}
			regress_aspect = modern_aspect;
			modern_aspect = before;  // applied by regress_init(), not now
			break;
		}
		case 269: // --selftest-gamepad
			selftest_gamepad = true;
			break;
		case 270: // --regress-state-out
			regress_state_out_path = option.arg;
			break;
		case 271: // --regress-snapshot=FRAME:FILE
		{
			char *end = NULL;
			const unsigned long frame = strtoul(option.arg, &end, 10);
			if (end == option.arg || *end != ':' || end[1] == '\0')
			{
				logError("%s: --regress-snapshot must be FRAME:FILE", argv[0]);
				exit(EXIT_FAILURE);
			}
			if (!regress_add_snapshot(frame, end + 1))
			{
				logError("%s: too many --regress-snapshot options (max 16)", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 272: // --regress-players
		{
			const int temp = atoi(option.arg);
			if (temp != 1 && temp != 2)
			{
				logError("%s: --regress-players must be 1 or 2", argv[0]);
				exit(EXIT_FAILURE);
			}
			regress_players = temp;
			break;
		}
		case 273: // --regress-arcade
			regress_arcade = 1;
			break;
		case 274: // --regress-replay-check
			regress_replay_check = 1;
			break;
			
		default:
			assert(false);
			break;
		}
	}
	
	if ((regress_demo != 0 || regress_scenario_episode != 0) && regress_audio)
	{
		logError("%s: --regress-audio cannot be combined with --regress-demo/--regress-level", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	if (regress_demo != 0 && regress_scenario_episode != 0)
	{
		logError("%s: --regress-demo and --regress-level are mutually exclusive", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	if (regress_replay_check && regress_demo == 0 && regress_scenario_episode == 0)
	{
		logError("%s: --regress-replay-check requires --regress-demo or --regress-level", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	// legacy parameter support
	for (int i = option.argn; i < argc; ++i)
	{
		for (uint j = 0; j < strlen(argv[i]); ++j)
			argv[i][j] = toupper(argv[i][j]);
		
		for (uint j = 0; j < COUNTOF(pars); ++j)
		{
			if (strcmp(argv[i], pars[j]) == 0)
			{
				switch (j)
				{
				case 0:
					richMode = true;
					break;
				case 1:
					recordDemo = true;
					break;
				case 2:
					ignore_joystick = true;
					break;
				case 3:
					constantPlay = true;
					break;
				case 4:
					constantDie = true;
					break;
				case 5:
					audio_disabled = true;
					break;
				case 6:
					xmas = false;
					break;
				case 7:
					xmas = true;
					break;
					
				default:
					assert(false);
					break;
				}
			}
		}
	}
}
