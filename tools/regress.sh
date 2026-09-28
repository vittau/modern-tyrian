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
UPDATE_MANIFEST=0
REPLAY_CHECK=0
INTERP_CHECK=0
SMOOTH_CHECK=0
PARALLAX_CHECK=0
if [ "${1:-}" = "--update" ]; then
	UPDATE=1
elif [ "${1:-}" = "--update-manifest" ]; then
	UPDATE_MANIFEST=1
elif [ "${1:-}" = "--replay-check" ]; then
	REPLAY_CHECK=1
elif [ "${1:-}" = "--interp-check" ]; then
	INTERP_CHECK=1
elif [ "${1:-}" = "--smoothness-check" ]; then
	SMOOTH_CHECK=1
elif [ "${1:-}" = "--parallax-check" ]; then
	PARALLAX_CHECK=1
fi

DATA_DIR="${TYRIAN_DATA:-$ROOT/data}"
BIN="$ROOT/opentyrian"
BASELINE_DIR="$ROOT/test/regress"
ACTUAL_DIR="$BASELINE_DIR/actual"
MANIFEST="$BASELINE_DIR/data-manifest.txt"
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

# --- source guard ------------------------------------------------------------
#
# C does not sequence two side-effecting calls in one expression (function
# arguments, the operands of +, ...).  Two RNG draws in the same expression
# therefore come out in a compiler-dependent order, which would silently break
# the baselines on another toolchain.  Refuse to run the suite if any such site
# is present.  See tools/check_rng_order.sh and .worker-reports/rng-order.md.
if ! "$ROOT/tools/check_rng_order.sh"; then
	echo ""
	echo "ERROR: an expression draws from the RNG more than once; the order of"
	echo "evaluation is unspecified by C.  Hoist each call into its own statement."
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

# --- data lock ---------------------------------------------------------------
#
# The baselines are only valid for the original freeware Tyrian 2.1 data.  A
# different data directory (e.g. a copy where one sprite was replaced) makes the
# output diverge silently, so refuse it up front.  The expected size and CRC of
# every file the cases read live in test/regress/data-manifest.txt; the CRC is
# the POSIX cksum one, which is available on macOS, Linux and MSYS2.

print_manifest_header() {
	echo "# Data files the regression cases read, with the size and POSIX cksum CRC of"
	echo "# the expected Tyrian 2.1 copy.  tools/regress.sh refuses a data directory"
	echo "# whose files do not match, because the baselines are only valid for the"
	echo "# original freeware data.  Regenerate with: tools/regress.sh --update-manifest"
	echo "#"
	echo "# size crc name"
}

# --update-manifest re-hashes the files already listed in the manifest, so it
# does not add or remove entries; edit the manifest for that.
if [ "$UPDATE_MANIFEST" -eq 1 ]; then
	if [ ! -f "$MANIFEST" ]; then
		echo "ERROR: no data manifest to update ($MANIFEST)"
		exit 1
	fi

	tmp="$MANIFEST.tmp"
	print_manifest_header > "$tmp"
	while read -r size crc name; do
		# A checkout with CRLF (e.g. Windows git autocrlf) leaves a stray \r
		# on the name; .gitattributes forces LF, but stay robust anyway.
		size=${size%$'\r'}
		crc=${crc%$'\r'}
		name=${name%$'\r'}
		case "$size" in ''|'#'*) continue ;; esac
		if [ ! -f "$DATA_DIR/$name" ]; then
			echo "missing: $name" >&2
			continue
		fi
		set -- $(cksum "$DATA_DIR/$name")
		printf '%s %s %s\n' "$2" "$1" "$name" >> "$tmp"
	done < "$MANIFEST"
	mv "$tmp" "$MANIFEST"
	echo "Data manifest updated from $DATA_DIR."
	exit 0
fi

if [ ! -f "$MANIFEST" ]; then
	echo "ERROR: missing data manifest $MANIFEST"
	exit 1
fi

