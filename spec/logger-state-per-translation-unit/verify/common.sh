# Shared helpers for this cycle's phase gates (a cycle record, not a standing gate).
# Sourced by p1.sh, p2.sh, p3.sh and prepush.sh. POSIX sh. Each gate copies the tree as a clone would
# hold it, runs `make -B check` with clang++ and g++-16, builds 9a41271's bin/mps beside the tree's,
# drives both in empty directories with clean environments, checks which files changed and how, and
# proves `make check` goes red under each injected defect (PLAN § 6). A gate prints
# `GATE FAIL: <reason>` and exits 1 on the first failure, or ends with a `<gate>: green` line.

# The tree under test is the one holding this script, whatever the caller's directory.
root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "gate: not in a git tree" >&2; exit 1; }
slug=logger-state-per-translation-unit
spec=spec/$slug
here="$root/$spec/verify"
BASE=9a41271
work=$(mktemp -d "${TMPDIR:-/tmp}/mps-gate.XXXXXX") || { echo "gate: mktemp failed" >&2; exit 1; }
logs="$work/logs"
mkdir -p "$logs" || { echo "gate: mkdir $logs" >&2; exit 1; }
trap 'chmod -R u+rwx "$work" 2>/dev/null; /bin/rm -rf "$work"' EXIT INT TERM

MUTANTS=0
SAME=0
DETAILED=0
nl='
'
esc=$(printf '\033')

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
need clang++; need g++-16; need rsync; need git; need awk; need python3; need hostname; need tar; need cmp
COMPILERS='clang++ g++-16'

# This machine's host name, as gethostname gives it; never written anywhere but $work
HOST=$(hostname)
test -n "$HOST" || fail "hostname prints nothing"
HOST_SHORT=${HOST%%.*}

# The gates are frozen at the plan commit: this directory must match it, with nothing added.
plan_commit=$(cd "$root" && git log -1 --format=%H --grep="^\[fix\] Plan $slug" -- "$spec/verify")
test -n "$plan_commit" || fail "no plan commit for $spec/verify"
( cd "$root" && git diff --quiet "$plan_commit" -- "$spec/verify" ) \
	|| fail "$spec/verify differs from the plan commit $plan_commit: the gates are frozen"
test -z "$(cd "$root" && git ls-files --others --exclude-standard -- "$spec/verify")" \
	|| fail "untracked files in $spec/verify: the gates are frozen"
( cd "$root" && git cat-file -e "$BASE^{commit}" ) 2>/dev/null || fail "no commit $BASE in this repository"

# tell FILE [N]: the last N lines (default 30) of FILE on stderr, this machine's host name made <host>
# (a failed check may echo a detailed line); gate output never names the host
tell() {
	tail -"${2:-30}" "$1" | sed "s/$HOST_SHORT[^ '\"]*/<host>/g" >&2
}

# copy_tree DEST: the files git knows about (tracked, plus untracked and unignored), as a clone would
# hold them once committed. Ignored build output never comes along.
copy_tree() {
	mkdir -p "$1" || fail "mkdir $1"
	( cd "$root" && git ls-files -z --cached --others --exclude-standard ) \
		| rsync -a --from0 --files-from=- "$root/" "$1/" || fail "copy of the tree into $1"
	test ! -e "$1/lib" || fail "copy of the tree already has lib/"
	test ! -e "$1/bin" || fail "copy of the tree already has bin/"
}

# stamped FILE: FILE's lines that hold a detailed stamp (a date, a space and a time, as the detailed
# style writes them), with escape sequences removed; status 0 when there is one
stamped() {
	sed "s/$esc\[[0-9;]*m//g" "$1" | grep -E '[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}'
}


# --- make check, both compilers ------------------------------------------------------------------

summary_ok() {
	grep -Eq '^check: [1-9][0-9]* passed, 0 failed$' "$1" || return 1
	grep -Eq '^cli: [1-9][0-9]* passed, 0 failed$' "$1" || return 1
}

