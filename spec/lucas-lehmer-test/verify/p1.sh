#!/bin/sh
# P1 gate: the Jacobi symbol and the stop rule (R5, R15, R18).
#   sh spec/lucas-lehmer-test/verify/p1.sh          the full gate (common.sh's header)
#   sh spec/lucas-lehmer-test/verify/p1.sh quick    the cheap partial mode
set -u
PHASE=1
GATE_MODE=${1:-full}
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/gate.sh"

gate_all
gate_end "P1 gate"
