# Shared helpers for this cycle's phase gates (a cycle record, not a standing gate).
# Sourced by p1.sh … p4.sh. POSIX sh. Each gate copies the tree as a clone would hold it, runs
# `make check` with clang++ and g++-16, drives the built bin/mps in an empty directory with a clean
# environment, runs the gate's own probes (probe-sieve.cpp, probe-parse.cpp) against the tree's
# modules, and proves `make check` goes red under each injected defect (mutant.cpp). A gate prints
# `GATE FAIL: <reason> (R-ID)` and exits 1 on the first failure, or ends with a `Pn gate: green` line.

# The tree under test is the one holding this script, whatever the caller's directory.
root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "gate: not in a git tree" >&2; exit 1; }
here="$root/spec/cli-and-sieve-fixes/verify"
work=$(mktemp -d "${TMPDIR:-/tmp}/mps-gate.XXXXXX") || { echo "gate: mktemp failed" >&2; exit 1; }
logs="$work/logs"
mkdir -p "$logs" || { echo "gate: mkdir $logs" >&2; exit 1; }
trap 'chmod -R u+rwx "$work" 2>/dev/null; /bin/rm -rf "$work"' EXIT INT TERM

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
need clang++; need g++-16; need rsync; need git; need awk
COMPILERS='clang++ g++-16'

# The gates are frozen at the plan commit: this directory must match it, with nothing added.
plan_commit=$(cd "$root" && git log -1 --format=%H --grep='^\[fix\] Plan cli-and-sieve-fixes' -- spec/cli-and-sieve-fixes/verify)
test -n "$plan_commit" || fail "no plan commit for spec/cli-and-sieve-fixes/verify"
( cd "$root" && git diff --quiet "$plan_commit" -- spec/cli-and-sieve-fixes/verify ) \
	|| fail "spec/cli-and-sieve-fixes/verify differs from the plan commit $plan_commit: the gates are frozen"
test -z "$(cd "$root" && git ls-files --others --exclude-standard -- spec/cli-and-sieve-fixes/verify)" \
	|| fail "untracked files in spec/cli-and-sieve-fixes/verify: the gates are frozen"

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

# summary COUNTS-VAR LOG: both halves reported, each with at least one expectation and none failed
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
			|| { cat "$logs/check-$cxx.log" >&2; fail "$cxx: make check is red (R10)"; }
		summary_ok "$logs/check-$cxx.log" || { cat "$logs/check-$cxx.log" >&2; fail "$cxx: make check summaries missing or wrong (R10)"; }
		if grep -q 'warning:' "$logs/check-$cxx.log"; then
			grep 'warning:' "$logs/check-$cxx.log" >&2
			fail "$cxx: warnings in the build"
		fi
	done
	CHECKS=$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-clang++.log")
	CLIS=$(sed -n 's/^cli: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-clang++.log")
}

