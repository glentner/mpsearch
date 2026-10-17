# Shared helpers for this cycle's phase gates (a cycle record, not a standing gate).
# Sourced by p1.sh, p2.sh, prepush.sh and redact.sh, after `PHASE=<n>`. POSIX sh.
#
# A gate holds the files PLAN § 2 changes to their pinned text (files.py), then runs four lanes at
# once, each a chain of plain serial makes with GNU make 3.81: for each compiler, lane A builds
# b3a6c5a fresh (the CI probe, `make`, `make check`, three touches) and then the references, each a
# fresh `make -B` of b3a6c5a with one setting changed; lane B walks a copy of the tree through
# compiler and setting changes without -B (PLAN § 6). Every output is then compared byte for byte
# with the reference for its settings (outputs.py), every no-op run must call no compiler and touch
# no file, and a touched source must recompile what it did at b3a6c5a. Then, alone, R4's timing;
# then `make clean`; then R9's imports. A gate prints `GATE FAIL: <reason>` and exits 1 on the first
# failure, or ends with a `<gate>: green` line.
#
# Footprint: at most four makes at once (one compiler process each, or a check binary), then the
# timing runs alone. About 11 minutes on mac-m2. Run one gate at a time.
#
# GATE_SELFTEST='<compilers>' is the planner's self-test against defective Makefiles: only those
# compilers' lane B (and what they compare against), the frozen-gate check skipped, every failure
# printed, and never a green line: exit 1 after any failure, 3 after none.

set -u
umask 022

# The tree under test is the one holding this script, whatever the caller's directory.
root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "gate: not in a git tree" >&2; exit 1; }
slug=objects-follow-the-compiler
spec=spec/$slug
here="$root/$spec/verify"
BASE=b3a6c5a
BUDGET=0.1
SELFTEST=${GATE_SELFTEST:-}
FAILS=0

# Nothing in the caller's environment steers make, the compiler, git or Python's bytecode
unset MAKEFLAGS MAKELEVEL MFLAGS MAKEFILES GNUMAKEFLAGS MAKEOVERRIDES CXX CXXFLAGS CPPFLAGS IEEE BOUNDS \
	PYTHONDONTWRITEBYTECODE PYTHONPYCACHEPREFIX GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE MPS_GATE_LOG

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

work=$(mktemp -d "${TMPDIR:-/tmp}/mps-gate.XXXXXX") || die "mktemp failed"
logs="$work/logs"
mkdir -p "$logs" "$work/rc" "$work/pg" "$work/man" "$work/sets" "$work/tag" "$work/shim" || die "mkdir under $work"
export GATE_WORK="$work"

# cleanup: every process group the gate started (each command under `limit` gets its own, recorded
# in $work/pg), then the lanes themselves; then the gate's own scratch directory, by Python's shutil
# (no rm involved), after checking it is this gate's mktemp directory and nothing else
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
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

# GNU make 3.81 is what macOS ships, and AGENTS.md holds the build to it (R6).
MAKE=/usr/bin/make
"$MAKE" --version 2>/dev/null | head -1 | grep -q '^GNU Make 3\.81' || die "need GNU Make 3.81 at $MAKE"
need clang++; need g++-16; need rsync; need git; need perl; need python3; need python3.14; need tar; need cmp
COMPILERS='clang++ g++-16'
other() { if [ "$1" = clang++ ]; then echo g++-16; else echo clang++; fi; }
if [ -n "$SELFTEST" ]; then
	for st_c in $SELFTEST; do
		case " $COMPILERS " in *" $st_c "*) ;; *) die "GATE_SELFTEST names $st_c, not one of $COMPILERS" ;; esac
	done
fi

# The gates are frozen at the plan commit: this directory must match it, with nothing added.
( cd "$root" && git cat-file -e "$BASE^{commit}" ) 2>/dev/null || die "no commit $BASE in this repository"
if [ -z "$SELFTEST" ]; then
	plan_commit=$(cd "$root" && git log -1 --format=%H --grep="^\[fix\] Plan $slug" -- "$spec/verify")
	test -n "$plan_commit" || die "no plan commit for $spec/verify"
	( cd "$root" && git diff --quiet "$plan_commit" -- "$spec/verify" ) \
		|| die "$spec/verify differs from the plan commit $plan_commit: the gates are frozen"
	test -z "$(cd "$root" && git ls-files --others --exclude-standard -- "$spec/verify")" \
		|| die "untracked files in $spec/verify: the gates are frozen"
	# A plan made after code landed is no plan: nothing but spec/ and the seed's adoption changed
	# between b3a6c5a and it
	test -z "$(cd "$root" && git diff --name-only "$BASE" "$plan_commit" -- . ':(exclude)spec' ":(exclude)issues/$slug.md")" \
		|| die "the plan commit $plan_commit follows changes outside spec/: the gates are frozen at the plan made before the build"
