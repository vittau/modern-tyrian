#!/bin/bash
# Final matrix observers. Data and save files never enter baseline output.
set -eEu -o pipefail
SOURCE_BIN=$1
VARIANT=$2
DATA=$(cd "$3" && pwd -P)
OUT=$4
ROOT=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
SCRATCH=$(mktemp -d "${TMPDIR:-/tmp}/tyrian-final.XXXXXX")
trap 'rm -rf "$SCRATCH"' EXIT
trap 'echo "FAIL final-regression: $VARIANT line $LINENO: $BASH_COMMAND" >&2' ERR
mkdir -p "$SCRATCH/bin" "$SCRATCH/cwd/data" "$SCRATCH/default" "$SCRATCH/user" "$SCRATCH/home"
native_path() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s\n' "$1"; fi; }
NATIVE_DEFAULT=$(native_path "$SCRATCH/default")
NATIVE_DATA=$(native_path "$DATA")
BIN="$SCRATCH/bin/$(basename "$SOURCE_BIN")"
cp "$SOURCE_BIN" "$BIN"
# Decoys at the executable search, ./data, cwd fallback and the redirected
# compiled-default root. Never alter the machine's compiled installation directory.
if [ -d "$ROOT/data" ]; then
	cp -R "$ROOT/data" "$SCRATCH/bin/data"
	cp -R "$ROOT/data/." "$SCRATCH/cwd/data/"
	cp -R "$ROOT/data/." "$SCRATCH/default/"
	cp "$ROOT/data/tyrian1.lvl" "$SCRATCH/cwd/tyrian1.lvl"
fi
run() (
	label=$1; shift
	cd "$SCRATCH/cwd"
	export HOME="$SCRATCH/home" XDG_CONFIG_HOME="$SCRATCH/home" APPDATA="$SCRATCH/home" SteamDeck=1
	export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
	export TYRIAN_REGRESS_DEFAULT="$NATIVE_DEFAULT" TYRIAN_DIR="$SCRATCH/default" TYRIAN2000_DATA="$SCRATCH/default"
	if [ "$VARIANT" = 2000 ] && [[ "$label" = namespace-2.1 || "$label" = mirror-* ]]; then
		export TYRIAN2000_DATA="$NATIVE_DATA" TYRIAN_REGRESS_DEFAULT="$NATIVE_DATA"
	fi
	"$BIN" --data="$DATA" --regress-data-audit="$DATA" --regress-state-out="$OUT/$label.txt" "$@"
) > "$OUT/$1.log" 2>&1
need() { grep -Fq "$2" "$OUT/$1.log"; }
record() {
	local label=$1 checksum bytes
	read -r checksum bytes _ < <(cksum "$OUT/$label.txt")
	printf '%s %s %s\n' "$label" "$bytes" "$checksum" >> "$OUT/final-regression.txt"
}
: > "$OUT/final-regression.txt"
# Prove the observer rejects an out-of-root read, rather than just printing it.
status=0
run audit-reject --variant="$VARIANT" --regress-screen=title --regress-data-audit="$SCRATCH/cwd" || status=$?
[ "$status" = 1 ]
need audit-reject 'Data audit FAIL: open outside selected root.'
# Demonstrate that the redirected compiled default is a real search location.
mkdir -p "$SCRATCH/probe-bin" "$SCRATCH/empty-cwd"
cp "$SOURCE_BIN" "$SCRATCH/probe-bin/$(basename "$SOURCE_BIN")"
(cd "$SCRATCH/empty-cwd" && HOME="$SCRATCH/home" XDG_CONFIG_HOME="$SCRATCH/home" APPDATA="$SCRATCH/home" \
	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TYRIAN_REGRESS_DEFAULT="$NATIVE_DEFAULT" \
	"$SCRATCH/probe-bin/$(basename "$SOURCE_BIN")" --variant=2.1 --regress-screen=title \
	--regress-data-audit="$SCRATCH/default" --regress-frames=90 --regress-state-out="$OUT/default-search.txt") > "$OUT/default-search.log" 2>&1
