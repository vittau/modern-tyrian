#!/bin/bash
# Tyrian 2000 data installer: data-free and network-free checks.
#
#   tools/check_installer.sh BIN DATA21_DIR OUTDIR
#
# Drives the real binary through `--install-2000=...` inside throw-away sandboxes
# (HOME, XDG_DATA_HOME, APPDATA and the copied binary all live under OUTDIR).
# Synthetic archives and folders come from tools/installer_fixtures.py, the test
# spec (`--install-2000-spec`) swaps the canonical size/SHA-256/manifest for
# theirs, and a C stub (tools/curl_stub.c, named in the spec by absolute path,
# so PATH plays no part) stands in for curl and the network.  No original
# Tyrian 2000 file is needed or produced.  DATA21_DIR (Tyrian 2.1) is only used
# for the "2.1 data given as Tyrian 2000" case.
set -uo pipefail

SOURCE_BIN=$1
DATA21=${2:-}
OUT=$3
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

rm -rf "$OUT"
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
FIX="$OUT/fixtures"
python3 "$ROOT/tools/installer_fixtures.py" "$FIX" || { echo "FAIL installer: could not generate fixtures (python3 needed)"; exit 1; }

VERIFY_MESSAGE="The downloaded Tyrian 2000 data could not be verified. Please try again or install the data manually."
URL="https://www.camanis.net/tyrian/tyrian2000.zip"
failures=0
checks=0

# Windows (MSYS2) runs the game as a native program: it logs Windows paths and
# takes the downloader path from the spec file as-is.
case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*) WINDOWS=1; EXE=.exe ;;
	*) WINDOWS=0; EXE= ;;
esac
# native PATH: the path in the form the game sees and logs, forward slashes
native() { if [ "$WINDOWS" = 1 ]; then cygpath -m "$1"; else printf '%s' "$1"; fi; }

# The downloader stub: built once, then the only "curl" any run can reach.
CC_BIN=$(command -v "${CC:-cc}" || command -v gcc || command -v clang || true)
[ -n "$CC_BIN" ] || { echo "FAIL installer: no C compiler for the curl stub"; exit 1; }
"$CC_BIN" -O0 -o "$OUT/curl-stub$EXE" "$ROOT/tools/curl_stub.c" || { echo "FAIL installer: could not build the curl stub"; exit 1; }
STUB_EXE="$(native "$OUT/curl-stub$EXE")"

fail() {
	echo "FAIL installer/$CASE: $*"
	[ -n "${LOG:-}" ] && [ -f "$LOG" ] && sed 's/^/    | /' "$LOG" | tail -n 12
	failures=$((failures + 1))
}
check() { checks=$((checks + 1)); }

# --- one sandbox per case ------------------------------------------------------------

new_sandbox() {
	CASE=$1
	unset STUB_MODE STUB_ZIP STUB_DOWNLOADER    # a case must ask for a downloader behaviour
	SB="$OUT/$1"
	rm -rf "$SB"
	mkdir -p "$SB/home" "$SB/xdg" "$SB/appdata" "$SB/cwd" "$SB/bin" "$SB/stubs" "$SB/empty"
	cp "$SOURCE_BIN" "$SB/bin/opentyrian"
	# Tripwires: a "curl" on PATH and beside the game both record the call and fail.
	cp "$OUT/curl-stub$EXE" "$SB/stubs/curl$EXE"
	cp "$OUT/curl-stub$EXE" "$SB/bin/curl$EXE"
	case "$(uname -s)" in
		Darwin) INSTALL="$SB/home/Library/Application Support/OpenTyrian/data-tyrian2000" ;;
		MINGW*|MSYS*|CYGWIN*) INSTALL="$SB/appdata/OpenTyrian/data-tyrian2000" ;;
		*) INSTALL="$SB/xdg/opentyrian/data-tyrian2000" ;;
	esac
	PARENT=$(dirname "$INSTALL")
	LOG=""
}

