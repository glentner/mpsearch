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
# Only behaviour that is right today is pinned; the known command-line bugs are recorded
# in `issues/`.
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
planned=14
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


run_stderr() { # ARGS...: stderr into err, the exit status into status; stdout discarded
	err=$("$mps" "$@" 2>&1 >/dev/null)
	status=$?
	got="exit $status, stderr '${err%%"$nl"*}'"
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

# The odd primes below 30; a leading 2 is accepted too.
odd_primes="3${nl}5${nl}7${nl}11${nl}13${nl}17${nl}19${nl}23${nl}29"
run -p 30
expect "-p 30 exits 0" [ "$status" -eq 0 ]
case $out in
	2"$nl"*) out=${out#2"$nl"} ;;
esac
expect "-p 30 prints the odd primes below 30" [ "$out" = "$odd_primes" ]

run_stderr -t ll 31
case "$nl$err$nl" in
	*"$nl    info [mps] Running Lucas-Lehmer test$nl"*) logged=yes ;;
	*)                                                  logged=no ;;
esac
expect "-t ll 31 logs the Lucas-Lehmer test" [ "$logged" = yes ]

run --no-such-option 31
expect "an unknown option exits 1" [ "$status" -eq 1 ]

run -t no-such-test 31
expect "an unknown -t name exits 1" [ "$status" -eq 1 ]

run 31 61
expect "two PVALs exit 1" [ "$status" -eq 1 ]

run 1234567890
expect "a 10-digit PVAL exits 1" [ "$status" -eq 1 ]

if [ $((passed + failed)) -ne "$planned" ]; then
	failed=$((failed + 1))
	echo " FAIL ran $((passed + failed - 1)) of $planned planned expectations"
fi

echo "cli: $passed passed, $failed failed"
if [ "$failed" -eq 0 ] && [ "$passed" -eq "$planned" ]; then
	exit 0
fi
exit 1
