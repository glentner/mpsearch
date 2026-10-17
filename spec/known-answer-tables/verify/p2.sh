#!/bin/sh
# P2 gate: the final residues, with all of P1 (R1-R7, R9-R12).
# The tables' commands under two Pythons and changed; make check green on clang++ and g++-16 with
# every pinned check; the files as PLAN § 2 leaves them; bin/mps as c41950c's; the tables against
# PLAN § 2's rows; the probe; make check red under each loader bug and each table edit of phases
# 1-2; the documents; R11's time (PLAN § 6).
set -u
PHASE=2
. "$(dirname "$0")/common.sh"

gate_all
echo "P2 gate: green ($GATE_LINE)"
