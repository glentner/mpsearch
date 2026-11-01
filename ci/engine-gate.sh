#!/bin/sh
# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: Apache-2.0
#
# Engine landing gate
#
# Usage: sh ci/engine-gate.sh [REV]
#        sh ci/engine-gate.sh compare LOG_A LOG_B
#
#   REV      the commit to test (default HEAD): anything git rev-parse names as a commit; the
#            result names its full hash
#   LOG_A    the output of one `make check-engine`, LOG_B that of another
#
# The landing gate of AGENTS.md § Factory: before a change to the FP64 engine's sources, its length
# table and measurement, or the numeric build flags lands, `make check-engine` must pass on the
# commit that lands under clang and under GCC, and the residue digests the two builds print must be
# equal. Run it from inside the repository; REV is a commit of the repository holding the current
# directory.
#
# Run mode tests the committed tree, never the working tree: it writes `git archive REV` into two
# fresh copies in a new directory under TMPDIR (/tmp when unset), one for each compiler, and runs
# `make CXX=<compiler> check-engine` in each, clang first, one at a time, each to its own log. A red
# clang run does not stop the GCC run, so a failure shows whether it is shared. Then it compares the
# two logs as compare mode does. Uncommitted changes in the working tree draw one warning line and
# are not tested. Each make builds everything from nothing, and `make check-engine` runs the engine
# tier at every supported length, so a run is two builds and two engine tiers in series: about 20
# minutes in all on an M2 Mac, each make's time printed as it ends. The two never run in parallel,
# which would skew both and share the machine with itself.
#
# Compare mode only compares two logs a run already wrote, in seconds; it is how this script's own
# checks are checked, on logs written for the purpose.
#
# Environment:
#
#   MPS_CLANG   the clang to test (default clang++); refused unless its predefined macros name
#               __clang__
#   MPS_GCC     the GCC to test; refused unless its predefined macros name __GNUC__ and neither
#               __clang__ nor __INTEL_LLVM_COMPILER. When unset, the first of g++-16, g++-15,
#               g++-14, g++-13, g++-12 and g++ on PATH that passes. A compiler is told by its
#               macros (`-dM -E`), never by its --version text: on macOS, g++ is Apple's clang.
#   MAKE        the make to run (default make); on macOS that is GNU make 3.81
#   TMPDIR      where the copies and the logs go
#
# MAKEFLAGS, MFLAGS, MAKELEVEL, GNUMAKEFLAGS, MAKEFILES, MAKEOVERRIDES, CXX, CXXFLAGS and CPPFLAGS
# are cleared before make runs, and CXX is given on make's command line. The copies are removed on
# exit; the logs are kept, and their directory is printed.
#
# A log passes only when it is a green `make check-engine`, and two logs pass only when their
# digests are equal. Equal digests alone are not a pass: a red check can still print its digest, and
# two builds that compute the same wrong residue print the same one. Each log must hold:
#
#   - the summaries SUMMARIES names, each with 0 failed and each exactly as many times as it says
#     (two `check:`, the default tier's and the engine tier's; one `cli:`; one `ll:`), and no
#     ` FAIL ` line;
#   - a digest line, `   residues <label>: <64 lower-case hex digits>`, for each label in LABELS,
#     each once. A line that begins with `residues` after any indent in any other form is
#     malformed, and a label not in LABELS is refused.
#
# Then each label's two digests are compared, in LABELS order, and each label whose digests differ
# is named with both. A change that adds a digest to the engine tier, or a step to
# `make check-engine`, edits LABELS or SUMMARIES in the same commit.
#
# Output: each line begins `engine-gate: `. When the gate is green it prints each label's digest,
# and its last line is
#
#   engine-gate: green on <full hash>: make check-engine under <clang> and <gcc>, 5 residue digests equal
#
# naming each compiler by the first line of its --version; in compare mode, `green on LOG_A and
# LOG_B: ...`. Otherwise every failure is printed, and the last line says the gate is red.
#
# Exit status: 0 green; 1 a red make, a log that is not a green make check-engine, a digest that
# differs, or an interrupted run; 2 bad arguments, an unreadable log, no repository or commit, or a
# compiler missing or not the one it must be.
#

LC_ALL=C
export LC_ALL
set -u
set -f

LABELS='squarings
products
partial chains
final chains
zero chains'

SUMMARIES='check 2
cli 1
ll 1'

NL='
'


say() { # TEXT: one line of output
	printf 'engine-gate: %s\n' "$1"
}


usage() {
	printf 'usage: sh ci/engine-gate.sh [REV]\n       sh ci/engine-gate.sh compare LOG_A LOG_B\n' >&2
	exit 2
}


