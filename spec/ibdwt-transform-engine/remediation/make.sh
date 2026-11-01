#!/bin/sh
# Gate remediation for P4 (foreman ruling, 2026-10-31). The frozen verify/prepush.sh and verify/p4.sh
# run the cumulative gate at PHASE=3, so files.py refuses the issues/ and ROADMAP.md edits that P4's
# own ledger check requires: no correct tree passes both. remediation/prepush.sh and remediation/p4.sh
# are the frozen scripts with two edits and nothing else:
#   - they source the frozen verify/ files;
#   - after gate.sh, files_check is redefined from the frozen gate.sh's own text, with files.py's
#     `changes` step graded as of phase 4. Every other check, interface, labels, AGENTS.md, the
#     Makefile, the sources, tests/cli.sh and the whole cumulative gate, stays at phase 3.
#   sh spec/ibdwt-transform-engine/remediation/make.sh          regenerate both scripts
#   sh spec/ibdwt-transform-engine/remediation/make.sh check    prove the committed ones are what this
#                                                               makes from the frozen verify/
set -u
here=$(cd "$(dirname "$0")" && pwd)
verify="$here/../verify"
gen() {
	sed -e 's|^\. "\$(dirname "\$0")/\([a-z]*\.sh\)"$|. "$(dirname "$0")/../verify/\1"|' \
	    -e '/^\. "\$(dirname "\$0")\/\.\.\/verify\/gate\.sh"$/r '"$here/override.sh" \
	    "$verify/$1"
}
case ${1:-make} in
	make)
		for s in prepush.sh p4.sh; do gen "$s" >"$here/$s"; done ;;
	check)
		for s in prepush.sh p4.sh; do
			gen "$s" | cmp -s - "$here/$s" || { echo "remediation: $s is not what make.sh makes from verify/$s" >&2; exit 1; }
			test "$(diff "$verify/$s" "$here/$s" | grep -c '^>')" -eq "$(( $(wc -l <"$here/override.sh") + 3 ))" \
				|| { echo "remediation: $s differs from verify/$s by more than the two edits" >&2; exit 1; }
		done
		echo "remediation: prepush.sh and p4.sh are verify/'s with only the two edits" ;;
	*) echo "usage: sh make.sh [check]" >&2; exit 2 ;;
esac
