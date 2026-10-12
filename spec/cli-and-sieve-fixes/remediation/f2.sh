#!/bin/sh
# P3 remediation gate (review cycle 1, F2): make check pins that --help states the five spellings of
# a valued option once. The tree as committed is green and its help gives the sentence once; with
# the sentence given twice in src/main.hpp's help, make check goes red with a failed check, both
# programs built (R1, R10).
set -u
. "$(dirname "$0")/../verify/common.sh"

build_all
n=$("$work/tree-clang++/bin/mps" --help 2>&1 | grep -c 'Give a valued option')
test "$n" -eq 1 || fail "the help gives the spellings sentence $n times, not once (R1)"

d="$work/twice"
copy_tree "$d"
awk '
	/Give a valued option/ && !done {
		first = $0
		if ((getline second) > 0) {
			plain = second
			sub(/\)HELP";$/, "", plain)
			print first; print plain; print first; print second
			done = 1
			next
		}
	}
	{ print }
' "$d/src/main.hpp" >"$d/main.hpp.new" && mv "$d/main.hpp.new" "$d/src/main.hpp" || fail "rewrite of the help"
s=0
( cd "$d" && "$MAKE" -B CXX=clang++ check ) >"$logs/twice.log" 2>&1 || s=1
test -x "$d/bin/check" && test -x "$d/bin/mps" || { tail -20 "$logs/twice.log" >&2; fail "the help with the sentence twice did not build"; }
n=$("$d/bin/mps" --help 2>&1 | grep -c 'Give a valued option')
test "$n" -eq 2 || fail "the rewrite gave the sentence $n times, want 2: the gate's own edit is wrong"
test "$s" -eq 1 || fail "make check stays green with the spellings sentence given twice in the help (R1, R10)"
grep -Eq '^(check|cli): [0-9]+ passed, [1-9][0-9]* failed$' "$logs/twice.log" \
	|| { tail -20 "$logs/twice.log" >&2; fail "with the sentence twice, make check went red with no failed check (R10)"; }

echo "F2 remediation gate: green (the help states the spellings once; make check red when it states them twice)"
