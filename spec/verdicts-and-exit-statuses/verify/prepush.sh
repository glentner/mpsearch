#!/bin/sh
# Pre-push checks for P4: everything its checklist runs before `git push`, runnable on its own.
#   sh spec/verdicts-and-exit-statuses/verify/prepush.sh        the P3 gate on the final tree, the
#                                                               deferral ledger, the code committed,
#                                                               and the site-private scan
#   sh spec/verdicts-and-exit-statuses/verify/prepush.sh scan   the site-private scan alone
# The scan reads ${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env} and covers every file the cycle
# touches and the messages and patches of origin/main...HEAD (R13, the deferral ledger).
set -u
PHASE=3
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/cluster.sh"

case ${1:-all} in
	scan)
		scan_cycle
		echo "prepush: scan clean"
		exit 0
		;;
	all) ;;
	*) fail "usage: sh prepush.sh [scan]" ;;
esac

build_all
p1_behaviour
p2_behaviour
p3_behaviour
mutants_setup
p1_mutants
p2_mutants
p3_mutants
r13_sweep
hygiene
ledger
code_committed
scan_cycle

echo "prepush: green (P1-P3 on the final tree, $MUTANTS mutants caught; ledger; code committed at $(cd "$root" && git rev-parse --short HEAD); scan clean)"
