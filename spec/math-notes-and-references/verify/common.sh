# Shared by this cycle's phase gates (a cycle record, not a standing gate). POSIX sh.
# Each gate runs, in order: the freeze check; notes.py for the phase (the files, the structure,
# the citations, the claims, the clean-room rules, AGENTS.md, the footprint and the records);
# refs.py (every reference against doi.org or its page, over the network); and the profile's
# `make check` on a copy of the tree, which must pass exactly as on the base. It prints
# `GATE FAIL: <reason>` and exits 1 on the first failure, or ends with a `P<n> gate: green` line.

root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "GATE FAIL: not in a git tree" >&2; exit 1; }
slug=math-notes-and-references
spec=spec/$slug
here="$root/$spec/verify"
work=$(mktemp -d "${TMPDIR:-/tmp}/mps-gate.XXXXXX") || { echo "GATE FAIL: mktemp" >&2; exit 1; }
trap 'chmod -R u+rwx "$work" 2>/dev/null; /bin/rm -rf "$work"' EXIT INT TERM

fail() {
	echo "GATE FAIL: $*" >&2
	exit 1
}

need() {
	command -v "$1" >/dev/null 2>&1 || fail "required tool not found: $1"
}

need git; need python3; need rsync
MAKE=/usr/bin/make
"$MAKE" --version 2>/dev/null | head -1 | grep -q '^GNU Make 3\.81' || fail "need GNU Make 3.81 at $MAKE"
python3 -c 'import sys; sys.exit(sys.version_info < (3, 8))' || fail "need Python 3.8 or newer"

# The gates are frozen at the plan commit: this directory must match it, with nothing added.
plan_commit=$(cd "$root" && git log -1 --format=%H --grep="^\[docs\] Plan $slug" -- "$spec/verify")
test -n "$plan_commit" || fail "no plan commit for $spec/verify"
test -z "$(cd "$root" && git log --format=%H --grep="^\[docs\] Build $slug" "$plan_commit")" \
	|| fail "the plan commit $plan_commit comes after a build commit: the gates are frozen before the build"
test -z "$(cd "$root" && git log --format=%H "$plan_commit..HEAD" -- "$spec/verify")" \
	|| fail "a commit after the plan commit $plan_commit touches $spec/verify: the gates are frozen"
( cd "$root" && git diff --quiet "$plan_commit" -- "$spec/verify" ) \
	|| fail "$spec/verify differs from the plan commit $plan_commit: the gates are frozen"
test -z "$(cd "$root" && git ls-files --others -- "$spec/verify")" \
	|| fail "untracked files in $spec/verify: the gates are frozen"

py() {
	python3 -I -B "$@"
}

# make_check: the profile's verify on a copy of the tree as a clone would hold it. The cycle
# changes no code, so the counts are the base's.
make_check() {
	mkdir -p "$work/tree" || fail "mkdir"
	( cd "$root" && git ls-files -z --cached --others --exclude-standard \
		| python3 -I -c 'import os, sys; sys.stdout.write("".join(p + "\0" for p in sys.stdin.read().split("\0") if p and os.path.lexists(p)))' ) \
		| rsync -a --from0 --files-from=- "$root/" "$work/tree/" || fail "copy of the tree"
	( cd "$work/tree" && "$MAKE" check ) >"$work/check.log" 2>&1 || { tail -20 "$work/check.log" >&2; fail "make check"; }
	grep -qx 'check: 55 passed, 0 failed' "$work/check.log" || fail "make check: not 55 checks passed, as on the base"
	grep -qx 'cli: 644 passed, 0 failed' "$work/check.log" || fail "make check: not 644 cli expectations passed, as on the base"
}

# gate N: the phase's checks
gate() {
	py "$here/notes.py" gate "$1" || exit 1
	py "$here/refs.py" || exit 1
	make_check
}
