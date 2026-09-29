#!/bin/bash
# fetch_t2000_data.sh — download, verify and stage the Tyrian 2000 data for CI.
#
#   tools/fetch_t2000_data.sh DEST      e.g. DEST="$RUNNER_TEMP/tyrian2000"
#
# The Tyrian 2000 data is never stored in this repository, never cached and never
# published: the CI downloads it from the author's site in every run, into a
# directory outside the checkout, and throws it away with the runner.  This is
# not the player-facing installer; it is the acquisition step of
# `make regress-2000 TYRIAN2000_DATA=DEST`.
#
# The script:
#   1. refuses a DEST inside a git work tree (or this checkout);
#   2. downloads the zip to a temporary file (timeout, retries);
#   3. requires the exact size and SHA-256 of the canonical archive;
#   4. lists the archive first and rejects absolute paths, "..", backslashes,
#      symlinks, duplicate names, any layout but one top directory holding 99
#      regular files, and a total size other than the expected one;
#   5. extracts it (unzip checks every CRC) into a temporary directory next to DEST
#      and verifies test/regress-2000/data-manifest.txt with the size/cksum
#      logic of tools/regress-2000.sh;
#   6. renames the verified directory to DEST.  Nothing partial is ever left at
#      DEST: any failure removes the temporary files and exits non-zero.
#
# A failed download or verification is an error, never a skipped suite.
#
# TYRIAN2000_URL overrides the source for offline tests of this script (the size
# and hash checks still apply, so it cannot substitute other data).
set -euo pipefail

URL="${TYRIAN2000_URL:-https://www.camanis.net/tyrian/tyrian2000.zip}"
EXPECTED_SIZE=5051363
EXPECTED_SHA256=348bc76e73514e452279b8730cf217daf0f70a282f07b6b94af653d87e921667
TOP=tyrian2000
EXPECTED_FILES=99
EXPECTED_ENTRIES=100            # the top directory plus the files
EXPECTED_UNCOMPRESSED=12337252  # bytes of all files once extracted
ATTEMPTS=3

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT/test/regress-2000/data-manifest.txt"

die() { echo "ERROR: fetch_t2000_data: $*" >&2; exit 1; }

case "${1:-}" in
	-h|--help)
		echo "Usage: tools/fetch_t2000_data.sh DEST   (DEST must be outside the git work tree)"
		exit 0 ;;
esac
[ "$#" -eq 1 ] && [ -n "$1" ] || { echo "Usage: tools/fetch_t2000_data.sh DEST" >&2; exit 2; }
DEST=$1

for tool in curl unzip awk find cksum wc sort; do
	command -v "$tool" >/dev/null 2>&1 || die "required tool not found: $tool"
done
if command -v sha256sum >/dev/null 2>&1; then
	sha256_of() { sha256sum "$1" | awk '{ print $1 }'; }
elif command -v shasum >/dev/null 2>&1; then
	sha256_of() { shasum -a 256 "$1" | awk '{ print $1 }'; }
else
	die "neither sha256sum nor shasum is available"
fi
[ -f "$MANIFEST" ] || die "missing $MANIFEST"

# --- DEST: absolute, outside every git work tree ------------------------------
# MSYS2 hands over Windows paths such as D:\a\_temp in $RUNNER_TEMP.
case "$DEST" in
	[A-Za-z]:[\\/]*) command -v cygpath >/dev/null 2>&1 && DEST=$(cygpath -u "$DEST") ;;
