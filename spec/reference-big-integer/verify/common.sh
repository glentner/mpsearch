# Shared helpers for this cycle's phase gates (a cycle record, not a standing gate).
# Sourced by p1.sh … p5.sh and prepush.sh, after `PHASE=<n>`. POSIX sh. Each gate copies the tree as a
# clone would hold it, runs `make -B check` with clang++ and g++-16, holds the files PLAN § 2 leaves
# alone to c9b82ff, drives the tree's bin/mps against c9b82ff's, reproduces every vector file with
# two Pythons and recomputes every record in Python (vectors.py), probes the tree's BigInt against
# Python on cases of its own (probe.cpp, probe.py), proves `make check` goes red under each injected
# bug (mutant.cpp) and each tampered vector file, checks the note and AGENTS.md (docs.py), and times
# `make check` against c9b82ff's (PLAN § 6). A gate prints `GATE FAIL: <reason>` and exits 1 on the
# first failure, or ends with a `<gate>: green` line. Gates are cumulative: each phase's gate runs
# every earlier phase's checks too, and none requires a phase's exact file set.

# The tree under test is the one holding this script, whatever the caller's directory.
root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "gate: not in a git tree" >&2; exit 1; }
slug=reference-big-integer
spec=spec/$slug
here="$root/$spec/verify"
BASE=c9b82ff
BUDGET=3.0
work=$(mktemp -d "${TMPDIR:-/tmp}/mps-gate.XXXXXX") || { echo "gate: mktemp failed" >&2; exit 1; }
logs="$work/logs"
mkdir -p "$logs" || { echo "gate: mkdir $logs" >&2; exit 1; }
trap 'chmod -R u+rwx "$work" 2>/dev/null; /bin/rm -rf "$work"' EXIT INT TERM

MUTANTS=0
TAMPERS=0
SAME=0
nl='
'

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
# A plan commit made after code landed is no plan: between c9b82ff and the plan commit, no code, test,
# note or AGENTS.md changes (a revised plan before the build keeps the prefix and passes)
test -z "$(cd "$root" && git diff --name-only "$BASE" "$plan_commit" -- src tests doc ci Makefile AGENTS.md README.md)" \
	|| fail "the plan commit $plan_commit follows changes outside spec/: the gates are frozen at the plan made before the build"

