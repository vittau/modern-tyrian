#!/bin/bash
# Real save/config serializers, exclusively inside isolated user roots.
set -eEu

SOURCE_BIN=$1
OUT=$3
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

# Under set -e a failing step would otherwise exit without a word (-E carries
# the trap into run_isolated and the other functions): name it and
# show the tail of the newest log, which is the run that step just checked.
report_failure() {
	local status=$1 line=$2 command=$3 log
	echo "FAIL user-paths: line $line exited $status: $command" >&2
	log=$(ls -t "$OUT"/*.log 2>/dev/null | head -n 1) || true
	if [ -n "$log" ]; then
		echo "--- tail of $log ---" >&2
		tail -n 20 "$log" >&2 || true
	fi
}
trap 'report_failure $? $LINENO "$BASH_COMMAND"' ERR
SANDBOX="$OUT/sandbox"
mkdir -p "$SANDBOX/home/.config" "$SANDBOX/xdg" "$SANDBOX/appdata" "$SANDBOX/cwd" "$SANDBOX/bin"
# Also isolate portable detection: never probe a config beside the real binary.
BIN="$SANDBOX/bin/$(basename "$SOURCE_BIN")"
cp "$SOURCE_BIN" "$BIN"

run_isolated() (
	cd "$SANDBOX/cwd"
	export HOME="$SANDBOX/home" XDG_CONFIG_HOME="$SANDBOX/xdg" APPDATA="$SANDBOX/appdata"
	export SteamDeck=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
	"$BIN" "$@"
)

run_files() {
	local label=$1 root=$2
	shift 2
	run_isolated --regress-user-root="$root" --regress-user-files "$@" > "$OUT/$label.log" 2>&1
}

expect_no_log() {
	if grep -Fq "$1" "$2"; then
		echo "FAIL user-paths: unexpected '$1' in $2"
		exit 1
	fi
}

# Generate a code-owned save with the game, without any game data or fixtures.
run_files generate "$SANDBOX/generated" --record
SAVE="$SANDBOX/generated/tyrian21/tyrian.sav"
[ "$(wc -c < "$SAVE")" -eq 2502 ]
[ "$(wc -c < "$SANDBOX/generated/tyrian.cfg")" -eq 28 ]
[ -s "$SANDBOX/generated/opentyrian.cfg" ]
[ -s "$SANDBOX/generated/tyrian21/demorec.1" ]
[ ! -e "$SANDBOX/generated/demorec.1" ]
grep -Fq 'nothing to migrate (no root tyrian.sav)' "$OUT/generate.log"
cp "$SAVE" "$OUT/generated.sav"

ROOT="$SANDBOX/migration"
mkdir -p "$ROOT"
cp "$SAVE" "$ROOT/tyrian.sav"
run_files migrate "$ROOT"
grep -Fq 'migrated root tyrian.sav to tyrian21/tyrian.sav; original kept' "$OUT/migrate.log"
cmp "$OUT/generated.sav" "$ROOT/tyrian.sav"
cmp "$OUT/generated.sav" "$ROOT/tyrian21/tyrian.sav"
[ ! -e "$ROOT/tyrian21/tyrian.sav.tmp" ]
# A second start loads and writes the existing namespaced save byte-for-byte.
run_files roundtrip "$ROOT"
grep -Fq 'nothing to migrate (tyrian21/tyrian.sav already exists)' "$OUT/roundtrip.log"
expect_no_log "'tyrian.sav' is invalid or missing" "$OUT/roundtrip.log"
cmp "$OUT/generated.sav" "$ROOT/tyrian.sav"
cmp "$OUT/generated.sav" "$ROOT/tyrian21/tyrian.sav"
# Even a different legacy file can never override the destination.
printf 'different root save' > "$ROOT/tyrian.sav"
cp "$ROOT/tyrian.sav" "$OUT/different-root.sav"
run_files existing "$ROOT"
cmp "$OUT/generated.sav" "$ROOT/tyrian21/tyrian.sav"
cmp "$OUT/different-root.sav" "$ROOT/tyrian.sav"
grep -Fq 'already exists' "$OUT/existing.log"

for size in 0 2501 2503 4722; do
	ROOT="$SANDBOX/wrong-$size"
	mkdir -p "$ROOT"
	# Code-generated zero bytes, including a 2000-sized file; no 2000 data.
	dd if=/dev/zero of="$ROOT/tyrian.sav" bs=1 count="$size" 2>/dev/null
	cp "$ROOT/tyrian.sav" "$OUT/wrong-$size.sav"
	run_files "wrong-$size" "$ROOT"
	grep -Fq "skipped root tyrian.sav (length $size, expected 2502)" "$OUT/wrong-$size.log"
	cmp "$OUT/wrong-$size.sav" "$ROOT/tyrian.sav"
	[ ! -e "$ROOT/tyrian21/tyrian.sav.tmp" ]
done

# A blocked namespace works on Windows too, regardless of chmod/ACL semantics.
ROOT="$SANDBOX/blocked"
mkdir -p "$ROOT"
cp "$SAVE" "$ROOT/tyrian.sav"
printf 'namespace obstruction' > "$ROOT/tyrian21"
cp "$ROOT/tyrian21" "$OUT/obstruction"
run_files blocked "$ROOT"
grep -Fq 'using root save read-only, saving disabled for this session' "$OUT/blocked.log"
expect_no_log "'tyrian.sav' is invalid or missing" "$OUT/blocked.log"
cmp "$OUT/generated.sav" "$ROOT/tyrian.sav"
cmp "$OUT/obstruction" "$ROOT/tyrian21"
# An early path error must never let a 2000-sized root save enter the loader.
ROOT="$SANDBOX/blocked-wrong-length"
mkdir -p "$ROOT"
dd if=/dev/zero of="$ROOT/tyrian.sav" bs=1 count=4722 2>/dev/null
printf 'namespace obstruction' > "$ROOT/tyrian21"
cp "$ROOT/tyrian.sav" "$OUT/blocked-wrong.sav"
run_files blocked-wrong "$ROOT"
grep -Fq 'saving disabled for this session' "$OUT/blocked-wrong.log"
grep -Fq "'tyrian.sav' is invalid or missing" "$OUT/blocked-wrong.log"
cmp "$OUT/blocked-wrong.sav" "$ROOT/tyrian.sav"
# Exercise failure at temp-file creation after the source has been read.
ROOT="$SANDBOX/temp-blocked"
mkdir -p "$ROOT/tyrian21/tyrian.sav.tmp"
cp "$SAVE" "$ROOT/tyrian.sav"
run_files temp-blocked "$ROOT"
grep -Fq 'using root save read-only, saving disabled for this session' "$OUT/temp-blocked.log"
expect_no_log "'tyrian.sav' is invalid or missing" "$OUT/temp-blocked.log"
cmp "$OUT/generated.sav" "$ROOT/tyrian.sav"
[ ! -e "$ROOT/tyrian21/tyrian.sav" ]
[ -d "$ROOT/tyrian21/tyrian.sav.tmp" ]

# A real read-only namespace on systems where chmod enforces directory access.
READ_ONLY_ROOT="$SANDBOX/read-only"
mkdir -p "$READ_ONLY_ROOT/tyrian21"
cp "$SAVE" "$READ_ONLY_ROOT/tyrian.sav"
chmod 500 "$READ_ONLY_ROOT/tyrian21"
if ( : > "$READ_ONLY_ROOT/tyrian21/probe" ) 2>/dev/null; then
	chmod 700 "$READ_ONLY_ROOT/tyrian21"
	echo 'SKIP chmod-only failure: directory remains writable; portable obstruction cases passed'
else
	run_files read-only "$READ_ONLY_ROOT"
	chmod 700 "$READ_ONLY_ROOT/tyrian21"
	grep -Fq 'using root save read-only, saving disabled for this session' "$OUT/read-only.log"
	expect_no_log "'tyrian.sav' is invalid or missing" "$OUT/read-only.log"
	cmp "$OUT/generated.sav" "$READ_ONLY_ROOT/tyrian.sav"
	[ ! -e "$READ_ONLY_ROOT/tyrian21/tyrian.sav" ]
	[ ! -e "$READ_ONLY_ROOT/tyrian21/tyrian.sav.tmp" ]
fi

# Retry after fixing the obstruction succeeds; read-only policy is session-only.
rmdir "$ROOT/tyrian21/tyrian.sav.tmp"
run_files retry "$ROOT"
grep -Fq 'migrated root tyrian.sav' "$OUT/retry.log"
cmp "$OUT/generated.sav" "$ROOT/tyrian.sav"
cmp "$OUT/generated.sav" "$ROOT/tyrian21/tyrian.sav"

# Shared config reads and writes: namespace-local decoys must be ignored.
ROOT="$SANDBOX/configs"
run_files config-generate "$ROOT"
run_files config-normalize "$ROOT"
# Preserve a recognizable DOS gamma byte through the real serializer.
printf '\003' | dd of="$ROOT/tyrian.cfg" bs=1 seek=8 conv=notrunc 2>/dev/null
cp "$ROOT/tyrian.cfg" "$OUT/tyrian-shared.cfg"
printf "section 'video'\n\titem 'presentation' 'modern'\n\titem 'starfield_speed_percent' '73'\n" > "$ROOT/opentyrian.cfg"
printf 'ignored DOS config' > "$ROOT/tyrian21/tyrian.cfg"
printf "section 'video'\n\titem 'presentation' 'classic'\n" > "$ROOT/tyrian21/opentyrian.cfg"
cp "$ROOT/tyrian21/tyrian.cfg" "$OUT/dos-decoy"
cp "$ROOT/tyrian21/opentyrian.cfg" "$OUT/modern-decoy"
run_files config-read-write "$ROOT"
cmp "$OUT/tyrian-shared.cfg" "$ROOT/tyrian.cfg"
grep -Fq "item 'presentation' 'modern'" "$ROOT/opentyrian.cfg"
grep -Fq "item 'starfield_speed_percent' '73'" "$ROOT/opentyrian.cfg"
cmp "$OUT/dos-decoy" "$ROOT/tyrian21/tyrian.cfg"
cmp "$OUT/modern-decoy" "$ROOT/tyrian21/opentyrian.cfg"

# Normal startup with --help exercises platform root selection and migration,
# exits before loading data, and reads only sandboxed HOME/XDG/APPDATA.
case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*) PLATFORM_ROOT="$SANDBOX/appdata/OpenTyrian" ;;
	*) PLATFORM_ROOT="$SANDBOX/xdg/opentyrian" ;;
esac
mkdir -p "$PLATFORM_ROOT"
cp "$SAVE" "$PLATFORM_ROOT/tyrian.sav"
run_isolated --help > "$OUT/platform-root.log" 2>&1
cmp "$OUT/generated.sav" "$PLATFORM_ROOT/tyrian.sav"
cmp "$OUT/generated.sav" "$PLATFORM_ROOT/tyrian21/tyrian.sav"
# Root opentyrian.cfg still enables portable mode beside the copied binary.
printf "section 'video'\n\titem 'presentation' 'classic'\n" > "$SANDBOX/bin/opentyrian.cfg"
cp "$SAVE" "$SANDBOX/bin/tyrian.sav"
run_isolated --help > "$OUT/portable.log" 2>&1
cmp "$OUT/generated.sav" "$SANDBOX/bin/tyrian.sav"
cmp "$OUT/generated.sav" "$SANDBOX/bin/tyrian21/tyrian.sav"
rm "$SANDBOX/bin/opentyrian.cfg" "$SANDBOX/bin/tyrian.sav"
rm -rf "$SANDBOX/bin/tyrian21" "$PLATFORM_ROOT"

# Every default regress/selftest path remains cut off from user files.
run_isolated --selftest-gamepad > "$OUT/selftest.log" 2>&1
run_isolated --data="$2" --regress-demo=1 --regress-frames=1 \
	--regress-out="$OUT/no-user.hashes" > "$OUT/no-user.log" 2>&1
[ -z "$(find "$SANDBOX/home" "$SANDBOX/xdg" "$SANDBOX/appdata" "$SANDBOX/cwd" -mindepth 1 -type f -print)" ]
[ -z "$(find "$SANDBOX/home/.config" "$SANDBOX/xdg" "$SANDBOX/appdata" "$SANDBOX/cwd" -mindepth 1 -print)" ]
expect_no_log 'Save migration:' "$OUT/selftest.log"
expect_no_log 'Save migration:' "$OUT/no-user.log"

# Invalid attempts must fail before enabling or creating a sandbox root.
for args in root-only files-only selftest-root empty-root flag-argument; do
	status=0
	case "$args" in
		root-only) set -- --regress-user-root="$SANDBOX/invalid" ;;
		files-only) set -- --regress-user-files ;;
		selftest-root) set -- --selftest-gamepad --regress-user-root="$SANDBOX/invalid" ;;
		empty-root) set -- --regress-user-root= --regress-user-files ;;
		flag-argument) set -- --regress-user-files=invalid ;;
	esac
	run_isolated "$@" > "$OUT/invalid-$args.log" 2>&1 || status=$?
	[ "$status" -eq 1 ]
	[ ! -e "$SANDBOX/invalid" ]
done
rm -rf "$SANDBOX"
echo 'PASS user-paths: migration, originals, repeat, failures/retry, save roundtrip, shared configs, demos, platform/portable roots, regress/selftest isolation'
