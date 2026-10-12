#!/bin/sh
# P1 gate: one logger state. make check green on clang++ and g++-16, with no detailed line and no
# host name in its output; the files as PLAN § 2 leaves them, log.hpp differing from 9a41271 only in
# the five declarations made inline; every run without -D, and every -D read too late, identical to
# 9a41271's, colour and TASK_ID included; the messages raised after parsing in the detailed style,
# stamped with this host and TASK_ID; make check red under each P1 defect (R1, R3-R7).
set -u
PHASE=1
. "$(dirname "$0")/common.sh"

build_all
footprint
wording
build_base
same_as_base "$R6_RUNS" R6
same_as_base "$R2_RUNS" R2
detailed "$R1_AFTER_PARSE"
mutants_setup
p1_mutants

echo "P1 gate: green (make check $CHECKS + $CLIS on clang++ and g++-16; $SAME runs as $BASE; $DETAILED detailed runs; $MUTANTS defects caught)"
