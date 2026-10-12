#!/bin/sh
# P2 gate: PVAL and -p N validated. Everything P1 proves, plus: a PVAL that is not one or more ASCII
# digits from 2 (0 with -p) to 999,999,999 is an argument error, exit 1, naming PVAL and the value,
# with no "Uncaught exception", no test run and (with -p) nothing on stdout; leading zeros are
# allowed; -- ends the options; make check goes red under each PVAL defect (R6, R7, R8, R10).
set -u
. "$(dirname "$0")/common.sh"

build_all
sieve_behaviour
pval_behaviour
mutants_setup
mutants_sieve
mutants_pval
hardened_check
caveats tests/check_math.cpp
hygiene

echo "P2 gate: green (make check $CHECKS + $CLIS on clang++ and g++-16; sieve, PVAL and -p N right; $MUTANTS mutants caught)"
