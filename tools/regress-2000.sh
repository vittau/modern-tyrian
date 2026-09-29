#!/bin/bash
# regress-2000.sh — headless regression harness for the Tyrian 2000 variant.
#
# Separate from tools/regress.sh: its own data manifest, its own baselines
# (test/regress-2000/) and its own cases.  The 2.1 suite neither needs nor
# touches the 2000 data.  Baselines here are hashes of what this engine renders
# and simulates from the installed data: they prove that the 2000 data loads and
# runs deterministically, not that it matches the original game.
#
#   TYRIAN2000_DATA=<dir> tools/regress-2000.sh            build, run all cases, compare
#   TYRIAN2000_DATA=<dir> tools/regress-2000.sh --update   regenerate the baselines
#
# The Tyrian 2000 data directory is explicit.  It is never searched for, never
# fetched and never replaced by ./data (which holds Tyrian 2.1): missing or
# mismatching data is an error, not a skipped suite.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

UPDATE=0
for arg in "$@"; do
	case "$arg" in
		--update) UPDATE=1 ;;
		-h|--help)
			echo "Usage: TYRIAN2000_DATA=<dir> tools/regress-2000.sh [--update]"
			exit 0 ;;
		*) echo "ERROR: unknown option: $arg" >&2; exit 2 ;;
	esac
done

DATA_DIR="${TYRIAN2000_DATA:-}"
BIN="$ROOT/opentyrian"
BASELINE_DIR="$ROOT/test/regress-2000"
ACTUAL_DIR="$BASELINE_DIR/actual"
MANIFEST="$BASELINE_DIR/data-manifest.txt"
MODERN_DETAIL=4

if [ -z "$DATA_DIR" ]; then
	echo "ERROR: TYRIAN2000_DATA is not set."
	echo "Point it at a verified Tyrian 2000 data directory:"
	echo "  make regress-2000 TYRIAN2000_DATA=<directory>"
	echo "This suite never falls back to ./data (Tyrian 2.1) and never downloads data."
	exit 1
fi
if [ ! -d "$DATA_DIR" ]; then
	echo "ERROR: TYRIAN2000_DATA '$DATA_DIR' is not a directory."
	exit 1
fi
DATA_DIR=$(cd "$DATA_DIR" && pwd)

if [ "${REGRESS_SKIP_BUILD:-0}" != "1" ]; then
	echo "Building opentyrian..."
	make -s || { echo "BUILD FAILED"; exit 1; }
fi
[ -x "$BIN" ] || { echo "ERROR: $BIN not found (build failed?)"; exit 1; }

# --- data lock: size and POSIX cksum CRC of every file (metadata only) --------
bad_data=0
while read -r size crc name; do
	size=${size%$'\r'}; crc=${crc%$'\r'}; name=${name%$'\r'}
	case "$size" in ''|'#'*) continue ;; esac
	if [ ! -f "$DATA_DIR/$name" ]; then
		echo "  missing: $name (expected size $size, crc $crc)"
		bad_data=$((bad_data + 1))
		continue
	fi
	# shellcheck disable=SC2046
	set -- $(cksum "$DATA_DIR/$name")
	if [ "$2" != "$size" ] || [ "$1" != "$crc" ]; then
		echo "  mismatched: $name (expected size $size, crc $crc; got size $2, crc $1)"
		bad_data=$((bad_data + 1))
	fi
done < "$MANIFEST"
if [ "$bad_data" -ne 0 ]; then
	echo ""
	echo "ERROR: the data in $DATA_DIR is not the expected Tyrian 2000 set"
	echo "($bad_data file(s) differ).  The baselines are only valid for the exact data"
	echo "they were generated from, so this run is refused."
	exit 1
fi

rm -rf "$ACTUAL_DIR"
mkdir -p "$ACTUAL_DIR"

