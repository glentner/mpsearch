#!/bin/sh
# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
#
# F1's class checker, cycle 2 (REVIEW.md, R3): every output the Makefile builds keeps
# .DELETE_ON_ERROR and make's interrupt cleanup when the settings change, so a link or compile that
# fails or is interrupted partway leaves no partial file that the next run calls up to date. A cycle
# record, not a standing gate.
#
#   sh spec/objects-follow-the-compiler/remediation/f1-partial-link.sh
#
# The tree is copied as a clone would hold it and built once with clang++ (the default), every
# output dated 2 s back, a build finished a moment ago. A constructed compiler, passed as CXX (so
# the settings differ from the build's), compiles and links with clang++, except that the output
# FAKE_TARGET names is written in place and then the call fails (FAKE_MODE=fail) or waits to be
# interrupted (FAKE_MODE=slow), as a linker that writes its output in place can. In its own copy of
# the build, each case switches to that compiler:
#   mps-fail      make CXX=<fake> bin/mps,   bin/mps's link fails partway
#   mps-int       make CXX=<fake> bin/mps,   SIGINT to make's process group during bin/mps's link
#   check-fail    make CXX=<fake> bin/check, bin/check's link fails partway
#   check-int     make CXX=<fake> bin/check, SIGINT during bin/check's link
#   object-fail   make CXX=<fake> bin/mps,   lib/mp.o's compile fails partway
#   object-int    make CXX=<fake> bin/check, SIGINT during lib/check/mp.o's compile
# After each, the half-written output must be gone, and the next run with the same settings (the
# constructed compiler, no fault) must remake it and leave a working binary. Before the cases, the
# database make prints with other settings must declare .DELETE_ON_ERROR and no output phony (only
# check, clean and FORCE), as it does with the build's own settings.
#
# GNU make 3.81 at /usr/bin/make unless F1_MAKE names another. Every make runs in a process group of
# its own, killed when the checker ends; the checker then removes its own mktemp directory, after
# checking the path, and deletes nothing else. Prints `F1 FAIL: <reason>` per failure and exits 1,
# or one `f1-partial-link: green (...)` line.

set -u
umask 022
unset MAKEFLAGS MAKELEVEL MFLAGS MAKEFILES GNUMAKEFLAGS MAKEOVERRIDES CXX CXXFLAGS CPPFLAGS IEEE BOUNDS \
	GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE F1_CALLS FAKE_MODE FAKE_TARGET

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
for t in clang++ git rsync perl python3; do
	command -v "$t" >/dev/null 2>&1 || die "required tool not found: $t"
done
MAKE_VERSION=$("$MAKE" --version 2>/dev/null | head -n 1)
case $MAKE_VERSION in "GNU Make "*) ;; *) die "$MAKE is not GNU make" ;; esac

work=$(mktemp -d "${TMPDIR:-/tmp}/mps-f1.XXXXXX") || die "mktemp failed"
mkdir -p "$work/pg" "$work/fake" "$work/log" || die "mkdir under $work"
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
    sys.exit("f1-partial-link: refusing to delete %s, which is not this checker's own temporary directory" % w)
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

# The constructed compiler: logs each call to $F1_CALLS, then either writes the output FAKE_TARGET
# names in place and fails or waits, or runs clang++
fake="$work/fake/cxx"
real_cxx=$(command -v clang++)
cat >"$fake" <<EOF
#!/bin/sh
{ printf 'cxx'; for a in "\$@"; do printf ' %s' "\$a"; done; printf '\\n'; } >>"\${F1_CALLS:-/dev/null}"
out= prev=
for a in "\$@"; do [ "\$prev" = -o ] && out=\$a; prev=\$a; done
if [ -n "\${FAKE_MODE:-}" ] && [ -n "\$out" ] && [ "\$out" = "\${FAKE_TARGET:-}" ]; then
	printf 'PARTIAL' >"\$out"
	chmod +x "\$out"
	[ "\$FAKE_MODE" = slow ] && exec /bin/sleep 120
	exit 1