bad_data=0
while read -r size crc name; do
	# See the note in the --update-manifest loop: strip a stray \r so the lock
	# still works if a CRLF checkout slips through on Windows.
	size=${size%$'\r'}
	crc=${crc%$'\r'}
	name=${name%$'\r'}
	case "$size" in ''|'#'*) continue ;; esac

	if [ ! -f "$DATA_DIR/$name" ]; then
		echo "  missing: $name (expected size $size, crc $crc)"
		bad_data=$((bad_data + 1))
		continue
	fi

	set -- $(cksum "$DATA_DIR/$name")
	got_crc=$1
	got_size=$2

	if [ "$got_size" != "$size" ] || [ "$got_crc" != "$crc" ]; then
		echo "  mismatched: $name (expected size $size, crc $crc; got size $got_size, crc $got_crc)"
		bad_data=$((bad_data + 1))
	fi
done < "$MANIFEST"

if [ "$bad_data" -ne 0 ]; then
	echo ""
	echo "ERROR: the Tyrian data in $DATA_DIR is not the expected Tyrian 2.1 set"
	echo "($bad_data file(s) differ).  The regression baselines are only valid for"
	echo "the exact freeware data they were generated from, so this run is refused"
	echo "to avoid a false divergence.  Point TYRIAN_DATA at that copy (a patched,"
	echo "repacked or differently-sourced release can change a sprite silently)."
	exit 1
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

# Describe a process exit status.  The shell reports a process killed by a
# signal as 128 + N, so a crash is distinguished from a plain non-zero exit.
exit_status_description() {
	local rc=$1
	if [ "$rc" -le 128 ]; then
		echo "exit code $rc"
	elif [ "$rc" -le 192 ]; then
		# 129..192 is 128 + a POSIX signal number, which is how a shell
		# reports a process killed by a signal.
		local sig=$((rc - 128))
		local name
		name=$(kill -l "$sig" 2>/dev/null)
		echo "CRASH: killed by signal $sig${name:+ ($name)}"
	else
		# Windows/other abnormal exit status (e.g. an NTSTATUS like 0xC0000005).
		printf 'CRASH: abnormal exit status %s (0x%X)' "$rc" "$rc"
	fi
}

# Print the captured stdout+stderr of a failed case so a crash, a sanitizer
# report or an engine error is visible in the harness output.  Any sanitizer /
# crash marker lines are shown first, then the tail of the log.
dump_failure_log() {
	local log=$1
	[ -f "$log" ] || return 0

	echo "  --- output ($log) ---"
	if grep -qE 'AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:|SUMMARY:|Fatal|assert' "$log" 2>/dev/null; then
		grep -nE 'AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:|SUMMARY:|Fatal|assert' "$log" | tail -n 20 | sed 's/^/  /'
	fi
	tail -n 20 "$log" | sed 's/^/  /'
	echo "  --- end output ---"
}

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
		echo "FAIL $label: $(exit_status_description "$rc") (${elapsed}s)"
		dump_failure_log "$log"
		failures=$((failures + 1))
		return
	fi

	if [ ! -f "$out" ]; then
		echo "FAIL $label: no output written (${elapsed}s)"
		dump_failure_log "$log"
		failures=$((failures + 1))
		return
	fi

	# An empty output file with a zero exit status is an early exit that never
	# presented a frame -- the "first differing line 0" signature in CI.  Call
	# it out instead of comparing it as an ordinary hash divergence.
	if [ ! -s "$out" ]; then
		echo "FAIL $label: empty output file - the run exited before presenting a frame (${elapsed}s)"
		dump_failure_log "$log"
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
		# A diff can also hide an engine error or a sanitizer report on stderr;
		# keep it in the failure output.
		dump_failure_log "$log"
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

# Script-driven death: E1:L3 has no item screen before the level, so
# --regress-script reaches real gameplay (playDemo == false) and the
# un-invincible player sits still and dies, presenting the "GAME OVER" banner
# over the playfield.  --regress-seed makes that run reproducible (the script
# path otherwise inherits main()'s time(NULL) seed).  This guards the Modern
# centring of the banner on the playfield, which the demo/scenario paths cannot
# reach (they turn the !playDemo death into an end-of-level).
run_case "modern-wide-script-death-d2" \
	--regress-script=1:3 --regress-seed=32402394 --regress-detail=2 \
	--regress-frames=3700 --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))