# The installer cases use synthetic data and stub curl, even in this suite.
if ! "$ROOT/tools/check_installer.sh" "$BIN" "" "$ACTUAL_DIR/installer"; then
	echo "ERROR: Tyrian 2000 installer checks failed"
	exit 1
fi

now() {
	if command -v perl >/dev/null 2>&1; then perl -MTime::HiRes=time -e 'printf "%.3f", time'; else date +%s; fi
}
total_start=$(now)
failures=0
cases=0

describe_status() {
	if [ "$1" -gt 128 ] && [ "$1" -le 192 ]; then echo "CRASH: signal $(($1 - 128))"; else echo "exit code $1"; fi
}

# run_case KIND LABEL args...: KIND is frames (--regress-out) or state (--regress-state-out).
run_case() {
	local kind=$1 label=$2 out log baseline rc start elapsed lines
	shift 2
	cases=$((cases + 1))
	out="$ACTUAL_DIR/$label.txt"
	log="$ACTUAL_DIR/$label.log"
	baseline="$BASELINE_DIR/$label.txt"
	local flag=--regress-out
	[ "$kind" = state ] && flag=--regress-state-out
	start=$(now)
	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" --variant=2000 --data="$DATA_DIR" "$flag=$out" "$@" >"$log" 2>&1
	rc=$?
	elapsed=$(awk "BEGIN { printf \"%.2f\", $(now) - $start }")
	if [ "$rc" -ne 0 ] || [ ! -s "$out" ]; then
		echo "FAIL $label: $(describe_status "$rc") (${elapsed}s)"
		tail -n 12 "$log" | sed 's/^/  /'
		failures=$((failures + 1))
		return
	fi
	if ! grep -Fq 'validation: ok.' "$log"; then
		echo "FAIL $label: the run did not validate the Tyrian 2000 data"
		failures=$((failures + 1))
		return
	fi
	lines=$(wc -l < "$out" | tr -d ' ')
	if [ "$UPDATE" -eq 1 ]; then
		cp "$out" "$baseline"
		echo "UPDATE $label: $lines lines, ${elapsed}s"
	elif [ ! -f "$baseline" ]; then
		echo "FAIL $label: missing baseline (run tools/regress-2000.sh --update)"
		failures=$((failures + 1))
	elif cmp -s "$baseline" "$out"; then
		echo "PASS $label: $lines lines, ${elapsed}s"
	else
		local hunk first
		hunk=$(diff "$baseline" "$out" | head -n 1)
		first=${hunk%%[cad]*}; first=${first%%,*}
		echo "FAIL $label: first differing line $((first - 1)) (${elapsed}s)"
		failures=$((failures + 1))
	fi
}

M="--regress-modern --regress-detail=$MODERN_DETAIL"

# Logic/RNG hashes of the five installed demos (their input recordings are the
# 2.1 ones; the maps and rules are not, so these baselines are the 2000 ones).
for d in 1 2 3 4 5; do
	run_case state "state-demo$d-d$MODERN_DETAIL" --regress-demo="$d" $M --regress-aspect=16:9
done

# Classic and Modern frames of the first demo.
run_case frames "demo1-d$MODERN_DETAIL" --regress-demo=1 --regress-detail="$MODERN_DETAIL"
run_case frames "modern-demo1-d$MODERN_DETAIL" --regress-demo=1 $M

# Direct level starts: episode 1 level 1, and episode 5 (levels 1 and 7; the
# latter carries events whose 2000 rules are Phase 3b and are skipped for now).
run_case frames "e1-level1-d$MODERN_DETAIL" --regress-level=1:1 --regress-detail="$MODERN_DETAIL" --regress-frames=900
run_case frames "e5-level1-d$MODERN_DETAIL" --regress-level=5:1 --regress-detail="$MODERN_DETAIL" --regress-frames=900
run_case frames "e5-level7-d$MODERN_DETAIL" --regress-level=5:7 --regress-detail="$MODERN_DETAIL" --regress-frames=900
run_case frames "modern-e5-level1-d$MODERN_DETAIL" --regress-level=5:1 $M --regress-frames=900 --regress-aspect=16:9
run_case state "state-e5-level1-d$MODERN_DETAIL" --regress-level=5:1 $M --regress-frames=900 --regress-aspect=16:9

