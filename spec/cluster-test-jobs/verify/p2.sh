#!/bin/sh
# P2 gate: every failure path goes red the way the GOAL says. Argument errors (exit 2) and scratch or
# run-directory errors (exit 3) create nothing; a hash that is not a commit, a failed clone, a missing
# target file, a bad module, a wrong partition, a failed build (clean and polluted), a failed check and
# a TERM at the time limit each leave a valid result.json with the right step failed and the rest not
# run, and exit 1 (R3, R4, R5, R6, R7).
set -u
. "$(dirname "$0")/common.sh"
need git; need dash; need clang++; need python3; need rsync; need pgrep
require_job
make_stubs
make_fixture

# expect_arg_error NAME ARGS...: exit 2, a message on stderr, and nothing created at all (R4)
expect_arg_error() {
	n=$1
	shift
	reset_g
	run_job "$n" "arg-$n" dash 2001 "$@"
	test "$rc" -eq 2 || { show "$n"; fail "$n: exited $rc, want 2 (R4)"; }
	test -s "$logs/$n.err" || fail "$n: no message on stderr (R4)"
	contained "arg-$n"
}

expect_arg_error zero-args
grep -qi usage "$logs/zero-args.err" || fail "zero arguments: no usage message (R4)"
expect_arg_error one-arg "$good"
grep -qi usage "$logs/one-arg.err" || fail "one argument: no usage message (R4)"
expect_arg_error three-args "$good" local-stub extra
grep -qi usage "$logs/three-args.err" || fail "three arguments: no usage message (R4)"
i=0
for h in xyz ABCDEF0 abcdef "$(printf '%.41s' "${good}0")" '' "$(printf 'abcdef0\nabcdef0')" '../../x'; do
	i=$((i + 1))
	expect_arg_error "bad-hash-$i" "$h" local-stub
	grep -q 'not a commit' "$logs/bad-hash-$i.err" || { show "bad-hash-$i"; fail "malformed hash '$h': message does not say 'not a commit'"; }
done
i=0
for t in ../x .x -x a/b '' 'a b'; do
	i=$((i + 1))
	expect_arg_error "bad-target-$i" "$good" "$t"
	grep -qi 'target' "$logs/bad-target-$i.err" || { show "bad-target-$i"; fail "malformed target '$t': message does not mention the target (R4)"; }
done

# Scratch (R5): none even after the login profile, relative, missing; nothing created, exit 3.
reset_g; G_NO_MPS_SCRATCH=1; G_NO_PROFILE_SCRATCH=1
run_job no-scratch no-scratch dash 3001 "$good" local-stub
test "$rc" -eq 3 || { show no-scratch; fail "no scratch variable: exited $rc, want 3 (R5)"; }
grep -qi scratch "$logs/no-scratch.err" || fail "no scratch variable: the message does not say scratch (R5)"
test -n "$(profile_line 3001)" || fail "no scratch variable: the job did not try the login profile first"
contained no-scratch
reset_g; G_MPS_SCRATCH=relative/dir
run_job rel-scratch rel-scratch dash 3002 "$good" local-stub
test "$rc" -eq 3 || { show rel-scratch; fail "relative MPS_SCRATCH: exited $rc, want 3 (R5)"; }
contained rel-scratch
reset_g; G_MPS_SCRATCH="$work/no-such-dir"
run_job missing-scratch missing-scratch dash 3003 "$good" local-stub
test "$rc" -eq 3 || { show missing-scratch; fail "missing MPS_SCRATCH directory: exited $rc, want 3 (R5)"; }
test ! -e "$work/no-such-dir" || fail "the job created a missing MPS_SCRATCH directory (R5)"

# Precedence, with module already a command so the profile stays out: SCRATCH alone is used;
# MPS_SCRATCH wins over SCRATCH. Not-a-commit runs are quick.
absent=0123456789abcdef0123456789abcdef01234567
git -C "$fx" cat-file -e "$absent^{commit}" 2>/dev/null && fail "the absent hash exists in the fixture"
reset_g; G_MODULE_EXE=1; G_NO_MPS_SCRATCH=1; G_SCRATCH="$work/sb-only-scratch/scr"
run_job only-scratch only-scratch dash 3004 "$absent" local-stub
test -f "$(rundir only-scratch "$absent" local-stub 3004)/result.json" || { show only-scratch; fail "with only SCRATCH set, the run is not under SCRATCH (R5)"; }
mkdir -p "$work/sb-both/other" || fail "mkdir other"
reset_g; G_MODULE_EXE=1; G_SCRATCH="$work/sb-both/other"
run_job both both dash 3005 "$absent" local-stub
test -f "$(rundir both "$absent" local-stub 3005)/result.json" || { show both; fail "MPS_SCRATCH did not take precedence over SCRATCH (R5)"; }
test -z "$(ls -A "$work/sb-both/other")" || fail "the job wrote under SCRATCH although MPS_SCRATCH was set (R5)"
contained both "$(rundir both "$absent" local-stub 3005)"

