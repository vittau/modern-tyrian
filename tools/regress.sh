#!/bin/bash
# regress.sh — headless regression harness for OpenTyrian.
#
# Builds the game and compares per-frame 8-bit framebuffer hashes against the
# committed baselines in test/regress/.  Two kinds of cases are run:
#
#   1. the five recorded demos (demo.1 .. demo.5) at every processor detail
#      level (1 .. 6), as demoN-dM;
#   2. synthetic level scenarios that cover render paths the demos never reach
#      (smoothies[] stays zero for the whole of every demo), as
#      scenario-<name>-dM.  See SCENARIOS below;
#   3. the Modern presentation (--regress-modern), which hashes the CPU-composed
#      XRGB8888 canvas instead of the 8-bit frame, as modern-<case>; the
#      modern-wide-<case> subset runs the same canvas at 16:9 (wider canvas plus
#      the procedural side panels).
#
# It also runs the offline audio regression (--regress-audio) as the "audio"
# case, which hashes the converted sound effects, per-second music rendering and
# per-second sound-effect mixing.  See src/regress_audio.c.
#
#   tools/regress.sh              build, run all cases, compare
#   tools/regress.sh --update     regenerate the baselines from the current tree
#
# The Tyrian data directory comes from $TYRIAN_DATA, defaulting to ./data.  If
# the data is missing, ./get_data.sh fetches the freeware Tyrian 2.1 release.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

UPDATE=0
if [ "${1:-}" = "--update" ]; then
	UPDATE=1
fi

DATA_DIR="${TYRIAN_DATA:-$ROOT/data}"
BIN="$ROOT/opentyrian"
BASELINE_DIR="$ROOT/test/regress"
ACTUAL_DIR="$BASELINE_DIR/actual"
DEMOS="1 2 3 4 5"
LEVELS="1 2 3 4 5 6"

# Synthetic scenarios: "name episode:level frame-cap detail...".
#
# Each entry starts a level directly (fixed RNG seed, fixed new-game loadout, no
# input, extra lives via the engine's own youAreCheating flag) and hashes the
# requested number of frames.  The details listed are exactly the ones where the
# scenario's covered path is reachable: lava/water need processorType > 2, and
# blur/iced need processorType > 1.  The six render paths are covered as:
#
#   lava_filter            scenario-flip / scenario-spotlight
#   water_filter           scenario-water
#   iced_blur_filter       scenario-iced
#   blur_filter            scenario-blur
#   starShowVGA code 1     scenario-flip        (vertical flip)
#   starShowVGA code 2     scenario-spotlight   (player spotlight)
SCENARIOS=(
	"water     4:9  1200 3 4 5 6"
	"flip      4:12 3600 3 4 5 6"
	"iced      4:8  1200 2 3 4 5 6"
	"spotlight 1:16 1200 3 4 5 6"
	"blur      4:19 1200 2 3 4 5 6"
)

# --- build -------------------------------------------------------------------

if [ "${REGRESS_SKIP_BUILD:-0}" != "1" ]; then
	echo "Building opentyrian..."
	if ! make -s; then
		echo "BUILD FAILED"
		exit 1
	fi
fi

if [ ! -x "$BIN" ]; then
	echo "ERROR: $BIN not found (build failed?)"
	exit 1
fi

# --- game data ---------------------------------------------------------------

if [ ! -f "$DATA_DIR/tyrian1.lvl" ]; then
	echo "Tyrian data not found in $DATA_DIR; fetching..."
	if ! "$ROOT/get_data.sh" "$DATA_DIR"; then
		echo "ERROR: failed to obtain Tyrian data"
		exit 1
	fi
fi

# --- helpers -----------------------------------------------------------------

mkdir -p "$BASELINE_DIR"
rm -rf "$ACTUAL_DIR"
mkdir -p "$ACTUAL_DIR"

now() {
	if command -v perl >/dev/null 2>&1; then
		perl -MTime::HiRes=time -e 'printf "%.3f", time'
	else
		date +%s
	fi
}

total_start=$(now)
failures=0
pairs=0

