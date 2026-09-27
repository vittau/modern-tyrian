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
#include "modern_hud.h"

#include "config.h"
#include "episodes.h"
#include "fonthand.h"
#include "helptext.h"
#include "mainint.h"
#include "modern.h"
#include "network.h"
#include "player.h"
#include "sprite.h"
#include "varz.h"
#include "vga256d.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Colours.  These are the same palette families the original sidebar uses for
// its shield (144) and armor (224) bars; everything else is text.
// ---------------------------------------------------------------------------
#define HUD_BAR_SHIELD 144
#define HUD_BAR_ARMOR  224
#define HUD_SEP_COLOR  16
#define HUD_LABEL_BANK 2
#define HUD_LABEL_DIM  3
#define HUD_VALUE_BANK 2
#define HUD_VALUE_BRIGHT 6
#define HUD_NUM_BANK   15
#define HUD_GAUGE_COLOR 112

// Content margin inside a panel.
#define HUD_MARGIN 2

// A panel is "roomy" once it is wide enough for full-word labels and wider
// bars (21:9 gives 120 px, 32:9 more).  Below this it uses the compact layout.
#define HUD_ROOMY_WIDTH 100

// Layout slots, in logical pixels from the top of the 200-row panel canvas.
//
// The life and superbomb icons are 12x14 tiles, so every row that follows one
// keeps at least 16 px of gap.  Names start at y and are 6 px tall.
//
// 1-player status panel (left).  Mirrors the original playfield elements (cash,
// lives, superbombs, special) plus the shield/armor/generator readouts.
#define ST_SEP_Y          90
#define ST_SHIELD_Y       96
#define ST_ARMOR_Y        116
#define ST_GEN_Y          136
#define ST_BOSS_Y         150
#define ST_BOSS_STEP      9
#define ST_TIMER_LABEL_Y  166
#define ST_TIMER_VALUE_Y  174

// 1-player armament panel (right).  The cheat notice shares this panel because
// the status panel holds the timer; together they stay clear of each other.
#define LD_FRONT_LABEL_Y  6
#define LD_FRONT_NAME_Y   15
#define LD_FRONT_PIPS_Y   25
#define LD_REAR_LABEL_Y   38
#define LD_REAR_NAME_Y    47
#define LD_REAR_PIPS_Y    57
#define LD_MODE_Y         66
#define LD_SK_L_LABEL_Y   82
#define LD_SK_L_ICON_Y    91
#define LD_SK_R_LABEL_Y   114
#define LD_SK_R_ICON_Y    123
#define LD_CHEAT_Y        148

// 2-player compact panel (one per player).  Player 2 keeps the bottom of its
// panel for the cheat notice (player 1's holds the timer there).
#define CP_NAME_Y         2
#define CP_LIVES_Y        11
#define CP_CASH_Y         28
#define CP_BOMBS_Y        37
#define CP_SHIELD_Y       54
#define CP_ARMOR_Y        64
#define CP_GEN_Y          74
#define CP_FRONT_NAME_Y   84
#define CP_FRONT_PIPS_Y   92
#define CP_REAR_NAME_Y    101
#define CP_REAR_PIPS_Y    109  // rear firing mode shares this row
#define CP_SK_L_NAME_Y    117
#define CP_SK_L_ICON_Y    125
#define CP_SK_R_NAME_Y    143
#define CP_SK_R_ICON_Y    150
#define CP_BOSS_Y         169
#define CP_TIMER_LABEL_Y  177
#define CP_TIMER_VALUE_Y  185
#define CP_CHEAT_Y        176

// ---------------------------------------------------------------------------
// Small helpers.
// ---------------------------------------------------------------------------

typedef struct
{
	SDL_Surface *surface;
	int w;
	bool right;   // right-align the content toward the outer screen edge
	bool roomy;
} HudPanel;

// Start x for a content block of the given width, honoring the panel's
// alignment.
static int hud_start(const HudPanel *p, int content_w)
{
	if (p->right)
	{
		int x = p->w - HUD_MARGIN - content_w;
		return x > HUD_MARGIN ? x : HUD_MARGIN;
	}
	return HUD_MARGIN;
}

