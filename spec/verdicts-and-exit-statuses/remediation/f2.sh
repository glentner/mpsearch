#!/bin/sh
# P2 remediation gate (review cycle 1, F2 and F3): AGENTS.md's map line for src/math.* names the
# exponent check, and make check pins that the exponent check reports a composite only through its
# exact re-check. The tree's check_exponent_by(p, search), driven on clang++ and g++-16, must:
# - turn a search that lies (0, 1, a non-divisor) into an error, never a composite;
# - take a larger true divisor to its own factor;
# - answer as check_exponent does under the true search.
# With check_exponent_by's `return composite_by_exponent(p, a);` replaced by building the composite
# directly, make check must go red with a failed check. (R3, R13; § Invariants 1)
set -u
PHASE=3
. "$(dirname "$0")/../verify/common.sh"
rem="$root/$spec/remediation"

build_all

grep -E '^- `src/math\.cpp`, `src/math\.hpp`: .*exponent check' "$root/AGENTS.md" >/dev/null \
	|| fail "AGENTS.md's map line for src/math.cpp and src/math.hpp does not name the exponent check (F2)"

for cxx in $COMPILERS; do
	d="$work/tree-$cxx"
	pb="$work/wiring-$cxx"
	( cd "$d" && "$cxx" -std=c++20 -O1 -fno-fast-math -ffp-contract=off -I src "$rem/wiring.cpp" \
		$(ls src/*.cpp | grep -v '^src/main\.cpp$') -o "$pb" ) >"$logs/wiring-build-$cxx.log" 2>&1 \
		|| { tail -30 "$logs/wiring-build-$cxx.log" >&2; fail "the wiring probe does not build against the tree ($cxx): math.hpp declares FactorSearch and check_exponent_by(p, search)"; }
	"$pb" >"$logs/wiring-$cxx.out" 2>&1 || { cat "$logs/wiring-$cxx.out" >&2; fail "the exponent check does not go through its re-check ($cxx, R3, § 1)"; }
done

s="$work/bypass"
copy_tree "$s"
n=$(grep -c 'return composite_by_exponent(p, a);' "$s/src/math.cpp")
test "$n" -eq 1 || fail "src/math.cpp holds 'return composite_by_exponent(p, a);' $n times, not once (the gate bypasses it)"
sed 's/return composite_by_exponent(p, a);/return Result::composite_by_factor(WORK_PTEST, p, "2^" + std::to_string(a) + "-1");/' \
	"$s/src/math.cpp" >"$s/math.cpp.new" && mv "$s/math.cpp.new" "$s/src/math.cpp" || fail "the bypass edit"
grep -q 'return Result::composite_by_factor(WORK_PTEST, p, ' "$s/src/math.cpp" || fail "the bypass edit did not apply"
if ( cd "$s" && "$MAKE" -B CXX=clang++ check ) >"$logs/bypass.log" 2>&1; then
	fail "make check stays green when the exponent check builds its composite without the re-check (F3, R3, R13)"
fi
test -x "$s/bin/check" && test -x "$s/bin/mps" \
	|| { tail -20 "$logs/bypass.log" >&2; fail "the bypass broke the build, so the red is not the checks' (R13)"; }
grep -Eq '^(check|cli): [0-9]+ passed, [1-9][0-9]* failed$' "$logs/bypass.log" \
	|| { tail -20 "$logs/bypass.log" >&2; fail "with the bypass, make check went red with no failed check (R13)"; }

echo "F2-F3 remediation gate: green (the map names the exponent check; check_exponent_by refuses a lying search on clang++ and g++-16; make check red when the re-check is bypassed)"
