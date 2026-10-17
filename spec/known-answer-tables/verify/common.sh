# Shared helpers for this cycle's phase gates (a cycle record, not a standing gate).
# Sourced by p1.sh … p4.sh and prepush.sh, after `PHASE=<n>`. POSIX sh. Each gate starts the tables'
# commands in the background (each under two Pythons, and each with one embedded value changed,
# which must fail), copies the tree as a clone would hold it, runs `make -B check` with clang++ and
# g++-16, holds the files PLAN § 2 leaves alone to c41950c (files.py), drives the tree's bin/mps
# against c41950c's, reads the tables itself and holds them to PLAN § 2's rows (tables.py), probes
# the loader against its own model (probe.cpp, probe.py), proves `make check` goes red under each
# injected loader bug and each scratch-copy edit of a table (mutant.cpp, tables.py tamper), checks
# the documents (docs.py), waits for the commands, and times `make check` against c41950c's
# (PLAN § 6). A gate prints `GATE FAIL: <reason>` and exits 1 on the first failure, or ends with a
# `<gate>: green` line. Gates are cumulative, and none requires a phase's exact file set.

# The tree under test is the one holding this script, whatever the caller's directory.
root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "gate: not in a git tree" >&2; exit 1; }
slug=known-answer-tables
spec=spec/$slug
here="$root/$spec/verify"
BASE=c41950c
BUDGET=0.5
work=$(mktemp -d "${TMPDIR:-/tmp}/mps-gate.XXXXXX") || { echo "gate: mktemp failed" >&2; exit 1; }
logs="$work/logs"
mkdir -p "$logs" || { echo "gate: mkdir $logs" >&2; exit 1; }
# Programmatic deletes (here and in tables.py) touch only this gate's own temporary directory
export GATE_WORK="$work"

# cleanup: the gate's own scratch directory goes when it ends, by Python's shutil (no rm involved),
# after checking that it is this gate's mktemp directory and nothing else
cleanup() {
	kill $(jobs -p) 2>/dev/null
	chmod -R u+rwx "$work" 2>/dev/null
	python3 -I - "$work" "${TMPDIR:-/tmp}" <<'PY'
import os, shutil, sys
w, t = os.path.realpath(sys.argv[1]), os.path.realpath(sys.argv[2])
if os.path.dirname(w) != t or not os.path.basename(w).startswith("mps-gate."):
    sys.exit("gate: refusing to delete %s, which is not this gate's own temporary directory" % w)
shutil.rmtree(w, ignore_errors=True)
PY
}
trap cleanup EXIT INT TERM

MUTANTS=0
TAMPERS=0
SAME=0

fail() {
	echo "GATE FAIL: $*" >&2
	exit 1
}

need() {
	command -v "$1" >/dev/null 2>&1 || fail "required tool not found: $1"
}

# GNU make 3.81 is what macOS ships, and AGENTS.md holds the build to it.
MAKE=/usr/bin/make
"$MAKE" --version 2>/dev/null | head -1 | grep -q '^GNU Make 3\.81' || fail "need GNU Make 3.81 at $MAKE"
need clang++; need g++-16; need rsync; need git; need awk; need python3; need python3.14; need perl; need tar; need cmp; need otool
COMPILERS='clang++ g++-16'

# The gates are frozen at the plan commit: this directory must match it, with nothing added.
plan_commit=$(cd "$root" && git log -1 --format=%H --grep="^\[feature\] Plan $slug" -- "$spec/verify")
test -n "$plan_commit" || fail "no plan commit for $spec/verify"
( cd "$root" && git diff --quiet "$plan_commit" -- "$spec/verify" ) \
	|| fail "$spec/verify differs from the plan commit $plan_commit: the gates are frozen"
test -z "$(cd "$root" && git ls-files --others --exclude-standard -- "$spec/verify")" \
	|| fail "untracked files in $spec/verify: the gates are frozen"
( cd "$root" && git cat-file -e "$BASE^{commit}" ) 2>/dev/null || fail "no commit $BASE in this repository"
# A plan commit made after code landed is no plan: between c41950c and the plan commit, no code,
# test, note or AGENTS.md changes (a revised plan before the build keeps the prefix and passes)
test -z "$(cd "$root" && git diff --name-only "$BASE" "$plan_commit" -- src tests doc ci Makefile AGENTS.md README.md)" \
	|| fail "the plan commit $plan_commit follows changes outside spec/: the gates are frozen at the plan made before the build"
( cd "$root" && git rev-parse -q --verify origin/main >/dev/null ) || fail "no origin/main in this repository"

