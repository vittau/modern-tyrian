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

#include <assert.h>
#include <string.h>

// Debug-only: a HUD primitive must not run past the bottom of the panel surface
// (the surfaces are deliberately wider than the visible panel, so only the
// height is checked).  Catches a layout row that would be clipped by the
// 200-row canvas; compiled out in release (NDEBUG).
#ifndef NDEBUG
#define HUD_ASSERT_FIT(hud_surf, hud_y, hud_h) \
	assert((hud_y) >= 0 && (hud_h) >= 0 && (hud_y) + (hud_h) <= (hud_surf)->h)
#else
#define HUD_ASSERT_FIT(hud_surf, hud_y, hud_h) ((void)0)
#endif

// ---------------------------------------------------------------------------
// Colours.  These are the same palette families the original sidebar uses for
// its shield (144) and armor (224) bars; everything else is text.
// ---------------------------------------------------------------------------
#define HUD_BAR_SHIELD 144
#define HUD_BAR_ARMOR  224
#define HUD_BAR_POWER  113
#define HUD_SEP_COLOR  16
#define HUD_LABEL_BANK 2
#define HUD_LABEL_DIM  3
#define HUD_VALUE_BANK 2
#define HUD_VALUE_BRIGHT 6
#define HUD_NUM_BANK   15
#define HUD_GAUGE_COLOR 112

// Content margin inside a panel.
#define HUD_MARGIN 2

// A panel is "roomy" once it is wide enough for full-word labels, wider bars
// and the generator name (16:9 gives 81/82 px, 21:9 more).  Below this it uses
// the compact layout (16:10 gives 60 px).
#define HUD_ROOMY_WIDTH 72

// Vertical vitals block (shield / armor / power reserve), shared by every
// layout.  Three columns filling bottom-up, the label above each bar and its
// numeric value below.  The 1P panel has room for 52 px bars; the 2P/16:10
// compact panel must also carry the weapon and sidekick icon+name+gauge rows,
// so its bars are shorter (26 px) while still being a much heavier block than
// the old 4 px horizontal lines.
#define VIT_BAR_H_1P      52
#define VIT_BAR_H_COMPACT 26
#define VIT_LABEL_GAP     7   // label row to bar top
#define VIT_VALUE_GAP     1   // bar bottom to value row

// Layout slots, in logical pixels from the top of the 200-row panel canvas.
//
// The life and superbomb icons are 12x14 tiles; every row that follows one
// leaves a gap so they cannot touch it.  Text rows are 6 px tall.
//
// 1-player ship-status panel (right).  The original put shield, armor and power
// on the right sidebar, so they stay there, now as vertical bars; the playfield
// status block the Modern HUD used to carry (name, lives, cash, superbombs and
// the special-weapon icon) is grouped with them as "the ship".  The special
// icon reserves a 28-row slot when one is held, otherwise the name starts near
// the top.
#define ST_SPECIAL_Y      2
#define ST_NAME_HI_Y      34
#define ST_NAME_Y         6
#define ST_NOSPECIAL_UP   14
#define ST_SEP_Y          90
#define ST_VIT_LABEL_Y    94
#define ST_GEN_Y          164
#define ST_CHEAT_Y        174
// The row after the name; lives/cash/superbombs follow it.  Computed at run
// time: ST_NAME_HI_Y + 10 when a special is held, ST_NAME_Y + 10 otherwise.

// 1-player armament panel (left).  Weapons, sidekicks and their gauges; the
// global boss bars and level timer sit at the bottom (they used to share the
// status panel, which no longer has the room for both them and the taller
// bars).
#define AR_FRONT_LABEL_Y  6
#define AR_FRONT_NAME_Y   15
#define AR_FRONT_PIPS_Y   25
#define AR_REAR_LABEL_Y   38
#define AR_REAR_NAME_Y    47
#define AR_REAR_PIPS_Y    57
#define AR_MODE_Y         66
#define AR_SK_L_LABEL_Y   82
#define AR_SK_L_ICON_Y    91
#define AR_SK_R_LABEL_Y   114
#define AR_SK_R_ICON_Y    123
#define AR_BOSS_Y         158
#define AR_BOSS_STEP      9
#define AR_TIMER_LABEL_Y  176
#define AR_TIMER_VALUE_Y  184

