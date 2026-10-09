#!/bin/bash
# CI-safe guard: no Tyrian 2000 data file may land in the source tree.
#
# Compares the size and POSIX cksum CRC of every tracked or unignored file with
# the Tyrian 2000 manifest (test/regress-2000/data-manifest.txt).  Files whose
# bytes are identical to the approved Tyrian 2.1 freeware data are listed in
# test/regress-2000/shared-with-2.1.txt and are not counted: they already ship
# legally with 2.1 (the archive contains many of them).  Only metadata is read
# from the manifests; the tool never needs any game data.
#
#   tools/check_no_t2000_data.sh              check the working tree
#   tools/check_no_t2000_data.sh --self-test  prove the check with synthetic files
set -eu

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# check_tree TREE MANIFEST SHARED: prints offenders, returns 1 when there is one.
check_tree() {
	local tree=$1 manifest=$2 shared=$3 list files file size crc found=0 status=0
	list=$(mktemp "${TMPDIR:-/tmp}/t2000-list.XXXXXX")
	files=$(mktemp "${TMPDIR:-/tmp}/t2000-files.XXXXXX")
	# "size crc name" of every manifest entry that is not shared with 2.1.
	awk 'FILENAME == ARGV[1] { if ($1 !~ /^#/ && NF >= 3) shared[$1 " " $2] = 1; next }
	     $1 !~ /^#/ && NF >= 3 && !(($1 " " $2) in shared) { print }' \
		"$shared" "$manifest" > "$list"

	if git -C "$tree" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
		# Tracked and untracked-but-not-ignored files; ./data and build output are ignored.
		git -C "$tree" ls-files -z -co --exclude-standard > "$files"
	else
		(cd "$tree" && find . -type f -not -path './.git/*' -not -path './data/*' -not -path './obj/*' -print0) > "$files"
	fi

	while IFS= read -r -d '' file; do
		file=${file#./}
		[ -f "$tree/$file" ] || continue
		size=$(wc -c < "$tree/$file")
		size=${size//[[:space:]]/}
		# Cheap filter first: only files of a manifest size are checksummed.
		awk -v s="$size" '$1 == s { found = 1 } END { exit !found }' "$list" || continue
		# shellcheck disable=SC2046
		set -- $(cksum < "$tree/$file")
		crc=$1
		if found=$(awk -v s="$size" -v c="$crc" '$1 == s && $2 == c { print $3; exit }' "$list") && [ -n "$found" ]; then
			echo "FAIL no-t2000-data: $file matches the Tyrian 2000 file '$found'"
			status=1
		fi
	done < "$files"
	rm -f "$list" "$files"
	return "$status"
}

self_test() {
	local out
	dir=$(mktemp -d "${TMPDIR:-/tmp}/t2000-selftest.XXXXXX")
	trap 'rm -rf "$dir"' EXIT
	mkdir -p "$dir/tree/sub"
	printf 'synthetic non-game bytes' > "$dir/tree/sub/plain.bin"
	printf 'other synthetic bytes' > "$dir/tree/renamed.dat"
	# A manifest that lists both files, and one that shares the second.
	# shellcheck disable=SC2046
	set -- $(cksum < "$dir/tree/sub/plain.bin"); printf '%s %s only.dat\n' "$2" "$1" > "$dir/manifest"
	# shellcheck disable=SC2046
	set -- $(cksum < "$dir/tree/renamed.dat"); printf '%s %s both.dat\n' "$2" "$1" >> "$dir/manifest"
	printf '%s %s both.dat\n' "$2" "$1" > "$dir/shared"

	if ! out=$(check_tree "$dir/tree" "$dir/manifest" "$dir/shared") && [ "$out" = "FAIL no-t2000-data: sub/plain.bin matches the Tyrian 2000 file 'only.dat'" ]; then
		:
	else
		echo "FAIL no-t2000-data: self-test did not flag a renamed 2000 file (got: $out)"
		return 1
	fi
	: > "$dir/shared"
	if check_tree "$dir/tree" "$dir/manifest" "$dir/shared" >/dev/null; then
		echo "FAIL no-t2000-data: self-test did not flag the file once it stopped being shared"
		return 1
	fi
	echo "PASS no-t2000-data self-test: 2000 files are found under any name; shared files are exempt"
}

if [ "${1:-}" = "--self-test" ]; then
	self_test
	exit
fi

MANIFEST="$ROOT/test/regress-2000/data-manifest.txt"
SHARED="$ROOT/test/regress-2000/shared-with-2.1.txt"
[ -f "$MANIFEST" ] && [ -f "$SHARED" ] || { echo "ERROR: missing Tyrian 2000 manifests in test/regress-2000"; exit 1; }
self_test
if check_tree "$ROOT" "$MANIFEST" "$SHARED"; then
	echo "PASS no-t2000-data: no Tyrian 2000 file in the tree ($(grep -vc '^#' "$MANIFEST") manifest entries, shared 2.1 files exempt)"
else
	echo "ERROR: Tyrian 2000 data must never be committed (see docs/MODERNIZATION.md, section 2)"
	exit 1
fi
