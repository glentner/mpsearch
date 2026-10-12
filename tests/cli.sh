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
planned=530
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

# Each -t name runs its test alone, and all three run for all and when -t is absent. No test
# decides anything yet, so each says so in one line on stdout, as it completes, and the run
# exits 42, no verdict.
start_tf='    info [mps] Running quick factor test'
start_p1='    info [mps] Running P-1 test'
start_ll='    info [mps] Running Lucas-Lehmer test'
start_all="$start_tf$nl$start_p1$nl$start_ll"
tail=" (mps $version)"
line_tf="tf: no verdict, trial factoring is not implemented yet$tail"
line_p1="p1: no verdict, P-1 is not implemented yet$tail"
line_ll="ll: no verdict, Lucas-Lehmer is not implemented yet$tail"

lines_all() { # P: the three no-verdict lines for M_P, each ended
	printf 'M%s %s\nM%s %s\nM%s %s\n' "$1" "$line_tf" "$1" "$line_p1" "$1" "$line_ll"
}

for test_name in tf trial-factor p1 p-minus-1 ll lucas-lehmer all; do
	case $test_name in
		tf|trial-factor)  want=$start_tf lines="M31 $line_tf$nl" ;;
		p1|p-minus-1)     want=$start_p1 lines="M31 $line_p1$nl" ;;
		ll|lucas-lehmer)  want=$start_ll lines="M31 $line_ll$nl" ;;
		all)              want=$start_all lines=$(lines_all 31; echo .) lines=${lines%.} ;;
	esac
	run_stderr -t "$test_name" 31
	expect "-t $test_name 31 exits 42" [ "$status" -eq 42 ]
	expect "-t $test_name 31 runs exactly its test" [ "$err" = "$want" ]
	run_all -t "$test_name" 31
	expect "-t $test_name 31 prints its line for each test" [ "$out" = "$lines" ]
done

run_stderr 31
expect "31 exits 42" [ "$status" -eq 42 ]
expect "31 runs all three tests" [ "$err" = "$start_all" ]

# Each prime exponent passes the exponent check silently and gets the three lines, tf, p1 and
# ll in the order they ran, each naming the exponent and the program's version.
for pval in 2 3 31 999999929 999999937; do
	lines=$(lines_all "$pval"; echo .)
	lines=${lines%.}
	run_all "$pval"
	expect "$pval exits 42" [ "$status" -eq 42 ]
	expect "$pval prints three no-verdict lines in order" [ "$out" = "$lines" ]
	run_stderr "$pval"
	expect "$pval runs all three tests" [ "$err" = "$start_all" ]
done

# The verdict lines are never coloured, whatever FORCE_COLOR says; the log is, so the check
# is not vacuous.
esc=$(printf '\033')
out=$(unset NO_COLOR; FORCE_COLOR=1 "$mps" 31 2>/dev/null)
got="stdout '${out%%"$nl"*}'"
expect "31 under FORCE_COLOR prints no escape on stdout" lacks "$out" "$esc"
err=$(unset NO_COLOR; FORCE_COLOR=1 "$mps" 31 2>&1 >/dev/null)
got="stderr '${err%%"$nl"*}'"
expect "31 under FORCE_COLOR colours its log" has "$err" "$esc"

# --file takes the next word as its path, so -t after it still selects the test.
run_stderr --file out.mps -t ll 31
expect "--file out.mps -t ll 31 exits 42" [ "$status" -eq 42 ]
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
expect "-- 31 exits 42" [ "$status" -eq 42 ]

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

# Leading zeros don't count against the bound. (The bound itself, 999999999, is composite, so
# the exponent check answers it; see below.)
run_stderr 0000000031
expect "PVAL 0000000031 exits 42" [ "$status" -eq 42 ]
case "$nl$err$nl" in
	*"$nl    info [mps] Running Lucas-Lehmer test$nl"*) logged=yes ;;
	*)                                                  logged=no ;;
esac
expect "PVAL 0000000031 runs the tests" [ "$logged" = yes ]

# The exponent is written from its value, so leading zeros never reach the line.
lines=$(lines_all 31; echo .)
lines=${lines%.}
run_all 0000000031
expect "PVAL 0000000031 prints its lines for M31" [ "$out" = "$lines" ]