# build_all: a copy per compiler under $work/tree-<cxx>, `make -B check` green, no warnings, and
# nothing in its output a cluster job's check.log must not carry. Sets CHECKS and CLIS to the
# passed counts, and BIN to the clang++ build's bin/mps.
build_all() {
	for cxx in $COMPILERS; do
		d="$work/tree-$cxx"
		copy_tree "$d"
		( cd "$d" && "$MAKE" -B CXX="$cxx" check ) >"$logs/check-$cxx.log" 2>&1 \
			|| { tell "$logs/check-$cxx.log" 40; fail "$cxx: make check is red (R7)"; }
		summary_ok "$logs/check-$cxx.log" || { tell "$logs/check-$cxx.log" 40; fail "$cxx: make check summaries missing or wrong (R7)"; }
		if grep -q 'warning:' "$logs/check-$cxx.log"; then
			grep 'warning:' "$logs/check-$cxx.log" >&2
			fail "$cxx: warnings in the build"
		fi
		if stamped "$logs/check-$cxx.log" >"$work/stamped"; then
			sed "s/$HOST_SHORT[^ ]*/<host>/g" "$work/stamped" | head -5 >&2
			fail "$cxx: make check's output holds a detailed line: a check logs in the detailed style without capturing it, and the cluster job's check.log would carry it (PLAN § 2, rule 3)"
		fi
		# A host name shorter than six characters could match a compiler flag; the stamp test above
		# still holds there
		if [ "${#HOST_SHORT}" -ge 6 ] && grep -qF "$HOST_SHORT" "$logs/check-$cxx.log"; then
			fail "$cxx: make check's output names this machine's host: checks use the fake host check-host (PLAN § 2, rule 1)"
		fi
	done
	CHECKS=$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-clang++.log")
	CLIS=$(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-clang++.log")
	test "$CHECKS $CLIS" = "$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-g++-16.log") $(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-g++-16.log")" \
		|| fail "the two compilers ran different numbers of checks"
	BIN="$work/tree-clang++/bin/mps"
}

# build_base: 9a41271's bin/mps, built with clang++ at the Makefile's own flags, as BASE_BIN
build_base() {
	b="$work/base"
	mkdir -p "$b" || fail "mkdir $b"
	( cd "$root" && git archive "$BASE" ) | ( cd "$b" && tar -xf - ) || fail "cannot export $BASE"
	( cd "$b" && "$MAKE" CXX=clang++ bin/mps ) >"$logs/base-build.log" 2>&1 \
		|| { tell "$logs/base-build.log" 20; fail "$BASE's bin/mps does not build"; }
	BASE_BIN="$b/bin/mps"
}


# --- the files -------------------------------------------------------------------------------------

# Everything the design leaves alone (PLAN § 2), held equal to 9a41271
FROZEN='src/main.cpp src/main.hpp src/math.hpp src/math.cpp src/result.hpp src/result.cpp src/mp.hpp src/mp.cpp src/io.hpp src/io.cpp src/ansi.hpp src/ansi.cpp src/exc.hpp src/cli.hpp Makefile README.md ci tests/check.cpp tests/check.hpp'

cat >"$work/footprint.py" <<'EOF'
import difflib, subprocess, sys

root, base, path = sys.argv[1], sys.argv[2], sys.argv[3]
want_removed, want_added = sys.argv[4].split("|") if sys.argv[4] else [], sys.argv[5].split("|") if sys.argv[5] else []
strip = sys.argv[6] == "strip"
old = subprocess.run(["git", "-C", root, "show", "%s:%s" % (base, path)], capture_output=True, text=True, check=True).stdout.splitlines()
new = open("%s/%s" % (root, path)).read().splitlines()
removed, added = [], []
for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, old, new, autojunk=False).get_opcodes():
    if tag in ("replace", "delete"):
        removed += old[i1:i2]
    if tag in ("replace", "insert"):
        added += new[j1:j2]