# limit SECONDS CMD...: CMD, killed after SECONDS (a mutant that never ends must not hang the gate)
limit() {
	perl -e '$t = shift; $SIG{ALRM} = sub { kill 9, $pid; exit 124 }; $pid = fork; if (!$pid) { exec @ARGV; exit 127 } alarm $t; waitpid $pid, 0; exit($? & 127 ? 128 + ($? & 127) : $? >> 8)' "$@"
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


# --- make check, both compilers ------------------------------------------------------------------

summary_ok() {
	grep -Eq '^check: [1-9][0-9]* passed, 0 failed$' "$1" || return 1
	grep -Eq '^cli: [1-9][0-9]* passed, 0 failed$' "$1" || return 1
}

# The check labels PLAN § 2 pins, by the phase that adds them
LABELS_1='SHA-256 matches the FIPS 180-4 examples
SplitMix64 matches its published outputs
every vector file carries its command and source
comparisons match the vectors
sums match the vectors
differences match the vectors
shifts match the vectors
folding modulo M_p matches the vectors
Res64 matches the vectors
hexadecimal matches the vectors and round-trips
errors are reported, never values'
LABELS_2='schoolbook products match the vectors
Karatsuba products match the vectors
products match the vectors
schoolbook squares match the vectors
Karatsuba squares match the vectors
squares match the vectors'
LABELS_3='general reduction matches the vectors
modular powers match the vectors
decimal matches the vectors and round-trips'
LABELS_4='the prime list carries its source
Lucas-Lehmer ends in zero for every listed prime
Lucas-Lehmer residues match the vectors
PRP ends in one for every listed prime
PRP residues match the vectors
the reference tests refuse exponents outside their theorems'

labels() {
	echo "$LABELS_1"
	[ "$PHASE" -ge 2 ] && echo "$LABELS_2"
	[ "$PHASE" -ge 3 ] && echo "$LABELS_3"
	[ "$PHASE" -ge 4 ] && echo "$LABELS_4"
	return 0
}

# build_all: a copy per compiler under $work/tree-<cxx>, `make -B check` green, no warnings, every
# pinned check run and passed; CHECKS and CLIS are the passed counts, BIN the clang++ bin/mps
build_all() {
	for cxx in $COMPILERS; do
		d="$work/tree-$cxx"
		copy_tree "$d"
		( cd "$d" && limit 900 "$MAKE" -B CXX="$cxx" check ) >"$logs/check-$cxx.log" 2>&1 \
			|| { tail -40 "$logs/check-$cxx.log" >&2; fail "$cxx: make check is red (R11)"; }
		summary_ok "$logs/check-$cxx.log" || { tail -40 "$logs/check-$cxx.log" >&2; fail "$cxx: make check summaries missing or wrong"; }
		if grep -q 'warning:' "$logs/check-$cxx.log"; then
			grep 'warning:' "$logs/check-$cxx.log" >&2
			fail "$cxx: warnings in the build"
		fi
		labels | while IFS= read -r lb; do
			grep -qxF "check $lb" "$logs/check-$cxx.log" || { echo "$lb" >"$work/missing-label"; break; }
		done
		test ! -s "$work/missing-label" || fail "$cxx: no check labelled '$(cat "$work/missing-label")' ran (PLAN § 2, the checks)"
	done
	CHECKS=$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-clang++.log")
	CLIS=$(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-clang++.log")
	test "$CHECKS $CLIS" = "$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-g++-16.log") $(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-g++-16.log")" \
		|| fail "the two compilers ran different numbers of checks"
	BIN="$work/tree-clang++/bin/mps"
}

# build_base: c9b82ff's tree and bin/mps, built with clang++ at the Makefile's own flags
build_base() {
	b="$work/base"
	mkdir -p "$b" || fail "mkdir $b"
	( cd "$root" && git archive "$BASE" ) | ( cd "$b" && tar -xf - ) || fail "cannot export $BASE"
	( cd "$b" && "$MAKE" CXX=clang++ check ) >"$logs/base-build.log" 2>&1 \
		|| { tail -20 "$logs/base-build.log" >&2; fail "$BASE's make check does not pass"; }
	BASE_BIN="$b/bin/mps"
}


# --- the files -------------------------------------------------------------------------------------

# Everything PLAN § 2 leaves alone, held equal to c9b82ff
FROZEN='spec/math-notes-and-references src/main.cpp src/main.hpp src/math.hpp src/math.cpp src/result.hpp src/io.hpp src/io.cpp src/ansi.hpp src/ansi.cpp src/exc.hpp src/cli.hpp src/cli.cpp src/log.hpp README.md ci tests/cli.sh tests/check.cpp tests/check.hpp tests/check_cli.cpp tests/check_log.cpp tests/check_math.cpp tests/check_result.cpp tests/log_peer.cpp tests/log_peer.hpp LICENSE LICENSE-EXCEPTION'

cat >"$work/footprint.py" <<'EOF'
import difflib, subprocess, sys

root, base, path = sys.argv[1], sys.argv[2], sys.argv[3]
want_removed = sys.argv[4].split("|") if sys.argv[4] else []
want_added = sys.argv[5].split("|") if sys.argv[5] else []
old = subprocess.run(["git", "-C", root, "show", "%s:%s" % (base, path)], capture_output=True, text=True, check=True).stdout.splitlines()
new = open("%s/%s" % (root, path)).read().splitlines()
removed, added = [], []
for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, old, new, autojunk=False).get_opcodes():
    if tag in ("replace", "delete"):
        removed += old[i1:i2]
    if tag in ("replace", "insert"):
        added += new[j1:j2]
def code(lines):
    return sorted(s.strip() for s in lines if s.strip() and not s.strip().startswith("//"))
bad = []
for line in added:
    if "/*" in line or "*/" in line or line.rstrip().endswith("\\"):
        bad.append("added line %r opens or closes a block comment or splices a line" % line)
if code(removed) != sorted(want_removed):
    bad.append("removed %r, want %r" % (code(removed), sorted(want_removed)))
if code(added) != sorted(want_added):
    bad.append("added %r, want %r" % (code(added), sorted(want_added)))
if bad:
    print("%s against %s, comment and blank lines aside: %s" % (path, base, "; ".join(bad)), file=sys.stderr)
    sys.exit(1)
EOF

# The file-local hex64 that moves from result.cpp to mp.* (P1), its code lines as c9b82ff has them
HEX64_LINES='static std::string hex64(std::uint64_t value) {|static const char DIGITS[] = "0123456789ABCDEF";|std::string text(16, '"'"'0'"'"');|for (int i = 15; i >= 0; i--) {|text[i] = DIGITS[value & 0xf];|value >>= 4;|}|return text;|}'

# footprint: the files PLAN § 2 leaves alone are c9b82ff's; result.cpp differs only by hex64 moving
# out and mp.hpp coming in; the Makefile only by MODULES gaining reference (required from P4); new
# files only where PLAN § 2 puts them; the files each phase must have exist
footprint() {
	cd "$root" || fail "cd $root"
	# shellcheck disable=SC2086
	git diff --quiet "$BASE" -- $FROZEN || { git diff --stat "$BASE" -- $FROZEN >&2; fail "a file PLAN § 2 leaves alone differs from $BASE"; }
	python3 -I "$work/footprint.py" "$root" "$BASE" src/result.cpp "$HEX64_LINES" '#include "mp.hpp"' \
		|| fail "src/result.cpp is not $BASE's with hex64 moved out and mp.hpp included (PLAN § 2; R4)"
	modules_new='MODULES  := ansi cli io mp math reference result'
	if git diff --quiet "$BASE" -- Makefile; then
		test "$PHASE" -lt 4 || fail "the Makefile's MODULES lacks reference (PLAN § 2, P4)"
	else
		python3 -I "$work/footprint.py" "$root" "$BASE" Makefile 'MODULES  := ansi cli io mp math result' "$modules_new" \
			|| fail "the Makefile differs from $BASE other than MODULES gaining reference (PLAN § 2; R9)"
	fi
	git ls-files --cached --others --exclude-standard -- src tests doc >"$work/files"
	git ls-tree -r --name-only "$BASE" -- src tests doc >"$work/base-files"
	grep -vxF -f "$work/base-files" "$work/files" >"$work/new-files" || true
	while IFS= read -r f; do
		case $f in
			src/reference.hpp|src/reference.cpp|tests/vector_reader.hpp|tests/vector_reader.cpp) ;;
			tests/check_mp.cpp|tests/check_reference.cpp|tests/vectors/generate.py) ;;
			tests/vectors/arith.txt|tests/vectors/folding.txt|tests/vectors/hex.txt|tests/vectors/products.txt) ;;
			tests/vectors/modular.txt|tests/vectors/decimal.txt|tests/vectors/reference.txt|tests/vectors/mersenne-exponents.txt) ;;
			doc/reference-arithmetic.md) ;;
			*) fail "new file $f is not one PLAN § 2 places" ;;
		esac
	done <"$work/new-files"
	need_files src/mp.hpp src/mp.cpp tests/vector_reader.hpp tests/vector_reader.cpp tests/check_mp.cpp \
		tests/vectors/generate.py tests/vectors/arith.txt tests/vectors/folding.txt tests/vectors/hex.txt \
		doc/reference-arithmetic.md
	[ "$PHASE" -ge 2 ] && need_files tests/vectors/products.txt
	[ "$PHASE" -ge 3 ] && need_files tests/vectors/modular.txt tests/vectors/decimal.txt
	[ "$PHASE" -ge 4 ] && need_files src/reference.hpp src/reference.cpp tests/check_reference.cpp \
		tests/vectors/reference.txt tests/vectors/mersenne-exponents.txt
	return 0
}

