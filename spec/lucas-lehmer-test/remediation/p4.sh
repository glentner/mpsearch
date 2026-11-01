#!/bin/sh
# P4 gate, dispatch B: the audit, the deferral ledger, the landing gate and the cluster runs.
#   sh spec/lucas-lehmer-test/verify/p4.sh          P4's own checks, then the P3 gate in full mode on the
#                                                   final tree, its file list graded as of P4 (about 40 min)
#   sh spec/lucas-lehmer-test/verify/p4.sh audit    P4's own checks alone (about 2 minutes)
# P4's own checks: the ledger (cluster.sh); R12 and R18's same-commit rules in the branch's history:
# the commit that first adds each driver path adds its high-blast-radius line and its § Map entry and
# changes doc/lucas-lehmer.md, the one that adds the check-long target names it in § Build and run,
# the one that adds ci/engine-gate.sh adds the profile's landing gate row, the one that adds
# `jacobi` changes doc/reference-arithmetic.md, and the one that adds the record's run fields bumps
# RECORD_VERSION; `## Times` quoting R16's two times and `## Landing gate` quoting a green run of
# ci/engine-gate.sh on a commit whose tree is the tested commit's (both from dispatch A's pre-push
# run); the cluster runs as recorded (redacted) in spec/lucas-lehmer-test/evidence/: three passed runs
# of one pushed commit on PUSH_REF whose code equals HEAD's outside spec/, issues/ and ROADMAP.md, each
# with GCC 13 or newer, GNU Make 4 or newer and that commit's job script, each check step's summaries
# (check:, cli:, ll:) equal to this tree's; sacct's state and exit code for all three; the scan
# covering the evidence. The evidence is checked for consistency, not provenance: the orchestrator
# grades the runs from the job ids, sacct and the session's transcript.
set -u
PHASE=3
case ${1:-all} in
	all) GATE_MODE=full ;;
	audit) GATE_MODE=quick ;;
	*) echo "usage: sh p4.sh [audit]" >&2; exit 2 ;;
esac
P4_MODE=${1:-all}
. "$(dirname "$0")/../verify/common.sh"
# Set after common.sh, which clears both from the caller's environment
CHANGES_PHASE=4
GATE_NO_ENGINE_RUN=1
. "$(dirname "$0")/../verify/gate.sh"
# Foreman ruling (2026-11-01, remediation/make.sh): F2 rewrote AGENTS.md's verify row; the agents step
# reads remediation/ll_files.py, the frozen ll_files.py with only that pinned line as the row reads now.
# The file list is graded as of phase 4, as verify/prepush.sh and verify/p4.sh grade it already.
CHANGES_PHASE=4
_fc=$(sed -n '/^files_check() {$/,/^}$/p' "$here/gate.sh" | sed 's|"\$here/ll_files\.py" agents|"$root/$spec/remediation/ll_files.py" agents|')
case $_fc in *'"$root/$spec/remediation/ll_files.py" agents '*) eval "$_fc" ;; *) die "remediation: files_check not found in gate.sh" ;; esac
sh "$root/$spec/remediation/make.sh" check >/dev/null || die "remediation: the wrapped gates drifted from verify/ (make.sh check)"
. "$(dirname "$0")/../verify/cluster.sh"

cd "$root" || die "cd $root"

ledger

# R12 and R18 in the history
first_adding() { git log --reverse --diff-filter=A --format=%H origin/main..HEAD -- "$1" | head -n 1; }
for hp in src/lucas.hpp src/lucas.cpp; do
	hc=$(first_adding "$hp")
	test -n "$hc" || { fail "no commit on this branch adds $hp"; continue; }
	py "$here/ll_files.py" added "$hc" "$hp" || fail "the commit that adds $hp ($(git rev-parse --short "$hc")) does not add its AGENTS.md lines (R18)"
	git show --format= --name-only "$hc" | grep -qx doc/lucas-lehmer.md \
		|| fail "the commit that adds $hp does not update doc/lucas-lehmer.md (AGENTS.md § Conventions)"
done
hc=$(git log --reverse --format=%H origin/main..HEAD -S'check-long:' -- Makefile | head -n 1)
test -n "$hc" && git show --format= "$hc" -- AGENTS.md | grep -q '^+make check-long' \
	|| fail "the commit that adds the check-long target does not name it in § Build and run (R18)"
