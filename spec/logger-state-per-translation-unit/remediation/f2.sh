#!/bin/sh
# P2 remediation gate (review cycle 2, F2): make check pins, through bin/mps, that every flag after
# `--` is a positional word, long as well as short. The tree as committed is green. Each of two
# parser defects must turn `make -B check` red, with tests/cli.sh's own checks failing and both
# programs built:
# - M2: the parser applies a `--detailed` it reads after `--`;
# - M6: the parser still reads long options after `--`, so `--json`, `--help` and `--detailed` act
#   there.
# (R2, R7; and cli-and-sieve-fixes' `--` ends the options)
set -u
PHASE=2
. "$(dirname "$0")/../verify/common.sh"

build_all

# mutant NAME AWK WHAT: a copy whose src/cli.cpp is rewritten by AWK, where make check must go red
# in tests/cli.sh's own checks
mutant() {
	m="$work/m-$1"
	copy_tree "$m"
	awk "$2" "$m/src/cli.cpp" >"$m/cli.cpp.new" && mv "$m/cli.cpp.new" "$m/src/cli.cpp" \
		|| fail "the $1 rewrite of src/cli.cpp found no '\t\t\tif (positional_only) {' line"
	! cmp -s "$m/src/cli.cpp" "$root/src/cli.cpp" || fail "the $1 rewrite changed nothing"
	if ( cd "$m" && "$MAKE" -B CXX=clang++ check ) >"$logs/m-$1.log" 2>&1; then
		fail "make check stays green when $3 (F2, R2, R7)"
	fi
	test -x "$m/bin/check" && test -x "$m/bin/mps" \
		|| { tail -20 "$logs/m-$1.log" >&2; fail "$1 broke the build, so the red is not the checks'"; }
	grep -Eq '^cli: [0-9]+ passed, [1-9][0-9]* failed$' "$logs/m-$1.log" \
		|| { tail -20 "$logs/m-$1.log" >&2; fail "with $1, tests/cli.sh's own checks stay green: pin it through bin/mps (F2)"; }
}

mutant M2 '
	/^\t\t\tif \(positional_only\) \{$/ && !done { print; print "\t\t\t\tif (opt == \"--detailed\") logging::set_style(logging::DETAILED);"; done = 1; next }
	{ print }
	END { if (!done) exit 3 }' "a --detailed after -- turns on the detailed style"
mutant M6 '
	/^\t\t\tif \(positional_only\) \{$/ && !done { print "\t\t\tif (positional_only and not starts_with(opt, \"--\")) {"; done = 1; next }
	{ print }
	END { if (!done) exit 3 }' "long options after -- are still read as options"

echo "F2 remediation gate: green (make check, through bin/mps, red when a --detailed after -- applies, and when long options after -- are read)"