// Copies `src` into `dst`, shortened with a trailing '.' only if characters had
// to be dropped to fit `max_w` at TINY_FONT size.  Trailing spaces are trimmed
// first: item names are fixed-width, space-padded strings, so otherwise a name
// that fits would still be measured (and marked) as too long.  Never overflows
// `dst`.
static void hud_truncate(char *dst, size_t dst_size, const char *src, int max_w)
{
	if (dst_size == 0)
		return;

	if (max_w < 0)
		max_w = 0;

	size_t n = strlen(src);
	while (n > 0 && src[n - 1] == ' ')
		--n;

	char full[128];
	size_t copy = MIN(n, dst_size - 1);
	copy = MIN(copy, sizeof full - 1);
	memcpy(full, src, copy);
	full[copy] = '\0';

	if (copy == 0)
	{
		dst[0] = '\0';
		return;
	}

	if (JE_textWidth(full, TINY_FONT) <= max_w)
	{
		memcpy(dst, full, copy + 1);
		return;
	}

	for (size_t len = copy; len > 0; --len)
	{
		char candidate[130];
		memcpy(candidate, full, len);
		candidate[len] = '.';
		candidate[len + 1] = '\0';
		if (JE_textWidth(candidate, TINY_FONT) <= max_w)
		{
			memcpy(dst, candidate, len + 2);
			return;
		}
	}
	dst[0] = '\0';
}

static void hud_text(SDL_Surface *surface, int x, int y, const char *s, unsigned int bank, int bright)
{
	JE_outText(surface, x, y, s, bank, bright);
}

// A procedural horizontal bar with a 1px frame, scaled to `max_value`.
static void hud_bar(SDL_Surface *surface, int x, int y, int w, int h, uint value, uint max_value, Uint8 base)
{
	if (w < 4)
		w = 4;
	if (h < 3)
		h = 3;

	fill_rectangle_xy(surface, x, y, x + w - 1, y + h - 1, base);
	fill_rectangle_xy(surface, x, y, x + w - 1, y, (Uint8)(base + 2));
	fill_rectangle_xy(surface, x, y + h - 1, x + w - 1, y + h - 1, (Uint8)(base + 1));

	if (max_value == 0)
		max_value = 1;

	int fw = (int)(((ulong)value * (ulong)(w - 2)) / max_value);
	if (fw > w - 2)
		fw = w - 2;
	if (fw > 0)
		fill_rectangle_xy(surface, x + 1, y + 1, x + fw, y + h - 2, (Uint8)(base + 12));
}

// A "LABEL [bar] value" row, aligned as a block.
static void hud_meter_row(const HudPanel *p, int y, const char *label, uint value, uint max_value, Uint8 base)
{
	char num[16];
	snprintf(num, sizeof num, "%u", value);

	const int label_w = JE_textWidth(label, TINY_FONT);
	const int num_w = JE_textWidth(num, TINY_FONT);

	int bar_w = p->w - 2 * HUD_MARGIN - label_w - num_w - 5;
	if (bar_w < 8)
		bar_w = 8;

	const int total = label_w + 3 + bar_w + 2 + num_w;
	const int x = hud_start(p, total);

	hud_text(p->surface, x, y, label, HUD_LABEL_BANK, HUD_LABEL_DIM);
	hud_bar(p->surface, x + label_w + 3, y + 2, bar_w, 4, value, max_value, base);
	hud_text(p->surface, x + label_w + 3 + bar_w + 2, y, num, HUD_NUM_BANK, 1);
}

static void hud_draw_separator(const HudPanel *p, int y)
{
	fill_rectangle_xy(p->surface, HUD_MARGIN, y, p->w - HUD_MARGIN - 1, y, HUD_SEP_COLOR);
}

// ---------------------------------------------------------------------------
// Individual elements.
// ---------------------------------------------------------------------------

// `reserve_corner` is true only in the compact 2-player layout, where the 1x
// special-weapon icon sits in the top-right corner and the name must not run
// under it.  In single player the special icon is the 2x2 one above the name,
// so the full panel width is available.
static void hud_draw_name(const HudPanel *p, int pi, int y, bool reserve_corner)
{
	char raw[64];

	if (isNetworkGame)
		snprintf(raw, sizeof raw, "%s", JE_getName(pi + 1));
	else
		snprintf(raw, sizeof raw, "%s", miscText[(pi == 0) ? 49 - 1 : 50 - 1]);

	const int max_w = p->w - 2 * HUD_MARGIN -
	                  ((reserve_corner && player[pi].items.special > 0) ? 13 : 0);
	char buf[64];
	hud_truncate(buf, sizeof buf, raw, max_w);

	const int x = hud_start(p, JE_textWidth(buf, TINY_FONT));
	hud_text(p->surface, x, y, buf, HUD_VALUE_BANK, HUD_VALUE_BRIGHT);
}

