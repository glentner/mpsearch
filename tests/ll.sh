#!/bin/sh
# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: Apache-2.0
#
# The Lucas-Lehmer test's known answers, through the command line
#
# Usage: sh tests/ll.sh BINARY [long]
#
# Runs `BINARY -t ll p` for every odd exponent of tests/known/mersenne-exponents.txt up to 10000,
# expecting a prime with a zero Res64 and exit 0, and for every ll row of
# tests/known/final-residues.txt up to 20000, expecting a composite with the row's Res64 and exit
# 40. With `long`, the bounds are 100000 and 200000, the long tier. A row a later row of the same
# exponent supersedes gives way to it. The verdict line holds a shift chosen at random, so it is
# compared up to its Res64. Shell built-ins only, in tests/cli.sh's style: each verdict line is
# printed as it comes, each failed expectation prints ` FAIL <name>`, and the last line is
# `ll: <passed> passed, <failed> failed`. The exit status is 0 only when every expectation passed
# and at least one ran.
#

set -u

if [ $# -eq 1 ]; then
	largest_prime=10000
	largest_row=20000
elif [ $# -eq 2 ] && [ "$2" = long ]; then
	largest_prime=100000
	largest_row=200000
else
	echo "usage: sh tests/ll.sh BINARY [long]" >&2
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


starts() { # TEXT PART: TEXT starts with PART
	case $1 in
		"$2"*) return 0 ;;
	esac
	return 1
}


run_ll() { # P STATUS VERDICT RES64: runs -t ll P, prints its line, and expects STATUS, VERDICT and RES64
	out=$("$mps" -t ll "$1" 2>/dev/null </dev/null)
	status=$?
	[ -z "$out" ] || echo "$out"
	got="exit $status, stdout '$out'"
	expect "-t ll $1 exits $2" [ "$status" -eq "$2" ]
	expect "-t ll $1 reports M$1 $3 with Res64 $4" starts "$out" "M$1 ll: $3, res64 $4, "
}


# The Mersenne prime exponents, `mersenne <p>`, ascending
while read -r op p rest; do
	[ "$op" = mersenne ] || continue
	[ "$p" -le "$largest_prime" ] || break
	[ $((p % 2)) -eq 1 ] || continue
	run_ll "$p" 0 prime 0000000000000000
done <"$here/known/mersenne-exponents.txt"

# The final residues, `ll <p> <Res64> ...`: the last row of each exponent holds its answer
answers=' '
while read -r op p res64 rest; do
	[ "$op" = ll ] || continue
	answers="$answers$p:$res64 "
done <"$here/known/final-residues.txt"

ran=' '
while read -r op p res64 rest; do
	[ "$op" = ll ] && [ "$p" -le "$largest_row" ] || continue
	case $ran in
		*" $p "*) continue ;;
	esac
	ran="$ran$p "
	answer=${answers##*" $p:"}
	run_ll "$p" 40 composite "${answer%% *}"
done <"$here/known/final-residues.txt"

if [ $((passed + failed)) -eq 0 ]; then
	failed=1
	echo " FAIL no known answer ran"
fi

echo "ll: $passed passed, $failed failed"
if [ "$failed" -eq 0 ]; then
	exit 0
fi
exit 1