# probe NAME CXX [FLAGS...]: build the gate probe NAME (probe-NAME.cpp) against that copy's src/
# modules, all but main.cpp, compiled afresh with FLAGS; prints the probe's path
probe() {
	pn=$1 pc=$2
	shift 2
	pd="$work/tree-$pc"
	pb="$work/probe-$pn-$pc-$#"
	( cd "$pd" && "$pc" -std=c++20 -O1 -fno-fast-math -ffp-contract=off "$@" -I src "$here/probe-$pn.cpp" \
		$(ls src/*.cpp | grep -v '^src/main\.cpp$') -o "$pb" ) >"$logs/probe-$pn-$pc.log" 2>&1 \
		|| { cat "$logs/probe-$pn-$pc.log" >&2; fail "the $pn probe does not build against the tree with $pc (cli.hpp and math.hpp keep the names PLAN gives)"; }
	echo "$pb"
}

# in_empty PROGRAM ARGS...: run a probe in an empty directory, failing the gate if it leaves a file
in_empty() {
	ie="$work/cwd/probe"
	mkdir -p "$ie" || fail "mkdir $ie"
	( cd "$ie" && "$@" )
	ie_status=$?
	test -z "$(ls -A "$ie")" || fail "parsing $* left '$(ls -A "$ie")' behind (R4: -f only sets the path)"
	return $ie_status
}


# --- driving bin/mps ------------------------------------------------------------------------------

# run NAME ARGS...: the current build's bin/mps ($BIN) in an empty directory with a clean
# environment; stdout and stderr into $logs/NAME.out and .err, the status into $status. A run that
# leaves anything in its directory fails the gate: parsing touches no file.
run() {
	rn=$1
	shift
	rd="$work/cwd/$rn"
	mkdir -p "$rd" || fail "mkdir $rd"
	( cd "$rd" && env -i PATH=/usr/bin:/bin HOME="$rd" LC_ALL=C NO_COLOR=1 "$BIN" "$@" ) \
		>"$logs/$rn.out" 2>"$logs/$rn.err"
	status=$?
	left=$(ls -A "$rd")
	test -z "$left" || fail "mps $*: left '$left' in its working directory (R4: -f only sets the path)"
	test "$status" -lt 128 || fail "mps $*: killed by signal $((status - 128))"
}

# want_status NAME N WHY
want_status() {
	test "$status" -eq "$2" || { show "$1"; fail "$3: exit $status, want $2"; }
}

# want_out NAME TEXT WHY: stdout is exactly TEXT (one trailing newline; empty TEXT means empty stdout)
want_out() {
	if [ -z "$2" ]; then
		test ! -s "$logs/$1.out" || { show "$1"; fail "$3: stdout is not empty"; }
	else
		printf '%s\n' "$2" | cmp -s - "$logs/$1.out" || { show "$1"; fail "$3: stdout differs"; }
	fi
}

# want_err NAME TEXT WHY: stderr is exactly TEXT
want_err() {
	if [ -z "$2" ]; then
		test ! -s "$logs/$1.err" || { show "$1"; fail "$3: stderr is not empty"; }
	else
		printf '%s\n' "$2" | cmp -s - "$logs/$1.err" || { show "$1"; fail "$3: stderr differs"; }
	fi
}

err_has() {
	grep -qF -- "$2" "$logs/$1.err" || { show "$1"; fail "$3: stderr lacks '$2'"; }
}

err_lacks() {
	if grep -qF -- "$2" "$logs/$1.err"; then show "$1"; fail "$3: stderr has '$2'"; fi
}

show() {
	echo "--- stdout of $1" >&2; cat "$logs/$1.out" >&2
	echo "--- stderr of $1" >&2; cat "$logs/$1.err" >&2
}

START_TF='    info [mps] Running quick factor test'
START_P1='    info [mps] Running P-1 test'
START_LL='    info [mps] Running Lucas-Lehmer test'
START_ALL="$START_TF$nl$START_P1$nl$START_LL"

# primes_below N: every prime below N, one per line, by trial division in awk
primes_below() {
	awk -v n="$1" 'BEGIN { for (i = 2; i < n; i++) { p = 1; for (d = 2; d * d <= i; d++) if (i % d == 0) { p = 0; break } if (p) print i } }'
}


# --- the requirements, observed ------------------------------------------------------------------

# sieve_behaviour: R8 through bin/mps, R9 through the sieve probe, for each compiler
sieve_behaviour() {
	for cxx in $COMPILERS; do
		BIN="$work/tree-$cxx/bin/mps"
		for n in 0 1 2; do
			run "p$n-$cxx" -p "$n"
			want_status "p$n-$cxx" 0 "mps -p $n ($cxx, R8)"
			want_out "p$n-$cxx" '' "mps -p $n ($cxx, R8)"
		done
		for n in 3 4 5 30 31 32 100 1000 7920; do
			run "p$n-$cxx" -p "$n"
			want_status "p$n-$cxx" 0 "mps -p $n ($cxx, R8)"
			want_out "p$n-$cxx" "$(primes_below "$n")" "mps -p $n: every prime below $n, 2 included, ascending, nothing else ($cxx, R8)"
		done
		for spelling in '--primes 30' '30 -p' '30 --primes'; do
			set -f
			# shellcheck disable=SC2086
			run "pspell-$cxx" $spelling
			set +f
			want_status "pspell-$cxx" 0 "mps $spelling ($cxx, R8)"
			want_out "pspell-$cxx" "$(primes_below 30)" "mps $spelling ($cxx, R8)"
		done

		ps=$(probe sieve "$cxx")
		r=$(in_empty "$ps" range 0 3000) || fail "sieve probe crashed over 0..3000 ($cxx, R9)"
		test "$r" = "range 0..3000: 0 bad" || fail "prime_sieve is wrong: $r ($cxx, R9)"
		r=$(in_empty "$ps" sieve 1000000 1000001 1000003 10000000 10000001) || fail "sieve probe crashed on large n ($cxx, R9)"
		want="1000000 ok${nl}1000001 ok${nl}1000003 ok${nl}10000000 ok${nl}10000001 ok"
		test "$r" = "$want" || fail "prime_sieve is wrong for large n: $r ($cxx, R9)"
	done
	hardened_sieve
}

# The standard libraries' bounds assertions: libc++ checks vector<bool>::operator[] from its fast
# mode up, libstdc++ under _GLIBCXX_ASSERTIONS from GCC 15 (research/03). Each define is inert on the
# other library.
HARD='-D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE'

# canary CXX: an out-of-range vector<bool> write traps with CXX and $HARD, so a hardened run that
# passes is not passing because the library does not check
canary() {
	printf '#include <vector>\nint main() { std::vector<bool> v(1, true); v[2] = true; return v[0] ? 0 : 1; }\n' >"$work/stray.cpp"
	"$1" -std=c++20 -O1 $HARD "$work/stray.cpp" -o "$work/stray-$1" >"$logs/stray-$1.log" 2>&1 \
		|| fail "cannot build the hardening canary with $1"
	if sh -c '"$1" || exit 1' canary "$work/stray-$1" >/dev/null 2>&1; then
		fail "$1 with $HARD does not trap an out-of-range vector<bool> write: the hardened checks would be vacuous"
	fi
}

# hardened_sieve: the tree's sieve, compiled with the standard library's bounds assertions, writes
# nothing outside its n entries for n = 0 … 64 (R9). The assertions are first shown to trap on a
# deliberate out-of-range write, so a library that does not check cannot pass this vacuously.
hardened_sieve() {
	for cxx in $COMPILERS; do
		canary "$cxx"
		ps=$(probe sieve "$cxx" $HARD)
		r=$(in_empty "$ps" range 0 64 2>"$logs/hard-$cxx.err") || { cat "$logs/hard-$cxx.err" >&2; fail "prime_sieve writes outside its n entries: the hardened build trapped ($cxx, R9)"; }
		test "$r" = "range 0..64: 0 bad" || fail "hardened prime_sieve is wrong: $r ($cxx, R9)"
	done
	# At scale: pi(10^8) = 5,761,455, the last prime below 10^8 being 99,999,989
	BIN="$work/tree-clang++/bin/mps"
	( cd "$work" && "$BIN" -p 100000000 ) >"$logs/p1e8.out" 2>/dev/null || fail "mps -p 100000000 failed (R8)"
	c=$(wc -l <"$logs/p1e8.out" | tr -d ' ')
	test "$c" -eq 5761455 || fail "mps -p 100000000 printed $c lines, want 5761455 (R8)"
	test "$(tail -n 1 "$logs/p1e8.out")" = 99999989 || fail "mps -p 100000000 does not end in 99999989 (R8)"
}

# Bad in either mode (R6, R7): junk, signs, whitespace, a non-ASCII digit, past the bound, and past
# 2^64 (where a wrapping or saturating parse would accept them), one per line; the empty string is
# added where the list is used, since splitting on newlines drops an empty line.
BAD_PVALS="abc
31abc
3.5
 31
31 
+31
-1
٣
0x1f
1e3
1000000000
0001000000000
18446744073709551616
18446744073709551647
00018446744073709551647
1234567890123456789012345678901234567890"

# pval_behaviour: R6, R7 through bin/mps and the parse probe
pval_behaviour() {
	pv_ifs=$IFS
	for cxx in $COMPILERS; do
		BIN="$work/tree-$cxx/bin/mps"
		i=0
		IFS=$nl
		set -f
		for v in 0 1 00 '' $BAD_PVALS; do
			IFS=$pv_ifs
			set +f
			i=$((i + 1))
			run "bad$i-$cxx" -- "$v"
			want_status "bad$i-$cxx" 1 "mps -- '$v' ($cxx, R6)"
			want_out "bad$i-$cxx" '' "mps -- '$v' ($cxx, R6)"
			err_has "bad$i-$cxx" "PVAL '$v'" "mps -- '$v' names PVAL and the value ($cxx, R6)"
			err_lacks "bad$i-$cxx" 'Uncaught exception' "mps -- '$v' ($cxx, R6)"
			err_lacks "bad$i-$cxx" 'Running' "mps -- '$v' runs no test ($cxx, R6)"
		done
		IFS=$nl
		set -f
		for v in '' $BAD_PVALS; do
			IFS=$pv_ifs
			set +f
			i=$((i + 1))
			run "pbad$i-$cxx" -p -- "$v"
			want_status "pbad$i-$cxx" 1 "mps -p -- '$v' ($cxx, R7)"
			want_out "pbad$i-$cxx" '' "mps -p -- '$v' prints nothing on stdout ($cxx, R7)"
			err_has "pbad$i-$cxx" "PVAL '$v'" "mps -p -- '$v' names PVAL and the value ($cxx, R7)"
			err_lacks "pbad$i-$cxx" 'Uncaught exception' "mps -p -- '$v' ($cxx, R7)"
		done
		IFS=$pv_ifs
		set +f
		# A bare word starting with a digit is PVAL without --, too
		run "bare-$cxx" 31abc
		want_status "bare-$cxx" 1 "mps 31abc ($cxx, R6)"
		err_has "bare-$cxx" "PVAL '31abc'" "mps 31abc ($cxx, R6)"
		for v in 2 31 999999999 0000000031 000000000999999999; do
			run "good-$v-$cxx" "$v"
			want_status "good-$v-$cxx" 0 "mps $v ($cxx, R6)"
			want_err "good-$v-$cxx" "$START_ALL" "mps $v runs the three tests ($cxx, R2, R6)"
			want_out "good-$v-$cxx" '' "mps $v ($cxx)"
		done
		for v in 0 000 0000000002; do
			run "pgood-$v-$cxx" -p "$v"
			want_status "pgood-$v-$cxx" 0 "mps -p $v ($cxx, R7)"
			want_out "pgood-$v-$cxx" '' "mps -p $v ($cxx, R7, R8)"
		done

		pp=$(probe parse "$cxx")
		for a in '-p 999999999' '999999999 -p' '0 -p' '--primes 0' '0000000031' '999999999'; do
			set -f
			# shellcheck disable=SC2086
			in_empty "$pp" $a >"$logs/parse.out" 2>&1
			set +f
			grep -q '^pval_number=' "$logs/parse.out" || { cat "$logs/parse.out" >&2; fail "PVAL '$a' is rejected by the parser ($cxx, R6, R7)"; }
		done
		in_empty "$pp" 0000000031 >"$logs/parse.out" 2>&1
		grep -qx 'pval_number=31' "$logs/parse.out" && grep -qx 'pval=0000000031' "$logs/parse.out" \
			&& grep -qx 'file_path=0000000031.mps' "$logs/parse.out" \
			|| { cat "$logs/parse.out" >&2; fail "PVAL 0000000031: want the number 31, the string as typed and the default path 0000000031.mps ($cxx, R6)"; }
		in_empty "$pp" -p 999999999 >"$logs/parse.out" 2>&1
		grep -qx 'pval_number=999999999' "$logs/parse.out" && grep -qx 'show_primes=1' "$logs/parse.out" \
			|| { cat "$logs/parse.out" >&2; fail "-p 999999999: want the bound accepted ($cxx, R7)"; }
		in_empty "$pp" 0 >"$logs/parse.out" 2>&1
		grep -q "^error: .*PVAL '0'" "$logs/parse.out" || { cat "$logs/parse.out" >&2; fail "PVAL 0 without -p: want an argument error ($cxx, R6)"; }
	done
}

# Literal help text (R1). Everything but the -p and -t lines and the closing paragraph is as on main.
HELP_TEXT="Usage:
  mps [-hv] [-p] PVAL [-s] [-t NAME] [-f PATH] [-d] [-D]
  Test if number is Mersenne prime.

Arguments:
  PVAL                    Mersenne prime exponent.

Options:
  -p, --primes            Print the primes below PVAL.
  -s, --skip-ptest        Suppress initial prime test on PVAL.
  -t, --test        NAME  One of 'tf' ('trial-factor'), 'p1' ('p-minus-1'),
                          'll' ('lucas-lehmer') or 'all' (default: 'all').
  -f, --file        PATH  File path for checkpointing.
  -d, --debug             Show debug messages.
  -D, --detailed          Show detailed logging messages.
  -v, --version           Show version and exit.
  -h, --help              Show this message and exit.

  Give a valued option as -x VALUE, -xVALUE, -x=VALUE, --long VALUE or
  --long=VALUE. Short flags may be grouped (-dD)."

# options_behaviour: R1–R5 through bin/mps and the parse probe
options_behaviour() {
	for cxx in $COMPILERS; do
		BIN="$work/tree-$cxx/bin/mps"

		# R1: the help, exactly, by either spelling; in colour, the aliases are not split
		for h in --help -h; do
			run "help$h-$cxx" "$h"
			want_status "help$h-$cxx" 0 "mps $h ($cxx, R1)"
			want_out "help$h-$cxx" "$HELP_TEXT" "mps $h prints the help PLAN gives ($cxx, R1)"
		done
		( env -i PATH=/usr/bin:/bin LC_ALL=C FORCE_COLOR=1 "$BIN" --help ) >"$logs/colour.out" 2>&1
		for alias in trial-factor p-minus-1 lucas-lehmer; do
			grep -qF "'$alias'" "$logs/colour.out" || fail "in colour, --help splits '$alias' (an option match inside a word) ($cxx, R1)"
		done
		grep -q "$(printf '\033')" "$logs/colour.out" || fail "FORCE_COLOR=1 --help has no colour at all ($cxx)"

		# R2: each name and alias runs that test alone; all and no -t run the three in order
		for pair in "tf:$START_TF" "trial-factor:$START_TF" "p1:$START_P1" "p-minus-1:$START_P1" \
		            "ll:$START_LL" "lucas-lehmer:$START_LL" "all:$START_ALL"; do
			name=${pair%%:*} want=${pair#*:}
			for sp in sep long; do
				case $sp in
					sep)  run "t-$name-$sp-$cxx" -t "$name" 31 ;;
					long) run "t-$name-$sp-$cxx" "--test=$name" 31 ;;
				esac
				want_status "t-$name-$sp-$cxx" 0 "mps -t $name 31 ($cxx, R2)"
				want_err "t-$name-$sp-$cxx" "$want" "mps -t $name 31 runs exactly its test ($cxx, R2)"
				want_out "t-$name-$sp-$cxx" '' "mps -t $name 31 ($cxx)"
			done
		done
		run "t-absent-$cxx" 31
		want_status "t-absent-$cxx" 0 "mps 31 ($cxx, R2)"
		want_err "t-absent-$cxx" "$START_ALL" "mps 31 runs the three tests ($cxx, R2)"

		# R3: any other -t value is an argument error naming it
		for v in qf quick factor pm p-min none '' no-such-test LL ' ll' 'll ' =ll; do
			for sp in sep join eq long longeq; do
				case $sp in
					sep)    run "r3-$sp-$cxx" -t "$v" 31 ;;
					join)   # -t joined to nothing is -t VALUE, and to =VALUE is -t=VALUE
					        case $v in ''|=*) continue ;; esac
					        run "r3-$sp-$cxx" "-t$v" 31 ;;
					eq)     run "r3-$sp-$cxx" "-t=$v" 31 ;;
					long)   run "r3-$sp-$cxx" --test "$v" 31 ;;
					longeq) run "r3-$sp-$cxx" "--test=$v" 31 ;;
				esac
				want_status "r3-$sp-$cxx" 1 "mps -t '$v' ($sp) ($cxx, R3)"
				err_has "r3-$sp-$cxx" "'$v'" "mps -t '$v' ($sp) names the value ($cxx, R3)"
				err_lacks "r3-$sp-$cxx" 'Uncaught exception' "mps -t '$v' ($cxx, R3)"
				err_lacks "r3-$sp-$cxx" 'Running' "mps -t '$v' runs no test ($cxx, R3)"
			done
		done

		# R4: the five spellings of each valued option set exactly VALUE; --file then -t ll
		pp=$(probe parse "$cxx")
		path='no-such-dir/x=y.mps'
		in_empty "$pp" --test ll --file "$path" 31 >"$logs/base.out" 2>&1
		grep -qx 'test_case=lucas-lehmer' "$logs/base.out" && grep -qx "file_path=$path" "$logs/base.out" \
			&& grep -qx 'opt_test_case=1' "$logs/base.out" && grep -qx 'opt_file_path=1' "$logs/base.out" \
			|| { cat "$logs/base.out" >&2; fail "--test ll --file $path: want Lucas-Lehmer and that path ($cxx, R4)"; }
		for tv in ll lucas-lehmer; do
			for ts in 1 2 3 4 5; do
				for fs in 1 2 3 4 5; do
					set -f
					case $ts in
						1) targ="-t $tv" ;; 2) targ="-t$tv" ;; 3) targ="-t=$tv" ;; 4) targ="--test $tv" ;; 5) targ="--test=$tv" ;;
					esac
					case $fs in
						1) farg="-f $path" ;; 2) farg="-f$path" ;; 3) farg="-f=$path" ;; 4) farg="--file $path" ;; 5) farg="--file=$path" ;;
					esac
					# shellcheck disable=SC2086
					in_empty "$pp" $farg $targ 31 >"$logs/spell.out" 2>&1
					set +f
					cmp -s "$logs/base.out" "$logs/spell.out" || { diff "$logs/base.out" "$logs/spell.out" >&2; fail "'$farg $targ 31' parses differently from '--test ll --file $path 31' ($cxx, R4)"; }
				done
			done
		done
		for a in '-f=a=b 31:file_path=a=b' '--file=a=b 31:file_path=a=b' '-f =x 31:file_path==x' '-fdD 31:file_path=dD' \
		         '-f==x 31:file_path==x' '--file==x 31:file_path==x' '-f== 31:file_path==' '--file a=b 31:file_path=a=b'; do
			set -f
			# shellcheck disable=SC2086
			in_empty "$pp" ${a%%:*} >"$logs/fv.out" 2>&1
			set +f
			grep -qx "${a#*:}" "$logs/fv.out" || { cat "$logs/fv.out" >&2; fail "'${a%%:*}': want ${a#*:} ($cxx, R4)"; }
		done
		run "file-ll-$cxx" --file out.mps -t ll 31
		want_status "file-ll-$cxx" 0 "mps --file out.mps -t ll 31 ($cxx, R4)"
		want_err "file-ll-$cxx" "$START_LL" "mps --file out.mps -t ll 31 runs Lucas-Lehmer alone ($cxx, R4)"
		for a in '-f=out.mps' '-fout.mps' '-f out.mps' '--file=out.mps'; do
			set -f
			# shellcheck disable=SC2086
			run "file-$cxx" $a 31
			set +f
			want_status "file-$cxx" 0 "mps $a 31 ($cxx, R4)"
		done
		for a in "-t" "--test" "-f" "--file" "-dt" ; do
			run "missing-$cxx" 31 "$a"
			want_status "missing-$cxx" 1 "mps 31 $a: a missing value ($cxx, R4)"
			err_lacks "missing-$cxx" 'Uncaught exception' "mps 31 $a ($cxx)"
		done
		for a in "-f=" "--file=" ; do
			run "empty-$cxx" "$a" 31
			want_status "empty-$cxx" 1 "mps $a 31: an empty path ($cxx, PLAN)"
		done

		# R5: a group is its flags given separately, in order
		for pair in '-dD 31:-d -D 31' '-Dd 31:-D -d 31' '-pd 30:-p -d 30' '-sdD 31:-s -d -D 31' \
		            '-hv:-h' '-vh:-v' '-hx:-h' '-hdD:-h' '-vq:-v' '-dh 31:-h' '-ps 30:-p -s 30'; do
			set -f
			# shellcheck disable=SC2086
			in_empty "$pp" ${pair%%:*} >"$logs/g1.out" 2>&1
			# shellcheck disable=SC2086
			in_empty "$pp" ${pair#*:} >"$logs/g2.out" 2>&1
			set +f
			cmp -s "$logs/g1.out" "$logs/g2.out" || { diff "$logs/g1.out" "$logs/g2.out" >&2; fail "'${pair%%:*}' parses differently from '${pair#*:}' ($cxx, R5)"; }
		done
		in_empty "$pp" -dD 31 >"$logs/g.out" 2>&1
		grep -qx 'debug_mode=1' "$logs/g.out" && grep -qx 'detailed_mode=1' "$logs/g.out" || fail "-dD 31 sets neither flag ($cxx, R5)"
		in_empty "$pp" -sdD 31 >"$logs/g.out" 2>&1
		grep -qx 'skip_ptest=1' "$logs/g.out" && grep -qx 'debug_mode=1' "$logs/g.out" && grep -qx 'detailed_mode=1' "$logs/g.out" \
			|| fail "-sdD 31 does not set all three ($cxx, R5)"
		in_empty "$pp" -hv >"$logs/g.out" 2>&1
		grep -q '^info: Usage:' "$logs/g.out" || fail "-hv is not the help ($cxx, R5)"
		in_empty "$pp" -vh >"$logs/g.out" 2>&1
		grep -q '^info: mps v' "$logs/g.out" || fail "-vh is not the version ($cxx, R5)"
		for g in '-dx:-x' '-xh:-x' '-dDq:-q' '-pdZ:-Z' '-s1:-1'; do
			in_empty "$pp" "${g%%:*}" 31 >"$logs/g.out" 2>&1
			grep -q "^error: .*${g#*:}" "$logs/g.out" || { cat "$logs/g.out" >&2; fail "'${g%%:*} 31': want an argument error naming ${g#*:} ($cxx, R5)"; }
		done
		# Duplicates in a group warn as they do apart; the warnings stay (PLAN)
		for f in d s; do
			run "dup1-$cxx" "-$f$f" -t ll 31
			run "dup2-$cxx" "-$f" "-$f" -t ll 31
			cmp -s "$logs/dup1-$cxx.err" "$logs/dup2-$cxx.err" || { show "dup1-$cxx"; show "dup2-$cxx"; fail "-$f$f and -$f -$f differ on stderr ($cxx, R5)"; }
			err_has "dup2-$cxx" "Duplicate use of -$f/" "-$f -$f warns of the duplicate ($cxx, PLAN)"
		done
		run "g-pd-$cxx" -pd 30
		want_status "g-pd-$cxx" 0 "mps -pd 30 ($cxx, R5)"
		want_out "g-pd-$cxx" "$(primes_below 30)" "mps -pd 30 lists the primes ($cxx, R5)"
		run "g-dx-$cxx" -dx 31
		want_status "g-dx-$cxx" 1 "mps -dx 31 ($cxx, R5)"
		err_lacks "g-dx-$cxx" 'Running' "mps -dx 31 runs no test ($cxx, R5)"
		run "g-hv-$cxx" -hv
		want_status "g-hv-$cxx" 0 "mps -hv ($cxx, R5)"
		want_out "g-hv-$cxx" "$HELP_TEXT" "mps -hv prints the help ($cxx, R5)"

		# PLAN: -- ends the options and is not PVAL; a lone - is a positional (a bad PVAL)
		in_empty "$pp" -- 31 >"$logs/g.out" 2>&1
		grep -qx 'pval=31' "$logs/g.out" || { cat "$logs/g.out" >&2; fail "-- 31: want PVAL 31 (PLAN)"; }
		in_empty "$pp" -t ll -- -dD >"$logs/g.out" 2>&1
		grep -q "^error: .*PVAL '-dD'" "$logs/g.out" || { cat "$logs/g.out" >&2; fail "-- -dD: want -dD as a bad PVAL (PLAN)"; }
		in_empty "$pp" - >"$logs/g.out" 2>&1
		grep -q "^error: .*PVAL '-'" "$logs/g.out" || { cat "$logs/g.out" >&2; fail "a lone -: want a bad PVAL (PLAN)"; }
	done
}


# --- R10: make check pins each requirement ------------------------------------------------------

MUT="$work/mut"

# mutants_setup: a copy of the tree wired through mutant.cpp, built with clang++; with no mutant
# selected, make check is green through the wrappers and none is applied.
mutants_setup() {
	copy_tree "$MUT"
	for f in cli math; do
		test -f "$MUT/src/$f.cpp" || fail "src/$f.cpp is missing"
	done
	{ echo '#define parse_args gate_real_parse_args'; cat "$MUT/src/cli.cpp"; } >"$work/cli.cpp" && mv "$work/cli.cpp" "$MUT/src/cli.cpp" || fail "wire cli.cpp"
	{ echo '#define prime_sieve gate_real_prime_sieve'; echo '#define display_primes gate_real_display_primes'; cat "$MUT/src/math.cpp"; } >"$work/math.cpp" \
		&& mv "$work/math.cpp" "$MUT/src/math.cpp" || fail "wire math.cpp"
	cp "$here/mutant.cpp" "$MUT/src/gate_mutant.cpp" || fail "copy mutant.cpp"
	MODS=$(sed -n 's/^MODULES *:= *//p' "$MUT/Makefile")
	test -n "$MODS" || fail "no MODULES line in the Makefile"
	( cd "$MUT" && "$MAKE" CXX=clang++ MODULES="$MODS gate_mutant" bin/mps bin/check ) >"$logs/mut-build.log" 2>&1 \
		|| { tail -30 "$logs/mut-build.log" >&2; fail "the tree does not build through the gate's wrappers: the checks call cli::parse_args(argc, argv), and cli.hpp and math.hpp keep the signatures PLAN gives (R10)"; }
	: >"$work/applied-control"
	( cd "$MUT" && env GATE_MUTANT_LOG="$work/applied-control" "$MAKE" CXX=clang++ MODULES="$MODS gate_mutant" check ) >"$logs/mut-control.log" 2>&1 \
		|| { tail -30 "$logs/mut-control.log" >&2; fail "make check is red through the gate's pass-through wrappers (R10)"; }
	summary_ok "$logs/mut-control.log" || fail "make check summaries missing through the wrappers"
	test ! -s "$work/applied-control" || fail "the control run applied a mutant"
}

# mutant ID R-ID [HALF]: make check goes red under the defect ID, which some check reached; with
# HALF (check or cli), that half's own summary must show the failure
MUTANTS=0
mutant() {
	ml="$work/applied-$1"
	: >"$ml"
	if ( cd "$MUT" && env GATE_MUTANT="$1" GATE_MUTANT_LOG="$ml" "$MAKE" CXX=clang++ MODULES="$MODS gate_mutant" check ) >"$logs/mut-$1.log" 2>&1; then
		test -s "$ml" || fail "no check in make check exercises what mutant $1 breaks ($2, R10)"
		fail "make check stays green when mutant $1 breaks $2 (R10)"
	fi
	test -s "$ml" || { tail -20 "$logs/mut-$1.log" >&2; fail "make check went red under $1 without reaching it ($2, R10)"; }
	# Caught: a failed check in either summary, or bin/check stopped by the defect before its own
	if ! grep -Eq '^(check|cli): [0-9]+ passed, [1-9][0-9]* failed$' "$logs/mut-$1.log" \
		&& grep -q '^check: ' "$logs/mut-$1.log"; then
		tail -20 "$logs/mut-$1.log" >&2
		fail "under $1, make check went red with no failed check in its summaries ($2, R10)"
	fi
	if [ -n "${3:-}" ]; then
		grep -Eq "^$3: [0-9]+ passed, [1-9][0-9]* failed\$" "$logs/mut-$1.log" \
			|| { tail -20 "$logs/mut-$1.log" >&2; fail "under $1, $3's own checks stay green ($2, R10)"; }
	fi
	MUTANTS=$((MUTANTS + 1))
}

mutants_sieve() {
	# tests/cli.sh itself must reject a listing without its 2 (R10's second clause)
	mutant d-no2 R8 cli
	for m in d-inclusive d-extra d-descending; do mutant "$m" R8; done
	for m in q-size0 q-size1 q-size2 q-zero1 q-one2 q-one3 q-three q-one-large q-zero-large q-square q-mid; do
		mutant "$m" R9
	done
}

mutants_pval() {
	for m in v-junk v-small v-big v-wrap v-zeros v-name v-run v-uncaught; do mutant "$m" R6; done
	for m in pv-junk pv-big pv-wrap pv-zero pv-name pv-stdout; do mutant "$m" R7; done
}

mutants_options() {
	help_mutant "'tf'" "'qf'"
	help_mutant "'p1'" "'pm'"
	help_mutant "'ll' (" "'lx' ("
	help_mutant "or 'all'" "or 'any'"
	help_mutant "default: 'all'" "default: 'll'"
	help_mutant "trial-factor" "trial-factoring"
	help_mutant "p-minus-1" "p-minus-one"
	help_mutant "lucas-lehmer" "lucas_lehmer"
	help_mutant "'tf' ('trial-factor'), 'p1' ('p-minus-1')" "'tf' ('p-minus-1'), 'p1' ('trial-factor')"
	help_mutant "below PVAL" "up to PVAL"
	help_mutant "-x VALUE, -xVALUE" "-xVALUE"
	help_mutant "-xVALUE" "-x VALUE"
	help_mutant "-x=VALUE" "-x VALUE"
	help_mutant "--long VALUE or" "or"
	help_mutant "--long=VALUE" "--long VALUE"
	# R2's "that test alone": the dispatch in main.cpp, each case sent elsewhere
	edit_mutant src/main.cpp "test_quick_factor(" "test_all(" R2
	edit_mutant src/main.cpp "test_p_minus_one(" "test_all(" R2
	edit_mutant src/main.cpp "test_lucas_lehmer(" "test_p_minus_one(" R2
	edit_mutant src/main.cpp "test_all(" "test_lucas_lehmer(" R2
	for m in n-tf n-trial-factor n-p1 n-p-minus-1 n-ll n-lucas-lehmer n-all n-absent; do mutant "$m" R2; done
	for m in r-qf r-quick r-factor r-pm r-p-min r-none r-empty r-unknown r-message; do mutant "$m" R3; done
	for m in s-t1 s-t2 s-t3 s-t4 s-t5 s-f1 s-f2 s-f3 s-f4 s-f5 s-file-as-test; do mutant "$m" R4; done
	for m in g-first g-reverse g-drop-bad g-drop-info; do mutant "$m" R5; done
}

# edit_mutant FILE FROM TO R-ID: with FROM replaced by TO everywhere in FILE, the tree still builds
# and make check goes red. Each edit starts from the files as copied, and must change its file.
edit_mutant() {
	hm="$work/edit"
	if [ ! -d "$hm" ]; then
		copy_tree "$hm"
		mkdir -p "$work/orig/src" || fail "mkdir"
		( cd "$hm" && "$MAKE" CXX=clang++ bin/mps bin/check ) >"$logs/edit-build.log" 2>&1 || fail "the edit-mutant copy does not build"
	fi
	test -f "$work/orig/$1" || cp "$hm/$1" "$work/orig/$1" || fail "save $1"
	for em_f in "$work"/orig/src/*; do
		cp "$em_f" "$hm/src/${em_f##*/}" || fail "restore ${em_f##*/}"
	done
	awk -v from="$2" -v to="$3" '{ out = ""; while ((i = index($0, from)) > 0) { out = out substr($0, 1, i - 1) to; $0 = substr($0, i + length(from)) } print out $0 }' \
		"$work/orig/$1" >"$hm/$1" || fail "edit $1"
	! cmp -s "$work/orig/$1" "$hm/$1" || fail "$1 has no '$2' to break ($4)"
	/bin/rm -f "$hm/bin/mps" "$hm/bin/check"
	sleep 1   # make 3.81 compares whole seconds
	touch "$hm/$1"
	if ( cd "$hm" && "$MAKE" CXX=clang++ check ) >"$logs/edit-mut.log" 2>&1; then
		fail "make check stays green with '$2' changed to '$3' in $1 ($4, R10)"
	fi
	test -x "$hm/bin/check" && test -x "$hm/bin/mps" || { tail -20 "$logs/edit-mut.log" >&2; fail "the edit of '$2' in $1 did not build: the red is not the checks' ($4, R10)"; }
	grep -Eq '^(check|cli): [0-9]+ passed, [1-9][0-9]* failed$' "$logs/edit-mut.log" \
		|| { tail -20 "$logs/edit-mut.log" >&2; fail "with '$2' changed in $1, make check went red with no failed check ($4, R10)"; }
	MUTANTS=$((MUTANTS + 1))
}

# help_mutant FROM TO: an edit of the help text in src/main.hpp, which R1 pins
help_mutant() {
	edit_mutant src/main.hpp "$1" "$2" R1
}

# hardened_check: make check, rebuilt with the bounds assertions, is green with both compilers, and
# goes red when the sieve writes outside its entries (the old defect, injected); the old sieve itself,
# from d179aa4, turns plain make check red (R9, R10). Run after the other mutants: it rebuilds $MUT.
hardened_check() {
	hflags="-O1 -Wall -Wextra $HARD"
	for cxx in $COMPILERS; do
		canary "$cxx"
		h="$work/hard-$cxx"
		copy_tree "$h"
		( cd "$h" && "$MAKE" -B CXX="$cxx" CXXFLAGS="$hflags" check ) >"$logs/hard-check-$cxx.log" 2>&1 \
			|| { tail -30 "$logs/hard-check-$cxx.log" >&2; fail "make check is red with the bounds assertions on ($cxx, R9)"; }
		summary_ok "$logs/hard-check-$cxx.log" || fail "hardened make check summaries missing ($cxx, R9)"
	done
	o="$work/old-sieve"
	copy_tree "$o"
	( cd "$root" && git show d179aa4:src/math.cpp ) >"$o/src/math.cpp" || fail "git show d179aa4:src/math.cpp"
	if ( cd "$o" && "$MAKE" CXX=clang++ check ) >"$logs/old-sieve.log" 2>&1; then
		fail "make check stays green with the old sieve from d179aa4 (R9, R10)"
	fi
	test -x "$o/bin/check" || fail "the old sieve did not build: the red is not the checks' (R10)"
	( cd "$MUT" && "$MAKE" -B CXX=clang++ CXXFLAGS="$hflags" MODULES="$MODS gate_mutant" bin/mps bin/check ) >"$logs/mut-hard-build.log" 2>&1 \
		|| fail "the hardened mutant build failed"
	mutant q-stray1 R9
	mutant q-stray2 R9
}

# caveats_tests: caveats over every file under tests/, at any depth
caveats_tests() {
	ct_ifs=$IFS
	IFS=$nl
	set -f
	for ct_f in $(cd "$root" && find tests -type f); do
		caveats "$ct_f"
	done
	set +f
	IFS=$ct_ifs
}

# caveats FILE...: no file describes the fixed defects as known, or accepts -p with or without its 2
caveats() {
	for f in "$@"; do
		test -f "$root/$f" || fail "$f is missing"
		if grep -Ein 'known to be wrong|known (parse |command-line )?bugs|right today|is accepted too|with or without (its |a )?2|leading 2|known defects?|known to fail' "$root/$f"; then
			fail "$f still describes the defects as known, or accepts -p with or without its 2 (R10)"
		fi
	done
}

# hygiene: math.hpp's signatures and the README are as on main; no spec ids in src/ or tests/
hygiene() {
	( cd "$root" && git diff --quiet origin/main -- src/math.hpp README.md ) || fail "src/math.hpp or README.md changed (PLAN keeps both)"
	if grep -rnE '(^|[^A-Za-z0-9_])(R[0-9]{1,2}|P[1-9])([^A-Za-z0-9_-]|$)' "$root/src" "$root/tests"; then
		fail "spec R#/P# ids in source (AGENTS.md § Conventions)"
	fi
}