# run_case LABEL "$@" -- run the binary and compare/update one baseline.
# The output file is derived from LABEL.
run_case() {
	local label=$1
	shift
	local out="$ACTUAL_DIR/$label.txt"
	local log="$ACTUAL_DIR/$label.log"
	local baseline="$BASELINE_DIR/$label.txt"
	local start elapsed rc lines hunk first

	start=$(now)
	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" --data="$DATA_DIR" --regress-out="$out" "$@" \
		>"$log" 2>&1
	rc=$?
	elapsed=$(awk "BEGIN { printf \"%.2f\", $(now) - $start }")

	if [ "$rc" -ne 0 ]; then
		echo "FAIL $label: exit code $rc (${elapsed}s)"
		tail -n 5 "$log"
		failures=$((failures + 1))
		return
	fi

	if [ ! -f "$out" ]; then
		echo "FAIL $label: no output written (${elapsed}s)"
		failures=$((failures + 1))
		return
	fi

	lines=$(wc -l < "$out" | tr -d ' ')

	if [ "$UPDATE" -eq 1 ]; then
		cp "$out" "$baseline"
		echo "UPDATE $label: $lines lines, ${elapsed}s"
		return
	fi

	if [ ! -f "$baseline" ]; then
		echo "FAIL $label: missing baseline (run tools/regress.sh --update)"
		failures=$((failures + 1))
		return
	fi

	if cmp -s "$baseline" "$out"; then
		echo "PASS $label: $lines lines, ${elapsed}s"
	else
		# First differing hunk, e.g. "12c12" or "5,7c5,9"; line N is record N-1.
		hunk=$(diff "$baseline" "$out" | head -n 1)
		first=${hunk%%[cad]*}
		first=${first%%,*}
		echo "FAIL $label: first differing line $((first - 1)) (${elapsed}s)"
		failures=$((failures + 1))
	fi
}

# --- demos -------------------------------------------------------------------

for d in $DEMOS; do
	for m in $LEVELS; do
		pairs=$((pairs + 1))
		run_case "demo$d-d$m" --regress-demo="$d" --regress-detail="$m"
	done
done

# --- synthetic level scenarios ----------------------------------------------

for spec in "${SCENARIOS[@]}"; do
	# shellcheck disable=SC2086
	set -- $spec
	sname=$1
	slvl=$2
	sframes=$3
	shift 3
	for m in "$@"; do
		pairs=$((pairs + 1))
		run_case "scenario-$sname-d$m" \
			--regress-level="$slvl" --regress-detail="$m" --regress-frames="$sframes"
	done
done

# --- modern presentation -----------------------------------------------------
#
# The Modern path composes on the CPU at the logical resolution; --regress-modern
# hashes its XRGB8888 canvas instead of the 8-bit frame.  Keep the subset small:
# all five demos at detail 2, plus every synthetic scenario at its lowest valid
# detail.

for d in $DEMOS; do
	pairs=$((pairs + 1))
	run_case "modern-demo$d-d2" --regress-demo="$d" --regress-detail=2 --regress-modern
done

for spec in "${SCENARIOS[@]}"; do
	# shellcheck disable=SC2086
	set -- $spec
	sname=$1
	slvl=$2
	sframes=$3
	sdetail=$4
	pairs=$((pairs + 1))
	run_case "modern-scenario-$sname-d$sdetail" \
		--regress-level="$slvl" --regress-detail="$sdetail" --regress-frames="$sframes" --regress-modern
done

# --- modern widescreen presentation ------------------------------------------
#
# The same Modern canvas widened to 16:9 (original 1.2 pixel aspect), hashing
# the whole canvas including the side panels and the relocated HUD.  Three
# representative 16:9 cases keep the extra runtime small; the old modern-* cases
# above pin 4:3.  A 16:10 case is included because 16:10 now has wide-enough
# panels for the relocated HUD (60 px) and a different, compact layout.

run_case "modern-wide-demo1-d2" --regress-demo=1 --regress-detail=2 --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))
run_case "modern-wide-demo3-d2" --regress-demo=3 --regress-detail=2 --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))
run_case "modern-wide-scenario-spotlight-d3" \
	--regress-level=1:16 --regress-detail=3 --regress-frames=1200 --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))
