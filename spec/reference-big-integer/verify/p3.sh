#!/bin/sh
# P3 gate: division, general reduction, powers and decimal, with all of P1-P2 (R2, R3, R5, R8-R12).
# make check green on clang++ and g++-16 with every pinned check; the files as PLAN § 2 leaves them;
# bin/mps as c9b82ff's; every vector file reproduced and recomputed; the probe; make check red under
# each bug and each tampered file of phases 1-3; the note and AGENTS.md; R10's time (PLAN § 6).
set -u
PHASE=3
. "$(dirname "$0")/common.sh"

gate_all
echo "P3 gate: green ($GATE_LINE)"
