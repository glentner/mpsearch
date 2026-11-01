#!/bin/sh
# P1 gate: the transform, the length table and its measurement (R2, R3, R8, R10, R11, R13).
#   sh spec/ibdwt-transform-engine/verify/p1.sh          the full gate (common.sh's header)
#   sh spec/ibdwt-transform-engine/verify/p1.sh quick    the cheap partial mode
set -u
PHASE=1
GATE_MODE=${1:-full}
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/gate.sh"

gate_all
gate_end "P1 gate"
