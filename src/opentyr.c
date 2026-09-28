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
#include "opentyr.h"

#include "config.h"
#include "demo.h"
#include "destruct.h"
#include "editship.h"
#include "episodes.h"
#include "file.h"
#include "font.h"
#include "fonthand.h"
#include "gamepad_selftest.h"
#include "helptext.h"
#include "interp.h"
#include "joystick.h"
#include "jukebox.h"
#include "keyboard.h"
#include "logging.h"
#include "loudness.h"
#include "mainint.h"
#include "modern.h"
#include "modern_bloom.h"
#include "mouse.h"
#include "mtrand.h"
#include "network.h"
#include "nortsong.h"
#include "nortvars.h"
#include "opentyrian_version.h"
#include "palette.h"
#include "params.h"
#include "picload.h"
#include "regress.h"
#include "sprite.h"
#include "tyrian2.h"
#include "varz.h"
#include "vfx.h"
#include "vga256d.h"
#include "video.h"
#include "video_scale.h"
#include "xmas.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *opentyrian_str = "OpenTyrian";
const char *opentyrian_version = OPENTYRIAN_VERSION;

static size_t getDisplayPickerItemsCount(void)
{
	int display_count = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&display_count);
	SDL_free(displays);

	return 1 + (size_t)display_count;
}

static const char *getDisplayPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	if (i == 0)
		return "Window";

	snprintf(buffer, bufferSize, "Display %d", (int)i);
	return buffer;
}

static size_t getScalerPickerItemsCount(void)
{
	return (size_t)scalers_count;
}

static const char *getScalerPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	(void)buffer, (void)bufferSize;

	return scalers[i].name;
}

static size_t getScalingModePickerItemsCount(void)
{
	return (size_t)ScalingMode_MAX;
}

static const char *getScalingModePickerItem(size_t i, char *buffer, size_t bufferSize)
{
	(void)buffer, (void)bufferSize;

	return scaling_mode_names[i];
}

static size_t getPresentationPickerItemsCount(void)
{
	return (size_t)PRESENTATION_MAX;
}

// The presentation/pixel-aspect names are canonical lowercase (cfg and CLI);
// the menu shows them capitalised like the other values ("Window", "None",
// "Integer", "On").  The aspect names keep their spelling ("auto" stays
// lowercase).
static const char *capitalized_name(const char *name, char *buffer, size_t bufferSize)
{
	size_t i = 0;
	for (; name[i] != '\0' && i + 1 < bufferSize; ++i)
		buffer[i] = i == 0 ? (char)toupper((unsigned char)name[i]) : name[i];
	buffer[i] = '\0';
	return buffer;
}

static const char *getPresentationPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	return capitalized_name(presentation_names[i], buffer, bufferSize);
}

static size_t getAspectPickerItemsCount(void)
{
	return (size_t)MODERN_ASPECT_MAX;
}

static const char *getAspectPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	(void)buffer, (void)bufferSize;

	return modern_aspect_names[i];
}

static size_t getPixelAspectPickerItemsCount(void)
{
	return (size_t)PIXEL_ASPECT_MAX;
}

static const char *getPixelAspectPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	return capitalized_name(modern_pixel_aspect_names[i], buffer, bufferSize);
}

static size_t getSmoothMotionPickerItemsCount(void)
{
	return 2;
}

static const char *getSmoothMotionPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	(void)buffer, (void)bufferSize;

	return i == 0 ? "On" : "Off";
}

static size_t getVfxPickerItemsCount(void)
{
	return (size_t)VFX_LEVEL_MAX;
}

static const char *getVfxPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	return capitalized_name(vfx_level_names[i], buffer, bufferSize);
}

// Set by setupMenuStartAt() so the regress harness can open a submenu directly.
static int setup_menu_start = -1;

void setupMenuStartAt(int menu)
{
	setup_menu_start = menu;
}

