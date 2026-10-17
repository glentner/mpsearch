#!/bin/sh
# Pre-push checks for P5: everything its checklist runs before `git push`, runnable on its own.
#   sh spec/reference-big-integer/verify/prepush.sh        the P4 gate on the final tree, the deferral
#                                                          ledger, the code committed, and the scan
#   sh spec/reference-big-integer/verify/prepush.sh scan   the scan alone
# The scan reads ${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}, first proves on planted lines that
# it catches a node's name after a detailed stamp and passes it redacted, then covers every file the
# cycle touches and the messages and patches of origin/main...HEAD.
set -u
PHASE=4
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/cluster.sh"

case ${1:-all} in
	scan)
		scan_cycle
		echo "prepush: scan clean (self-test passed)"
		exit 0
		;;
	all) ;;
	*) fail "usage: sh prepush.sh [scan]" ;;
esac

gate_all
ledger
code_committed
scan_cycle

echo "prepush: green (P4 gate on the final tree: $GATE_LINE; ledger; code committed at $(cd "$root" && git rev-parse --short HEAD); scan clean)"
