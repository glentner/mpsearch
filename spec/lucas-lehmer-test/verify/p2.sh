#!/bin/sh
# P2 gate: the test: the shift, the checks, the recovery and the result (R1-R4, R6-R14, R16, R18, and P1's).
#   sh spec/lucas-lehmer-test/verify/p2.sh          the full gate (common.sh's header)
#   sh spec/lucas-lehmer-test/verify/p2.sh quick    the cheap partial mode
set -u
PHASE=2
GATE_MODE=${1:-full}
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/gate.sh"

gate_all
gate_end "P2 gate"
