#!/bin/sh
# P2 gate: residues: the digit form, the squaring with its subtraction, the product and the monitor (R1, R4, R6, R7, R9, R14, and P1's).
#   sh spec/ibdwt-transform-engine/verify/p2.sh          the full gate (common.sh's header)
#   sh spec/ibdwt-transform-engine/verify/p2.sh quick    the cheap partial mode
set -u
PHASE=2
GATE_MODE=${1:-full}
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/gate.sh"

gate_all
gate_end "P2 gate"