need default-search 'validation: ok.'
# Every successful open must pass the in-process resolved-root assertion.
run audit-title --variant="$VARIANT" --regress-screen=title --regress-frames=90
need audit-title 'Data audit:'
record audit-title
EPISODES=4
[ "$VARIANT" = 2000 ] && EPISODES=5
for ep in $(seq 1 "$EPISODES"); do
	level=$(SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy "$BIN" --variant="$VARIANT" --data="$DATA" \
		--regress-flow="list-levels:ep=$ep" --regress-out="$SCRATCH/list.txt" 2>/dev/null | sed -n '/^[0-9][0-9]*:[0-9][0-9]*$/p' | head -n 1)
	[ -n "$level" ]
	run "audit-episode$ep" --variant="$VARIANT" --regress-script="$level" --regress-frames=150 --regress-seed=2000
	need "audit-episode$ep" 'Data audit:'
	record "audit-episode$ep"
	# Discover bosses via loaded event 79 records, not copied level metadata.
	found=0
	for physical in $(seq 1 20); do
		run "boss-e$ep-l$physical" --variant="$VARIANT" --regress-level="$ep:$physical" --regress-boss --regress-frames=1200
		if grep -Fq 'Boss coverage: fight ran 60 ticks.' "$OUT/boss-e$ep-l$physical.log"; then
			need "boss-e$ep-l$physical" ' active episode '
			record "boss-e$ep-l$physical"
			found=1; break
		fi
	done
	[ "$found" = 1 ]
done
# Real full-game menu navigation, including the launcher's gamepad reducer and
# the shared last_variant preference. This is one variant/provider per process.
run handoff --regress-handoff="$VARIANT" --regress-flow=episode:ep=1,ticks=4 --regress-gamepad --regress-user-root="$SCRATCH/user"
need handoff "Launcher coverage: gamepad chose $VARIANT."
need handoff 'Flow coverage: flow complete.'
grep -Eq "User audit: rb .*[/\\]$( [ "$VARIANT" = 2000 ] && echo tyrian2000 || echo tyrian21 )[/\\]tyrian.sav" "$OUT/handoff.log"
record handoff
run handoff-reload --regress-handoff="$VARIANT" --regress-flow=episode:ep=1,ticks=4 --regress-gamepad --regress-user-root="$SCRATCH/user"
need handoff-reload "Launcher coverage: preselected $VARIANT."
cmp "$OUT/handoff.txt" "$OUT/handoff-reload.txt"
# Real Save confirmation and title Load; each uses a fresh slot in a sandbox.
for ep in 1 "$EPISODES"; do
	mkdir -p "$SCRATCH/save$ep"
	run "save-ui-e$ep" --variant="$VARIANT" --regress-flow="save:ep=$ep,ticks=40" --regress-user-root="$SCRATCH/save$ep"
	need "save-ui-e$ep" 'slot 1 saved through options menu.'
	need "save-ui-e$ep" 'save/load state hash identical; title Load screen used.'
	need "save-ui-e$ep" 'loaded game continued for 40 ticks.'
	namespace=tyrian21; size=2502
	[ "$VARIANT" = 2000 ] && namespace=tyrian2000 && size=4722
	[ "$(wc -c < "$SCRATCH/save$ep/$namespace/tyrian.sav" | tr -d ' ')" = "$size" ]
	record "save-ui-e$ep"
done
# Both menu and pause hold the gameplay composition in Modern.
for menu in ingame pause; do
	run "pause-$menu" --variant="$VARIANT" --regress-script=1:3 --regress-seed=32402394 --regress-detail=4 \
		--regress-frames=700 --regress-menu="$menu" --regress-modern --regress-aspect=16:9 --regress-gameplay-check --regress-out="$OUT/pause-$menu-frames.txt"
	need "pause-$menu" 'Menu coverage: in-level request'
	need "pause-$menu" 'Gameplay composition check:'
	record "pause-$menu"
	record "pause-$menu-frames"