hc=$(first_adding ci/engine-gate.sh)
test -n "$hc" && git show --format= "$hc" -- AGENTS.md | grep -q '^+| landing gate |' \
	|| fail "the commit that adds ci/engine-gate.sh does not add the profile's landing gate row (R17)"
hc=$(git log --reverse --format=%H origin/main..HEAD -S'int jacobi(const BigInt& a, const BigInt& n) {' -- src/mp.cpp | head -n 1)
test -n "$hc" && git show --format= --name-only "$hc" | grep -qx doc/reference-arithmetic.md \
	|| fail "the commit that adds jacobi does not write it up in doc/reference-arithmetic.md (R18, § Conventions)"
hc=$(git log --reverse --format=%H origin/main..HEAD -S'"lengths' -- src/result.cpp | head -n 1)
test -n "$hc" && git show --format= "$hc" -- src/result.hpp | grep -Eq '^\+[[:space:]]*static const int RECORD_VERSION = 2;' \
	|| fail "the commit that adds the run to the record does not bump RECORD_VERSION (R12)"

# The Times and the landing gate, quoted from dispatch A's pre-push run
ld="$spec/evidence/ledger.md"
grep -E '^- make check: ' "$ld" | grep -F 'bcb6fdf' | grep -Eq '[+-][0-9]+\.[0-9]+ s' \
	|| fail "evidence/ledger.md's '## Times' does not quote make check against bcb6fdf (R16)"
grep -Eq '^- make check-long: [0-9]+ s' "$ld" || fail "evidence/ledger.md's '## Times' does not quote make check-long's seconds (R16)"
eg_hash=$(sed -n 's/^- engine-gate: green on \([0-9a-f]\{40\}\): .*/\1/p' "$ld" | head -n 1)
test -n "$eg_hash" || fail "evidence/ledger.md's '## Landing gate' quotes no green line of ci/engine-gate.sh (R17)"

# The counts the cluster's check step must equal: this tree's
if [ "$P4_MODE" = audit ]; then
	copy_tree "$work/audit-tree" || die "cannot copy the tree"
	mk "$work/audit-tree" clang++ audit-check 2400 check || { tail_of audit-check; die "make check is red"; }
	sumlog="$logs/audit-check.out"
else
	gate_all
	sumlog="$logs/check-clang++.out"
fi
cd "$root" || die "cd $root"
CHECKS=$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$sumlog")
CLIS=$(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$sumlog")
LLS=$(sed -n 's/^ll: \([0-9]*\) passed, 0 failed$/\1/p' "$sumlog")
test -n "$CHECKS" && test -n "$CLIS" && test -n "$LLS" || die "no check counts from this tree"

# The cluster runs
ev="$root/$spec/evidence"
test -d "$ev" || fail "no evidence directory $spec/evidence"
test -f "$ev/jobs.tsv" || fail "no evidence/jobs.tsv (label, job id, sacct state, sacct exit code)"
test "$(head -n 1 "$ev/jobs.tsv")" = "$(printf 'label\tjob_id\tstate\texit_code')" || fail "jobs.tsv header"
test "$(awk 'NR > 1' "$ev/jobs.tsv" | wc -l | tr -d ' ')" -eq 3 || fail "jobs.tsv holds other than three jobs"

cat >"$work/fields.py" <<'PY'
import json, sys
r = json.load(open(sys.argv[1]))
for k in sys.argv[2:]:
    v = r
    for part in k.split("."):
        v = v[int(part)] if isinstance(v, list) else v[part]
    print("null" if v is None else v)
PY
field() { python3 -I "$work/fields.py" "$1" "$2" || die "cannot read $2 from ${1#"$root"/}"; }
step_codes() {
	for i in 0 1 2 3 4; do
		case $(field "$1" "steps.$i.status") in
			passed) printf P ;; failed) printf F ;; not_run) printf N ;; *) printf '?' ;;
		esac
	done
}
out_script() {
	test -f "$ev/$1.out" || die "no scheduler output evidence/$1.out"
	sed -n 's/^mps-test: script \([0-9a-f]\{40\}\)$/\1/p' "$ev/$1.out" | head -n 1
}
job_line() { awk -F '\t' -v l="$1" '$1 == l { print $2, $3, $4 }' "$ev/jobs.tsv"; }