fi
( cd "$root" && git rev-parse -q --verify origin/main >/dev/null ) || die "no origin/main in this repository"

# limit SECONDS CMD...: CMD in a process group of its own, recorded in $work/pg while it runs, and
# killed with its whole group after SECONDS or when the gate ends, so no make, compiler or check
# outlives the gate. Exits with CMD's status (128 + the signal if killed).
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

# git_copy DEST: copy_tree, then a repository of its own holding every file, so `git status` there
# shows exactly what a build or an import leaves behind
git_copy() {
	copy_tree "$1" || return 1
	( cd "$1" && git init -q . && git add -A \
		&& git -c user.name=gate -c user.email=gate@invalid -c commit.gpgsign=false -c core.hooksPath=/dev/null \
			commit -q -m tree ) >/dev/null 2>&1
}

# base_copy DEST: b3a6c5a as git archive writes it
base_copy() {
	mkdir -p "$1" && ( cd "$root" && git archive "$BASE" ) | ( cd "$1" && tar -xf - )
}

# The compiler shim: clang++ and g++-16 first on PATH, each logging its own name and arguments to
# $MPS_GATE_LOG, one call per line, then running the real compiler
for sh_c in $COMPILERS; do
	sh_real=$(command -v "$sh_c")
	cat >"$work/shim/$sh_c" <<EOF
#!/bin/sh
{ printf '%s' "\${0##*/}"; for a in "\$@"; do printf ' %s' "\$a"; done; printf '\\n'; } >>"\${MPS_GATE_LOG:-/dev/null}"
exec "$sh_real" "\$@"
EOF
	chmod +x "$work/shim/$sh_c" || die "shim for $sh_c"
done

out() { python3 -I -B "$here/outputs.py" "$@"; }

# The changed settings: each of R2's variables at its b3a6c5a value plus one -include of a header
# that adds a used string, so every object the variable compiles comes out different
for tg_v in CXXFLAGS CPPFLAGS IEEE BOUNDS; do
	tg_n=$(echo "$tg_v" | tr 'A-Z' 'a-z')
	printf 'static const char mps_gate_tag_%s[] __attribute__((used)) = "%s";\n' "$tg_n" "$tg_n" >"$work/tag/$tg_n.h"
done
tagged() {
	case $1 in
		CXXFLAGS) echo "-O3 -Wall -Wextra -include $work/tag/cxxflags.h" ;;
		CPPFLAGS) echo "-I src -MMD -MP -include $work/tag/cppflags.h" ;;
		IEEE)     echo "-std=c++20 -fno-fast-math -ffp-contract=off -include $work/tag/ieee.h" ;;
		BOUNDS)   echo "-D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE -include $work/tag/bounds.h" ;;
	esac
}
VARIANTS='CXXFLAGS CPPFLAGS IEEE BOUNDS'
TOUCHES='src/ansi.cpp src/mp.hpp tests/check_cli.cpp'


# --- The lanes -----------------------------------------------------------------------------------

# lane_fail LANE MSG: record why a lane stopped, and stop it
lane_fail() {
	echo "$2" >"$work/lanefail.$1"
	exit 1
}

# mk LANE STEP DIR MAKEARGS...: make in DIR with the shim first on PATH; its stdout in
# $logs/LANE.STEP.out, stderr in .err, compiler calls in .calls, status in $work/rc/LANE.STEP
mk() {
	mk_l=$1 mk_s=$2 mk_d=$3
	shift 3
	: >"$logs/$mk_l.$mk_s.calls"
	( cd "$mk_d" && MPS_GATE_LOG="$logs/$mk_l.$mk_s.calls" && PATH="$work/shim:$PATH" && export MPS_GATE_LOG PATH \
		&& limit 1500 "$MAKE" "$@" ) >"$logs/$mk_l.$mk_s.out" 2>"$logs/$mk_l.$mk_s.err"
	mk_rc=$?
	echo "$mk_rc" >"$work/rc/$mk_l.$mk_s"
	return "$mk_rc"
}

