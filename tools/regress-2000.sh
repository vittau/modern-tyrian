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
CASE_FILTER='.*'
LAUNCHER_ONLY=0
UPDATE_CASES=""
for arg in "$@"; do
	case "$arg" in
		--update) UPDATE=1 ;;
		--update-case=*) UPDATE=1; UPDATE_CASES="$UPDATE_CASES ${arg#*=}" ;;
		--case=*) CASE_FILTER=${arg#*=} ;;
		--only-launcher) LAUNCHER_ONLY=1 ;;
		-h|--help)
			echo "Usage: TYRIAN2000_DATA=<dir> tools/regress-2000.sh [--update | --update-case=LABEL ...] [--case=REGEX] [--only-launcher]"
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
	[[ "$label" =~ $CASE_FILTER ]] || return
	if [ -n "$UPDATE_CASES" ]; then
		case " $UPDATE_CASES " in *" $label "*) ;; *) return ;; esac
	fi
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
	if [ "$kind" != launcher ] && ! grep -Fq 'validation: ok.' "$log"; then
		echo "FAIL $label: the run did not validate the Tyrian 2000 data"
		failures=$((failures + 1))
		return
	fi
	lines=$(wc -l < "$out" | tr -d ' ')
	# A hash alone cannot establish that a late level event was reached.
	local coverage=""
	case "$label" in
		state-e4-level5-*) coverage='Rule coverage: event 68 replacement ran.' ;;
		state-e5-level5-*) coverage='Rule coverage: spawn -200 ran.' ;;
		state-e5-level7-events-*) coverage='Rule coverage: event 68 replacement ran.' ;;
		state-e5-level8-*) coverage='Rule coverage: event 58 launch ran.' ;;
		rules-*) coverage="Rule fixture PASS: ${label#rules-}" ;;
	esac
	if [ -n "$coverage" ] && ! grep -Fq "$coverage" "$log"; then
		echo "FAIL $label: required rule did not run ($coverage)"
		failures=$((failures + 1))
		return
	fi
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

# Launcher fixtures are independent of either game's data, save files and clock.
# Run just these with --only-launcher, including when adding/updating their hashes.
for size in 1280x720 1280x800; do
	for data in installed missing; do
		for panel in 1 2; do
			run_case launcher "launcher-$size-$data-p$panel" "--regress-launcher=$size,$data,$panel"
		done
	done
done
run_case launcher launcher-1280x800-about --regress-launcher=1280x800,installed,2,about
run_case launcher launcher-1280x800-message --regress-launcher=1280x800,missing,2,message
# The install flow: the choice dialog (with and without file pickers), a
# mid-download frame at fixed fake progress, the no-curl error, success, and
# the place-the-files screen for a system without a file dialog.
for extra in install install-nodlg progress nocurl success manual; do
	run_case launcher "launcher-1280x800-$extra" "--regress-launcher=1280x800,missing,2,$extra"
done
run_case launcher launcher-1920x1080-install --regress-launcher=1920x1080,missing,2,install
if [ "$LAUNCHER_ONLY" -eq 1 ]; then
	if [ "$failures" -eq 0 ]; then
		echo "All $cases launcher cases passed."
		exit 0
	fi
	echo "$failures of $cases launcher cases failed."
	exit 1
fi

M="--regress-modern --regress-detail=$MODERN_DETAIL"

# Logic/RNG hashes of the five installed demos (their input recordings are the
# 2.1 ones; the maps and rules are not, so these baselines are the 2000 ones).
for d in 1 2 3 4 5; do
	run_case state "state-demo$d-d$MODERN_DETAIL" --regress-demo="$d" $M --regress-aspect=16:9
done

# Classic and Modern frames of the first demo.
run_case frames "demo1-d$MODERN_DETAIL" --regress-demo=1 --regress-detail="$MODERN_DETAIL" --regress-demo-hud-check
run_case frames "modern-demo1-d$MODERN_DETAIL" --regress-demo=1 $M --regress-demo-hud-check