# A composite exponent is answered before any test runs, without -t and under every -t name:
# one line naming the factor 2^a-1, with a the exponent's smallest prime factor, and exit 40.
# 999002449 is 31607 squared, the largest smallest prime factor a PVAL can have; 9 is a prime
# square, where the search must try the square root itself.
for pair in 4:2 9:3 33:3 999999999:3 999002449:31607; do
	pval=${pair%%:*}
	factor=${pair#*:}
	for test_name in none tf trial-factor p1 p-minus-1 ll lucas-lehmer all; do
		if [ "$test_name" = none ]; then
			set -- "$pval"
		else
			set -- -t "$test_name" "$pval"
		fi
		run_stderr "$@"
		expect "$* exits 40" [ "$status" -eq 40 ]
		expect "$* runs no test" lacks "$err" "Running"
		run_all "$@"
		expect "$* prints the exponent check's line" [ "$out" = "M$pval ptest: composite, factor 2^$factor-1$tail$nl" ]
	done
done

# -s and --skip-ptest skip the exponent check: the tests run on a composite exponent as on a
# prime one, and nothing is printed for the check.
for pval in 4 33 999999999 31; do
	lines=$(lines_all "$pval"; echo .)
	lines=${lines%.}
	for skip in -s --skip-ptest; do
		run_all "$skip" "$pval"
		expect "$skip $pval exits 42" [ "$status" -eq 42 ]
		expect "$skip $pval prints the tests' lines" [ "$out" = "$lines" ]
		run_stderr "$skip" "$pval"
		expect "$skip $pval runs all three tests" [ "$err" = "$start_all" ]
	done
done

run_all -s -t ll 33
expect "-s -t ll 33 exits 42" [ "$status" -eq 42 ]
expect "-s -t ll 33 prints the Lucas-Lehmer line" [ "$out" = "M33 $line_ll$nl" ]

# -p lists the primes whatever -s says, and never asks the exponent check.
run_all -p 33 -s
expect "-p 33 -s exits 0" [ "$status" -eq 0 ]
expect "-p 33 -s prints the primes below 33" [ "$out" = "${below_30}31$nl" ]

# -j and --json print one JSON object per line on stdout in place of each verdict line, in the
# same order, with the same status and the same log on stderr. The records are compared byte for
# byte: keys in a fixed order, spaced as written here, the exponent from its value.
program='"program": {"name": "mps", "version": "'"$version"'"}, "record_version": 1}'
record_tf='"worktype": "tf", "status": "no-verdict", "reason": "trial factoring is not implemented yet", '"$program"
record_p1='"worktype": "p1", "status": "no-verdict", "reason": "P-1 is not implemented yet", '"$program"
record_ll='"worktype": "ll", "status": "no-verdict", "reason": "Lucas-Lehmer is not implemented yet", '"$program"

records_all() { # P: the three no-verdict records for M_P, each ended
	printf '{"exponent": %s, %s\n{"exponent": %s, %s\n{"exponent": %s, %s\n' \
		"$1" "$record_tf" "$1" "$record_p1" "$1" "$record_ll"
}

for typed in 2:2 3:3 31:31 999999929:999999929 999999937:999999937 0000000031:31; do
	pval=${typed%%:*}
	value=${typed#*:}
	records=$(records_all "$value"; echo .)
	records=${records%.}
	for json in -j --json; do
		run_all "$json" "$pval"
		expect "$json $pval exits 42" [ "$status" -eq 42 ]
		expect "$json $pval prints three no-verdict records for $value" [ "$out" = "$records" ]
		run_stderr "$json" "$pval"
		expect "$json $pval logs as without $json" [ "$err" = "$start_all" ]
	done
done

for test_name in tf trial-factor p1 p-minus-1 ll lucas-lehmer all; do
	case $test_name in
		tf|trial-factor)  want=$start_tf records="{\"exponent\": 31, $record_tf$nl" ;;
		p1|p-minus-1)     want=$start_p1 records="{\"exponent\": 31, $record_p1$nl" ;;
		ll|lucas-lehmer)  want=$start_ll records="{\"exponent\": 31, $record_ll$nl" ;;
		all)              want=$start_all records=$(records_all 31; echo .) records=${records%.} ;;
	esac
	run_all -j -t "$test_name" 31
	expect "-j -t $test_name 31 exits 42" [ "$status" -eq 42 ]
	expect "-j -t $test_name 31 prints its record for each test" [ "$out" = "$records" ]
	run_stderr -j -t "$test_name" 31
	expect "-j -t $test_name 31 runs exactly its test" [ "$err" = "$want" ]
done

