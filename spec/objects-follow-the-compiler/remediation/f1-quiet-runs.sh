#!/bin/sh
# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
#
# F1's class checker (REVIEW.md, R3): no make run that builds nothing changes or deletes a build
# output, and dry and question runs still tell the truth. A cycle record, not a standing gate.
#
#   sh spec/objects-follow-the-compiler/remediation/f1-quiet-runs.sh
#
# The tree is copied as a clone would hold it and built once with clang++ (the default), every
# output dated 2 s ahead so nothing can lean on the clock. Then each case runs in its own copy of
# that build, with settings other than the build's:
#   dry       make -n CXX=g++-16 check            must print g++-16's compiles and links
#   question  make -q CXXFLAGS=-O0                 must exit 1 (out of date)
#   database  make -npq CXX=g++-16 .DEFAULT        what shell completion and editors run
#   typo      make CXX=g++-16 chek                 an unknown goal
#   probe     ci/test-commit.sh's compiler probe   must print exactly g++
#   elsewhere make -f <copy>/Makefile [check]      from another directory holding lib/ and bin/
#             CXX=g++-16
# and, with the build's own settings, make -n check must print no compile or link and make -q
# bin/mps bin/check must exit 0. After each case every file under lib/ and bin/ must keep its
# inode, size, nanosecond mtime and bytes (and so must the other directory's), no compiler may have
# run, and a plain make check must then run no compile or link and pass.
#
# GNU make 3.81 at /usr/bin/make unless F1_MAKE names another. Every make runs in a process group of
# its own, killed when the checker ends; the checker then removes its own mktemp directory, after
# checking the path, and deletes nothing else. Prints `F1 FAIL: <reason>` per failure and exits 1,
# or one `f1-quiet-runs: green (...)` line.

set -u
umask 022
unset MAKEFLAGS MAKELEVEL MFLAGS MAKEFILES GNUMAKEFLAGS MAKEOVERRIDES CXX CXXFLAGS CPPFLAGS IEEE BOUNDS \
	GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE F1_CALLS

root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "F1 FAIL: not in a git tree" >&2; exit 1; }
MAKE=${F1_MAKE:-/usr/bin/make}
FAILS=0

die() {
	echo "F1 FAIL: $*" >&2
	exit 1
}
fail() {
	echo "F1 FAIL: $*" >&2
	FAILS=$((FAILS + 1))
}
for t in clang++ g++-16 git rsync perl python3 cmp; do
	command -v "$t" >/dev/null 2>&1 || die "required tool not found: $t"
done
MAKE_VERSION=$("$MAKE" --version 2>/dev/null | head -n 1)
case $MAKE_VERSION in "GNU Make "*) ;; *) die "$MAKE is not GNU make" ;; esac

work=$(mktemp -d "${TMPDIR:-/tmp}/mps-f1.XXXXXX") || die "mktemp failed"
mkdir -p "$work/pg" "$work/shim" "$work/man" "$work/log" || die "mkdir under $work"
export F1_WORK="$work"

# cleanup: every process group the checker started, then its own mktemp directory (Python's
# shutil, after checking the path is that directory and nothing else)
cleanup() {
	trap '' INT TERM HUP
	for cl_sig in TERM KILL; do
		for cl_f in "$work"/pg/*; do
			[ -f "$cl_f" ] && kill -s "$cl_sig" -- "-${cl_f##*/}" 2>/dev/null
		done
		perl -e 'select(undef, undef, undef, 0.3)'
	done
	chmod -R u+rwx "$work" 2>/dev/null
	python3 -I - "$work" "${TMPDIR:-/tmp}" <<'PY'
import os, shutil, sys
w, t = os.path.realpath(sys.argv[1]), os.path.realpath(sys.argv[2])
if os.path.dirname(w) != t or not os.path.basename(w).startswith("mps-f1."):
    sys.exit("f1-quiet-runs: refusing to delete %s, which is not this checker's own temporary directory" % w)
shutil.rmtree(w, ignore_errors=True)
PY
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

