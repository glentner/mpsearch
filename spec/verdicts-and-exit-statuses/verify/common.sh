# Shared helpers for this cycle's phase gates (a cycle record, not a standing gate).
# Sourced by p1.sh ... p4.sh and prepush.sh. POSIX sh. Each gate copies the tree as a clone would
# hold it, runs `make check` with clang++ and g++-16, drives the built bin/mps in an empty directory
# with a clean environment, runs the gate's own probe (probe.cpp) against the tree's modules, and
# proves `make check` goes red under each injected defect (mutant.cpp) and each help edit. A gate
# prints `GATE FAIL: <reason> (R-ID)` and exits 1 on the first failure, or ends with a
# `<gate>: green` line. Machinery copied from spec/cli-and-sieve-fixes/verify/ and cut to this cycle.

# The tree under test is the one holding this script, whatever the caller's directory.
root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "gate: not in a git tree" >&2; exit 1; }
spec=spec/verdicts-and-exit-statuses
here="$root/$spec/verify"
work=$(mktemp -d "${TMPDIR:-/tmp}/mps-gate.XXXXXX") || { echo "gate: mktemp failed" >&2; exit 1; }
logs="$work/logs"
mkdir -p "$logs" || { echo "gate: mkdir $logs" >&2; exit 1; }
trap 'chmod -R u+rwx "$work" 2>/dev/null; /bin/rm -rf "$work"' EXIT INT TERM

nl='
'
MUTANTS=0

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
need clang++; need g++-16; need rsync; need git; need awk; need python3
COMPILERS='clang++ g++-16'

# The gates are frozen at the plan commit: this directory must match it, with nothing added.
plan_commit=$(cd "$root" && git log -1 --format=%H --grep='^\[feature\] Plan verdicts-and-exit-statuses' -- "$spec/verify")
test -n "$plan_commit" || fail "no plan commit for $spec/verify"
( cd "$root" && git diff --quiet "$plan_commit" -- "$spec/verify" ) \
	|| fail "$spec/verify differs from the plan commit $plan_commit: the gates are frozen"
test -z "$(cd "$root" && git ls-files --others --exclude-standard -- "$spec/verify")" \
	|| fail "untracked files in $spec/verify: the gates are frozen"

# The program version, from its one home
V=$(sed -n 's/^static const std::string APP_VERSION = "\([^"]*\)";$/\1/p' "$root/src/main.hpp")
test -n "$V" || fail "no APP_VERSION in src/main.hpp"

# copy_tree DEST: the files git knows about (tracked, plus untracked and unignored), as a clone would
# hold them once committed. Ignored build output never comes along.
copy_tree() {
	mkdir -p "$1" || fail "mkdir $1"
	( cd "$root" && git ls-files -z --cached --others --exclude-standard ) \
		| rsync -a --from0 --files-from=- "$root/" "$1/" || fail "copy of the tree into $1"
	test ! -e "$1/lib" || fail "copy of the tree already has lib/"
	test ! -e "$1/bin" || fail "copy of the tree already has bin/"
}


# --- make check, both compilers ------------------------------------------------------------------

summary_ok() {
	grep -Eq '^check: [1-9][0-9]* passed, 0 failed$' "$1" || return 1
	grep -Eq '^cli: [1-9][0-9]* passed, 0 failed$' "$1" || return 1
}

# build_all: a copy per compiler under $work/tree-<cxx>, `make -B check` green, no warnings. Sets
# CHECKS and CLIS to the clang++ run's passed counts.
build_all() {
	for cxx in $COMPILERS; do
		d="$work/tree-$cxx"
		copy_tree "$d"
		( cd "$d" && "$MAKE" -B CXX="$cxx" check ) >"$logs/check-$cxx.log" 2>&1 \
			|| { tail -40 "$logs/check-$cxx.log" >&2; fail "$cxx: make check is red (R13)"; }
		summary_ok "$logs/check-$cxx.log" || { tail -40 "$logs/check-$cxx.log" >&2; fail "$cxx: make check summaries missing or wrong (R13)"; }
		if grep -q 'warning:' "$logs/check-$cxx.log"; then
			grep 'warning:' "$logs/check-$cxx.log" >&2
			fail "$cxx: warnings in the build"
		fi
	done
	CHECKS=$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-clang++.log")
	CLIS=$(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-clang++.log")
	test "$CHECKS" = "$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-g++-16.log")" \
		|| fail "the two compilers ran different numbers of checks"
}


# --- driving bin/mps ------------------------------------------------------------------------------

# run NAME ARGS...: the current build's bin/mps ($BIN) in an empty directory with a clean
# environment; stdout and stderr into $logs/NAME.out and .err, the status into $status. With
# RUN_COLOR=1 the run has FORCE_COLOR=1 and no NO_COLOR.
run() {
	rn=$1
	shift
	rd="$work/cwd/$rn"
	mkdir -p "$rd" || fail "mkdir $rd"
	if [ "${RUN_COLOR:-0}" = 1 ]; then
		( cd "$rd" && env -i PATH=/usr/bin:/bin HOME="$rd" LC_ALL=C FORCE_COLOR=1 "$BIN" "$@" ) >"$logs/$rn.out" 2>"$logs/$rn.err"
	else
		( cd "$rd" && env -i PATH=/usr/bin:/bin HOME="$rd" LC_ALL=C NO_COLOR=1 "$BIN" "$@" ) >"$logs/$rn.out" 2>"$logs/$rn.err"
	fi
	status=$?
	test "$status" -lt 128 || fail "mps $*: killed by signal $((status - 128))"
	test -z "$(ls -A "$rd")" || fail "mps $*: left '$(ls -A "$rd")' in its working directory"
}

# run_merged NAME ARGS...: as run, with stderr and stdout on one stream ($logs/NAME.out)
run_merged() {
	rn=$1
	shift
	rd="$work/cwd/$rn"
	mkdir -p "$rd" || fail "mkdir $rd"
	( cd "$rd" && env -i PATH=/usr/bin:/bin HOME="$rd" LC_ALL=C NO_COLOR=1 "$BIN" "$@" ) >"$logs/$rn.out" 2>&1
	status=$?
}