static void hud_draw_lives(const HudPanel *p, int pi, int y)
{
	if (!(onePlayerAction || twoPlayerMode) || player[pi].lives == NULL)
		return;

	const uint extra_lives = *player[pi].lives - 1;

	int content_w;
	char count[16] = "";

	if (extra_lives >= 5)
	{
		snprintf(count, sizeof count, "%u", extra_lives);
		content_w = 12 + 2 + JE_textWidth(count, TINY_FONT);
	}
	else
	{
		content_w = (int)extra_lives * 12;
	}

	int x = hud_start(p, content_w);

	if (extra_lives >= 5)
	{
		blit_sprite2(p->surface, x, y, spriteSheet9, 285);
		hud_text(p->surface, x + 14, y + 3, count, HUD_NUM_BANK, 1);
	}
	else if (extra_lives >= 1)
	{
		for (uint i = 0; i < extra_lives; ++i)
		{
			blit_sprite2(p->surface, x, y, spriteSheet9, 285);
			x += 12;
		}
	}
}

static void hud_draw_cash(const HudPanel *p, int pi, int y)
{
	char buf[24];
	snprintf(buf, sizeof buf, "%lu", player[pi].cash);
	const int x = hud_start(p, JE_textWidth(buf, TINY_FONT));
	hud_text(p->surface, x, y, buf, HUD_VALUE_BANK, 4);
}

static void hud_draw_bombs(const HudPanel *p, int pi, int y)
{
	const uint bombs = player[pi].superbombs;
	if (bombs == 0)
		return;

	int x = hud_start(p, (int)bombs * 12);

	for (uint j = 0; j < bombs; ++j)
	{
		blit_sprite2(p->surface, x, y, spriteSheet9, 304);
		x += 12;
	}
}

static void hud_draw_special(const HudPanel *p, int pi, int y, bool big)
{
	if (player[pi].items.special == 0)
		return;

	const uint graphic = special[player[pi].items.special].itemgraphic;

	if (big)
		blit_sprite2x2(p->surface, (p->w - 24) / 2, y, spriteSheet10, graphic);
	else
		blit_sprite2(p->surface, p->w - 1 - 12, y, spriteSheet10, graphic);
}

static void hud_draw_generator(const HudPanel *p, int pi, int y)
{
	if (p->roomy)
	{
		char raw[64], buf[72];
		snprintf(raw, sizeof raw, "GEN %s", powerSys[player[pi].items.generator].name);
		hud_truncate(buf, sizeof buf, raw, p->w - 2 * HUD_MARGIN);
		hud_text(p->surface, hud_start(p, JE_textWidth(buf, TINY_FONT)), y, buf, HUD_LABEL_BANK, HUD_LABEL_DIM);
	}
	else
	{
		char buf[24], out[32];
		snprintf(buf, sizeof buf, "PWR %u", powerSys[player[pi].items.generator].power);
		hud_truncate(out, sizeof out, buf, p->w - 2 * HUD_MARGIN);
		hud_text(p->surface, hud_start(p, JE_textWidth(out, TINY_FONT)), y, out, HUD_LABEL_BANK, HUD_LABEL_DIM);
	}
}

// Weapon power pips (1..11), as the original sidebar's segments.
static void hud_draw_pips(const HudPanel *p, int pi, int port, int y)
{
	if (player[pi].items.weapon[port].id == 0)
		return;

	const uint power = player[pi].items.weapon[port].power;
	int x = hud_start(p, 22);

	for (uint j = 1; j <= power && j <= 11; ++j)
	{
		fill_rectangle_xy(p->surface, x, y, x + 1, y + 3, (Uint8)(115 + j));
		x += 2;
	}
}

