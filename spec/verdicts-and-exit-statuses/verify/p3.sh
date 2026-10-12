#!/bin/sh
# P3 gate: -j/--json and the help. Everything P1 and P2 prove, plus: under -j each result is one
# JSON record, parsed strictly and byte-equal to the record PLAN gives, with the status and stderr
# of the plain run; -j groups and warns like any flag; --help and the usage exactly as PLAN gives
# them; the parser, the help and the usage in one commit; make check red under the -j mutants and
# the help edits (R10, R11, R13).
set -u
PHASE=3
. "$(dirname "$0")/common.sh"

build_all
p1_behaviour
p2_behaviour
p3_behaviour
mutants_setup
p1_mutants
p2_mutants
p3_mutants
r13_sweep
hygiene

echo "P3 gate: green (make check $CHECKS + $CLIS on clang++ and g++-16; R1-R12 observed; $MUTANTS mutants caught)"