# limit SECONDS CMD...: CMD, killed after SECONDS (a bug that never ends must not hang the gate)
limit() {
	perl -e '$t = shift; $SIG{ALRM} = sub { kill 9, $pid; exit 124 }; $pid = fork; if (!$pid) { exec @ARGV; exit 127 } alarm $t; waitpid $pid, 0; exit($? & 127 ? 128 + ($? & 127) : $? >> 8)' "$@"
}

# copy_tree DEST: the files git knows about (tracked, plus untracked and unignored) that exist, as a
# clone would hold them once committed. Ignored build output never comes along.
copy_tree() {
	mkdir -p "$1" || fail "mkdir $1"
	( cd "$root" && git ls-files -z --cached --others --exclude-standard | perl -0 -ne 'chomp; print "$_\0" if -e $_' ) \
		| rsync -a --from0 --files-from=- "$root/" "$1/" || fail "copy of the tree into $1"
	test ! -e "$1/lib" || fail "copy of the tree already has lib/"
	test ! -e "$1/bin" || fail "copy of the tree already has bin/"
}

STEMS_1=mersenne-exponents
STEMS_2=final-residues
STEMS_3=partial-residues

stems() {
	echo "$STEMS_1"
	[ "$PHASE" -ge 2 ] && echo "$STEMS_2"
	[ "$PHASE" -ge 3 ] && echo "$STEMS_3"
	return 0
}


# --- The commands: reproduction, recomputation and refusal, in the background ---------------------

VECTOR_STEMS='arith folding hex products modular decimal reference'

# run_bg NAME DIR CMD: CMD in DIR, in the background, its output in $logs/NAME.log, its status in
# $work/status/NAME
run_bg() {
	rb_name=$1 rb_dir=$2
	shift 2
	mkdir -p "$work/status"
	( cd "$rb_dir" && limit 1500 "$@" >"$logs/$rb_name.log" 2>&1; echo $? >"$work/status/$rb_name" ) &
}

# commands_start: each table's command traced under python3 from inside (command.py: every row it
# must recompute calls its pinned function, and it writes the table), and run as its header says
# under python3.14; the vector families under python3; and tables.py's second computation of every
# row the command recomputes, in four processes
commands_start() {
	copy_tree "$work/repro-python3"
	copy_tree "$work/repro-python3.14"
	mkdir -p "$work/repro-python3.14/shim" && ln -s "$(command -v python3.14)" "$work/repro-python3.14/shim/python3" || fail "shim for python3.14"
	for stem in $(stems); do
		f="tests/known/$stem.txt"
		test -f "$root/$f" || fail "$f is missing (PLAN § 2)"
		cmd=$(sed -n '2s/^# command: //p' "$root/$f")
		test "$cmd" = "python3 tests/known/generate.py $stem > tests/known/$stem.txt" \
			|| fail "$f: the second line is not '# command: python3 tests/known/generate.py $stem > tests/known/$stem.txt' (R2)"
		run_bg "trace-$stem" "$work/repro-python3" python3 -I -B "$here/command.py" trace . "$stem" "$work/record-$stem" "$work/trace-$stem.txt"
		run_bg "repro-$stem" "$work/repro-python3.14" env PATH="$work/repro-python3.14/shim:$PATH" sh -c "python3 --version >&2; $cmd"
	done
	copy_tree "$work/repro-vectors"
	for stem in $VECTOR_STEMS; do
		f="tests/vectors/$stem.txt"
		cmd=$(sed -n '2s/^# command: //p' "$root/$f")
		test "$cmd" = "python3 tests/vectors/generate.py $stem > tests/vectors/$stem.txt" || fail "$f: its command line changed"
		run_bg "vectors-$stem" "$work/repro-vectors" sh -c "$cmd"
	done
	if [ "$PHASE" -ge 2 ]; then
		for part in 0 1 2 3; do
			run_bg "recompute-$part" "$work" python3 -I -B "$here/tables.py" recompute "$PHASE" "$part"
		done
	fi
}