# The run directory already exists (R5): exit 3, untouched.
pre=$(rundir exists "$good" local-stub 3006)
mkdir -p "$pre" && : >"$pre/sentinel" || fail "pre-create the run directory"
reset_g
run_job exists exists dash 3006 "$good" local-stub
test "$rc" -eq 3 || { show exists; fail "existing run directory: exited $rc, want 3 (R5)"; }
grep -qi exist "$logs/exists.err" || fail "existing run directory: the message does not say so (R5)"
test "$(ls -A "$pre")" = sentinel || fail "the job touched an existing run directory (R5)"

# expect_step_failure NAME SANDBOX JOBID HASH TARGET FAILED RAN [KEY=VALUE...]: exit 1, result.json
# with FAILED failed and the rest not run, logs for the steps in RAN only, contained.
expect_step_failure() {
	sf_n=$1 sf_s=$2 sf_j=$3 sf_h=$4 sf_t=$5 sf_f=$6 sf_ran=$7
	shift 7
	run_job "$sf_n" "$sf_s" dash "$sf_j" "$sf_h" "$sf_t"
	test "$rc" -ne 0 || { show "$sf_n"; fail "$sf_n: exited 0 although $sf_f failed (R7)"; }
	test "$rc" -eq 1 || { show "$sf_n"; fail "$sf_n: exited $rc, want 1 for a failed step (R7)"; }
	sf_r=$(rundir "$sf_s" "$sf_h" "$sf_t" "$sf_j")
	result_check "$sf_r/result.json" failed="$sf_f" requested="$sf_h" target="$sf_t" job_id="$sf_j" \
		not_before="$t_before" not_after="$t_after" "$@"
	expect_logs "$sf_r" "$sf_ran"
	contained "$sf_s" "$sf_r"
	forbidden_none
}

# Not a commit (R3): an absent full hash, an absent short hash, and a hex tag that names a commit.
reset_g
expect_step_failure absent-full absent-full 4001 "$absent" local-stub checkout "clone checkout" commit=null
grep -q "not a commit" "$logs/absent-full.err" && grep -qF "$absent" "$logs/absent-full.err" \
	|| { show absent-full; fail "absent hash: stderr does not say 'not a commit' and name the hash (R3)"; }
grep -q "not a commit" "$(rundir absent-full "$absent" local-stub 4001)/checkout.log" || fail "absent hash: checkout.log lacks the message (R3)"
short=
for c in abcdef0 1234567 fedcba9 7654321 0f0f0f0; do
	git -C "$fx" rev-parse --verify --quiet "$c^{commit}" >/dev/null || { short=$c; break; }
done
test -n "$short" || fail "no absent short hash candidate"
expect_step_failure absent-short absent-short 4002 "$short" local-stub checkout "clone checkout" commit=null
grep -q "not a commit" "$logs/absent-short.err" || fail "absent short hash: no 'not a commit' message (R3)"
expect_step_failure trap-ref trap-ref 4003 "$trap_ref" local-stub checkout "clone checkout" commit=null
grep -q "not a commit" "$logs/trap-ref.err" || fail "a tag named like hex was taken for a commit (R2, R3)"

# A clone that fails.
reset_g; G_URL="file://$work/no-such-repo.git"
expect_step_failure no-clone no-clone 4004 "$good" local-stub clone "clone" commit=null

# No target file (R4): named in the message, no build.
reset_g
expect_step_failure no-target no-target 4005 "$good" no-such-target toolchain "clone checkout toolchain" commit="$good"
grep -q 'no-such-target' "$logs/no-target.err" || fail "missing target: the message does not name it (R4)"

# A module that does not load.
reset_g
expect_step_failure bad-module bad-module 4006 "$good" local-badmodule toolchain "clone checkout toolchain"
grep -q 'nosuch/9' "$(rundir bad-module "$good" local-badmodule 4006)/toolchain.log" || fail "bad module: toolchain.log does not show the refused load"

# A wrong partition, with a cluster name holding a double quote, a backslash and a tab: the refusal
# names the partition, and result.json stays valid with the cluster escaped (the tab dropped).
tab=$(printf '\t')
reset_g; G_PARTITION='else\cwhere'; G_CLUSTER="we\"ird\\clus${tab}ter"
expect_step_failure wrong-partition wrong-partition 4007 "$good" local-stub toolchain "clone checkout toolchain" \
	cluster='we"ird\cluster'
