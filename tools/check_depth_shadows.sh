#!/bin/bash
# check_depth_shadows.sh -- one case of the depth-shadow (stage 2) proof, shared by
# tools/regress.sh (Tyrian 2.1) and tools/regress-2000.sh (Tyrian 2000).
#
#   check_depth_shadows.sh BIN VARIANT DATA_DIR OUT_DIR LABEL OFF_BASELINE SHADOW_BASELINE LEVEL REQUIRE -- ARGS...
#
# VARIANT          2.1 or 2000.
# OFF_BASELINE     the committed frame-hash file the Depth-Off run must reproduce (the
#                  existing baseline of the same scenario), or "-" for none.
# SHADOW_BASELINE  the committed frame-hash file of the LEVEL run (new baselines of the
#                  depth cases), written instead of compared when DEPTH_UPDATE=1.
# LEVEL            on: the setting the baseline belongs to.
# REQUIRE          space-separated assertions over the "Depth shadows:" log line of the
#                  LEVEL run, KEY>N or KEY=N (frames, shadowed_px, bg2, ground, sky,
#                  player, sidekick, bg3, top, blend, space_frames).  Stage 3 keys are
#                  prefixed with their line: fog.KEY (frames, fogged_px, blend_fogged_px,
#                  space_frames) over "Depth fog:", light.KEY (frames, lit_px, reduced_px,
#                  bg1, bg2, ground, sky, top, player, sidekick, bg3) over "Depth light:"
#                  (the light line needs --regress-lighting in ARGS).  A hash alone cannot
#                  show that a path ran.  The word "identical" says the LEVEL run must
#                  present the very same frames as Off (the space levels: no shadow).
# ARGS             the scenario, e.g. --regress-demo=1 --regress-modern --regress-detail=4.
#
# The scenario runs twice: Depth off and on.  The state/RNG hash streams
# must be identical across both (the shadows are display-only), Off must match its
# existing baseline, the LEVEL run must match its own baseline, and the frames of the
# On setting must differ from Off unless the case is "identical".
#
# Only counts and hashes are read from the logs, so no Tyrian 2000 content is ever
# copied out of a run.
set -uo pipefail

if [ "$#" -lt 11 ] || [ "$9" = "--" ]; then
	echo "usage: $0 BIN VARIANT DATA_DIR OUT_DIR LABEL OFF_BASELINE SHADOW_BASELINE LEVEL REQUIRE -- ARGS..." >&2
	exit 2
fi

BIN=$1 VARIANT=$2 DATA_DIR=$3 OUT_DIR=$4 LABEL=$5 OFF_BASE=$6 SHADOW_BASE=$7 LEVEL=$8 REQUIRE=$9
shift 9
[ "$1" = "--" ] && shift

mkdir -p "$OUT_DIR"
variant_args=()
[ "$VARIANT" = "2000" ] && variant_args=(--variant=2000)

log="$OUT_DIR/$LABEL.$LEVEL.log"
fail() {
	echo "FAIL $LABEL: $1"
	[ -f "$log" ] && grep -E 'Depth |FAIL|rror' "$log" | tail -n 5 | sed 's/^/  /'
	exit 1
}

for setting in off on; do
	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" ${variant_args[@]+"${variant_args[@]}"} --data="$DATA_DIR" "$@" --regress-depth="$setting" \
		--regress-out="$OUT_DIR/$LABEL.$setting.txt" --regress-state-out="$OUT_DIR/$LABEL.$setting.state" \
		>"$OUT_DIR/$LABEL.$setting.log" 2>&1
	rc=$?
	[ "$rc" -eq 0 ] || { log="$OUT_DIR/$LABEL.$setting.log"; fail "the $setting run exited with $rc"; }
	[ -s "$OUT_DIR/$LABEL.$setting.txt" ] && [ -s "$OUT_DIR/$LABEL.$setting.state" ] || fail "empty output ($setting)"
done
log="$OUT_DIR/$LABEL.$LEVEL.log"

if [ "$VARIANT" = "2000" ] && ! grep -Fq 'validation: ok.' "$log"; then
	fail "the run did not validate the Tyrian 2000 data"
fi

off="$OUT_DIR/$LABEL.off"
cmp -s "$off.state" "$OUT_DIR/$LABEL.on.state" || fail "state/RNG hashes differ between depth off and on"
if [ "$OFF_BASE" != "-" ]; then
	[ -f "$OFF_BASE" ] || fail "missing baseline $OFF_BASE"
	cmp -s "$OFF_BASE" "$off.txt" || fail "depth-off frame hashes differ from the baseline $(basename "$OFF_BASE")"
fi

identical=0
case " $REQUIRE " in *" identical "*) identical=1 ;; esac
for setting in on; do
	if [ "$identical" -eq 1 ]; then
		cmp -s "$off.txt" "$OUT_DIR/$LABEL.$setting.txt" || fail "frames differ with depth $setting, but this case must not shadow"
	else
		cmp -s "$off.txt" "$OUT_DIR/$LABEL.$setting.txt" && fail "depth $setting presented the same frames as off (no shadow)"
	fi
done


if [ "${DEPTH_UPDATE:-0}" != "1" ]; then
[ -f "$SHADOW_BASE" ] || fail "missing baseline $SHADOW_BASE (run the suite with --update)"
cmp -s "$SHADOW_BASE" "$OUT_DIR/$LABEL.$LEVEL.txt" || fail "depth $LEVEL frame hashes differ from the baseline $(basename "$SHADOW_BASE")"
fi

depth_stat() {
	local line='Depth shadows:' key=$1
	case "$key" in
		fog.*) line='Depth fog:'; key=${key#fog.} ;;
		light.*) line='Depth light:'; key=${key#light.} ;;
	esac
	grep -E "$line" "$log" | grep -oE "[[:space:]]$key=[0-9]+" | head -n 1 | cut -d= -f2
}

summary=""
for req in $REQUIRE; do
	[ "$req" = identical ] && continue
	case "$req" in
		*'>'*) key=${req%%>*}; op='>'; want=${req#*>} ;;
		*'='*) key=${req%%=*}; op='='; want=${req#*=} ;;
		*) fail "bad assertion '$req'" ;;
	esac
	got=$(depth_stat "$key")
	[ -n "$got" ] || fail "no '$key' in the depth shadow line"
	if [ "$op" = '>' ] && [ "$got" -le "$want" ]; then fail "coverage: $key=$got, expected > $want"; fi
	if [ "$op" = '=' ] && [ "$got" -ne "$want" ]; then fail "coverage: $key=$got, expected = $want"; fi
	summary="$summary $key=$got"
done

if [ "${DEPTH_UPDATE:-0}" = "1" ]; then
	cp "$OUT_DIR/$LABEL.$LEVEL.txt" "$SHADOW_BASE"
	echo "UPDATE $LABEL: $(wc -l < "$SHADOW_BASE" | tr -d ' ') lines ($LEVEL)"
	exit 0
fi

lines=$(wc -l < "$SHADOW_BASE" | tr -d ' ')
echo "PASS $LABEL: $lines lines, depth off/on state identical, $LEVEL baseline ok, coverage:$summary"
rm -f "$OUT_DIR/$LABEL".*.txt "$OUT_DIR/$LABEL".*.state
exit 0