# commands_wait: each table written byte for byte by the traced run (python3) and by its header's
# command (python3.14) (R2, R5); each vector family unchanged by its command; every pinned row the
# command recomputes agrees with tables.py's second computation; and the command refuses, naming the
# row, each answer sabotaged and each embedded Res64 changed (command.py refuse; R9, R5)
commands_wait() {
	wait
	for stem in $(stems); do
		test "$(cat "$work/status/trace-$stem" 2>/dev/null)" = 0 \
			|| { tail -5 "$logs/trace-$stem.log" >&2; fail "tests/known/$stem.txt: the traced run of its command fails (command.py)"; }
		cmp -s "$work/trace-$stem.txt" "$root/tests/known/$stem.txt" \
			|| fail "tests/known/$stem.txt: its command under python3 does not reproduce it byte for byte (R2, R5)"
		test "$(cat "$work/status/repro-$stem" 2>/dev/null)" = 0 \
			|| { tail -5 "$logs/repro-$stem.log" >&2; fail "tests/known/$stem.txt: its command fails under python3.14"; }
		cmp -s "$work/repro-python3.14/tests/known/$stem.txt" "$root/tests/known/$stem.txt" \
			|| fail "tests/known/$stem.txt: its command under python3.14 does not reproduce it byte for byte (R2, R5)"
		grep -q '^Python 3\.14' "$logs/repro-$stem.log" || fail "the reproduction of $stem under python3.14 ran another Python"
	done
	for stem in $VECTOR_STEMS; do
		test "$(cat "$work/status/vectors-$stem" 2>/dev/null)" = 0 || { tail -5 "$logs/vectors-$stem.log" >&2; fail "tests/vectors/$stem.txt: its command fails"; }
		cmp -s "$work/repro-vectors/tests/vectors/$stem.txt" "$root/tests/vectors/$stem.txt" \
			|| fail "tests/vectors/$stem.txt: its command no longer reproduces it (the family's removal touched another)"
	done
	RECOMPUTED=0
	if [ "$PHASE" -ge 2 ]; then
		for part in 0 1 2 3; do
			test "$(cat "$work/status/recompute-$part" 2>/dev/null)" = 0 \
				|| { tail -5 "$logs/recompute-$part.log" >&2; fail "a pinned row disagrees with the gate's second computation (tables.py recompute)"; }
			RECOMPUTED=$((RECOMPUTED + $(sed -n 's/^recompute [0-9]: \([0-9]*\) rows.*/\1/p' "$logs/recompute-$part.log")))
		done
	fi
	REFUSALS=
	for stem in $(stems); do
		( cd "$work/repro-python3" && limit 600 python3 -I -B "$here/command.py" refuse . "$stem" "$work/record-$stem" ) >"$logs/refuse-$stem.log" 2>&1 \
			|| { tail -5 "$logs/refuse-$stem.log" >&2; fail "tests/known/generate.py $stem does not refuse a wrong answer (command.py refuse; R9, R5)"; }
		REFUSALS="$REFUSALS $(tail -1 "$logs/refuse-$stem.log");"
	done
}


# --- make check, both compilers ------------------------------------------------------------------

summary_ok() {
	grep -Eq '^check: [1-9][0-9]* passed, 0 failed$' "$1" || return 1
	grep -Eq '^cli: [1-9][0-9]* passed, 0 failed$' "$1" || return 1
}

C_READS='the loader reads a table it is given'
C_MAL='the loader refuses a malformed table'
C_NOSRC='the loader refuses a row without a source'
C_TWO='the loader refuses two answers for one key'
C_SUPER='the loader honours supersession'
C_FP='the loader refuses a fingerprint with fewer than two engines'
C_LOADS='every known-answer table loads'
C_BIB='every published source has a bibliography entry'
C_BORDER='exponents above 10000 cite a published list'
C_R9='final residues agree with the reference residues'
C_COMPOSITE='final residues are of composite Mersenne numbers'

# The check labels PLAN § 2 pins, by the phase that adds them, and the reference checks kept
LABELS_1="$C_READS
$C_MAL
$C_NOSRC
$C_TWO
$C_SUPER
$C_FP
$C_LOADS
$C_BIB
$C_BORDER
Lucas-Lehmer ends in zero for every listed prime
Lucas-Lehmer residues match the vectors
PRP ends in one for every listed prime
PRP residues match the vectors
the reference tests refuse exponents outside their theorems"
LABELS_2="$C_R9
$C_COMPOSITE"

labels() {
	echo "$LABELS_1"
	[ "$PHASE" -ge 2 ] && echo "$LABELS_2"
	return 0
}