esac
case "$DEST" in /*) ;; *) DEST="$PWD/$DEST" ;; esac
while [ "${DEST%/}" != "$DEST" ] && [ "$DEST" != "/" ]; do DEST=${DEST%/}; done
[ "$DEST" != "/" ] || die "DEST must not be the filesystem root"

anc=$DEST
while [ ! -d "$anc" ]; do anc=$(dirname "$anc"); done
anc_real=$(cd "$anc" && pwd -P)
root_real=$(cd "$ROOT" && pwd -P)
case "$anc_real/" in
	"$root_real"/*) die "DEST ($DEST) is inside this checkout ($root_real); use a directory outside it, e.g. \$RUNNER_TEMP/tyrian2000" ;;
esac
if command -v git >/dev/null 2>&1 && [ "$(git -C "$anc_real" rev-parse --is-inside-work-tree 2>/dev/null || true)" = "true" ]; then
	die "DEST ($DEST) is inside a git work tree; the Tyrian 2000 data must never be inside a checkout"
fi

if [ -e "$DEST" ]; then
	# An empty directory is fine; anything else is left alone, not overwritten.
	[ -d "$DEST" ] && [ -z "$(find "$DEST" -mindepth 1 -maxdepth 1 -print -quit)" ] \
		|| die "DEST ($DEST) already exists and is not an empty directory; remove it first"
fi
PARENT=$(dirname "$DEST")
mkdir -p "$PARENT"

# The temporary directory is a sibling of DEST, so the final rename never crosses
# a filesystem and is atomic.
WORK=$(mktemp -d "$PARENT/.t2000-fetch.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
ZIP="$WORK/tyrian2000.zip"

file_size() { wc -c < "$1" | tr -d '[:space:]'; }

# --- download + size/hash -----------------------------------------------------
fetched=0
attempt=1
while [ "$attempt" -le "$ATTEMPTS" ]; do
	echo "Downloading the Tyrian 2000 archive (attempt $attempt of $ATTEMPTS) ..."
	rm -f "$ZIP"
	if curl --fail --silent --show-error --location \
		--proto '=https,file' --proto-redir '=https' \
		--connect-timeout 20 --max-time 300 --max-filesize 8000000 \
		--retry 3 --retry-delay 5 --retry-connrefused \
		--output "$ZIP" "$URL"; then
		size=$(file_size "$ZIP")
		if [ "$size" = "$EXPECTED_SIZE" ]; then
			sha=$(sha256_of "$ZIP")
			if [ "$sha" = "$EXPECTED_SHA256" ]; then
				fetched=1
				break
			fi
			echo "  SHA-256 mismatch: got $sha" >&2
		else
			echo "  size mismatch: got $size bytes, expected $EXPECTED_SIZE" >&2
		fi
	else
		echo "  download failed" >&2
	fi
	attempt=$((attempt + 1))
	[ "$attempt" -le "$ATTEMPTS" ] && sleep 5
done
[ "$fetched" -eq 1 ] || die "could not obtain the canonical archive ($EXPECTED_SIZE bytes, SHA-256 $EXPECTED_SHA256)"
echo "Archive verified: $EXPECTED_SIZE bytes, SHA-256 $EXPECTED_SHA256"

# --- inspect the archive before extracting anything ---------------------------
# Names only, one per line (unzip -Z1); a name with a newline cannot survive this,
# and the entry count below would not match.
names="$WORK/names.txt"
unzip -Z1 "$ZIP" > "$names" || die "cannot list the archive"
[ "$(wc -l < "$names" | tr -d '[:space:]')" -eq "$EXPECTED_ENTRIES" ] || die "the archive does not have $EXPECTED_ENTRIES entries"

bad=$(LC_ALL=C awk -v top="$TOP" '
	$0 == top "/" { dirs++; next }
	{
		if (index($0, top "/") != 1) { print "outside " top "/: " $0; next }
		n = substr($0, length(top) + 2)
		if (n == "" || n == "." || n == ".." || n ~ /[\/\\:]/ || n ~ /[[:cntrl:]]/)
			print "unsafe entry name: " $0
	}
	END { if (dirs != 1) print "expected exactly one top directory entry, found " dirs + 0 }
' "$names")
[ -z "$bad" ] || die "rejected archive: $bad"
dup=$(LC_ALL=C sort -f "$names" | uniq -di | head -n 1 || true)
[ -z "$dup" ] || die "rejected archive: duplicate entry name (ignoring case): $dup"

# Types and sizes (unzip -Z): only one directory and regular files, of the expected total size.
listing="$WORK/listing.txt"
unzip -Z "$ZIP" | LC_ALL=C awk '$1 ~ /^[-dlcbps?][-rwxsStT]+$/' > "$listing"
kinds=$(awk '{ print substr($1, 1, 1) }' "$listing" | sort | uniq -c | awk '{ printf "%s%s%s", sep, $2, "=" $1; sep = " " }')
[ "$kinds" = "-=$EXPECTED_FILES d=1" ] || die "rejected archive: entry types are '$kinds' (only $EXPECTED_FILES regular files and one directory are allowed; no symlinks)"
total=$(awk '{ t += $4 } END { printf "%d", t }' "$listing")
[ "$total" -eq "$EXPECTED_UNCOMPRESSED" ] || die "rejected archive: uncompressed size is $total, expected $EXPECTED_UNCOMPRESSED"

# --- extract into a temporary directory (unzip verifies every CRC) --------------
STAGE="$WORK/stage"
mkdir "$STAGE"
unzip -q "$ZIP" -d "$STAGE" || die "extraction failed (corrupt archive?)"
rm -f "$ZIP"

[ -z "$(find "$STAGE" -type l -print -quit)" ] || die "extraction produced a symlink"
[ "$(find "$STAGE" -mindepth 1 -maxdepth 1 | wc -l | tr -d '[:space:]')" -eq 1 ] && [ -d "$STAGE/$TOP" ] \
	|| die "extraction did not produce exactly one top directory '$TOP'"
[ -z "$(find "$STAGE/$TOP" -mindepth 1 ! -type f -print -quit)" ] || die "extraction produced something that is not a regular file"
[ "$(find "$STAGE/$TOP" -type f | wc -l | tr -d '[:space:]')" -eq "$EXPECTED_FILES" ] || die "extraction did not produce $EXPECTED_FILES files"

# --- data lock: size and POSIX cksum CRC of every runtime file ------------------
# Same check as tools/regress-2000.sh.
bad_data=0
while read -r size crc name; do
	size=${size%$'\r'}; crc=${crc%$'\r'}; name=${name%$'\r'}
	case "$size" in ''|'#'*) continue ;; esac
	if [ ! -f "$STAGE/$TOP/$name" ]; then
		echo "  missing: $name (expected size $size, crc $crc)" >&2
		bad_data=$((bad_data + 1))
		continue
	fi
	# shellcheck disable=SC2046
	set -- $(cksum "$STAGE/$TOP/$name")
	if [ "$2" != "$size" ] || [ "$1" != "$crc" ]; then
		echo "  mismatched: $name (expected size $size, crc $crc; got size $2, crc $1)" >&2
		bad_data=$((bad_data + 1))
	fi
done < "$MANIFEST"
[ "$bad_data" -eq 0 ] || die "$bad_data file(s) differ from test/regress-2000/data-manifest.txt"

# --- atomic publish -----------------------------------------------------------
[ ! -d "$DEST" ] || rmdir "$DEST"
mv "$STAGE/$TOP" "$DEST"
echo "OK: Tyrian 2000 data verified in $DEST ($EXPECTED_FILES files, manifest matches)"
