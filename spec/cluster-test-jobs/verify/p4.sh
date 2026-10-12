#!/bin/sh
# P4 gate: the real cluster runs, as recorded (redacted) in spec/cluster-test-jobs/evidence/. Three
# green runs of one pushed commit whose code equals HEAD's, each with GCC 13 or newer on the
# architecture its target promises and the committed job script; a negative control with a hash not
# in the repository; a real red build of an earlier commit of this branch; sacct's state and exit code
# for all five jobs; the site-private scan of files and history; the deferral ledger
# (R1, R11; R3 and R7 on a cluster).
# This checks the evidence for consistency, not provenance: the orchestrator grades R11 from the job
# ids, sacct and the session's MCP transcript.
set -u
. "$(dirname "$0")/common.sh"
need git; need python3
ev="$root/spec/cluster-test-jobs/evidence"
test -d "$ev" || fail "no evidence directory spec/cluster-test-jobs/evidence (R11)"
cd "$root" || fail "cd $root"

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
	test -f "$ev/$1.out" || fail "no scheduler output evidence/$1.out (R11)"
	sed -n 's/^mps-test: script \([0-9a-f]\{40\}\)$/\1/p' "$ev/$1.out" | head -n 1
}
ledger() { # LABEL: "job_id state exit_code" from evidence/jobs.tsv
	awk -F '\t' -v l="$1" '$1 == l { print $2, $3, $4 }' "$ev/jobs.tsv"
}
test -f "$ev/jobs.tsv" || fail "no evidence/jobs.tsv (label, job id, sacct state, sacct exit code)"
common_checks() { # LABEL FILE: on every one of the five records
	tg=$(field "$2" target)
	case " $REAL_TARGETS " in *" $tg "*) ;; *) fail "$1: target '$tg' is not one of $REAL_TARGETS" ;; esac
	test "$(field "$2" cluster)" = '<cluster>' || fail "$1: cluster is not redacted to <cluster> (R9)"
	test "$(field "$2" host)" = '<host>' || fail "$1: host is not redacted to <host> (R9)"
	j=$(field "$2" job_id)
	printf '%s\n' "$j" | grep -Eqx '[0-9]+' || fail "$1: job id '$j' is not a Slurm job id"
	case " $ids " in *" $j "*) fail "$1: job id $j reused" ;; esac
	ids="$ids $j"
	set -- "$1" "$2" $(ledger "$1")
	test "$#" -eq 5 || fail "$1: no single line in evidence/jobs.tsv"
	test "$3" = "$j" || fail "$1: jobs.tsv says job $3, result.json says $j"
	sacct_state=$4 sacct_exit=$5
}

