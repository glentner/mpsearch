#!/bin/sh
# P2 gate: -D and -d as the parser reads them. All of P1's gate, with cli.cpp differing from 9a41271
# only in the two statements; the messages raised while parsing in the detailed style once -D is
# read, stamped with this host and TASK_ID; make check red under each P1 and P2 defect (R1-R7).
set -u
PHASE=2
. "$(dirname "$0")/common.sh"

build_all
footprint
wording
build_base
same_as_base "$R6_RUNS" R6
same_as_base "$R2_RUNS" R2
detailed "$R1_AFTER_PARSE"
detailed "$R1_WHILE_PARSING"
mutants_setup
p1_mutants
p2_mutants

echo "P2 gate: green (make check $CHECKS + $CLIS on clang++ and g++-16; $SAME runs as $BASE; $DETAILED detailed runs; $MUTANTS defects caught)"
