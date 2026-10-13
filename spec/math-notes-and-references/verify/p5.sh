# Phase P5 gate for math-notes-and-references (PLAN § 6): every note at once, the starting set
# (R6), and the deferral ledger. POSIX sh.
. "$(dirname "$0")/common.sh"
gate 5
cd "$root" || fail "cd $root"
# The ledger: every seed indexed exactly once in ROADMAP.md, and this cycle's seed still adopted
for f in issues/*.md; do
	n=$(grep -c "\*\*Seed:\*\* \[\`$f\`\]" ROADMAP.md)
	test "$n" -eq 1 || fail "$f is indexed $n times in ROADMAP.md"
done
grep -qx 'status: adopted:math-notes-and-references' issues/math-notes-and-references.md \
	|| fail "issues/math-notes-and-references.md is no longer adopted by this cycle"
test -s "$spec/evidence/ledger.md" || fail "$spec/evidence/ledger.md is missing: the deferral ledger (TECH P5)"
grep -q '^## Deferrals' "$spec/evidence/ledger.md" || fail "the ledger has no '## Deferrals' section"
echo "P5 gate: green"
