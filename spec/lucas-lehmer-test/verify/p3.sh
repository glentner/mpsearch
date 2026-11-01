#!/bin/sh
# P3 gate: the long tier, the landing gate and the remaining notes (R16-R18, and P1's and P2's).
#   sh spec/lucas-lehmer-test/verify/p3.sh          the full gate (common.sh's header)
#   sh spec/lucas-lehmer-test/verify/p3.sh quick    the cheap partial mode
set -u
PHASE=3
GATE_MODE=${1:-full}
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/gate.sh"

gate_all
gate_end "P3 gate"