def code(lines):
    out = []
    for line in lines:
        s = line.strip()
        if s == "" or s.startswith("//"):
            continue
        out.append(s if strip else line)
    return sorted(out)
bad = []
for line in added:
    if "/*" in line or "*/" in line or line.rstrip().endswith("\\"):
        bad.append("added line %r opens or closes a block comment or splices a line" % line)
if code(removed) != sorted(want_removed):
    bad.append("removed %r, want %r" % (code(removed), sorted(want_removed)))
if code(added) != sorted(want_added):
    bad.append("added %r, want %r" % (code(added), sorted(want_added)))
for w in want_added:
    n = sum(1 for line in new if (line.strip() if strip else line) == w)
    if n != 1:
        bad.append("%r appears %d times, want once" % (w, n))
if bad:
    print("%s against %s, comment and blank lines aside: %s" % (path, base, "; ".join(bad)), file=sys.stderr)
    sys.exit(1)
EOF

# footprint: the files the design leaves alone are 9a41271's; log.hpp differs only in the five
# declarations, and cli.cpp (from P2) only in the two statements, comment and blank lines aside
footprint() {
	cd "$root" || fail "cd $root"
	# shellcheck disable=SC2086
	git diff --quiet "$BASE" -- $FROZEN || { git diff --stat "$BASE" -- $FROZEN >&2; fail "a file PLAN § 2 leaves alone differs from $BASE (src/main.cpp, src/math.* and src/result.* among them)"; }
	test -z "$(git ls-files --others --exclude-standard -- src ci)" || fail "new files under src/ or ci/ (PLAN § 2)"
	t=$(printf '\t')
	python3 -I "$work/footprint.py" "$root" "$BASE" src/log.hpp \
		"${t}static Style style;|${t}static Level level;|${t}static std::string hostname;|${t}static std::string task_id;|${t}static std::string name;" \
		"${t}inline Style style;|${t}inline Level level;|${t}inline std::string hostname;|${t}inline std::string task_id;|${t}inline std::string name;" \
		exact || fail "src/log.hpp is not 9a41271's with the five declarations made inline (PLAN § 2; R5, R6)"
	if [ "$PHASE" -ge 2 ]; then
		python3 -I "$work/footprint.py" "$root" "$BASE" src/cli.cpp "" \
			"logging::set_level(logging::DEBUG);|logging::set_style(logging::DETAILED);" \
			strip || fail "src/cli.cpp is not 9a41271's with the two statements added (PLAN § 2; R1, R2, R4)"
	else
		git diff --quiet "$BASE" -- src/cli.cpp || fail "src/cli.cpp changes in P1 (PLAN § 2: the parser moves in P2)"
	fi
}

# wording: nothing under tests/ describes a per-file copy (R7); no spec ids in src/ or tests/
wording() {
	if grep -rniE "copy per translation unit|copy per file|this file's copy|per-file copy|per file copy" "$root/tests"; then
		fail "a file under tests/ still describes the per-file copy (R7)"
	fi
	if grep -rnE '(^|[^A-Za-z0-9_])(R[0-9]{1,2}|P[1-9])([^A-Za-z0-9_-]|$)' "$root/src" "$root/tests"; then
		fail "spec R#/P# ids in source (AGENTS.md § Conventions)"
	fi
}


# --- driving bin/mps -------------------------------------------------------------------------------

# drive BIN NAME ENV ARGS...: BIN in an empty directory, with only PATH, HOME, LC_ALL and ENV (NAME=VALUE
# words joined by commas, or '-') in its environment; stdout, stderr and the status into
# $logs/NAME.out, NAME.err and NAME.status
drive() {
	dv_bin=$1 dv_name=$2 dv_env=$3
	shift 3
	[ "$dv_env" = - ] && dv_env=
	dv_dir="$work/cwd/$dv_name"
	mkdir -p "$dv_dir" || fail "mkdir $dv_dir"
	dv_ifs=$IFS
	IFS=,
	set -f
	# shellcheck disable=SC2086
	( cd "$dv_dir" && exec env -i PATH=/usr/bin:/bin HOME="$dv_dir" LC_ALL=C $dv_env "$dv_bin" "$@" ) \
		>"$logs/$dv_name.out" 2>"$logs/$dv_name.err"
	dv_status=$?
	set +f
	IFS=$dv_ifs
	echo "$dv_status" >"$logs/$dv_name.status"
	test "$dv_status" -lt 128 || fail "mps $* [$dv_env]: killed by a signal"
	test -z "$(ls -A "$dv_dir")" || fail "mps $* [$dv_env]: left files in its working directory"
}