// 2-player compact panel (one per player).  Both players' panels carry the same
// blocks (status, vertical vitals, weapons, sidekicks, bottom text); player 1
// (left) holds the timer, player 2 (right) the cheat notice, and boss bar b goes
// in panel b.  The rows are packed tighter than the 1P panel and the vertical
// bars are shorter so the base layout's sidekick icon+gauge blocks still fit;
// see VIT_BAR_H_COMPACT.
#define CP_NAME_Y         2
#define CP_LIVES_Y        9
#define CP_CASH_Y         24
#define CP_BOMBS_Y        31
#define CP_VIT_LABEL_Y    47
#define CP_FRONT_NAME_Y   90
#define CP_FRONT_PIPS_Y   97
#define CP_REAR_NAME_Y    104
#define CP_REAR_PIPS_Y    111  // rear firing mode shares this row
#define CP_SK_L_LABEL_Y   118
#define CP_SK_L_ICON_Y    125
#define CP_SK_R_LABEL_Y   143
#define CP_SK_R_ICON_Y    150
#define CP_BOSS_Y         169
#define CP_TIMER_LABEL_Y  177
#define CP_TIMER_VALUE_Y  185
#define CP_CHEAT_Y        177

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
	HUD_ASSERT_FIT(surface, y, 6);
	JE_outText(surface, x, y, s, bank, bright);
}

// JE_outText() with blit_sprite_hv() (clamped) instead of
// blit_sprite_hv_unsafe().  Plain text is byte-identical; the difference only
// shows when a glyph nibble plus the '~' brightness boost would overflow the
// low nibble (see hud_draw_message_strip).  `~` toggles +4 exactly like
// JE_outText().
static void hud_text_clamped(SDL_Surface *surface, int x, int y, const char *s, unsigned int bank, int bright)
{
	HUD_ASSERT_FIT(surface, y, 6);

	int toggled = 0;

	for (int i = 0; s[i] != '\0'; ++i)
	{
		const int sprite_id = fontMap[(unsigned char)s[i]];

		switch (s[i])
		{
		case ' ':
			x += 6;
			break;

		case '~':
			toggled = (toggled == 0) ? 4 : 0;
			break;

		default:
			if (sprite_id != -1 && sprite_exists(TINY_FONT, sprite_id))
			{
				blit_sprite_hv(surface, x, y, TINY_FONT, sprite_id, bank, bright + toggled);
				x += sprite(TINY_FONT, sprite_id)->width + 1;
			}
			break;
		}
	}
}

// The longest of three label spellings that fits `max_w` at TINY_FONT.
static const char *hud_pick_label(const char *full, const char *mid, const char *short_, int max_w)
{
	if (JE_textWidth(full, TINY_FONT) <= max_w)
		return full;
	if (JE_textWidth(mid, TINY_FONT) <= max_w)
		return mid;
	return short_;
}

// A procedural vertical bar with a 1px bevel, filled bottom-up and scaled to
// `max_value`.  Mirrors the old hud_bar(), just turned 90 degrees: the whole
// rectangle is the base-family trough, the value is a brighter column growing
// from the bottom.
static void hud_vbar(SDL_Surface *surface, int x, int y, int w, int h,
                     uint value, uint max_value, Uint8 base)
{
	if (w < 3)
		w = 3;
	if (h < 4)
		h = 4;

	HUD_ASSERT_FIT(surface, y, h);

	// Trough with a light left edge and a dark right edge, like the horizontal
	// bar's top/bottom rows.
	fill_rectangle_xy(surface, x, y, x + w - 1, y + h - 1, base);
	fill_rectangle_xy(surface, x, y, x, y + h - 1, (Uint8)(base + 2));
	fill_rectangle_xy(surface, x + w - 1, y, x + w - 1, y + h - 1, (Uint8)(base + 1));

	if (max_value == 0)
		max_value = 1;

	int fh = (int)(((ulong)value * (ulong)(h - 2)) / max_value);
	if (fh > h - 2)
		fh = h - 2;
	if (fh > 0)
		fill_rectangle_xy(surface, x + 1, y + h - 1 - fh, x + w - 2, y + h - 2, (Uint8)(base + 12));
}