# build_all: a copy per compiler under $work/tree-<cxx>, `make -B check` green, no warnings, every
# pinned check run and passed, the removed one gone; CHECKS and CLIS are the passed counts, BIN the
# clang++ bin/mps
build_all() {
	for cxx in $COMPILERS; do
		d="$work/tree-$cxx"
		copy_tree "$d"
		( cd "$d" && limit 900 "$MAKE" -B CXX="$cxx" check ) >"$logs/check-$cxx.log" 2>&1 \
			|| { tail -40 "$logs/check-$cxx.log" >&2; fail "$cxx: make check is red"; }
		summary_ok "$logs/check-$cxx.log" || { tail -40 "$logs/check-$cxx.log" >&2; fail "$cxx: make check summaries missing or wrong"; }
		if grep -q 'warning:' "$logs/check-$cxx.log"; then
			grep 'warning:' "$logs/check-$cxx.log" >&2
			fail "$cxx: warnings in the build"
		fi
		labels | while IFS= read -r lb; do
			grep -qxF "check $lb" "$logs/check-$cxx.log" || { echo "$lb" >"$work/missing-label"; break; }
		done
		test ! -s "$work/missing-label" || fail "$cxx: no check labelled '$(cat "$work/missing-label")' ran (PLAN § 2, the checks)"
		! grep -qxF 'check the prime list carries its source' "$logs/check-$cxx.log" \
			|| fail "$cxx: 'the prime list carries its source' still runs; its job moved to check_known.cpp (PLAN § 2)"
	done
	CHECKS=$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-clang++.log")
	CLIS=$(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-clang++.log")
	test "$CHECKS $CLIS" = "$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-g++-16.log") $(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-g++-16.log")" \
		|| fail "the two compilers ran different numbers of checks"
	BIN="$work/tree-clang++/bin/mps"
}

# build_base: c41950c's tree and bin/mps, built with clang++ at the Makefile's own flags
build_base() {
	b="$work/base"
	mkdir -p "$b" || fail "mkdir $b"
	( cd "$root" && git archive "$BASE" ) | ( cd "$b" && tar -xf - ) || fail "cannot export $BASE"
	( cd "$b" && "$MAKE" CXX=clang++ check ) >"$logs/base-build.log" 2>&1 \
		|| { tail -20 "$logs/base-build.log" >&2; fail "$BASE's make check does not pass"; }
	BASE_BIN="$b/bin/mps"
}


# --- the files -----------------------------------------------------------------------------------

files() {
	python3 -I -B "$here/files.py" "$PHASE" "$root" "$BASE" >"$logs/files.log" 2>&1 \
		|| { cat "$logs/files.log" >&2; fail "the footprint (files.py)"; }
}

# wording: no spec ids in src/ or tests/; no process started from the checks or the program (R11)
wording() {
	if grep -rnE '(^|[^A-Za-z0-9_])(R[0-9]{1,2}|P[1-9])([^A-Za-z0-9_-]|$)' "$root/src" "$root"/tests/*.cpp "$root"/tests/*.hpp \
		"$root/tests/cli.sh" "$root/tests/vectors/generate.py" "$root/tests/known/generate.py"; then
		fail "spec R#/P# ids in source (AGENTS.md § Conventions)"
	fi
	# Any spelling of a process start a grep can see, a call or a pointer: system, popen, the exec
	# family, posix_spawn, fork and vfork (R11)
	if grep -nE '(^|[^A-Za-z0-9_])(system|popen|_popen|execv[pP]?e?|execl[pe]?|execvP|posix_spawnp?|fork|vfork|wordexp)([^A-Za-z0-9_]|$)' \
		"$root"/src/*.cpp "$root"/src/*.hpp "$root"/tests/*.cpp "$root"/tests/*.hpp | grep -vE '^[^:]*:[0-9]+:[[:space:]]*//'; then
		fail "src/ or tests/ names a way to start a process: make check runs no tool beyond what it uses today (R11)"
	fi
	# Nothing in src/ or tests/ includes from outside them (spec/ is no source; a relative climb is not either)
	if grep -nE '^[[:space:]]*#[[:space:]]*include[[:space:]]*["<][^">]*(\.\.|spec/)' "$root"/src/*.cpp "$root"/src/*.hpp "$root"/tests/*.cpp "$root"/tests/*.hpp; then
		fail "src/ or tests/ includes a file from outside src/ and tests/"
	fi
	# The tables are read by path only in the loader and the pinned path constants (R1)
	if grep -nF 'known/' "$root"/src/*.cpp "$root"/src/*.hpp "$root"/tests/*.cpp "$root"/tests/*.hpp \
		| grep -vE '^[^:]*/src/known\.(cpp|hpp):' \
		| grep -vE '^[^:]*/tests/check_(known|reference)\.cpp:[0-9]+:static const std::string [A-Z_]+ = "tests/known/[a-z-]+\.txt";$' \
		| grep -vE '^[^:]*/tests/check_known\.cpp:[0-9]+:.*tests/known/(generate\.py )?" \+ stem \+' \
		| grep -vE '^[^:]*:[0-9]+:[[:space:]]*//'; then
		fail "a table's path outside src/known.* and the pinned path constants of check_known.cpp and check_reference.cpp (R1)"
	fi
	if grep -nE '\b(urllib|http\.client|socket|requests|subprocess)\b' "$root/tests/known/generate.py"; then
		fail "tests/known/generate.py reaches the network or another process: it embeds its published rows (GOAL, Clarifications)"
	fi
}


# --- R11: bin/mps unchanged, no new library, no Python in make check -----------------------------

# drive BIN NAME ARGS...: BIN in an empty directory with a clean environment (NO_COLOR or FORCE_COLOR
# from $DRIVE_ENV); stdout, stderr and status into $logs/NAME.{out,err,status}
drive() {
	dv_bin=$1 dv_name=$2
	shift 2
	dv_dir="$work/cwd/$dv_name"
	mkdir -p "$dv_dir" || fail "mkdir $dv_dir"
	( cd "$dv_dir" && exec env -i PATH=/usr/bin:/bin HOME="$dv_dir" LC_ALL=C "$DRIVE_ENV" "$dv_bin" "$@" ) \
		>"$logs/$dv_name.out" 2>"$logs/$dv_name.err"
	echo $? >"$logs/$dv_name.status"
	test -z "$(ls -A "$dv_dir")" || fail "mps $*: left files in its working directory"
}

# Runs that reach every path of bin/mps today: the verdict lines, -j, the exponent check, the
# statuses, the parser's errors, help and version
R11_RUNS='31
2
3
33
999999999
999999937
-t tf 31
-t p1 31
-t ll 31
-t all 31
-s 33
-j 31
-j 33
-j -t ll 31
-p 30
-p 0
-d 31
-D 31
-x 31
-t nope 31
abc
0
1000000000
31 61
-v
-h
'

# same_as_base: for every run under NO_COLOR and FORCE_COLOR, the tree's bin/mps and c41950c's print
# the same bytes and exit alike, a detailed stamp's time aside (R11)
same_as_base() {
	sb_n=0
	while IFS= read -r sb_line; do
		[ -n "$sb_line" ] || continue
		set -f
		# shellcheck disable=SC2086
		set -- $sb_line
		set +f
		for DRIVE_ENV in NO_COLOR=1 FORCE_COLOR=1; do
			sb_n=$((sb_n + 1))
			drive "$BIN" "same-t$sb_n" "$@"
			drive "$BASE_BIN" "same-b$sb_n" "$@"
			for sb_x in out err status; do
				sed -E 's/[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}/<time>/g' "$logs/same-t$sb_n.$sb_x" >"$logs/same-t$sb_n.$sb_x.n"
				sed -E 's/[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}/<time>/g' "$logs/same-b$sb_n.$sb_x" >"$logs/same-b$sb_n.$sb_x.n"
				cmp -s "$logs/same-t$sb_n.$sb_x.n" "$logs/same-b$sb_n.$sb_x.n" \
					|| fail "mps $sb_line [$DRIVE_ENV]: $sb_x differs from $BASE's (R11)"
			done
		done
	done <<EOF
$R11_RUNS
EOF
	SAME=$sb_n
}

# libraries: bin/mps links exactly the libraries c41950c's does (R11, § Invariants 5)
libraries() {
	otool -L "$BIN" | tail -n +2 | awk '{ print $1 }' | sort >"$work/libs-tree"
	otool -L "$BASE_BIN" | tail -n +2 | awk '{ print $1 }' | sort >"$work/libs-base"
	cmp -s "$work/libs-tree" "$work/libs-base" || { diff "$work/libs-base" "$work/libs-tree" >&2; fail "bin/mps links other libraries than $BASE's (R11)"; }
}

# no_python: make check, warm, with python, python3 and every python3.N first on PATH as stubs that
# record being called and fail: none is called, and make check stays green (R11)
no_python() {
	np="$work/shim"
	mkdir -p "$np" || fail "mkdir $np"
	for py in python python3 python3.8 python3.9 python3.10 python3.11 python3.12 python3.13 python3.14 python3.15 uv; do
		printf '#!/bin/sh\necho "%s $*" >>"%s/called"\nexit 97\n' "$py" "$work" >"$np/$py"
		chmod +x "$np/$py"
	done
	( cd "$work/tree-clang++" && PATH="$np:$PATH" "$MAKE" CXX=clang++ check ) >"$logs/no-python.log" 2>&1 \
		|| { tail -20 "$logs/no-python.log" >&2; fail "make check is red with Python stubbed out (R11)"; }
	test ! -s "$work/called" || fail "make check ran $(head -1 "$work/called") (R11)"
	# A stub on PATH misses an absolute path, so the binaries must name no interpreter either
	for nb in bin/check bin/mps; do
		if strings -a "$work/tree-clang++/$nb" | grep -nE '(/[A-Za-z0-9._/-]*(python|perl|ruby|uv)[0-9.]*([^A-Za-z0-9]|$)|/usr/bin/env|/bin/(ba|z|da|k)?sh([^A-Za-z0-9]|$)|/usr/local/bin|/opt/homebrew/bin)'; then
			fail "$nb names an interpreter or a tool path (R11)"
		fi
	done
}


# --- the tables and the probe --------------------------------------------------------------------

tables() {
	limit 300 python3 -I -B "$here/tables.py" check "$PHASE" "$root" >"$logs/tables.log" 2>&1 \
		|| { tail -20 "$logs/tables.log" >&2; fail "the tables (tables.py)"; }
	TABLES=$(tail -1 "$logs/tables.log")
}

# probe: verify/probe.cpp built against the tree's src/known.cpp with the checks' bounds assertions;
# its answers on probe.py's tables equal tables.py's (R1, R3, R4, R10)
probe() {
	clang++ -O2 -std=c++20 -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE -I "$root/src" \
		"$here/probe.cpp" "$root/src/known.cpp" -o "$work/probe" >"$logs/probe-build.log" 2>&1 \
		|| { tail -20 "$logs/probe-build.log" >&2; fail "the probe does not build against src/known.cpp: known.hpp keeps the names and members PLAN § 2 gives"; }
	limit 300 python3 -I -B "$here/probe.py" "$PHASE" "$root" "$work/probe" >"$logs/probe.log" 2>&1 \
		|| { tail -20 "$logs/probe.log" >&2; fail "the loader disagrees with the gate's reading of the format (probe.py)"; }
	PROBE=$(tail -1 "$logs/probe.log")
}

# pointmut: every point mutant of the tree's src/known.cpp disagrees with tables.py on the probe's
# tables, or is listed, with its reason, in spec/known-answer-tables/equivalent-mutants.txt
pointmut() {
	mkdir -p "$work/pointmut"
	limit 900 python3 -I -B "$here/pointmut.py" "$PHASE" "$root" "$work/pointmut" >"$logs/pointmut.log" 2>&1 \
		|| { tail -20 "$logs/pointmut.log" >&2; fail "a point mutant of the loader passes the probe (pointmut.py)"; }
	POINTMUT=$(tail -1 "$logs/pointmut.log")
}


# --- bugs and scratch-copy edits -----------------------------------------------------------------

MUT="$work/mut"

# wire_defs FILE NAME...: rename each NAME's definition, and nothing else, to gate_real_NAME: the one
# unindented line that holds `NAME(` after a non-identifier character and does not end in `;`
# (PLAN § 2: each is defined out of line, at file scope, once, its name on the first line)
wire_defs() {
	wf=$1
	shift
	test -f "$MUT/$wf" || fail "$wf is missing"
	for wn in "$@"; do
		awk -v n="$wn" '
			BEGIN { re = "^([A-Za-z_][^;]*[^A-Za-z0-9_])?" n "\\(" }
			$0 ~ re && $0 !~ /;[ \t]*$/ { hits++; i = match($0, "(^|[^A-Za-z0-9_])" n "\\("); if (i > 0 && substr($0, i, length(n)) != n) i++; $0 = substr($0, 1, i - 1) "gate_real_" substr($0, i) }
			{ print }
			END { if (hits != 1) exit 3 }' "$MUT/$wf" >"$work/wired" \
			|| fail "no single definition of $wn at file scope in $wf (PLAN § 2: the gates wrap it)"
		mv "$work/wired" "$MUT/$wf" || fail "wire $wf"
	done
}

# mutants_setup: a copy whose known.cpp and reference.cpp have their public definitions renamed, with
# verify/mutant.cpp linked in their place, built once with clang++; the pass-through control run of
# bin/check is green, changes nothing, and calls the reference tests as R6 says
mutants_setup() {
	copy_tree "$MUT"
	wire_defs src/known.cpp parse
	{ printf '#include "known.hpp"\nnamespace known {\n\tTable gate_real_parse(const std::string& path, const std::string& text);\n}\n'; cat "$MUT/src/known.cpp"; } >"$work/wired" \
		&& mv "$work/wired" "$MUT/src/known.cpp" || fail "declare the wired loader"
	wire_defs src/reference.cpp lucas_lehmer prp3
	{ printf '#include "mp.hpp"\nnamespace reference {\n\tBigInt gate_real_lucas_lehmer(long unsigned p);\n\tBigInt gate_real_prp3(long unsigned p);\n}\n'; cat "$MUT/src/reference.cpp"; } >"$work/wired" \
		&& mv "$work/wired" "$MUT/src/reference.cpp" || fail "declare the wired reference tests"
	cp "$here/mutant.cpp" "$MUT/src/gate_mutant.cpp" || fail "copy mutant.cpp"
	MODS=$(sed -n 's/^MODULES *:= *//p' "$MUT/Makefile")
	test -n "$MODS" || fail "no MODULES line in the Makefile"
	( cd "$MUT" && "$MAKE" CXX=clang++ MODULES="$MODS gate_mutant" bin/mps bin/check ) >"$logs/mut-build.log" 2>&1 \
		|| { tail -30 "$logs/mut-build.log" >&2; fail "the tree does not build through the gate's wrappers: known.hpp and reference.hpp keep the names PLAN § 2 gives"; }
	: >"$work/applied-control"
	( cd "$MUT" && limit 300 env GATE_MUTANT_LOG="$work/applied-control" GATE_TRACE="$work/trace" bin/check ) >"$logs/mut-control.log" 2>&1 \
		|| { tail -30 "$logs/mut-control.log" >&2; fail "bin/check is red through the gate's pass-through wrappers"; }
	test ! -s "$work/applied-control" || fail "the control run applied a bug"
	python3 -I -B "$here/tables.py" coverage "$MUT" "$work/trace" >"$logs/coverage.log" 2>&1 \
		|| { tail -5 "$logs/coverage.log" >&2; fail "the reference tests' exponents in make check (R6)"; }
	COVERAGE=$(tail -1 "$logs/coverage.log")
	# The reference checks read their exponents through the loader: with 9941 gone from what
	# known::parse reads, neither reference test sees 9941 (R1, R6)
	: >"$work/applied-drop"
	( cd "$MUT" && limit 300 env GATE_MUTANT=drop-9941 GATE_MUTANT_LOG="$work/applied-drop" GATE_TRACE="$work/trace-drop" bin/check ) >"$logs/mut-drop.log" 2>&1
	test -s "$work/applied-drop" || fail "the loader never read an exponent table holding 9941 (drop-9941)"
	! grep -qx 'll 9941' "$work/trace-drop" && ! grep -qx 'prp 9941' "$work/trace-drop" \
		|| fail "with 9941 gone from what known::parse reads, make check still runs a reference test on 9941: it reads the exponents some other way (R1, R6)"
	grep -qx 'll 9689' "$work/trace-drop" || fail "with 9941 gone, the reference tests stop before 9689"
	COVERAGE="$COVERAGE; through the loader alone"
	cp -R "$MUT/tests/known" "$work/known-orig" || fail "save the tables"
}

# mutant ID LABEL WHAT: bin/check is red under GATE_MUTANT=ID, the wrapper changed a table's text,
# and the check labelled LABEL failed
mutant() {
	ml="$work/applied-$1"
	: >"$ml"
	if ( cd "$MUT" && limit 120 env GATE_MUTANT="$1" GATE_MUTANT_LOG="$ml" bin/check ) >"$logs/mut-$1.log" 2>&1; then
		test -s "$ml" || fail "no table reaches the bug $1 stands for ($3): no text changed"
		fail "make check stays green under $1 ($3)"
	fi
	test -s "$ml" || { tail -20 "$logs/mut-$1.log" >&2; fail "bin/check went red under $1 without it changing a text ($3)"; }
	grep -qxF " FAIL $2" "$logs/mut-$1.log" \
		|| { grep '^ FAIL' "$logs/mut-$1.log" >&2; fail "under $1, the check '$2' passes ($3)"; }
	MUTANTS=$((MUTANTS + 1))
}

# tamper NAME STEM LABEL: one table edited by tables.py; bin/check green when the edit is a control,
# otherwise red with LABEL failing and, when the edit names a line, the output naming the table and
# that line (R3)
tamper() {
	tf="$MUT/tests/known/$2.txt"
	named=$(python3 -I -B "$here/tables.py" tamper "$1" "$tf") || fail "tamper $1: tables.py cannot apply it to $2.txt"
	( cd "$MUT" && limit 120 bin/check ) >"$logs/tamper-$1.log" 2>&1
	tm_status=$?
	cp "$work/known-orig/$2.txt" "$tf" || fail "restore $2"
	if [ "$named" = green ]; then
		test "$tm_status" -eq 0 || { grep '^ FAIL' "$logs/tamper-$1.log" >&2; fail "make check is red after $1, a valid edit of tests/known/$2.txt (R4, R10)"; }
	else
		test "$tm_status" -ne 0 || fail "make check stays green after $1, an edit of tests/known/$2.txt (R3)"
		grep -qxF " FAIL $3" "$logs/tamper-$1.log" \
			|| { grep '^ FAIL' "$logs/tamper-$1.log" >&2; fail "after $1, the check '$3' passes"; }
		case $named in
			missing) grep -qF "tests/known/$2.txt" "$logs/tamper-$1.log" || fail "after $1, make check does not name tests/known/$2.txt" ;;
			-) ;;
			*) grep -qF "tests/known/$2.txt:$named: " "$logs/tamper-$1.log" \
				|| fail "after $1, make check does not name the table and the row, tests/known/$2.txt:$named (R3)" ;;
		esac
	fi
	TAMPERS=$((TAMPERS + 1))
}

