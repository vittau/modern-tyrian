#!/bin/bash
# Cheap bootstrap/provider guards; no new baselines or original asset fixtures.
set -eu

SOURCE_BIN=$1
DATA_DIR=$(cd "$2" && pwd)
OUT=$3
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
SANDBOX="$OUT/user-sandbox"
mkdir -p "$SANDBOX/home" "$SANDBOX/xdg" "$SANDBOX/appdata" "$SANDBOX/cwd" "$SANDBOX/bin"
# Isolate executable/portable data discovery as well as the platform user root.
BIN="$SANDBOX/bin/$(basename "$SOURCE_BIN")"
cp "$SOURCE_BIN" "$BIN"
# MSYS2 cp can append .exe when SOURCE_BIN omitted the executable suffix.
if [ -f "$BIN.exe" ]; then BIN="$BIN.exe"; fi

run_isolated() (
	cd "$SANDBOX/cwd"
	export HOME="$SANDBOX/home" XDG_CONFIG_HOME="$SANDBOX/xdg" APPDATA="$SANDBOX/appdata"
	export XDG_DATA_HOME="$SANDBOX/xdg" TYRIAN2000_DATA=
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

mkdir -p "$OUT/empty-data" "$OUT/wrong-data" "$OUT/partial-data" "$OUT/wrong-data-2000" \
	"$OUT/partial-data-2000" "$OUT/nofallback-2000" "$SANDBOX/cwd/data"
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

# Tyrian 2000 (--variant=2000): a root is validated as a whole installation, in
# regress mode so that no user file can be touched.  All of these use code-owned
# headers or the approved 2.1 data, never a Tyrian 2000 file.
v2000_args=(--variant=2000 --regress-demo=1 --regress-out="$OUT/unexpected.hashes")
# No --data, no TYRIAN2000_DATA: the default search must not fall back to the
# 2.1 ./data directory of the working directory.
: > "$SANDBOX/cwd/data/tyrian1.lvl"
expect_failure v2000-no-data 'Tyrian 2000 requires its own data files' "${v2000_args[@]}"
grep -Fq 'validation: not-found; file: tyrian1.lvl.' "$OUT/v2000-no-data.log"
expect_failure v2000-empty 'Tyrian 2000 requires its own data files' \
	--data="$OUT/empty-data" "${v2000_args[@]}"
# Wrong variant, both ways.  2.1 data given as 2000:
expect_failure v2000-given-21-data 'The Tyrian v2.0/v2.1 data files were found.  Tyrian 2000 requires the Tyrian 2000 data files.' \
	--data="$DATA_DIR" "${v2000_args[@]}"
grep -Fq 'validation: wrong-variant; file: tyrian.shp.' "$OUT/v2000-given-21-data.log"
: > "$OUT/wrong-data-2000/tyrian1.lvl"
printf '\014\000' > "$OUT/wrong-data-2000/tyrian.shp"
expect_failure v2000-given-12-banks 'The Tyrian v2.0/v2.1 data files were found.  Tyrian 2000 requires the Tyrian 2000 data files.' \
	--data="$OUT/wrong-data-2000" "${v2000_args[@]}"
# ... and (above) a 13-bank header given as 2.1.  A 13-bank header without the
# rest of the installation names the first missing file, never a 2.1 file.
: > "$OUT/partial-data-2000/tyrian1.lvl"
printf '\015\000' > "$OUT/partial-data-2000/tyrian.shp"
expect_failure v2000-missing-file 'A required Tyrian 2000 data file is missing.' \
	--data="$OUT/partial-data-2000" "${v2000_args[@]}"
grep -Fq 'validation: missing-file; file: tyrian.hdt.' "$OUT/v2000-missing-file.log"
: > "$OUT/nofallback-2000/tyrian1.lvl"
expect_failure v2000-no-fallback 'A required Tyrian 2000 data file could not be opened.' \
	--data="$OUT/nofallback-2000" "${v2000_args[@]}"

# The copied binary directory is an authored fixture, including MSYS2 .exe names.
if [ -n "$(find "$SANDBOX" -path "$SANDBOX/bin" -prune -o -type f -print | grep -v '/cwd/data/tyrian1.lvl$')" ]; then
	echo 'FAIL variant-bootstrap: a user file was created'
	find "$SANDBOX" -type f -print
	exit 1
fi
# No namespace/migration directory may appear in the isolated user roots.
if [ -n "$(find "$SANDBOX/home" "$SANDBOX/xdg" "$SANDBOX/appdata" -mindepth 1 -print)" ]; then
	echo 'FAIL variant-bootstrap: a user directory was created'
	exit 1
fi
rm -rf "$SANDBOX" "$OUT/empty-data" "$OUT/wrong-data" "$OUT/partial-data" "$OUT/wrong-data-2000" \
	"$OUT/partial-data-2000" "$OUT/nofallback-2000"
echo 'PASS variant-bootstrap: early errors, 2.1 frame/state identity, single root, 2000 provider diagnostics both ways, no user files'
