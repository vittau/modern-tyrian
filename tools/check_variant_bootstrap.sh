#!/bin/bash
# Cheap bootstrap/provider guards; no new baselines or original asset fixtures.
set -eu

BIN=$1
DATA_DIR=$(cd "$2" && pwd)
OUT=$3
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
SANDBOX="$OUT/user-sandbox"
mkdir -p "$SANDBOX/home" "$SANDBOX/xdg" "$SANDBOX/appdata" "$SANDBOX/cwd"

run_isolated() (
	cd "$SANDBOX/cwd"
	export HOME="$SANDBOX/home" XDG_CONFIG_HOME="$SANDBOX/xdg" APPDATA="$SANDBOX/appdata"
	export SteamDeck=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
	"$BIN" "$@"
)

expect_failure() {
	local label=$1 message=$2 status=0
	shift 2
	run_isolated "$@" > "$OUT/$label.log" 2>&1 || status=$?
	if [ "$status" -ne 1 ] || ! grep -Fq "$message" "$OUT/$label.log"; then
		echo "FAIL $label: expected exit 1 and '$message', got exit $status"
		cat "$OUT/$label.log"
		exit 1
	fi
}

# No regress flag: early rejection must precede even Deck or explicit logging.
expect_failure variant-unavailable 'Tyrian 2000 is not available yet.' \
	--log-file="$SANDBOX/cwd/should-not-exist.log" --variant=2000
expect_failure variant-unknown "Unknown game variant 'bogus'." --variant=bogus
expect_failure variant-conflict 'Conflicting --variant options.' --variant=2.1 --variant=2000

for variant in default explicit; do
	set --
	if [ "$variant" = explicit ]; then set -- --variant=2.1; fi
	run_isolated "$@" --data="$DATA_DIR" --regress-demo=1 --regress-detail=2 \
		--regress-frames=30 --regress-out="$OUT/$variant.hashes" \
		--regress-state-out="$OUT/$variant.state" > "$OUT/$variant.log" 2>&1
	[ "$(wc -l < "$OUT/$variant.hashes")" -eq 30 ] || { echo "FAIL variant-$variant: wrong frame count"; exit 1; }
	grep -Fq 'validation: ok.' "$OUT/$variant.log" || { echo "FAIL variant-$variant: no validation log"; exit 1; }
done
cmp "$OUT/default.hashes" "$OUT/explicit.hashes"
cmp "$OUT/default.state" "$OUT/explicit.state"

# Existing short/abbreviated --data forms and last-occurrence precedence.
run_isolated --variant 2.1 -st/does-not-exist --dat "$DATA_DIR" --regress-demo=1 \
	--regress-detail=2 --regress-frames=30 --regress-out="$OUT/aliases.hashes" > "$OUT/aliases.log" 2>&1
cmp "$OUT/default.hashes" "$OUT/aliases.hashes"

mkdir -p "$OUT/empty-data" "$OUT/wrong-data" "$OUT/partial-data"
expect_failure data-explicit-empty 'The Tyrian data files were not found.' \
	--data="$OUT/empty-data" --regress-demo=1 --regress-out="$OUT/unexpected.hashes"
# Code-owned shape-count header, not a file copied from Tyrian 2000.
: > "$OUT/wrong-data/tyrian1.lvl"
printf '\015\000' > "$OUT/wrong-data/tyrian.shp"
expect_failure data-wrong-variant 'The Tyrian 2000 data files were found.  OpenTyrian requires the Tyrian v2.0/v2.1 data files.' \
	--data="$OUT/wrong-data" --regress-demo=1 --regress-out="$OUT/unexpected.hashes"
grep -Fq 'validation: wrong-variant; file: tyrian.shp.' "$OUT/data-wrong-variant.log"
: > "$OUT/partial-data/tyrian1.lvl"
expect_failure data-no-fallback 'The Tyrian shape data file could not be opened.' \
	--data="$OUT/partial-data" --regress-demo=1 --regress-out="$OUT/unexpected.hashes"

if [ -n "$(find "$SANDBOX" -type f -print)" ]; then
	echo 'FAIL variant-bootstrap: a user file was created'
	find "$SANDBOX" -type f -print
	exit 1
fi
# No namespace/migration directory may appear in the isolated user roots.
if [ -n "$(find "$SANDBOX/home" "$SANDBOX/xdg" "$SANDBOX/appdata" -mindepth 1 -print)" ]; then
	echo 'FAIL variant-bootstrap: a user directory was created'
	exit 1
fi
rm -rf "$SANDBOX" "$OUT/empty-data" "$OUT/wrong-data" "$OUT/partial-data"
echo 'PASS variant-bootstrap: early errors, 2.1 frame/state identity, single root, no user files'