static void hud_draw_weapon(const HudPanel *p, int pi, int port, int label_y, int name_y, int pips_y, int mode_y, bool mode_inline)
{
	const uint id = player[pi].items.weapon[port].id;
	const bool rear = (port == REAR_WEAPON);

	if (label_y >= 0)
	{
		const char *label = rear ? (p->roomy ? "REAR" : "R") : (p->roomy ? "FRONT" : "F");
		hud_text(p->surface, hud_start(p, JE_textWidth(label, TINY_FONT)), label_y, label, HUD_LABEL_BANK, HUD_LABEL_DIM);
	}

	char raw[64], buf[72];
	if (id == 0)
		snprintf(raw, sizeof raw, "%s", "NONE");
	else
		snprintf(raw, sizeof raw, "%s", weaponPort[id].name);

	hud_truncate(buf, sizeof buf, raw, p->w - 2 * HUD_MARGIN);
	hud_text(p->surface, hud_start(p, JE_textWidth(buf, TINY_FONT)), name_y, buf, HUD_VALUE_BANK, HUD_VALUE_BRIGHT);

	hud_draw_pips(p, pi, port, pips_y);

	// Rear firing mode: the original sidebar shows it with two port-config
	// buttons in single player.  It shares the pips row in the compact layout.
	if (rear && id != 0)
	{
		char mode[20], mout[24];
		snprintf(mode, sizeof mode, p->roomy ? "MODE %u" : "M%u", player[pi].weapon_mode);
		hud_truncate(mout, sizeof mout, mode, p->w - 2 * HUD_MARGIN);

		if (mode_inline)
		{
			int x = hud_start(p, 22 + 3 + JE_textWidth(mout, TINY_FONT)) + 22 + 3;
			hud_text(p->surface, x, pips_y, mout, HUD_LABEL_BANK, HUD_LABEL_DIM);
		}
		else if (mode_y >= 0)
		{
			hud_text(p->surface, hud_start(p, JE_textWidth(mout, TINY_FONT)), mode_y, mout, HUD_LABEL_BANK, HUD_LABEL_DIM);
		}
	}
}

// One sidekick: name plus the original icon and segmented gauge.  `label_y` is
// the text row, `icon_y` the icon/gauge top.
static void hud_draw_sidekick(const HudPanel *p, int pi, int k, int label_y, int icon_y)
{
	const uint id = player[pi].items.sidekick[k];
	const char *which = (k == LEFT_SIDEKICK) ? (p->roomy ? "LEFT" : "L") : (p->roomy ? "RIGHT" : "R");

	char label[64], lout[72];
	if (id == 0)
		snprintf(label, sizeof label, "%s NONE", which);
	else
		snprintf(label, sizeof label, "%s %s", which, options[id].name);

	hud_truncate(lout, sizeof lout, label, p->w - 2 * HUD_MARGIN);
	hud_text(p->surface, hud_start(p, JE_textWidth(lout, TINY_FONT)), label_y, lout, HUD_LABEL_BANK, HUD_LABEL_DIM);

	if (id == 0)
		return;

	// Icon, matching the original sidebar (OPTION_SHAPES, icongr-1).
	const int icon_x = hud_start(p, 28);
	if (options[id].icongr > 0)
		blit_sprite(p->surface, icon_x, icon_y, OPTION_SHAPES, options[id].icongr - 1);

	// Gauge: limited-ammo sidekicks show ammo/max; infinite-ammo ones show the
	// charge level (the original sidebar gauge becomes a discharge bar).
	const uint ammo_max = options[id].ammo;
	if (ammo_max > 0)
	{
		draw_segmented_gauge(p->surface, icon_x, icon_y + 14, HUD_GAUGE_COLOR, 2, 2,
		                     MAX(1, (uint)(ammo_max / 10)), (uint)player[pi].sidekick[k].ammo);
	}
	else
	{
		draw_segmented_gauge(p->surface, icon_x, icon_y + 14, HUD_GAUGE_COLOR, 2, 2, 1,
		                     player[pi].sidekick[k].charge);
	}

	// A numeric readout alongside the gauge: ammo/max (or charge/max) when the
	// panel is roomy, a short ammo form in the compact layout, "INF" for a
	// plain unlimited sidekick.  Compact charge-type sidekicks rely on the
	// gauge, exactly like the original sidebar.
	const uint pwr = options[id].pwr;
	char num[24];

	if (ammo_max > 0)
		snprintf(num, sizeof num, p->roomy ? "%d/%u" : "x%d", player[pi].sidekick[k].ammo, ammo_max);
	else if (pwr > 0)
		snprintf(num, sizeof num, "%u/%u", player[pi].sidekick[k].charge, pwr);
	else
		snprintf(num, sizeof num, "INF");

	const int num_w = JE_textWidth(num, TINY_FONT);
	const int num_x = p->right ? (icon_x - 2 - num_w) : (icon_x + 30);
	hud_text(p->surface, num_x, icon_y + 4, num, HUD_NUM_BANK, 1);
}

