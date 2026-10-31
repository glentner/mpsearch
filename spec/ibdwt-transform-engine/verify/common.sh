# Shared helpers for this cycle's phase gates (a cycle record, not a standing gate).
# Sourced by p1.sh, p2.sh, p3.sh, p4.sh, prepush.sh and redact.sh, after `PHASE=<n>`. POSIX sh.
#
# A gate copies the tree as a clone would hold it (untracked files included), builds it with
# clang++ and g++-16 under GNU make 3.81 behind a shim that logs every compiler call, and checks,
# in order: the files PLAN § 2 lets the phase change and the interface it pins (files.py); the
# numeric flags (R10) and standard C++ alone (R11); make check on both compilers; the length table
# against its measurement and a re-run of the measurement's boundary probes (lengths.py, R3); the
# gate's own probe of the engine on both builds, their residue digests equal (probe.cpp, R1, R4,
# R6-R9, R14); the builder's checks against the mutants (interpose.cpp); from P2, the engine tier
# on both compilers, R12's time on clang, and a traced, sabotaged run that every graded engine-tier
# check must fail (trace.py); make check's time against 0bc49cb (R12); the note and the
# bibliography (docs.py, R13). A gate prints `GATE FAIL: <reason>` and exits 1 at the first
# failure, or ends with a `<gate>: green (...)` line.
#
# Modes: `full` (the default) as above; `quick`, the cheap partial mode: the files, the flags and
# C++ checks, make check with clang only, the table against its measurement with the boundary
# probes of eight lengths, the probe in quick mode, the mutants, the default tier sabotaged, the
# note; no engine tier, no GCC, no timing.
#
# Footprint: at most two jobs at once (two compiles, a make check beside a measurement probe, or
# the GCC engine tier beside the sabotaged run), each one process tree in its own process group,
# well under 2 GB together; the clang engine tier runs beside at most one boundary-probe worker.
# A full gate takes about 36 to 47 minutes on an M2 Mac from P2 on (about 8 in P1), a quick one about 3.
# Run one gate at a time.
#
# GATE_SELFTEST=1 is the planner's self-test against defective trees: the frozen-gate check
# skipped, every failure printed, and never a green line: exit 1 after any failure, 3 after none.

set -u
umask 022

# The tree under test is the one holding this script, whatever the caller's directory.
root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "gate: not in a git tree" >&2; exit 1; }
slug=ibdwt-transform-engine
spec=spec/$slug
here="$root/$spec/verify"
BASE=0bc49cb
DEFAULT_BUDGET=5
HEAVY_BUDGET=900
SELFTEST=${GATE_SELFTEST:-}
FAILS=0
MODE=${GATE_MODE:-full}

# Nothing in the caller's environment steers make, the compiler, git, Python or the interposer
unset MAKEFLAGS MAKELEVEL MFLAGS MAKEFILES GNUMAKEFLAGS MAKEOVERRIDES CXX CXXFLAGS CPPFLAGS IEEE BOUNDS \
	PYTHONDONTWRITEBYTECODE PYTHONPYCACHEPREFIX GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE MPS_GATE_LOG \
	GATE_MUTANT GATE_TRACE

# die: a failure the gate cannot go on from, self-test or not
die() {
	echo "GATE FAIL: $*" >&2
	exit 1
}

# fail: a check failed; the self-test counts it and goes on
fail() {
	echo "GATE FAIL: $*" >&2
	[ -n "$SELFTEST" ] || exit 1
	FAILS=$((FAILS + 1))
	return 1
}

need() {
	command -v "$1" >/dev/null 2>&1 || die "required tool not found: $1"
}

case $MODE in full|quick) ;; *) die "GATE_MODE is $MODE, not full or quick" ;; esac

work=$(mktemp -d "${TMPDIR:-/tmp}/mps-gate.XXXXXX") || die "mktemp failed"
logs="$work/logs"
mkdir -p "$logs" "$work/rc" "$work/pg" "$work/shim" || die "mkdir under $work"
export GATE_WORK="$work"

# cleanup: every process group the gate started (each command under `limit` gets its own, recorded
# in $work/pg), then the gate's own jobs; then the gate's scratch directory, by Python's shutil (no
# rm involved), after checking it is this gate's mktemp directory and nothing else; then that no
# process of this gate's is left
cleanup() {
	trap '' INT TERM HUP
	for cl_sig in TERM KILL; do
		for cl_f in "$work"/pg/*; do
			[ -f "$cl_f" ] && kill -s "$cl_sig" -- "-${cl_f##*/}" 2>/dev/null
		done
		for cl_p in $(jobs -p); do
			kill -s "$cl_sig" "$cl_p" 2>/dev/null
		done
		perl -e 'select(undef, undef, undef, 0.5)'
	done
	wait 2>/dev/null
	chmod -R u+rwx "$work" 2>/dev/null
	python3 -I - "$work" "${TMPDIR:-/tmp}" <<'PY'
import os, shutil, sys
w, t = os.path.realpath(sys.argv[1]), os.path.realpath(sys.argv[2])
if os.path.dirname(w) != t or not os.path.basename(w).startswith("mps-gate."):
    sys.exit("gate: refusing to delete %s, which is not this gate's own temporary directory" % w)
shutil.rmtree(w, ignore_errors=True)
PY
	if pgrep -f "$work" >/dev/null 2>&1; then
		echo "gate: processes still name $work after cleanup" >&2
	fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