# The loader's bugs, each the text a loader without one rule would accept (PLAN § 6)
p1_mutants() {
	mutant source-default "$C_NOSRC" "a row above every source line given a default source"
	mutant conflict-blind "$C_TWO" "a second answer for a key dropped, not refused"
	mutant fingerprint-blind "$C_TWO" "a fingerprint against a computed row dropped, not refused"
	mutant supersede-blind "$C_SUPER" "corrections read as plain rows"
	mutant one-engine "$C_FP" "a fingerprint of fewer than two engines accepted"
	mutant runs-anywhere "$C_FP" "runs on a row that is not a fingerprint ignored"
	mutant res64-case "$C_MAL" "a Res64 in lowercase accepted"
	mutant count-blind "$C_MAL" "the trailer's count not checked"
	mutant ascending-blind "$C_MAL" "exponents out of order or repeated skipped"
	mutant prime-blind "$C_MAL" "a composite exponent accepted"
	mutant agreed-blind "$C_MAL" "one agreeing result accepted"
	mutant origin-blind "$C_MAL" "a source of an unknown kind read as computed"
	mutant tail-blind "$C_MAL" "lines after the trailer ignored"
	mutant weak-correction "$C_SUPER" "a computed correction of a published final residue above the bound accepted"
	mutant runs-supersede "$C_FP" "a fingerprint's correction honoured"
	mutant final-in-partial "$C_MAL" "a final Lucas-Lehmer residue accepted in the partial table"
}