# limit SECONDS CMD...: CMD in a process group of its own, recorded in $work/pg while it runs and
# killed with its group after SECONDS or when the checker ends; exits with CMD's status
limit() {
	li_t=$1
	shift
	perl -e '
		my ($t, $pg) = (shift, shift);
		$pg eq "$ENV{F1_WORK}/pg" or die "limit: $pg is not this checker'"'"'s own record directory\n";
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

# Compiler shims first on PATH: each call is logged to $F1_CALLS, then the real compiler runs
for sh_c in clang++ g++-16; do
	sh_real=$(command -v "$sh_c")
	cat >"$work/shim/$sh_c" <<EOF
#!/bin/sh
{ printf '%s' "\${0##*/}"; for a in "\$@"; do printf ' %s' "\$a"; done; printf '\\n'; } >>"\${F1_CALLS:-/dev/null}"
exec "$sh_real" "\$@"
EOF
	chmod +x "$work/shim/$sh_c" || die "shim for $sh_c"
done

# mk NAME DIR MAKEARGS...: make in DIR, shims first on PATH; stdout, stderr and compiler calls in
# $work/log/NAME.{out,err,calls}; returns make's status
mk() {
	mk_n=$1 mk_d=$2
	shift 2
	: >"$work/log/$mk_n.calls"
	( cd "$mk_d" && F1_CALLS="$work/log/$mk_n.calls" && PATH="$work/shim:$PATH" && export F1_CALLS PATH \
		&& limit 900 "$MAKE" "$@" ) >"$work/log/$mk_n.out" 2>"$work/log/$mk_n.err"
}

# man DIR NAME: every file under DIR's lib/ and bin/ with its inode, size, mtime (ns) and SHA-256
man() {
	python3 -I - "$1" "$work/man/$2" <<'PY'
import hashlib, os, sys
root, out = sys.argv[1], sys.argv[2]
rows = []
for top in ("lib", "bin"):
    for dirpath, dirnames, filenames in os.walk(os.path.join(root, top)):
        dirnames.sort()
        for name in sorted(filenames):
            p = os.path.join(dirpath, name)
            st = os.lstat(p)
            with open(p, "rb") as f:
                h = hashlib.sha256(f.read()).hexdigest()
            rows.append("%s %d %d %d %s" % (os.path.relpath(p, root), st.st_ino, st.st_size, st.st_mtime_ns, h))
with open(out, "w") as f:
    f.write("\n".join(rows) + "\n")
PY
}

# same NAME BEFORE AFTER WHAT: the two manifests are identical and not empty
same() {
	test "$(wc -l <"$work/man/$2" | tr -d ' ')" -gt 1 || die "$1: nothing under lib/ and bin/ before the run: the check proves nothing"
	cmp -s "$work/man/$2" "$work/man/$3" && return 0
	sa_n=$(diff "$work/man/$2" "$work/man/$3" | grep -c '^[<>]')
	fail "$4 changed lib/ or bin/ ($sa_n manifest lines differ; first: $(diff "$work/man/$2" "$work/man/$3" | grep '^[<>]' | head -n 1 | cut -c 1-100))"
}

# no_calls NAME WHAT: the run called no compiler
no_calls() {
	test ! -s "$work/log/$1.calls" || fail "$2 called the compiler: $(head -n 1 "$work/log/$1.calls" | cut -c 1-100)"
}

# --- The build: a clone's files, built once with the defaults, outputs dated 2 s ahead ------------

built="$work/built"
mkdir -p "$built" || die "mkdir $built"
( cd "$root" && git ls-files -z --cached --others --exclude-standard | perl -0 -ne 'chomp; print "$_\0" if -e $_' ) \
	| rsync -a --from0 --files-from=- "$root/" "$built/" || die "cannot copy the tree"
test ! -e "$built/lib" && test ! -e "$built/bin" || die "the copy holds build output"
mk build "$built" bin/mps bin/check || { tail -n 20 "$work/log/build.err" >&2; die "make bin/mps bin/check failed"; }
grep -q '^clang++ .* -o bin/mps$' "$work/log/build.calls" && grep -q '^clang++ .* -o bin/check$' "$work/log/build.calls" \
	|| die "the build linked no bin/mps and bin/check with clang++"
python3 -I - "$built" <<'PY' || die "cannot date the outputs"
import os, sys, time
t = time.time() + 2
for top in ("lib", "bin"):
    for dirpath, _, filenames in os.walk(os.path.join(sys.argv[1], top)):
        for name in filenames:
            os.utime(os.path.join(dirpath, name), (t, t))
PY

# case NAME: a copy of the build for one case, its manifest as NAME.before
case_copy() {
	cp -Rp "$built" "$work/$1" || die "cannot copy the build for $1"
	man "$work/$1" "$1.before" || die "manifest before $1"
}

# settle NAME WHAT: after the case, lib/ and bin/ as before, then a plain make check runs no
# compile or link and passes
settle() {
	man "$work/$1" "$1.after" || die "manifest after $1"
	same "$1" "$1.before" "$1.after" "$2"
	mk "$1.next" "$work/$1" check
	se_rc=$?
	test "$se_rc" = 0 || fail "$2: the next plain make check exits $se_rc"
	no_calls "$1.next" "$2: the next plain make check"
	! grep -q '^clang++ ' "$work/log/$1.next.out" || fail "$2: the next plain make check printed a compile or link"
	grep -q '^check: [0-9]* passed, 0 failed$' "$work/log/$1.next.out" || fail "$2: the next plain make check printed no passing summary"
}

# --- The cases ------------------------------------------------------------------------------------

# A dry run with another compiler: prints the rebuild, deletes nothing
case_copy dry
mk dry "$work/dry" -n CXX=g++-16 check
dr_rc=$?
test "$dr_rc" = 0 || fail "make -n CXX=g++-16 check exits $dr_rc"
no_calls dry "make -n CXX=g++-16 check"
grep -q '^g++-16 .* -c src/mp\.cpp -o lib/check/mp\.o$' "$work/log/dry.out" \
	&& grep -q '^g++-16 .* -o bin/check$' "$work/log/dry.out" && grep -q '^g++-16 .* -o bin/mps$' "$work/log/dry.out" \
	|| fail "make -n CXX=g++-16 check does not print g++-16's compiles and links: the dry run hides the rebuild"
settle dry "make -n CXX=g++-16 check"

# A question run with other flags: out of date, deletes nothing
case_copy question
mk question "$work/question" -q CXXFLAGS=-O0
qu_rc=$?
test "$qu_rc" = 1 || fail "make -q CXXFLAGS=-O0 exits $qu_rc, not 1 (out of date)"
no_calls question "make -q CXXFLAGS=-O0"
settle question "make -q CXXFLAGS=-O0"

# The database dump shell completion and editors read
case_copy database
mk database "$work/database" -npq CXX=g++-16 .DEFAULT
no_calls database "make -npq CXX=g++-16 .DEFAULT"
grep -q '^# GNU Make' "$work/log/database.out" || fail "make -npq CXX=g++-16 .DEFAULT printed no database"
settle database "make -npq CXX=g++-16 .DEFAULT"

# A mistyped goal
case_copy typo
mk typo "$work/typo" CXX=g++-16 chek
ty_rc=$?
test "$ty_rc" != 0 || fail "make CXX=g++-16 chek exits 0"
no_calls typo "make CXX=g++-16 chek"
settle typo "make CXX=g++-16 chek"

# ci/test-commit.sh's compiler probe, verbatim, in a tree another compiler built
case_copy probe
( cd "$work/probe" && printf 'mps-print-cxx:\n\t@echo $(CXX)\n' | limit 120 "$MAKE" -s -f Makefile -f - mps-print-cxx CXX=g++ ) \
	>"$work/log/probe.out" 2>"$work/log/probe.err"
pr_rc=$?
test "$pr_rc" = 0 && test "$(cat "$work/log/probe.out")" = g++ && test ! -s "$work/log/probe.err" \
	|| fail "the CI probe exits $pr_rc and prints '$(head -c 60 "$work/log/probe.out")' (stderr '$(head -c 60 "$work/log/probe.err")'), not exactly g++"
settle probe "the CI probe"

# make -f <copy>/Makefile from another directory that holds lib/ and bin/ files of its own
case_copy elsewhere
other="$work/other"
mkdir -p "$other/lib/check" "$other/bin" || die "mkdir $other"
for el_f in lib/main.o lib/main.d lib/ansi.o lib/check/check.o lib/settings bin/mps bin/check; do
	printf 'not this repository: %s\n' "$el_f" >"$other/$el_f" || die "cannot plant $other/$el_f"
done
man "$other" other.before || die "manifest of $other"
mk elsewhere-make "$other" -f "$work/elsewhere/Makefile" CXX=g++-16
mk elsewhere-check "$other" -f "$work/elsewhere/Makefile" CXX=g++-16 check
no_calls elsewhere-make "make -f <copy>/Makefile CXX=g++-16 from another directory"
no_calls elsewhere-check "make -f <copy>/Makefile CXX=g++-16 check from another directory"
man "$other" other.after || die "manifest of $other"
same elsewhere-other other.before other.after "make -f <copy>/Makefile CXX=g++-16 [check] from another directory (in that directory)"
settle elsewhere "make -f <copy>/Makefile CXX=g++-16 [check] from another directory"

# With the build's own settings, the dry and question runs say there is nothing to do
case_copy current
mk current-dry "$work/current" -n check
cu_rc=$?
test "$cu_rc" = 0 || fail "make -n check with the build's settings exits $cu_rc"
! grep -Eq '^(clang\+\+|g\+\+-16) ' "$work/log/current-dry.out" \
	|| fail "make -n check with the build's settings prints a compile or link: the dry run invents a rebuild"
mk current-question "$work/current" -q bin/mps bin/check
cu_rc=$?
test "$cu_rc" = 0 || fail "make -q bin/mps bin/check with the build's settings exits $cu_rc, not 0 (up to date)"
no_calls current-dry "make -n check with the build's settings"
no_calls current-question "make -q bin/mps bin/check with the build's settings"
settle current "make -n check and make -q with the build's settings"

if [ "$FAILS" -gt 0 ]; then
	echo "f1-quiet-runs: $FAILS checks failed ($MAKE_VERSION)" >&2
	exit 1
fi
echo "f1-quiet-runs: green ($MAKE_VERSION; with other settings than the build's, make -n check, make -q, make -npq .DEFAULT, a mistyped goal, the CI probe and make -f from another directory left lib/ and bin/ (and the other directory) byte-, inode- and mtime-identical and called no compiler; -n printed g++-16's rebuild and -q exited 1; with the build's settings -n printed no compile and -q exited 0; each next plain make check compiled and linked nothing and passed)"
