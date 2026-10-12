#!/bin/sh
# Pre-push checks for P3: everything its checklist runs before `git push`, runnable on its own.
#   sh spec/logger-state-per-translation-unit/verify/prepush.sh        the P2 gate on the final tree,
#                                                                      the deferral ledger, the code
#                                                                      committed, and the scan
#   sh spec/logger-state-per-translation-unit/verify/prepush.sh scan   the scan alone
# The scan reads ${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}, first proves on planted lines that
# it catches a node's name after a detailed stamp and passes it redacted, then covers every file the
# cycle touches and the messages and patches of origin/main...HEAD (R7, the deferral ledger).
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
	*) fail "usage: sh prepush.sh [scan]" ;;
esac

build_all
footprint
wording
build_base
same_as_base "$R6_RUNS" R6
same_as_base "$R2_RUNS" R2
detailed "$R1_AFTER_PARSE"
detailed "$R1_WHILE_PARSING"
mutants_setup
p1_mutants
p2_mutants
ledger
code_committed
scan_cycle

echo "prepush: green (P2 gate on the final tree: make check $CHECKS + $CLIS, $SAME runs as $BASE, $DETAILED detailed runs, $MUTANTS defects caught; ledger; code committed at $(cd "$root" && git rev-parse --short HEAD); scan clean)"