static void hud_draw_cheat(const HudPanel *p, int y)
{
	if (!youAreCheating)
		return;

	static const char *const cheat_lines[3] = { "Cheaters", "always", "prosper." };
	for (int l = 0; l < 3; ++l)
	{
		int x = (p->w - JE_textWidth(cheat_lines[l], TINY_FONT)) / 2;
		if (x < HUD_MARGIN)
			x = HUD_MARGIN;
		hud_text(p->surface, x, y + l * 8, cheat_lines[l], 3, 4);
	}
}

// ---------------------------------------------------------------------------
// Layouts.
// ---------------------------------------------------------------------------

// 1-player status panel: everything the original playfield HUD carried plus the
// shield/armor/generator readouts.  Always player 0.
static void hud_draw_status_panel(const HudPanel *p)
{
	// Adaptive upper block: the special icon reserves a 28-row slot, otherwise
	// the name starts near the top so the panel does not look half empty.
	int y = 2;
	if (player[0].items.special > 0)
	{
		hud_draw_special(p, 0, y, true);
		y = 34;
	}
	else
	{
		y = 6;
	}

	hud_draw_name(p, 0, y, false);
	y += 10;

	if (onePlayerAction || twoPlayerMode)
	{
		// The life icon is a 12x14 tile: give the row 16 px so the icon and its
		// count cannot touch the cash row below (review fix 1).
		hud_draw_lives(p, 0, y);
		y += 16;
	}

	hud_draw_cash(p, 0, y);
	y += 11;

	hud_draw_bombs(p, 0, y);

	hud_draw_separator(p, ST_SEP_Y);

	hud_meter_row(p, ST_SHIELD_Y, p->roomy ? "SHIELD" : "SH", player[0].shield,
	              MAX(player[0].shield_max, 1u), HUD_BAR_SHIELD);
	hud_meter_row(p, ST_ARMOR_Y, p->roomy ? "ARMOR" : "AR", player[0].armor,
	              MAX(player[0].initial_armor, 1u), HUD_BAR_ARMOR);

	hud_draw_generator(p, 0, ST_GEN_Y);
}

// 1-player armament panel.
static void hud_draw_armament_panel(const HudPanel *p)
{
	hud_draw_weapon(p, 0, FRONT_WEAPON, LD_FRONT_LABEL_Y, LD_FRONT_NAME_Y, LD_FRONT_PIPS_Y, -1, false);
	hud_draw_weapon(p, 0, REAR_WEAPON, LD_REAR_LABEL_Y, LD_REAR_NAME_Y, LD_REAR_PIPS_Y, LD_MODE_Y, false);

	hud_draw_sidekick(p, 0, LEFT_SIDEKICK, LD_SK_L_LABEL_Y, LD_SK_L_ICON_Y);
	hud_draw_sidekick(p, 0, RIGHT_SIDEKICK, LD_SK_R_LABEL_Y, LD_SK_R_ICON_Y);

	// The global cheat notice goes wherever the timer is not (review fix 1):
	// the armament panel in single player, player 2's panel in two player.
	hud_draw_cheat(p, LD_CHEAT_Y);
}

// 2-player compact panel: one player's whole status + armament stacked.
// `with_cheat` is true for player 2's panel, which holds the global cheat
// notice (player 1's holds the timer).
static void hud_draw_compact_panel(const HudPanel *p, int pi, bool with_cheat)
{
	hud_draw_special(p, pi, 3, false);
	hud_draw_name(p, pi, CP_NAME_Y, true);
	hud_draw_lives(p, pi, CP_LIVES_Y);
	hud_draw_cash(p, pi, CP_CASH_Y);
	hud_draw_bombs(p, pi, CP_BOMBS_Y);

	hud_meter_row(p, CP_SHIELD_Y, "S", player[pi].shield,
	              MAX(player[pi].shield_max, 1u), HUD_BAR_SHIELD);
	hud_meter_row(p, CP_ARMOR_Y, "A", player[pi].armor,
	              MAX(player[pi].initial_armor, 1u), HUD_BAR_ARMOR);

	hud_draw_generator(p, pi, CP_GEN_Y);

	hud_draw_weapon(p, pi, FRONT_WEAPON, -1, CP_FRONT_NAME_Y, CP_FRONT_PIPS_Y, -1, false);
	hud_draw_weapon(p, pi, REAR_WEAPON, -1, CP_REAR_NAME_Y, CP_REAR_PIPS_Y, -1, true);

	hud_draw_sidekick(p, pi, LEFT_SIDEKICK, CP_SK_L_NAME_Y, CP_SK_L_ICON_Y);
	hud_draw_sidekick(p, pi, RIGHT_SIDEKICK, CP_SK_R_NAME_Y, CP_SK_R_ICON_Y);

	if (with_cheat)
		hud_draw_cheat(p, CP_CHEAT_Y);
}