fi
exec "$real_cxx" "\$@"
EOF
chmod +x "$fake" || die "cannot make the constructed compiler"

# mk NAME DIR MAKEARGS...: make in DIR, with FAKE_MODE and FAKE_TARGET as the caller set FM and FT;
# stdout, stderr and compiler calls in $work/log/NAME.{out,err,calls}; returns make's status
FM= FT=
mk() {
	mk_n=$1 mk_d=$2
	shift 2
	: >"$work/log/$mk_n.calls"
	( cd "$mk_d" && F1_CALLS="$work/log/$mk_n.calls" && FAKE_MODE=$FM && FAKE_TARGET=$FT \
		&& export F1_CALLS FAKE_MODE FAKE_TARGET && limit 900 "$MAKE" "$@" ) >"$work/log/$mk_n.out" 2>"$work/log/$mk_n.err"
}

# --- The build: a clone's files, built once with the defaults, outputs dated 2 s back -------------

built="$work/built"
mkdir -p "$built" || die "mkdir $built"
( cd "$root" && git ls-files -z --cached --others --exclude-standard | perl -0 -ne 'chomp; print "$_\0" if -e $_' ) \
	| rsync -a --from0 --files-from=- "$root/" "$built/" || die "cannot copy the tree"
test ! -e "$built/lib" && test ! -e "$built/bin" || die "the copy holds build output"
mk build "$built" bin/mps bin/check || { tail -n 20 "$work/log/build.err" >&2; die "make bin/mps bin/check failed"; }
test -x "$built/bin/mps" && test -x "$built/bin/check" || die "the build made no bin/mps and bin/check"
python3 -I - "$built" <<'PY' || die "cannot date the outputs"
import os, sys, time
t = time.time() - 2
for top in ("lib", "bin"):
    for dirpath, _, filenames in os.walk(os.path.join(sys.argv[1], top)):
        for name in filenames:
            os.utime(os.path.join(dirpath, name), (t, t))
PY

# --- The database: .DELETE_ON_ERROR, and nothing phony but check, clean and FORCE -----------------

# phony NAME MAKEARGS...: the sorted .PHONY prerequisites of the database make prints (-q, so no
# recipe runs) in a copy of the build
phony() {
	ph_n=$1
	shift
	cp -Rp "$built" "$work/$ph_n" || die "cannot copy the build for $ph_n"
	mk "$ph_n" "$work/$ph_n" -pq "$@" bin/mps
	grep -q '^# GNU Make' "$work/log/$ph_n.out" || die "make -pq $* printed no database"
	grep -q '^\.DELETE_ON_ERROR:' "$work/log/$ph_n.out" \
		|| fail "make -pq $* bin/mps: the database declares no .DELETE_ON_ERROR"
	sed -n 's/^\.PHONY:[[:space:]]*//p' "$work/log/$ph_n.out" | tr ' ' '\n' | grep -v '^$' | sort -u | tr '\n' ' '
}
ph_same=$(phony db-same)
ph_other=$(phony db-other CXX="$fake")
test "$ph_same" = "check clean FORCE " || fail "with the build's settings, .PHONY is '$ph_same', not 'check clean FORCE'"
test "$ph_other" = "check clean FORCE " \
	|| fail "with other settings, .PHONY is '$ph_other', not 'check clean FORCE': a phony output loses .DELETE_ON_ERROR and make's interrupt cleanup"

# --- The cases ------------------------------------------------------------------------------------