# stub_spec SRC DEST: copies a test spec to DEST with the downloader line for the
# stub (or STUB_DOWNLOADER) appended; prints DEST in the form the game can open.
# Anything that starts the game with a spec (run, or a launcher-flow case) goes
# through this, so the downloader is always named by absolute path.
stub_spec() {
	cp "$1" "$2"
	printf '\ndownloader %s\n' "${STUB_DOWNLOADER:-$STUB_EXE}" >> "$2"
	native "$2"
}

# run LABEL args...: sets RC and LOG.  STUB_MODE / STUB_ZIP pick the downloader
# behaviour; STUB_DOWNLOADER replaces the downloader path (default: the stub).
# Every test spec gets a "downloader" line appended, and a download without a
# spec is refused here, so no run can start the real curl.
run() {
	local label=$1 bin=${BIN_OVERRIDE:-$SB/bin/opentyrian} arg has_spec=0 has_download=0
	local args=()
	shift
	LOG="$SB/$label.log"
	for arg in "$@"; do
		case "$arg" in
			--install-2000-spec=*)
				has_spec=1
				arg="--install-2000-spec=$(stub_spec "${arg#--install-2000-spec=}" "$SB/$label.spec")" ;;
			--install-2000=download|--launcher-flow=download) has_download=1 ;;
		esac
		args+=("$arg")
	done
	if [ "$has_download" = 1 ] && [ "$has_spec" = 0 ]; then
		echo "FAIL installer/$CASE: harness error: a download run needs a test spec"; exit 1
	fi
	rm -f "$SB/trip"
	(
		cd "$SB/cwd" || exit 99
		env HOME="$SB/home" XDG_DATA_HOME="$SB/xdg" APPDATA="$SB/appdata" \
			TYRIAN2000_DATA= \
			PATH="$SB/stubs:$PATH" SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
			STUB_LOG="$SB/curl.log" STUB_PID="$SB/stub.pid" STUB_TRIP="$SB/trip" STUB_MODE="${STUB_MODE:-}" STUB_ZIP="${STUB_ZIP:-}" \
			"$bin" "${args[@]}"
	) > "$LOG" 2>&1
	RC=$?
	check
	[ ! -e "$SB/trip" ] || fail "an unexpected downloader (curl from PATH?) was started"
}

expect_rc() { check; [ "$RC" -eq "$1" ] || fail "expected exit $1, got $RC"; }
expect_log() { check; grep -Fq -- "$1" "$LOG" || fail "log lacks: $1"; }
# a path in the log: the game logs Windows paths (any mix of slashes, any drive-letter case)
expect_log_path() { check; tr '\\' / < "$LOG" | grep -Fqi -- "$(native "$1")" || fail "log lacks the path: $(native "$1")"; }
expect_no_log() { check; ! grep -Fq -- "$1" "$LOG" || fail "log unexpectedly has: $1"; }
expect_not_installed() { check; [ ! -e "$INSTALL" ] || fail "something was installed at $INSTALL"; }
expect_clean() {
	check
	local left
	left=$(ls -A "$PARENT" 2>/dev/null | grep -E '\.part$|\.tmp$|\.old$' || true)
	[ -z "$left" ] || fail "temporary files left in $PARENT: $left"
}
expect_files_match() {  # SPEC: every "size crc name" line is installed and matches
	local spec=$1 size crc name got n=0
	while read -r size crc name; do
		case "$size" in ''|'#'*|archive-*|cancel-*) continue ;; esac
		n=$((n + 1))
		if [ ! -f "$INSTALL/$name" ]; then check; fail "missing installed file $name"; continue; fi
		# shellcheck disable=SC2046
		set -- $(cksum < "$INSTALL/$name")
		check
		[ "$1" = "$crc" ] && [ "$2" = "$size" ] || fail "installed $name differs from the spec"
	done < "$spec"
	check
	[ "$n" -gt 0 ] || fail "spec listed no files"
}
# process_alive PID: the stub's native pid; MSYS kill cannot see native pids
process_alive() {
	[ -n "$1" ] || return 1
	if [ "$WINDOWS" = 1 ]; then tasklist //FI "PID eq $1" //NH 2>/dev/null | grep -Eq "(^|[[:space:]])$1([[:space:]]|$)"
	else kill -0 "$1" 2>/dev/null; fi
}
tree_sum() { (cd "$1" && find . -type f -print0 | sort -z | xargs -0 cksum) 2>/dev/null; }

