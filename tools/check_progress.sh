#!/bin/bash
# Progress checks, without baselines, downloads or protected-data exports.
# Usage: tools/check_progress.sh [binary] [2.1|2000] [existing-data-dir]
# PROGRESS_FIXTURE_ONLY=1 runs only authored synthetic observer fixtures.
# Real probes use ordinary level ticks, never --regress-boss acceleration.
# Their coverage is an early-route sample, not proof of full-level completion.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN=${1:-"$ROOT/opentyrian"}
VARIANT=${2:-2.1}
DATA=${3:-}
case "$VARIANT" in
    2.1) DATA=${DATA:-${TYRIAN_DATA:-"$ROOT/data"}} ;;
    2000) DATA=${DATA:-${TYRIAN2000_DATA:-}} ;;
    *) echo 'FAIL progress: variant must be 2.1 or 2000' >&2; exit 1 ;;
esac
WORK=$(mktemp -d "${TMPDIR:-/tmp}/progress-check.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
CC=${CC:-cc}
"$CC" -std=iso9899:1999 -pedantic -Wall -Wextra -Werror \
    -DMODERN_PROGRESS_STANDALONE -I"$ROOT/src" \
    "$ROOT/tools/progress_fixture.c" "$ROOT/src/modern_progress.c" -o "$WORK/fixture"
"$WORK/fixture"
if [ "${PROGRESS_FIXTURE_ONLY:-0}" = 1 ]; then exit 0; fi
[ -x "$BIN" ] || { echo 'FAIL progress: build the binary first' >&2; exit 1; }
[ -n "$DATA" ] && [ -d "$DATA" ] || {
    echo 'FAIL progress: supply an existing data directory (never downloads)' >&2; exit 1;
}
DATA=$(cd "$DATA" && pwd)
mkdir -p "$WORK/home"
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
# Sequential, enemy-clear endpoint, rewind candidate and return candidate.
# 2000 additionally samples the marker-free E5/L1 fallback. No raw data leaves
# the provider; files contain only hashes and aggregate observer coverage.
LEVELS='1:6 1:4 1:16 4:18'
if [ "$VARIANT" = 2000 ]; then LEVELS="$LEVELS 5:1"; fi
for level in $LEVELS; do
    for mode in classic modern; do
        extra=()
        if [ "$mode" = modern ]; then extra=(--regress-modern --regress-aspect=16:9); fi
        for observer in on off; do
            flags=()
            if [ "$observer" = off ]; then flags=(--regress-observer-off); else flags=(--regress-progress-check); fi
            label=${level/:/-}-$mode-$observer
            if ! "$BIN" --variant="$VARIANT" --data="$DATA" --regress-level="$level" \
                --regress-user-root="$WORK/home" --regress-frames=300 --regress-detail=4 --regress-seed=32402394 \
                --regress-state-out="$WORK/$label.state" --regress-out="$WORK/$label.frame" \
                ${extra[@]+"${extra[@]}"} ${flags[@]+"${flags[@]}"} 2>&1 | \
                awk '/Progress coverage:/ { line=$0; sub(/^.*Progress coverage:/,"Progress coverage:",line); if (line ~ /^Progress coverage: [a-z_0-9= -]+$/) print line }' > "$WORK/$label.log"; then
                # Filter before writing: only aggregate progress metadata may
                # enter logs, particularly for protected 2000 data.
                echo "FAIL progress: $VARIANT $level $mode observer=$observer" >&2; exit 1
            fi
        done
        prefix=${level/:/-}-$mode
        cmp "$WORK/$prefix-on.state" "$WORK/$prefix-off.state" || {
            echo "FAIL progress: state/RNG changed $VARIANT $level $mode" >&2; exit 1;
        }
        if [ "$mode" = classic ]; then
            cmp "$WORK/$prefix-on.frame" "$WORK/$prefix-off.frame" || {
                echo "FAIL progress: Classic framebuffer changed $VARIANT $level" >&2; exit 1;
            }
        else
            coverage=$(grep '^.*Progress coverage:' "$WORK/$prefix-on.log" | tail -n 1)
            # Nonzero ticks alone does not prove traversal; require advancing
            # samples except for explicitly unsupported marker-free fallback.
            if [ "$level" = 4:18 ] || [ "$level" = 5:1 ]; then
                echo "$coverage" | grep -Eq 'samples=[1-9][0-9]*' || exit 1
            else
                echo "$coverage" | grep -Eq 'advances=[1-9][0-9]*' || {
                    echo "FAIL progress: no natural traversal sample $VARIANT $level" >&2; exit 1;
                }
            fi
            echo "$coverage" | grep -Eq 'events=[1-9][0-9]*' || {
                echo "FAIL progress: no event hook executed $VARIANT $level" >&2; exit 1;
            }
            echo "$coverage"
        fi
        echo "PASS progress: $VARIANT $level $mode observer OFF/ON state/RNG"
    done
done
# Script1:3 reaches JE_mainKeyboardInput/JE_playerMovement (not playDemo).
# Existing --constant supplies autofire through that path; pause is requested
# on the final frame. This exposes cancellation accidentally inserted into the
# general input branch that scenario playback cannot reach.
for observer in on off; do
    flags=()
    if [ "$observer" = off ]; then flags=(--regress-observer-off); else flags=(--regress-progress-check); fi
    if ! "$BIN" --variant="$VARIANT" --data="$DATA" --regress-script=1:3 \
        --constant --regress-stick=16383,0 --regress-menu=pause --regress-frames=700 \
        --regress-modern --regress-aspect=16:9 --regress-seed=32402394 --regress-detail=4 \
        --regress-user-root="$WORK/home" --regress-state-out="$WORK/input-$observer.state" \
        ${flags[@]+"${flags[@]}"} 2>&1 | \
        awk '/Progress coverage:/ { line=$0; sub(/^.*Progress coverage:/,"Progress coverage:",line); if (line ~ /^Progress coverage: [a-z_0-9= -]+$/) print line }' > "$WORK/input-$observer.log"; then
        echo "FAIL progress: $VARIANT normal input/pause observer=$observer" >&2; exit 1
    fi
done
cmp "$WORK/input-on.state" "$WORK/input-off.state"
coverage=$(cat "$WORK/input-on.log")
for token in 'advances=[1-9][0-9]*' 'events=[1-9][0-9]*' 'shot_samples=[1-9][0-9]*' 'menu_requests=1' 'blocked_samples=0 '; do
    echo "$coverage" | grep -Eq "$token" || {
        echo "FAIL progress: $VARIANT normal input/pause missing $token" >&2; exit 1;
    }
done
echo "$coverage"
echo "PASS progress: $VARIANT real keyboard/movement autofire and pause OFF/ON"

# An isolated runtime fixture, deliberately accelerated by --regress-boss.
# Positive boss_wait_samples prove actual active matching enemy state, while
# per-frame harness assertions prove the gauge freezes. It is NOT a natural
# progression-to-boss or final-boss-defeat test.
if ! "$BIN" --variant="$VARIANT" --data="$DATA" --regress-level=1:1 \
    --regress-boss --regress-frames=3000 --regress-modern --regress-aspect=16:9 \
    --regress-progress-check --regress-user-root="$WORK/home" \
    --regress-state-out="$WORK/boss.state" 2>&1 | \
    awk '/Progress coverage:/ { line=$0; sub(/^.*Progress coverage:/,"Progress coverage:",line); if (line ~ /^Progress coverage: [a-z_0-9= -]+$/) print line }' > "$WORK/boss.log"; then
    echo "FAIL progress: $VARIANT accelerated runtime boss fixture" >&2; exit 1
fi
coverage=$(cat "$WORK/boss.log")
echo "$coverage" | grep -Eq 'boss_wait_samples=[1-9][0-9]*' || {
    echo "FAIL progress: $VARIANT runtime boss wait not executed" >&2; exit 1;
}
echo "$coverage"
echo "PASS progress: $VARIANT accelerated runtime boss-wait fixture (not natural route)"
echo 'PASS progress: representative early-route probes (no full-route/boss claim)'