void setupMenu(void)
{
	typedef enum
	{
		MENU_ITEM_NONE = 0,
		MENU_ITEM_DONE,
		MENU_ITEM_GRAPHICS,
		MENU_ITEM_SOUND,
		MENU_ITEM_JUKEBOX,
		MENU_ITEM_DESTRUCT,
		MENU_ITEM_DISPLAY,
		MENU_ITEM_SCALER,
		MENU_ITEM_SCALING_MODE,
		MENU_ITEM_PRESENTATION,
		MENU_ITEM_ASPECT,
		MENU_ITEM_PIXEL_ASPECT,
		MENU_ITEM_SMOOTH_MOTION,
		MENU_ITEM_VFX,
		MENU_ITEM_MUSIC_VOLUME,
		MENU_ITEM_SOUND_VOLUME,
	} MenuItemId;

	typedef enum
	{
		MENU_NONE = SETUP_MENU_NONE,
		MENU_SETUP = SETUP_MENU_SETUP,
		MENU_GRAPHICS = SETUP_MENU_GRAPHICS,
		MENU_SOUND = SETUP_MENU_SOUND,
	} MenuId;

	typedef struct
	{
		MenuItemId id;
		const char *name;
		const char *description;
		size_t (*getPickerItemsCount)(void);
		const char *(*getPickerItem)(size_t i, char *buffer, size_t bufferSize);
		bool requiresModern;  // greyed out and ignored in Classic
	} MenuItem;

	typedef struct
	{
		const char *header;
		const MenuItem items[10];
	} Menu;

	static const Menu menus[] = {
		[MENU_SETUP] = {
			.header = "Setup",
			.items = {
				{ MENU_ITEM_GRAPHICS, "Graphics...", "Change the graphics settings." },
				{ MENU_ITEM_SOUND, "Sound...", "Change the sound settings." },
				{ MENU_ITEM_JUKEBOX, "Jukebox", "Listen to the music of Tyrian." },
				// { MENU_ITEM_DESTRUCT, "Destruct", "Play a bonus mini-game." },
				{ MENU_ITEM_DONE, "Done", "Return to the main menu." },
				{ -1 }
			},
		},
		[MENU_GRAPHICS] = {
			.header = "Graphics",
			.items = {
				{ MENU_ITEM_DISPLAY, "Display:", "Change the display mode.", getDisplayPickerItemsCount, getDisplayPickerItem },
				{ MENU_ITEM_SCALER, "Scaler:", "Change the pixel art scaling algorithm.", getScalerPickerItemsCount, getScalerPickerItem },
				{ MENU_ITEM_SCALING_MODE, "Scaling Mode:", "Change the scaling mode.", getScalingModePickerItemsCount, getScalingModePickerItem },
				{ MENU_ITEM_PRESENTATION, "Presentation:", "Change the presentation mode.", getPresentationPickerItemsCount, getPresentationPickerItem, true },
				{ MENU_ITEM_ASPECT, "Aspect:", "Change the Modern aspect ratio.", getAspectPickerItemsCount, getAspectPickerItem, true },
				{ MENU_ITEM_PIXEL_ASPECT, "Pixel Aspect:", "Change the pixel aspect.", getPixelAspectPickerItemsCount, getPixelAspectPickerItem },
				{ MENU_ITEM_SMOOTH_MOTION, "Smooth Motion:", "Present Modern gameplay at the display refresh.", getSmoothMotionPickerItemsCount, getSmoothMotionPickerItem, true },
				{ MENU_ITEM_VFX, "Effects:", "Change the Modern VFX level.", getVfxPickerItemsCount, getVfxPickerItem, true },
				{ MENU_ITEM_DONE, "Done", "Return to the previous menu." },
				{ -1 }
			},
		},
		[MENU_SOUND] = {
			.header = "Sound",
			.items = {
				{ MENU_ITEM_MUSIC_VOLUME, "Music Volume", "Change volume with the left/right arrow keys." },
				{ MENU_ITEM_SOUND_VOLUME, "Sound Volume", "Change volume with the left/right arrow keys." },
				{ MENU_ITEM_DONE, "Done", "Return to the previous menu." },
				{ -1 }
			},
		},
	};

	char buffer[100];

	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');  // need mouse pointer sprites

	bool restart = true;

	MenuId menuParents[COUNTOF(menus)] = { MENU_NONE };
	size_t selectedMenuItemIndexes[COUNTOF(menus)] = { 0 };
	MenuId currentMenu = MENU_SETUP;
	if (setup_menu_start >= 0)
	{
		currentMenu = (MenuId)setup_menu_start;
		setup_menu_start = -1;
	}
	MenuItemId currentPicker = MENU_ITEM_NONE;
	size_t pickerSelectedIndex = 0;

	const int xCenter = 320 / 2;
	const int yMenuHeader = 4;
	const int xMenuItem = 45;
	const int xMenuItemName = xMenuItem;
	const int wMenuItemName = 135;
	const int xMenuItemValue = xMenuItemName + wMenuItemName;
	const int wMenuItemValue = 95;
	const int wMenuItem = wMenuItemName + wMenuItemValue;
	const int yMenuItems = 37;
	int dyMenuItems = 21;
	const int hMenuItem = 13;
	const int yMenuStatus = 190;
	// Keep at least the shorter menus' inter-row gap (21 - 13) between the last
	// row and the status line, so an eight-row menu (Graphics) does not touch it.
	const int yMenuItemsBottomGap = 8;

	for (; ; )
	{
		setFrameCount(1);

		if (restart)
		{
			JE_loadPic(VGAScreen2, 2, false);
			fill_rectangle_wh(VGAScreen2, 0, 192, 320, 8, 0);
		}

		// Restore background.
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);

		const Menu *menu = &menus[currentMenu];

		// Draw header.
		drawFontHvShadowAligned(VGAScreen, xCenter, yMenuHeader, menu->header, FONT_LARGE, ALIGN_CENTER, 15, -3, false, 2);

		int yPicker = 0;
		const int dyPickerItem = 15;
		const int dyPickerItemPadding = 2;
		const int hPickerItem = dyPickerItem - dyPickerItemPadding;

		size_t *const selectedMenuItemIndex = &selectedMenuItemIndexes[currentMenu];
		const MenuItem *const menuItems = menu->items;

		// Count the items first so a menu with many entries (Graphics now has
		// nine) can tighten its row spacing and still fit above the status line.
		size_t menuItemsCount = 0;
		while (menuItems[menuItemsCount].id != (MenuItemId)-1)
			menuItemsCount += 1;

		if (menuItemsCount > 1)
		{
			const int maxDy = (yMenuStatus - yMenuItemsBottomGap - hMenuItem - yMenuItems) / (int)(menuItemsCount - 1);
			if (dyMenuItems > maxDy)
				dyMenuItems = maxDy;
		}

		// Draw menu items.

		for (size_t i = 0; i < menuItemsCount; ++i)
		{
			const MenuItem *const menuItem = &menuItems[i];

			const int y = yMenuItems + dyMenuItems * i;

			const bool selected = i == *selectedMenuItemIndex;
			const bool disabled = (currentPicker != MENU_ITEM_NONE && !selected) ||
			                      (menuItem->requiresModern && presentation != PRESENTATION_MODERN);

			if (selected)
				yPicker = y;

			const char *const name = menuItem->name;

			drawFontHvShadow(VGAScreen, xMenuItemName, y, name, FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);

			switch (menuItem->id)
			{
			case MENU_ITEM_DISPLAY:;
				const char *value = "Window";
				if (fullscreen_display >= 0)
				{
					snprintf(buffer, sizeof(buffer), "Display %d", fullscreen_display + 1);
					value = buffer;
				}

				drawFontHvShadow(VGAScreen, xMenuItemValue, y, value, FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_SCALER:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, scalers[scaler].name, FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_SCALING_MODE:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, scaling_mode_names[scaling_mode], FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_PRESENTATION:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, capitalized_name(presentation_names[presentation], buffer, sizeof buffer), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_ASPECT:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, modern_aspect_names[modern_aspect], FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_PIXEL_ASPECT:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, capitalized_name(modern_pixel_aspect_names[modern_pixel_aspect], buffer, sizeof buffer), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_SMOOTH_MOTION:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, interp_smooth_motion ? "On" : "Off", FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_VFX:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, capitalized_name(vfx_level_names[vfx_level], buffer, sizeof buffer), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_MUSIC_VOLUME:
				JE_barDrawShadow(VGAScreen, xMenuItemValue, y, 1, music_disabled ? 170 : 174, (tyrMusicVolume + 4) / 8, 2, 10);
				JE_rectangle(VGAScreen, xMenuItemValue - 2, y - 2, xMenuItemValue + 96, y + 11, 242);
				break;

			case MENU_ITEM_SOUND_VOLUME:
				JE_barDrawShadow(VGAScreen, xMenuItemValue, y, 1, samples_disabled ? 170 : 174, (fxVolume + 4) / 8, 2, 10);
				JE_rectangle(VGAScreen, xMenuItemValue - 2, y - 2, xMenuItemValue + 96, y + 11, 242);
				break;

			default:
				break;
			}
		}

		// Draw status text.
		JE_textShade(VGAScreen, xMenuItemName, yMenuStatus, menuItems[*selectedMenuItemIndex].description, 15, 4, PART_SHADE);

		// Draw picker box and items.

		if (currentPicker != MENU_ITEM_NONE)
		{
			const MenuItem *selectedMenuItem = &menuItems[*selectedMenuItemIndex];
			const size_t pickerItemsCount = selectedMenuItem->getPickerItemsCount();

			const int hPicker = dyPickerItem * pickerItemsCount - dyPickerItemPadding;
			yPicker = MIN(yPicker, 200 - 10 - (hPicker + 5 + 2));

			JE_rectangle(VGAScreen, xMenuItemValue - 5, yPicker- 3, xMenuItemValue + wMenuItemValue + 5 - 1, yPicker + hPicker + 3 - 1, 248);
			JE_rectangle(VGAScreen, xMenuItemValue - 4, yPicker- 4, xMenuItemValue + wMenuItemValue + 4 - 1, yPicker + hPicker + 4 - 1, 250);
			JE_rectangle(VGAScreen, xMenuItemValue - 3, yPicker- 5, xMenuItemValue + wMenuItemValue + 3 - 1, yPicker + hPicker + 5 - 1, 248);
			fill_rectangle_wh(VGAScreen, xMenuItemValue - 2, yPicker - 2, wMenuItemValue + 2 + 2, hPicker + 2 + 2, 224);

			for (size_t i = 0; i < pickerItemsCount; ++i)
			{
				const int y = yPicker + dyPickerItem * (int)i;

				const bool selected = i == pickerSelectedIndex;

				const char *value = selectedMenuItem->getPickerItem(i, buffer, sizeof buffer);

				drawFontHvShadow(VGAScreen, xMenuItemValue, y, value, FONT_NORMAL, 15, -3 + (selected ? 2 : 0), false, 2);
			}
		}

		if (restart)
		{
			mouseCursor = MOUSE_POINTER_NORMAL;

			fade_palette(colors, 10, 0, 255);

			restart = false;
		}

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		int oldFullscreenDisplay = fullscreen_display;
		while (true)
		{
			waitUntilElapsed();

			// If full-screen is toggled via keyboard shortcut then display
			// setting needs to be updated.
			if (fullscreen_display != oldFullscreenDisplay)
				break;

			if (hasInput(INPUT_ANY))
				break;

			setFrameCount(1);
		}

		if (currentPicker == MENU_ITEM_NONE)
		{
			// Handle menu item interaction.

			bool action = false;

			MouseInput mouseInput;
			KeyboardInput keyboardInput;

			if (mouseGetInput(INPUT_ANY, &mouseInput))
			{
				// Find menu item name or value that was hovered or clicked.
				if (mouseInput.x >= xMenuItem && mouseInput.x < xMenuItem + wMenuItem)
				{
					for (size_t i = 0; i < menuItemsCount; ++i)
					{
						const int yMenuItem = yMenuItems + dyMenuItems * i;
						if (mouseInput.y >= yMenuItem && mouseInput.y < yMenuItem + hMenuItem)
						{
							if (*selectedMenuItemIndex != i)
							{
								JE_playSampleNum(S_CURSOR);

								*selectedMenuItemIndex = i;
							}

							if (mouseInput.button == SDL_BUTTON_LEFT &&
							    mouseInput.y >= yMenuItem && mouseInput.y < yMenuItem + hMenuItem)
							{
								// Act on menu item via name.
								if (mouseInput.x >= xMenuItemName && mouseInput.x < xMenuItemName + wMenuItemName)
								{
									action = true;
								}

								// Act on menu item via value.
								else if (mouseInput.x >= xMenuItemValue && mouseInput.x < xMenuItemValue + wMenuItemValue)
								{
									switch (menuItems[*selectedMenuItemIndex].id)
									{
									case MENU_ITEM_DISPLAY:
									case MENU_ITEM_SCALER:
									case MENU_ITEM_SCALING_MODE:
									case MENU_ITEM_PRESENTATION:
									case MENU_ITEM_ASPECT:
									case MENU_ITEM_PIXEL_ASPECT:
									case MENU_ITEM_SMOOTH_MOTION:
									{
										action = true;
										break;
									}
									case MENU_ITEM_MUSIC_VOLUME:
									{
										JE_playSampleNum(S_CURSOR);

										int value = (mouseInput.x - xMenuItemValue) * 255 / (wMenuItemValue - 1);
										tyrMusicVolume = MIN(MAX(0, value), 255);

										set_volume(tyrMusicVolume, fxVolume);
										break;
									}
									case MENU_ITEM_SOUND_VOLUME:
									{
										int value = (mouseInput.x - xMenuItemValue) * 255 / (wMenuItemValue - 1);
										fxVolume = MIN(MAX(0, value), 255);

										set_volume(tyrMusicVolume, fxVolume);

										JE_playSampleNum(S_CURSOR);
										break;
									}
									default:
										break;
									}
								}
							}

							break;
						}
					}
				}

				if (mouseInput.button == SDL_BUTTON_RIGHT)
				{
					JE_playSampleNum(S_SPRING);

					currentMenu = menuParents[currentMenu];
				}
			}
			else if (keyboardGetInput(&keyboardInput))
			{
				switch (keyboardInput.scancode)
				{
				case SDL_SCANCODE_UP:
				{
					JE_playSampleNum(S_CURSOR);

					*selectedMenuItemIndex = *selectedMenuItemIndex == 0
						? menuItemsCount - 1
						: *selectedMenuItemIndex - 1;
					break;
				}
				case SDL_SCANCODE_DOWN:
				{
					JE_playSampleNum(S_CURSOR);

					*selectedMenuItemIndex = *selectedMenuItemIndex == menuItemsCount - 1
						? 0
						: *selectedMenuItemIndex + 1;
					break;
				}
				case SDL_SCANCODE_LEFT:
				{
					switch (menuItems[*selectedMenuItemIndex].id)
					{
					case MENU_ITEM_MUSIC_VOLUME:
					{
						JE_playSampleNum(S_CURSOR);

						JE_changeVolume(&tyrMusicVolume, -8, &fxVolume, 0);
						break;
					}
					case MENU_ITEM_SOUND_VOLUME:
					{
						JE_changeVolume(&tyrMusicVolume, 0, &fxVolume, -8);

						JE_playSampleNum(S_CURSOR);
						break;
					}
					default:
						break;
					}
					break;
				}
				case SDL_SCANCODE_RIGHT:
				{
					switch (menuItems[*selectedMenuItemIndex].id)
					{
					case MENU_ITEM_MUSIC_VOLUME:
					{
						JE_playSampleNum(S_CURSOR);

						JE_changeVolume(&tyrMusicVolume, 8, &fxVolume, 0);
						break;
					}
					case MENU_ITEM_SOUND_VOLUME:
					{
						JE_changeVolume(&tyrMusicVolume, 0, &fxVolume, 8);

						JE_playSampleNum(S_CURSOR);
						break;
					}
					default:
						break;
					}
					break;
				}
				case SDL_SCANCODE_SPACE:
				case SDL_SCANCODE_RETURN:
				{
					action = true;
					break;
				}
				case SDL_SCANCODE_ESCAPE:
				{
					JE_playSampleNum(S_SPRING);

					currentMenu = menuParents[currentMenu];
					break;
				}
				default:
					break;
				}
			}

			// A Modern-only setting is greyed out and ignored while Classic is
			// active, so its picker never opens.
			const MenuItem *const selectedMenuItem = &menuItems[*selectedMenuItemIndex];
			if (action && !(selectedMenuItem->requiresModern && presentation != PRESENTATION_MODERN))
			{
				const MenuItemId selectedMenuItemId = selectedMenuItem->id;

				switch (selectedMenuItemId)
				{
				case MENU_ITEM_DONE:
				{
					JE_playSampleNum(S_SELECT);

					currentMenu = menuParents[currentMenu];
					break;
				}
				case MENU_ITEM_GRAPHICS:
				{
					JE_playSampleNum(S_SELECT);

					menuParents[MENU_GRAPHICS] = currentMenu;
					currentMenu = MENU_GRAPHICS;
					selectedMenuItemIndexes[currentMenu] = 0;
					break;
				}
				case MENU_ITEM_SOUND:
				{
					JE_playSampleNum(S_SELECT);

					menuParents[MENU_SOUND] = currentMenu;
					currentMenu = MENU_SOUND;
					selectedMenuItemIndexes[currentMenu] = 0;
					break;
				}
				case MENU_ITEM_JUKEBOX:
				{
					JE_playSampleNum(S_SELECT);

					fade_black(10);

					jukebox();

					restart = true;
					break;
				}
				case MENU_ITEM_DESTRUCT:
				{
					JE_playSampleNum(S_SELECT);

					fade_black(10);

					JE_destructGame();

					restart = true;
					break;
				}
				case MENU_ITEM_DISPLAY:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = (size_t)(fullscreen_display + 1);
					break;
				}
				case MENU_ITEM_SCALER:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = scaler;
					break;
				}
				case MENU_ITEM_SCALING_MODE:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = scaling_mode;
					break;
				}
				case MENU_ITEM_PRESENTATION:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = (size_t)presentation;
					break;
				}
				case MENU_ITEM_ASPECT:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = (size_t)modern_aspect;
					break;
				}
				case MENU_ITEM_PIXEL_ASPECT:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = (size_t)modern_pixel_aspect;
					break;
				}
				case MENU_ITEM_SMOOTH_MOTION:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = interp_smooth_motion ? 0 : 1;
					break;
				}
				case MENU_ITEM_VFX:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = (size_t)vfx_level;
					break;
				}
				case MENU_ITEM_MUSIC_VOLUME:
				{
					JE_playSampleNum(S_CLICK);

					music_disabled = !music_disabled;
					if (!music_disabled)
						restart_song();
					break;
				}
				case MENU_ITEM_SOUND_VOLUME:
				{
					samples_disabled = !samples_disabled;

					JE_playSampleNum(S_CLICK);
					break;
				}
				default:
					break;
				}
			}

			if (currentMenu == MENU_NONE)
			{
				fade_black(10);

				return;
			}
		}
		else
		{
			const MenuItem *selectedMenuItem = &menuItems[*selectedMenuItemIndex];

			// Handle picker interaction.

			bool action = false;

			MouseInput mouseInput;
			KeyboardInput keyboardInput;

			if (mouseGetInput(INPUT_ANY, &mouseInput))
			{
				const size_t pickerItemsCount = selectedMenuItem->getPickerItemsCount();

				// Find picker item that was hovered or clicked.
				if (mouseInput.x >= xMenuItemValue && mouseInput.x < xMenuItemValue + wMenuItemValue)
				{
					for (size_t i = 0; i < pickerItemsCount; ++i)
					{
						const int yPickerItem = yPicker + dyPickerItem * i;

						if (mouseInput.y >= yPickerItem && mouseInput.y < yPickerItem + hPickerItem)
						{
							if (pickerSelectedIndex != i)
							{
								JE_playSampleNum(S_CURSOR);

								pickerSelectedIndex = i;
							}

							// Act on picker item.
							if (mouseInput.button == SDL_BUTTON_LEFT &&
							    mouseInput.x >= xMenuItemValue && mouseInput.y < xMenuItemValue + wMenuItemName &&
							    mouseInput.y >= yPickerItem && mouseInput.y < yPickerItem + hPickerItem)
							{
								action = true;
							}
						}
					}
				}

				if (mouseInput.button == SDL_BUTTON_RIGHT)
				{
					JE_playSampleNum(S_SPRING);

					currentPicker = MENU_ITEM_NONE;
				}
			}
			else if (keyboardGetInput(&keyboardInput))
			{
				switch (keyboardInput.scancode)
				{
				case SDL_SCANCODE_UP:
				{
					JE_playSampleNum(S_CURSOR);

					const size_t pickerItemsCount = selectedMenuItem->getPickerItemsCount();

					pickerSelectedIndex = pickerSelectedIndex == 0
						? pickerItemsCount - 1
						: pickerSelectedIndex - 1;
					break;
				}
				case SDL_SCANCODE_DOWN:
				{
					JE_playSampleNum(S_CURSOR);

					const size_t pickerItemsCount = selectedMenuItem->getPickerItemsCount();

					pickerSelectedIndex = pickerSelectedIndex == pickerItemsCount - 1
						? 0
						: pickerSelectedIndex + 1;
					break;
				}
				case SDL_SCANCODE_SPACE:
				case SDL_SCANCODE_RETURN:
				{
					action = true;
					break;
				}
				case SDL_SCANCODE_ESCAPE:
				{
					JE_playSampleNum(S_SPRING);

					currentPicker = MENU_ITEM_NONE;
					break;
				}
				default:
					break;
				}
			}

			if (action)
			{
				JE_playSampleNum(S_CLICK);

				switch (selectedMenuItem->id)
				{
				case MENU_ITEM_DISPLAY:
				{
					if ((int)pickerSelectedIndex - 1 != fullscreen_display)
						reinit_fullscreen((int)pickerSelectedIndex - 1);
					break;
				}
				case MENU_ITEM_SCALER:
				{
					if (pickerSelectedIndex != scaler)
					{
						const int oldScaler = scaler;
						if (!init_scaler(pickerSelectedIndex) &&  // try new scaler
							!init_scaler(oldScaler))              // revert on fail
						{
							exit(EXIT_FAILURE);
						}
					}
					break;
				}
				case MENU_ITEM_SCALING_MODE:
				{
					scaling_mode = pickerSelectedIndex;
					break;
				}
				case MENU_ITEM_PRESENTATION:
				{
					presentation = (Presentation)pickerSelectedIndex;
					video_apply_display_settings();
					break;
				}
				case MENU_ITEM_ASPECT:
				{
					modern_aspect = (ModernAspect)pickerSelectedIndex;
					video_apply_display_settings();
					break;
				}
				case MENU_ITEM_PIXEL_ASPECT:
				{
					modern_pixel_aspect = (ModernPixelAspect)pickerSelectedIndex;
					video_apply_display_settings();
					break;
				}
				case MENU_ITEM_SMOOTH_MOTION:
				{
					interp_smooth_motion = pickerSelectedIndex == 0;
					break;
				}
				case MENU_ITEM_VFX:
				{
					vfx_level = (VfxLevel)pickerSelectedIndex;
					break;
				}
				default:
					break;
				}

				currentPicker = MENU_ITEM_NONE;
			}
		}
	}
}

