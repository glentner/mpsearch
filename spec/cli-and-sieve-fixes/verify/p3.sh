#!/bin/sh
# P3 gate: the options parse as --help says. Everything P1 and P2 prove, plus: the help text PLAN
# gives, intact in colour; each -t name and alias runs exactly its test; any other -t value is an
# argument error naming it; the five spellings of -t and -f set exactly VALUE; grouped boolean
# flags act as if given separately, in order; parsing creates no file; make check goes red under
# each option defect; no file under tests/ describes the defects as known (R1-R10).
set -u
. "$(dirname "$0")/common.sh"

build_all
sieve_behaviour
pval_behaviour
options_behaviour
mutants_setup
mutants_sieve
mutants_pval
mutants_options
hardened_check
caveats_tests
hygiene

echo "P3 gate: green (make check $CHECKS + $CLIS on clang++ and g++-16; R1-R9 observed; $MUTANTS mutants caught; no caveats in tests/)"
