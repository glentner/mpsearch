# Shared helpers for this cycle's phase gates (a cycle record, not a standing gate).
# Sourced by p1.sh, p2.sh, p3.sh. POSIX sh.

# The tree under test is the one holding this script, whatever the caller's directory.
root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "gate: not in a git tree" >&2; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/mps-gate.XXXXXX") || { echo "gate: mktemp failed" >&2; exit 1; }
logs="$work/logs"
mkdir -p "$logs" || { echo "gate: mkdir $logs" >&2; exit 1; }
trap '/bin/rm -rf "$work"' EXIT INT TERM

fail() {
	echo "GATE FAIL: $*" >&2
	exit 1
}

# GNU make 3.81 is what macOS ships, and R4 holds the build to it.
MAKE=/usr/bin/make
"$MAKE" --version 2>/dev/null | head -1 | grep -q '^GNU Make 3\.81' || fail "need GNU Make 3.81 at $MAKE (R4)"

# copy_tree DEST: the files git knows about (tracked, plus untracked and unignored), as a clone would
# hold them once committed. Ignored build output never comes along.
copy_tree() {
	mkdir -p "$1" || fail "mkdir $1"
	( cd "$root" && git ls-files -z --cached --others --exclude-standard ) \
		| rsync -a --from0 --files-from=- "$root/" "$1/" || fail "copy of the tree into $1"
	test ! -e "$1/lib" || fail "copy of the tree already has lib/"
	test ! -e "$1/bin" || fail "copy of the tree already has bin/"
}

# copy_base DEST: origin/main exactly as committed
copy_base() {
	mkdir -p "$1" || fail "mkdir $1"
	( cd "$root" && git archive -o "$work/base.tar" origin/main ) || fail "git archive origin/main"
	( cd "$1" && tar -xf "$work/base.tar" ) || fail "extract origin/main"
	test -f "$1/Makefile" || fail "origin/main extract has no Makefile"
}

need() {
	command -v "$1" >/dev/null 2>&1 || fail "required tool not found: $1"
}

# compiled_with CXX LOG: every compile and link line in LOG starts with CXX (CXX is honoured, R1)
compiled_with() {
	other=$(grep -E -- ' -c | -o bin/' "$2" | grep -v -- "^$1 " || true)
	test -z "$other" || { echo "$other" >&2; fail "$1: build lines not run by $1 (CXX ignored, R1)"; }
	# The compiler name is matched literally: '+' in clang++ and g++-16 is an operator in -E patterns.
	grep -- " -o bin/" "$2" | grep -q -- "^$1 " || fail "$1: no link line run by $1 in the build log"
}

# ieee_on_every_compile CXX LOG MIN: each compile line carries the IEEE flags (R5)
ieee_on_every_compile() {
	ncomp=$(grep -c -- ' -c ' "$2")
	nieee=$(grep -- ' -c ' "$2" | grep -c -- '-fno-fast-math -ffp-contract=off')
	test "$ncomp" -ge "$3" || fail "$1: expected at least $3 compile lines, saw $ncomp"
	test "$ncomp" -eq "$nieee" || fail "$1: $nieee of $ncomp compile lines carry the IEEE flags (R5)"
}

# ieee_after_fast_math CXX LOG: with CXXFLAGS=-ffast-math, the IEEE flags still come after it (R5)
ieee_after_fast_math() {
	bad=$(grep -- ' -c ' "$2" | awk '{ f = index($0, "-ffast-math"); n = index($0, "-fno-fast-math"); c = index($0, "-ffp-contract=off"); if (!(f > 0 && n > f && c > f)) print }')
	test -z "$bad" || { echo "$bad" >&2; fail "$1: IEEE flags not after a command-line -ffast-math (R5)"; }
}