# The attract demos follow the active mode's HUD (--regress-demo-hud-check): the
# Classic runs above keep the original sidebar; these Modern runs must compose
# the side panels, filled, on every in-level frame, at each wide aspect.
run_case frames "modern-demo1-16x9-d$MODERN_DETAIL" --regress-demo=1 $M --regress-aspect=16:9 --regress-demo-hud-check
run_case frames "modern-demo2-21x9-d$MODERN_DETAIL" --regress-demo=2 $M --regress-aspect=21:9 --regress-demo-hud-check
run_case frames "modern-demo3-16x10-d$MODERN_DETAIL" --regress-demo=3 $M --regress-aspect=16:10 --regress-demo-hud-check

# Direct level starts: episode 1 level 1, and episode 5 (levels 1 and 7).
run_case frames "e1-level1-d$MODERN_DETAIL" --regress-level=1:1 --regress-detail="$MODERN_DETAIL" --regress-frames=900
run_case frames "e5-level1-d$MODERN_DETAIL" --regress-level=5:1 --regress-detail="$MODERN_DETAIL" --regress-frames=900
run_case frames "e5-level7-d$MODERN_DETAIL" --regress-level=5:7 --regress-detail="$MODERN_DETAIL" --regress-frames=900
run_case frames "modern-e5-level1-d$MODERN_DETAIL" --regress-level=5:1 $M --regress-frames=900 --regress-aspect=16:9
run_case state "state-e5-level1-d$MODERN_DETAIL" --regress-level=5:1 $M --regress-frames=900 --regress-aspect=16:9

# Wide gameplay with the newest items: the highest ship, a chargeable sidekick and
# the Flying Punch (trail 198, explosion 54), firing and sweeping, with lighting,
# bloom and particles on so the tag buffer and coloured light see the 2000 shots.
# The 1P/2P runs also draw the HUD with the charging sidekick display.
L="--regress-items-new --regress-fire --regress-lighting=high --regress-bloom=high --regress-vfx=high"
run_case frames "modern-lit-e5-level1-21x9-d$MODERN_DETAIL" --regress-level=5:1 --regress-modern $L \
	--regress-frames=700 --regress-aspect=21:9 --regress-gameplay-check
run_case frames "modern-lit-e5-level8-16x9-d$MODERN_DETAIL" --regress-level=5:8 --regress-modern $L \
	--regress-frames=700 --regress-aspect=16:9 --regress-gameplay-check
run_case frames "modern-lit-e4-level5-32x9-d$MODERN_DETAIL" --regress-level=4:5 --regress-modern $L \
	--regress-frames=500 --regress-aspect=32:9
run_case frames "modern-hud-2000items-2p-16x9-d$MODERN_DETAIL" --regress-level=5:1 --regress-modern $L \
	--regress-players=2 --regress-frames=400 --regress-aspect=16:9
run_case frames "modern-hud-2000items-1p-21x9-d$MODERN_DETAIL" --regress-level=1:1 --regress-modern $L \
	--regress-frames=400 --regress-aspect=21:9
run_case frames "e5-level1-2000items-classic-d$MODERN_DETAIL" --regress-level=5:1 --regress-detail="$MODERN_DETAIL" \
	--regress-items-new --regress-fire --regress-frames=400

# Episode 5's ending (script section 19: pictures, text, flash) in Classic and
# in Modern at 16:9 and 21:9.  The frame cap ends the run before the scene waits
# for a key.
run_case frames "script-e5-ending-classic" --regress-script=5:19 --regress-frames=600
for aspect in 16:9 21:9; do
	run_case frames "modern-script-e5-ending-${aspect/:/x}" --regress-script=5:19 --regress-modern \
		--regress-aspect="$aspect" --regress-frames=600
done

# Christmas mode: tyrianc.shp (with the ship bank Tyrian 2000 adds) replaces
# tyrian.shp.  Regress runs pin Christmas off; --regress-xmas turns it on.  Only
# Tyrian 2000 has a case here (this suite has no 2.1 data); the 2.1 baselines
# prove that its default is unchanged.
run_case frames "xmas-e1-level1-d$MODERN_DETAIL" --regress-xmas --regress-level=1:1 --regress-detail="$MODERN_DETAIL" --regress-frames=200
run_case frames "xmas-e5-level1-d$MODERN_DETAIL" --regress-xmas --regress-level=5:1 --regress-detail="$MODERN_DETAIL" --regress-frames=200