refuse() { # MESSAGE: an argument or the environment is wrong; say so and stop
	say "$1" >&2
	exit 2
}


label_count() {
	printf '%s\n' "$LABELS" | grep -c .
}


check_log() { # LOG NAME: print what makes LOG not a green make check-engine; status 1 if anything
	#
	# Check one log
	#
	# The summaries, each green and each as often as SUMMARIES says; no failed check; every digest
	# line well-formed; and each label of LABELS printed exactly once, and no other.

	bad=0
	IFS=$NL
	for summary in $SUMMARIES; do
		word=${summary% *} want=${summary#* }
		all=$(grep -c -E "^$word: [0-9]+ passed, [0-9]+ failed\$" "$1")
		green=$(grep -c -E "^$word: [0-9]+ passed, 0 failed\$" "$1")
		if [ "$all" -ne "$want" ] || [ "$green" -ne "$want" ]; then
			say "$2 is not a green make check-engine: $green green of $all '$word:' summaries, want $want green"
			bad=1
		fi
	done
	unset IFS
	fails=$(grep -c '^ FAIL ' "$1")
	if [ "$fails" -ne 0 ]; then
		say "$2 is not a green make check-engine: $fails FAIL lines"
		bad=1
	fi

	malformed=$(grep -E '^[[:space:]]*residues' "$1" | grep -v -c -E '^   residues [a-z][a-z ]*: [0-9a-f]{64}$')
	if [ "$malformed" -ne 0 ]; then
		grep -E '^[[:space:]]*residues' "$1" | grep -v -E '^   residues [a-z][a-z ]*: [0-9a-f]{64}$' |
		while IFS= read -r line; do
			say "$2 has a malformed digest line: '$line'"
		done
		bad=1
	fi

	printed=$(sed -n -e 's/^   residues \([a-z][a-z ]*\): [0-9a-f]\{64\}$/\1/p' "$1")
	IFS=$NL
	for label in $printed; do
		known=0
		for want in $LABELS; do
			[ "$label" = "$want" ] && known=1
		done
		if [ "$known" -eq 0 ]; then
			say "$2 prints a digest for '$label', which is not in LABELS"
			bad=1
		fi
	done
	for label in $LABELS; do
		count=$(printf '%s\n' "$printed" | grep -c -F -x -e "$label")
		if [ "$count" -eq 0 ]; then
			say "$2 prints no digest for '$label'"
			bad=1
		elif [ "$count" -gt 1 ]; then
			say "$2 prints a digest for '$label' $count times"
			bad=1
		fi
	done
	unset IFS
	return "$bad"
}


digest_of() { # LOG LABEL: every digest LOG prints for LABEL, one a line
	sed -n -e "s/^   residues $2: \\([0-9a-f]\\{64\\}\\)\$/\\1/p" "$1"
}


compare_logs() { # LOG_A NAME_A LOG_B NAME_B: print every failure, or else the digests; status 1 on any failure
	#
	# Compare two logs
	#
	# Both logs checked, then each label's two digests compared. A digest missing or repeated in
	# either log is already named by the check, so only a label printed once in each is compared.

	failed=0
	check_log "$1" "$2" || failed=1
	check_log "$3" "$4" || failed=1
	IFS=$NL
	for label in $LABELS; do
		a=$(digest_of "$1" "$label")
		b=$(digest_of "$3" "$label")
		case $a$b in
			*"$NL"*) continue ;;
		esac
		[ -n "$a" ] && [ -n "$b" ] || continue
		if [ "$a" != "$b" ]; then
			say "residues $label differ: $2 $a, $4 $b"
			failed=1
		fi
	done
	unset IFS
	[ "$failed" -eq 0 ] || return 1
	IFS=$NL
	for label in $LABELS; do
		say "residues $label: $(digest_of "$1" "$label")"
	done
	unset IFS
	return 0
}


is_clang() { # CXX: its predefined macros name __clang__
	"$1" -dM -E -x c++ /dev/null 2>/dev/null | grep -q '^#define __clang__ '
}


is_gcc() { # CXX: its predefined macros name __GNUC__, and neither __clang__ nor __INTEL_LLVM_COMPILER
	macros=$("$1" -dM -E -x c++ /dev/null 2>/dev/null) || return 1
	printf '%s\n' "$macros" | grep -q '^#define __GNUC__ ' || return 1
	! printf '%s\n' "$macros" | grep -q -E '^#define (__clang__|__INTEL_LLVM_COMPILER) '
}


