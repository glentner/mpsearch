#!/bin/sh
# P1 remediation gate (review cycle 1, F1): a hostile reason never breaks a verdict line or a record.
# The gate drives the tree's own verdict_line and json_record on clang++ and g++-16 and requires:
# - every line is one line with no control byte (below 0x20, or DEL) and keeps its form;
# - a plain reason stays as it is;
# - every record is one line of valid UTF-8 that parses, keeps its fields and holds a valid-UTF-8
#   reason exactly.
# make check must pin both halves, each turning it red with a failed check:
# - line_text passing the reason through as it is;
# - json_text's body put back to the reviewed head's escaper, which passes bytes from 0x80 up as
#   they are.
# (R6, R10, R13; § Invariants 1)
set -u
PHASE=3
. "$(dirname "$0")/../verify/common.sh"
need python3
rem="$root/$spec/remediation"
reviewed=df9bd39

build_all
version=$(sed -n 's/^static const std::string APP_VERSION = "\(.*\)";$/\1/p' "$root/src/main.hpp")
test -n "$version" || fail "no APP_VERSION in src/main.hpp"

for cxx in $COMPILERS; do
	d="$work/tree-$cxx"
	pb="$work/hostile-$cxx"
	( cd "$d" && "$cxx" -std=c++20 -O1 -fno-fast-math -ffp-contract=off -I src "$rem/hostile.cpp" \
		$(ls src/*.cpp | grep -v '^src/main\.cpp$') -o "$pb" ) >"$logs/hostile-build-$cxx.log" 2>&1 \
		|| { tail -30 "$logs/hostile-build-$cxx.log" >&2; fail "the hostile-reason probe does not build against the tree ($cxx)"; }
	"$pb" >"$work/hostile-$cxx.out" 2>"$logs/hostile-$cxx.err" || fail "the hostile-reason probe failed ($cxx)"
	python3 -I "$rem/hostile.py" "$work/hostile-$cxx.out" "$version" \
		|| fail "a hostile reason breaks a verdict line or a record ($cxx, R6, R10, § 1)"
done

# body_swap FILE NAME BODY: NAME's definition in FILE, the unindented line that holds `NAME(`, does
# not end in `;` and ends in `{`, with its body up to the next line that is exactly `}` replaced by
# the lines of the file BODY
body_swap() {
	awk -v n="$2" -v bf="$3" '
		BEGIN { re = "^([A-Za-z_][^;]*[^A-Za-z0-9_])?" n "\\(" }
		swapping { if ($0 == "}") { while ((getline l < bf) > 0) print l; print; swapping = 0; swapped = 1 } next }
		$0 ~ re && $0 !~ /;[ \t]*$/ && !found { found = 1; print; if ($0 ~ /\{[ \t]*$/) swapping = 1; next }
		{ print }
		END { if (!swapped) exit 3 }' "$1" >"$1.swap" && mv "$1.swap" "$1"
}

# swapped_red NAME FUNC BODY WHAT: plain make check goes red, with a failed check and both programs
# built, in a copy whose FUNC has the body BODY
swapped_red() {
	s="$work/swap-$1"
	copy_tree "$s"
	body_swap "$s/src/result.cpp" "$2" "$3" \
		|| fail "src/result.cpp has no definition of $2 at file scope whose first line ends in { and whose body ends at a line that is exactly } (the gate swaps its body)"
	! cmp -s "$s/src/result.cpp" "$root/src/result.cpp" || fail "swapping $2's body changed nothing"
	if ( cd "$s" && "$MAKE" -B CXX=clang++ check ) >"$logs/swap-$1.log" 2>&1; then
		fail "make check stays green with $4 (R13)"
	fi
	test -x "$s/bin/check" && test -x "$s/bin/mps" \
		|| { tail -20 "$logs/swap-$1.log" >&2; fail "with $4 the build broke, so the red is not the checks' (R13)"; }
	grep -Eq '^(check|cli): [0-9]+ passed, [1-9][0-9]* failed$' "$logs/swap-$1.log" \
		|| { tail -20 "$logs/swap-$1.log" >&2; fail "with $4, make check went red with no failed check (R13)"; }
}

printf '\treturn text;\n' >"$work/body-raw-line"
git -C "$root" show "$reviewed:src/result.cpp" | awk '
	/^static std::string json_string\(const std::string& text\) \{$/ { inside = 1; next }
	inside && $0 == "}" { exit }
	inside { print }' >"$work/body-raw-utf8"
grep -q 'out += char(c);' "$work/body-raw-utf8" || fail "could not take json_string's body from $reviewed"

swapped_red raw-line line_text "$work/body-raw-line" "line_text passing the reason through as it is"
swapped_red raw-utf8 json_text "$work/body-raw-utf8" "json_text back to the reviewed escaper, bytes from 0x80 up as they are"

echo "F1 remediation gate: green (hostile reasons keep every line one line and every record valid JSON in UTF-8, on clang++ and g++-16; make check red without either fix)"