show() {
	echo "--- stdout of $1" >&2; cat "$logs/$1.out" >&2
	echo "--- stderr of $1" >&2; cat "$logs/$1.err" >&2 2>/dev/null
}

# want_status NAME N WHY
want_status() {
	test "$status" -eq "$2" || { show "$1"; fail "$3: exit $status, want $2"; }
}

# want_out NAME TEXT WHY: stdout is exactly TEXT and a final newline (empty TEXT: empty stdout)
want_out() {
	if [ -z "$2" ]; then
		test ! -s "$logs/$1.out" || { show "$1"; fail "$3: stdout is not empty"; }
	else
		printf '%s\n' "$2" | cmp -s - "$logs/$1.out" || { show "$1"; printf 'want:\n%s\n' "$2" >&2; fail "$3: stdout differs"; }
	fi
}

# want_err NAME TEXT WHY: stderr is exactly TEXT (empty TEXT: empty stderr)
want_err() {
	if [ -z "$2" ]; then
		test ! -s "$logs/$1.err" || { show "$1"; fail "$3: stderr is not empty"; }
	else
		printf '%s\n' "$2" | cmp -s - "$logs/$1.err" || { show "$1"; fail "$3: stderr differs"; }
	fi
}

err_lacks() {
	if grep -qF -- "$2" "$logs/$1.err"; then show "$1"; fail "$3: stderr has '$2'"; fi
}

START_TF='    info [mps] Running quick factor test'
START_P1='    info [mps] Running P-1 test'
START_LL='    info [mps] Running Lucas-Lehmer test'
START_ALL="$START_TF$nl$START_P1$nl$START_LL"
TAIL=" (mps $V)"
NV_TF="tf: no verdict, trial factoring is not implemented yet$TAIL"
NV_P1="p1: no verdict, P-1 is not implemented yet$TAIL"
NV_LL="ll: no verdict, Lucas-Lehmer is not implemented yet$TAIL"

# lines3 P: the three no-verdict lines for M_P
lines3() {
	printf 'M%s %s\nM%s %s\nM%s %s' "$1" "$NV_TF" "$1" "$NV_P1" "$1" "$NV_LL"
}

# records FILE SPEC...: FILE holds exactly one JSON record per SPEC, in order, each parsing strictly
# (no duplicate keys, no NaN, ASCII) and byte-equal to json.dumps of the record the SPEC names:
# P:WORKTYPE:nv:REASON, P:WORKTYPE:stub (the stub's no verdict), P:WORKTYPE:f:FACTOR,
# P:WORKTYPE:r:RES64, P:WORKTYPE:prime, P:WORKTYPE:pp, P:WORKTYPE:nvx (the gate's hostile reason).
cat >"$work/records.py" <<'EOF'
import json, sys

def pairs(kv):
    keys = [k for k, _ in kv]
    if len(set(keys)) != len(keys):
        raise ValueError("duplicate key in %r" % keys)
    return dict(kv)

def constant(name):
    raise ValueError("not JSON: " + name)

version, path, specs = sys.argv[1], sys.argv[2], sys.argv[3:]
want = []
for spec in specs:
    p, worktype, kind, *rest = spec.split(":", 3)
    r = {"exponent": int(p), "worktype": worktype}
    if kind == "nv":
        r.update(status="no-verdict", reason=rest[0])
    elif kind == "f":
        r.update(status="composite", factors=[rest[0]])
    elif kind == "r":
        r.update(status="composite", res64=rest[0])
    elif kind == "prime":
        r.update(status="prime")
    elif kind == "pp":
        r.update(status="probable-prime")
    elif kind == "stub":
        r.update(status="no-verdict", reason={"tf": "trial factoring", "p1": "P-1", "ll": "Lucas-Lehmer"}[worktype] + " is not implemented yet")
    elif kind == "nvx":
        r.update(status="no-verdict", reason='a"b\\c\nd\x01e')
    else:
        sys.exit("bad spec " + spec)
    r["program"] = {"name": "mps", "version": version}
    r["record_version"] = 1
    want.append(r)
text = open(path, "rb").read()
if want and not text.endswith(b"\n"):
    sys.exit("output does not end in a newline")
try:
    lines = text.decode("ascii").split("\n")[:-1] if text else []
except UnicodeDecodeError:
    sys.exit("output is not ASCII")
if len(lines) != len(want):
    sys.exit("%d lines, want %d:\n%s" % (len(lines), len(want), text.decode("ascii", "replace")))
for i, (line, w) in enumerate(zip(lines, want)):
    try:
        json.loads(line, object_pairs_hook=pairs, parse_constant=constant)
    except ValueError as e:
        sys.exit("line %d does not parse as JSON (%s): %s" % (i + 1, e, line))
    if specs[i].endswith(":nvx"):
        # Escapes may be spelled either way; the parsed value, key order and types must match
        got = json.loads(line, object_pairs_hook=pairs)
        if list(got) != list(w) or json.dumps(got) != json.dumps(w):
            sys.exit("line %d: %s\n   want the value of: %s" % (i + 1, line, json.dumps(w)))
    elif line != json.dumps(w):
        sys.exit("line %d: %s\n   want: %s" % (i + 1, line, json.dumps(w)))
EOF

# want_records NAME WHY SPEC...: stdout of run NAME is exactly these records
want_records() {
	wr=$1 why=$2
	shift 2
	python3 -I "$work/records.py" "$V" "$logs/$wr.out" "$@" >"$logs/$wr.rec" 2>&1 \
		|| { show "$wr"; cat "$logs/$wr.rec" >&2; fail "$why"; }
}

R_TF='trial factoring is not implemented yet'
R_P1='P-1 is not implemented yet'
R_LL='Lucas-Lehmer is not implemented yet'

# smallest_factor P: the smallest prime factor of P, by trial division in awk
smallest_factor() {
	awk -v n="$1" 'BEGIN { if (n % 2 == 0) { print 2; exit } for (d = 3; d * d <= n; d += 2) if (n % d == 0) { print d; exit } print n }'
}


