#!/bin/sh
# P2 gate: the exponent check. Everything P1 proves, plus: a composite PVAL runs no test and is
# answered by one line naming 2^a-1, a the smallest prime factor, under every -t name, exit 40; a
# prime PVAL prints nothing for the check; -s/--skip-ptest runs the tests on any PVAL; every PVAL
# from 2 to 2000 against trial division; the probe's re-check refusals; make check red under each
# exponent-check mutant (R3, R4, R5, R11's -s line, R13).
set -u
PHASE=2
. "$(dirname "$0")/common.sh"

build_all
p1_behaviour
p2_behaviour
mutants_setup
p1_mutants
p2_mutants
r13_sweep
hygiene

echo "P2 gate: green (make check $CHECKS + $CLIS on clang++ and g++-16; R1-R9, R12 observed; $MUTANTS mutants caught)"
