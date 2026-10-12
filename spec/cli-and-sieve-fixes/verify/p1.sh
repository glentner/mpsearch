#!/bin/sh
# P1 gate: the sieve right at its edges. make check green with clang++ and g++-16; mps -p N lists
# every prime below N, 2 included, and nothing for N <= 2; prime_sieve(n) has n entries, true exactly
# at the primes below n, for n = 0 ... 3000 and large n, and writes nothing outside them under the
# library's bounds assertions; make check goes red under each sieve and listing defect, and with the
# assertions on, under the old out-of-range write; tests/check_math.cpp no longer calls the sieve
# wrong at its edges (R8, R9, R10).
set -u
. "$(dirname "$0")/common.sh"

build_all
sieve_behaviour
mutants_setup
mutants_sieve
hardened_check
caveats tests/check_math.cpp
hygiene

echo "P1 gate: green (make check $CHECKS + $CLIS on clang++ and g++-16; -p and the sieve right from 0; hardened; $MUTANTS mutants caught)"