# --- the gate's probe ------------------------------------------------------------------------------

# probe CXX: build verify/probe.cpp against that copy's src/ modules, all but main.cpp, with the
# gate's phase; prints the probe's path
probe() {
	pc=$1
	pd="$work/tree-$pc"
	pb="$work/probe-$pc"
	if [ ! -x "$pb" ]; then
		( cd "$pd" && "$pc" -std=c++20 -O1 -fno-fast-math -ffp-contract=off -DGATE_PHASE="$PHASE" -I src "$here/probe.cpp" \
			$(ls src/*.cpp | grep -v '^src/main\.cpp$') -o "$pb" ) >"$logs/probe-$pc.log" 2>&1 \
			|| { tail -30 "$logs/probe-$pc.log" >&2; fail "the probe does not build against the tree with $pc: result.hpp, math.hpp and cli.hpp keep the names and signatures PLAN § 2 gives"; }
	fi
	echo "$pb"
}

# probe_want CXX MODE WHY: the probe's output in MODE equals the gate's expectation in $work/want-MODE
probe_want() {
	pw=$(probe "$1")
	( cd "$work" && env -i PATH=/usr/bin:/bin LC_ALL=C NO_COLOR=1 "$pw" "$2" ) >"$logs/probe-$2-$1.out" 2>"$logs/probe-$2-$1.err" \
		|| { cat "$logs/probe-$2-$1.err" >&2; fail "the probe failed in mode $2 ($1): $3"; }
	cmp -s "$work/want-$2" "$logs/probe-$2-$1.out" \
		|| { diff "$work/want-$2" "$logs/probe-$2-$1.out" >&2; fail "$3 ($1)"; }
}


# --- R12 and R11: what changes in one commit ---------------------------------------------------

# same_commit WHY FILE NEEDLE [FILE NEEDLE]...: the first commit that put the first NEEDLE in the
# first FILE put every NEEDLE in its FILE (each absent from that commit's parent); with no such
# commit yet, every NEEDLE is in its working-tree FILE and absent from HEAD's, so the next commit
# carries them together. A NEEDLE starting with = must be a whole line.
same_commit() {
	sc_why=$1
	shift
	sc_c=$(cd "$root" && git log --format=%H -S"${2#=}" -- "$1" | tail -1)
	if [ -n "$sc_c" ]; then
		sc_new="$sc_c" sc_old="$sc_c^"
	else
		sc_new=WORKTREE sc_old=HEAD
	fi
	while [ $# -ge 2 ]; do
		case $2 in
			=*) sc_g=-qxF sc_n=${2#=} ;;
			*)  sc_g=-qF sc_n=$2 ;;
		esac
		if [ "$sc_new" = WORKTREE ]; then
			grep "$sc_g" -- "$sc_n" "$root/$1" || fail "$1 lacks '$sc_n' ($sc_why)"
		else
			( cd "$root" && git show "$sc_new:$1" 2>/dev/null ) | grep "$sc_g" -- "$sc_n" \
				|| fail "commit $sc_new does not put '$sc_n' in $1 with the rest ($sc_why)"
		fi
		if ( cd "$root" && git show "$sc_old:$1" 2>/dev/null ) | grep "$sc_g" -- "$sc_n"; then
			fail "'$2' was already in $1 before the change that must carry it ($sc_why)"
		fi
		shift 2
	done
}

agents_r12() {
	same_commit "R12: the result format, its paths and the reworded sentences in one commit" \
		src/result.cpp 'report_run' \
		AGENTS.md '=- `src/result.hpp`, `src/result.cpp`' \
		AGENTS.md '=- `src/main.cpp`' \
		AGENTS.md '=A cycle that adds an arithmetic engine, a test driver that prints a verdict, a checkpoint or result format of `mps`, or a table of known answers adds its paths here in the same commit.' \
		AGENTS.md '=- A change to a checkpoint or result format of `mps` bumps that format'"'"'s version in the same commit.'
	if grep -qF 'A change to the checkpoint or result format bumps' "$root/AGENTS.md"; then
		fail "AGENTS.md still has the old § Conventions sentence (R12)"
	fi
	grep -qF '`src/result.cpp`, `src/result.hpp`' "$root/AGENTS.md" || fail "AGENTS.md § Map has no line for src/result.* (PLAN § 2)"
}


# --- mutants ---------------------------------------------------------------------------------------

MUT="$work/mut"

# mutants_setup: a copy whose result.cpp and math.cpp have the definitions of their entry points
# renamed, and whose cli.cpp has parse_args renamed, with verify/mutant.cpp linked in their place;
# pass-through control run green. The copy builds at -O0: the checks, not the speed, are under test.
mutants_setup() {
	copy_tree "$MUT"
	wire_defs src/result.cpp is_decisive verdict_line json_record status_of report_result report_run
	wire_defs src/math.cpp test_quick_factor test_p_minus_one test_lucas_lehmer run_in_order test_all
	[ "$PHASE" -ge 2 ] && wire_defs src/math.cpp smallest_prime_factor composite_by_exponent check_exponent
	wire src/cli.cpp parse_args
	cp "$here/mutant.cpp" "$MUT/src/gate_mutant.cpp" || fail "copy mutant.cpp"
	echo "#define GATE_PHASE $PHASE" >"$MUT/src/gate_phase.h" || fail "write gate_phase.h"
	MODS=$(sed -n 's/^MODULES *:= *//p' "$MUT/Makefile")
	test -n "$MODS" || fail "no MODULES line in the Makefile"
	( cd "$MUT" && "$MAKE" CXX=clang++ CXXFLAGS=-O0 MODULES="$MODS gate_mutant" bin/mps bin/check ) >"$logs/mut-build.log" 2>&1 \
		|| { tail -30 "$logs/mut-build.log" >&2; fail "the tree does not build through the gate's wrappers: result.hpp, math.hpp and cli.hpp keep the names and signatures PLAN § 2 gives (R13)"; }
	: >"$work/applied-control"
	( cd "$MUT" && env GATE_MUTANT_LOG="$work/applied-control" "$MAKE" CXX=clang++ CXXFLAGS=-O0 MODULES="$MODS gate_mutant" check ) >"$logs/mut-control.log" 2>&1 \
		|| { tail -30 "$logs/mut-control.log" >&2; fail "make check is red through the gate's pass-through wrappers (R13)"; }
	summary_ok "$logs/mut-control.log" || fail "make check summaries missing through the wrappers"
	test ! -s "$work/applied-control" || fail "the control run applied a mutant"
}

# wire_defs FILE NAME...: rename each NAME's definition, and nothing else, to gate_real_NAME: the one
# line at file scope (unindented) that holds `NAME(` and does not end in `;` (PLAN § 5: each is
# defined out of line, at file scope, its name on the definition's first line)
wire_defs() {
	wf=$1
	shift
	test -f "$MUT/$wf" || fail "$wf is missing"
	for wn in "$@"; do
		awk -v n="$wn" '
			BEGIN { re = "^([A-Za-z_][^;]*[^A-Za-z0-9_])?" n "\\(" }
			$0 ~ re && $0 !~ /;[ \t]*$/ && !done { i = index($0, n "("); $0 = substr($0, 1, i - 1) "gate_real_" substr($0, i); done = 1; hits++ }
			{ print }
			END { if (hits != 1) exit 3 }' "$MUT/$wf" >"$work/wired" \
			|| fail "no definition of $wn at file scope in $wf (PLAN § 5: the gates wrap it)"
		mv "$work/wired" "$MUT/$wf" || fail "wire $wf"
	done
}

# wire FILE NAME...: prepend `#define NAME gate_real_NAME` for each NAME
wire() {
	wf=$1
	shift
	test -f "$MUT/$wf" || fail "$wf is missing"
	{ for wn in "$@"; do echo "#define $wn gate_real_$wn"; done; cat "$MUT/$wf"; } >"$work/wired" \
		&& mv "$work/wired" "$MUT/$wf" || fail "wire $wf"
}

# mutant_run NAME MUTANT ARGS...: the mutant build's bin/mps under GATE_MUTANT=MUTANT, as run does
mutant_run() {
	mr=$1 mm=$2
	shift 2
	rd="$work/cwd/$mr"
	mkdir -p "$rd" || fail "mkdir $rd"
	( cd "$rd" && env -i PATH=/usr/bin:/bin HOME="$rd" LC_ALL=C NO_COLOR=1 GATE_MUTANT="$mm" "$MUT/bin/mps" "$@" ) >"$logs/$mr.out" 2>"$logs/$mr.err"
	status=$?
}

# mutant_drives: what bin/mps does when a test decides, or when the exponent search lies, through
# the wrappers: so main.cpp's path really runs through test_all's stop and the re-check
mutant_drives() {
	mutant_run md-decide t-tf-composite 31
	want_status md-decide 40 "mps 31 with a trial factoring that finds a factor: the composite's status (R7, R8)"
	want_out md-decide "M31 tf: composite, factor 3$TAIL" "mps 31 with a deciding trial factoring: one line, then the run stops (R7)"
	want_err md-decide "$START_TF" "mps 31 with a deciding trial factoring: P-1 and Lucas-Lehmer never start (R7)"
	mutant_run md-prime t-ll-prime 31
	want_status md-prime 0 "mps 31 with a Lucas-Lehmer that proves prime (R8)"
	want_out md-prime "M31 $NV_TF${nl}M31 $NV_P1${nl}M31 ll: prime$TAIL" "mps 31 ending in a prime: three lines in order (R7)"
	if [ "$PHASE" -ge 2 ]; then
		mutant_run md-liar g-liar 33
		want_status md-liar 20 "mps 33 when the exponent search claims 5: the re-check refuses it, a runtime error (R3, R9, § 1)"
		want_out md-liar '' "mps 33 when the exponent search claims 5: no verdict printed (R3, R9, § 1)"
		err_lacks md-liar "Running" "mps 33 when the exponent search claims 5: no test runs"
		grep -q . "$logs/md-liar.err" || fail "mps 33 when the exponent search claims 5: nothing reported on stderr (R9)"
	fi
}

# mutant ID WHAT [HALF]: make check is red under GATE_MUTANT=ID, with a failed check, and the
# mutant was reached; with HALF (check or cli), that half itself fails
mutant() {
	ml="$work/applied-$1"
	: >"$ml"
	if ( cd "$MUT" && env GATE_MUTANT="$1" GATE_MUTANT_LOG="$ml" "$MAKE" CXX=clang++ CXXFLAGS=-O0 MODULES="$MODS gate_mutant" check ) >"$logs/mut-$1.log" 2>&1; then
		test -s "$ml" || fail "no check in make check exercises what mutant $1 breaks ($2)"
		fail "make check stays green when mutant $1 breaks $2"
	fi
	test -s "$ml" || { tail -20 "$logs/mut-$1.log" >&2; fail "make check went red under $1 without reaching it ($2)"; }
	grep -Eq '^(check|cli): [0-9]+ passed, [1-9][0-9]* failed$' "$logs/mut-$1.log" \
		|| { tail -20 "$logs/mut-$1.log" >&2; fail "under $1, make check went red with no failed check ($2)"; }
	if [ -n "${3:-}" ]; then
		grep -Eq "^$3: [0-9]+ passed, [1-9][0-9]* failed\$" "$logs/mut-$1.log" \
			|| { tail -20 "$logs/mut-$1.log" >&2; fail "under $1, $3's own checks stay green ($2)"; }
	fi
	MUTANTS=$((MUTANTS + 1))
}

# edit_mutant FILE FROM TO WHAT [HALF] [NTH]: make check is red with FROM in FILE replaced by TO
# (every occurrence, or only the NTH), with a failed check, in HALF (check or cli) when given
edit_mutant() {
	hm="$work/edit"
	if [ ! -d "$hm" ]; then
		copy_tree "$hm"
		mkdir -p "$work/orig/src" || fail "mkdir"
		( cd "$hm" && "$MAKE" CXX=clang++ CXXFLAGS=-O0 bin/mps bin/check ) >"$logs/edit-build.log" 2>&1 || fail "the edit-mutant copy does not build"
	fi
	test -f "$work/orig/$1" || cp "$hm/$1" "$work/orig/$1" || fail "save $1"
	for em_f in "$work"/orig/src/* "$work"/orig/AGENTS.md; do
		[ -f "$em_f" ] || continue
		case $em_f in
			*/AGENTS.md) cp "$em_f" "$hm/AGENTS.md" ;;
			*) cp "$em_f" "$hm/src/${em_f##*/}" ;;
		esac || fail "restore ${em_f##*/}"
	done
	awk -v from="$2" -v to="$3" -v nth="${6:-0}" '{ out = ""; while ((i = index($0, from)) > 0) { k++; out = out substr($0, 1, i - 1) ((nth == 0 || k == nth) ? to : from); $0 = substr($0, i + length(from)) } print out $0 }' \
		"$work/orig/$1" >"$hm/$1" || fail "edit $1"
	! cmp -s "$work/orig/$1" "$hm/$1" || fail "$1 has no '$2' to break ($4)"
	case $1 in
		src/*)
			/bin/rm -f "$hm/bin/mps" "$hm/bin/check"
			sleep 1   # make 3.81 compares whole seconds
			touch "$hm/$1"
			;;
	esac
	if ( cd "$hm" && "$MAKE" CXX=clang++ CXXFLAGS=-O0 check ) >"$logs/edit-mut.log" 2>&1; then
		fail "make check stays green with '$2' changed to '$3' in $1 ($4)"
	fi
	test -x "$hm/bin/check" && test -x "$hm/bin/mps" || { tail -20 "$logs/edit-mut.log" >&2; fail "the edit of '$2' in $1 did not build: the red is not the checks' ($4)"; }
	grep -Eq '^(check|cli): [0-9]+ passed, [1-9][0-9]* failed$' "$logs/edit-mut.log" \
		|| { tail -20 "$logs/edit-mut.log" >&2; fail "with '$2' changed in $1, make check went red with no failed check ($4)"; }
	if [ -n "${5:-}" ]; then
		grep -Eq "^$5: [0-9]+ passed, [1-9][0-9]* failed\$" "$logs/edit-mut.log" \
			|| { tail -20 "$logs/edit-mut.log" >&2; fail "with '$2' changed in $1, $5's own checks stay green ($4)"; }
	fi
	MUTANTS=$((MUTANTS + 1))
}


# --- sweeps ----------------------------------------------------------------------------------------

# r13_sweep: no check under tests/ expects a run with a PVAL and without -p, help or version to exit 0
r13_sweep() {
	cs="$root/tests/cli.sh"
	# Names: every expectation named "... exits 0" is a -p, help or version run
	grep -n 'exits 0"' "$cs" | grep -vE 'expect "(\$option|--help|-h|-p |-p \$|0 -p|-v|--version|mps alone)[^"]* exits 0"' >"$work/exits0"
	test ! -s "$work/exits0" || { cat "$work/exits0" >&2; fail "tests/cli.sh names a non--p PVAL run that exits 0 (R13)"; }
	# Pairs: a status 0 compared after a run of bin/mps without -p, -h, -v, --help or --version
	awk '
		/^[ \t]*(run|run_all|run_stderr)([ \t]|$)/ { args = $0; next }
		/"\$status" -eq 0([^0-9]|$)|"\$status" = 0([^0-9]|$)/ {
			if (args !~ /(^|[ \t"])(-p|--primes|-h|--help|-v|--version|\$option)([ \t"]|$)/ && args !~ /^[ \t]*run(_all|_stderr)?[ \t]*$/)
				{ print FILENAME ":" NR ": " $0 "   (after: " args ")"; bad = 1 }
		}
		END { exit bad }' "$cs" >"$work/pairs" || { cat "$work/pairs" >&2; fail "tests/cli.sh expects exit 0 after a non--p PVAL run (R13)"; }
	if grep -rn 'exit 0\|exits 0' "$root/tests" --include='*.cpp' | grep -iv 'prime\|help\|version\|-p\|primes' >"$work/cpp0"; then
		test ! -s "$work/cpp0" || { cat "$work/cpp0" >&2; fail "a check under tests/ names an exit 0 that is not prime, help, version or -p (R13)"; }
	fi
}

hygiene() {
	( cd "$root" && git diff --quiet origin/main -- README.md ) || fail "README.md changed (GOAL non-goal: it documents no options)"
	if grep -rnE '(^|[^A-Za-z0-9_])(R[0-9]{1,2}|P[1-9])([^A-Za-z0-9_-]|$)' "$root/src" "$root/tests"; then
		fail "spec R#/P# ids in source (AGENTS.md § Conventions)"
	fi
	if grep -n 'include "ansi.hpp"' "$root/src/result.cpp" "$root/src/result.hpp"; then
		fail "the result module includes ansi.hpp: results are never coloured (PLAN § 2)"
	fi
}


# --- P1: results, the line, the record, the statuses ---------------------------------------------

p1_behaviour() {
	for cxx in $COMPILERS; do
		BIN="$work/tree-$cxx/bin/mps"
		for p in 2 3 31 999999937; do
			for spelling in plain all; do
				if [ "$spelling" = all ]; then set -- -t all "$p"; else set -- "$p"; fi
				run "r-$p-$spelling" "$@"
				want_status "r-$p-$spelling" 42 "mps $*: no test decides ($cxx, R8)"
				want_out "r-$p-$spelling" "$(lines3 "$p")" "mps $*: one no-verdict line per test, tf, p1, ll ($cxx, R2, R6, R7)"
				want_err "r-$p-$spelling" "$START_ALL" "mps $*: the three start lines, unchanged ($cxx, R2)"
			done
		done
		run r-zeros 0000000031
		want_out r-zeros "$(lines3 31)" "mps 0000000031 names M31 ($cxx, R6)"
		run_merged r-merged 31
		want_out r-merged "$START_TF${nl}M31 $NV_TF$nl$START_P1${nl}M31 $NV_P1$nl$START_LL${nl}M31 $NV_LL" \
			"mps 31: each line as its test completes ($cxx, R6, R7)"
		for name in tf trial-factor p1 p-minus-1 ll lucas-lehmer; do
			case $name in
				tf|trial-factor)  line="M31 $NV_TF" start=$START_TF ;;
				p1|p-minus-1)     line="M31 $NV_P1" start=$START_P1 ;;
				ll|lucas-lehmer)  line="M31 $NV_LL" start=$START_LL ;;
			esac
			run "r-t-$name" -t "$name" 31
			want_status "r-t-$name" 42 "mps -t $name 31 ($cxx, R8)"
			want_out "r-t-$name" "$line" "mps -t $name 31: its one line ($cxx, R6)"
			want_err "r-t-$name" "$start" "mps -t $name 31: its one start line ($cxx, R2)"
		done
		run r-help --help;            want_status r-help 0 "--help ($cxx, R8)"
		run r-version --version;      want_status r-version 0 "--version ($cxx, R8)"
		want_out r-version "mps v$V" "--version ($cxx)"
		run r-primes -p 30;           want_status r-primes 0 "-p 30 ($cxx, R8)"
		want_out r-primes "2${nl}3${nl}5${nl}7${nl}11${nl}13${nl}17${nl}19${nl}23${nl}29" "-p 30 lists the primes ($cxx, R8)"
		run r-badopt --no-such 31;    want_status r-badopt 1 "an unknown option ($cxx, R8: the error classes keep their values)"
		want_out r-badopt '' "an unknown option prints no verdict ($cxx)"
		run r-badpval abc;            want_status r-badpval 1 "a bad PVAL ($cxx, R8)"
		RUN_COLOR=1 run r-colour 31
		if grep -q "$(printf '\033')" "$logs/r-colour.out"; then show r-colour; fail "a verdict line is coloured under FORCE_COLOR ($cxx, R6)"; fi
		grep -q "$(printf '\033')" "$logs/r-colour.err" || fail "FORCE_COLOR colours nothing at all: the colour check is vacuous ($cxx)"
	done
	for cxx in $COMPILERS; do
		probe_printer "$cxx"
	done
	agents_r12
}

probe_printer() {
	echo "probe printer: ok" >"$work/want-printer"
	probe_want "$1" printer "the printer, the line, the record and the statuses on constructed results (R1, R6-R10)"
	pr=$(probe "$1")
	( cd "$work" && "$pr" records "$work/records-$1.out" ) >"$logs/probe-records-$1.log" 2>&1 || fail "probe records ($1)"
	cp "$work/records-$1.out" "$logs/records-$1.out"
	want_records "records-$1" "the records of each outcome do not parse as the record PLAN gives ($1, R10)" \
		"31:tf:nv:$R_TF" 31:ll:prime 31:ll:pp '33:ptest:f:2^3-1' 999999937:ll:r:0123456789ABCDEF 5:p1:nvx
}

p1_mutants() {
	mutant_drives
	for m in s-nv-0 s-nv-50 s-comp-42 s-prime-41 s-pp-0 s-conflict s-error-0 s-json; do mutant "$m" "a status (R8, R9, R10)"; done
	mutant s-nv-0 "the no-verdict status of mps PVAL (R8, R13)" cli
	for m in r-conflict-prints@plain r-conflict-prints@json r-error-prints r-hold-off r-json-ignored r-quiet r-quiet-error; do
		mutant "$m" "the printer (R9, R10)"
	done
	for k in nv prime pp factor residue; do
		mutant "l-noversion@$k" "the version on every verdict line (R6, R13)"
		mutant "j-noversion@$k" "record_version on every record (R10, R13)"
		mutant "j-noprogram@$k" "program on every record (R10, R13)"
	done
	mutant l-noversion@nv "the version on every verdict line of bin/mps (R13)" cli
	for m in l-version l-noreason l-exponent l-word; do mutant "$m" "the verdict line (R6)"; done
	for m in j-invalid j-version2 j-progversion j-expstring j-worktype j-statusword j-noreason; do mutant "$m" "the JSON record (R10)"; done
	for m in a-order a-nostop a-stopfirst a-batch d-prime-not d-pp-not d-nv-yes; do mutant "$m" "-t all's order and stop (R7)"; done
	mutant a-order "the order of mps 31's lines (R7)" cli
	for m in t-ll-prime t-tf-composite t-noreason; do mutant "$m" "the stubs (R2)"; done
	edit_mutant AGENTS.md '- `src/result.hpp`, `src/result.cpp`' '- `src/result.hpp`' "the high-blast-radius list (R12, R13)" cli
	edit_mutant AGENTS.md '- `src/main.cpp`' '' "the high-blast-radius list, its second mention after § Map (R12, R13)" cli 2
	edit_mutant AGENTS.md 'a checkpoint or result format of `mps`, or a table' 'a checkpoint or result format, or a table' "the paragraph R12 rewords (R12, R13)" cli
	edit_mutant AGENTS.md 'A change to a checkpoint or result format of `mps` bumps' 'A change to the checkpoint or result format bumps' "the sentence R12 rewords (R12, R13)" cli
	edit_mutant src/math.cpp 'Running quick factor test' 'Running quick-factor test' "trial factoring's start line (R2)" cli
	edit_mutant src/math.cpp 'Running P-1 test' 'Running P-1 tests' "P-1's start line (R2)" cli
	edit_mutant src/math.cpp 'Running Lucas-Lehmer test' 'Running Lucas Lehmer test' "Lucas-Lehmer's start line (R2)" cli
	enum_mutant PROBABLE_PRIME 41 51
	enum_mutant ERR_UNKNOWN 30 31
	enum_mutant NO_VERDICT 42 43
	version_drive
}

# enum_mutant NAME FROM TO: make check is red with Status's NAME moved from FROM to TO (R8: the values)
enum_mutant() {
	em_line=$(grep -E "^[[:space:]]*$1[[:space:]]*=[[:space:]]*$2[[:space:]]*," "$root/src/main.hpp")
	test -n "$em_line" || fail "src/main.hpp has no '$1 = $2,' in enum Status (R8)"
	edit_mutant src/main.hpp "$em_line" "$(printf '%s' "$em_line" | sed "s/= *$2/= $3/")" "Status $1 = $2 (R8)"
}

# version_drive: with APP_VERSION changed, make check is green and every line and record names the
# new version: nothing else holds the program version
version_drive() {
	vd="$work/version"
	copy_tree "$vd"
	sed "s/^static const std::string APP_VERSION = \"$V\";/static const std::string APP_VERSION = \"9.9.9-gate\";/" "$root/src/main.hpp" >"$vd/src/main.hpp"
	( cd "$vd" && "$MAKE" CXX=clang++ CXXFLAGS=-O0 check ) >"$logs/version.log" 2>&1 \
		|| { tail -30 "$logs/version.log" >&2; fail "make check is red with APP_VERSION 9.9.9-gate: something else holds the version (R6, R13)"; }
	BIN="$vd/bin/mps"
	run vd-line 31
	grep -c " (mps 9.9.9-gate)\$" "$logs/vd-line.out" | grep -qx 3 || { show vd-line; fail "the verdict lines do not name APP_VERSION (R6)"; }
	if [ "$PHASE" -ge 3 ]; then
		run vd-json -j 31
		grep -c '"program": {"name": "mps", "version": "9.9.9-gate"}' "$logs/vd-json.out" | grep -qx 3 \
			|| { show vd-json; fail "the records do not name APP_VERSION (R10)"; }
	fi
}


# --- P2: the exponent check ----------------------------------------------------------------------

p2_behaviour() {
	for cxx in $COMPILERS; do
		BIN="$work/tree-$cxx/bin/mps"
		for pair in 4:2 9:3 33:3 999999999:3 999002449:31607; do
			p=${pair%%:*} a=${pair#*:}
			for name in none tf trial-factor p1 p-minus-1 ll lucas-lehmer all; do
				if [ "$name" = none ]; then set -- "$p"; else set -- -t "$name" "$p"; fi
				run "x-$p-$name" "$@"
				want_status "x-$p-$name" 40 "mps $*: composite ($cxx, R3)"
				want_out "x-$p-$name" "M$p ptest: composite, factor 2^$a-1$TAIL" "mps $*: the exponent check's one line, 2^$a-1 ($cxx, R3, R6)"
				err_lacks "x-$p-$name" "Running" "mps $*: no test runs ($cxx, R3)"
			done
		done
		for p in 2 3 31 999999929 999999937; do
			run "x-prime-$p" "$p"
			want_status "x-prime-$p" 42 "mps $p ($cxx, R4)"
			want_out "x-prime-$p" "$(lines3 "$p")" "mps $p: no exponent-check line, the tests run ($cxx, R4)"
		done
		for p in 4 33 999999999 31; do
			for s in -s --skip-ptest; do
				run "x-skip-$p$s" "$s" "$p"
				want_status "x-skip-$p$s" 42 "mps $s $p ($cxx, R5)"
				want_out "x-skip-$p$s" "$(lines3 "$p")" "mps $s $p: the tests run, no exponent-check line ($cxx, R5)"
				want_err "x-skip-$p$s" "$START_ALL" "mps $s $p: the three start lines ($cxx, R5)"
			done
		done
		run x-skip-ll -s -t ll 33
		want_out x-skip-ll "M33 $NV_LL" "mps -s -t ll 33 ($cxx, R5)"
		run x-skip-p -p 33 -s
		want_status x-skip-p 0 "mps -p 33 -s ($cxx, R8)"
	done
	# Every exponent from 2 to 2000 against trial division, one test so each run is one line
	BIN="$work/tree-clang++/bin/mps"
	p=2
	while [ "$p" -le 2000 ]; do
		a=$(smallest_factor "$p")
		run sweep -t tf "$p"
		if [ "$a" = "$p" ]; then
			want_status sweep 42 "mps -t tf $p (R4)"
			want_out sweep "M$p $NV_TF" "mps -t tf $p: prime exponent, the test runs (R4)"
		else
			want_status sweep 40 "mps -t tf $p (R3)"
			want_out sweep "M$p ptest: composite, factor 2^$a-1$TAIL" "mps -t tf $p: the smallest prime factor $a (R3)"
		fi
		p=$((p + 1))
	done
	for cxx in $COMPILERS; do
		echo "probe exponent: ok" >"$work/want-exponent"
		probe_want "$cxx" exponent "the exponent check and its re-check on the gate's numbers (R3, R4)"
	done
}

p2_mutants() {
	for m in x-next x-largest x-guard x-square; do mutant "$m" "the exponent check (R3)"; done
	for p in 4 33 999999999 999002449; do mutant "x-none@$p" "the exponent check on $p (R3)" cli; done
	for p in 2 3 31 999999937; do mutant "x-prime-composite@$p" "a prime exponent $p (R4)" cli; done
	for name in none tf trial-factor p1 p-minus-1 ll lucas-lehmer all; do
		mutant "x-skip-name@$name" "the exponent check under -t $name (R3)" cli
	done
	mutant x-skip-ignored "-s/--skip-ptest (R5)" cli
}


# --- P3: -j/--json and the help ------------------------------------------------------------------

HELP_TEXT="Usage:
  mps [-hv] [-p] PVAL [-s] [-t NAME] [-j] [-f PATH] [-d] [-D]
  Test if number is Mersenne prime.

Arguments:
  PVAL                    Mersenne prime exponent.

Options:
  -p, --primes            Print the primes below PVAL.
  -s, --skip-ptest        Skip the exponent check (that PVAL is prime).
  -t, --test        NAME  One of 'tf' ('trial-factor'), 'p1' ('p-minus-1'),
                          'll' ('lucas-lehmer') or 'all' (default: 'all').
  -j, --json              Print each result as one JSON object per line.
  -f, --file        PATH  File path for checkpointing.
  -d, --debug             Show debug messages.
  -D, --detailed          Show detailed logging messages.
  -v, --version           Show version and exit.
  -h, --help              Show this message and exit.

  Give a valued option as -x VALUE, -xVALUE, -x=VALUE, --long VALUE or
  --long=VALUE. Short flags may be grouped (-dD).

Exit status:
  0       Prime, or no verdict by design (--help, --version, -p).
  1       Wrong command-line invocation.
  2       Configuration error.
  10      File I/O or formatting error.
  20      Runtime error.
  30      Unexpected internal failure.
  40      Composite.
  41      Probable prime.
  42      No verdict.
  50-59   Reserved for burn-in."
USAGE_TEXT="Usage:
  mps [-hv] [-p] PVAL [-s] [-t NAME] [-j] [-f PATH] [-d] [-D]
  Check if number is Mersenne prime."

# records3 P: the specs of the three no-verdict records for M_P
records3() {
	echo "$1:tf:stub" "$1:p1:stub" "$1:ll:stub"
}

p3_behaviour() {
	for cxx in $COMPILERS; do
		BIN="$work/tree-$cxx/bin/mps"
		for p in 2 3 31 999999937 0000000031; do
			n=$(expr "$p" + 0)
			for flag in -j --json; do
				run "j-$p$flag" "$flag" "$p"
				want_status "j-$p$flag" 42 "mps $flag $p: the status without -j ($cxx, R10)"
				# shellcheck disable=SC2046
				want_records "j-$p$flag" "mps $flag $p: three no-verdict records ($cxx, R10)" $(records3 "$n")
				want_err "j-$p$flag" "$START_ALL" "mps $flag $p: the start lines as without -j ($cxx, R10)"
			done
		done
		for pair in 4:2 33:3 999999999:3 999002449:31607; do
			p=${pair%%:*} a=${pair#*:}
			for name in none tf p-minus-1 ll all; do
				if [ "$name" = none ]; then set -- -j "$p"; else set -- -j -t "$name" "$p"; fi
				run "jx-$p-$name" "$@"
				want_status "jx-$p-$name" 40 "mps $* ($cxx, R10)"
				want_records "jx-$p-$name" "mps $*: the exponent check's record, 2^$a-1 ($cxx, R3, R10)" "$p:ptest:f:2^$a-1"
				err_lacks "jx-$p-$name" "Running" "mps $*: no test runs ($cxx, R3)"
			done
		done
		for spelling in '-jd 31' '-dj 31' '-sj 33' '-js 33' '31 -j' '-jt ll 31' '-j -j 31'; do
			set -f
			# shellcheck disable=SC2086
			run "jg" $spelling
			set +f
			want_status jg 42 "mps $spelling ($cxx, R10)"
			case $spelling in
				*'ll 31') want_records jg "mps $spelling ($cxx, R10)" 31:ll:stub ;;
				*33)      # shellcheck disable=SC2046
				          want_records jg "mps $spelling: -j groups with -s ($cxx, R10)" $(records3 33) ;;
				*)        # shellcheck disable=SC2046
				          want_records jg "mps $spelling: -j groups ($cxx, R10)" $(records3 31) ;;
			esac
		done
		run jdup -j -j 31
		test "$(grep -c 'Duplicate use of -j/--json' "$logs/jdup.err")" -eq 1 || { show jdup; fail "-j -j warns once ($cxx)"; }
		run jval --json=1 31
		want_status jval 1 "--json=1 is an argument error ($cxx, R10)"
		want_out jval '' "--json=1 prints nothing ($cxx, R10)"
		run jprimes -j -p 30
		want_status jprimes 0 "-j -p 30 ($cxx, R8)"
		want_out jprimes "2${nl}3${nl}5${nl}7${nl}11${nl}13${nl}17${nl}19${nl}23${nl}29" "-j -p 30 lists the primes ($cxx)"
		run jhelp --help
		want_out jhelp "$HELP_TEXT" "--help is the text PLAN § 2 gives ($cxx, R11)"
		run jh -h
		want_out jh "$HELP_TEXT" "-h is the help ($cxx, R11)"
		run jusage
		want_status jusage 0 "mps alone prints the usage ($cxx)"
		want_out jusage "$USAGE_TEXT" "the usage line names -j ($cxx, R11)"
		RUN_COLOR=1 run jcolour -j 31
		if grep -q "$(printf '\033')" "$logs/jcolour.out"; then show jcolour; fail "a record is coloured under FORCE_COLOR ($cxx, R10)"; fi
		RUN_COLOR=1 run jhelpc --help
		grep -qF "$(printf '\033[1mExit status:')" "$logs/jhelpc.out" || { show jhelpc; fail "'Exit status:' is not bold like the other headers ($cxx, PLAN § 2)"; }
		sed "s/$(printf '\033')\[[0-9;]*m//g" "$logs/jhelpc.out" >"$logs/jhelpc-plain.out"
		printf '%s\n' "$HELP_TEXT" | cmp -s - "$logs/jhelpc-plain.out" || fail "the coloured help, escapes removed, is not the help ($cxx, R11)"
	done
	same_commit "R11: the parser, the help and the usage line in one commit" \
		src/cli.hpp '{"--json",' \
		src/main.hpp '  -j, --json              Print each result as one JSON object per line.' \
		src/main.hpp '[-t NAME] [-j] [-f PATH]' \
		src/main.hpp 'Skip the exponent check (that PVAL is prime).' \
		src/main.hpp '=Exit status:' \
		src/cli.cpp "'j'"
}

p3_mutants() {
	mutant j-flag-ignored "-j/--json (R10)" cli
	mutant j-group-ignored "-j inside a group (R10)" cli
	mutant j-flag-always "plain output without -j (R6, R10)"
	edit_mutant src/main.hpp '  -j, --json              Print each result as one JSON object per line.' '' "the help's -j line (R11)" cli
	edit_mutant src/main.hpp '  42      No verdict.' '' "the help's status 42 (R11)" cli
	edit_mutant src/main.hpp '  50-59   Reserved for burn-in.' '' "the help's reserved statuses (R11)" cli
	edit_mutant src/main.hpp 'Skip the exponent check (that PVAL is prime).' 'Suppress initial prime test on PVAL.' "the help's -s (R11)" cli
	edit_mutant src/main.hpp '[-t NAME] [-j] [-f PATH]' '[-t NAME] [-f PATH]' "the usage line alone (R11)" cli 1
}