need_files() {
	for nf in "$@"; do
		test -f "$root/$nf" || fail "$nf is missing (PLAN § 2)"
	done
}

# wording: no spec ids in src/ or tests/; no process started from the checks or the program (R9)
wording() {
	if grep -rnE '(^|[^A-Za-z0-9_])(R[0-9]{1,2}|P[1-9])([^A-Za-z0-9_-]|$)' "$root/src" "$root"/tests/*.cpp "$root"/tests/*.hpp "$root/tests/cli.sh" "$root/tests/vectors/generate.py"; then
		fail "spec R#/P# ids in source (AGENTS.md § Conventions)"
	fi
	if grep -nE '\b(system|popen|execv[pe]?|execl[pe]?|posix_spawnp?|fork)[ \t]*\(' "$root"/src/*.cpp "$root"/src/*.hpp "$root"/tests/*.cpp "$root"/tests/*.hpp; then
		fail "src/ or tests/ starts a process: make check runs no tool beyond what it uses today (R9)"
	fi
}


# --- R9: bin/mps unchanged, no new library, no Python in make check ------------------------------

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
R9_RUNS='31
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

# same_as_base: for every run of R9_RUNS under NO_COLOR and FORCE_COLOR, the tree's bin/mps and
# c9b82ff's print the same bytes and exit alike, a detailed stamp's time aside (R9)
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
					|| fail "mps $sb_line [$DRIVE_ENV]: $sb_x differs from $BASE's (R9)"
			done
		done
	done <<EOF
$R9_RUNS
EOF
	SAME=$sb_n
}

# libraries: bin/mps links exactly the libraries c9b82ff's does (R9, § Invariants 5)
libraries() {
	otool -L "$BIN" | tail -n +2 | awk '{ print $1 }' | sort >"$work/libs-tree"
	otool -L "$BASE_BIN" | tail -n +2 | awk '{ print $1 }' | sort >"$work/libs-base"
	cmp -s "$work/libs-tree" "$work/libs-base" || { diff "$work/libs-base" "$work/libs-tree" >&2; fail "bin/mps links other libraries than $BASE's (R9)"; }
}

# no_python: make check, warm, with python, python3 and every python3.N first on PATH as stubs that
# record being called and fail: none is called, and make check stays green (R9)
no_python() {
	np="$work/shim"
	mkdir -p "$np" || fail "mkdir $np"
	for py in python python3 python3.8 python3.9 python3.10 python3.11 python3.12 python3.13 python3.14 python3.15 uv; do
		printf '#!/bin/sh\necho "%s $*" >>"%s/called"\nexit 97\n' "$py" "$work" >"$np/$py"
		chmod +x "$np/$py"
	done
	( cd "$work/tree-clang++" && PATH="$np:$PATH" "$MAKE" CXX=clang++ check ) >"$logs/no-python.log" 2>&1 \
		|| { tail -20 "$logs/no-python.log" >&2; fail "make check is red with Python stubbed out (R9)"; }
	test ! -s "$work/called" || fail "make check ran $(head -1 "$work/called") (R9)"
}


# --- R8 and the vectors ---------------------------------------------------------------------------

stems() {
	echo arith folding hex
	[ "$PHASE" -ge 2 ] && echo products
	[ "$PHASE" -ge 3 ] && echo modular decimal
	[ "$PHASE" -ge 4 ] && echo reference mersenne-exponents
	return 0
}

# reproduce: every family's command, as its header gives it, run from a copy of the tree's root by
# python3 and by python3.14 (in parallel), writes the committed file byte for byte (R8)
reproduce() {
	rp="$work/repro"
	for py in python3 python3.14; do
		copy_tree "$rp-$py"
		mkdir -p "$rp-$py/shim" && ln -s "$(command -v "$py")" "$rp-$py/shim/python3" || fail "shim for $py"
	done
	for stem in $(stems); do
		f="tests/vectors/$stem.txt"
		cmd=$(sed -n '2s/^# command: //p' "$root/$f")
		test "$cmd" = "python3 tests/vectors/generate.py $stem > tests/vectors/$stem.txt" \
			|| fail "$f: the second line is not '# command: python3 tests/vectors/generate.py $stem > tests/vectors/$stem.txt' (R8)"
		for py in python3 python3.14; do
			( cd "$rp-$py" && limit 600 env PATH="$rp-$py/shim:$PATH" sh -c "python3 --version >&2; $cmd" ) >"$logs/repro-$stem-$py.log" 2>&1 &
		done
	done
	wait
	for stem in $(stems); do
		grep -q '^Python 3\.14' "$logs/repro-$stem-python3.14.log" || fail "the reproduction of $stem under python3.14 ran another Python"
		for py in python3 python3.14; do
			cmp -s "$rp-$py/tests/vectors/$stem.txt" "$root/tests/vectors/$stem.txt" \
				|| { tail -5 "$logs/repro-$stem-$py.log" >&2; fail "tests/vectors/$stem.txt: its command under $py does not reproduce it byte for byte (R8)"; }
		done
	done
}

# vectors: vectors.py parses every family, recomputes every record with Python's integers, and
# checks the sizes and edges PLAN § 2 lists for this phase (R1-R4, R6-R8)
vectors() {
	threshold=$(sed -n 's/^static const std::size_t KARATSUBA_THRESHOLD = \([0-9][0-9]*\);.*/\1/p' "$root/src/mp.hpp")
	test -n "$threshold" || fail "src/mp.hpp has no 'static const std::size_t KARATSUBA_THRESHOLD = N;' (PLAN § 2)"
	test "$threshold" -ge 8 && test "$threshold" -le 256 || fail "KARATSUBA_THRESHOLD is $threshold; PLAN § 2 keeps it within 8 to 256"
	limit 900 python3 -I -B "$here/vectors.py" "$PHASE" "$root" "$threshold" >"$logs/vectors.log" 2>&1 \
		|| { tail -20 "$logs/vectors.log" >&2; fail "the vector files (vectors.py)"; }
	VECTORS=$(tail -1 "$logs/vectors.log")
}