done
if [ "$VARIANT" = 2000 ]; then
	run audit-battle --variant=2000 --regress-flow=battle:sel=1,ticks=40,cash=999999 --regress-user-root="$SCRATCH/battle"
	need audit-battle 'Timed Battle saving unavailable:'
	need audit-battle 'Timed Battle 1 over'
	record audit-battle
	run mouse-gamepad --variant=2000 --regress-flow=mouse:ep=1 --regress-gamepad
	for b in 0 1 2; do need mouse-gamepad "Mouse button $b action cycled."; done
	need mouse-gamepad 'Mouse Reset restored all defaults.'
	need mouse-gamepad 'Mouse Done returned to options.'
	record mouse-gamepad
	# Same user root, both actual codecs. Preserve the other variant's bytes
	# across writes, then reload each serializer without default regeneration.
	DATA21=$(cd "$ROOT/data" && pwd -P)
	run mirror-title --variant=2.1 --data="$DATA21" --regress-data-audit="$DATA21" --regress-screen=title --regress-frames=90
	need mirror-title 'Data audit:'
	# Switch the shared launcher preference both ways across processes, with
	# the opposite real root available as a default-search decoy.
	run mirror-handoff --data="$DATA21" --regress-data-audit="$DATA21" --regress-handoff=2.1 \
		--regress-flow=episode:ep=1,ticks=4 --regress-gamepad --regress-user-root="$SCRATCH/user"
	need mirror-handoff 'Launcher coverage: preselected 2000.'
	need mirror-handoff 'Launcher coverage: gamepad chose 2.1.'
	run handoff-back --regress-handoff=2000 --regress-flow=episode:ep=1,ticks=4 --regress-gamepad --regress-user-root="$SCRATCH/user"
	need handoff-back 'Launcher coverage: preselected 2.1.'
	need handoff-back 'Launcher coverage: gamepad chose 2000.'
	cmp "$OUT/handoff.txt" "$OUT/handoff-back.txt"
	for v in 2.1 2000 2.1 2000; do
		run "namespace-$v" --variant="$v" --regress-user-files --regress-user-root="$SCRATCH/user" \
			--data="$( [ "$v" = 2.1 ] && echo "$DATA21" || echo "$DATA" )" \
			--regress-data-audit="$( [ "$v" = 2.1 ] && echo "$DATA21" || echo "$DATA" )"
		[ ! -f "$SCRATCH/previous-$v.sav" ] || cmp "$SCRATCH/previous-$v.sav" "$SCRATCH/user/$( [ "$v" = 2.1 ] && echo tyrian21 || echo tyrian2000 )/tyrian.sav"
		cp "$SCRATCH/user/$( [ "$v" = 2.1 ] && echo tyrian21 || echo tyrian2000 )/tyrian.sav" "$SCRATCH/previous-$v.sav"
	done
	[ "$(wc -c < "$SCRATCH/user/tyrian21/tyrian.sav" | tr -d ' ')" = 2502 ]
	[ "$(wc -c < "$SCRATCH/user/tyrian2000/tyrian.sav" | tr -d ' ')" = 4722 ]
	[ -f "$SCRATCH/user/opentyrian.cfg" ] && [ -f "$SCRATCH/user/tyrian.cfg" ]
	[ ! -f "$SCRATCH/user/tyrian21/opentyrian.cfg" ] && [ ! -f "$SCRATCH/user/tyrian2000/opentyrian.cfg" ]
	grep -Eq 'User audit: rb .*[/\]tyrian21[/\]tyrian.sav' "$OUT/namespace-2.1.log"
	grep -Eq 'User audit: rb .*[/\]tyrian2000[/\]tyrian.sav' "$OUT/namespace-2000.log"
	! grep -Eq 'User audit: rb .*[/\]tyrian2000[/\]tyrian.sav'  "$OUT/namespace-2.1.log"
	! grep -Eq 'User audit: rb .*[/\]tyrian21[/\]tyrian.sav'  "$OUT/namespace-2000.log"
fi
# Independent adapter/hot-plug/remapping selftest for this variant.
(cd "$SCRATCH/cwd" && HOME="$SCRATCH/home" XDG_CONFIG_HOME="$SCRATCH/home" APPDATA="$SCRATCH/home" \
	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy "$BIN" --variant="$VARIANT" --selftest-gamepad) > "$OUT/gamepad-selftest.log" 2>&1
printf 'PASS final-regression %s: audited roots, bosses, UI saves, pause, gamepad and handoff\n' "$VARIANT"
