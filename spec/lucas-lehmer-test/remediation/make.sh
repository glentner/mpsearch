#!/bin/sh
# Gate remediation for review cycle 1 (foreman ruling, 2026-11-01). F2 rewrote AGENTS.md's verify row
# to say what make check runs, and the frozen verify/ll_files.py keeps the old row among the lines
# AGENTS.md must hold, so no correct tree passes the frozen gates. remediation/p3.sh, prepush.sh and
# p4.sh are the frozen scripts with two edits and nothing else:
#   - they source the frozen verify/ files;
#   - after gate.sh, override.sh redefines files_check from the frozen gate.sh's own text with its
#     AGENTS.md step reading remediation/ll_files.py, and grades the file list as of phase 4 (a no-op
#     in prepush.sh and p4.sh, which set it so; the P3 gate needs it once P4's ledger edits issues/).
# remediation/ll_files.py is the frozen verify/ll_files.py with one line changed: the verify row in
# KEPT reads as AGENTS.md's row now does. Every other check of the frozen gates runs as frozen.
#   sh spec/lucas-lehmer-test/remediation/make.sh          regenerate the four files
#   sh spec/lucas-lehmer-test/remediation/make.sh check    prove the committed ones are what this makes
#                                                          from the frozen verify/, and differ from it
#                                                          by the named edits alone
set -u
here=$(cd "$(dirname "$0")" && pwd)
verify="$here/../verify"
OLD='        "| verify | `make check`: the checks and the smoke test, both always run |"]'
NEW='        "| verify | `make check`: the checks, the smoke test and the known-answer runs (`tests/ll.sh`), all always run |"]'
SCRIPTS='p3.sh prepush.sh p4.sh'

gen() {
	sed -e 's|^\. "\$(dirname "\$0")/\([a-z]*\.sh\)"$|. "$(dirname "$0")/../verify/\1"|' \
	    -e '/^\. "\$(dirname "\$0")\/\.\.\/verify\/gate\.sh"$/r '"$here/override.sh" \
	    "$verify/$1"
}

gen_py() {
	awk -v old="$OLD" -v new="$NEW" '{ if ($0 == old) { print new; n++ } else print } END { exit n != 1 }' "$verify/ll_files.py"
}

# ungen FILE: a wrapped script with the override's lines dropped and its sources pointed back
ungen() {
	awk -v n="$(wc -l <"$here/override.sh")" 'skip > 0 { skip--; next } { print } /^\. "\$\(dirname "\$0"\)\/\.\.\/verify\/gate\.sh"$/ { skip = n }' "$1" \
		| sed -e 's|^\. "\$(dirname "\$0")/\.\./verify/\([a-z]*\.sh\)"$|. "$(dirname "$0")/\1"|'
}

case ${1:-make} in
	make)
		for s in $SCRIPTS; do gen "$s" >"$here/$s" || exit 1; done
		gen_py >"$here/ll_files.py" || { echo "remediation: the verify row is not once in verify/ll_files.py" >&2; exit 1; } ;;
	check)
		for s in $SCRIPTS; do
			gen "$s" | cmp -s - "$here/$s" || { echo "remediation: $s is not what make.sh makes from verify/$s" >&2; exit 1; }
			ungen "$here/$s" | cmp -s - "$verify/$s" || { echo "remediation: $s differs from verify/$s by more than the two edits" >&2; exit 1; }
			test "$(grep -c '/\.\./verify/' "$here/$s")" -eq "$(grep -c '^\. "\$(dirname "\$0")/' "$verify/$s")" \
				|| { echo "remediation: $s does not source every frozen file from verify/" >&2; exit 1; }
		done
		gen_py | cmp -s - "$here/ll_files.py" || { echo "remediation: ll_files.py is not what make.sh makes from verify/ll_files.py" >&2; exit 1; }
		test "$(diff "$verify/ll_files.py" "$here/ll_files.py" | grep -c '^[<>]')" -eq 2 \
			&& diff "$verify/ll_files.py" "$here/ll_files.py" | grep -qxF "< $OLD" \
			&& diff "$verify/ll_files.py" "$here/ll_files.py" | grep -qxF "> $NEW" \
			|| { echo "remediation: ll_files.py differs from verify/ll_files.py by more than the verify row" >&2; exit 1; }
		echo "remediation: p3.sh, prepush.sh and p4.sh are verify/'s with only the two edits, and ll_files.py verify/'s with only the verify row" ;;
	*) echo "usage: sh make.sh [check]" >&2; exit 2 ;;
esac