// Reproduces the `tempW` side effects of the original JE_inGameDisplays,
// which the parallax pan and the state hashes depend on.  No drawing.
static void hud_preserve_tempw(void)
{
	if (!(onePlayerAction || twoPlayerMode))
		return;

	char stemp[21];

	for (int temp = 0; temp < (onePlayerAction ? 1 : 2); temp++)
	{
		const uint extra_lives = *player[temp].lives - 1;

		tempW = (temp == 0) ? 30 : 270;

		if (extra_lives >= 5)
		{
			tempW = (temp == 0) ? 45 : 250;
		}
		else if (extra_lives >= 1)
		{
			for (uint i = 0; i < extra_lives; ++i)
				tempW += (temp == 0) ? 12 : -12;
		}

		if (isNetworkGame)
			snprintf(stemp, sizeof stemp, "%s", JE_getName(temp + 1));
		else
			snprintf(stemp, sizeof stemp, "%s", miscText[(temp == 0) ? 49 - 1 : 50 - 1]);

		tempW = (temp == 0) ? 28 : (285 - JE_textWidth(stemp, TINY_FONT));
	}
}

// ---------------------------------------------------------------------------
// Public entry points.
// ---------------------------------------------------------------------------

void modern_hud_draw(void)
{
	if (!modern_hud_in_panels())
		return;

	hud_preserve_tempw();

	const int w = modern_side_panel_width();
	const bool two = (twoPlayerMode && !galagaMode);

	if (two)
	{
		HudPanel p1 = { modern_hud_surface(0), w, false, w >= HUD_ROOMY_WIDTH };
		HudPanel p2 = { modern_hud_surface(1), w, true,  w >= HUD_ROOMY_WIDTH };

		// Player 2's panel carries the global cheat notice; player 1's the
		// timer (see hud_draw_compact_panel / modern_hud_draw_timer).
		hud_draw_compact_panel(&p1, 0, false);
		hud_draw_compact_panel(&p2, 1, true);
	}
	else
	{
		// 1 player: the status panel hugs the left edge, the armament panel
		// reads left-to-right (labels before their values) next to the frame.
		HudPanel left  = { modern_hud_surface(0), w, false, w >= HUD_ROOMY_WIDTH };
		HudPanel right = { modern_hud_surface(1), w, false, w >= HUD_ROOMY_WIDTH };

		hud_draw_status_panel(&left);
		hud_draw_armament_panel(&right);
	}
}

void modern_hud_draw_timer(const char *label, const char *value, int brightness)
{
	if (!modern_hud_in_panels())
		return;

	SDL_Surface *surface = modern_hud_surface(0);
	const int w = modern_side_panel_width();
	const bool two = (twoPlayerMode && !galagaMode);
	const int label_y = two ? CP_TIMER_LABEL_Y : ST_TIMER_LABEL_Y;
	const int value_y = two ? CP_TIMER_VALUE_Y : ST_TIMER_VALUE_Y;

	int x = (w - JE_textWidth(label, TINY_FONT)) / 2;
	if (x < HUD_MARGIN)
		x = HUD_MARGIN;
	JE_textShade(surface, x, label_y, label, 7, brightness, FULL_SHADE);

	x = (w - JE_textWidth(value, SMALL_FONT_SHAPES)) / 2;
	if (x < HUD_MARGIN)
		x = HUD_MARGIN;
	JE_dString(surface, x, value_y, value, SMALL_FONT_SHAPES);
}

bool modern_hud_boss_target(int bar, bool two_player, SDL_Surface **surface, int *cx, int *y, int *half_width)
{
	if (!modern_hud_in_panels())
		return false;

	const int w = modern_side_panel_width();

	if (two_player)
	{
		*surface = modern_hud_surface(bar);
		*y = CP_BOSS_Y;
	}
	else
	{
		*surface = modern_hud_surface(0);
		*y = ST_BOSS_Y + bar * ST_BOSS_STEP;
	}

	*cx = w / 2;
	*half_width = MIN(25, (w - 2) / 2);
	return true;
}
