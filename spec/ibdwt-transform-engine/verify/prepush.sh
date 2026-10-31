#!/bin/sh
# Pre-push checks for P4: everything its checklist runs before `git push`, runnable on its own.
#   sh spec/ibdwt-transform-engine/verify/prepush.sh        the deferral ledger, the code committed,
#                                                           the scan, then the P3 gate in full mode
#                                                           on the final tree (about 50 min); its
#                                                           green line carries R12's two times,
#                                                           which `## Times` quotes
#   sh spec/ibdwt-transform-engine/verify/prepush.sh scan   the scan alone
# The scan reads ${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}, first proves on planted lines that
# it catches a node's name after a detailed stamp and passes it redacted, then covers every file the
# cycle touches and the messages and patches of origin/main...HEAD.
set -u
PHASE=3
GATE_MODE=full
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/gate.sh"
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

ledger_pre() {
	# the ledger as far as it exists before the runs: the Notes bullets and the seed index; the
	# evidence's ## Times comes after this run
	( cd "$root" || exit 1
	  for p in issues/large-gcd-and-jacobi.md issues/cpu-transform-throughput.md issues/engine-differential-standing-check.md; do
		grep -q 'deferred from `ibdwt-transform-engine`' "$p" || { echo "$p has no Notes bullet for this cycle (TECH P4)"; exit 1; }
	  done
	  test "$(ls issues/*.md | wc -l | tr -d ' ')" -eq "$(grep -c '^\*\*Seed:\*\*' ROADMAP.md)" || { echo "ROADMAP's Seed lines do not match issues/"; exit 1; }
	) >"$work/ledger" 2>&1 || { cat "$work/ledger" >&2; fail "deferral ledger"; }
}

ledger_pre
code_committed
scan_cycle
gate_all

gate_end "prepush" >/dev/null
echo "prepush: green (ledger; code committed at $(cd "$root" && git rev-parse --short HEAD); scan clean; the P3 gate on the final tree: $GATE_LINE)"
echo "prepush: times for ## Times: - make check: $TIMES; - make check-engine: $HEAVY s on clang++ (budget $HEAVY_BUDGET s)"