# --- 1. known-answer tests ---------------------------------------------------------------

new_sandbox selftest
run selftest --install-2000=selftest
expect_rc 0
expect_log "self-test passed"
CASE=missing-cli-value
run missing-value --install-2000
expect_rc 1
expect_log 'is not "download"'
expect_not_installed

# --- 2. the constants and the C manifest match the reference manifest ----------------------

CASE=manifest-consistency
LOG=""
sed -n 's/^	{ \([0-9]*\), \([0-9]*\)UL, "\(.*\)" },$/\1 \2 \3/p' "$ROOT/src/installer_manifest.h" > "$OUT/c-manifest.txt"
awk '$1 !~ /^#/ && NF >= 3 { print $1, $2, $3 }' "$ROOT/test/regress-2000/data-manifest.txt" > "$OUT/txt-manifest.txt"
check; [ -s "$OUT/c-manifest.txt" ] || fail "no entries parsed from src/installer_manifest.h"
check; cmp -s "$OUT/c-manifest.txt" "$OUT/txt-manifest.txt" || fail "src/installer_manifest.h differs from test/regress-2000/data-manifest.txt"
check; grep -Fq '#define INSTALLER_EXPECTED_SIZE 5051363u' "$ROOT/src/installer.h" || fail "INSTALLER_EXPECTED_SIZE changed"
check; grep -Fq '"348bc76e73514e452279b8730cf217daf0f70a282f07b6b94af653d87e921667"' "$ROOT/src/installer.h" || fail "INSTALLER_EXPECTED_SHA256 changed"
check; grep -Fq "\"$URL\"" "$ROOT/src/installer.h" || fail "INSTALLER_DOWNLOAD_URL changed"

# --- 3. clean install, detection, reinstall ---------------------------------------------------

new_sandbox clean-install
run detect-before --install-2000=detect
expect_rc 1
expect_log "Tyrian 2000 is not installed"
expect_log_path "$INSTALL"                 # the documented per-user location
run install --install-2000="$FIX/valid.zip" --install-2000-spec="$FIX/valid.spec"
expect_rc 0
expect_log "Tyrian 2000 data installed."
expect_no_log "non-canonical"
expect_files_match "$FIX/valid.spec"
expect_clean
check; [ ! -e "$INSTALL/tyrian2.exe" ] && [ ! -e "$INSTALL/readme.txt" ] || fail "files the engine never reads were unpacked"
CASE=existing-install
run detect-after --install-2000=detect
expect_rc 0
expect_log "Tyrian 2000 data found in"
BEFORE=$(tree_sum "$INSTALL")
CASE=reinstall
run reinstall --install-2000="$FIX/valid.zip" --install-2000-spec="$FIX/valid.spec"
expect_rc 0
expect_clean
expect_files_match "$FIX/valid.spec"

# Recover a process interruption between the old/install and temp/install
# renames. Even a failed next job must preserve the complete previous data.
CASE=swap-recovery
mv "$INSTALL" "$PARENT/data-tyrian2000.old"
mkdir "$PARENT/data-tyrian2000.tmp"
echo interrupted > "$PARENT/data-tyrian2000.tmp/incomplete"
run recovery --install-2000="$FIX/valid.zip" --install-2000-spec="$FIX/wrong-sha.spec"
expect_rc 1
expect_log "$VERIFY_MESSAGE"
expect_clean
check; [ "$(tree_sum "$INSTALL")" = "$BEFORE" ] || fail "the interrupted swap did not restore the previous install"

