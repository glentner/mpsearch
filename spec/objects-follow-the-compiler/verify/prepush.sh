#!/bin/sh
# Pre-push checks for P2: everything its checklist runs before `git push`, runnable on its own.
#   sh spec/objects-follow-the-compiler/verify/prepush.sh        the deferral ledger, the code
#                                                                committed, the scan, then the P1
#                                                                gate on the final tree
#   sh spec/objects-follow-the-compiler/verify/prepush.sh scan   the scan alone
# The scan reads ${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}, first proves on planted lines that
# it catches a node's name after a detailed stamp and passes it redacted, then covers every file the
# cycle touches and the messages and patches of origin/main...HEAD.
set -u
PHASE=2
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/cluster.sh"

case ${1:-all} in
	scan)
		scan_cycle
		echo "prepush: scan clean (self-test passed)"
		exit 0
		;;
	all) ;;
	*) die "usage: sh prepush.sh [scan]" ;;
esac

ledger
code_committed
scan_cycle
gate_all

gate_end "prepush" >/dev/null
echo "prepush: green (ledger; code committed at $(cd "$root" && git rev-parse --short HEAD); scan clean; the P1 gate on the final tree: $GATE_LINE)"
