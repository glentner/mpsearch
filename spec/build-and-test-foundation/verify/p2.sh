#!/bin/sh
# P2 gate: make check is green with both compilers (CXX honoured, IEEE flags on the checks too), needs
# nothing beyond the compiler, make 3.81 and sh built-ins, turns red and names the failure for every
# hollow-gate break, clean removes exactly the build output, and the profile names make check
# (R1, R3, R4, R5, R6, R9, R10).
set -u
. "$(dirname "$0")/common.sh"
need clang++; need g++-16; need rsync

summary_ok() { # LOG: both halves reported, at least one check each, none failed
	grep -Eq '^check: [1-9][0-9]* passed, 0 failed$' "$1" || return 1
	grep -Eq '^cli: [1-9][0-9]* passed, 0 failed$' "$1" || return 1
}

for cxx in clang++ g++-16; do
	d="$work/green-$cxx"
	copy_tree "$d"
	( cd "$d" && "$MAKE" -B CXX="$cxx" check >"$logs/check-$cxx.log" 2>&1 ) || { cat "$logs/check-$cxx.log" >&2; fail "$cxx: make check is red on the clean tree (R3)"; }
	summary_ok "$logs/check-$cxx.log" || { cat "$logs/check-$cxx.log" >&2; fail "$cxx: make check summaries missing or wrong (R3)"; }
	if grep -q 'warning:' "$logs/check-$cxx.log"; then grep 'warning:' "$logs/check-$cxx.log" >&2; fail "$cxx: warnings building the checks (R1)"; fi
	compiled_with "$cxx" "$logs/check-$cxx.log"
	ieee_on_every_compile "$cxx" "$logs/check-$cxx.log" 9
	grep -- ' -c ' "$logs/check-$cxx.log" | grep -q -- 'tests/' || fail "$cxx: no check source compiled"

	# The checks, too, keep the IEEE flags after a command-line -ffast-math.
	( cd "$d" && "$MAKE" -B CXX="$cxx" CXXFLAGS=-ffast-math bin/check >"$logs/fast-$cxx.log" 2>&1 ) \
		|| { cat "$logs/fast-$cxx.log" >&2; fail "$cxx: make CXXFLAGS=-ffast-math bin/check failed"; }
	ieee_after_fast_math "$cxx" "$logs/fast-$cxx.log"

	# clean removes exactly the build output: lib/*.o, lib/*.d, bin/mps, bin/check (R6).
	( cd "$d" && "$MAKE" CXX="$cxx" check >/dev/null 2>&1 ) || fail "$cxx: rebuild before clean"
	: >"$d/lib/keep.txt"; : >"$d/bin/other"
	( cd "$d" && find . -type f | sort ) >"$logs/before-$cxx"
	( cd "$d" && "$MAKE" clean >/dev/null 2>&1 ) || fail "$cxx: make clean failed"
	( cd "$d" && find . -type f | sort ) >"$logs/after-$cxx"
	comm -23 "$logs/before-$cxx" "$logs/after-$cxx" >"$logs/removed-$cxx"
	unexpected=$(grep -Ev '^\./lib/[^/]+\.(o|d)$|^\./bin/(mps|check)$' "$logs/removed-$cxx" || true)
	test -z "$unexpected" || { echo "$unexpected" >&2; fail "$cxx: make clean removed files that are not build output (R6)"; }
	grep -qx './bin/check' "$logs/removed-$cxx" || fail "$cxx: make clean left bin/check (R6)"
	grep -qx './bin/mps' "$logs/removed-$cxx" || fail "$cxx: make clean left bin/mps (R6)"
	left=$( cd "$d" && ls lib/*.o lib/*.d 2>/dev/null || true )
	test -z "$left" || fail "$cxx: make clean left $left (R6)"
	comm -13 "$logs/before-$cxx" "$logs/after-$cxx" | grep -q . && fail "$cxx: make clean created files"
done

# A deleted check file must drop out of bin/check without -B: the link follows the set of check files.
b="$work/relink"
copy_tree "$b"
printf '#include "check.hpp"\n\nstatic void gate_doomed() { check::expect(false, "doomed"); }\n\nstatic check::Suite\ncheck_by_name {\n\t{"gate doomed", gate_doomed},\n};\n' >"$b/tests/check_zz_doomed.cpp"
if ( cd "$b" && "$MAKE" CXX=clang++ check >"$logs/relink-1.log" 2>&1 ); then fail "relink control: the doomed check did not fail"; fi
sleep 1   # make 3.81 compares whole seconds
/bin/rm -f "$b/tests/check_zz_doomed.cpp"
( cd "$b" && "$MAKE" CXX=clang++ check >"$logs/relink-2.log" 2>&1 ) \
	|| { cat "$logs/relink-2.log" >&2; fail "deleting a check file did not relink bin/check: stale checks still run (R3)"; }

# R4: the smoke script uses no external tool: no absolute tool paths, and it passes with an empty PATH.
abs=$(awk 'NR > 1 && /(^|[^A-Za-z0-9_.])\/(usr|bin|sbin|opt)\//' "$root/tests/cli.sh")
test -z "$abs" || { echo "$abs" >&2; fail "tests/cli.sh calls a tool by absolute path (R4)"; }
d="$work/green-clang++"
( cd "$d" && "$MAKE" CXX=clang++ >/dev/null 2>&1 ) || fail "rebuild for the empty-PATH run"
( cd "$d" && env -i PATH=/var/empty LC_ALL=C /bin/sh tests/cli.sh bin/mps >"$logs/cli-nopath.log" 2>&1 ) \
	|| { cat "$logs/cli-nopath.log" >&2; fail "tests/cli.sh needs a tool beyond sh built-ins (R4)"; }
grep -Eq '^cli: [1-9][0-9]* passed, 0 failed$' "$logs/cli-nopath.log" || fail "tests/cli.sh summary missing under an empty PATH (R4)"

# The smoke script must fail on a wrong binary, whichever way it is wrong, and name what failed.
for wrong in /usr/bin/true /usr/bin/false; do
	if ( cd "$d" && sh tests/cli.sh "$wrong" >"$logs/cli-wrong.log" 2>&1 ); then
		cat "$logs/cli-wrong.log" >&2; fail "tests/cli.sh passed with $wrong as the binary (hollow gate)"
	fi
	grep -q '^ FAIL ' "$logs/cli-wrong.log" || { cat "$logs/cli-wrong.log" >&2; fail "tests/cli.sh did not name the failures for $wrong (R3)"; }
done

# Each break in its own fresh copy. Runtime breaks must first build, then make check must go red with
# the exact line the runner prints.
write_break() { # NAME BODY ENTRIES
	b="$work/break-$1"
	copy_tree "$b"
	{
		printf '#include <cstdlib>\n#include "check.hpp"\n\n'
		printf '%s\n\n' "$2"
		printf 'static check::Suite\ncheck_by_name {\n%s\n};\n' "$3"
	} >"$b/tests/check_zz_gate.cpp"
}
runtime_break() { # NAME ANCHORED-REGEX
	( cd "$b" && "$MAKE" -B CXX=clang++ bin/check >"$logs/break-$1-build.log" 2>&1 ) \
		|| { cat "$logs/break-$1-build.log" >&2; fail "break '$1' does not compile: the gate's API assumption no longer holds"; }
	if ( cd "$b" && "$MAKE" CXX=clang++ check >"$logs/break-$1.log" 2>&1 ); then
		cat "$logs/break-$1.log" >&2; fail "break '$1' left make check green (hollow gate)"
	fi
	grep -Eq -- "$2" "$logs/break-$1.log" || { cat "$logs/break-$1.log" >&2; fail "break '$1' did not print a line matching $2"; }
}

write_break failing 'static void gate_failing() { check::expect(false, "deliberate failure"); }' \
	'	{"gate deliberate failure", gate_failing},'
runtime_break failing '^ FAIL gate deliberate failure$'

write_break empty 'static void gate_empty() {}' \
	'	{"gate empty body", gate_empty},'
runtime_break empty '^ FAIL gate empty body$'

write_break duplicate 'static void gate_dup() { check::expect(true, "fine"); }' \
	'	{"gate duplicate", gate_dup},
	{"gate duplicate", gate_dup},'
runtime_break duplicate '^ FAIL gate duplicate$'

write_break crash 'static void gate_crash() { std::abort(); }' \
	'	{"gate crash", gate_crash},'
runtime_break crash '^check gate crash$'

write_break exit 'static void gate_exit() { std::exit(0); }' \
	'	{"gate exit zero", gate_exit},'
runtime_break exit '^check gate exit zero$'

# A check missing from its table must fail the build, and the compiler must name it.
write_break unlisted 'static void gate_listed() { check::expect(true, "fine"); }
static void gate_unlisted() { check::expect(true, "fine"); }' \
	'	{"gate listed", gate_listed},'
if ( cd "$b" && "$MAKE" -B CXX=clang++ check >"$logs/break-unlisted.log" 2>&1 ); then
	cat "$logs/break-unlisted.log" >&2; fail "an unlisted check left make check green (hollow gate)"
fi
grep 'gate_unlisted' "$logs/break-unlisted.log" | grep -q 'unused' || { cat "$logs/break-unlisted.log" >&2; fail "unlisted check: no unused-function error naming it"; }

# An empty registry: remove every check file but the runner's own; bin/check must still link and say so.
b="$work/break-empty-registry"
copy_tree "$b"
for f in "$b"/tests/check_*.cpp; do /bin/rm -f "$f"; done
ls "$b"/tests/check_*.cpp >/dev/null 2>&1 && fail "could not empty the registry"
( cd "$b" && "$MAKE" -B CXX=clang++ bin/check >"$logs/break-registry-build.log" 2>&1 ) \
	|| { cat "$logs/break-registry-build.log" >&2; fail "bin/check does not link with an empty registry"; }
if ( cd "$b" && "$MAKE" CXX=clang++ check >"$logs/break-registry.log" 2>&1 ); then
	cat "$logs/break-registry.log" >&2; fail "an empty registry left make check green (hollow gate)"
fi
grep -q '^ FAIL (no checks registered)$' "$logs/break-registry.log" || { cat "$logs/break-registry.log" >&2; fail "empty registry: no ' FAIL (no checks registered)' line"; }
grep -q '^check: 0 passed, 1 failed$' "$logs/break-registry.log" || fail "empty registry: wrong summary line"

# R10: the profile names make and make check, the release gate mirrors it, and the workaround is gone.
grep -q '^| setup | `make` |' "$root/AGENTS.md" || fail "AGENTS.md profile setup is not make (R10)"
grep -q '^| verify | `make check`' "$root/AGENTS.md" || fail "AGENTS.md profile verify is not make check (R10)"
grep -q 'gate: \[make clean, make check\]' "$root/AGENTS.md" || fail "AGENTS.md release gate is not [make clean, make check] (R10)"
if grep -q 'mkdir -p lib bin' "$root/AGENTS.md"; then fail "AGENTS.md still carries the mkdir workaround (R10)"; fi

echo "P2 gate: green (both compilers honoured; IEEE on checks; exact clean; empty PATH; 2 wrong binaries and 7 breaks all red and named)"
