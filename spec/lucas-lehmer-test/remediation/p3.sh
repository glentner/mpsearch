#!/bin/sh
# P3 gate: the long tier, the landing gate and the remaining notes (R16-R18, and P1's and P2's).
#   sh spec/lucas-lehmer-test/verify/p3.sh          the full gate (common.sh's header)
#   sh spec/lucas-lehmer-test/verify/p3.sh quick    the cheap partial mode
set -u
PHASE=3
GATE_MODE=${1:-full}
. "$(dirname "$0")/../verify/common.sh"
. "$(dirname "$0")/../verify/gate.sh"
# Foreman ruling (2026-11-01, remediation/make.sh): F2 rewrote AGENTS.md's verify row; the agents step
# reads remediation/ll_files.py, the frozen ll_files.py with only that pinned line as the row reads now.
# The file list is graded as of phase 4, as verify/prepush.sh and verify/p4.sh grade it already.
CHANGES_PHASE=4
_fc=$(sed -n '/^files_check() {$/,/^}$/p' "$here/gate.sh" | sed 's|"\$here/ll_files\.py" agents|"$root/$spec/remediation/ll_files.py" agents|')
case $_fc in *'"$root/$spec/remediation/ll_files.py" agents '*) eval "$_fc" ;; *) die "remediation: files_check not found in gate.sh" ;; esac
sh "$root/$spec/remediation/make.sh" check >/dev/null || die "remediation: the wrapped gates drifted from verify/ (make.sh check)"

gate_all
gate_end "P3 gate"
