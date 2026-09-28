#!/bin/sh
# check_rng_order.sh - fail if one C full-expression calls an RNG function two
# or more times.
#
# C leaves the relative order of two side-effecting calls in one expression
# unspecified (function arguments, the operands of +, a braced initializer, ...),
# so clang and gcc are free to draw the random numbers in different orders and
# the rendered frame diverges.  This class of bug already broke CI once
# (see .worker-reports/vfx-fix2.md item D); this guard keeps it from coming
# back.  Every site must draw each number in its own statement.
#
# It is intentionally simple and portable: POSIX awk, no python.  It strips
# comments and literals, splits the translation unit at expression boundaries
# (`;`, `{`, `}`, and the sequence-point operators `&&`, `||`, `?`, `:`), and
# counts RNG calls in each resulting expression.  See tools/regress.sh, which
# runs it before the cases.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Collect the sources explicitly so a bare glob cannot be passed through to awk
# as a literal path when (say) src/ has no headers.
FILES=$(find "$ROOT/src" -maxdepth 1 -type f \( -name '*.c' -o -name '*.h' \) -print)
if [ -z "$FILES" ]; then
	echo "check_rng_order: no source files found under $ROOT/src" >&2
	exit 1
fi

# shellcheck disable=SC2016
awk '
function rngcount(s,   n) {
	# Longest names first so vfx_rand_range() is not counted as vfx_rand().
	n = 0
	while (match(s, /(mt_rand_1|mt_rand_lt1|mt_rand|vfx_rand_range|vfx_rand_bipolar|vfx_rand)[ \t]*\(/)) {
		n++
		s = substr(s, RSTART + RLENGTH)
	}
	return n
}
function check() {
	if (buf != "") {
		c = rngcount(buf)
		if (c >= 2) {
			printf "%s:%d: %d RNG calls in one expression (unspecified order): %s\n", \
			       FILENAME, bufline, c, buf > "/dev/stderr"
			bad = 1
		}
	}
	buf = ""
	bufline = 0
}
BEGIN { bad = 0 }
{
	line = $0
	n = length(line)
	i = 1
	while (i <= n) {
		c = substr(line, i, 1)
		d = substr(line, i, 2)
		if (in_block) { if (d == "*/") { in_block = 0; i += 2; continue } i++; continue }
		if (in_line)  { i++; continue }            # ends at the physical newline
		if (in_str)   { if (c == "\\") { i += 2; continue } if (c == "\"") in_str = 0; i++; continue }
		if (in_chr)   { if (c == "\\") { i += 2; continue } if (c == "'"'"'") in_chr = 0; i++; continue }
		if (d == "/*") { in_block = 1; i += 2; continue }
		if (d == "//") { in_line = 1; i++; continue }
		if (c == "\"") { in_str = 1; i++; continue }
		if (c == "'"'"'") { in_chr = 1; i++; continue }
		if (c == ";" || c == "{" || c == "}") { check(); i++; continue }
		if (d == "&&" || d == "||")            { check(); i += 2; continue }
		if (c == "?" || c == ":")              { check(); i++; continue }
		if (buf == "") bufline = FNR
		buf = buf c
		i++
	}
	in_line = 0
}
END {
	check()
	exit (bad ? 1 : 0)
}
' $FILES
