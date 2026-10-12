#!/bin/sh
# P1 gate: the job's happy path on the Mac. A copy of the job, run from an unrelated directory, loads
# the fake login profile when module is missing (with HOME pointing nowhere), clones the fixture over
# file://, checks out exactly the commit (full and short hash; the fixture's HEAD is newer), loads the
# target's modules, builds and checks with the target's make variables and the loaded toolchain,
# writes a correct result.json and one log per step, rebuilds from scratch without touching the first
# run, survives a polluted environment and GNU tools, writes nothing outside its run directory and runs
# no interpreter; the header and AGENTS.md document it (R1, R2, R5, R6, R7, R10, R12).
set -u
. "$(dirname "$0")/common.sh"
need git; need dash; need clang++; need python3; need rsync
require_job

# Static checks: a POSIX sh script with its header, no interpreter, no bashism, no /tmp, no spec ids.
head -n 1 "$JOB" | grep -qx '#!/bin/sh' || fail "ci/test-commit.sh does not start with #!/bin/sh (R10)"
sed -n 2,3p "$JOB" | grep -q '^# SPDX-FileCopyrightText: ' || fail "ci/test-commit.sh lacks the SPDX copyright line"
sed -n 2,3p "$JOB" | grep -qx '# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception' \
	|| fail "ci/test-commit.sh lacks the SPDX license line"
dash -n "$JOB" || fail "ci/test-commit.sh is not valid for dash (R10)"
/bin/sh -n "$JOB" || fail "ci/test-commit.sh is not valid for sh (R10)"
code=$(grep -nv '^[[:space:]]*#' "$JOB")
hit=$(printf '%s\n' "$code" | grep -E '(^|[^A-Za-z0-9_])(python[0-9.]*|jq|perl|ruby|node|bash)([^A-Za-z0-9_]|$)' || true)
test -z "$hit" || { echo "$hit" >&2; fail "ci/test-commit.sh names an interpreter beyond sh (R10)"; }
hit=$(printf '%s\n' "$code" | grep -E '\[\[|<<<|\$'"'"'|^[0-9]+:[[:space:]]*(function|local|declare|typeset|source)[[:space:]]|\$\{[A-Za-z_]+//' || true)
test -z "$hit" || { echo "$hit" >&2; fail "ci/test-commit.sh uses a bashism (R10)"; }
hit=$(printf '%s\n' "$code" | grep -E "(^|[[:space:]=:])['\"]?/tmp([/[:space:]'\"]|\$)" || true)
test -z "$hit" || { echo "$hit" >&2; fail "ci/test-commit.sh writes under /tmp (R5)"; }
hit=$(grep -nE '(^|[^A-Za-z0-9_])(R[0-9]{1,2}|P[0-9])([^A-Za-z0-9_]|$)' "$JOB" || true)
test -z "$hit" || { echo "$hit" >&2; fail "ci/test-commit.sh cites spec ids (Conventions: no R#/P# in source)"; }
grep -qF "$DEFAULT_URL" "$JOB" || fail "ci/test-commit.sh does not name $DEFAULT_URL as its default (R2)"
awk '{ if (sub(/\\$/, "")) { buf = buf $0 " "; next } print buf $0; buf = "" }' "$JOB" \
	| grep -E '^[[:space:]]*unset[[:space:]]' >"$work/unsets"
for v in CDPATH GIT_DIR GIT_WORK_TREE GIT_CONFIG_PARAMETERS MAKEFLAGS MAKELEVEL MFLAGS CXX CXXFLAGS; do
	grep -qw -- "$v" "$work/unsets" || fail "ci/test-commit.sh does not unset the inherited $v"
done