# Tyrian 2000 gameplay rules (src/game_rules.c), as logic/RNG hashes over enough
# frames to reach the events.  Which rule each level exercises:
#   5:5  spawn X -200 (random position) and launch types of the second enemy bank
#   5:7  events 58 (set launch), 59 and 68 (replace enemy)
#   5:8  event 58
#   4:5  event 68 as replace enemy (it is random explosions in 2.1)
# These prove the rules run deterministically and did not drift, not that they
# match the DOS game (the events are the fork's approximations).
run_case state "state-e5-level5-d$MODERN_DETAIL" --regress-level=5:5 --regress-detail="$MODERN_DETAIL" --regress-seed=2000 --regress-frames=1500
run_case state "state-e5-level7-events-d$MODERN_DETAIL" --regress-level=5:7 --regress-detail="$MODERN_DETAIL" --regress-frames=7000
run_case state "state-e5-level8-d$MODERN_DETAIL" --regress-level=5:8 --regress-detail="$MODERN_DETAIL" --regress-seed=2000 --regress-frames=1500
run_case state "state-e4-level5-d$MODERN_DETAIL" --regress-level=4:5 --regress-detail="$MODERN_DETAIL" --regress-frames=8000

# Code-owned runtime assertions, followed by real Modern ticks. Replay observes
# the new ships, sidekick charges and Punch/explosion objects without RNG draws.
for fixture in events spawn sidekicks twiddle punch timed; do
	run_case state "rules-$fixture" --regress-level=5:1 $M --regress-frames=240 \
		--regress-aspect=16:9 --regress-replay-check --regress-rules="$fixture"
done

# Non-gameplay screens that read the 2000 strings, pictures, palettes, ships and
# the 126-record credits.  The historic cases keep their names; every screen is
# also rendered Modern at 16:9 and 21:9.
run_case frames "screen-title" --regress-screen=title
run_case frames "modern-screen-title-16x9" --regress-screen=title --regress-modern --regress-aspect=16:9
run_case frames "screen-game-menu" --regress-screen=game-menu
run_case frames "screen-ship-specs" --regress-screen=ship-specs
run_case frames "screen-credits" --regress-screen=credits

# LABEL|SCREEN: the descriptor after ':' selects the fixture (see
# src/regress_screen.c).  Menus 3, 12 and 15 are the options, the limited options
# and the mouse settings menus; ships are selected by new big illustrations 45
# and 46; the weapon simulator shows the 2000 upgrade policy (front "None" keeps
# its power controls, the rear weapon previews its two modes).
SCREENS=(
	"title|title"
	"episode-select|episode-select"
	"gameplay-select|gameplay-select"
	"game-menu|game-menu"
	"options|options"
	"options-mouse-help|options:sel=8"
	"options-limited|options-limited"
	"options-limited-mouse-help|options-limited:sel=4"
	"mouse|mouse"
	"mouse-reset-help|mouse:sel=5"
	"upgrade|upgrade"
	"upgrade-ship45|upgrade:shipgraphic=45"
	"upgrade-ship46|upgrade:shipgraphic=46"
	"upgrade-port45|upgrade:shipgraphic=45,front=45"
	"upgrade-port48|upgrade:shipgraphic=46,front=48"
	"ship-specs|ship-specs"
	"ship-specs-ship45|ship-specs:shipgraphic=45"
	"ship-specs-ship46|ship-specs:shipgraphic=46"
	"weapon-sim-front-none|weapon-sim:cat=3,front=0"
	"weapon-sim-rear-none|weapon-sim:cat=4,rear=0"
	"weapon-sim-done|weapon-sim:cat=3,front=0,sel=3"
	"weapon-sim-rear-modes|weapon-sim:cat=4,twomode=1,mode=2"
	"high-scores-ep1|high-scores"
	"high-scores-ep5|high-scores:page=4"
	"high-scores-battle1|high-scores:page=5"
	"high-scores-battle3|high-scores:page=7"
	"credits|credits"
)
for entry in "${SCREENS[@]}"; do
	label=${entry%%|*}; screen=${entry#*|}
	case "$label" in
		title|game-menu|ship-specs|credits) ;;  # classic case exists above
		*) run_case frames "screen-$label" --regress-screen="$screen" ;;
	esac
	for aspect in 16:9 21:9; do
		name="modern-screen-$label-${aspect/:/x}"
		[ "$name" = "modern-screen-title-16x9" ] && continue
		run_case frames "$name" --regress-screen="$screen" --regress-modern --regress-aspect="$aspect"
	done
