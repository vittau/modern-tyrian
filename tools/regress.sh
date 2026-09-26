#!/bin/bash
# regress.sh — headless demo regression harness for OpenTyrian.
#
# Builds the game, replays each recorded demo (demo.1 .. demo.5) headless at
# every processor detail level (1 .. 6), and compares the per-frame 8-bit
# framebuffer hashes against the committed baselines in test/regress/.
#
#   tools/regress.sh              build, run all (demo, level) pairs, compare
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

# --- run ---------------------------------------------------------------------

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

for d in $DEMOS; do
	for m in $LEVELS; do
		pairs=$((pairs + 1))
		name="demo$d-d$m"
		out="$ACTUAL_DIR/$name.txt"
		log="$ACTUAL_DIR/$name.log"
		baseline="$BASELINE_DIR/$name.txt"

		start=$(now)
		SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
			"$BIN" --data="$DATA_DIR" --regress-demo="$d" --regress-detail="$m" --regress-out="$out" \
			>"$log" 2>&1
		rc=$?
		elapsed=$(awk "BEGIN { printf \"%.2f\", $(now) - $start }")

		if [ "$rc" -ne 0 ]; then
			echo "FAIL $name (demo $d, detail $m): exit code $rc (${elapsed}s)"
			tail -n 5 "$log"
			failures=$((failures + 1))
			continue
		fi

		if [ ! -f "$out" ]; then
			echo "FAIL $name (demo $d, detail $m): no output written (${elapsed}s)"
			failures=$((failures + 1))
			continue
		fi

		frames=$(wc -l < "$out" | tr -d ' ')

		if [ "$UPDATE" -eq 1 ]; then
			cp "$out" "$baseline"
			echo "UPDATE $name (demo $d, detail $m): $frames frames, ${elapsed}s"
			continue
		fi

		if [ ! -f "$baseline" ]; then
			echo "FAIL $name (demo $d, detail $m): missing baseline (run tools/regress.sh --update)"
			failures=$((failures + 1))
			continue
		fi

		if cmp -s "$baseline" "$out"; then
			echo "PASS $name (demo $d, detail $m): $frames frames, ${elapsed}s"
		else
			# First differing hunk, e.g. "12c12" or "5,7c5,9"; line N is frame N-1.
			hunk=$(diff "$baseline" "$out" | head -n 1)
			first=${hunk%%[cad]*}
			first=${first%%,*}
			echo "FAIL $name (demo $d, detail $m): first differing frame $((first - 1)) (${elapsed}s)"
			failures=$((failures + 1))
		fi
	done
done

total=$(awk "BEGIN { printf \"%.1f\", $(now) - $total_start }")

if [ "$UPDATE" -eq 1 ]; then
	echo "Baselines updated in $BASELINE_DIR ($pairs pairs, ${total}s total)"
	exit 0
fi

if [ "$failures" -eq 0 ]; then
	echo "All $pairs (demo, detail) pairs passed in ${total}s."
	exit 0
fi

echo "$failures of $pairs (demo, detail) pairs failed in ${total}s."
exit 1