# R12: the header is the manual.
awk 'NR == 1 { next } /^#/ { print; next } { exit }' "$JOB" >"$work/header"
for word in HASH TARGET MPS_REPO_URL MPS_SCRATCH SCRATCH MPS_SLURM_ACCOUNT MPS_SLURM_QOS \
	'mpsearch/runs/<hash>/<target>/<jobid>/' result.json "$DEFAULT_URL" /etc/profile \
	--partition --gpus --cpus-per-task --mem --time --output; do
	grep -qF -- "$word" "$work/header" || fail "the job's header does not state $word (R12)"
done
for s in $STEPS; do
	grep -qF "$s.log" "$work/header" || fail "the job's header does not list $s.log in the run-directory layout (R12)"
done
awk '/^## Map/ { on = 1; next } /^## / { on = 0 } on' "$root/AGENTS.md" >"$work/map"
grep -qF 'ci/test-commit.sh' "$work/map" || fail "AGENTS.md Map does not name ci/test-commit.sh (R12)"
grep -qF 'ci/targets/' "$work/map" || fail "AGENTS.md Map does not name ci/targets/ (R12)"
grep -E '^\| cluster test \|' "$root/AGENTS.md" | grep -qF 'ci/test-commit.sh' \
	|| fail "AGENTS.md Profile has no '| cluster test |' row naming ci/test-commit.sh (R12)"

make_stubs
make_fixture
makeline=$(/usr/bin/make --version 2>/dev/null | head -n 1)
host=$(uname -n)
arch=$(uname -m)
test -n "$makeline" || fail "cannot compute the expected make line"
blob=$(git hash-object "$work/submit/test-commit.sh")
test "$newer" != "$good" || fail "fixture: HEAD is the tested commit"

# happy_checks NAME RUN REQUESTED JOBID: everything a green run must show
happy_checks() {
	result_check "$2/result.json" failed=none requested="$3" commit="$good" target=local-stub \
		cluster=localtest host="$host" arch="$arch" job_id="$4" compiler="$WRAPPER_LINE" make="$makeline" \
		not_before="$t_before" not_after="$t_after"
	test "$(git -C "$2/src" rev-parse HEAD)" = "$good" || fail "$1: HEAD in the build tree is not $good (R2)"
	test ! -e "$2/src/NEWER" || fail "$1: the build tree holds the fixture's newer commit (R2)"
	test ! -e "$2/src/.git/objects/info/alternates" || fail "$1: the clone borrows objects through alternates (R5)"
	expect_logs "$2" "$STEPS"
	test "$(step_lines "$1")" = "$STEPS" || { show "$1"; fail "$1: steps announced as '$(step_lines "$1")', want '$STEPS' (R6)"; }
	test "$(script_line "$1")" = "$blob" || { show "$1"; fail "$1: the job did not print 'mps-test: script $blob', the hash of the script it ran"; }
	p=$(grep -n 'fake-module: purge' "$2/toolchain.log" | head -n 1 | cut -d: -f1)
	l=$(grep -n 'fake-module: load stub/1.0' "$2/toolchain.log" | head -n 1 | cut -d: -f1)
	test -n "$p" && test -n "$l" && test "$p" -lt "$l" || { cat "$2/toolchain.log" >&2; fail "$1: toolchain did not purge, then load the target's module (R6)"; }
	compiled_with "$2/build.log" gate-c++ 6
	compiled_with "$2/check.log" gate-c++ 1
	grep -Eq '^check: [1-9][0-9]* passed, 0 failed$' "$2/check.log" || fail "$1: check.log has no green check summary (R6)"
	grep -Eq '^cli: [1-9][0-9]* passed, 0 failed$' "$2/check.log" || fail "$1: check.log has no green cli summary (R6)"
	test -x "$2/src/bin/mps" || fail "$1: no bin/mps in the build tree"
	find "$2" -name 'result.json.*' | grep -q . && fail "$1: a temporary result file was left behind"
	forbidden_none
}