# Same run with the ESC in-game menu opened on the last frame; owns the baseline
# the gameplay-composition menu case below is compared against.
run_case "modern-wide-script-menu-d2" \
	--regress-script=1:3 --regress-seed=32402394 --regress-detail=2 \
	--regress-frames=700 --regress-menu=ingame --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))

# --- Modern VFX (Fase 2) ------------------------------------------------------
#
# VFX are pinned OFF for every other case (so no pre-VFX baseline changes); these
# two opt into a level with the Modern 16:9 canvas and hash the composited
# particles.  One is a busy demo (the demo2 boss fight, many explosions and
# shots) at high, one a ground/scroll synthetic scenario (E4:L12) at low, so both
# new levels have a baseline.  Both must be reproducible run to run.  See
# src/vfx.c.

run_case "vfx-demo2-d2" --regress-demo=2 --regress-detail=2 --regress-modern --regress-aspect=16:9 --regress-vfx=high
pairs=$((pairs + 1))
run_case "vfx-scenario-flip-d3" \
	--regress-level=4:12 --regress-detail=3 --regress-frames=3600 --regress-modern --regress-aspect=16:9 --regress-vfx=low
pairs=$((pairs + 1))

# --- non-gameplay screens -----------------------------------------------------
#
# --regress-screen renders one non-gameplay screen in a deterministic state and
# exits after its frame cap (REGRESS_SCREEN_FRAMES).  The Modern 16:9 canvas
# guards the widescreen compositions added in step S1: Vert- for the title and
# the pic-2 menus (elements sharp at 1x), the widened pic-1 right panel (the
# extra columns inserted past the rightmost element), and the solid edge fill.
# The Classic runs of the same screens prove the harness itself does not alter
# the 8-bit screens.  The 21:9 cases guard the widest compositions.

SCREENS=(
	title episode-select high-scores game-menu upgrade purchase options
	cube-list cube-reader keyboard joystick load-save solid setup
)

for s in "${SCREENS[@]}"; do
	pairs=$((pairs + 1))
	run_case "modern-screen-$s-16x9" \
		--regress-screen="$s" --regress-modern --regress-aspect=16:9
	pairs=$((pairs + 1))
	run_case "screen-$s" --regress-screen="$s"
done

pairs=$((pairs + 1))
run_case "modern-screen-title-21x9" \
	--regress-screen=title --regress-modern --regress-aspect=21:9
pairs=$((pairs + 1))
run_case "modern-screen-game-menu-21x9" \
	--regress-screen=game-menu --regress-modern --regress-aspect=21:9

# Step S2: procedural screens whose content is extended across the canvas by
# code (starfield, grid, star map, credits).  The Modern 16:9 canvas guards the
# extension, the Classic run proves the 8-bit screen is untouched, and the 21:9
# cases guard the widest compositions (nav map and jukebox, the two the plan
# calls out).
S2_SCREENS=( nav-map ship-specs jukebox weapon-sim credits )

for s in "${S2_SCREENS[@]}"; do
	pairs=$((pairs + 1))
	run_case "modern-screen-$s-16x9" \
		--regress-screen="$s" --regress-modern --regress-aspect=16:9
	pairs=$((pairs + 1))
	run_case "screen-$s" --regress-screen="$s"
done

pairs=$((pairs + 1))
run_case "modern-screen-nav-map-21x9" \
	--regress-screen=nav-map --regress-modern --regress-aspect=21:9
pairs=$((pairs + 1))
run_case "modern-screen-jukebox-21x9" \
	--regress-screen=jukebox --regress-modern --regress-aspect=21:9

