#!/bin/sh
# P1 gate: results, the verdict line, the record and the statuses. make check green with clang++
# and g++-16; mps on a prime exponent prints one no-verdict line per test, tf, p1, ll, each as its
# test completes, and exits 42; each -t name its one line; the error classes and the 0 of --help,
# --version and -p unchanged; nothing coloured on stdout; the probe's constructed results through
# the line, the record, status_of and the printer (R9's failures included); AGENTS.md reworded in
# the commit that adds the result module; make check red under each P1 mutant; no non--p PVAL
# expected to exit 0 (R1, R2, R6-R10, R12, R13).
set -u
PHASE=1
. "$(dirname "$0")/common.sh"

build_all
p1_behaviour
mutants_setup
p1_mutants
r13_sweep
hygiene

echo "P1 gate: green (make check $CHECKS + $CLIS on clang++ and g++-16; R1, R2, R6-R10, R12 observed; $MUTANTS mutants caught)"