int main(int argc, char *argv[])
{
#ifndef NDEBUG
	SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
#endif

	mt_srand(time(NULL));

	logInfo("%s", "");
	logInfo("Welcome to... >> %s %s <<", opentyrian_str, opentyrian_version);
	logInfo("%s", "");
	logInfo("Copyright (C) The OpenTyrian Development Team");
	logInfo("%s", "");
	logInfo("This program comes with ABSOLUTELY NO WARRANTY.");
	logInfo("This is free software, and you are welcome to redistribute it");
	logInfo("under certain conditions.  See the file COPYING for details.");
	logInfo("%s", "");

	// Detect regress/selftest mode before SDL_Init(): the regress hint below
	// must be set before SDL_Init(), and detecting the mode before loading the
	// configuration lets the user's config and save files be skipped.
	// JE_paramCheck() below does the real parsing.
	bool regress = regress_scan_args(argc, argv);
	bool selftest = gamepad_selftest_scan_args(argc, argv);

	// macOS: a regress run must not become (or be brought to) the foreground,
	// or a stray osascript/System Events keystroke or a cursor warp from the
	// real desktop could focus it and inject input mid-run.  SDL ignores this
	// hint on other platforms.  The input events themselves are discarded in
	// handleSdlEvents() as well.
	if (regress)
		SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");

	if (!SDL_Init(0))
	{
		logFatal("Failed to initialize SDL: %s", SDL_GetError());
		return EXIT_FAILURE;
	}

	atexit(SDL_Quit);

	if (!regress && !selftest)
	{
		loadConfiguration();
		loadSaves();
	}

	xmas = xmas_time();  // arg handler may override

	JE_paramCheck(argc, argv);

	if (selftest)
	{
		int result = gamepad_selftest_run();

		return result;
	}

	if (regress)
	{
		// Apply the regress pins after JE_paramCheck() so they win over any
		// command-line option (including -x/-X, which set xmas).
		regress_init();
		xmas = false;
	}

	logInfo("Presentation mode: %s.", presentation_names[presentation]);

	if (presentation == PRESENTATION_MODERN)
	{
		logInfo("Modern geometry: aspect %s, pixel aspect %s.", modern_aspect_names[modern_aspect], modern_pixel_aspect_names[modern_pixel_aspect]);
		logInfo("Modern lighting: bloom %s, lighting %s.", modern_quality_names[modern_bloom_quality], modern_quality_names[modern_lighting_quality]);
	}

	if (!findDataFiles())
	{
		logFatal("The Tyrian data files were not found.  OpenTyrian requires the Tyrian v2.0/v2.1 data files.");
		return EXIT_FAILURE;
	}

	File file = dataFileOpen("tyrian.shp", "rb");
	Uint16 temp = fileReadU16(&file);
	fileClose(&file);

	if (temp == 11)
	{
		logFatal("The Tyrian v1.0/v1.1 data files were found.  OpenTyrian requires the Tyrian v2.0/v2.1 data files.");
		return EXIT_FAILURE;
	}
	else if (temp == 13)
	{
		logFatal("The Tyrian 2000 data files were found.  OpenTyrian requires the Tyrian v2.0/v2.1 data files.");
		return EXIT_FAILURE;
	}

	if (regress_audio_active())
	{
		// Offline audio regression: no video, input or audio device needed.
		regress_audio_run();

		return EXIT_SUCCESS;
	}

	JE_scanForEpisodes();

	init_video();
	init_keyboard();
	init_joysticks();
	if (has_mouse)
		logInfo("Assuming mouse detected.");  // SDL can't tell us if there isn't one.

	if (xmas && (!dataFileExists("tyrianc.shp") || !dataFileExists("voicesc.snd")))
	{
		xmas = false;

		logWarn("Christmas is missing.");
	}

	loadPals();
	JE_loadMainShapeTables(xmas ? "tyrianc.shp" : "tyrian.shp");

	if (xmas && !xmas_prompt())
	{
		xmas = false;

		free_main_shape_tables();
		JE_loadMainShapeTables("tyrian.shp");
	}

	/* Default Options */
	youAreCheating = false;
	smoothScroll = true;
	loadDestruct = false;

	if (!audio_disabled)
	{
		logInfo("Initializing SDL audio...");

		init_audio();

		loadSndFile(xmas);
	}
	else
	{
		logInfo("Audio is disabled.");
	}

	if (recordDemo)
		logInfo("Game will be recorded.");

	loadExtraShapes();  /*Editship*/

	JE_loadHelpText();

	if (isNetworkGame)
	{
#ifdef WITH_NETWORK
		if (network_init())
		{
			network_tyrian_halt(3, false);
		}
#else
		logFatal("OpenTyrian was compiled without networking support.");
		return EXIT_FAILURE;
#endif
	}

	if (regress_screen_active())
	{
		// Render one non-gameplay screen in a deterministic state, present
		// --regress-frames frames and exit (see src/regress_screen.c).  This
		// never returns.
		regress_screen_run();
		return EXIT_SUCCESS;
	}

	if (regress_demo != 0 || regress_scenario_active())
	{
		// Replay the requested demo (or start the requested synthetic level)
		// directly, skipping intro, title and menus.
		if (regress_demo != 0)
			setDemoNumber(regress_demo);

		JE_initPlayerData();

		playDemo = true;

		JE_main();

		// JE_main() returns once playback ends (demo exhausted, scenario frame
		// cap reached, or level over).
		regress_finish();

		return EXIT_SUCCESS;
	}

	for (; ; )
	{
#ifdef NDEBUG
		if (!isNetworkGame && !stoppedDemo)
			intro_logos();
#endif

		JE_initPlayerData();
		JE_sortHighScores();

		playDemo = false;
		stoppedDemo = false;

		gameLoaded = false;
		jumpSection = false;

#ifdef WITH_NETWORK
		if (isNetworkGame)
		{
			networkStartScreen();
		}
		else
#endif
		{
			if (!titleScreen())
			{
				// Player quit from title screen.
				break;
			}
		}

		if (loadDestruct)
		{
			JE_destructGame();

			loadDestruct = false;
		}
		else
		{
			JE_main();

			if (trentWin)
			{
				// Player beat SuperTyrian.
				break;
			}
		}
	}

	JE_tyrianHalt(0);

	return 0;
}