run_make() { # NAME CXX: make check-engine in the copy NAME, its output in NAME.log; status that of make
	say "make CXX=$2 check-engine in a fresh copy (log $work/$1.log)"
	started=$(date +%s)
	(cd "$work/$1" && "$make" CXX="$2" check-engine) >"$work/$1.log" 2>&1 </dev/null
	status=$?
	seconds=$(($(date +%s) - started))
	if [ "$status" -eq 0 ]; then
		say "$2: make check-engine passed in $seconds s"
	else
		say "$2: make check-engine failed (status $status) in $seconds s"
	fi
	return "$status"
}


# Compare mode
if [ $# -ge 1 ] && [ "$1" = compare ]; then
	[ $# -eq 3 ] || usage
	for log in "$2" "$3"; do
		[ -f "$log" ] && [ -r "$log" ] || refuse "cannot read the log $log"
	done
	if compare_logs "$2" "$2" "$3" "$3"; then
		say "green on $2 and $3: two green make check-engine logs, $(label_count) residue digests equal"
		exit 0
	fi
	say "red: $2 and $3 do not pass"
	exit 1
fi


# Run mode
case $# in
	0) REV=HEAD ;;
	1) case $1 in -*) usage ;; esac; REV=$1 ;;
	*) usage ;;
esac

root=$(git rev-parse --show-toplevel 2>/dev/null) || refuse "not inside a git repository"
rev=$(git -C "$root" rev-parse --verify --quiet "$REV^{commit}") || refuse "$REV is not a commit"

clang=${MPS_CLANG:-clang++}
command -v "$clang" >/dev/null 2>&1 || refuse "no clang: $clang is not on PATH; set MPS_CLANG"
is_clang "$clang" || refuse "$clang is not clang (its macros do not name __clang__); set MPS_CLANG"

if [ -n "${MPS_GCC:-}" ]; then
	gcc=$MPS_GCC
	command -v "$gcc" >/dev/null 2>&1 || refuse "no GCC: $gcc is not on PATH; set MPS_GCC"
	is_gcc "$gcc" || refuse "$gcc is not GCC (its macros name __clang__ or lack __GNUC__); set MPS_GCC"
else
	gcc=
	for candidate in g++-16 g++-15 g++-14 g++-13 g++-12 g++; do
		command -v "$candidate" >/dev/null 2>&1 || continue
		if is_gcc "$candidate"; then
			gcc=$candidate
			break
		fi
	done
	[ -n "$gcc" ] || refuse "no GCC found (g++ is clang or missing); set MPS_GCC"
fi

clang_version=$("$clang" --version 2>/dev/null | sed -n 1p)
gcc_version=$("$gcc" --version 2>/dev/null | sed -n 1p)
make=${MAKE:-make}
command -v "$make" >/dev/null 2>&1 || refuse "no make: $make is not on PATH; set MAKE"

unset MAKEFLAGS MFLAGS MAKELEVEL GNUMAKEFLAGS MAKEFILES MAKEOVERRIDES CXX CXXFLAGS CPPFLAGS

say "testing $rev ($(git -C "$root" log -1 --format=%s "$rev"))"
if [ -n "$(git -C "$root" status --porcelain 2>/dev/null)" ]; then
	say "testing $REV as committed; the working tree has uncommitted changes, which are not tested"
fi
say "clang: $clang, $clang_version"
say "gcc: $gcc, $gcc_version"

tmp=${TMPDIR:-/tmp}
work=$(mktemp -d "${tmp%/}/mps-engine-gate.XXXXXX") || refuse "cannot make a directory under $tmp"

cleanup() { # the copies and the archive; the logs stay
	/bin/rm -rf "$work/clang" "$work/gcc" "$work/tree.tar"
}
trap cleanup EXIT
trap 'say "interrupted; logs in $work" >&2; exit 1' INT TERM HUP

git -C "$root" archive --format=tar -o "$work/tree.tar" "$rev" || refuse "git archive $rev failed"
for name in clang gcc; do
	mkdir "$work/$name" && tar -xf "$work/tree.tar" -C "$work/$name" || refuse "cannot unpack $rev into $work/$name"
done

red=0
run_make clang "$clang" || red=1
run_make gcc "$gcc" || red=1

say "logs in $work"
if compare_logs "$work/clang.log" "${clang##*/}" "$work/gcc.log" "${gcc##*/}" && [ "$red" -eq 0 ]; then
	say "green on $rev: make check-engine under $clang_version and $gcc_version, $(label_count) residue digests equal"
	exit 0
fi
say "red on $rev"
exit 1