grep -qF 'else\cwhere' "$logs/wrong-partition.err" && grep -q "partition" "$logs/wrong-partition.err" \
	|| { show wrong-partition; fail "wrong partition: the message does not name it verbatim (an echo that eats backslashes?)"; }

# A build that fails, clean and under a polluted environment (MAKEFLAGS=n would turn it green), and a
# check that fails (R6).
reset_g
expect_step_failure red-build red-build 4008 "$red_build" local-stub build "clone checkout toolchain build" commit="$red_build"
grep -q 'deliberate gate failure' "$logs/red-build.err" || { show red-build; fail "failed build: the scheduler output does not show the error"; }
reset_g; G_POLLUTE=1
expect_step_failure red-polluted red-polluted 4009 "$red_build" local-stub build "clone checkout toolchain build" commit="$red_build"
grep -q 'deliberate gate failure' "$logs/red-polluted.err" || { show red-polluted; fail "failed build under pollution: the error is not shown"; }
reset_g
expect_step_failure red-check red-check 4010 "$red_check" local-stub check "$STEPS" commit="$red_check"
grep -q ' FAIL gate deliberate failure' "$(rundir red-check "$red_check" local-stub 4010)/check.log" || fail "failed check: check.log lacks the failure"

# TERM at the time limit: the job shell first, then its children, as Slurm does. result.json must
# still be written, with build failed after at least 2 s and check not run.
kill_tree() { # PID SIGNAL: signal the descendants of PID, deepest first
	for c in $(pgrep -P "$1"); do
		kill_tree "$c" "$2"
	done
	kill "-$2" "$1" 2>/dev/null
}
tree_pids() { # PID: PID's descendants
	for c in $(pgrep -P "$1"); do
		echo "$c"
		tree_pids "$c"
	done
}
reset_g
mkdir -p "$work/sb-term/scr" "$work/sb-term/home" "$work/sb-term/tmp" "$work/sb-term/cwd" || fail "mkdir term sandbox"
t_before=$(now)
(
	cd "$work/sb-term/cwd" || exit 125
	exec env -i PATH="$work/bin:/usr/bin:/bin:/usr/sbin:/sbin" HOME="$work/sb-term/home" TMPDIR="$work/sb-term/tmp" LC_ALL=C \
		GATE_SANDBOX="$work/sb-term" SLURM_JOB_ID=5001 SLURM_CLUSTER_NAME=localtest SLURM_JOB_PARTITION=local \
		MPS_REPO_URL="$url" MPS_SCRATCH="$work/sb-term/scr" dash "$work/submit/test-commit.sh" "$slow" local-stub
) >"$logs/term.out" 2>"$logs/term.err" &
pid=$!
waited=0
until grep -qx 'mps-test: build' "$logs/term.out"; do
	sleep 1
	waited=$((waited + 1))
	if [ "$waited" -ge 120 ] || ! kill -0 "$pid" 2>/dev/null; then
		kill_tree "$pid" KILL; show term; fail "the slow build never started"
	fi
done
sleep 3
kids=$(tree_pids "$pid")
test -n "$kids" || fail "TERM: the job has no child processes mid-build"
kill -TERM "$pid"
sleep 1
for c in $kids; do kill -TERM "$c" 2>/dev/null; done
waited=0
while kill -0 "$pid" 2>/dev/null; do
	sleep 1
	waited=$((waited + 1))
	[ "$waited" -lt 30 ] || { kill_tree "$pid" KILL; fail "the job did not exit within 30 s of TERM"; }
done
wait "$pid"
rc=$?
t_after=$(now)
test "$rc" -ne 0 || fail "TERM: the job exited 0 (R7)"
r=$(rundir term "$slow" local-stub 5001)
result_check "$r/result.json" failed=build commit="$slow" job_id=5001 min_seconds=build:2 \
	not_before="$t_before" not_after="$t_after"
grep -q 'interrupted' "$logs/term.err" || { show term; fail "TERM: no 'interrupted' message: the trap did not run"; }
sleep 1
for c in $kids; do
	kill -0 "$c" 2>/dev/null && { kill -KILL "$c" 2>/dev/null; fail "TERM: process $c of the job outlived it"; }
done
contained term "$r"

echo "P2 gate: green (16 malformed or miscounted arguments; 6 scratch and run-directory cases; 3 not-a-commit; clone, target, module, partition, build (clean and polluted), check and TERM failures all red and recorded)"