# --- Load / Save / Quit (S3) --------------------------------------------------
#
# The in-game Load screen is the "load-save" case above.  These cover the
# screens the user reported: the in-game Save (same pic-1 layout), the title
# Load Game (pic 2), and the quit confirmation dialog (over the pic-1 menu).
# The quit 21:9 case guards the capped Modern centring shift, and the ship-specs
# 21:9 case guards the full-width grid.

pairs=$((pairs + 1))
run_case "modern-screen-save-16x9" \
	--regress-screen=save --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))
run_case "screen-save" --regress-screen=save
pairs=$((pairs + 1))
run_case "modern-screen-save-21x9" \
	--regress-screen=save --regress-modern --regress-aspect=21:9

pairs=$((pairs + 1))
run_case "modern-screen-load-16x9" \
	--regress-screen=load --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))
run_case "screen-load" --regress-screen=load

pairs=$((pairs + 1))
run_case "modern-screen-quit-16x9" \
	--regress-screen=quit --regress-modern --regress-aspect=16:9
pairs=$((pairs + 1))
run_case "screen-quit" --regress-screen=quit
pairs=$((pairs + 1))
run_case "modern-screen-quit-21x9" \
	--regress-screen=quit --regress-modern --regress-aspect=21:9

pairs=$((pairs + 1))
run_case "modern-screen-ship-specs-21x9" \
	--regress-screen=ship-specs --regress-modern --regress-aspect=21:9

# --- modern bloom + dynamic lighting -----------------------------------------
#
# The bloom/dynamic-light pass is pinned OFF unless --regress-lighting opts in,
# so every case above is unaffected.  These two cases cover the effects on the
# 16:9 playfield: a demo with shots and explosions, and a smoothie scenario
# (lava + the player spotlight).  They use "high", which is the pre-merge "low"
# tuning (the strongest level now, after the user found even that too strong
# and asked for the weaker "low" to become the default).

run_case "modern-light-demo1-d2" \
	--regress-demo=1 --regress-detail=2 --regress-modern --regress-aspect=16:9 \
	--regress-lighting=high
pairs=$((pairs + 1))
run_case "modern-light-scenario-spotlight-d3" \
	--regress-level=1:16 --regress-detail=3 --regress-frames=1200 --regress-modern --regress-aspect=16:9 \
	--regress-lighting=high
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

	if [ "$rc" -ne 0 ] || [ ! -s "$out" ]; then
		echo "FAIL $label: $(exit_status_description "$rc") (no state output)"
		dump_failure_log "$log"
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
		dump_failure_log "$log"
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

# --- draw-list replay check (Fase 2, stages 1-2) -----------------------------
#
# Runs the same case with the level draw list recorded every tick and replayed
# into a scratch surface; the run must reproduce every level frame byte for byte
# and, because recording only observes, its frame-hash stream must equal the
# Classic baseline it is compared against.  Representative cases run in every
# normal pass; the full sweep is `tools/regress.sh --replay-check`.

run_replay_case() {
	local label=$1 baseline_label=$2
	shift 2
	local out="$ACTUAL_DIR/$label.txt"
	local log="$ACTUAL_DIR/$label.log"
	local baseline="$BASELINE_DIR/$baseline_label.txt"

	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" --data="$DATA_DIR" --regress-out="$out" --regress-replay-check "$@" \
		>"$log" 2>&1
	rc=$?

	if [ "$rc" -ne 0 ] || [ ! -s "$out" ]; then
		echo "FAIL $label: replay check failed ($(exit_status_description "$rc"))"
		grep -E "Replay check|mismatch" "$log" | tail -n 3
		dump_failure_log "$log"
		failures=$((failures + 1))
		return
	fi

	if [ ! -f "$baseline" ]; then
		echo "FAIL $label: missing baseline $baseline_label"
		failures=$((failures + 1))
		return
	fi

	if cmp -s "$baseline" "$out"; then
		echo "PASS $label: $(wc -l < "$out" | tr -d ' ') lines, replay identical"
	else
		echo "FAIL $label: frame hashes differ from $baseline_label"
		dump_failure_log "$log"
		failures=$((failures + 1))
	fi
}