// The shield / armor / power-reserve block: three vertical bars side by side,
// each with its label above and its numeric value below.  `label_y` is the
// label row; the bars start VIT_LABEL_GAP below it and are `bar_h` tall.  The
// block spans the panel width (minus the content margin), centered.
static void hud_draw_vitals(const HudPanel *p, int pi, int label_y, int bar_h)
{
	const uint values[3] = { player[pi].shield, player[pi].armor, MIN(power, 900u) };
	const uint maxima[3] = { MAX(player[pi].shield_max, 1u),
	                         MAX(player[pi].initial_armor, 1u), 900 };
	const Uint8 bases[3] = { HUD_BAR_SHIELD, HUD_BAR_ARMOR, HUD_BAR_POWER };
	static const char *const labels_full[3]  = { "SHIELD", "ARMOR", "POWER" };
	static const char *const labels_mid[3]   = { "SHLD",   "ARMR",  "PWR"   };
	static const char *const labels_short[3] = { "SH",     "AR",    "PW"    };

	const int avail = p->w - 2 * HUD_MARGIN;
	int col_w = avail / 3;
	if (col_w < 12)
		col_w = 12;
	const int block_x = HUD_MARGIN + (avail - col_w * 3) / 2;

	const int bar_w = MIN(col_w - 4, 16);
	const int bar_y = label_y + VIT_LABEL_GAP;
	const int value_y = bar_y + bar_h + VIT_VALUE_GAP;

	for (int i = 0; i < 3; ++i)
	{
		const int col_x = block_x + i * col_w;

		const char *label = hud_pick_label(labels_full[i], labels_mid[i], labels_short[i], col_w);
		const int lx = col_x + (col_w - JE_textWidth(label, TINY_FONT)) / 2;
		hud_text(p->surface, lx, label_y, label, HUD_LABEL_BANK, HUD_LABEL_DIM);

		const int bx = col_x + (col_w - bar_w) / 2;
		hud_vbar(p->surface, bx, bar_y, bar_w, bar_h, values[i], maxima[i], bases[i]);

		char num[16];
		snprintf(num, sizeof num, "%u", values[i]);
		const int nx = col_x + (col_w - JE_textWidth(num, TINY_FONT)) / 2;
		hud_text(p->surface, nx, value_y, num, HUD_NUM_BANK, 1);
	}
}

static void hud_draw_separator(const HudPanel *p, int y)
{
	HUD_ASSERT_FIT(p->surface, y, 1);
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

	HUD_ASSERT_FIT(p->surface, y, 14);

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

	HUD_ASSERT_FIT(p->surface, y, 14);

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

	HUD_ASSERT_FIT(p->surface, y, big ? 28 : 14);

	if (big)
		blit_sprite2x2(p->surface, (p->w - 24) / 2, y, spriteSheet10, graphic);
	else
		blit_sprite2(p->surface, p->w - 1 - 12, y, spriteSheet10, graphic);
}

// Generator name (extra information: the original in-game HUD had no label,
// only the power-reserve bar).  Shown only in the roomy layouts; the power bar
// itself is always part of the vertical vitals block.
static void hud_draw_generator(const HudPanel *p, int pi, int y)
{
	char raw[64], buf[72];
	snprintf(raw, sizeof raw, "GEN %s", powerSys[player[pi].items.generator].name);
	hud_truncate(buf, sizeof buf, raw, p->w - 2 * HUD_MARGIN);
	hud_text(p->surface, hud_start(p, JE_textWidth(buf, TINY_FONT)), y, buf, HUD_LABEL_BANK, HUD_LABEL_DIM);
}