run_mutants() {
	mutants_setup
	p1_mutants
	python3 -I -B "$here/tables.py" tampers "$PHASE" >"$work/tampers" || fail "tables.py tampers"
	while read -r tname tstem tlabel; do
		tamper "$tname" "$tstem" "$tlabel"
	done <"$work/tampers"
}


# --- the documents -------------------------------------------------------------------------------

docs() {
	python3 -I -B "$here/docs.py" "$PHASE" "$root" "$BASE" >"$logs/docs.log" 2>&1 \
		|| { cat "$logs/docs.log" >&2; fail "AGENTS.md, the COMPOSITE comment or the bibliography (docs.py)"; }
	DOCS=$(tail -1 "$logs/docs.log")
}


# --- R11's time ----------------------------------------------------------------------------------

# timing: warm `make check` (nothing to rebuild), the least of five interleaved runs (ten when the
# first five miss), for the tree and for c41950c on this machine, with each compiler; the tree's is
# at most BUDGET seconds longer (R11)
timing() {
	TIMES=
	for cxx in $COMPILERS; do
		bt="$work/base-$cxx"
		if [ ! -d "$bt" ]; then
			mkdir -p "$bt" && ( cd "$root" && git archive "$BASE" ) | ( cd "$bt" && tar -xf - ) || fail "cannot export $BASE"
		fi
		( cd "$bt" && "$MAKE" -B CXX="$cxx" check ) >"$logs/time-base-build-$cxx.log" 2>&1 || fail "$BASE: make check with $cxx"
		( cd "$work/tree-$cxx" && "$MAKE" CXX="$cxx" check ) >"$logs/time-tree-build-$cxx.log" 2>&1 || fail "tree: make check with $cxx"
		python3 -I - "$MAKE" "$cxx" "$bt" "$work/tree-$cxx" "$BUDGET" >"$logs/time-$cxx.log" 2>&1 <<'EOF' || { cat "$logs/time-$cxx.log" >&2; fail "$cxx: make check takes more than $BUDGET s longer than at $BASE (R11)"; }
import os, subprocess, sys, time
make, cxx, base, tree, budget = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], float(sys.argv[5])
def run(d):
    t = time.perf_counter()
    subprocess.run([make, "CXX=" + cxx, "check"], cwd=d, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    return time.perf_counter() - t
b, t = [], []
# Interleaved, so a change in the machine's load reaches both; the least of each, so a busy moment
# cannot make a correct tree fail; and once more before failing
for attempt in range(2):
    for _ in range(5):
        b.append(run(base))
        t.append(run(tree))
    mb, mt = min(b), min(t)
    if mt - mb <= budget:
        break
load = os.getloadavg()[0]
print("%s: %.2f s at c41950c, %.2f s now, %+.2f s (budget %+.1f s; least of %d runs each; load %.1f)"
      % (cxx, mb, mt, mt - mb, budget, len(t), load))
sys.exit(0 if mt - mb <= budget else 1)
EOF
		TIMES="$TIMES$(cat "$logs/time-$cxx.log"); "
	done
}


# gate_all: everything a phase gate checks, cumulatively up to PHASE; GATE_LINE summarizes it
gate_all() {
	commands_start
	build_all
	files
	wording
	build_base
	same_as_base
	libraries
	no_python
	tables
	probe
	pointmut
	run_mutants
	docs
	commands_wait
	timing
	GATE_LINE="make check $CHECKS + $CLIS on clang++ and g++-16; $SAME runs as $BASE, same libraries, no Python; tables reproduced by two Pythons; $RECOMPUTED pinned rows recomputed by a second method;$REFUSALS $TABLES; $PROBE; $POINTMUT; $COVERAGE; $MUTANTS bugs and $TAMPERS table edits as planned; $DOCS; $TIMES"
}