# man DIR NAME: the manifest of DIR's lib/ and bin/ as $work/man/NAME.json
man() {
	out manifest "$1" "$work/man/$2.json"
}

# probe LANE STEP DIR: ci/test-commit.sh's compiler probe, verbatim, then which of lib/ and bin/
# exist
probe() {
	( cd "$3" && printf 'mps-print-cxx:\n\t@echo $(CXX)\n' | limit 120 "$MAKE" -s -f Makefile -f - mps-print-cxx CXX=g++ ) \
		>"$logs/$1.$2.out" 2>"$logs/$1.$2.err"
	echo $? >"$work/rc/$1.$2"
	( cd "$3" && for pr_d in lib bin; do test -e "$pr_d" && echo "$pr_d"; done ) >"$logs/$1.$2.made"
}

# noop LANE STEP DIR MAKEARGS...: a run that must do nothing, between two manifests
noop() {
	no_l=$1 no_s=$2 no_d=$3
	man "$no_d" "$no_l.$no_s.before" || lane_fail "$no_l" "manifest before $no_s"
	mk "$@" || lane_fail "$no_l" "$no_s: make $* failed with nothing to do"
	man "$no_d" "$no_l.$no_s.after" || lane_fail "$no_l" "manifest after $no_s"
}

# touches LANE DIR CXX: date lib/ and bin/ 20 s back and one file 10 s back, then make check; the
# objects each touch compiled, from the shim's log
touches() {
	for to_t in $TOUCHES; do
		to_n=$(echo "$to_t" | tr '/.' '--')
		out age "$2" -20 && out date "$2/$to_t" -10 || lane_fail "$1" "cannot date files for touch $to_t"
		mk "$1" "touch-$to_n" "$2" CXX="$3" check || lane_fail "$1" "make check after touching $to_t failed"
		out calls "$logs/$1.touch-$to_n.calls" >"$work/sets/$1.touch-$to_n" || lane_fail "$1" "calls after touching $to_t"
	done
}

# lane_a CXX [refs]: b3a6c5a fresh, as CI runs it (the probe, make, make check), the touches; with
# `refs`, then each reference: a fresh make -B of b3a6c5a with one setting changed
lane_a() {
	la_x=$1 la_l=a-$1
	base_copy "$work/base-$la_x" || lane_fail "$la_l" "cannot export $BASE"
	probe "$la_l" probe "$work/base-$la_x"
	mk "$la_l" fresh-make "$work/base-$la_x" CXX="$la_x" || lane_fail "$la_l" "$BASE: make CXX=$la_x failed"
	mk "$la_l" fresh-check "$work/base-$la_x" CXX="$la_x" check || lane_fail "$la_l" "$BASE: make CXX=$la_x check failed"
	man "$work/base-$la_x" "$la_l.default" || lane_fail "$la_l" "manifest"
	touches "$la_l" "$work/base-$la_x" "$la_x"
	[ "${2:-}" = refs ] || exit 0
	base_copy "$work/refs-$la_x" || lane_fail "$la_l" "cannot export $BASE"
	for la_v in $VARIANTS; do
		mk "$la_l" "ref-$la_v" "$work/refs-$la_x" -B CXX="$la_x" "$la_v=$(tagged "$la_v")" bin/mps bin/check \
			|| lane_fail "$la_l" "$BASE: make -B with $la_v changed failed"
		man "$work/refs-$la_x" "$la_l.$la_v" || lane_fail "$la_l" "manifest"
	done
	mk "$la_l" ref-O0 "$work/refs-$la_x" -B CXX="$la_x" CXXFLAGS=-O0 bin/mps || lane_fail "$la_l" "$BASE: make -B CXXFLAGS=-O0 failed"
	man "$work/refs-$la_x" "$la_l.O0" || lane_fail "$la_l" "manifest"
	exit 0
}