# Cleanup must unlink a stale staging symlink without following it.
CASE=staging-symlink
mkdir -p "$SB/protected"
echo keep > "$SB/protected/keep.txt"
ln -s "$SB/protected" "$PARENT/data-tyrian2000.tmp"
run staging-symlink --install-2000="$FIX/valid.zip" --install-2000-spec="$FIX/valid.spec"
expect_rc 0
expect_clean
check; [ "$(cat "$SB/protected/keep.txt")" = keep ] || fail "staging cleanup followed a symlink"

# Check the manual source before clearing staging, including relative aliases.
CASE=staging-source
mkdir "$PARENT/data-tyrian2000.tmp"
cp "$FIX/valid.zip" "$PARENT/data-tyrian2000.tmp/input.zip"
run staging-source --install-2000="$PARENT/data-tyrian2000.tmp/../data-tyrian2000.tmp/input.zip" --install-2000-spec="$FIX/valid.spec"
expect_rc 1
expect_log "installer's own temporary space"
check; cmp -s "$FIX/valid.zip" "$PARENT/data-tyrian2000.tmp/input.zip" || fail "the chosen source was modified"
rm -rf "$PARENT/data-tyrian2000.tmp"

# --- 4. bad archives are rejected and never touch an existing install ---------------------------

CASE=corrupt-zip
run badcrc --install-2000="$FIX/badcrc.zip" --install-2000-spec="$FIX/badcrc.spec"
expect_rc 1
expect_log "damaged"
expect_clean
check; [ "$(tree_sum "$INSTALL")" = "$BEFORE" ] || fail "a rejected zip changed the existing install"
for bad in truncated garbage badcrc-ignored; do
	CASE="$bad-zip"
	run "$bad" --install-2000="$FIX/$bad.zip" --install-2000-spec="$FIX/$bad.spec"
	expect_rc 1
	if [ "$bad" = badcrc-ignored ]; then expect_log "readme.txt failed its check"; else expect_log "not a valid zip"; fi
	expect_clean
	check; [ "$(tree_sum "$INSTALL")" = "$BEFORE" ] || fail "a rejected zip changed the existing install"
done
CASE=oversize-zip
run oversize --install-2000="$FIX/oversize.zip" --install-2000-spec="$FIX/oversize.spec"
expect_rc 1
expect_log "far too large"
expect_clean
check; [ "$(tree_sum "$INSTALL")" = "$BEFORE" ] || fail "an oversized entry changed the existing install"
for unsafe in dotdot absolute symlink duplicate backslash nul; do
	CASE="unsafe-$unsafe"
	run "unsafe-$unsafe" --install-2000="$FIX/unsafe-$unsafe.zip" --install-2000-spec="$FIX/unsafe-$unsafe.spec"
	expect_rc 1
	if [ "$unsafe" = nul ]; then expect_log "not a valid zip"; else expect_log "not safe to unpack"; fi
	expect_clean
	check; [ "$(tree_sum "$INSTALL")" = "$BEFORE" ] || fail "a rejected zip changed the existing install"
	check; [ -z "$(find "$SB" -name 'evil*' 2>/dev/null)" ] && [ ! -e /tmp/tyrian2000-installer-evil.txt ] || fail "an unsafe entry was written"
done
for shape in second-top root-file; do
	CASE="unsafe-$shape"
	run "unsafe-$shape" --install-2000="$FIX/unsafe-$shape.zip" --install-2000-spec="$FIX/unsafe-$shape.spec"
	expect_rc 1
	expect_log "top-level folder"
	expect_clean
	check; [ "$(tree_sum "$INSTALL")" = "$BEFORE" ] || fail "a rejected zip changed the existing install"
done

# --- 5. download: interruption and retry, URL, verification ---------------------------------------