# after NAME TARGET GOAL WHAT: the partial TARGET is gone, and the next run with the same settings
# and no fault remakes it, with a working bin/mps or bin/check at the end
after() {
	af_d="$work/$1"
	if [ -e "$af_d/$2" ]; then
		fail "$4 left $2 ($(head -c 20 "$af_d/$2" | tr -c '[:print:]' '.'), $(wc -c <"$af_d/$2" | tr -d ' ') bytes)"
	fi
	FM= FT=
	mk "$1.next" "$af_d" CXX="$fake" "$3"
	af_rc=$?
	test "$af_rc" = 0 || { fail "$4: the next make CXX=<fake> $3 exits $af_rc"; return; }
	grep -q "^cxx .* -o $2\$" "$work/log/$1.next.calls" \
		|| fail "$4: the next make CXX=<fake> $3 did not remake $2 ($(head -n 1 "$work/log/$1.next.out" | cut -c 1-80))"
	case $3 in
		bin/mps) "$af_d/bin/mps" --version >/dev/null 2>&1 || fail "$4: after the next make, bin/mps --version fails" ;;
		bin/check) "$af_d/bin/check" >/dev/null 2>&1 || fail "$4: after the next make, bin/check fails" ;;
	esac
}

# failing NAME TARGET GOAL: the switch to the fake compiler, whose call making TARGET writes it and
# fails
failing() {
	cp -Rp "$built" "$work/$1" || die "cannot copy the build for $1"
	FM=fail FT=$2
	mk "$1" "$work/$1" CXX="$fake" "$3"
	fa_rc=$?
	FM= FT=
	grep -q "^cxx .* -o $2\$" "$work/log/$1.calls" || die "$1: the switch never reached $2's call: the check proves nothing"
	test "$fa_rc" != 0 || fail "$1: make CXX=<fake> $3 exits 0 though $2's call failed"
	after "$1" "$2" "$3" "a failed make of $2 after a switch"
}

# interrupted NAME TARGET GOAL: the switch to the fake compiler, SIGINT to make's process group once
# the call making TARGET has written it
interrupted() {
	cp -Rp "$built" "$work/$1" || die "cannot copy the build for $1"
	FM=slow FT=$2
	mk "$1" "$work/$1" CXX="$fake" "$3" &
	in_bg=$!
	FM= FT=
	in_n=0
	until [ "$(cat "$work/$1/$2" 2>/dev/null)" = PARTIAL ]; do
		in_n=$((in_n + 1))
		if [ "$in_n" -gt 3000 ] || ! kill -0 "$in_bg" 2>/dev/null; then
			kill -s TERM "$in_bg" 2>/dev/null
			wait "$in_bg"
			die "$1: $2 never reached its call: the check proves nothing"
		fi
		perl -e 'select(undef, undef, undef, 0.2)'
	done
	perl -e 'select(undef, undef, undef, 0.3)'
	in_pg=
	for in_f in "$work"/pg/*; do
		[ -f "$in_f" ] && in_pg=${in_f##*/}
	done
	test -n "$in_pg" || die "$1: no process group recorded for the interrupted make"
	kill -s INT -- "-$in_pg" 2>/dev/null
	wait "$in_bg"
	in_rc=$?
	test "$in_rc" != 0 || fail "$1: the interrupted make CXX=<fake> $3 exits 0"
	after "$1" "$2" "$3" "an interrupted make of $2 after a switch"
}

failing mps-fail bin/mps bin/mps
interrupted mps-int bin/mps bin/mps
failing check-fail bin/check bin/check
interrupted check-int bin/check bin/check
failing object-fail lib/mp.o bin/mps
interrupted object-int lib/check/mp.o bin/check

if [ "$FAILS" -gt 0 ]; then
	echo "f1-partial-link: $FAILS checks failed ($MAKE_VERSION)" >&2
	exit 1
fi
echo "f1-partial-link: green ($MAKE_VERSION; with other settings than the build's the database declares .DELETE_ON_ERROR and nothing phony but check, clean and FORCE; after a switch, a link of bin/mps and of bin/check and a compile of lib/mp.o and lib/check/mp.o that failed or was interrupted partway left no partial file, and the next run with the same settings remade each and left a working binary)"