# --- the probe ------------------------------------------------------------------------------------

# probe: verify/probe.cpp built against the tree's src/ with the checks' bounds assertions, answering
# probe.py's cases; every answer equals Python's (R1-R7)
probe() {
	srcs="$root/src/mp.cpp"
	[ "$PHASE" -ge 4 ] && srcs="$srcs $root/src/reference.cpp"
	# shellcheck disable=SC2086
	clang++ -O2 -std=c++20 -fno-fast-math -ffp-contract=off -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE \
		-DGATE_PHASE="$PHASE" -I "$root/src" "$here/probe.cpp" $srcs -o "$work/probe" >"$logs/probe-build.log" 2>&1 \
		|| { tail -20 "$logs/probe-build.log" >&2; fail "the probe does not build against src/: mp.hpp keeps the names and signatures PLAN § 2 gives"; }
	limit 600 python3 -I -B "$here/probe.py" "$PHASE" "$work/probe" >"$logs/probe.log" 2>&1 \
		|| { tail -20 "$logs/probe.log" >&2; fail "the tree's BigInt disagrees with Python (probe.py)"; }
	PROBE=$(tail -1 "$logs/probe.log")
}


# --- mutants --------------------------------------------------------------------------------------

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

# mutants_setup: a copy whose mp.cpp (and reference.cpp) have their public definitions renamed, with
# verify/mutant.cpp linked in their place, built once with clang++ at the Makefile's flags; the
# pass-through control run of bin/check is green and changes nothing
mutants_setup() {
	copy_tree "$MUT"
	wire_defs src/mp.cpp compare add subtract shift_left shift_right reduce_mersenne hex64 res64 to_hex from_hex
	[ "$PHASE" -ge 2 ] && wire_defs src/mp.cpp multiply_schoolbook multiply_karatsuba multiply square_schoolbook square_karatsuba square
	[ "$PHASE" -ge 3 ] && wire_defs src/mp.cpp divide reduce power_mod to_decimal from_decimal
	if [ "$PHASE" -ge 4 ]; then
		wire_defs src/reference.cpp lucas_lehmer prp3
		{ printf '#include "mp.hpp"\nnamespace reference {\n\tBigInt gate_real_lucas_lehmer(long unsigned p);\n\tBigInt gate_real_prp3(long unsigned p);\n}\n'; cat "$MUT/src/reference.cpp"; } >"$work/wired" \
			&& mv "$work/wired" "$MUT/src/reference.cpp" || fail "declare the wired reference tests"
	fi
	cp "$here/mutant.cpp" "$MUT/src/gate_mutant.cpp" || fail "copy mutant.cpp"
	echo "#define GATE_PHASE $PHASE" >"$MUT/src/gate_phase.h" || fail "write gate_phase.h"
	MODS=$(sed -n 's/^MODULES *:= *//p' "$MUT/Makefile")
	test -n "$MODS" || fail "no MODULES line in the Makefile"
	( cd "$MUT" && "$MAKE" CXX=clang++ MODULES="$MODS gate_mutant" bin/mps bin/check ) >"$logs/mut-build.log" 2>&1 \
		|| { tail -30 "$logs/mut-build.log" >&2; fail "the tree does not build through the gate's wrappers: mp.hpp and reference.hpp keep the names and signatures PLAN § 2 gives"; }
	: >"$work/applied-control"
	( cd "$MUT" && limit 300 env GATE_MUTANT_LOG="$work/applied-control" GATE_TRACE="$work/trace" bin/check ) >"$logs/mut-control.log" 2>&1 \
		|| { tail -30 "$logs/mut-control.log" >&2; fail "bin/check is red through the gate's pass-through wrappers"; }
	test ! -s "$work/applied-control" || fail "the control run applied a mutant"
	# Every vector record reaches the entry points PLAN § 2 names (R1-R4, R6, R7, R11)
	python3 -I -B "$here/vectors.py" coverage "$PHASE" "$root" "$threshold" "$work/trace" >"$logs/coverage.log" 2>&1 \
		|| { tail -5 "$logs/coverage.log" >&2; fail "make check skips vector records (vectors.py coverage)"; }
	COVERAGE=$(tail -1 "$logs/coverage.log")
	# The products' structure: schoolbook is schoolbook, Karatsuba recurses, the dispatchers choose by
	# the threshold (R1: "by schoolbook and by Karatsuba separately")
	if [ "$PHASE" -ge 2 ]; then
		trace_objs="$MUT/lib/mp.o $MUT/lib/gate_mutant.o"
		[ "$PHASE" -ge 4 ] && trace_objs="$trace_objs $MUT/lib/reference.o"
		# shellcheck disable=SC2086
		clang++ -std=c++20 -O2 -I "$MUT/src" "$here/trace.cpp" $trace_objs -o "$work/trace-driver" >"$logs/trace-build.log" 2>&1 \
			|| { tail -20 "$logs/trace-build.log" >&2; fail "the trace driver does not build against the wrapped tree"; }
		"$work/trace-driver" >"$logs/trace.log" 2>&1 || { cat "$logs/trace.log" >&2; fail "the products' structure is not PLAN § 2's (trace.cpp)"; }
	fi
	cp -R "$MUT/tests/vectors" "$work/vectors-orig" || fail "save the vectors"
}