# Without -D: every message mps logs, and the runs that log nothing (R6)
R6_RUNS='31
2
3
33
999999999
-t tf 31
-t p1 31
-t ll 31
-t all 31
-s 33
--skip-ptest 4
-j 31
-j -t ll 31
-p 30
-p 0
-d 31
-d -t ll 31
-dj 31
--debug 33
-p -p 3
-pp 3
-s -s 31
-j -j 31
-d -d 31
-t ll -t tf 31
--file a --file b 31
-x 31
--nope 31
-t nope 31
-t
-f
--json=1 31
--debug=1 31
-f= 31
abc
0
1000000000
31 61
-- -1
-v
-h'

# With a -D the parser reads too late, or never as a flag (R2)
R2_RUNS='-x -D 31
-xD 31
-p -p -D 3
-ppD 3
-f -D 31
-t -D 31
-- -D
--detailed=1 31'

# same_as_base RUNS WHAT: under each environment, the tree's bin/mps and 9a41271's print the same
# stdout and stderr, byte for byte, and exit alike, for each line of RUNS (split at spaces)
same_as_base() {
	sb_what=$2
	sb_n=0
	while IFS= read -r sb_line; do
		[ -n "$sb_line" ] || continue
		set -f
		# shellcheck disable=SC2086
		set -- $sb_line
		set +f
		for sb_env in NO_COLOR=1 FORCE_COLOR=1 NO_COLOR=1,TASK_ID=abc FORCE_COLOR=1,TASK_ID=abc NO_COLOR=1,TASK_ID=; do
			sb_n=$((sb_n + 1))
			drive "$BIN" "same-t$sb_n" "$sb_env" "$@"
			drive "$BASE_BIN" "same-b$sb_n" "$sb_env" "$@"
			for sb_x in out err status; do
				cmp -s "$logs/same-t$sb_n.$sb_x" "$logs/same-b$sb_n.$sb_x" || {
					echo "--- $BASE" >&2; cat "$logs/same-b$sb_n.$sb_x" >&2
					echo "--- tree" >&2; sed "s/$HOST_SHORT[^ ]*/<host>/g" "$logs/same-t$sb_n.$sb_x" >&2
					fail "mps $sb_line [$sb_env]: $sb_x differs from $BASE's ($sb_what)"
				}
			done
		done
	done <<EOF
$1
EOF
	SAME=$((SAME + sb_n))
}

cat >"$work/stamp.py" <<'EOF'
import datetime, re, sys

tree, base, hosts, task = sys.argv[1:5]
hosts = hosts.split("\n")
host = hosts[0]
now = datetime.datetime.now()
def lines(p):
    return open(p, encoding="utf-8", errors="surrogateescape").read().split("\n")
t, b = lines(tree), lines(base)
def bad(msg):
    for h in hosts:
        msg = msg.replace(h, "<host>")
    print(msg, file=sys.stderr)
    sys.exit(1)
if len(b) < 2:
    bad("%s logs nothing for the paired run: the pair is hollow" % sys.argv[5])
if len(t) != len(b):
    bad("%d lines on stderr, %s's default run has %d" % (len(t) - 1, sys.argv[5], len(b) - 1))
if t[-1] != "":
    bad("stderr does not end in a newline")