# Non-gameplay screens that read the 2000 strings, pictures, palettes, ships and
# the 126-record credits.
run_case frames "screen-title" --regress-screen=title
run_case frames "modern-screen-title-16x9" --regress-screen=title --regress-modern --regress-aspect=16:9
run_case frames "screen-game-menu" --regress-screen=game-menu
run_case frames "screen-ship-specs" --regress-screen=ship-specs
run_case frames "screen-credits" --regress-screen=credits

# Offline audio: 31 effects and nine voices at their 2000 IDs, 41 songs.
run_case frames "audio" --regress-audio

# Save codec against the real HDT: a fresh 2000 save is 4,722 bytes, its score
# boards get their names from the HDT, every difficulty starts at zero, and a
# second start loads it without regenerating anything.
cases=$((cases + 1))
save_root="$ACTUAL_DIR/save-root"
rm -rf "$save_root"; mkdir -p "$save_root" "$ACTUAL_DIR/save-cwd"
save_run() {
	(cd "$ACTUAL_DIR/save-cwd" && HOME="$ACTUAL_DIR/save-cwd" XDG_CONFIG_HOME="$ACTUAL_DIR/save-cwd" APPDATA="$ACTUAL_DIR/save-cwd" \
		SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy "$BIN" --variant=2000 --data="$DATA_DIR" \
		--regress-user-root="$save_root" --regress-user-files --regress-out="$ACTUAL_DIR/save.out") > "$ACTUAL_DIR/$1.log" 2>&1
}
save_ok=1
save_run save-fresh || save_ok=0
save="$save_root/tyrian2000/tyrian.sav"
if [ "$save_ok" -eq 1 ]; then
	[ "$(wc -c < "$save" | tr -d ' ')" -eq 4722 ] || save_ok=0
	[ ! -e "$save_root/tyrian21" ] || save_ok=0
	# Difficulty bytes: last byte of each 35-byte (Timed Battle) and 39-byte entry.
	for i in $(seq 0 29); do
		[ "$(od -An -tu1 -j $((2502 + i * 35 + 34)) -N1 "$save" | tr -d ' ')" = 0 ] || save_ok=0
	done
	for i in $(seq 0 29); do
		[ "$(od -An -tu1 -j $((3552 + i * 39 + 38)) -N1 "$save" | tr -d ' ')" = 0 ] || save_ok=0
	done
	cp "$save" "$ACTUAL_DIR/save-first.sav"
	save_run save-reload || save_ok=0
	cmp -s "$ACTUAL_DIR/save-first.sav" "$save" || save_ok=0
	if grep -Fq "is invalid or missing" "$ACTUAL_DIR/save-reload.log"; then save_ok=0; fi
fi
if [ "$save_ok" -eq 1 ]; then
	echo "PASS save2000-fresh: 4,722 bytes, zero difficulties, exact reload"
else
	echo "FAIL save2000-fresh"
	tail -n 8 "$ACTUAL_DIR/save-fresh.log" | sed 's/^/  /'
	failures=$((failures + 1))
fi

total=$(awk "BEGIN { printf \"%.1f\", $(now) - $total_start }")
if [ "$UPDATE" -eq 1 ]; then
	echo "Baselines updated in $BASELINE_DIR ($cases cases, ${total}s total)"
	exit "$([ "$failures" -eq 0 ] && echo 0 || echo 1)"
fi
if [ "$failures" -eq 0 ]; then
	echo "All $cases regress-2000 cases passed in ${total}s."
	exit 0
fi
echo "$failures of $cases regress-2000 cases failed in ${total}s."
exit 1