done

# More than one preview cycle: cover both the power/cost line and rear-mode hint.
for aspect in classic 16:9 21:9; do
	extra=''
	[ "$aspect" = classic ] || extra="--regress-modern --regress-aspect=$aspect"
	run_case frames "screen-rear-mode-cycle-${aspect/:/x}" --regress-screen=weapon-sim:cat=4,twomode=1,mode=1 \
		--regress-frames=210 $extra
done

# Modern HUD with the widest item names of the data (debug builds assert that
# every HUD row fits its panel), one and two players.
for aspect in 16:9 21:9 32:9; do
	for players in 1 2; do
		run_case frames "modern-hud-widest-${players}p-${aspect/:/x}" --regress-level=1:1 $M \
			--regress-players="$players" --regress-loadout=widest --regress-frames=120 --regress-aspect="$aspect"
	done
done

# --- Flows and the level sweep ------------------------------------------------
# --regress-flow (src/regress_flow.h) plays the real menus with a scripted
# keyboard: Timed Battle from the title to the score board, and a Full Game
# episode from the title to the start of the next one, with every level ended as
# completed after a few ticks.  Each flow must log its "Flow coverage" lines
# (a leading ! means the line must be absent) and its logic-state hashes are
# summarised as "label lines crc" in one baseline per group, so the repository
# holds a few hundred bytes for them instead of a hash per tick.

# aggregate_case NAME FILE: compare (or, when updating, store) an aggregate baseline.
aggregate_case() {
	local name=$1 actual=$2 baseline="$BASELINE_DIR/$1.txt"
	if [ "$UPDATE" -eq 1 ]; then
		cp "$actual" "$baseline"
		echo "UPDATE $name: $(wc -l < "$actual" | tr -d ' ') lines"
	elif [ ! -f "$baseline" ]; then
		echo "FAIL $name: missing baseline (run tools/regress-2000.sh --update-case=$name)"
		failures=$((failures + 1))
	elif cmp -s "$baseline" "$actual"; then
		echo "PASS $name: $(wc -l < "$actual" | tr -d ' ') runs"
	else
		echo "FAIL $name: differs from the baseline"
		diff "$baseline" "$actual" | head -n 6 | sed 's/^/  /'
		failures=$((failures + 1))
	fi
}

# flow_run LABEL AGGREGATE COVERAGE ARGS...: one run; appends "LABEL lines crc" to AGGREGATE.
flow_run() {
	local label=$1 aggregate=$2 coverage=$3 out log rc want
	shift 3
	out="$ACTUAL_DIR/${label//:/-}.state"
	log="$ACTUAL_DIR/${label//:/-}.log"
	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" --variant=2000 --data="$DATA_DIR" --regress-state-out="$out" "$@" >"$log" 2>&1
	rc=$?
	if [ "$rc" -ne 0 ] || [ ! -s "$out" ]; then
		echo "  FAIL $label: $(describe_status "$rc")"
		tail -n 6 "$log" | sed 's/^/    /'
		flow_failed=1
		return
	fi
	while IFS= read -r want; do
		[ -n "$want" ] || continue
		case "$want" in
			'!'*) if grep -Fq "${want#!}" "$log"; then echo "  FAIL $label: unexpected: ${want#!}"; flow_failed=1; fi ;;
			*) if ! grep -Fq "$want" "$log"; then echo "  FAIL $label: missing: $want"; flow_failed=1; fi ;;
		esac
	done <<< "$coverage"
	# shellcheck disable=SC2046
	set -- $(cksum "$out")
	echo "$label $(wc -l < "$out" | tr -d ' ') $1" >> "$aggregate"
	rm -f "$out"
}

