#!/bin/sh
# Pre-push checks for P4, dispatch A: everything its checklist runs before `git push`, runnable alone.
#   sh spec/lucas-lehmer-test/verify/prepush.sh        the deferral ledger's Notes bullets, the code
#                                                      committed, the scan, the P3 gate in full mode on
#                                                      the final tree (its file list graded as of P4),
#                                                      then the landing gate's run mode on HEAD (about
#                                                      60 min in all); its last two lines carry R16's two
#                                                      times and the landing gate's green line, which
#                                                      dispatch B quotes in evidence/ledger.md
#   sh spec/lucas-lehmer-test/verify/prepush.sh scan   the scan alone
# The scan reads ${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}, first proves on planted lines that
# it catches a node's name after a detailed stamp and passes it redacted, then covers every file the
# cycle touches and the messages and patches of origin/main...HEAD.
set -u
PHASE=3
GATE_MODE=full
. "$(dirname "$0")/../verify/common.sh"
# Set after common.sh, which clears both from the caller's environment
CHANGES_PHASE=4
GATE_NO_ENGINE_RUN=1
. "$(dirname "$0")/../verify/gate.sh"
# Foreman ruling (2026-11-01, remediation/make.sh): F2 rewrote AGENTS.md's verify row; the agents step
# reads remediation/ll_files.py, the frozen ll_files.py with only that pinned line as the row reads now.
# The file list is graded as of phase 4, as verify/prepush.sh and verify/p4.sh grade it already.
CHANGES_PHASE=4
_fc=$(sed -n '/^files_check() {$/,/^}$/p' "$here/gate.sh" | sed 's|"\$here/ll_files\.py" agents|"$root/$spec/remediation/ll_files.py" agents|')
case $_fc in *'"$root/$spec/remediation/ll_files.py" agents '*) eval "$_fc" ;; *) die "remediation: files_check not found in gate.sh" ;; esac
sh "$root/$spec/remediation/make.sh" check >/dev/null || die "remediation: the wrapped gates drifted from verify/ (make.sh check)"
. "$(dirname "$0")/../verify/cluster.sh"

case ${1:-all} in
	scan)
		scan_cycle
		echo "prepush: scan clean (self-test passed)"
		exit 0
		;;
	all) ;;
	*) die "usage: sh prepush.sh [scan]" ;;
esac

ledger_notes
code_committed
scan_cycle
gate_all
pp_head=$(cd "$root" && git rev-parse HEAD)
EG_LINE=
engine_gate_run "$pp_head"
EG_HEAD=$EG_LINE
if [ -n "$SELFTEST" ]; then
	echo "prepush (selftest): times for ## Times: - make check: $TIMES; - make check-long: $LONG s on clang++ (budget $LONG_BUDGET s)"
	echo "prepush (selftest): landing gate for ## Landing gate: - $EG_HEAD"
fi
gate_end "prepush" >/dev/null
echo "prepush: green (ledger notes; code committed at $(cd "$root" && git rev-parse --short HEAD); scan clean; the P3 gate on the final tree: $GATE_LINE)"
echo "prepush: times for ## Times: - make check: $TIMES; - make check-long: $LONG s on clang++ (budget $LONG_BUDGET s)"
echo "prepush: landing gate for ## Landing gate: - $EG_HEAD"