# lane_b CXX: the tree, fresh as CI runs it, then switched without -B or make clean (PLAN § 6)
lane_b() {
	lb_x=$1 lb_y=$(other "$1") lb_l=b-$1 lb_d="$work/tree-$1"
	git_copy "$lb_d" || lane_fail "$lb_l" "cannot copy the tree"
	probe "$lb_l" probe "$lb_d"
	mk "$lb_l" fresh-make "$lb_d" CXX="$lb_x" || lane_fail "$lb_l" "make CXX=$lb_x failed in a fresh copy"
	mk "$lb_l" fresh-check "$lb_d" CXX="$lb_x" check || lane_fail "$lb_l" "make CXX=$lb_x check failed in a fresh copy"
	man "$lb_d" "$lb_l.fresh" || lane_fail "$lb_l" "manifest"
	noop "$lb_l" noop-check "$lb_d" CXX="$lb_x" check
	noop "$lb_l" noop-make "$lb_d" CXX="$lb_x"
	touches "$lb_l" "$lb_d" "$lb_x"
	# R1: the other compiler, `make` then `make check`, straight after a build
	mk "$lb_l" switch-make "$lb_d" CXX="$lb_y" || lane_fail "$lb_l" "make CXX=$lb_y after CXX=$lb_x failed"
	man "$lb_d" "$lb_l.switch-make" || lane_fail "$lb_l" "manifest"
	mk "$lb_l" switch-check "$lb_d" CXX="$lb_y" check || lane_fail "$lb_l" "make CXX=$lb_y check after CXX=$lb_x failed"
	man "$lb_d" "$lb_l.switch-check" || lane_fail "$lb_l" "manifest"
	noop "$lb_l" noop-other "$lb_d" CXX="$lb_y" check
	# R7: the probe in a tree another compiler built
	probe "$lb_l" probe-stale "$lb_d"
	# R1: back by the environment's CXX, every output dated 2 s ahead (a build in the same second)
	out age "$lb_d" 2 2>/dev/null
	( CXX=$lb_x && export CXX && mk "$lb_l" back-env "$lb_d" check ) || lane_fail "$lb_l" "CXX=$lb_x make check (environment) failed"
	man "$lb_d" "$lb_l.back-env" || lane_fail "$lb_l" "manifest"
	# R2: each setting changed in turn, then twice unchanged, then the defaults
	for lb_v in $VARIANTS; do
		out age "$lb_d" 2 || lane_fail "$lb_l" "cannot date the outputs"
		mk "$lb_l" "set-$lb_v" "$lb_d" CXX="$lb_x" "$lb_v=$(tagged "$lb_v")" check || lane_fail "$lb_l" "make check with $lb_v changed failed"
		man "$lb_d" "$lb_l.set-$lb_v" || lane_fail "$lb_l" "manifest"
		noop "$lb_l" "noop-$lb_v" "$lb_d" CXX="$lb_x" "$lb_v=$(tagged "$lb_v")" check
	done
	out age "$lb_d" 2 || lane_fail "$lb_l" "cannot date the outputs"
	mk "$lb_l" defaults "$lb_d" CXX="$lb_x" check || lane_fail "$lb_l" "make check back at the defaults failed"
	man "$lb_d" "$lb_l.defaults" || lane_fail "$lb_l" "manifest"
	# R2 through `make`: CXXFLAGS=-O0, then plain make
	out age "$lb_d" 2 || lane_fail "$lb_l" "cannot date the outputs"
	mk "$lb_l" O0 "$lb_d" CXX="$lb_x" CXXFLAGS=-O0 || lane_fail "$lb_l" "make CXXFLAGS=-O0 failed"
	man "$lb_d" "$lb_l.O0" || lane_fail "$lb_l" "manifest"
	out age "$lb_d" 2 || lane_fail "$lb_l" "cannot date the outputs"
	mk "$lb_l" plain "$lb_d" CXX="$lb_x" || lane_fail "$lb_l" "plain make after CXXFLAGS=-O0 failed"
	man "$lb_d" "$lb_l.plain" || lane_fail "$lb_l" "manifest"
	# R5: everything built once more, then what git sees
	mk "$lb_l" refill "$lb_d" CXX="$lb_x" check || lane_fail "$lb_l" "make check failed"
	( cd "$lb_d" && git status --porcelain --ignored --untracked-files=all ) >"$logs/$lb_l.status" 2>&1 \
		|| lane_fail "$lb_l" "git status failed"
	exit 0
}