# run_interp_case LABEL BASELINE_LABEL "$@" -- interpolate-check one case.
# The stage-3 renderer must reproduce the real game_screen at alpha=1 (the run
# exits non-zero on any mismatch) and, because recording only observes, the
# frame-hash stream must still equal the Classic baseline.
run_interp_case() {
	local label=$1 baseline_label=$2
	shift 2
	local out="$ACTUAL_DIR/$label.txt"
	local log="$ACTUAL_DIR/$label.log"
	local baseline="$BASELINE_DIR/$baseline_label.txt"

	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" --data="$DATA_DIR" --regress-out="$out" --regress-interp-check "$@" \
		>"$log" 2>&1
	rc=$?

	if [ "$rc" -ne 0 ] || [ ! -s "$out" ]; then
		echo "FAIL $label: interpolation check failed ($(exit_status_description "$rc"))"
		grep -E "Interp check|mismatch" "$log" | tail -n 3
		dump_failure_log "$log"
		failures=$((failures + 1))
		return
	fi

	if [ ! -f "$baseline" ]; then
		echo "FAIL $label: missing baseline $baseline_label"
		failures=$((failures + 1))
		return
	fi

	if cmp -s "$baseline" "$out"; then
		echo "PASS $label: $(wc -l < "$out" | tr -d ' ') lines, interp alpha=1 identical"
	else
		echo "FAIL $label: frame hashes differ from $baseline_label"
		dump_failure_log "$log"
		failures=$((failures + 1))
	fi
}

# run_gameplay_case LABEL BASELINE_LABEL "$@" -- assert every presented in-level
# Modern frame uses the gameplay composition (drops the classic sidebar).  The
# run also emits the Modern canvas hash stream, which must still equal the
# Modern baseline for the same case.
run_gameplay_case() {
	local label=$1 baseline_label=$2
	shift 2
	local out="$ACTUAL_DIR/$label.txt"
	local log="$ACTUAL_DIR/$label.log"
	local baseline="$BASELINE_DIR/$baseline_label.txt"

	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" --data="$DATA_DIR" --regress-out="$out" --regress-gameplay-check "$@" \
		>"$log" 2>&1
	rc=$?

	if [ "$rc" -ne 0 ] || [ ! -s "$out" ]; then
		echo "FAIL $label: gameplay composition check failed ($(exit_status_description "$rc"))"
		grep -E "Gameplay composition|FAILED" "$log" | tail -n 3
		dump_failure_log "$log"
		failures=$((failures + 1))
		return
	fi

	if [ ! -f "$baseline" ]; then
		echo "FAIL $label: missing baseline $baseline_label"
		failures=$((failures + 1))
		return
	fi

	if cmp -s "$baseline" "$out"; then
		echo "PASS $label: $(wc -l < "$out" | tr -d ' ') lines, sidebar dropped"
	else
		echo "FAIL $label: frame hashes differ from $baseline_label"
		dump_failure_log "$log"
		failures=$((failures + 1))
	fi
}

# run_smoothness_case LABEL BASELINE_LABEL "$@" -- per tick, check that every
# background layer and matched object moves monotonically across the sub-frames
# (the run exits non-zero on any non-monotonic motion) and that the frame-hash
# stream still equals the Classic baseline.
run_smoothness_case() {
	local label=$1 baseline_label=$2
	shift 2
	local out="$ACTUAL_DIR/$label.txt"
	local log="$ACTUAL_DIR/$label.log"
	local baseline="$BASELINE_DIR/$baseline_label.txt"

	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" --data="$DATA_DIR" --regress-out="$out" --regress-interp-smoothness "$@" \
		>"$log" 2>&1
	rc=$?

	if [ "$rc" -ne 0 ] || [ ! -s "$out" ]; then
		echo "FAIL $label: smoothness check failed ($(exit_status_description "$rc"))"
		grep -E "Smoothness|FAILED" "$log" | tail -n 3
		dump_failure_log "$log"
		failures=$((failures + 1))
		return
	fi

	if [ ! -f "$baseline" ]; then
		echo "FAIL $label: missing baseline $baseline_label"
		failures=$((failures + 1))
		return
	fi

	if cmp -s "$baseline" "$out"; then
		echo "PASS $label: $(wc -l < "$out" | tr -d ' ') lines, motion monotonic"
	else
		echo "FAIL $label: frame hashes differ from $baseline_label"
		dump_failure_log "$log"
		failures=$((failures + 1))
	fi
}