# GNU make 3.81 is what macOS ships, and AGENTS.md holds the build to it.
MAKE=/usr/bin/make
"$MAKE" --version 2>/dev/null | head -1 | grep -q '^GNU Make 3\.81' || die "need GNU Make 3.81 at $MAKE"
need clang++; need g++-16; need rsync; need git; need perl; need python3; need tar; need cmp; need pgrep; need otool
COMPILERS='clang++ g++-16'
[ "$MODE" = quick ] && COMPILERS='clang++'

# The gates are frozen at the plan commit: this directory must match it, with nothing added.
( cd "$root" && git cat-file -e "$BASE^{commit}" ) 2>/dev/null || die "no commit $BASE in this repository"
if [ -z "$SELFTEST" ]; then
	plan_commit=$(cd "$root" && git log -1 --format=%H --grep="^\[feature\] Plan $slug" -- "$spec/verify")
	test -n "$plan_commit" || die "no plan commit for $spec/verify"
	( cd "$root" && git diff --quiet "$plan_commit" -- "$spec/verify" ) \
		|| die "$spec/verify differs from the plan commit $plan_commit: the gates are frozen"
	test -z "$(cd "$root" && git ls-files --others --exclude-standard -- "$spec/verify")" \
		|| die "untracked files in $spec/verify: the gates are frozen"
	# A plan made after code landed is no plan: nothing but spec/ and issues/ changed between
	# 0bc49cb and it
	test -z "$(cd "$root" && git diff --name-only "$BASE" "$plan_commit" -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md')" \
		|| die "the plan commit $plan_commit follows changes outside spec/: the gates are frozen at the plan made before the build"
fi
( cd "$root" && git rev-parse -q --verify origin/main >/dev/null ) || die "no origin/main in this repository"

# limit SECONDS CMD...: CMD in a process group of its own, recorded in $work/pg while it runs, and
# killed with its whole group after SECONDS or when the gate ends, so no make, compiler, check or
# probe outlives the gate. Exits with CMD's status (128 + the signal if killed).
limit() {
	li_t=$1
	shift
	perl -e '
		my ($t, $pg) = (shift, shift);
		$pg eq "$ENV{GATE_WORK}/pg" or die "limit: $pg is not this gate'"'"'s own record directory\n";
		my $pid = fork;
		defined $pid or die "limit: fork: $!\n";
		if (!$pid) { setpgrp(0, 0); exec @ARGV or exit 127 }
		setpgrp($pid, $pid);
		open(my $f, ">", "$pg/$pid") and close($f);
		$SIG{ALRM} = sub { kill "KILL", -$pid };
		$SIG{TERM} = $SIG{INT} = $SIG{HUP} = sub { kill "KILL", -$pid; waitpid $pid, 0; unlink "$pg/$pid"; exit 143 };
		alarm $t;
		waitpid $pid, 0;
		my $st = $?;
		alarm 0;
		unlink "$pg/$pid";
		exit($st & 127 ? 128 + ($st & 127) : $st >> 8)' "$li_t" "$work/pg" "$@"
}

# copy_tree DEST: the files git knows about (tracked, plus untracked and unignored) that exist, as a
# clone would hold them once committed. Ignored build output never comes along.
copy_tree() {
	mkdir -p "$1" || return 1
	( cd "$root" && git ls-files -z --cached --others --exclude-standard | perl -0 -ne 'chomp; print "$_\0" if -e $_' ) \
		| rsync -a --from0 --files-from=- "$root/" "$1/" || return 1
	test ! -e "$1/lib" && test ! -e "$1/bin"
}

# base_copy DEST: 0bc49cb as git archive writes it
base_copy() {
	mkdir -p "$1" && ( cd "$root" && git archive "$BASE" ) | ( cd "$1" && tar -xf - )
}

# The compiler shim: clang++ and g++-16 first on PATH, each logging its own name and arguments to
# $MPS_GATE_LOG, one call per line, then running the real compiler
for sh_c in clang++ g++-16; do
	sh_real=$(command -v "$sh_c")
	cat >"$work/shim/$sh_c" <<EOF
#!/bin/sh
{ printf '%s' "\${0##*/}"; for a in "\$@"; do printf ' %s' "\$a"; done; printf '\\n'; } >>"\${MPS_GATE_LOG:-/dev/null}"
exec "$sh_real" "\$@"
EOF
	chmod +x "$work/shim/$sh_c" || die "shim for $sh_c"
done

# mk DIR CXX LOGNAME SECONDS MAKEARGS...: make in DIR with the shim first on PATH; output in
# $logs/LOGNAME.out, compiler calls in $logs/LOGNAME.calls
mk() {
	mk_d=$1 mk_x=$2 mk_n=$3 mk_t=$4
	shift 4
	( cd "$mk_d" && PATH="$work/shim:$PATH" MPS_GATE_LOG="$logs/$mk_n.calls" limit "$mk_t" "$MAKE" CXX="$mk_x" "$@" ) \
		>"$logs/$mk_n.out" 2>&1
}

# tail_of LOGNAME: the end of a log, for a failure message
tail_of() {
	tail -n 15 "$logs/$1.out" 2>/dev/null | sed 's/^/    /' >&2
}

# The phase's own header for the interposer and the probe
printf '#define GATE_PHASE %s\n' "$PHASE" >"$work/gate_phase.h"

py() { python3 -I -B "$@"; }