# mutant ID LABEL WHAT: bin/check is red under GATE_MUTANT=ID, the mutant changed an answer, and the
# check labelled LABEL failed (a bug may also make a later check run away; the time limit ends it)
mutant() {
	ml="$work/applied-$1"
	: >"$ml"
	if ( cd "$MUT" && limit 120 env GATE_MUTANT="$1" GATE_MUTANT_LOG="$ml" bin/check ) >"$logs/mut-$1.log" 2>&1; then
		test -s "$ml" || fail "no check reaches the bug $1 stands for ($3): no answer changed"
		fail "make check stays green under $1 ($3)"
	fi
	test -s "$ml" || { tail -20 "$logs/mut-$1.log" >&2; fail "bin/check went red under $1 without it changing an answer ($3)"; }
	grep -qxF " FAIL $2" "$logs/mut-$1.log" \
		|| { grep '^ FAIL' "$logs/mut-$1.log" >&2; fail "under $1, the check '$2' passes ($3)"; }
	MUTANTS=$((MUTANTS + 1))
}

# tamper ID STEM LABEL HOW: with one vector file edited by vectors.py's tamper HOW (its trailer
# recomputed unless HOW is the trailer itself), bin/check is red and the check LABEL failed
tamper() {
	cp "$work/vectors-orig/$2.txt" "$MUT/tests/vectors/$2.txt" || fail "restore $2"
	python3 -I -B "$here/vectors.py" tamper "$4" "$MUT/tests/vectors/$2.txt" || fail "tamper $1: vectors.py cannot apply '$4' to $2.txt"
	( cd "$MUT" && limit 120 bin/check ) >"$logs/tamper-$1.log" 2>&1
	tm_status=$?
	cp "$work/vectors-orig/$2.txt" "$MUT/tests/vectors/$2.txt" || fail "restore $2"
	test "$tm_status" -ne 0 || fail "make check stays green with tests/vectors/$2.txt tampered ($4)"
	grep -qxF " FAIL $3" "$logs/tamper-$1.log" \
		|| { grep '^ FAIL' "$logs/tamper-$1.log" >&2; fail "with tests/vectors/$2.txt tampered ($4), the check '$3' passes"; }
	TAMPERS=$((TAMPERS + 1))
}

