#!/bin/sh
# P3 gate: the license expression in every header, LICENSE unchanged, LICENSE-EXCEPTION and the README
# section present, the deferral ledger complete, and make check still green from a clone-like copy
# (R7, R8).
set -u
. "$(dirname "$0")/common.sh"
need rsync

cd "$root" || fail "cd $root"

grep -rn 'GPLv3' src tests Makefile; rc=$?
test "$rc" -eq 1 || fail "GPLv3 identifiers remain, or the search itself failed (grep rc=$rc) (R7)"
n=0
for f in src/*.cpp src/*.hpp tests/*.cpp tests/*.hpp tests/*.sh Makefile; do
	[ -e "$f" ] || continue
	n=$((n + 1))
	head -3 "$f" | grep -Eq '^(//|#) SPDX-License-Identifier: GPL-3\.0-only WITH GPL-3\.0-linking-exception$' \
		|| fail "$f lacks the exact license expression in its first three lines (R7)"
done
test "$n" -ge 20 || fail "expected at least 20 files with headers, saw $n"

git diff --quiet origin/main -- LICENSE || fail "LICENSE changed (R8)"
test -f LICENSE-EXCEPTION || fail "LICENSE-EXCEPTION missing (R8)"
git ls-files --error-unmatch LICENSE-EXCEPTION >/dev/null 2>&1 || git ls-files --others --exclude-standard | grep -qx LICENSE-EXCEPTION \
	|| fail "LICENSE-EXCEPTION is ignored by git (R8)"
grep -q 'Additional permission under GNU GPL version 3 section 7' LICENSE-EXCEPTION || fail "exception heading missing (R8)"
for lib in 'CUDA' 'ROCm' 'Metal' 'libtpu'; do
	grep -q "$lib" LICENSE-EXCEPTION || fail "LICENSE-EXCEPTION does not name $lib (R8)"
done
grep -q 'LICENSE-EXCEPTION' AGENTS.md || fail "AGENTS.md does not name LICENSE-EXCEPTION, the file that grants the permission (R8)"
if grep -q 'the license file grants' AGENTS.md; then fail "AGENTS.md still says the license file grants the permission (R8)"; fi
grep -q 'LICENSE-EXCEPTION' README.md || fail "README does not name LICENSE-EXCEPTION (R8)"
grep -qi 'version 3 only' README.md || fail "README does not say version 3 only (R8)"

# Deferral ledger: every seed is indexed, and this cycle's deferrals have homes.
seeds=$(ls issues/*.md | wc -l | tr -d ' ')
entries=$(grep -c '^\*\*Seed:\*\*' ROADMAP.md)
test "$seeds" -eq "$entries" || fail "ROADMAP has $entries Seed lines for $seeds seeds"
test -f issues/logger-state-per-translation-unit.md || fail "logger-state seed missing"
grep -q '^status: unshaped$' issues/logger-state-per-translation-unit.md || fail "logger-state seed status"
grep -q 'issues/logger-state-per-translation-unit.md' ROADMAP.md || fail "logger-state seed not in ROADMAP"
grep -qi 'out of range\|out-of-range' issues/results-and-cli-fixes.md || fail "sieve out-of-range write not recorded"

copy_tree "$work/final"
( cd "$work/final" && "$MAKE" -B check >"$logs/final.log" 2>&1 ) || { cat "$logs/final.log" >&2; fail "make check red after P3"; }

echo "P3 gate: green ($n headers; LICENSE unchanged; exception and README present; ledger complete)"