green=
ids=
RUNS_LINE=
for t in x86-64-avx512:x86_64 x86-64-avx2:x86_64 aarch64-gb10:aarch64; do
	name=${t%%:*} arch=${t#*:}
	f="$ev/$name.json"
	result_check "$f" failed=none target="$name" arch="$arch"
	test "$(field "$f" cluster)" = '<cluster>' || fail "$name: cluster is not redacted to <cluster>"
	test "$(field "$f" host)" = '<host>' || fail "$name: host is not redacted to <host>"
	j=$(field "$f" job_id)
	printf '%s\n' "$j" | grep -Eqx '[0-9]+' || fail "$name: job id '$j' is not a Slurm job id"
	case " $ids " in *" $j "*) fail "$name: job id $j reused" ;; esac
	ids="$ids $j"
	set -- $(job_line "$name")
	test "$#" -eq 3 || die "$name: no single line in evidence/jobs.tsv"
	test "$1" = "$j" || fail "$name: jobs.tsv says job $1, result.json says $j"
	test "$2 $3" = "COMPLETED 0:0" || fail "$name: sacct says $2 $3, want COMPLETED 0:0"
	test "$(step_codes "$f")" = PPPPP || fail "$name: not every step passed"
	field "$f" compiler | grep -Eq 'GCC\) (1[3-9]|[2-9][0-9])\.' || fail "$name: compiler '$(field "$f" compiler)' is not GCC 13 or newer"
	field "$f" make | grep -Eq '^GNU Make ([4-9]|[1-9][0-9])\.' || fail "$name: make '$(field "$f" make)' is not GNU Make 4 or newer"
	c=$(field "$f" commit)
	if [ -z "$green" ]; then green=$c; else test "$c" = "$green" || fail "$name tested $c, another target tested $green: not one head"; fi
	test "$(out_script "$name")" = "$(git rev-parse "$c:ci/test-commit.sh")" \
		|| fail "$name: the script that ran is not $c's ci/test-commit.sh"
	cl="$ev/$name.check.log"
	test -f "$cl" || die "no evidence/$name.check.log (the run's check.log, redacted)"
	grep -qx "check: $CHECKS passed, 0 failed" "$cl" || fail "$name: check.log does not say 'check: $CHECKS passed, 0 failed', this tree's count"
	grep -qx "cli: $CLIS passed, 0 failed" "$cl" || fail "$name: check.log does not say 'cli: $CLIS passed, 0 failed', this tree's count"
	grep -qx "ll: $LLS passed, 0 failed" "$cl" || fail "$name: check.log does not say 'll: $LLS passed, 0 failed', this tree's count"
	RUNS_LINE="$RUNS_LINE $name;"
done
git cat-file -e "$green^{commit}" 2>/dev/null || fail "the tested commit $green is not in this repository"
git merge-base --is-ancestor "$green" HEAD || fail "the tested commit $green is not an ancestor of HEAD"
git diff --quiet "$green" HEAD -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md' \
	|| fail "code changed after the tested commit $green: the runs do not cover HEAD"
test -z "$(git status --porcelain -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md')" \
	|| fail "uncommitted or untracked code in the working tree: the runs do not cover what this gate built"
git merge-base --is-ancestor "$green" "$PUSH_REF" 2>/dev/null \
	|| fail "the tested commit $green is not on the pushed branch $PUSH_REF"
git merge-base --is-ancestor origin/main "$green" || fail "the tested commit $green does not contain origin/main"
if [ -n "$eg_hash" ]; then
	git cat-file -e "$eg_hash^{commit}" 2>/dev/null || fail "the landing gate's commit $eg_hash is not in this repository"
	test "$(git rev-parse "$eg_hash^{tree}" 2>/dev/null)" = "$(git rev-parse "$green^{tree}")" \
		|| git diff --quiet "$eg_hash" "$green" -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md' \
		|| fail "the landing gate ran on $eg_hash, whose code is not the tested commit's (R17)"
fi

scan_cycle evidence

[ -z "$SELFTEST" ] || gate_end "P4"
if [ "$P4_MODE" = audit ]; then
	echo "P4 audit: green (ledger; R12 and R18 in the history; Times and the landing gate quoted; 3 targets passed on $green:$RUNS_LINE sacct agrees; scan clean; the full gate not run)"
else
	echo "P4 gate: green (ledger; R12 and R18 in the history; Times and the landing gate quoted; P3 on the final tree: $GATE_LINE; 3 targets passed on $green:$RUNS_LINE sacct agrees; scan clean)"
fi
