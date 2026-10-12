#!/bin/sh
# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
#
# Command-line smoke test
#
# Usage: sh tests/cli.sh BINARY
#
# Runs BINARY with shell built-ins only, so it needs nothing beyond a POSIX shell. Output is
# captured into variables before it is compared, never piped, so every status is the
# binary's own. There is no `set -e` and no `local`; `set -u` stops a misspelled variable,
# and `planned` catches a skipped expectation. Each failed expectation prints
# ` FAIL <name>`; the last line is `cli: <passed> passed, <failed> failed`.
#

set -u

if [ $# -ne 1 ]; then
	echo "usage: sh tests/cli.sh BINARY" >&2
	exit 2
fi
mps=$1

# The caller's environment must not change the output.
LC_ALL=C
NO_COLOR=1
export LC_ALL NO_COLOR
unset FORCE_COLOR TASK_ID

case $0 in
	*/*) here=${0%/*} ;;
	*)   here=. ;;
esac

nl='
'
planned=126
passed=0
failed=0
got=


expect() { # NAME CMD...: passes only when CMD succeeds
	name=$1
	shift
	if "$@"; then
		passed=$((passed + 1))
	else
		failed=$((failed + 1))
		echo "   got $got"
		echo " FAIL $name"
	fi
}


run() { # ARGS...: stdout into out, the exit status into status; stderr discarded
	out=$("$mps" "$@" 2>/dev/null)
	status=$?
	got="exit $status, stdout '${out%%"$nl"*}'"
}


run_all() { # ARGS...: the whole of stdout into out, trailing newlines kept; status as for run
	out=$("$mps" "$@" 2>/dev/null; echo ".$?")
	status=${out##*.}
	out=${out%.*}
	got="exit $status, stdout '$out'"
}


run_stderr() { # ARGS...: stderr into err, the exit status into status; stdout discarded
	err=$("$mps" "$@" 2>&1 >/dev/null)
	status=$?
	got="exit $status, stderr '${err%%"$nl"*}'"
}


has() { # TEXT PART: TEXT holds PART
	case $1 in
		*"$2"*) return 0 ;;
	esac
	return 1
}


lacks() { # TEXT PART: TEXT does not hold PART
	! has "$1" "$2"
}


once() { # TEXT PART: TEXT holds PART exactly once
	case $1 in
		*"$2"*"$2"*) return 1 ;;
		*"$2"*)      return 0 ;;
	esac
	return 1
}


# The version lives only in src/main.hpp.
version=
version_source="$here/../src/main.hpp"
if [ -r "$version_source" ]; then
	while IFS= read -r line; do
		case $line in
			'static const std::string APP_VERSION = "'*'";')
				version=${line#*\"}
				version=${version%\"*}
				;;
		esac
	done <"$version_source"
fi
got="version '$version'"
expect "version read from src/main.hpp" [ -n "$version" ]

for option in --version -v; do
	run "$option"
	expect "$option exits 0" [ "$status" -eq 0 ]
	expect "$option prints mps v$version" [ "$out" = "mps v$version" ]
done

run --help
expect "--help exits 0" [ "$status" -eq 0 ]
case $out in
	Usage:*) usage=yes ;;
	*)       usage=no ;;
esac
expect "--help starts with Usage:" [ "$usage" = yes ]
help=$out

# The help names each test with its alias, says which is the default, states the five
# spellings of a valued option, and says -p lists the primes below PVAL.
for part in "'tf' ('trial-factor')" "'p1' ('p-minus-1')" "'ll' ('lucas-lehmer')" "'all' (default: 'all')" \
            "-x VALUE," "-xVALUE," "-x=VALUE," "--long VALUE or" "--long=VALUE." "Print the primes below PVAL."; do
	got="--help without '$part'"
	expect "--help holds $part" has "$help" "$part"
done

# The help states the five spellings in one place, so each appears exactly once.
for part in "-x VALUE" "-xVALUE" "-x=VALUE" "--long VALUE" "--long=VALUE"; do
	got="--help without '$part' exactly once"
	expect "--help states $part once" once "$help" "$part"
done

run -h
expect "-h prints the help" [ "$out" = "$help" ]

# -p lists every prime below PVAL, 2 included, one per line; below 3 there is none.
for n in 0 1 2; do
	run_all -p "$n"
	expect "-p $n exits 0" [ "$status" -eq 0 ]
	expect "-p $n prints nothing" [ -z "$out" ]
done

below_30="2${nl}3${nl}5${nl}7${nl}11${nl}13${nl}17${nl}19${nl}23${nl}29${nl}"
for pair in "3:2${nl}" "4:2${nl}3${nl}" "30:$below_30" "31:$below_30" "32:${below_30}31${nl}"; do
	n=${pair%%:*}
	primes=${pair#*:}
	run_all -p "$n"
	expect "-p $n exits 0" [ "$status" -eq 0 ]
	expect "-p $n prints the primes below $n" [ "$out" = "$primes" ]
done

# Each -t name runs its test alone, and all three run for all and when -t is absent.
start_tf='    info [mps] Running quick factor test'
start_p1='    info [mps] Running P-1 test'
start_ll='    info [mps] Running Lucas-Lehmer test'
start_all="$start_tf$nl$start_p1$nl$start_ll"
for name in tf trial-factor p1 p-minus-1 ll lucas-lehmer all; do
	case $name in
		tf|trial-factor)  want=$start_tf ;;
		p1|p-minus-1)     want=$start_p1 ;;
		ll|lucas-lehmer)  want=$start_ll ;;
		all)              want=$start_all ;;
	esac
	run_stderr -t "$name" 31
	expect "-t $name 31 exits 0" [ "$status" -eq 0 ]
	expect "-t $name 31 runs exactly its test" [ "$err" = "$want" ]
done

run_stderr 31
expect "31 exits 0" [ "$status" -eq 0 ]
expect "31 runs all three tests" [ "$err" = "$start_all" ]

# --file takes the next word as its path, so -t after it still selects the test.
run_stderr --file out.mps -t ll 31
expect "--file out.mps -t ll 31 exits 0" [ "$status" -eq 0 ]
expect "--file out.mps -t ll 31 runs the Lucas-Lehmer test alone" [ "$err" = "$start_ll" ]

run --no-such-option 31
expect "an unknown option exits 1" [ "$status" -eq 1 ]

run -t no-such-test 31
expect "an unknown -t name exits 1" [ "$status" -eq 1 ]

run 31 61
expect "two PVALs exit 1" [ "$status" -eq 1 ]

# A bad PVAL is an argument error that names PVAL and the value, and runs no test. PVAL is
# ASCII digits from 2 to 999999999; past 2^64 it must not wrap into range.
for pval in abc 31abc 3.5 ' 31' '31 ' +31 '' 0 1 00 1000000000 18446744073709551647; do
	run_stderr "$pval"
	expect "PVAL '$pval' exits 1" [ "$status" -eq 1 ]
	expect "PVAL '$pval' is named in the error" has "$err" "PVAL '$pval'"
	expect "PVAL '$pval' is not an uncaught exception" lacks "$err" "Uncaught exception"
	expect "PVAL '$pval' runs no test" lacks "$err" "Running"
done

# -- ends the options, so what follows is PVAL even when it starts with -
run_stderr -- -1
expect "PVAL '-1' after -- exits 1" [ "$status" -eq 1 ]
expect "PVAL '-1' after -- is named in the error" has "$err" "PVAL '-1'"
expect "PVAL '-1' after -- runs no test" lacks "$err" "Running"

run_stderr -- 31
expect "-- 31 exits 0" [ "$status" -eq 0 ]

# With -p, PVAL may be 0 or 1 too, and a bad one prints nothing on stdout.
for pval in abc 1000000000; do
	run_all -p "$pval"
	expect "-p $pval exits 1" [ "$status" -eq 1 ]
	expect "-p $pval prints nothing" [ -z "$out" ]
	run_stderr -p "$pval"
	expect "-p $pval is named in the error" has "$err" "PVAL '$pval'"
	expect "-p $pval is not an uncaught exception" lacks "$err" "Uncaught exception"
done

run_all 0 -p
expect "0 -p exits 0" [ "$status" -eq 0 ]
expect "0 -p prints nothing" [ -z "$out" ]

# Leading zeros don't count against the bound, and the bound itself runs.
for pval in 0000000031 999999999; do
	run_stderr "$pval"
	expect "PVAL $pval exits 0" [ "$status" -eq 0 ]
	case "$nl$err$nl" in
		*"$nl    info [mps] Running Lucas-Lehmer test$nl"*) logged=yes ;;
		*)                                                  logged=no ;;
	esac
	expect "PVAL $pval runs the tests" [ "$logged" = yes ]
done

if [ $((passed + failed)) -ne "$planned" ]; then
	failed=$((failed + 1))
	echo " FAIL ran $((passed + failed - 1)) of $planned planned expectations"
fi

echo "cli: $passed passed, $failed failed"
if [ "$failed" -eq 0 ] && [ "$passed" -eq "$planned" ]; then
	exit 0
fi
exit 1
