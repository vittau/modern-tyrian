#!/bin/bash
# check_depth_layers.sh -- one case of the depth-layer (stage 1) proof, shared by
# tools/regress.sh (Tyrian 2.1) and tools/regress-2000.sh (Tyrian 2000).
#
#   check_depth_layers.sh BIN VARIANT DATA_DIR OUT_DIR LABEL BASELINE REQUIRE -- ARGS...
#
# VARIANT   2.1 or 2000.
# BASELINE  a committed frame-hash file the layers-on run must reproduce, or "-"
#           for none (the layers-on and layers-off runs are always compared).
# REQUIRE   space-separated coverage assertions over the "Layer coverage/filters/
#           present" log lines, KEY>N or KEY=N (see src/regress.c for the keys),
#           e.g. "bg2blend>0 water>0".  A hash alone cannot show that a path ran.
# ARGS      the scenario, e.g. --regress-demo=1 --regress-modern --regress-detail=4.
#
# The case runs the scenario twice: with --regress-layer-check (layers stamped,
# the interpolated frame at alpha = 1 compared with the live one every tick) and
# without it.  Both must produce the identical frame and state hash streams -- the
# layer buffer changes no presented pixel, no game state and no RNG draw -- and
# the first run must log "Layer check PASS" and meet every REQUIRE assertion.
#
# Only counts and hashes are read from the logs, so no Tyrian 2000 content is
# ever copied out of a run.
set -uo pipefail

if [ "$#" -lt 8 ] || [ "$7" = "--" ]; then
	echo "usage: $0 BIN VARIANT DATA_DIR OUT_DIR LABEL BASELINE REQUIRE -- ARGS..." >&2
	exit 2
fi

BIN=$1 VARIANT=$2 DATA_DIR=$3 OUT_DIR=$4 LABEL=$5 BASELINE=$6 REQUIRE=$7
shift 7
[ "$1" = "--" ] && shift

mkdir -p "$OUT_DIR"
variant_args=()
[ "$VARIANT" = "2000" ] && variant_args=(--variant=2000)

on="$OUT_DIR/$LABEL.on"
off="$OUT_DIR/$LABEL.off"
log="$OUT_DIR/$LABEL.log"
offlog="$OUT_DIR/$LABEL.off.log"

fail() {
	echo "FAIL $LABEL: $1"
	[ -f "$log" ] && grep -E 'Layer |FAIL|rror' "$log" | tail -n 5 | sed 's/^/  /'
	exit 1
}

SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
	"$BIN" ${variant_args[@]+"${variant_args[@]}"} --data="$DATA_DIR" "$@" --regress-layer-check \
	--regress-out="$on.txt" --regress-state-out="$on.state" >"$log" 2>&1
rc=$?
[ "$rc" -eq 0 ] || fail "layers-on run exited with $rc"
grep -Fq 'Layer check PASS' "$log" || fail "no 'Layer check PASS' line"

SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
	"$BIN" ${variant_args[@]+"${variant_args[@]}"} --data="$DATA_DIR" "$@" \
	--regress-out="$off.txt" --regress-state-out="$off.state" >"$offlog" 2>&1
rc=$?
[ "$rc" -eq 0 ] || fail "layers-off run exited with $rc"

if [ "$VARIANT" = "2000" ] && ! grep -Fq 'validation: ok.' "$log"; then
	fail "the run did not validate the Tyrian 2000 data"
fi

[ -s "$on.txt" ] && [ -s "$on.state" ] || fail "empty output"
cmp -s "$on.txt" "$off.txt" || fail "frame hashes differ with layers on/off"
cmp -s "$on.state" "$off.state" || fail "state/RNG hashes differ with layers on/off"
if [ "$BASELINE" != "-" ]; then
	[ -f "$BASELINE" ] || fail "missing baseline $BASELINE"
	cmp -s "$BASELINE" "$on.txt" || fail "frame hashes differ from the baseline $(basename "$BASELINE")"
fi

layer_stat() {
	grep -E 'Layer (coverage|filters|present):' "$log" | grep -oE "[[:space:]]$1=[0-9]+" | head -n 1 | cut -d= -f2
}

summary=""
for req in $REQUIRE; do
	case "$req" in
		*'>'*) key=${req%%>*}; op='>'; want=${req#*>} ;;
		*'='*) key=${req%%=*}; op='='; want=${req#*=} ;;
		*) fail "bad assertion '$req'" ;;
	esac
	got=$(layer_stat "$key")
	[ -n "$got" ] || fail "no '$key' in the layer coverage lines"
	if [ "$op" = '>' ] && [ "$got" -le "$want" ]; then fail "coverage: $key=$got, expected > $want"; fi
	if [ "$op" = '=' ] && [ "$got" -ne "$want" ]; then fail "coverage: $key=$got, expected = $want"; fi
	summary="$summary $key=$got"
done

ticks=$(grep -oE 'Layer check PASS: ticks=[0-9]+' "$log" | grep -oE '[0-9]+$')
echo "PASS $LABEL: $ticks ticks, layers on/off identical (frames+state), coverage:$summary"
rm -f "$on.txt" "$on.state" "$off.txt" "$off.state"
exit 0