# run_parallax_case LABEL BASELINE_LABEL "$@" -- assert the interpolated
# presentation never advances the starfield or the background scroll, so the
# per-tick motion is the same with smooth motion on or off.  The run also emits
# the frame-hash stream, which must still equal the Classic baseline.
run_parallax_case() {
	local label=$1 baseline_label=$2
	shift 2
	local out="$ACTUAL_DIR/$label.txt"
	local log="$ACTUAL_DIR/$label.log"
	local baseline="$BASELINE_DIR/$baseline_label.txt"

	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" --data="$DATA_DIR" --regress-out="$out" --regress-parallax-check "$@" \
		>"$log" 2>&1
	rc=$?

	if [ "$rc" -ne 0 ] || [ ! -f "$out" ]; then
		echo "FAIL $label: parallax check failed (exit $rc)"
		grep -E "Parallax|FAILED" "$log" | tail -n 3
		failures=$((failures + 1))
		return
	fi

	if [ ! -f "$baseline" ]; then
		echo "FAIL $label: missing baseline $baseline_label"
		failures=$((failures + 1))
		return
	fi

	if cmp -s "$baseline" "$out"; then
		echo "PASS $label: $(wc -l < "$out" | tr -d ' ') lines, per-tick motion preserved"
	else
		echo "FAIL $label: frame hashes differ from $baseline_label"
		failures=$((failures + 1))
	fi
}

# run_parallax_level LABEL FRAMES EPISODE:LEVEL -- the reported ASTEROID levels
# have no frame baseline, so this only asserts the check's exit code and prints
# its summary.
run_parallax_level() {
	local label=$1 frames=$2 lvl=$3
	local out="$ACTUAL_DIR/$label.txt"
	local log="$ACTUAL_DIR/$label.log"

	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" --data="$DATA_DIR" --regress-out="$out" --regress-parallax-check \
		--regress-level="$lvl" --regress-frames="$frames" \
		>"$log" 2>&1
	rc=$?

	if [ "$rc" -ne 0 ]; then
		echo "FAIL $label: parallax check failed (exit $rc)"
		grep -E "Parallax|FAILED" "$log" | tail -n 3
		failures=$((failures + 1))
		return
	fi

	echo "PASS $label: $(grep -oE 'Parallax check: .*' "$log" | tail -n 1)"
}