# lanes: lane B for each compiler under test, lane A for both (with the references for those under
# test), all at once; then each lane's failure, if any, with the tail of its last log
lanes() {
	for la_c in $COMPILERS; do
		if [ -z "$SELFTEST" ] || case " $SELFTEST " in *" $la_c "*) true ;; *) false ;; esac; then
			( lane_a "$la_c" refs ) &
			( lane_b "$la_c" ) &
		else
			( lane_a "$la_c" ) &
		fi
	done
	wait
	for la_f in "$work"/lanefail.*; do
		[ -f "$la_f" ] || continue
		la_n=${la_f##*/lanefail.}
		la_last=$(ls -t "$logs/$la_n".*.err 2>/dev/null | head -n 1)
		[ -n "$la_last" ] && tail -n 15 "$la_last" >&2
		die "lane $la_n: $(cat "$la_f")"
	done
}


# --- The checks, on what the lanes recorded -------------------------------------------------------

# rc_ok LANE.STEP
rc_ok() { test "$(cat "$work/rc/$1" 2>/dev/null)" = 0; }

# same SCOPE REF GOT WHAT
same() {
	out same "$1" "$work/man/$2.json" "$work/man/$3.json" 2>"$work/same.err" \
		|| fail "$4: $(cat "$work/same.err")"
}

# quiet LANE STEP CXX WHAT: a no-op run called no compiler, printed no command and touched no file
quiet() {
	test ! -s "$logs/$1.$2.calls" || fail "$4: make called the compiler with nothing to rebuild: $(head -n 1 "$logs/$1.$2.calls" | cut -c 1-120) (R3)"
	! grep -q "^$3 " "$logs/$1.$2.out" || fail "$4: make printed a compile or link command with nothing to rebuild (R3)"
	out unchanged "$work/man/$1.$2.before.json" "$work/man/$1.$2.after.json" 2>"$work/unchanged.err" \
		|| fail "$4: $(cat "$work/unchanged.err") (R3)"
}

# by LANE STEP CXX WHAT: every compiler call in the run was by CXX, and the run linked
by() {
	out calls "$logs/$1.$2.calls" "$3" >"$work/calls.txt" 2>"$work/calls.err" || fail "$4: $(cat "$work/calls.err") (R1)"
	grep -qx 'bin/mps' "$work/calls.txt" || fail "$4: the run linked no bin/mps (R1)"
}

check_r7() { # CXX
	c7_x=$1
	for c7_s in a-$c7_x.probe b-$c7_x.probe b-$c7_x.probe-stale; do
		case $c7_s in a-*) c7_w="$BASE" ;; *) c7_w="the tree" ;; esac
		[ "$c7_s" = "a-$c7_x.probe" ] && ! rc_ok "$c7_s" && die "$BASE: the CI probe fails here; the gate's environment is wrong"
		rc_ok "$c7_s" || fail "$c7_w: the CI probe (${c7_s#*.}) exits $(cat "$work/rc/$c7_s") (R7)"
		test "$(cat "$logs/$c7_s.out")" = g++ && test "$(wc -l <"$logs/$c7_s.out" | tr -d ' ')" = 1 \
			|| fail "$c7_w: the CI probe (${c7_s#*.}) printed '$(head -c 80 "$logs/$c7_s.out")', not exactly the compiler name (R7)"
		test ! -s "$logs/$c7_s.err" || fail "$c7_w: the CI probe (${c7_s#*.}) wrote to stderr: $(head -c 120 "$logs/$c7_s.err") (R7)"
	done
	test ! -s "$logs/b-$c7_x.probe.made" || fail "the CI probe in a fresh copy left $(tr '\n' ' ' <"$logs/b-$c7_x.probe.made")behind: make would not run as at $BASE (R7)"
	for c7_s in fresh-make fresh-check; do
		out commands "$logs/a-$c7_x.$c7_s.out" "$c7_x" >"$work/cmd-base"
		out commands "$logs/b-$c7_x.$c7_s.out" "$c7_x" >"$work/cmd-tree"
		test -s "$work/cmd-base" || die "$BASE printed no commands for a fresh ${c7_s#fresh-} with $c7_x"
		cmp -s "$work/cmd-base" "$work/cmd-tree" \
			|| fail "$c7_x: a fresh-clone ${c7_s#fresh-} runs other commands than at $BASE: $(diff "$work/cmd-base" "$work/cmd-tree" | sed -n 2p | cut -c 1-140) (R7)"
	done
	out summaries "$logs/a-$c7_x.fresh-check.out" >"$work/sum-base" || die "$BASE: make check with $c7_x is not green"
	out summaries "$logs/b-$c7_x.fresh-check.out" >"$work/sum-tree" 2>"$work/sum.err" || fail "$c7_x: fresh make check: $(cat "$work/sum.err") (R7)"
	cmp -s "$work/sum-base" "$work/sum-tree" || fail "$c7_x: make check's summaries differ from $BASE's (R7)"
	same all "a-$c7_x.default" "b-$c7_x.fresh" "$c7_x: a fresh-clone make check (R7)"
	CHECKS=$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$work/sum-tree")
	CLIS=$(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$work/sum-tree")
}

