#!/bin/sh
# check_game_rules.sh - build and run test/unit/game_rules_test.c: consistency of
# the per-variant rule tables (game_rules.c) against the data schemas, and the
# high-score boards of both variants.  It needs no game data and touches none.
# regress.sh runs it with the other structural checks.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-cc}"
PKG_CONFIG="${PKG_CONFIG:-pkg-config}"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/game_rules_test.XXXXXX")" || exit 1
trap 'rm -rf "$WORK"' EXIT INT TERM

SDL_CFLAGS="$($PKG_CONFIG sdl3 --cflags 2>/dev/null | sed 's/-I\/include//')"

# shellcheck disable=SC2086
if ! "$CC" -std=iso9899:1999 -pedantic -Wall -Wextra -Wno-missing-field-initializers \
	-DTARGET_UNIX -I"$ROOT/src" $SDL_CFLAGS -o "$WORK/game_rules_test" \
	"$ROOT/test/unit/game_rules_test.c" "$ROOT/src/game_rules.c" "$ROOT/src/game_variant.c" \
	"$ROOT/src/game_schema.c" "$ROOT/src/sndmast.c" "$ROOT/src/highscores.c" 2> "$WORK/build.log"; then
	echo "FAIL game-rules: the check did not build"
	sed 's/^/  /' "$WORK/build.log" | head -n 20
	exit 1
fi

if ! "$WORK/game_rules_test"; then
	echo "FAIL game-rules"
	exit 1
fi
echo "PASS game-rules"

# Prove the legacy 68 action through the game itself, without changing any of
# the 164 frame/state baselines. Optional arguments supplied by regress.sh.
if [ "$#" -ge 3 ]; then
	mkdir -p "$3"
	if ! SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy "$1" --variant=2.1 --data="$2" \
		--regress-level=1:1 --regress-rules=events --regress-frames=180 \
		--regress-state-out="$3/state.txt" > "$3/run.log" 2>&1 ||
		! grep -Fq 'Rule coverage: 2.1 event 68 random explosions ran.' "$3/run.log"; then
		echo "FAIL game-rules: legacy event 68 runtime assertion"
		tail -n 8 "$3/run.log"
		exit 1
	fi
	echo "PASS game-rules: 2.1 event 68 runtime assertion"
fi
