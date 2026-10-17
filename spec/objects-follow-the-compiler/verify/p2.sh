#!/bin/sh
# P2 gate: the audit, the deferral ledger and the cluster runs, on the final tree.
#   sh spec/objects-follow-the-compiler/verify/p2.sh         P2's own checks, then the P1 gate
#                                                            (about 12 min)
#   sh spec/objects-follow-the-compiler/verify/p2.sh audit   P2's own checks alone (about a minute)
# P2's own checks: the ledger; the commit that first changes the Makefile changes the profile's
# verify row too (R8); the cluster runs as recorded (redacted) in spec/objects-follow-the-compiler/
# evidence/: three passed runs of one pushed commit whose code equals HEAD's outside spec/, issues/
# and ROADMAP.md, each with GCC 13 or newer, GNU Make 4 or newer and that commit's job script, each
# check step's summaries equal to this tree's (from a clang++ make check of HEAD in audit mode, the
# gate's own in full mode), each build step linking bin/mps, each check step compiling only
# lib/check/ objects and bin/check (the settings record holding under GNU Make 4, R3 and R6); sacct's
# state and exit code for all three; the scan covering the evidence. The evidence is checked for
# consistency, not provenance: the orchestrator grades the runs from the job ids, sacct and the
# session's MCP transcript.
set -u
PHASE=2
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/cluster.sh"

MODE=${1:-all}
case $MODE in
	all|audit) ;;
	*) die "usage: sh p2.sh [audit]" ;;
esac

cd "$root" || die "cd $root"

ledger

# R8: the profile's verify row changes in the commit that first changes the Makefile
first=$(git log --reverse --format=%H origin/main..HEAD -- Makefile | head -n 1)
test -n "$first" || fail "no commit on this branch changes the Makefile"
git show --format= "$first" -- AGENTS.md | grep -qxF '+| verify | `make check`: the checks and the smoke test, both always run |' \
	|| fail "the commit that first changes the Makefile ($(git rev-parse --short "$first")) does not change the profile's verify row (R8)"

# The counts the cluster's check step must equal: this tree's
if [ "$MODE" = audit ]; then
	copy_tree "$work/audit-tree" || die "cannot copy the tree"
	( cd "$work/audit-tree" && limit 1500 "$MAKE" CXX=clang++ check ) >"$logs/audit-check.log" 2>&1 \
		|| { tail -20 "$logs/audit-check.log" >&2; die "make check is red"; }
	CHECKS=$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/audit-check.log")
	CLIS=$(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/audit-check.log")
else
	gate_all
fi
cd "$root" || die "cd $root"
test -n "$CHECKS" && test -n "$CLIS" && test "$CHECKS" != '?' || die "no check counts from this tree"

# The cluster runs.
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
field() { # FILE KEY: one field of a result.json ("steps.3.status" walks the array)
	python3 -I "$work/fields.py" "$1" "$2" || die "cannot read $2 from ${1#"$root"/}"
}
step_codes() { # FILE: P/F/N per step, in order
	for i in 0 1 2 3 4; do
		case $(field "$1" "steps.$i.status") in
			passed) printf P ;; failed) printf F ;; not_run) printf N ;; *) printf '?' ;;
		esac
	done
}
out_script() { # LABEL: the script hash the job printed in that run's scheduler output
	test -f "$ev/$1.out" || die "no scheduler output evidence/$1.out"
	sed -n 's/^mps-test: script \([0-9a-f]\{40\}\)$/\1/p' "$ev/$1.out" | head -n 1
}
job_line() { # LABEL: "job_id state exit_code" from evidence/jobs.tsv
	awk -F '\t' -v l="$1" '$1 == l { print $2, $3, $4 }' "$ev/jobs.tsv"
}

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
	field "$f" make | grep -Eq '^GNU Make ([4-9]|[1-9][0-9])\.' || fail "$name: make '$(field "$f" make)' is not GNU Make 4 or newer (R6)"
	c=$(field "$f" commit)
	if [ -z "$green" ]; then green=$c; else test "$c" = "$green" || fail "$name tested $c, another target tested $green: not one head"; fi
	test "$(out_script "$name")" = "$(git rev-parse "$c:ci/test-commit.sh")" \
		|| fail "$name: the script that ran is not $c's ci/test-commit.sh"
	bl="$ev/$name.build.log" cl="$ev/$name.check.log"
	test -f "$bl" || die "no evidence/$name.build.log (the run's build.log, redacted)"
	test -f "$cl" || die "no evidence/$name.check.log (the run's check.log, redacted)"
	grep -Eq '^g\+\+ .* -o bin/mps$' "$bl" || fail "$name: build.log shows no g++ link of bin/mps"
	grep -Eq '^g\+\+ .* -o lib/main\.o$' "$bl" || fail "$name: build.log shows no g++ compile of lib/main.o"
	! grep -Eq ' -o lib/check/' "$bl" || fail "$name: the build step compiled lib/check/ objects"
	grep -qx "check: $CHECKS passed, 0 failed" "$cl" || fail "$name: check.log does not say 'check: $CHECKS passed, 0 failed', this tree's count"
	grep -qx "cli: $CLIS passed, 0 failed" "$cl" || fail "$name: check.log does not say 'cli: $CLIS passed, 0 failed', this tree's count"
	grep -Eq '^g\+\+ .* -o lib/check/[a-z_]+\.o$' "$cl" || fail "$name: check.log shows no g++ compile of a lib/check/ object"
	grep -Eq '^g\+\+ .* -o bin/check$' "$cl" || fail "$name: check.log shows no g++ link of bin/check"
	! grep -Eq ' -o (lib/[a-z_]+\.o|bin/mps)$' "$cl" \
		|| fail "$name: the check step rebuilt what the build step had built with the same settings (R3 under GNU Make 4)"
	RUNS_LINE="$RUNS_LINE $name ($(field "$f" make));"
done
git cat-file -e "$green^{commit}" 2>/dev/null || fail "the tested commit $green is not in this repository"
git merge-base --is-ancestor "$green" HEAD || fail "the tested commit $green is not an ancestor of HEAD"
git diff --quiet "$green" HEAD -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md' \
	|| fail "code changed after the tested commit $green: the runs do not cover HEAD"
test -z "$(git status --porcelain -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md')" \
	|| fail "uncommitted or untracked code in the working tree: the runs do not cover what this gate built"
git merge-base --is-ancestor "$green" "origin/fix/$slug" 2>/dev/null \
	|| fail "the tested commit $green is not on the pushed branch origin/fix/$slug"
git merge-base --is-ancestor origin/main "$green" || fail "the tested commit $green does not contain origin/main"

scan_cycle evidence

[ -z "$SELFTEST" ] || gate_end "P2"
if [ "$MODE" = audit ]; then
	echo "P2 audit: green (ledger; R8's row in the Makefile's commit; 3 targets passed on $green with GCC 13+:$RUNS_LINE check steps rebuilt only lib/check/; sacct agrees; scan clean; the full gate not run)"
else
	echo "P2 gate: green (ledger; R8's row in the Makefile's commit; P1 on the final tree: $GATE_LINE; 3 targets passed on $green with GCC 13+:$RUNS_LINE check steps rebuilt only lib/check/; sacct agrees; scan clean)"
fi
