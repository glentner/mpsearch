#!/bin/sh
# P1 remediation gate (review cycle 1, F1): the profile's own `make check`, with no flags added,
# shows the sieve writes nothing outside its entries. The tree as committed is green on clang++ and
# g++-16 with no warnings and one summary per half. Restore the old in-word write of entry 2, for
# n = 1 and 2 or for n = 2 alone, and `make -B check` goes red on both compilers with both programs
# built, so the red is the checks'; the same guard written another way stays green. bin/mps is still
# compiled without bounds assertions, and every compile keeps the IEEE flags last, right before -c
# (§ Invariants 2) (R9, R10).
set -u
. "$(dirname "$0")/../verify/common.sh"

build_all
for cxx in $COMPILERS; do
	test "$(grep -c '^check: ' "$logs/check-$cxx.log")" -eq 1 || fail "$cxx: make check prints other than one check summary (R10)"
	test "$(grep -c '^cli: ' "$logs/check-$cxx.log")" -eq 1 || fail "$cxx: make check prints other than one cli summary (R10)"
done

# guard NAME TO: src/math.cpp with its guard `if (n > 2)` written as TO, in a fresh copy per
# compiler; runs plain `make -B check` there and leaves its status in $gs and its copy in $gd
guard() {
	gd="$work/guard-$1-$2"
	copy_tree "$gd"
	grep -qF 'if (n > 2)' "$gd/src/math.cpp" || fail "src/math.cpp has no 'if (n > 2)' guard to rewrite ($1, R9)"
	sed "s/if (n > 2)/$3/" "$gd/src/math.cpp" >"$gd/math.cpp.new" && mv "$gd/math.cpp.new" "$gd/src/math.cpp" \
		|| fail "rewrite of the guard ($1)"
	grep -qF "$3" "$gd/src/math.cpp" || fail "the guard rewrite $1 did not apply"
	gs=0
	( cd "$gd" && "$MAKE" -B CXX="$2" check ) >"$logs/guard-$1-$2.log" 2>&1 || gs=1
}

for cxx in $COMPILERS; do
	for g in 'n12:if (n > 0)' 'n2:if (n >= 2)'; do
		name=${g%%:*}
		guard "$name" "$cxx" "${g#*:}"
		if test "$gs" -eq 0; then
			tail -4 "$logs/guard-$name-$cxx.log" >&2
			fail "plain make check stays green when the sieve writes entry 2 for $name ($cxx, R9)"
		fi
		test -x "$gd/bin/check" && test -x "$gd/bin/mps" \
			|| { tail -20 "$logs/guard-$name-$cxx.log" >&2; fail "with the stray write for $name the build broke: the red is not the checks' ($cxx, R10)"; }
	done
	guard same "$cxx" 'if (2 < n)'
	test "$gs" -eq 0 || { tail -20 "$logs/guard-same-$cxx.log" >&2; fail "make check goes red with the guard written as 2 < n: the checks see the text, not the writes ($cxx, R9)"; }
done

# The program's own build: no bounds assertions, IEEE flags last on every compile.
d="$work/flags"
copy_tree "$d"
( cd "$d" && "$MAKE" -n -B CXX=clang++ bin/mps ) >"$logs/flags-mps.log" 2>&1 || fail "make -n bin/mps"
( cd "$d" && "$MAKE" -n -B CXX=clang++ check ) >"$logs/flags-check.log" 2>&1 || fail "make -n check"
grep -q ' -c ' "$logs/flags-mps.log" || fail "make -n bin/mps shows no compile"
if grep -E '_GLIBCXX_ASSERTIONS|_GLIBCXX_DEBUG|_LIBCPP_HARDENING_MODE|-fsanitize' "$logs/flags-mps.log"; then
	fail "bin/mps is compiled with bounds assertions or sanitizers: its flags belong to § Invariants 2"
fi
for f in "$logs/flags-mps.log" "$logs/flags-check.log"; do
	if grep ' -c ' "$f" | grep -v -e '-std=c++20 -fno-fast-math -ffp-contract=off -c '; then
		fail "a compile without the IEEE flags last, right before -c (§ Invariants 2)"
	fi
done

echo "F1 remediation gate: green (plain make check red under the stray write for n = 1, 2 and n = 2 on clang++ and g++-16; equivalent guard green; bin/mps unhardened; IEEE last)"