tail = "[%s] " % task if task else ""
for i, (x, y) in enumerate(zip(t[:-1], b[:-1]), 1):
    m = re.fullmatch(r"([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}) (.*)", x, re.S)
    if not m:
        bad("line %d has no detailed stamp: %r" % (i, x))
    try:
        when = datetime.datetime.strptime(m.group(1), "%Y-%m-%d %H:%M:%S")
    except ValueError:
        bad("line %d: %r is not a date and time" % (i, m.group(1)))
    if abs((when - now).total_seconds()) > 120:
        bad("line %d: %s is not the local time (now %s)" % (i, m.group(1), now.strftime("%Y-%m-%d %H:%M:%S")))
    if not any(m.group(2) == h + " " + tail + y for h in hosts):
        bad("line %d is %r after its time, want <host> %r" % (i, m.group(2), tail + y))
EOF

# Pairs `TREE ARGS|BASE ARGS`: messages raised once parsing is over (P1), and while parsing (P2)
R1_AFTER_PARSE='-D -t ll 31|-t ll 31
-D 31|31
--detailed -s 33|-s 33
-Dj 31|-j 31
31 -D|31
-D -t tf -s 4|-t tf -s 4'
R1_WHILE_PARSING='-D -p -p 3|-p -p 3
-Dpp 3|-pp 3
-p -D -p 3|-p -p 3
-D -s -s 31|-s -s 31
-D -j -j 31|-j -j 31
-D -d -d 31|-d -d 31
-D -t ll -t tf 31|-t ll -t tf 31
-D -f a -f b 31|-f a -f b 31
-D -x 31|-x 31
-D --nope 31|--nope 31
-D -t nope 31|-t nope 31
-D -t|-t
-D abc|abc
-D 1000000000|1000000000
-D 31 61|31 61
-D -- -1|-- -1'

# detailed PAIRS: for each pair, under TASK_ID=abc, unset and empty (NO_COLOR=1), the tree's bin/mps
# prints on stderr, line for line, 9a41271's stderr for the base arguments, each line behind a stamp:
# a date and a time, this machine's host, then `[abc] ` only under TASK_ID=abc; stdout and status
# are 9a41271's (R1, R3)
detailed() {
	dt_n=0
	while IFS='|' read -r dt_tree dt_base; do
		[ -n "$dt_tree" ] || continue
		dt_n=$((dt_n + 1))
		set -f
		# shellcheck disable=SC2086
		set -- $dt_base
		set +f
		drive "$BASE_BIN" "det-b$dt_n" NO_COLOR=1 "$@"
		for dt_task in abc unset empty; do
			case $dt_task in
				abc)   dt_env=NO_COLOR=1,TASK_ID=abc dt_want=abc ;;
				unset) dt_env=NO_COLOR=1 dt_want= ;;
				empty) dt_env=NO_COLOR=1,TASK_ID= dt_want= ;;
			esac
			set -f
			# shellcheck disable=SC2086
			set -- $dt_tree
			set +f
			# macOS may rename the host on a network conflict: either name seen around the run will do
			dt_h1=$(hostname)
			drive "$BIN" "det-t$dt_n-$dt_task" "$dt_env" "$@"
			dt_h2=$(hostname)
			python3 -I "$work/stamp.py" "$logs/det-t$dt_n-$dt_task.err" "$logs/det-b$dt_n.err" "$dt_h1$nl$dt_h2" "$dt_want" "$BASE" \
				|| fail "mps $dt_tree [TASK_ID $dt_task]: not $BASE's lines for '$dt_base' in the detailed style (R1, R3)"
			cmp -s "$logs/det-t$dt_n-$dt_task.out" "$logs/det-b$dt_n.out" || fail "mps $dt_tree [TASK_ID $dt_task]: stdout is not $BASE's for '$dt_base'"
			cmp -s "$logs/det-t$dt_n-$dt_task.status" "$logs/det-b$dt_n.status" || fail "mps $dt_tree [TASK_ID $dt_task]: status is not $BASE's for '$dt_base'"
			DETAILED=$((DETAILED + 1))
		done
	done <<EOF
$1
EOF
}