# missing STEM LABEL: with the file removed, bin/check is red and LABEL failed
missing() {
	/bin/rm -f "$MUT/tests/vectors/$1.txt"
	( cd "$MUT" && limit 120 bin/check ) >"$logs/missing-$1.log" 2>&1
	ms_status=$?
	cp "$work/vectors-orig/$1.txt" "$MUT/tests/vectors/$1.txt" || fail "restore $1"
	test "$ms_status" -ne 0 || fail "make check stays green with tests/vectors/$1.txt missing"
	grep -qxF " FAIL $2" "$logs/missing-$1.log" || fail "with tests/vectors/$1.txt missing, the check '$2' passes"
	TAMPERS=$((TAMPERS + 1))
}

C_SHA='SHA-256 matches the FIPS 180-4 examples'
C_HEAD='every vector file carries its command and source'
C_CMP='comparisons match the vectors'
C_ADD='sums match the vectors'
C_SUB='differences match the vectors'
C_SHIFT='shifts match the vectors'
C_FOLD='folding modulo M_p matches the vectors'
C_RES='Res64 matches the vectors'
C_HEX='hexadecimal matches the vectors and round-trips'
C_ERR='errors are reported, never values'
C_MULS='schoolbook products match the vectors'
C_MULK='Karatsuba products match the vectors'
C_MUL='products match the vectors'
C_SQRS='schoolbook squares match the vectors'
C_SQRK='Karatsuba squares match the vectors'
C_SQR='squares match the vectors'
C_MOD='general reduction matches the vectors'
C_POW='modular powers match the vectors'
C_DEC='decimal matches the vectors and round-trips'
C_LIST='the prime list carries its source'
C_LL0='Lucas-Lehmer ends in zero for every listed prime'
C_LL='Lucas-Lehmer residues match the vectors'
C_PRP1='PRP ends in one for every listed prime'
C_PRP='PRP residues match the vectors'
C_DOMAIN='the reference tests refuse exponents outside their theorems'

