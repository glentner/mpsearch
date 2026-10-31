#!/bin/sh
# P3 gate: squaring chains against the known answers, and the two tiers' budgets (R5, R12, and P1's and P2's).
#   sh spec/ibdwt-transform-engine/verify/p3.sh          the full gate (common.sh's header)
#   sh spec/ibdwt-transform-engine/verify/p3.sh quick    the cheap partial mode
set -u
PHASE=3
GATE_MODE=${1:-full}
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/gate.sh"

gate_all
gate_end "P3 gate"