check_r3() { # CXX
	c3_x=$1 c3_y=$(other "$1")
	quiet "b-$c3_x" noop-check "$c3_x" "$c3_x: make check twice"
	quiet "b-$c3_x" noop-make "$c3_x" "$c3_x: make after make check"
	quiet "b-$c3_x" noop-other "$c3_y" "$c3_y: make check twice, after a switch"
	for c3_v in $VARIANTS; do
		quiet "b-$c3_x" "noop-$c3_v" "$c3_x" "$c3_x: make check twice with $c3_v changed"
	done
	c3_all=$(cat "$logs/a-$c3_x.fresh-make.calls" "$logs/a-$c3_x.fresh-check.calls" | wc -l | tr -d ' ')
	for c3_t in $TOUCHES; do
		c3_n=$(echo "$c3_t" | tr '/.' '--')
		test -s "$work/sets/a-$c3_x.touch-$c3_n" || die "$BASE recompiled nothing after touching $c3_t: the touch proves nothing"
		test "$(wc -l <"$work/sets/a-$c3_x.touch-$c3_n" | tr -d ' ')" -lt "$c3_all" \
			|| die "$BASE rebuilt everything after touching $c3_t: the touch cannot tell an incremental build"
		cmp -s "$work/sets/a-$c3_x.touch-$c3_n" "$work/sets/b-$c3_x.touch-$c3_n" \
			|| fail "$c3_x: touching $c3_t rebuilt $(tr '\n' ' ' <"$work/sets/b-$c3_x.touch-$c3_n" | cut -c 1-160) but $BASE rebuilt $(tr '\n' ' ' <"$work/sets/a-$c3_x.touch-$c3_n" | cut -c 1-160) (R3)"
	done
}

check_r1() { # CXX
	c1_x=$1 c1_y=$(other "$1")
	same mps "a-$c1_y.default" "b-$c1_x.switch-make" "make CXX=$c1_y after make check with $c1_x (R1)" \
		&& by "b-$c1_x" switch-make "$c1_y" "make CXX=$c1_y after $c1_x"
	same all "a-$c1_y.default" "b-$c1_x.switch-check" "make CXX=$c1_y check after $c1_x (R1)"
	rc_ok "b-$c1_x.switch-check" && out summaries "$logs/b-$c1_x.switch-check.out" >/dev/null 2>&1 \
		|| fail "make CXX=$c1_y check after $c1_x is not green (R1)"
	same all "a-$c1_x.default" "b-$c1_x.back-env" "CXX=$c1_x (environment) make check after $c1_y, outputs dated ahead (R1)" \
		&& by "b-$c1_x" back-env "$c1_x" "CXX=$c1_x (environment) make check after $c1_y"
}