# The bugs each phase's checks must catch, one class each (PLAN § 6)
p1_mutants() {
	mutant cmp-top "$C_CMP" "a comparison that stops at the top limb"
	mutant add-carry-out "$C_ADD" "a sum that drops its final carry"
	mutant add-carry-in "$C_ADD" "a carry lost when a limb of all ones meets an incoming carry"
	mutant sub-borrow-in "$C_SUB" "a borrow lost when a limb of all ones meets an incoming borrow"
	mutant sub-short "$C_SUB" "a borrow that stops at the end of the shorter operand"
	mutant sub-below "$C_ERR" "a difference below zero returned, not refused"
	mutant shl-word "$C_SHIFT" "a left shift by a multiple of 64 shifting a limb by 64"
	mutant shr-word "$C_SHIFT" "a right shift by a multiple of 64 shifting a limb by 64"
	mutant shr-top "$C_SHIFT" "a right shift whose limb loop stops one short"
	mutant fold-once "$C_FOLD" "a fold that is never repeated"
	mutant fold-carry "$C_FOLD" "a fold that loses the carry out of its p-bit sum"
	mutant fold-mp "$C_FOLD" "a fold that leaves M_p for 0"
	mutant res64-raw "$C_RES" "Res64 of the unreduced value"
	mutant res64-mp "$C_RES" "Res64 at x = M_p"
	mutant hex64-unpadded "$C_RES" "Res64 printed without its leading zeros"
	mutant hex-lower "$C_HEX" "hexadecimal in lowercase"
	mutant hex-lead "$C_HEX" "hexadecimal with the top limb's leading zeros"
	mutant parse-prefix "$C_ERR" "a parser that stops at the first bad character and keeps the rest"
	mutant parse-empty "$C_ERR" "an empty string parsed as 0"
	mutant parse-tail "$C_ERR" "a parser that validates only its first 16 digits"
	# Every family is read (missing); the reader's envelope rules are one code path, shown on three
	for stem in $(stems); do
		missing "$stem" "$C_HEAD"
	done
	tamper arith-trailer arith "$C_HEAD" trailer
	tamper folding-command folding "$C_HEAD" command
	tamper hex-source hex "$C_HEAD" source
	tamper cmp arith "$C_CMP" value-cmp
	tamper add arith "$C_ADD" value-add
	tamper sub arith "$C_SUB" value-sub
	tamper shl arith "$C_SHIFT" value-shl
	tamper shr arith "$C_SHIFT" value-shr
	tamper fold folding "$C_FOLD" value-fold
	tamper res64 folding "$C_RES" value-res64
	tamper hex hex "$C_HEX" value-hex
}

p2_mutants() {
	mutant mul-row-carry "$C_MULS" "a schoolbook row that drops its last carry"
	mutant sqr-double "$C_SQRS" "squaring's doubling losing the bit out of each limb"
	mutant kara-odd "$C_MULK" "a Karatsuba split at odd sizes recombined at the wrong offset"
	mutant kara-sqr-odd "$C_SQRK" "a Karatsuba square split at odd sizes recombined at the wrong offset"
	mutant kara-sum-carry "$C_MULK" "a Karatsuba half-sum that drops its carry limb"
	mutant kara-sqr-sum-carry "$C_SQRK" "a Karatsuba square's half-sum that drops its carry limb"
	mutant kara-own-split "$C_MULK" "unequal operands each split at their own middle"
	tamper mul products "$C_MUL" value-mul
	tamper sqr products "$C_SQR" value-sqr
}

