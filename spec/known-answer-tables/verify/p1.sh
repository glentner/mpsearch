#!/bin/sh
# P1 gate: the format, the loader, the exponent table and R6 (R1-R6, R10-R12).
# The tables' commands under two Pythons and changed; make check green on clang++ and g++-16 with
# every pinned check; the files as PLAN § 2 leaves them; bin/mps as c41950c's; the tables against
# PLAN § 2's rows; the probe; make check red under each loader bug and each table edit of phases
# 1-1; the documents; R11's time (PLAN § 6).
set -u
PHASE=1
. "$(dirname "$0")/common.sh"

gate_all
echo "P1 gate: green ($GATE_LINE)"