check_r2() { # CXX
	c2_x=$1 c2_prev=a-$1.default
	for c2_v in $VARIANTS; do
		if [ "$c2_v" = BOUNDS ]; then c2_sc=check; else c2_sc=all; fi
		out differs "$c2_sc" "$work/man/$c2_prev.json" "$work/man/a-$c2_x.$c2_v.json" 2>"$work/differs.err" \
			|| die "$c2_x, $c2_v changed: $(cat "$work/differs.err")"
		same all "a-$c2_x.$c2_v" "b-$c2_x.set-$c2_v" "$c2_x: make check with $c2_v changed, outputs dated ahead (R2)"
		c2_prev=a-$c2_x.$c2_v
	done
	same all "a-$c2_x.default" "b-$c2_x.defaults" "$c2_x: make check back at the defaults, outputs dated ahead (R2)"
	out differs mps "$work/man/a-$c2_x.default.json" "$work/man/a-$c2_x.O0.json" 2>"$work/differs.err" \
		|| die "$c2_x, CXXFLAGS=-O0: $(cat "$work/differs.err")"
	same mps "a-$c2_x.O0" "b-$c2_x.O0" "$c2_x: make CXXFLAGS=-O0 after make check (R2)"
	same mps "a-$c2_x.default" "b-$c2_x.plain" "$c2_x: plain make after CXXFLAGS=-O0 (R2)"
}

check_r5() { # CXX
	c5_bad=$(grep -v '^!! lib/' "$logs/b-$1.status" | grep -v '^!! bin/')
	test -z "$c5_bad" || fail "$1: after every setting, git sees files outside lib/ and bin/: $(echo "$c5_bad" | head -n 3 | tr '\n' ' ') (R5)"
	grep -q '^!! lib/' "$logs/b-$1.status" || fail "$1: git sees nothing under lib/ after a build: the check proves nothing"
}

# clean_check CXX: make clean in the built tree copy leaves no file under lib/ or bin/ (R5)
clean_check() {
	cc_d="$work/tree-$1"
	test -f "$cc_d/bin/check" || die "$1: the tree copy is not built before make clean"
	( cd "$cc_d" && limit 120 "$MAKE" clean ) >"$logs/clean-$1.log" 2>&1 || fail "$1: make clean failed (R5)"
	cc_left=$(cd "$cc_d" && find lib bin ! -type d 2>/dev/null)
	test -z "$cc_left" || fail "$1: make clean left $(echo "$cc_left" | head -n 3 | tr '\n' ' ')(R5)"
}

# timing CXX: make check with nothing to rebuild, b3a6c5a's copy against the tree's, interleaved, the
# least of each (R4); and make's own share, a no-op make of both binaries, least of 20 each
timing() {
	ti_x=$1
	for ti_d in "$work/base-$ti_x" "$work/tree-$ti_x"; do
		( cd "$ti_d" && limit 1500 "$MAKE" CXX="$ti_x" check ) >"$logs/time-warm-$ti_x.log" 2>&1 \
			|| die "make check in ${ti_d##*/} before timing failed"
	done
	cmp -s "$work/base-$ti_x/bin/check" "$work/tree-$ti_x/bin/check" && cmp -s "$work/base-$ti_x/bin/mps" "$work/tree-$ti_x/bin/mps" \
		|| fail "$ti_x: the tree's binaries are not $BASE's, so the timing compares different checks (R4)"
	python3 -I - "$MAKE" "$ti_x" "$work/base-$ti_x" "$work/tree-$ti_x" "$BUDGET" >"$logs/time-$ti_x.log" 2>&1 <<'EOF' \
		|| { cat "$logs/time-$ti_x.log" >&2; fail "$ti_x: make check with nothing to rebuild takes more than $BUDGET s longer than at b3a6c5a (R4)"; }
import os, subprocess, sys, time
make, cxx, base, tree, budget = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], float(sys.argv[5])
def run(d, goals):
    t = time.perf_counter()
    subprocess.run([make, "CXX=" + cxx] + goals, cwd=d, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    return time.perf_counter() - t
b, t = [], []
# Interleaved, so a change in the machine's load reaches both; the least of each, so a busy moment
# cannot make a correct tree fail; and once more before failing
for attempt in range(2):
    for _ in range(5):
        b.append(run(base, ["check"]))
        t.append(run(tree, ["check"]))
    mb, mt = min(b), min(t)
    if mt - mb <= budget:
        break
ob, ot = [], []
for _ in range(20):
    ob.append(run(base, ["bin/mps", "bin/check"]))
    ot.append(run(tree, ["bin/mps", "bin/check"]))
print("%s: %.3f s at b3a6c5a, %.3f s now, %+.3f s (budget %+.1f s; least of %d runs each; make's own share %.3f s -> %.3f s; load %.1f)"
      % (cxx, mb, mt, mt - mb, budget, len(t), min(ob), min(ot), os.getloadavg()[0]))
sys.exit(0 if mt - mb <= budget else 1)
EOF
	TIMES="${TIMES:-}$(cat "$logs/time-$ti_x.log"); "
}

