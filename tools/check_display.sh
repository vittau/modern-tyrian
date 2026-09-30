#!/bin/bash
# Shared display lifecycle checks. A dummy window plus queued SDL mutations
# models Cocoa's asynchronous resize/fullscreen contract without opening a window.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN=${1:-"$ROOT/opentyrian"}
DATA=${2:-"$ROOT/data"}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/display-check.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/home"
export HOME="$WORK/home" SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
CC=${CC:-cc}
PKG_CONFIG=${PKG_CONFIG:-pkg-config}
DEFS=-DTARGET_UNIX
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) DEFS=-DTARGET_WIN32 ;; esac
PACKAGES=sdl3
if "$PKG_CONFIG" --exists sdl3-net; then PACKAGES="sdl3 sdl3-net"; DEFS="$DEFS -DWITH_NETWORK"; fi
CFLAGS=$($PKG_CONFIG --cflags $PACKAGES | sed 's/-I\/include//g')
LIBS=$($PKG_CONFIG --libs $PACKAGES | sed 's/-L\/lib //g')
cat > "$WORK/display_test.c" <<'C'
#include "video.h"
#include "modern.h"
#include "palette.h"
#include "game_variant.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool asynchronous;
static int pending_w, pending_h;
bool display_test_set_size(SDL_Window *window, int w, int h)
{
	if (!asynchronous) return SDL_SetWindowSize(window, w, h);
	pending_w = w; pending_h = h;
	return true;
}
bool display_test_fullscreen(SDL_Window *window, bool full)
{
	if (!asynchronous) return SDL_SetWindowFullscreen(window, full);
	// Leaving fullscreen exposes an old 4:3 restored window before resize.
	if (!full) SDL_SetWindowSize(window, 320, 240);
	pending_w = full ? 3440 : 320;
	pending_h = full ? 1440 : 240;
	return true;
}
bool display_test_sync(SDL_Window *window)
{
	if (pending_w) {
		SDL_SetWindowSize(window, pending_w, pending_h);
		pending_w = pending_h = 0;
	}
	return SDL_SyncWindow(window);
}
static void check_canvas(void)
{
	int w, h;
	SDL_GetWindowSize(main_window, &w, &h);
	float aspect = modern_aspect == MODERN_ASPECT_AUTO ? (float)w / h : modern_aspect_ratio();
	int expected = (int)lroundf(200 * 1.2f * aspect);
	if (expected < 320) expected = 320;
	assert(modern_current_frame()->w == expected);
	assert(pending_w == 0);
	assert(modern_hud_in_panels() == (expected >= MODERN_PLAYFIELD_W + 2 * MODERN_HUD_MIN_PANEL_WIDTH));
	if (modern_hud_in_panels()) {
		for (int p = 0; p < 2; ++p) {
			SDL_Surface *hud = modern_hud_surface(p);
			assert(hud != NULL && hud->w >= modern_side_panel_width());
			memset(hud->pixels, 1, (size_t)hud->pitch * hud->h);
		}
		modern_mark_gameplay_frame();
		modern_build_frame(VGAScreen);
		assert(modern_last_frame_gameplay_panels());
	}
}
static void check_modal(ModernAspect aspect)
{
	modern_aspect = aspect;
	video_apply_display_settings();
	memset(VGAScreen->pixels, 2, (size_t)VGAScreen->pitch * VGAScreen->h);
	modern_backdrop_set(1, VGAScreen->pixels, VGAScreen->pitch);
	SDL_Surface *dialog = modern_dialog_begin(VGAScreen);
	const int width = 189;
	const int left = 50 + modern_dialog_offset_x(50, width);
	for (int y = 50; y < 150; ++y)
		memset((Uint8 *)dialog->pixels + y * dialog->pitch + left, 3, width);
	modern_build_frame(dialog);
	const ModernFrame *frame = modern_current_frame();
	int first = -1, last = -1;
	for (int x = 0; x < frame->w; ++x)
		if (frame->pixels[75 * frame->w + x] == rgb_palette[3]) {
			if (first < 0) first = x;
			last = x;
		}
	assert(last - first + 1 == width);
	assert(first == (frame->w - width) / 2);
	assert(!modern_frame_is_split(NULL, NULL, NULL, NULL));
	modern_present_frame();
	Sint32 x = left + 25, y = 128, original_x = x;
	mapScreenPointToWindow(&x, &y);
	mapWindowPointToScreen(&x, &y);
	assert(x >= original_x - 1 && x <= original_x + 1 && y >= 127 && y <= 129);
	modern_dialog_end();
	modern_backdrop_clear();
}
int main(void)
{
	presentation = PRESENTATION_MODERN;
	modern_aspect = MODERN_ASPECT_16_9;
	fullscreen_display = -1;
	init_video();
	rgb_palette[1] = 0xffffff; rgb_palette[2] = 0x123456; rgb_palette[3] = 0xabcdef;
	asynchronous = true;
	for (int variant = 0; variant <= VARIANT_TYRIAN2000; ++variant) {
		assert(gameVariantSelect((GameVariant)variant) == GAME_VARIANT_OK);
		modern_aspect = MODERN_ASPECT_AUTO;
		SDL_SetWindowSize(main_window, 3440, 1440);
		modern_update_canvas_size();
		reinit_fullscreen(-1); check_canvas();
		assert(modern_hud_in_panels());
		reinit_fullscreen(0); check_canvas();
		reinit_fullscreen(-1); check_canvas();
		assert(modern_hud_in_panels());
		modern_aspect = MODERN_ASPECT_4_3;
		video_apply_display_settings(); check_canvas();
		assert(!modern_hud_in_panels());
		modern_aspect = MODERN_ASPECT_16_9;
		video_apply_display_settings(); check_canvas();
		modern_aspect = MODERN_ASPECT_AUTO;
		SDL_SetWindowSize(main_window, 3200, 900);
		video_on_win_resize(); check_canvas();
		assert(modern_current_frame()->w == 853);
		assert(video_fit_launcher_window());
		assert(pending_w == 0);
		int w, h, rw, rh;
		SDL_GetWindowSizeInPixels(main_window, &w, &h);
		SDL_GetRenderOutputSize(video_renderer(), &rw, &rh);
		assert(w == rw && h == rh);
		check_modal(MODERN_ASPECT_21_9);
		check_modal(MODERN_ASPECT_32_9);
	}
	deinit_video();
	puts("PASS display: async fullscreen/windowed, resize, aspect, launcher sizing, modal centre and mouse mapping (both variants)");
	return 0;
}
C
# Reuse the normal build's objects; rename main and wrap only window mutations.
# No project sources or build files are generated/changed by this check.
"$CC" -std=iso9899:1999 $DEFS $CFLAGS -I"$ROOT/src" -Dmain=display_unused_main \
	-c "$ROOT/src/opentyr.c" -o "$WORK/opentyr.o"