new_sandbox interrupted-download
STUB_ZIP="$FIX/valid.zip" STUB_MODE=partial
run download-interrupted --install-2000=download --install-2000-spec="$FIX/valid.spec"
expect_rc 1
expect_log "The download failed"
expect_not_installed
expect_clean
STUB_MODE=serve
run download-retry --install-2000=download --install-2000-spec="$FIX/valid.spec"
expect_rc 0
expect_files_match "$FIX/valid.spec"
expect_clean
CASE=download-command
check; grep -Fq -- "$URL" "$SB/curl.log" || fail "curl was not given $URL"
check; grep -Fq -- "--proto =https" "$SB/curl.log" || fail "curl was not restricted to https"
check; tr '\\' / < "$SB/curl.log" | grep -Fqi -- "--output $(native "$PARENT")/tyrian2000.zip.part" || fail "the download did not go to a temp file beside the install location"

new_sandbox wrong-sha
STUB_ZIP="$FIX/valid.zip" STUB_MODE=serve
run wrong-sha --install-2000=download --install-2000-spec="$FIX/wrong-sha.spec"
expect_rc 1
expect_log "$VERIFY_MESSAGE"
expect_not_installed
expect_clean
CASE=wrong-size
run wrong-size --install-2000=download --install-2000-spec="$FIX/download-only.spec"
expect_rc 1
expect_log "$VERIFY_MESSAGE"
expect_not_installed
expect_clean
CASE=corrupt-download
STUB_ZIP="$FIX/badcrc.zip"
run corrupt-download --install-2000=download --install-2000-spec="$FIX/badcrc.spec"
expect_rc 1
expect_log "$VERIFY_MESSAGE"
expect_not_installed
expect_clean
CASE=manual-wrong-sha
run manual-wrong-sha --install-2000="$FIX/valid.zip" --install-2000-spec="$FIX/wrong-sha.spec"
expect_rc 1
expect_log "$VERIFY_MESSAGE"
expect_not_installed
expect_clean
CASE=http-error
STUB_MODE=fail22
run http-error --install-2000=download --install-2000-spec="$FIX/valid.spec"
expect_rc 1
expect_log "refused the request"
expect_not_installed
expect_clean

new_sandbox cancel
STUB_ZIP="$FIX/valid.zip" STUB_MODE=slow
run cancel --install-2000=download --install-2000-spec="$FIX/cancel.spec"
expect_rc 1
expect_log "Installation cancelled. Nothing was installed."
expect_not_installed
expect_clean
check; ! process_alive "$(cat "$SB/stub.pid" 2>/dev/null)" || fail "curl was left running after the cancel"

new_sandbox concurrent
STUB_ZIP="$FIX/valid.zip" STUB_MODE=slow
sed 's/cancel-after-ms 700/cancel-after-ms 2500/' "$FIX/cancel.spec" > "$SB/long-cancel.spec"
run first --install-2000=download --install-2000-spec="$SB/long-cancel.spec" &
first_pid=$!
for ((attempt=0; attempt<60; attempt++)); do
	[ -f "$SB/stub.pid" ] && break
	sleep 0.05
done
run second --install-2000="$FIX/valid.zip" --install-2000-spec="$FIX/valid.spec"
expect_rc 1
expect_log "install location is busy"
wait "$first_pid"
expect_not_installed
expect_clean

new_sandbox no-curl
STUB_ZIP="$FIX/valid.zip" STUB_MODE=serve STUB_DOWNLOADER="$(native "$SB/empty")/no-such-curl$EXE"
run no-curl --install-2000=download --install-2000-spec="$FIX/valid.spec"
expect_rc 1
expect_log "curl program, which was not found"
expect_log "Install the data manually"
expect_not_installed
expect_clean
unset STUB_DOWNLOADER

# --- 6. manual install from folders ---------------------------------------------------------------------