case_wanted() {
	[[ "$1" =~ $CASE_FILTER ]] || return 1
	if [ -n "$UPDATE_CASES" ]; then
		case " $UPDATE_CASES " in *" $1 "*) ;; *) return 1 ;; esac
	fi
	return 0
}

if case_wanted flows; then
	cases=$((cases + 1))
	flow_failed=0
	flows_actual="$ACTUAL_DIR/flows.txt"
	: > "$flows_actual"
	start=$(now)
	# Timed Battle: the three battles run to the end of their timers and are
	# ranked on their own boards; one is ended early (time bonus), one is lost.
	for sel in 1 2 3; do
		flow_run "battle$sel" "$flows_actual" "Flow coverage: Timed Battle $sel starts at section
Flow coverage: level timer expired (Timed Battle)
Flow coverage: Timed Battle time bonus 0
Flow coverage: Timed Battle life bonus
Flow coverage: Timed Battle $sel over, cash
Flow coverage: score 51000 on Timed Battle board $((sel - 1)): ranked
Flow coverage: score entered as 'ACE' at rank 1
Flow coverage: the game returned to the title" \
			"--regress-flow=battle:sel=$sel,cash=50000"
	done
	# The Modern presentation must carry the battle timer in its HUD panels.
	flow_run battle1-modern "$flows_actual" "Flow coverage: level timer expired (Timed Battle)
Flow coverage: score entered as 'ACE' at rank 1" \
		"--regress-flow=battle:sel=1,cash=50000" --regress-modern --regress-detail="$MODERN_DETAIL" --regress-aspect=16:9
	flow_run battle-complete "$flows_actual" "Flow coverage: Timed Battle 2 starts at section
Flow coverage: Timed Battle time bonus
!level timer expired
Flow coverage: Timed Battle 2 over, cash
Flow coverage: score entered as 'ACE' at rank 1" \
		"--regress-flow=battle:sel=2,cash=50000,ticks=300"
	flow_run battle-death "$flows_actual" "Flow coverage: Timed Battle 1 starts at section
Flow coverage: player killed after 200 ticks
Flow coverage: Timed Battle game over, back to the title
!Timed Battle 1 over
!score entered" \
		"--regress-flow=battle:sel=1,die=200"
	# Full Game: every episode from the title to the start of the next one.
	for ep in 1 2 3 4 5; do
		next=$((ep % 5 + 1))
		flow_run "episode$ep" "$flows_actual" "Flow coverage: level begins: episode $ep section
Flow coverage: episode $ep end ran
Flow coverage: episode $ep -> episode $next set up
Flow coverage: flow complete" \
			"--regress-flow=episode:ep=$ep"
	done
	elapsed=$(awk "BEGIN { printf \"%.1f\", $(now) - $start }")
	if [ "$flow_failed" -ne 0 ]; then
		echo "FAIL flows: coverage or run failures (${elapsed}s)"
		failures=$((failures + 1))
	else
		aggregate_case flows "$flows_actual"
		echo "  (flows took ${elapsed}s)"
	fi
fi

# Arcade paths, from the secret codes typed at the title screen: the nine arcade
# ships of Tyrian 2000 (the last two are new), Super Tyrian with its choice of
# starting episode, and Destruct (its intro and mode menu, then back out).
if case_wanted arcade-flows; then
	cases=$((cases + 1))
	flow_failed=0
	arcade_actual="$ACTUAL_DIR/arcade-flows.txt"
	: > "$arcade_actual"
	start=$(now)
	ship_items=(3 1 5 10 2 11 12 15 17)   # the ship item each arcade ship flies
	for ship in 1 2 3 4 5 6 7 8 9; do
		flow_run "arcade$ship" "$arcade_actual" "Flow coverage: arcade ship $ship chosen (ship item ${ship_items[$((ship - 1))]}, episode 1)
Flow coverage: level begins: episode 1
Flow coverage: flow complete: level 1 ran (arcade mode $ship, ship ${ship_items[$((ship - 1))]})" \
			"--regress-flow=arcade:ship=$ship"
	done
	for ep in 1 2 3 4 5; do
		flow_run "supertyrian$ep" "$arcade_actual" "Flow coverage: Super Tyrian starts in episode $ep.
