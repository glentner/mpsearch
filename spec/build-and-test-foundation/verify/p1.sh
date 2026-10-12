#!/bin/sh
# P1 gate: fresh-copy build with both compilers (CXX honoured), IEEE flags on every compile line and
# after a command-line -ffast-math, warning-free, clean removes the mps build output, <cmath> beside the
# logger, and output byte-identical to origin/main (R1, R2, R5, R6, R9).
set -u
. "$(dirname "$0")/common.sh"
need clang++; need g++-16; need rsync

for cxx in clang++ g++-16; do
	d="$work/tree-$cxx"
	copy_tree "$d"
	( cd "$d" && "$MAKE" CXX="$cxx" >"$logs/build-$cxx.log" 2>&1 ) || { cat "$logs/build-$cxx.log" >&2; fail "$cxx: make failed in a fresh copy (R1, R2)"; }
	test -x "$d/bin/mps" || fail "$cxx: bin/mps missing after make (R2)"
	if grep -q 'warning:' "$logs/build-$cxx.log"; then grep 'warning:' "$logs/build-$cxx.log" >&2; fail "$cxx: warnings (R1)"; fi
	compiled_with "$cxx" "$logs/build-$cxx.log"
	ieee_on_every_compile "$cxx" "$logs/build-$cxx.log" 6

	( cd "$d" && "$MAKE" clean >/dev/null 2>&1 && "$MAKE" CXX="$cxx" CXXFLAGS=-ffast-math >"$logs/fast-$cxx.log" 2>&1 ) \
		|| { cat "$logs/fast-$cxx.log" >&2; fail "$cxx: make CXXFLAGS=-ffast-math failed"; }
	ieee_after_fast_math "$cxx" "$logs/fast-$cxx.log"

	# clean removes the mps build output; P2 checks the full set once bin/check exists.
	( cd "$d" && "$MAKE" CXX="$cxx" >/dev/null 2>&1 ) || fail "$cxx: rebuild before clean"
	( cd "$d" && "$MAKE" clean >/dev/null 2>&1 ) || fail "$cxx: make clean failed"
	for f in "$d"/lib/*.o "$d"/lib/*.d "$d/bin/mps"; do
		test ! -e "$f" || fail "$cxx: make clean left $f (R6)"
	done

	# <cmath> beside the logger, in both orders, compiled to code with the build's flags.
	printf '#include <cmath>\n#include "log.hpp"\nint main() { logging::info("x"); return std::log(1.0) == 0.0 ? 0 : 1; }\n' >"$work/probe1.cpp"
	printf '#include "log.hpp"\n#include <cmath>\nint main() { logging::info("x"); return std::log(1.0) == 0.0 ? 0 : 1; }\n' >"$work/probe2.cpp"
	for probe in "$work/probe1.cpp" "$work/probe2.cpp"; do
		"$cxx" -std=c++20 -O3 -fno-fast-math -ffp-contract=off -Wall -Wextra -I "$d/src" -c "$probe" -o /dev/null >"$logs/probe.log" 2>&1 \
			|| { cat "$logs/probe.log" >&2; fail "$cxx: <cmath> + log.hpp does not compile (R1)"; }
		if grep -q 'warning:' "$logs/probe.log"; then cat "$logs/probe.log" >&2; fail "$cxx: probe warns (R1)"; fi
	done
done

# Output identical to origin/main (R9): same compiler, same host, timestamps masked.
copy_base "$work/base"
mkdir -p "$work/base/lib" "$work/base/bin"
( cd "$work/base" && "$MAKE" CXX=clang++ >"$logs/base.log" 2>&1 ) || { cat "$logs/base.log" >&2; fail "origin/main does not build here"; }
( cd "$work/tree-clang++" && "$MAKE" CXX=clang++ >/dev/null 2>&1 ) || fail "rebuild for the output comparison"
old="$work/base/bin/mps"
new="$work/tree-clang++/bin/mps"
cmp -s "$old" "$new" && fail "the two binaries are identical files: the comparison would be vacuous"
i=0
compare() {
	label=$1; shift
	i=$((i + 1))
	( cd "$work" && env -i PATH="$PATH" HOME="$HOME" LC_ALL=C $ENVS "$old" "$@" ) >"$logs/old.$i" 2>&1; echo "exit $?" >>"$logs/old.$i"
	( cd "$work" && env -i PATH="$PATH" HOME="$HOME" LC_ALL=C $ENVS "$new" "$@" ) >"$logs/new.$i" 2>&1; echo "exit $?" >>"$logs/new.$i"
	sed -E 's/[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}/<ts>/g' "$logs/old.$i" >"$logs/old.$i.m"
	sed -E 's/[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}/<ts>/g' "$logs/new.$i" >"$logs/new.$i.m"
	test -s "$logs/old.$i.m" || fail "no output captured for: $label"
	cmp -s "$logs/old.$i.m" "$logs/new.$i.m" || { diff "$logs/old.$i.m" "$logs/new.$i.m" >&2; fail "output differs from origin/main: $label (R9)"; }
}
ENVS=""
compare "--version" --version
compare "-v" -v
compare "--help" --help
compare "-p 30" -p 30
compare "-t ll 31" -t ll 31
compare "31" 31
compare "-d 31" -d 31
compare "-D 31" -D 31
compare "--bogus" --bogus 31
compare "-t bogus" -t bogus 31
compare "abc" abc
compare "10 digits" 1234567890
ENVS="TASK_ID=abc"
compare "TASK_ID -D -d -t ll" -D -d -t ll 31
ENVS="FORCE_COLOR=1"
compare "FORCE_COLOR -t ll" -t ll 31
ENVS="NO_COLOR=1"
compare "NO_COLOR --help" --help
test "$i" -eq 15 || fail "expected 15 output comparisons, ran $i"

echo "P1 gate: green (clang++ and g++-16 honoured; IEEE flags; 15 outputs identical to origin/main)"
