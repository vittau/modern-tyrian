#!/bin/bash
# check_depth_held.sh -- one case of the held-frame proof (depth shadows and bloom/lighting
# kept on the pause screen, the in-game menu and the in-game help), shared by
# tools/regress.sh (Tyrian 2.1) and tools/regress-2000.sh (Tyrian 2000).
#
#   check_depth_held.sh BIN VARIANT DATA_DIR OUT_DIR LABEL BASELINE REQUIRE -- ARGS...
#
# BASELINE  the committed frame-hash file of the Depth-Low run (this case's own baseline,
#           written instead of compared when HELD_UPDATE=1).
# REQUIRE   space-separated assertions over the "Depth held:", "Light held:" and
#           "Held check:" log lines of the Depth-Low run, KEY>N, KEY<N or KEY=N (frames,
#           shadowed_px, space_frames, emitter_px, lit_px, overlay_px, overlay_changed_px,
#           compared_px, mismatch_px, mismatch_max_dist).  A hash alone cannot show that a
#           held frame really got shadows and light.
# ARGS      the scenario, which must open an in-level screen on its last frame, e.g.
#           --regress-script=1:3 --regress-frames=350 --regress-menu=pause --regress-modern.
#
# The scenario runs four times: effects off (the old behaviour), lighting only, Depth Low
# with lighting, and Depth High with lighting.  The state/RNG streams must be identical in
# all four (the effects are display-only); the Depth-Low run must equal its baseline, must
# not present the same last frame as lighting alone (the held frame got the shadows) and
# must pass the REQUIRE list.  The runs also pass --regress-held-check, which compares the
# held frame with the last live frame outside the overlay.
#
# Only counts and hashes are read from the logs, so no Tyrian 2000 content is ever copied
# out of a run.
set -uo pipefail

if [ "$#" -lt 9 ] || [ "$8" != "--" ]; then
	echo "usage: $0 BIN VARIANT DATA_DIR OUT_DIR LABEL BASELINE REQUIRE -- ARGS..." >&2
	exit 2
fi

BIN=$1 VARIANT=$2 DATA_DIR=$3 OUT_DIR=$4 LABEL=$5 BASELINE=$6 REQUIRE=$7
shift 8

mkdir -p "$OUT_DIR"
variant_args=()
[ "$VARIANT" = "2000" ] && variant_args=(--variant=2000)

log="$OUT_DIR/$LABEL.low.log"
fail() {
	echo "FAIL $LABEL: $1"
	[ -f "$log" ] && grep -E 'held|Held|FAIL|rror' "$log" | tail -n 6 | sed 's/^/  /'
	exit 1
}

run() {
	local name=$1 depth=$2 lighting=$3
	shift 3
	log="$OUT_DIR/$LABEL.$name.log"
	SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
		"$BIN" ${variant_args[@]+"${variant_args[@]}"} --data="$DATA_DIR" "$@" \
		--regress-depth="$depth" --regress-lighting="$lighting" --regress-held-check \
		--regress-out="$OUT_DIR/$LABEL.$name.txt" --regress-state-out="$OUT_DIR/$LABEL.$name.state" \
		>"$log" 2>&1
	local rc=$?
	[ "$rc" -eq 0 ] || fail "the $name run exited with $rc"
	[ -s "$OUT_DIR/$LABEL.$name.txt" ] && [ -s "$OUT_DIR/$LABEL.$name.state" ] || fail "empty output ($name)"
}

run none off off "$@"
run light off low "$@"
run low low low "$@"
run high high low "$@"
log="$OUT_DIR/$LABEL.low.log"

if [ "$VARIANT" = "2000" ] && ! grep -Fq 'validation: ok.' "$log"; then
	fail "the run did not validate the Tyrian 2000 data"
fi

for name in light low high; do
	cmp -s "$OUT_DIR/$LABEL.none.state" "$OUT_DIR/$LABEL.$name.state" || fail "state/RNG hashes differ between effects off and $name"
done

# The held frame is the last one.  Lighting alone must change it against no effects at all
# (before the held frames reused the light, it was identical), and Depth Low must change it
# against lighting alone.
[ "$(tail -n 1 "$OUT_DIR/$LABEL.none.txt")" != "$(tail -n 1 "$OUT_DIR/$LABEL.light.txt")" ] ||
	fail "the held frame is the same with lighting on and with every effect off (no light on it)"
# A starfield level casts no shadow ("shadowed_px=0" in REQUIRE): the held frame must then be the
# very same with Depth Low and High as with lighting alone.
if [[ " $REQUIRE " == *" shadowed_px=0 "* ]]; then
	[ "$(tail -n 1 "$OUT_DIR/$LABEL.light.txt")" = "$(tail -n 1 "$OUT_DIR/$LABEL.low.txt")" ] &&
		[ "$(tail -n 1 "$OUT_DIR/$LABEL.light.txt")" = "$(tail -n 1 "$OUT_DIR/$LABEL.high.txt")" ] ||
		fail "the held frame has a shadow with depth on, but this case must not shadow"
else
	[ "$(tail -n 1 "$OUT_DIR/$LABEL.light.txt")" != "$(tail -n 1 "$OUT_DIR/$LABEL.low.txt")" ] ||
		fail "the held frame is the same with Depth Low and with lighting alone (no shadow on it)"
	[ "$(tail -n 1 "$OUT_DIR/$LABEL.low.txt")" != "$(tail -n 1 "$OUT_DIR/$LABEL.high.txt")" ] ||
		fail "the held frame is the same with Depth Low and High"
fi

if [ "${HELD_UPDATE:-0}" = "1" ]; then
	cp "$OUT_DIR/$LABEL.low.txt" "$BASELINE"
	echo "UPDATE $LABEL: $(wc -l < "$BASELINE" | tr -d ' ') lines"
	exit 0
fi
[ -f "$BASELINE" ] || fail "missing baseline $BASELINE (run the suite with --update)"
cmp -s "$BASELINE" "$OUT_DIR/$LABEL.low.txt" || fail "depth low frame hashes differ from the baseline $(basename "$BASELINE")"

held_stat() {
	grep -E 'Depth held:|Light held:|Held check:' "$log" | grep -oE "[[:space:]]$1=[0-9]+" | head -n 1 | cut -d= -f2
}

summary=""
for req in $REQUIRE; do
	case "$req" in
		*'>'*) key=${req%%>*}; op='>'; want=${req#*>} ;;
		*'<'*) key=${req%%<*}; op='<'; want=${req#*<} ;;
		*'='*) key=${req%%=*}; op='='; want=${req#*=} ;;
		*) fail "bad assertion '$req'" ;;
	esac
	got=$(held_stat "$key")
	[ -n "$got" ] || fail "no '$key' in the held lines"
	if [ "$op" = '>' ] && [ "$got" -le "$want" ]; then fail "coverage: $key=$got, expected > $want"; fi
	if [ "$op" = '<' ] && [ "$got" -ge "$want" ]; then fail "coverage: $key=$got, expected < $want"; fi
	if [ "$op" = '=' ] && [ "$got" -ne "$want" ]; then fail "coverage: $key=$got, expected = $want"; fi
	summary="$summary $key=$got"
done

lines=$(wc -l < "$BASELINE" | tr -d ' ')
echo "PASS $LABEL: $lines lines, effects off/lighting/low/high state identical, baseline ok, coverage:$summary"
rm -f "$OUT_DIR/$LABEL".*.txt "$OUT_DIR/$LABEL".*.state
exit 0