// Weapon power pips (1..11), as the original sidebar's segments.
static void hud_draw_pips(const HudPanel *p, int pi, int port, int y)
{
	if (player[pi].items.weapon[port].id == 0)
		return;

	const uint power = player[pi].items.weapon[port].power;
	HUD_ASSERT_FIT(p->surface, y, 4);
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

// One sidekick: name, the OPTION_SHAPES mode icon (MONO/DUAL with its charge
// ticks, matching the original sidebar) and the segmented gauge.  `label_y` is
// the text row; `icon_y` is the icon top, with the gauge just below it.
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
	HUD_ASSERT_FIT(p->surface, icon_y, 14);
	if (options[id].icongr > 0)
		blit_sprite(p->surface, icon_x, icon_y, OPTION_SHAPES, options[id].icongr - 1);

	// Gauge: limited-ammo sidekicks show ammo/max; infinite-ammo ones show the
	// charge level (the original sidebar gauge becomes a discharge bar).
	const uint ammo_max = options[id].ammo;
	HUD_ASSERT_FIT(p->surface, icon_y + 14, 2);
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
	HUD_ASSERT_FIT(p->surface, y + 2 * 8, 6);
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

// 1-player ship-status panel (the right one): everything the original playfield
// HUD carried (special icon, name, lives, cash, superbombs) grouped with the
// shield / armor / power vertical bars the original drew on the right sidebar,
// plus the generator name and the cheat notice.  Always player 0.
static void hud_draw_status_panel(const HudPanel *p)
{
	// Adaptive upper block: the special icon reserves a 28-row slot, otherwise
	// the name starts near the top.  When no special is held the whole block is
	// 28 rows shorter, so the lower block (separator, vitals, generator, cheat)
	// slides up by the same amount to keep the panel balanced instead of leaving
	// a hole under the score.
	const bool special = (player[0].items.special > 0);
	const int drop = special ? 0 : ST_NOSPECIAL_UP;

	int y;
	if (special)
	{
		hud_draw_special(p, 0, ST_SPECIAL_Y, true);
		y = ST_NAME_HI_Y;
	}
	else
	{
		y = ST_NAME_Y;
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

	hud_draw_separator(p, ST_SEP_Y - drop);

	// Shield, armor and power reserve: three vertical bars, bottom-up.  The
	// generator name and cheat notice keep their bottom slots so the freed
	// upper rows become even space instead of a hole under the score.
	hud_draw_vitals(p, 0, ST_VIT_LABEL_Y - drop, VIT_BAR_H_1P);

	if (p->roomy)
		hud_draw_generator(p, 0, ST_GEN_Y);

	hud_draw_cheat(p, ST_CHEAT_Y);
}

// 1-player armament panel (the left one): front/rear weapon name, power pips
// and rear firing mode, then both sidekicks.  The global boss bars and the
// level timer sit at the bottom, below the sidekicks.
static void hud_draw_armament_panel(const HudPanel *p)
{
	hud_draw_weapon(p, 0, FRONT_WEAPON, AR_FRONT_LABEL_Y, AR_FRONT_NAME_Y, AR_FRONT_PIPS_Y, -1, false);
	hud_draw_weapon(p, 0, REAR_WEAPON, AR_REAR_LABEL_Y, AR_REAR_NAME_Y, AR_REAR_PIPS_Y, AR_MODE_Y, false);

	hud_draw_sidekick(p, 0, LEFT_SIDEKICK, AR_SK_L_LABEL_Y, AR_SK_L_ICON_Y);
	hud_draw_sidekick(p, 0, RIGHT_SIDEKICK, AR_SK_R_LABEL_Y, AR_SK_R_ICON_Y);
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

	hud_draw_vitals(p, pi, CP_VIT_LABEL_Y, VIT_BAR_H_COMPACT);

	hud_draw_weapon(p, pi, FRONT_WEAPON, -1, CP_FRONT_NAME_Y, CP_FRONT_PIPS_Y, -1, false);
	hud_draw_weapon(p, pi, REAR_WEAPON, -1, CP_REAR_NAME_Y, CP_REAR_PIPS_Y, -1, true);

	hud_draw_sidekick(p, pi, LEFT_SIDEKICK, CP_SK_L_LABEL_Y, CP_SK_L_ICON_Y);
	hud_draw_sidekick(p, pi, RIGHT_SIDEKICK, CP_SK_R_LABEL_Y, CP_SK_R_ICON_Y);

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

// The 16-row message strip under the playfield.  It is an opaque info bar: the
// level name on the top line and the current in-game message (JE_drawTextWindow)
// on the bottom line.  Drawn into the strip surface; modern.c composites it.
static void hud_draw_message_strip(void)
{
	SDL_Surface *surface = modern_hud_message_surface();
	if (surface == NULL)
		return;

	memset(surface->pixels, 0, (size_t)surface->pitch * (size_t)surface->h);

	// Black bar with a separator line along its top edge.
	fill_rectangle_xy(surface, 0, 0, MODERN_PLAYFIELD_W - 1, surface->h - 1, 0);
	fill_rectangle_xy(surface, 0, 0, MODERN_PLAYFIELD_W - 1, 0, HUD_SEP_COLOR);

	const int name_w = JE_textWidth(levelName, TINY_FONT);
	if (name_w > 0 && name_w <= MODERN_PLAYFIELD_W - 4)
		hud_text(surface, 2, 2, levelName, HUD_LABEL_BANK, HUD_VALUE_BRIGHT);

	const char *message = modern_message_text();
	if (message != NULL && message[0] != '\0')
	{
		char buf[64];
		hud_truncate(buf, sizeof buf, message, MODERN_PLAYFIELD_W - 4);

		int x = (MODERN_PLAYFIELD_W - JE_textWidth(buf, TINY_FONT)) / 2;
		if (x < 2)
			x = 2;

		// Event strings use '~' to toggle +4 (e.g. "~WARNING:~ Spikes
		// ahead!!").  JE_outText() draws with blit_sprite_hv_unsafe(), whose
		// low nibble is not clamped, so at this brightness the '~' peak wraps
		// into the colour block and corrupts the glyphs ("dark with strange
		// dots").  Draw the message with the clamped primitive instead; plain
		// messages are byte-identical to JE_outText().
		hud_text_clamped(surface, x, 9, buf, HUD_NUM_BANK, 5);
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
		// 1 player: armament on the left, the ship status/vitals panel on the
		// right (where the original sidebar kept shield/armor/power), hugging
		// the outer screen edges.
		HudPanel left  = { modern_hud_surface(0), w, false, w >= HUD_ROOMY_WIDTH };
		HudPanel right = { modern_hud_surface(1), w, true,  w >= HUD_ROOMY_WIDTH };

		hud_draw_armament_panel(&left);
		hud_draw_status_panel(&right);
	}

	hud_draw_message_strip();
}

// The intro has no gameplay tick yet, so only the message strip (bar + level
// name) is drawn; the panels stay cleared.  modern_hud_draw() draws the same
// strip on the first gameplay frame, so the level name simply carries over.
void modern_hud_show_intro(void)
{
	if (!modern_hud_in_panels())
		return;

	hud_draw_message_strip();
}

void modern_hud_draw_timer(const char *label, const char *value, int brightness)
{
	if (!modern_hud_in_panels())
		return;

	SDL_Surface *surface = modern_hud_surface(0);
	const int w = modern_side_panel_width();
	const bool two = (twoPlayerMode && !galagaMode);
	const int label_y = two ? CP_TIMER_LABEL_Y : AR_TIMER_LABEL_Y;
	const int value_y = two ? CP_TIMER_VALUE_Y : AR_TIMER_VALUE_Y;

	int x = (w - JE_textWidth(label, TINY_FONT)) / 2;
	if (x < HUD_MARGIN)
		x = HUD_MARGIN;
	HUD_ASSERT_FIT(surface, label_y, 8);
	JE_textShade(surface, x, label_y, label, 7, brightness, FULL_SHADE);

	x = (w - JE_textWidth(value, SMALL_FONT_SHAPES)) / 2;
	if (x < HUD_MARGIN)
		x = HUD_MARGIN;
	HUD_ASSERT_FIT(surface, value_y, 8);
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
		*y = AR_BOSS_Y + bar * AR_BOSS_STEP;
	}

	*cx = w / 2;
	*half_width = MIN(25, (w - 2) / 2);
	return true;
}
