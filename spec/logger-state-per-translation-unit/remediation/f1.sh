#!/bin/sh
# P2 remediation gate (review cycle 1, F1): make check pins, through bin/mps, that a -D read after
# `--` changes nothing. The tree as committed is green. With the reviewer's mutant, where the
# parser applies a -D it reads after `--`, `make -B check` goes red with tests/cli.sh's own checks
# failing and both programs built (R2, R7).
set -u
PHASE=2
. "$(dirname "$0")/../verify/common.sh"

build_all

m="$work/after-dashdash"
copy_tree "$m"
awk '
	/^\t\t\tif \(positional_only\) \{$/ && !done { print; print "\t\t\t\tif (opt == \"-D\") logging::set_style(logging::DETAILED);"; done = 1; next }
	{ print }
	END { if (!done) exit 3 }' "$m/src/cli.cpp" >"$m/cli.cpp.new" && mv "$m/cli.cpp.new" "$m/src/cli.cpp" \
	|| fail "src/cli.cpp has no '\t\t\tif (positional_only) {' line for the gate's mutant"
grep -q 'if (opt == "-D") logging::set_style' "$m/src/cli.cpp" || fail "the mutant did not apply"
if ( cd "$m" && "$MAKE" -B CXX=clang++ check ) >"$logs/after-dashdash.log" 2>&1; then
	fail "make check stays green when a -D after -- turns on the detailed style (F1, R2, R7)"
fi
test -x "$m/bin/check" && test -x "$m/bin/mps" \
	|| { tail -20 "$logs/after-dashdash.log" >&2; fail "the mutant broke the build, so the red is not the checks'"; }
grep -Eq '^cli: [0-9]+ passed, [1-9][0-9]* failed$' "$logs/after-dashdash.log" \
	|| { tail -20 "$logs/after-dashdash.log" >&2; fail "with the mutant, tests/cli.sh's own checks stay green: pin it through bin/mps (F1)"; }

echo "F1 remediation gate: green (make check, through bin/mps, red when a -D after -- turns on the detailed style)"
