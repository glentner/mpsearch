#!/bin/sh
# P4 gate: everything P3 proves, on the final tree; the deferral ledger and R11's seed and its
# ROADMAP entry; and the cluster runs, as recorded (redacted) in spec/cli-and-sieve-fixes/evidence/:
# three passed runs of one pushed commit whose code equals HEAD's, each with GCC 13 or newer and that
# commit's job script, each check step's summaries equal to this tree's; sacct's state and exit code
# for all three; the site-private scan of files and history (R1-R11).
# The evidence is checked for consistency, not provenance: the orchestrator grades the runs from the
# job ids, sacct and the session's MCP transcript.
set -u
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/cluster.sh"

build_all
sieve_behaviour
pval_behaviour
options_behaviour
mutants_setup
mutants_sieve
mutants_pval
mutants_options
hardened_check
caveats_tests
hygiene

cd "$root" || fail "cd $root"

# R11: the follow-up seed, unshaped, carrying the deferred scope, indexed right after this cycle's
seed=issues/verdicts-and-exit-statuses.md
test -f "$seed" || fail "no $seed (R11)"
grep -qx 'status: unshaped' "$seed" || fail "$seed is not status: unshaped (R11)"
grep -qx 'kind: [a-z][a-z-]*' "$seed" || fail "$seed has no kind (R11)"
for needle in '--json' 'exit status' 'verdict' 'result type' 'composite' 'skip-ptest' 'field names' \
              'matched the expected answer' 'issues/burn-in-mode.md' '## Problem' '## Why it was deferred' \
              '## Sketch of the acceptance criteria'; do
	grep -qiF -- "$needle" "$seed" || fail "$seed does not carry '$needle' (R11)"
done
next=$(awk '/^\*\*Seed:\*\*/ { if (hit) { print; exit } if (index($0, "issues/results-and-cli-fixes.md")) hit = 1 }' ROADMAP.md)
case $next in
	*"($seed)"*) ;;
	*) fail "the ROADMAP entry after this cycle's (issues/results-and-cli-fixes.md) is not $seed's: '$next' (R11)" ;;
esac
grep -qx 'status: adopted:cli-and-sieve-fixes' issues/results-and-cli-fixes.md || fail "this cycle's seed is no longer adopted:cli-and-sieve-fixes"

# Deferral ledger: every seed is indexed once, and every issue TECH.md names exists and is indexed.
seeds=$(ls issues/*.md | wc -l | tr -d ' ')
entries=$(grep -c '^\*\*Seed:\*\*' ROADMAP.md)
test "$seeds" -eq "$entries" || fail "ROADMAP has $entries Seed lines for $seeds seeds (deferral ledger)"
for p in issues/*.md; do
	test "$(grep -c "^\*\*Seed:\*\* .*($p)" ROADMAP.md)" -eq 1 || fail "$p is not indexed exactly once in ROADMAP.md (deferral ledger)"
done
for p in $(grep -oE 'issues/[a-z0-9-]+\.md' spec/cli-and-sieve-fixes/TECH.md | sort -u); do
	test -f "$p" || fail "TECH.md names $p, which does not exist (deferral ledger)"
done

# The cluster runs.
ev="$root/spec/cli-and-sieve-fixes/evidence"
test -d "$ev" || fail "no evidence directory spec/cli-and-sieve-fixes/evidence"
test -f "$ev/jobs.tsv" || fail "no evidence/jobs.tsv (label, job id, sacct state, sacct exit code)"
test "$(head -n 1 "$ev/jobs.tsv")" = "$(printf 'label\tjob_id\tstate\texit_code')" || fail "jobs.tsv header"
test "$(awk 'NR > 1' "$ev/jobs.tsv" | wc -l | tr -d ' ')" -eq 3 || fail "jobs.tsv holds other than three jobs"

cat >"$work/fields.py" <<'EOF'
import json, sys
r = json.load(open(sys.argv[1]))
for k in sys.argv[2:]:
    v = r
    for part in k.split("."):
        v = v[int(part)] if isinstance(v, list) else v[part]
    print("null" if v is None else v)
EOF
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
ledger() { # LABEL: "job_id state exit_code" from evidence/jobs.tsv
	awk -F '\t' -v l="$1" '$1 == l { print $2, $3, $4 }' "$ev/jobs.tsv"
}

green=
ids=
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
	set -- $(ledger "$name")
	test "$#" -eq 3 || fail "$name: no single line in evidence/jobs.tsv"
	test "$1" = "$j" || fail "$name: jobs.tsv says job $1, result.json says $j"
	test "$2 $3" = "COMPLETED 0:0" || fail "$name: sacct says $2 $3, want COMPLETED 0:0"
	test "$(step_codes "$f")" = PPPPP || fail "$name: not every step passed"
	field "$f" compiler | grep -Eq 'GCC\) (1[3-9]|[2-9][0-9])\.' || fail "$name: compiler '$(field "$f" compiler)' is not GCC 13 or newer"
	c=$(field "$f" commit)
	if [ -z "$green" ]; then green=$c; else test "$c" = "$green" || fail "$name tested $c, another target tested $green: not one head"; fi
	test "$(out_script "$name")" = "$(git rev-parse "$c:ci/test-commit.sh")" \
		|| fail "$name: the script that ran is not $c's ci/test-commit.sh"
	# The check step ran this tree's checks, all of them, on GCC on Linux
	cl="$ev/$name.check.log"
	test -f "$cl" || fail "no evidence/$name.check.log (the run's check.log, redacted)"
	grep -qx "check: $CHECKS passed, 0 failed" "$cl" || fail "$name: check.log does not say 'check: $CHECKS passed, 0 failed', this tree's count"
	grep -qx "cli: $CLIS passed, 0 failed" "$cl" || fail "$name: check.log does not say 'cli: $CLIS passed, 0 failed', this tree's count"
	grep -q 'g++' "$cl" || fail "$name: check.log shows no g++ compile or link"
done
git cat-file -e "$green^{commit}" 2>/dev/null || fail "the tested commit $green is not in this repository"
git merge-base --is-ancestor "$green" HEAD || fail "the tested commit $green is not an ancestor of HEAD"
git diff --quiet "$green" HEAD -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md' \
	|| fail "code changed after the tested commit $green: the runs do not cover HEAD"
test -z "$(git status --porcelain -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md')" \
	|| fail "uncommitted or untracked code in the working tree: the runs do not cover what this gate built"
git merge-base --is-ancestor "$green" origin/fix/cli-and-sieve-fixes 2>/dev/null \
	|| fail "the tested commit $green is not on the pushed branch origin/fix/cli-and-sieve-fixes"
git merge-base --is-ancestor origin/main "$green" || fail "the tested commit $green does not contain origin/main"

# Everything this cycle adds, and its history, are free of site-private detail.
scan_cycle

echo "P4 gate: green (P1-P3 on the final tree, $MUTANTS mutants caught; R11 seed and ledger; 3 targets passed on $green with GCC 13+, $CHECKS + $CLIS checks each; sacct agrees; scan clean)"