if [ "$UPDATE" -eq 0 ] && [ "$REPLAY_CHECK" -eq 0 ] && [ "$INTERP_CHECK" -eq 0 ] && [ "$SMOOTH_CHECK" -eq 0 ] && [ "$PARALLAX_CHECK" -eq 0 ]; then
	run_replay_case "replay-demo1-d2"   "demo1-d2"   --regress-demo=1 --regress-detail=2
	pairs=$((pairs + 1))
	run_replay_case "replay-demo3-d4"   "demo3-d4"   --regress-demo=3 --regress-detail=4
	pairs=$((pairs + 1))
	run_replay_case "replay-scenario-flip-d3" "scenario-flip-d3" \
		--regress-level=4:12 --regress-detail=3 --regress-frames=3600
	pairs=$((pairs + 1))
	run_replay_case "replay-scenario-iced-d2" "scenario-iced-d2" \
		--regress-level=4:8 --regress-detail=2 --regress-frames=1200
	pairs=$((pairs + 1))

	# --- interpolation identity check (Fase 2, stage 3) -------------------------
	#
	# Render every level tick's interpolated frame at alpha=1 with the persistent
	# stage-3 renderer and compare it byte for byte with the real game_screen.
	run_interp_case "interp-demo1-d2"   "demo1-d2"   --regress-demo=1 --regress-detail=2
	pairs=$((pairs + 1))
	run_interp_case "interp-demo3-d4"   "demo3-d4"   --regress-demo=3 --regress-detail=4
	pairs=$((pairs + 1))
	run_interp_case "interp-scenario-flip-d3" "scenario-flip-d3" \
		--regress-level=4:12 --regress-detail=3 --regress-frames=3600
	pairs=$((pairs + 1))
	run_interp_case "interp-scenario-iced-d2" "scenario-iced-d2" \
		--regress-level=4:8 --regress-detail=2 --regress-frames=1200
	pairs=$((pairs + 1))

	# --- smoothness check (Fase 2, stage 3) -------------------------------------
	#
	# Per level tick, re-derive the sub-frame positions and require every
	# background layer (including the ship-following pan that used to wrap) and
	# every matched object to move monotonically between the two ticks.
	run_smoothness_case "smoothness-demo1-d2" "demo1-d2" \
		--regress-demo=1 --regress-detail=2
	pairs=$((pairs + 1))
	run_smoothness_case "smoothness-scenario-flip-d3" "scenario-flip-d3" \
		--regress-level=4:12 --regress-detail=3 --regress-frames=3600
	pairs=$((pairs + 1))

	# --- parallax guard (starfield/background per-tick motion) ------------------
	#
	# The interpolated presentation must not advance the starfield or the
	# background scroll; the guard exits non-zero if it does, and the frame-hash
	# stream must still equal the Classic baseline.  The two ASTEROID levels are
	# the reported case and have no baseline, so they are check-only.
	run_parallax_case "parallax-demo1-d2" "demo1-d2" --regress-demo=1 --regress-detail=2
	pairs=$((pairs + 1))
	run_parallax_level "parallax-scenario-asteroid" 1200 "1:1"
	pairs=$((pairs + 1))
	run_parallax_level "parallax-scenario-asteroid2" 1200 "1:2"
	pairs=$((pairs + 1))

	# --- gameplay composition check (Fase 2, bug B) -----------------------------
	#
	# Assert that no presented in-level Modern frame falls back to the full
	# 320x200 composition (the classic sidebar); the Modern canvas hash stream
	# must still equal the modern-wide baseline, i.e. the check only observes.
	run_gameplay_case "gameplay-wide-scenario-spotlight-d3" "modern-wide-scenario-spotlight-d3" \
		--regress-level=1:16 --regress-detail=3 --regress-frames=1200 --regress-modern --regress-aspect=16:9
	pairs=$((pairs + 1))

	# In-game menu composition (pause-hud): --regress-menu opens the ESC in-game
	# menu on the run's last presented frame, so the gameplay-composition check
	# sees it presented over the Modern playfield + HUD panels instead of the
	# classic full-frame composition.  Same pinned E1:L3 script/seed as the death
	# case (real gameplay, playDemo == false); the menu replaces the final frame.
	# This fails if the pause/menu stops holding the gameplay composition.
	run_gameplay_case "gameplay-wide-script-menu-d2" "modern-wide-script-menu-d2" \
		--regress-script=1:3 --regress-seed=32402394 --regress-detail=2 \
		--regress-frames=700 --regress-menu=ingame --regress-modern --regress-aspect=16:9
	pairs=$((pairs + 1))
fi

# Full interpolation identity sweep: every demo and scenario at every detail
# level, comparing the alpha=1 interpolated frame with the real one.
# (tools/regress.sh --interp-check)
if [ "$INTERP_CHECK" -eq 1 ]; then
	for d in $DEMOS; do
		for m in $LEVELS; do
			run_interp_case "interp-demo$d-d$m" "demo$d-d$m" --regress-demo="$d" --regress-detail="$m"
		done
	done

	for spec in "${SCENARIOS[@]}"; do
		set -- $spec
		sname=$1
		slvl=$2
		sframes=$3
		shift 3
		for m in "$@"; do
			run_interp_case "interp-scenario-$sname-d$m" "scenario-$sname-d$m" \
				--regress-level="$slvl" --regress-detail="$m" --regress-frames="$sframes"
		done
	done

	total=$(awk "BEGIN { printf \"%.1f\", $(now) - $total_start }")
	if [ "$failures" -eq 0 ]; then
		echo "All interpolation-check cases passed in ${total}s."
		exit 0
	fi
	echo "$failures interpolation-check cases failed in ${total}s."
	exit 1
