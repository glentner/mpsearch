#!/bin/sh
# P2 gate: schoolbook and Karatsuba products and squares, with all of P1 (R1, R8-R12).
# make check green on clang++ and g++-16 with every pinned check; the files as PLAN § 2 leaves them;
# bin/mps as c9b82ff's; every vector file reproduced and recomputed; the probe; make check red under
# each bug and each tampered file of phases 1-2; the note and AGENTS.md; R10's time (PLAN § 6).
set -u
PHASE=2
. "$(dirname "$0")/common.sh"

gate_all
echo "P2 gate: green ($GATE_LINE)"