Flow coverage: level begins: episode $ep
Flow coverage: flow complete: level 1 ran (arcade mode 0, ship 13)" \
			"--regress-flow=supertyrian:ep=$ep"
	done
	flow_run destruct "$arcade_actual" "Flow coverage: Destruct started from the title.
Flow coverage: script complete" \
		"--regress-flow=destruct"
	elapsed=$(awk "BEGIN { printf \"%.1f\", $(now) - $start }")
	if [ "$flow_failed" -ne 0 ]; then
		echo "FAIL arcade-flows: coverage or run failures (${elapsed}s)"
		failures=$((failures + 1))
	else
		aggregate_case arcade-flows "$arcade_actual"
		echo "  (arcade flows took ${elapsed}s)"
	fi
fi

# Every level of the five episodes, started through the episode script and run
# for a short, fixed number of frames; the list of levels is read from the
# installed episode files (--regress-flow=list-levels) so nothing about them is
# kept here.  Logic-state hashes, so renderer work cannot move them.
if case_wanted level-sweep; then
	cases=$((cases + 1))
	sweep_actual="$ACTUAL_DIR/level-sweep.txt"
	: > "$sweep_actual"
	sweep_failed=0
	start=$(now)
	for ep in 1 2 3 4 5; do
		levels=$(SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy "$BIN" --variant=2000 --data="$DATA_DIR" \
			--regress-flow="list-levels:ep=$ep" --regress-out="$ACTUAL_DIR/levels.tmp" 2>/dev/null | grep -E '^[0-9]+:[0-9]+$')
		[ -n "$levels" ] || { echo "  FAIL level-sweep: no levels listed for episode $ep"; sweep_failed=1; continue; }
		for level in $levels; do
			# Timed Battle 2 of episode 1 is never on the battle menu (the menu sends
			# battle 2 to episode 5) and its level names a shape file that does not
			# exist, so the game cannot start it.
			[ "$level" = 1:44 ] && continue
			flow_failed=0
			flow_run "$level" "$sweep_actual" "" --regress-script="$level" --regress-detail="$MODERN_DETAIL" --regress-seed=2000 --regress-frames=150
			[ "$flow_failed" -eq 0 ] || sweep_failed=1
		done
	done
	elapsed=$(awk "BEGIN { printf \"%.1f\", $(now) - $start }")
	if [ "$sweep_failed" -ne 0 ]; then
		echo "FAIL level-sweep (${elapsed}s)"
		failures=$((failures + 1))
	else
		aggregate_case level-sweep "$sweep_actual"
		echo "  (level sweep took ${elapsed}s, $(wc -l < "$sweep_actual" | tr -d ' ') levels)"
	fi
fi

# Final matrix observers have their own state/frame aggregate and coverage
# assertions. No existing case or baseline is updated when adding this family.
if case_wanted final-regression; then
	cases=$((cases + 1))
	if "$ROOT/tools/check_final_regression.sh" "$BIN" 2000 "$DATA_DIR" "$ACTUAL_DIR/final-regression"; then
		aggregate_case final-regression "$ACTUAL_DIR/final-regression/final-regression.txt"
	else
		failures=$((failures + 1))
	fi
fi

# Offline audio: 31 effects and nine voices at their 2000 IDs, 41 songs.
run_case frames "audio" --regress-audio

# Save codec against the real HDT: a fresh 2000 save is 4,722 bytes, its score
# boards get their names from the HDT, every difficulty starts at zero, and a
# second start loads it without regenerating anything.
cases=$((cases + 1))
# Generated defaults contain HDT names, so these saves also stay outside the
# checkout.  Logs and frame/state hashes in actual/ carry no extracted strings.
save_root=$(mktemp -d "${TMPDIR:-/tmp}/tyrian2000-save.XXXXXX") || exit 1
trap 'rm -rf "$save_root"' EXIT
save_cwd="$save_root/cwd"
mkdir -p "$save_cwd"
save_run() {
	(cd "$save_cwd" && HOME="$save_cwd" XDG_CONFIG_HOME="$save_cwd" APPDATA="$save_cwd" \
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
	cp "$save" "$save_root/first.sav"
	save_run save-reload || save_ok=0
	cmp -s "$save_root/first.sav" "$save" || save_ok=0
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