p3_mutants() {
	mutant div-add-back "$C_MOD" "Knuth's refined estimate without the add-back step"
	mutant div-one-correction "$C_MOD" "a one-limb estimate corrected only once"
	mutant mod-to-m "$C_MOD" "a remainder in [0, m], not [0, m)"
	mutant mod-zero "$C_ERR" "a zero modulus answered, not refused"
	mutant pow-one "$C_POW" "a power modulo 1 that starts at 1 and is never reduced"
	mutant pow-start "$C_POW" "a power that starts from the base, so exponent 0 gives the base"
	mutant pow-reversed "$C_POW" "a power reading the exponent's bits from the wrong end"
	mutant dec-pad "$C_DEC" "decimal groups printed without their leading zeros"
	mutant parse-prefix-dec "$C_ERR" "a decimal parser that stops at the first bad character"
	mutant parse-empty-dec "$C_ERR" "an empty string parsed as 0 in decimal"
	mutant parse-tail-dec "$C_ERR" "a decimal parser that validates only its first 19 digits"
	tamper mod modular "$C_MOD" value-mod
	tamper powmod modular "$C_POW" value-powmod
	tamper dec decimal "$C_DEC" value-dec
}

p4_mutants() {
	mutant ll-extra "$C_LL" "one Lucas-Lehmer iteration too many"
	mutant ll-extra "$C_LL0" "one Lucas-Lehmer iteration too many, on a prime"
	mutant ll-seed "$C_LL" "Lucas-Lehmer from s_0 = 10, which proves the primes alike"
	mutant prp-x "$C_PRP1" "x_p returned for the type-1 residue"
	mutant prp-base2 "$C_PRP" "base 2, which calls every M_p a probable prime"
	mutant domain "$C_DOMAIN" "p = 2 answered, not refused"
	tamper ll reference "$C_LL" value-ll
	tamper prp reference "$C_PRP" value-prp
	tamper list-composite mersenne-exponents "$C_LL0" add-composite
	tamper list-source mersenne-exponents "$C_LIST" drop-oeis
}

run_mutants() {
	mutants_setup
	p1_mutants
	[ "$PHASE" -ge 2 ] && p2_mutants
	[ "$PHASE" -ge 3 ] && p3_mutants
	[ "$PHASE" -ge 4 ] && p4_mutants
	return 0
}


# --- R12 and AGENTS.md ----------------------------------------------------------------------------

docs() {
	python3 -I -B "$here/docs.py" "$PHASE" "$root" >"$logs/docs.log" 2>&1 \
		|| { tail -20 "$logs/docs.log" >&2; fail "the note, the bibliography or AGENTS.md (docs.py)"; }
	DOCS=$(tail -1 "$logs/docs.log")
}


# --- R10 ------------------------------------------------------------------------------------------

# timing: warm `make check` (nothing to rebuild), the least of five interleaved runs (ten when the
# first five miss), for the tree and for c9b82ff on this machine, with each compiler; the tree's is at
# most BUDGET seconds longer (R10)
timing() {
	TIMES=
	for cxx in $COMPILERS; do
		bt="$work/base-$cxx"
		if [ ! -d "$bt" ]; then
			mkdir -p "$bt" && ( cd "$root" && git archive "$BASE" ) | ( cd "$bt" && tar -xf - ) || fail "cannot export $BASE"
		fi
		( cd "$bt" && "$MAKE" -B CXX="$cxx" check ) >"$logs/time-base-build-$cxx.log" 2>&1 || fail "$BASE: make check with $cxx"
		( cd "$work/tree-$cxx" && "$MAKE" CXX="$cxx" check ) >"$logs/time-tree-build-$cxx.log" 2>&1 || fail "tree: make check with $cxx"
		python3 -I - "$MAKE" "$cxx" "$bt" "$work/tree-$cxx" "$BUDGET" >"$logs/time-$cxx.log" 2>&1 <<'EOF' || { cat "$logs/time-$cxx.log" >&2; fail "$cxx: make check takes more than $BUDGET s longer than at $BASE (R10)"; }
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
print("%s: %.2f s at c9b82ff, %.2f s now, %+.2f s (budget %+.1f s; least of %d runs each; load %.1f)"
      % (cxx, mb, mt, mt - mb, budget, len(t), load))
sys.exit(0 if mt - mb <= budget else 1)
EOF
		TIMES="$TIMES$(cat "$logs/time-$cxx.log"); "
	done
}


# gate_all: everything a phase gate checks, cumulatively up to PHASE; GATE_LINE summarizes it
gate_all() {
	build_all
	footprint
	wording
	build_base
	same_as_base
	libraries
	no_python
	reproduce
	vectors
	probe
	run_mutants
	docs
	timing
	GATE_LINE="make check $CHECKS + $CLIS on clang++ and g++-16; $SAME runs as $BASE, same libraries, no Python; vectors reproduced by two Pythons; $VECTORS; $PROBE; $COVERAGE; $MUTANTS bugs and $TAMPERS tampered files caught; $DOCS; $TIMES"
}