new_sandbox folder-plain
run plain --install-2000="$FIX/folder-plain" --install-2000-spec="$FIX/folders.spec"
expect_rc 0
expect_no_log "non-canonical"
expect_files_match "$FIX/folders.spec"
expect_clean
check; [ "$(ls "$FIX/folder-plain" | wc -l | tr -d ' ')" -eq 24 ] || fail "the source folder was modified"

# Read-only source files (CD, GOG, read-only share): installing twice must still
# be able to remove the first install's backup (Windows keeps the attribute).
new_sandbox folder-readonly
cp -R "$FIX/folder-plain" "$SB/ro-source"
chmod a-w "$SB"/ro-source/*
run readonly-first --install-2000="$SB/ro-source" --install-2000-spec="$FIX/folders.spec"
expect_rc 0
run readonly-second --install-2000="$SB/ro-source" --install-2000-spec="$FIX/folders.spec"
expect_rc 0
expect_files_match "$FIX/folders.spec"
expect_clean
chmod -R u+w "$SB/ro-source"

new_sandbox folder-gog
run gog --install-2000="$FIX/folder-gog" --install-2000-spec="$FIX/folders.spec"
expect_rc 0
expect_log "Tyrian 2000 data found in"       # located below the chosen folder
expect_files_match "$FIX/folders.spec"          # upper-case names came out canonical
check; [ -f "$FIX/folder-gog/Tyrian 2000/game/TYRIAN1.LVL" ] || fail "the source folder was modified"

new_sandbox folder-noncanonical
run noncanonical --install-2000="$FIX/folder-noncanonical" --install-2000-spec="$FIX/folders.spec"
expect_rc 0
expect_log "tyrian.cdt differs from the original release"
expect_log "non-canonical"
check; [ -f "$INSTALL/tyrian1.lvl" ] || fail "the non-canonical folder was not installed"

new_sandbox folder-incomplete
run incomplete --install-2000="$FIX/folder-incomplete" --install-2000-spec="$FIX/folders.spec"
expect_rc 1
expect_log "levels3.dat is missing"
expect_not_installed
expect_clean

new_sandbox folder-empty
run empty --install-2000="$FIX/folder-empty" --install-2000-spec="$FIX/folders.spec"
expect_rc 1
expect_log "No Tyrian 2000 data files were found"
expect_not_installed
CASE=folder-missing
run missing --install-2000="$SB/does-not-exist"
expect_rc 1
expect_log "is not \"download\""

# --- 7. 2.1 data given as Tyrian 2000 ----------------------------------------------------------------------

new_sandbox wrong-variant
run v21-synthetic --install-2000="$FIX/folder-v21" --install-2000-spec="$FIX/folders.spec"
expect_rc 1
expect_log "The Tyrian v2.0/v2.1 data files were found.  Tyrian 2000 requires the Tyrian 2000 data files."
expect_not_installed
expect_clean
if [ -n "$DATA21" ] && [ -f "$DATA21/tyrian.shp" ]; then
	CASE=wrong-variant-real-21
	run v21-real --install-2000="$DATA21"
	expect_rc 1
	expect_log "The Tyrian v2.0/v2.1 data files were found."
	expect_not_installed
	expect_clean
fi

# --- 8. portable mode ------------------------------------------------------------------------------------------

new_sandbox portable
mkdir -p "$SB/portable"
cp "$SOURCE_BIN" "$SB/portable/opentyrian"
: > "$SB/portable/opentyrian.cfg"
BIN_OVERRIDE="$SB/portable/opentyrian"
run portable --install-2000="$FIX/valid.zip" --install-2000-spec="$FIX/valid.spec"
unset BIN_OVERRIDE
expect_rc 0
check; [ -f "$SB/portable/data-tyrian2000/tyrian1.lvl" ] || fail "portable mode did not install beside the executable"
check; [ ! -e "$INSTALL" ] || fail "portable mode also wrote to the per-user location"
expect_log_path "$SB/portable/data-tyrian2000"

# --- 9. the launcher's install flow (no window: --launcher-flow drives the screen's controller) ---------------

new_sandbox launcher-download
STUB_ZIP="$FIX/valid.zip" STUB_MODE=serve
run flow-download --launcher-flow=download --install-2000-spec="$FIX/valid.spec"
expect_rc 0
expect_log 'overlay=result title="TYRIAN 2000 INSTALLED" installed=1'
expect_files_match "$FIX/valid.spec"
expect_clean

new_sandbox launcher-no-curl
STUB_ZIP="$FIX/valid.zip" STUB_MODE=serve STUB_DOWNLOADER="$(native "$SB/empty")/no-such-curl$EXE"
run flow-no-curl --launcher-flow=download --install-2000-spec="$FIX/valid.spec"
unset STUB_DOWNLOADER
expect_rc 1
expect_log 'overlay=result title="CANNOT DOWNLOAD" installed=0'
expect_log "You can still install from a .zip file or an existing folder."
expect_not_installed
expect_clean

new_sandbox launcher-download-failed
STUB_ZIP="$FIX/valid.zip" STUB_MODE=fail22
run flow-network --launcher-flow=download --install-2000-spec="$FIX/valid.spec"
expect_rc 1
expect_log 'overlay=result title="INSTALL FAILED" installed=0'
expect_not_installed
expect_clean

new_sandbox launcher-zip
run flow-zip --launcher-flow="zip=$(native "$FIX/valid.zip")" --install-2000-spec="$FIX/valid.spec"
expect_rc 0
expect_log 'title="TYRIAN 2000 INSTALLED" installed=1'
expect_files_match "$FIX/valid.spec"
expect_clean

new_sandbox launcher-folder
run flow-folder --launcher-flow="folder=$(native "$FIX/folder-gog")" --install-2000-spec="$FIX/folders.spec"
expect_rc 0
expect_log 'title="TYRIAN 2000 INSTALLED" installed=1'
expect_files_match "$FIX/folders.spec"
expect_clean

# The file dialog's callback, fed as SDL does: a path, a cancel, a failure.
new_sandbox launcher-picker
run flow-picker-zip --launcher-flow="picker-zip=$(native "$FIX/valid.zip")" --install-2000-spec="$FIX/valid.spec"
expect_rc 0
expect_log 'title="TYRIAN 2000 INSTALLED" installed=1'
new_sandbox launcher-picker-folder
run flow-picker-folder --launcher-flow="picker-folder=$(native "$FIX/folder-plain")" --install-2000-spec="$FIX/folders.spec"
expect_rc 0
expect_log 'title="TYRIAN 2000 INSTALLED" installed=1'
new_sandbox launcher-picker-cancel
run flow-picker-cancel --launcher-flow=picker-cancel
expect_rc 0
expect_log "overlay=install"
expect_not_installed
new_sandbox launcher-picker-error
run flow-picker-error --launcher-flow=picker-error
expect_rc 0
expect_log "overlay=manual"
expect_log "The file dialog could not be opened here."
expect_not_installed

# No dialog at all: a zip placed where the screen says is found by LOOK NOW.
new_sandbox launcher-scan
run flow-scan-empty --launcher-flow=scan --install-2000-spec="$FIX/valid.spec"
expect_rc 1
expect_log "overlay=manual"
expect_log "Nothing found yet in those places."
expect_not_installed
mkdir -p "$PARENT"
cp "$FIX/valid.zip" "$PARENT/tyrian2000.zip"
run flow-scan --launcher-flow=scan --install-2000-spec="$FIX/valid.spec"
expect_rc 0
expect_log 'title="TYRIAN 2000 INSTALLED" installed=1'
expect_files_match "$FIX/valid.spec"
check; cmp -s "$FIX/valid.zip" "$PARENT/tyrian2000.zip" || fail "the placed zip was modified"

echo "installer: $checks checks, $failures failures"
[ "$failures" -eq 0 ]