# --- defects ---------------------------------------------------------------------------------------

MUT="$work/mut"

# mutants_setup: a copy of the tree for the defects, green as it stands
mutants_setup() {
	copy_tree "$MUT"
	mkdir -p "$work/orig/src" || fail "mkdir $work/orig/src"
	( cd "$MUT" && "$MAKE" -B CXX=clang++ check ) >"$logs/mut-control.log" 2>&1 \
		|| { tell "$logs/mut-control.log"; fail "make check is red on the copy for the defects"; }
	summary_ok "$logs/mut-control.log" || fail "make check summaries missing on the copy for the defects"
}

# mutant ID CHECK CLI FILE FROM TO [FILE FROM TO]...: with every FROM replaced by its TO (each must
# occur; awk reads \n and \t in them), `make -B check` at the Makefile's own flags (clang++) builds
# and goes red, failing at least bin/check's check labelled CHECK and tests/cli.sh's expectation named
# CLI ('-' for none; at least one is named). Other checks may fail too.
mutant() {
	mu_id=$1 mu_check=$2 mu_cli=$3
	shift 3
	mu_files=
	while [ $# -ge 3 ]; do
		mu_f=$1
		test -f "$work/orig/$mu_f" || cp "$MUT/$mu_f" "$work/orig/$mu_f" || fail "save $mu_f"
		awk -v from="$2" -v to="$3" '
			{ out = ""; while ((i = index($0, from)) > 0) { k++; out = out substr($0, 1, i - 1) to; $0 = substr($0, i + length(from)) } print out $0 }
			END { if (!k) exit 3 }' "$MUT/$mu_f" >"$work/edited" \
			|| fail "defect $mu_id: $mu_f has no '$2' to break (PLAN § 2 gives its text)"
		mv "$work/edited" "$MUT/$mu_f" || fail "edit $mu_f"
		mu_files="$mu_files $mu_f"
		shift 3
	done
	test $# -eq 0 || fail "defect $mu_id: edits come in threes"
	/bin/rm -f "$MUT/bin/mps" "$MUT/bin/check"
	( cd "$MUT" && "$MAKE" -B CXX=clang++ check ) >"$logs/mut-$mu_id.log" 2>&1
	mu_status=$?
	for mu_f in $mu_files; do
		cp "$work/orig/$mu_f" "$MUT/$mu_f" || fail "restore $mu_f"
	done
	test "$mu_status" -ne 0 || fail "make check stays green under defect $mu_id"
	test -x "$MUT/bin/check" && test -x "$MUT/bin/mps" \
		|| { tell "$logs/mut-$mu_id.log" 20; fail "defect $mu_id did not build: the red is not the checks'"; }
	test "$mu_check$mu_cli" != -- || fail "defect $mu_id names no check that must fail"
	if [ "$mu_check" != - ]; then
		grep -Eq '^check: [0-9]+ passed, [1-9][0-9]* failed$' "$logs/mut-$mu_id.log" \
			|| { tell "$logs/mut-$mu_id.log" 20; fail "under defect $mu_id, bin/check's checks stay green"; }
		grep -qxF " FAIL $mu_check" "$logs/mut-$mu_id.log" \
			|| { grep '^ FAIL' "$logs/mut-$mu_id.log" | sed "s/$HOST_SHORT[^ ']*/<host>/g" >&2; fail "under defect $mu_id, the check '$mu_check' passes"; }
	fi
	if [ "$mu_cli" != - ]; then
		grep -Eq '^cli: [0-9]+ passed, [1-9][0-9]* failed$' "$logs/mut-$mu_id.log" \
			|| { tell "$logs/mut-$mu_id.log" 20; fail "under defect $mu_id, tests/cli.sh's checks of bin/mps stay green"; }
		grep -qxF " FAIL $mu_cli" "$logs/mut-$mu_id.log" \
			|| { grep '^ FAIL' "$logs/mut-$mu_id.log" | sed "s/$HOST_SHORT[^ ']*/<host>/g" >&2; fail "under defect $mu_id, tests/cli.sh's expectation '$mu_cli' passes"; }
	fi
	MUTANTS=$((MUTANTS + 1))
}

# The task stamp dropped from one file's messages: its include of the logger sees a task id of its own
lost_stamp='#define task_id task_id_lost\n#include "log.hpp"\n#undef task_id'

# The names PLAN § 2 gives the checks and tests/cli.sh's expectations
C_ONE="the logger's state is one copy"
C_LEVEL="the level holds across units"
C_STYLE="the detailed style holds across units"
C_PRINTER="the printer's errors log in the shared style"
C_PARSER="-D and -d apply as the parser reads them"
S_START="-D -t ll 31 stamps its start line with TASK_ID"
S_NO_TASK="-D 31 without TASK_ID has no task stamp"
S_COLOUR="-t ll 31 under FORCE_COLOR logs its exact bytes"
S_WARN="-D -p -p 3 stamps its warning"
S_ERROR="-D -x 31 stamps its error"
S_LATE="-x -D 31 errs in the default style"
S_DEBUG="-d 31 logs as without -d"

p1_mutants() {
	mutant all-static "$C_ONE" "$S_START" \
		src/log.hpp 'inline Style style;' 'static Style style;' \
		src/log.hpp 'inline Level level;' 'static Level level;' \
		src/log.hpp 'inline std::string hostname;' 'static std::string hostname;' \
		src/log.hpp 'inline std::string task_id;' 'static std::string task_id;' \
		src/log.hpp 'inline std::string name;' 'static std::string name;'
	mutant level-static "$C_LEVEL" - \
		src/log.hpp 'inline Level level;' 'static Level level;'
	mutant style-static "$C_STYLE" "$S_START" \
		src/log.hpp 'inline Style style;' 'static Style style;'
	mutant hostname-static "$C_STYLE" "$S_START" \
		src/log.hpp 'inline std::string hostname;' 'static std::string hostname;'
	mutant name-static "$C_ONE" - \
		src/log.hpp 'inline std::string name;' 'static std::string name;'
	mutant stamp-math - "$S_START" \
		src/math.cpp '#include "log.hpp"' "$lost_stamp"
	mutant stamp-result "$C_PRINTER" - \
		src/result.cpp '#include "log.hpp"' "$lost_stamp"
	mutant no-bold - "$S_COLOUR" \
		src/log.hpp 'bold(color_by_level[level](name))' 'color_by_level[level](name)'
	mutant empty-stamp - "$S_NO_TASK" \
		src/log.hpp 'if (not task_id.empty() ) {' 'if (true) {'
}

p2_mutants() {
	mutant late-D "$C_PARSER" "$S_WARN" \
		src/cli.cpp 'logging::set_style(logging::DETAILED);' ';'
	mutant late-d "$C_PARSER" - \
		src/cli.cpp 'logging::set_level(logging::DEBUG);' ';'
	mutant stamp-cli "$C_PARSER" "$S_WARN" \
		src/cli.cpp '#include "log.hpp"' "$lost_stamp"
	mutant stamp-main - "$S_ERROR" \
		src/main.cpp '} catch (cli::argument_error const& error) {' '} catch (cli::argument_error const& error) {\n\t\tlogging::task_id.clear();'
	mutant prescan - "$S_LATE" \
		src/main.cpp 'logging::init();' 'logging::init();\n\tfor (int i = 1; i < argc; i++)\n\t\tif (std::string(argv[i]) == "-D")\n\t\t\tlogging::set_style(logging::DETAILED);'
	mutant d-detailed - "$S_DEBUG" \
		src/cli.cpp 'logging::set_level(logging::DEBUG);' 'logging::set_level(logging::DEBUG); logging::set_style(logging::DETAILED);'
	mutant D-debug "$C_PARSER" - \
		src/cli.cpp 'logging::set_style(logging::DETAILED);' 'logging::set_style(logging::DETAILED); logging::set_level(logging::DEBUG);'
}