# files: what changed against b3a6c5a, and each edited file against PLAN § 2 (R6, R8, R9, Non-goals)
files() {
	cd "$root" || die "cd $root"
	fi_changed=$( { git diff --name-only "$BASE"; git ls-files --others --exclude-standard; } | sort -u)
	for fi_p in $fi_changed; do
		case $fi_p in
			Makefile|AGENTS.md|.gitignore|"$spec"/*|"issues/$slug.md") ;;
			issues/*.md|ROADMAP.md) [ "$PHASE" -ge 2 ] || fail "$fi_p changed before the last phase (PLAN § 2)" ;;
			*) fail "$fi_p changed: PLAN § 2 leaves it as $BASE's (GOAL Non-goals)" ;;
		esac
	done
	for fi_p in Makefile AGENTS.md .gitignore; do
		echo "$fi_changed" | grep -qx "$fi_p" || fail "$fi_p is $BASE's: PLAN § 2 changes it"
	done
	git show "$BASE:AGENTS.md" >"$work/base-AGENTS.md" && git show "$BASE:.gitignore" >"$work/base-gitignore" || die "git show $BASE"
	python3 -I -B "$here/files.py" agents "$work/base-AGENTS.md" AGENTS.md || fail "AGENTS.md (R8)"
	python3 -I -B "$here/files.py" gitignore "$work/base-gitignore" .gitignore || fail ".gitignore (R9)"
	python3 -I -B "$here/files.py" makefile Makefile || fail "Makefile (R6)"
}

# pycache: each generator imported as a module from its own directory, by both Pythons, leaves git
# nothing to report (R9). Not -I: the import must find the module in the current directory.
pycache() {
	py_d="$work/pycache"
	git_copy "$py_d" || die "cannot copy the tree"
	for py_p in python3 python3.14; do
		for py_g in tests/vectors tests/known; do
			( cd "$py_d/$py_g" && limit 300 "$py_p" -c 'import generate' ) >"$logs/pycache.log" 2>&1 \
				|| { cat "$logs/pycache.log" >&2; fail "$py_p cannot import $py_g/generate.py as a module"; }
		done
	done
	test -d "$py_d/tests/vectors/__pycache__" && test -d "$py_d/tests/known/__pycache__" \
		|| fail "no __pycache__ appeared beside the generators: the import check proves nothing"
	py_s=$(cd "$py_d" && git status --porcelain --untracked-files=all)
	test -z "$py_s" || fail "git status after importing the generators: $(echo "$py_s" | head -n 2 | tr '\n' ' ') (R9)"
}

# gate_all: everything a phase gate checks; GATE_LINE summarizes it
gate_all() {
	CHECKS=? CLIS=?
	files
	lanes
	TESTED=${SELFTEST:-$COMPILERS}
	for ga_x in $TESTED; do
		check_r7 "$ga_x"
		check_r3 "$ga_x"
		check_r1 "$ga_x"
		check_r2 "$ga_x"
		check_r5 "$ga_x"
	done
	TIMES=
	for ga_x in $TESTED; do
		timing "$ga_x"
		clean_check "$ga_x"
	done
	pycache
	GATE_LINE="files as PLAN § 2 pins them; on $TESTED with GNU make 3.81: the CI probe and a fresh clone as at $BASE (make check $CHECKS + $CLIS), no-op runs quiet, touches as at $BASE, every switch of compiler or setting byte for byte a fresh make -B, git sees only lib/ and bin/, make clean empty; ${TIMES}imports leave git clean"
}

# gate_end NAME: the green line, or the self-test's tally
gate_end() {
	if [ -n "$SELFTEST" ]; then
		echo "selftest summary: ${GATE_LINE:-}"
		if [ "$FAILS" -gt 0 ]; then echo "selftest: $FAILS checks failed" >&2; exit 1; fi
		echo "selftest: no check failed (not a verdict: $SELFTEST only, frozen-gate check skipped)"
		exit 3
	fi
	echo "$1: green ($GATE_LINE)"
}