run_case "modern-wide-16x10-demo1-d2" --regress-demo=1 --regress-detail=2 --regress-modern --regress-aspect=16:10
pairs=$((pairs + 1))

# --- modern bloom + dynamic lighting -----------------------------------------
#
# The bloom/dynamic-light pass is pinned OFF unless --regress-bloom/--regress-lighting
# opt in, so every case above is unaffected.  These two cases cover the effects
# on the 16:9 playfield: a demo with shots and explosions, and a smoothie
# scenario (lava + the player spotlight).  Medium is the proposed default.

run_case "modern-light-demo1-d2" \
	--regress-demo=1 --regress-detail=2 --regress-modern --regress-aspect=16:9 \
	--regress-bloom=medium --regress-lighting=medium
pairs=$((pairs + 1))
run_case "modern-light-scenario-spotlight-d3" \
	--regress-level=1:16 --regress-detail=3 --regress-frames=1200 --regress-modern --regress-aspect=16:9 \
	--regress-bloom=medium --regress-lighting=medium
pairs=$((pairs + 1))

# --- game-state hashes --------------------------------------------------------
#
# The state-hash stream covers the RNG, the players, the enemy/shot arrays,
# boss_bar[], tempW and the level event position, and is independent of the
# presentation.  These cases run it through the Modern 16:9 path (where the
# relocated HUD is active) and compare against baselines that were produced by a
# Classic run, so any Modern change that perturbs game state fails here.  The
# state-scenario-spotlight-2p case additionally exercises the lives block of
# JE_inGameDisplays (and therefore its tempW writes) via --regress-players=2.
# See src/regress.c (regress_state_hash).

run_state_case() {
	local label=$1
	shift
	local out="$ACTUAL_DIR/$label.txt"
	local log="$ACTUAL_DIR/$label.log"
	local baseline="$BASELINE_DIR/$label.txt"

	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" --data="$DATA_DIR" --regress-state-out="$out" "$@" \
		>"$log" 2>&1
	rc=$?

	if [ "$rc" -ne 0 ] || [ ! -f "$out" ]; then
		echo "FAIL $label: exit code $rc (no state output)"
		tail -n 5 "$log"
		failures=$((failures + 1))
		return
	fi

	lines=$(wc -l < "$out" | tr -d ' ')

	if [ "$UPDATE" -eq 1 ]; then
		cp "$out" "$baseline"
		echo "UPDATE $label: $lines lines"
		return
	fi

	if [ ! -f "$baseline" ]; then
		echo "FAIL $label: missing baseline (run tools/regress.sh --update)"
		failures=$((failures + 1))
		return
	fi

	if cmp -s "$baseline" "$out"; then
		echo "PASS $label: $lines lines"
	else
		hunk=$(diff "$baseline" "$out" | head -n 1)
		first=${hunk%%[cad]*}
		first=${first%%,*}
		echo "FAIL $label: first differing line $((first - 1))"
		failures=$((failures + 1))
	fi
}

run_state_case "state-demo1-d2" --regress-demo=1 --regress-detail=2 --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))
run_state_case "state-demo3-d2" --regress-demo=3 --regress-detail=2 --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))
run_state_case "state-scenario-spotlight-d3" \
	--regress-level=1:16 --regress-detail=3 --regress-frames=1200 --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))
run_state_case "state-scenario-spotlight-2p-d3" \
	--regress-level=1:16 --regress-detail=3 --regress-frames=1200 --regress-players=2 --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))

# --- offline audio -----------------------------------------------------------

pairs=$((pairs + 1))
run_case "audio" --regress-audio

total=$(awk "BEGIN { printf \"%.1f\", $(now) - $total_start }")

if [ "$UPDATE" -eq 1 ]; then
	echo "Baselines updated in $BASELINE_DIR ($pairs cases, ${total}s total)"
	exit 0
fi

if [ "$failures" -eq 0 ]; then
	echo "All $pairs regression cases passed in ${total}s."
	exit 0
fi

echo "$failures of $pairs regression cases failed in ${total}s."
exit 1
