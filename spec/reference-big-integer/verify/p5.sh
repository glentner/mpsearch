#!/bin/sh
# P5 gate: everything prepush.sh checks but the uncommitted-code rule, on the final tree (the P4 gate,
# the deferral ledger, the scan with its self-test), plus the cluster runs as recorded (redacted) in
# spec/reference-big-integer/evidence/: three passed runs of one pushed commit whose code equals
# HEAD's outside spec/, issues/ and ROADMAP.md, each with GCC 13 or newer and that commit's job
# script, each check step's summaries equal to this tree's; sacct's state and exit code for all
# three; each check step's seconds recorded in evidence/ledger.md against cycle 6's run on the same
# target (the cluster's check step compiles too, so R10 itself is gated on this Mac); the scan
# covering the evidence. The evidence is checked for consistency, not provenance: the orchestrator
# grades the runs from the job ids, sacct and the session's MCP transcript.
set -u
PHASE=4
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/cluster.sh"

gate_all
ledger

cd "$root" || fail "cd $root"

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
	python3 -I "$work/fields.py" "$1" "$2" || fail "cannot read $2 from ${1#"$root"/}"
}
step_codes() { # FILE: P/F/N per step, in order
	for i in 0 1 2 3 4; do
		case $(field "$1" "steps.$i.status") in
			passed) printf P ;; failed) printf F ;; not_run) printf N ;; *) printf '?' ;;
		esac
	done
}
out_script() { # LABEL: the script hash the job printed in that run's scheduler output
	test -f "$ev/$1.out" || fail "no scheduler output evidence/$1.out"
	sed -n 's/^mps-test: script \([0-9a-f]\{40\}\)$/\1/p' "$ev/$1.out" | head -n 1
}
job_line() { # LABEL: "job_id state exit_code" from evidence/jobs.tsv
	awk -F '\t' -v l="$1" '$1 == l { print $2, $3, $4 }' "$ev/jobs.tsv"
}

green=
ids=
TIMES_LINE=
PRIOR=$(git log -1 --format=%H -- spec/logger-state-per-translation-unit/evidence/x86-64-avx512.json)
test -n "$PRIOR" || fail "no commit holds cycle 6's evidence"
grep -qx '## Times' "$ev/ledger.md" 2>/dev/null || fail "evidence/ledger.md has no '## Times' section"
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
	test "$#" -eq 3 || fail "$name: no single line in evidence/jobs.tsv"
	test "$1" = "$j" || fail "$name: jobs.tsv says job $1, result.json says $j"
	test "$2 $3" = "COMPLETED 0:0" || fail "$name: sacct says $2 $3, want COMPLETED 0:0"
	test "$(step_codes "$f")" = PPPPP || fail "$name: not every step passed"
	field "$f" compiler | grep -Eq 'GCC\) (1[3-9]|[2-9][0-9])\.' || fail "$name: compiler '$(field "$f" compiler)' is not GCC 13 or newer"
	c=$(field "$f" commit)
	if [ -z "$green" ]; then green=$c; else test "$c" = "$green" || fail "$name tested $c, another target tested $green: not one head"; fi
	test "$(out_script "$name")" = "$(git rev-parse "$c:ci/test-commit.sh")" \
		|| fail "$name: the script that ran is not $c's ci/test-commit.sh"
	# The check step ran this tree's checks, all of them, on GCC on Linux, and echoed no detailed line
	cl="$ev/$name.check.log"
	test -f "$cl" || fail "no evidence/$name.check.log (the run's check.log, redacted)"
	grep -qx "check: $CHECKS passed, 0 failed" "$cl" || fail "$name: check.log does not say 'check: $CHECKS passed, 0 failed', this tree's count"
	grep -qx "cli: $CLIS passed, 0 failed" "$cl" || fail "$name: check.log does not say 'cli: $CLIS passed, 0 failed', this tree's count"
	grep -q 'g++' "$cl" || fail "$name: check.log shows no g++ compile or link"
	# The check step's seconds, against cycle 6's run on the same target (R10's record, PLAN § 6)
	secs=$(field "$f" steps.4.seconds)
	before=$(git show "$PRIOR:spec/logger-state-per-translation-unit/evidence/$name.json" | python3 -I -c 'import json, sys; print(json.load(sys.stdin)["steps"][4]["seconds"])') \
		|| fail "cannot read cycle 6's check seconds for $name"
	grep -Eq "^- \`$name\`: check step $secs s \(cycle 6: $before s\)" "$ev/ledger.md" \
		|| fail "evidence/ledger.md's ## Times has no line '- \`$name\`: check step $secs s (cycle 6: $before s)…'"
	TIMES_LINE="$TIMES_LINE $name $secs s (was $before s);"
done
git cat-file -e "$green^{commit}" 2>/dev/null || fail "the tested commit $green is not in this repository"
git merge-base --is-ancestor "$green" HEAD || fail "the tested commit $green is not an ancestor of HEAD"
git diff --quiet "$green" HEAD -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md' \
	|| fail "code changed after the tested commit $green: the runs do not cover HEAD"
test -z "$(git status --porcelain -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md')" \
	|| fail "uncommitted or untracked code in the working tree: the runs do not cover what this gate built"
git merge-base --is-ancestor "$green" "origin/feature/$slug" 2>/dev/null \
	|| fail "the tested commit $green is not on the pushed branch origin/feature/$slug"
git merge-base --is-ancestor origin/main "$green" || fail "the tested commit $green does not contain origin/main"

# Everything this cycle adds, and its history, are free of site-private detail and of node names.
scan_cycle evidence

echo "P5 gate: green (P1-P4 on the final tree: $GATE_LINE; ledger; 3 targets passed on $green with GCC 13+; check steps:$TIMES_LINE sacct agrees; scan clean)"