"$CC" -std=iso9899:1999 $DEFS $CFLAGS -I"$ROOT/src" \
	-DSDL_SetWindowSize=display_test_set_size -DSDL_SetWindowFullscreen=display_test_fullscreen \
	-DSDL_SyncWindow=display_test_sync -c "$ROOT/src/video.c" -o "$WORK/video.o"
OBJECTS=()
for obj in "$ROOT"/obj/*.o "$ROOT"/obj/third_party/miniz/*.o; do
	case "$obj" in */opentyr.o|*/video.o|*/resources.o) ;; *) OBJECTS+=("$obj") ;; esac
done
"$CC" -std=iso9899:1999 $DEFS $CFLAGS -I"$ROOT/src" "$WORK/display_test.c" \
	"$WORK/opentyr.o" "$WORK/video.o" "${OBJECTS[@]}" $LIBS -lm -o "$WORK/display_test"
"$WORK/display_test"
for size in 2560x1080 3840x1080; do
	"$BIN" --regress-launcher="$size,installed,1,first-frame" --regress-out="$WORK/first.txt" > "$WORK/launcher.log" 2>&1
	"$BIN" --regress-launcher="$size,installed,1" --regress-out="$WORK/steady.txt" >> "$WORK/launcher.log" 2>&1
	# Compare pixel hashes; the spec label intentionally differs.
	[ "$(sed 's/.*frame=//' "$WORK/first.txt")" = "$(sed 's/.*frame=//' "$WORK/steady.txt")" ]
done
echo 'PASS display: first launcher frame matches steady 21:9 and 32:9 output'
# Actual quit art is covered by the suite's hashes; add a 32:9 2.1 hash here.
"$BIN" --variant=2.1 --data="$DATA" --regress-screen=quit --regress-modern \
	--regress-aspect=32:9 --regress-frames=3 --regress-out="$WORK/quit.txt" > "$WORK/quit.log" 2>&1
cmp "$WORK/quit.txt" "$ROOT/test/regress/display-quit-32x9.txt"
echo 'PASS display: quit modal 32:9 frame hash'