fi

# Full smoothness sweep: every demo and scenario at every detail level, checking
# the sub-frame motion of every background layer and matched object.
# (tools/regress.sh --smoothness-check)
if [ "$SMOOTH_CHECK" -eq 1 ]; then
	for d in $DEMOS; do
		for m in $LEVELS; do
			run_smoothness_case "smoothness-demo$d-d$m" "demo$d-d$m" --regress-demo="$d" --regress-detail="$m"
		done
	done

	for spec in "${SCENARIOS[@]}"; do
		set -- $spec
		sname=$1
		slvl=$2
		sframes=$3
		shift 3
		for m in "$@"; do
			run_smoothness_case "smoothness-scenario-$sname-d$m" "scenario-$sname-d$m" \
				--regress-level="$slvl" --regress-detail="$m" --regress-frames="$sframes"
		done
	done

	total=$(awk "BEGIN { printf \"%.1f\", $(now) - $total_start }")
	if [ "$failures" -eq 0 ]; then
		echo "All smoothness-check cases passed in ${total}s."
		exit 0
	fi
	echo "$failures smoothness-check cases failed in ${total}s."
	exit 1
fi

# Full parallax sweep: every demo and scenario at every detail level, requiring
# the interpolated presentation to leave the starfield/background scroll
# untouched (tools/regress.sh --parallax-check).
if [ "$PARALLAX_CHECK" -eq 1 ]; then
	for d in $DEMOS; do
		for m in $LEVELS; do
			run_parallax_case "parallax-demo$d-d$m" "demo$d-d$m" --regress-demo="$d" --regress-detail="$m"
		done
	done

	for spec in "${SCENARIOS[@]}"; do
		set -- $spec
		sname=$1
		slvl=$2
		sframes=$3
		shift 3
		for m in "$@"; do
			run_parallax_case "parallax-scenario-$sname-d$m" "scenario-$sname-d$m" \
				--regress-level="$slvl" --regress-detail="$m" --regress-frames="$sframes"
		done
	done

	# The reported ASTEROID levels (check-only: no baseline).
	run_parallax_level "parallax-scenario-asteroid" 1200 "1:1"
	run_parallax_level "parallax-scenario-asteroid2" 1200 "1:2"

	total=$(awk "BEGIN { printf \"%.1f\", $(now) - $total_start }")
	if [ "$failures" -eq 0 ]; then
		echo "All parallax-check cases passed in ${total}s."
		exit 0
	fi
	echo "$failures parallax-check cases failed in ${total}s."
	exit 1
fi

# Full replay sweep: every demo and scenario at every detail level.  This is the
# complete stages 1-2 proof (tools/regress.sh --replay-check).
if [ "$REPLAY_CHECK" -eq 1 ]; then
	for d in $DEMOS; do
		for m in $LEVELS; do
			run_replay_case "replay-demo$d-d$m" "demo$d-d$m" --regress-demo="$d" --regress-detail="$m"
		done
	done

	for spec in "${SCENARIOS[@]}"; do
		set -- $spec
		sname=$1
		slvl=$2
		sframes=$3
		shift 3
		for m in "$@"; do
			run_replay_case "replay-scenario-$sname-d$m" "scenario-$sname-d$m" \
				--regress-level="$slvl" --regress-detail="$m" --regress-frames="$sframes"
		done
	done

	total=$(awk "BEGIN { printf \"%.1f\", $(now) - $total_start }")
	if [ "$failures" -eq 0 ]; then
		echo "All replay-check cases passed in ${total}s."
		exit 0
	fi
	echo "$failures replay-check cases failed in ${total}s."
	exit 1
fi

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
