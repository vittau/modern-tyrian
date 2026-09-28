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
#ifndef MODERN_BLOOM_H
#define MODERN_BLOOM_H

#include "modern.h"

#include <stdbool.h>

// Modern lighting quality, shared by the two effects of this module:
//
//   * `modern_bloom`    — a tight additive glow around the brightest pixels
//     (shots, lasers, explosions, engine flames).  This is Phase 1 "Bloom".
//   * `modern_lighting` — a wide coloured illumination map: every emissive
//     pixel spreads its hue over the surrounding playfield, so terrain,
//     enemies and the ship read as lit by nearby fire (Phase 2 "Luzes
//     dinâmicas", first version).
//
// Three levels: OFF, LOW and HIGH.  HIGH is the pre-merge LOW; LOW is half of
// it.  The default outside Modern is OFF (pinned in regression mode); a Modern
// user gets LOW and can pick a level from the in-game settings, the config file
// or the command line.
typedef enum
{
	MODERN_QUALITY_OFF = 0,
	MODERN_QUALITY_LOW,
	MODERN_QUALITY_HIGH,
	MODERN_QUALITY_MAX
} ModernQuality;

extern const char *const modern_quality_names[MODERN_QUALITY_MAX];

extern ModernQuality modern_bloom_quality;
extern ModernQuality modern_lighting_quality;

// Parses "off", "low" or "high" into `*quality`; "medium" is accepted as the
// pre-merge name of HIGH.  Returns false (leaving it unchanged) for any other
// value.
bool set_modern_quality_by_name(const char *name, ModernQuality *quality);

// The effect pass.  Registered by modern_init() and run on every Modern
// frame; it does nothing unless the frame is a gameplay frame and at least
// one of the two effects is on.  It only reads `frame->src`/`frame->palette`
// and writes the playfield rectangle of `frame->pixels`, so it is
// deterministic, allocation-free and cannot touch game state.
void modern_bloom_pass(ModernFrame *frame);

// --- Per-object light sources (extension point) -----------------------------
//
// Today the light map is derived entirely from the emissive pixels of the
// 8-bit source.  The Phase 2 draw list being built elsewhere will eventually
// report exact per-object lights (a shot, an explosion, an engine flame) with
// a position and radius; those can be pushed here and will be merged into the
// same light map.
//
// The coordinate space is the logical playfield: x in [0, MODERN_PLAYFIELD_W),
// y in [0, MODERN_PLAYFIELD_H), with the pixel at that position receiving the
// light.  `radius` is in logical pixels.  `r`/`g`/`b` are the light colour.
//
// Sources are consumed and cleared by the next modern_bloom_pass(); they are
// additive with the pixel-derived lights.  Not used yet — nothing calls
// modern_lighting_add_source() in this task.
#define MODERN_LIGHT_MAX_SOURCES 64

void modern_lighting_reset_sources(void);
void modern_lighting_add_source(int x, int y, int radius, Uint8 r, Uint8 g, Uint8 b);

#endif /* MODERN_BLOOM_H */
