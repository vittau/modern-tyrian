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
#ifndef LAUNCHER_ART_H
#define LAUNCHER_ART_H

// The launcher's PNGs (assets/launcher/), embedded in the binary.  The
// definitions are generated at build time by tools/embed_assets.sh (Makefile)
// or visualc/embed_assets.ps1 (Visual Studio), never committed.
extern const unsigned char launcher_art_panel21[];
extern const unsigned long launcher_art_panel21_size;
extern const unsigned char launcher_art_panel2000[];
extern const unsigned long launcher_art_panel2000_size;
extern const unsigned char launcher_art_title21[];
extern const unsigned long launcher_art_title21_size;
extern const unsigned char launcher_art_title2000[];
extern const unsigned long launcher_art_title2000_size;

#endif // LAUNCHER_ART_H