# Run 1: dash, the full hash, as the MCP server submits: no module command and no scratch variable,
# so the job must load the (fake) login profile, whose SCRATCH it then uses.
reset_g; G_NO_MPS_SCRATCH=1
run_job happy main dash 1001 "$good" local-stub
test "$rc" -eq 0 || { show happy; fail "the happy path exited $rc under dash (R7)"; }
r1=$(rundir main "$good" local-stub 1001)
test "$(profile_line 1001)" = "job=1001 home=absent" \
	|| fail "the job did not load the login profile with HOME pointing at a missing path (profile record: '$(profile_line 1001)')"
happy_checks happy "$r1" "$good" 1001
contained main "$r1"
( cd "$r1" && find . -type f -exec cksum {} + | sort ) >"$work/r1.before"

# Run 2: sh, a 12-digit short hash, the same scratch, module already a command: the profile must not
# be loaded, the run must be new and rebuild everything, and the first run is unreadable meanwhile.
short=$(printf '%.12s' "$good")
reset_g; G_MODULE_EXE=1
chmod 000 "$r1" || fail "chmod the first run"
run_job happy2 main /bin/sh 1002 "$short" local-stub
chmod 755 "$r1" || fail "restore the first run"
test "$rc" -eq 0 || { show happy2; fail "the happy path exited $rc under sh with a short hash (R7)"; }
test -z "$(profile_line 1002)" || fail "the job loaded the login profile although module was a command"
r2=$(rundir main "$short" local-stub 1002)
happy_checks happy2 "$r2" "$short" 1002
n1=$(grep -c -- ' -c ' "$r1/build.log")
n2=$(grep -c -- ' -c ' "$r2/build.log")
test "$n2" -eq "$n1" || fail "the second run compiled $n2 files, the first $n1: a build was reused (R5)"
( cd "$r1" && find . -type f -exec cksum {} + | sort ) >"$work/r1.after"
cmp -s "$work/r1.before" "$work/r1.after" || fail "the second run changed the first run's directory (R5)"
contained main "$r1" "$r2"

# Run 3: dash under a polluted environment; it must not change the outcome.
git -C "$work/decoy" rev-parse --verify -q HEAD >/dev/null && fail "decoy repository already has commits"
reset_g; G_POLLUTE=1
run_job polluted polluted dash 1003 "$good" local-stub
test "$rc" -eq 0 || { show polluted; fail "the happy path exited $rc under a polluted environment (inherited CDPATH, GIT_*, MAKEFLAGS, CXX)"; }
r3=$(rundir polluted "$good" local-stub 1003)
happy_checks polluted "$r3" "$good" 1003
test -z "$(ls -A "$work/decoy" | grep -v '^\.git$')" || fail "the job wrote into the decoy work tree named by GIT_WORK_TREE"
git -C "$work/decoy" rev-parse --verify -q HEAD >/dev/null && fail "the job committed into the decoy repository named by GIT_DIR"
contained polluted "$r3"

# Run 4: GNU coreutils first on PATH, when Homebrew has them.
gnubin=/opt/homebrew/opt/coreutils/libexec/gnubin
if [ -d "$gnubin" ]; then
	reset_g; G_PATH_PREFIX=$gnubin
	run_job gnu gnu dash 1004 "$good" local-stub
	test "$rc" -eq 0 || { show gnu; fail "the happy path exited $rc with GNU coreutils first on PATH (R10)"; }
	r4=$(rundir gnu "$good" local-stub 1004)
	happy_checks gnu "$r4" "$good" 1004
	contained gnu "$r4"
	gnu_note="GNU coreutils"
else
	echo "gate: note: no Homebrew coreutils at $gnubin; GNU tools are covered by P4's runs on Linux" >&2
	gnu_note="GNU coreutils absent (covered by P4)"
fi

echo "P1 gate: green (profile with HOME absent; dash and sh; full and short hash; gate-c++ -O1 in build and check; fresh rebuild with the first run locked; polluted environment; $gnu_note; contained; header and AGENTS.md)"