# The exponent check's answer is a record too, its factor the exact string 2^a-1.
for pair in 4:2 9:3 33:3 999999999:3 999002449:31607; do
	pval=${pair%%:*}
	factor=${pair#*:}
	for test_name in none tf trial-factor p1 p-minus-1 ll lucas-lehmer all; do
		if [ "$test_name" = none ]; then
			set -- -j "$pval"
		else
			set -- -j -t "$test_name" "$pval"
		fi
		run_stderr "$@"
		expect "$* exits 40" [ "$status" -eq 40 ]
		expect "$* runs no test" lacks "$err" "Running"
		run_all "$@"
		expect "$* prints the exponent check's record" [ "$out" = "{\"exponent\": $pval, \"worktype\": \"ptest\", \"status\": \"composite\", \"factors\": [\"2^$factor-1\"], $program$nl" ]
	done
done

for pval in 4 33 999999999 31; do
	records=$(records_all "$pval"; echo .)
	records=${records%.}
	for skip in -s --skip-ptest; do
		run_all -j "$skip" "$pval"
		expect "-j $skip $pval exits 42" [ "$status" -eq 42 ]
		expect "-j $skip $pval prints the tests' records" [ "$out" = "$records" ]
	done
done

# -j is a flag like any other: it groups, stands anywhere, warns when given twice and takes no
# value.
records_31=$(records_all 31; echo .)
records_31=${records_31%.}
records_33=$(records_all 33; echo .)
records_33=${records_33%.}

run_all -jd 31
expect "-jd 31 exits 42" [ "$status" -eq 42 ]
expect "-jd 31 prints the records" [ "$out" = "$records_31" ]
run_all -dj 31
expect "-dj 31 exits 42" [ "$status" -eq 42 ]
expect "-dj 31 prints the records" [ "$out" = "$records_31" ]
run_all -sj 33
expect "-sj 33 exits 42" [ "$status" -eq 42 ]
expect "-sj 33 prints the tests' records" [ "$out" = "$records_33" ]
run_all -js 33
expect "-js 33 exits 42" [ "$status" -eq 42 ]
expect "-js 33 prints the tests' records" [ "$out" = "$records_33" ]
run_all 31 -j
expect "31 -j exits 42" [ "$status" -eq 42 ]
expect "31 -j prints the records" [ "$out" = "$records_31" ]
run_all -jt ll 31
expect "-jt ll 31 exits 42" [ "$status" -eq 42 ]
expect "-jt ll 31 prints the Lucas-Lehmer record" [ "$out" = "{\"exponent\": 31, $record_ll$nl" ]
run_all -j -j 31
expect "-j -j 31 exits 42" [ "$status" -eq 42 ]
expect "-j -j 31 prints the records once" [ "$out" = "$records_31" ]
run_stderr -j -j 31
expect "-j -j 31 warns once" once "$err" "Duplicate use of -j/--json"

run_all --json=1 31
expect "--json=1 31 exits 1" [ "$status" -eq 1 ]
expect "--json=1 31 prints nothing" [ -z "$out" ]

run_all -p 30 -j
expect "-p 30 -j exits 0" [ "$status" -eq 0 ]
expect "-p 30 -j prints the primes below 30" [ "$out" = "$below_30" ]

# The records are never coloured either.
out=$(unset NO_COLOR; FORCE_COLOR=1 "$mps" -j 31 2>/dev/null)
got="stdout '${out%%"$nl"*}'"
expect "-j 31 under FORCE_COLOR prints no escape on stdout" lacks "$out" "$esc"

# The help lists -j, says what -s skips, and gives every exit status; the usage line names -j,
# and mps alone prints it.
usage_line='  mps [-hv] [-p] PVAL [-s] [-t NAME] [-j] [-f PATH] [-d] [-D]'
for part in "$usage_line" \
            '  -s, --skip-ptest        Skip the exponent check (that PVAL is prime).' \
            '  -j, --json              Print each result as one JSON object per line.' \
            'Exit status:' \
            '  0       Prime, or no verdict by design (--help, --version, -p).' \
            '  1       Wrong command-line invocation.' \
            '  2       Configuration error.' \
            '  10      File I/O or formatting error.' \
            '  20      Runtime error.' \
            '  30      Unexpected internal failure.' \
            '  40      Composite.' \
            '  41      Probable prime.' \
            '  42      No verdict.' \
            '  50-59   Reserved for burn-in.'; do
	got="--help without the line '$part'"
	expect "--help has the line $part" has "$nl$help$nl" "$nl$part$nl"
done

run_all
expect "mps alone exits 0" [ "$status" -eq 0 ]
expect "mps alone prints the usage" [ "$out" = "Usage:$nl$usage_line$nl  Check if number is Mersenne prime.$nl" ]

# AGENTS.md names the result format and its paths among the high-blast-radius paths, and both
# sentences that bind a format name a checkpoint or result format of mps.
list_result='- `src/result.hpp`, `src/result.cpp`'
list_main='- `src/main.cpp`'
paragraph='A cycle that adds an arithmetic engine, a test driver that prints a verdict, a checkpoint or result format of `mps`, or a table of known answers adds its paths here in the same commit.'
convention='- A change to a checkpoint or result format of `mps` bumps that format'"'"'s version in the same commit.'
has_list_result=no has_list_main=no has_paragraph=no has_convention=no
agents_source="$here/../AGENTS.md"
if [ -r "$agents_source" ]; then
	while IFS= read -r line; do
		case $line in
			"$list_result") has_list_result=yes ;;
			"$list_main")   has_list_main=yes ;;
			"$paragraph")   has_paragraph=yes ;;
			"$convention")  has_convention=yes ;;
		esac
	done <"$agents_source"
fi
got="AGENTS.md at '$agents_source'"
expect "AGENTS.md lists src/result.hpp and src/result.cpp" [ "$has_list_result" = yes ]
expect "AGENTS.md lists src/main.cpp" [ "$has_list_main" = yes ]
expect "AGENTS.md binds the paths of a result format of mps" [ "$has_paragraph" = yes ]
expect "AGENTS.md bumps the version of a result format of mps" [ "$has_convention" = yes ]

if [ $((passed + failed)) -ne "$planned" ]; then
	failed=$((failed + 1))
	echo " FAIL ran $((passed + failed - 1)) of $planned planned expectations"
fi

echo "cli: $passed passed, $failed failed"
if [ "$failed" -eq 0 ] && [ "$passed" -eq "$planned" ]; then
	exit 0
fi
exit 1