# Three green runs (R11).
green=
ids=
for t in x86-64-avx512:x86_64 x86-64-avx2:x86_64 aarch64-gb10:aarch64; do
	name=${t%%:*} arch=${t#*:}
	f="$ev/$name.json"
	result_check "$f" failed=none target="$name" arch="$arch"
	common_checks "$name" "$f"
	test "$sacct_state $sacct_exit" = "COMPLETED 0:0" || fail "$name: sacct says $sacct_state $sacct_exit, want COMPLETED 0:0"
	test "$(step_codes "$f")" = PPPPP || fail "$name: not every step passed (R11)"
	field "$f" compiler | grep -Eq 'GCC\) (1[3-9]|[2-9][0-9])\.' || fail "$name: compiler '$(field "$f" compiler)' is not GCC 13 or newer (R11)"
	c=$(field "$f" commit)
	if [ -z "$green" ]; then green=$c; else test "$c" = "$green" || fail "$name tested $c, another target tested $green: not one head (R11)"; fi
	test "$(out_script "$name")" = "$(git rev-parse "$c:ci/test-commit.sh")" \
		|| fail "$name: the script that ran is not $c's ci/test-commit.sh (R1)"
done
git cat-file -e "$green^{commit}" 2>/dev/null || fail "the tested commit $green is not in this repository"
git merge-base --is-ancestor "$green" HEAD || fail "the tested commit $green is not an ancestor of HEAD"
git diff --quiet "$green" HEAD -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md' \
	|| fail "code changed after the tested commit $green: the runs do not cover HEAD (R11)"
git merge-base --is-ancestor "$green" "origin/feature/cluster-test-jobs" 2>/dev/null \
	|| fail "the tested commit $green is not on the pushed branch origin/feature/cluster-test-jobs (R11: pushed head)"
git merge-base --is-ancestor origin/main "$green" || fail "the tested commit $green does not contain origin/main (the D1 fix)"

# The negative control (R3 on a cluster, R11).
f="$ev/not-a-commit.json"
result_check "$f" failed=checkout commit=null
common_checks not-a-commit "$f"
test "$sacct_state $sacct_exit" = "FAILED 1:0" || fail "not-a-commit: sacct says $sacct_state $sacct_exit, want FAILED 1:0"
test "$(step_codes "$f")" = PFNNN || fail "not-a-commit: steps are $(step_codes "$f"), want PFNNN (R3)"
h=$(field "$f" requested)
printf '%s\n' "$h" | grep -Eqx '[0-9a-f]{7,40}' || fail "not-a-commit: '$h' is not a well-formed hash, so the control proves nothing"
git rev-parse --verify --quiet "$h^{commit}" >/dev/null && fail "not-a-commit: $h is a commit in this repository"
grep -q 'not a commit' "$ev/not-a-commit.out" && grep -qF "$h" "$ev/not-a-commit.out" \
	|| fail "not-a-commit.out does not show 'not a commit' and the hash (R3)"
test "$(out_script not-a-commit)" = "$(git rev-parse "$green:ci/test-commit.sh")" \
	|| fail "not-a-commit: the script that ran is not the tested commit's ci/test-commit.sh"

# A real red build: an earlier commit of this branch, with ci/, that GCC on Linux cannot compile.
f="$ev/red-build.json"
result_check "$f" failed=build
common_checks red-build "$f"
test "$sacct_state $sacct_exit" = "FAILED 1:0" || fail "red-build: sacct says $sacct_state $sacct_exit, want FAILED 1:0"
test "$(step_codes "$f")" = PPPFN || fail "red-build: steps are $(step_codes "$f"), want PPPFN"
rb=$(field "$f" commit)
test "$rb" != "$green" || fail "red-build tested the green commit"
git merge-base --is-ancestor "$rb" HEAD || fail "red-build commit $rb is not an ancestor of HEAD"
git cat-file -e "$rb:ci/test-commit.sh" 2>/dev/null || fail "red-build commit $rb has no ci/test-commit.sh"
test "$(out_script red-build)" = "$(git rev-parse "$rb:ci/test-commit.sh")" || fail "red-build: the script that ran is not $rb's"
field "$f" compiler | grep -Eq 'GCC\) (1[3-9]|[2-9][0-9])\.' || fail "red-build was not a GCC 13+ build"
grep -q 'error:' "$ev/red-build.out" || fail "red-build.out does not show the compiler error"

# Everything this cycle adds, and its history, are free of site-private detail (R9).
scan_cycle

# Deferral ledger: every seed is indexed, and every issue TECH.md names exists.
seeds=$(ls issues/*.md | wc -l | tr -d ' ')
entries=$(grep -c '^\*\*Seed:\*\*' ROADMAP.md)
test "$seeds" -eq "$entries" || fail "ROADMAP has $entries Seed lines for $seeds seeds"
test -f spec/cluster-test-jobs/TECH.md || fail "no TECH.md"
for p in $(grep -oE 'issues/[a-z0-9-]+\.md' spec/cluster-test-jobs/TECH.md | sort -u); do
	test -f "$p" || fail "TECH.md names $p, which does not exist (deferral ledger)"
	grep -qF "$p" ROADMAP.md || fail "$p has no ROADMAP entry (deferral ledger)"
done

echo "P4 gate: green (3 targets passed on $green with GCC 13+ and the committed script; not-a-commit and a real red build; sacct for 5 jobs; scan clean; ledger complete)"
