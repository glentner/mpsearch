#!/bin/sh
# P2 remediation gate (review cycle 1, F2): a HASH that does not name exactly one commit gets a
# message that says why. A prefix two commits share is refused as ambiguous, naming the hash and its
# commits and never saying "not a commit", under dash and sh, and a branch named like the prefix
# changes nothing. A prefix one commit shares with a blob, or with an annotated tag, checks out that
# commit. A blob's or a tag's own hash, an absent hash and a malformed one still say "not a commit"
# (R2, R3). The colliding objects were found offline by a birthday search over fixed object bodies,
# so they are the same on every run; the gate checks that before it relies on them.
set -u
. "$(dirname "$0")/../verify/common.sh"
need git; need dash; need clang++; need python3; need rsync
require_job
make_stubs
make_fixture

r="$work/repo.git"
sig='gate <gate@invalid> 1700000000 +0000'
et=$(git -C "$r" hash-object -w -t tree /dev/null) || fail "write the empty tree"

# mk_commit TREE MESSAGE: a commit with fixed metadata, written to the bare repository
mk_commit() {
	printf 'tree %s\nauthor %s\ncommitter %s\n\n%s\n' "$1" "$sig" "$sig" "$2" \
		| git -C "$r" hash-object -w -t commit --stdin
}

# want_obj GOT WANT WHAT: the object written has the precomputed name
want_obj() {
	test "$1" = "$2" || fail "fixture $3 is '$1', want $2: the precomputed collision does not hold"
}

# prefix_objects REPO PREFIX: each object in REPO whose name begins with PREFIX, with its type
prefix_objects() {
	git -C "$1" cat-file --batch-check='%(objectname) %(objecttype)' --batch-all-objects \
		| grep "^$2" | sort
}

nl='
'

# Two commits sharing a seven-digit prefix; they first differ in the eighth digit.
amb=037d766
amb_a=037d766af4af67064990dba5d7ea6c14983e3b4a
amb_b=037d766d54790918cbeb665afb246f1a7e983d22
want_obj "$(mk_commit "$et" 'collision 2820')" "$amb_a" "commit A"
want_obj "$(mk_commit "$et" 'collision 24658')" "$amb_b" "commit B"

# One commit and one blob sharing a prefix; the blob is reachable through a holder commit's tree.
cb=24f047a
cb_commit=24f047adbef723202f1e62ee0ba7776b9d9f826a
cb_blob=24f047aedbea43aa0c13a918d984de2594f48e6c
want_obj "$(mk_commit "$et" 'collision 35684')" "$cb_commit" "commit beside a blob"
want_obj "$(printf 'blob 1216\n' | git -C "$r" hash-object -w --stdin)" "$cb_blob" "blob"
holder_tree=$(printf '100644 blob %s\tf\n' "$cb_blob" | git -C "$r" mktree) || fail "holder tree"
holder=$(mk_commit "$holder_tree" holder) || fail "holder commit"

# One commit and one annotated tag object sharing a prefix; the tag points at a third commit.
ct=4848684
ct_commit=4848684754ec2c636213e2bcc18de3612da197d4
ct_tag=4848684f17cf4651fb79876c597cf5e319b0602d
base=$(mk_commit "$et" base) || fail "base commit"
want_obj "$base" c1d793fe909d21d804ea758ed9ac8755da12af29 "tagged commit"
want_obj "$(mk_commit "$et" 'collision 176564')" "$ct_commit" "commit beside a tag"
want_obj "$(printf 'object %s\ntype commit\ntag gate-715\ntagger %s\n\ntag 715\n' "$base" "$sig" \
	| git -C "$r" hash-object -w -t tag --stdin)" "$ct_tag" "tag object"

for ref in "refs/heads/gate-amb-a $amb_a" "refs/heads/gate-amb-b $amb_b" "refs/heads/gate-cb $cb_commit" \
	"refs/heads/gate-blob-holder $holder" "refs/heads/gate-ct $ct_commit" "refs/tags/gate-715 $ct_tag"; do
	git -C "$r" update-ref ${ref} || fail "fixture ref $ref"
done

# The same repository with a branch named like the shared prefix, pointing at one of the two.
git clone -q --mirror "$r" "$work/trap.git" || fail "mirror the fixture"
git -C "$work/trap.git" update-ref "refs/heads/$amb" "$amb_a" || fail "fixture branch $amb"

# Each prefix names exactly the objects the cases below assume, and nothing else.
test "$(prefix_objects "$r" "$amb")" = "$amb_a commit${nl}$amb_b commit" || fail "prefix $amb does not name exactly two commits"
test "$(prefix_objects "$r" "$cb")" = "$cb_commit commit${nl}$cb_blob blob" || fail "prefix $cb does not name exactly a commit and a blob"
test "$(prefix_objects "$r" "$ct")" = "$ct_commit commit${nl}$ct_tag tag" || fail "prefix $ct does not name exactly a commit and a tag"
test "$(prefix_objects "$work/trap.git" "$amb")" = "$amb_a commit${nl}$amb_b commit" || fail "the trap mirror changed prefix $amb"

# expect_red NAME SHELL JOBID HASH FAILED RAN [KEY=VALUE...]: exit 1, result.json with FAILED failed
# and the rest not run, logs for the steps in RAN only, contained, no interpreter run.
expect_red() {
	er_n=$1 er_sh=$2 er_j=$3 er_h=$4 er_f=$5 er_ran=$6
	shift 6
	run_job "$er_n" "$er_n" "$er_sh" "$er_j" "$er_h" local-stub
	test "$rc" -eq 1 || { show "$er_n"; fail "$er_n: exited $rc, want 1 (R7)"; }
	er_r=$(rundir "$er_n" "$er_h" local-stub "$er_j")
	result_check "$er_r/result.json" failed="$er_f" requested="$er_h" target=local-stub job_id="$er_j" \
		not_before="$t_before" not_after="$t_after" "$@"
	expect_logs "$er_r" "$er_ran"
	contained "$er_n" "$er_r"
	forbidden_none
}

# expect_ambiguous NAME: the run refused the shared prefix as ambiguous, naming it and both commits,
# on stderr and in checkout.log, and never called it "not a commit".
expect_ambiguous() {
	ea_log="$(rundir "$1" "$amb" local-stub "$2")/checkout.log"
	for ea_f in "$logs/$1.err" "$ea_log"; do
		grep -F "$amb" "$ea_f" | grep -qi ambiguous || { show "$1"; fail "$1: ${ea_f##*/} does not call $amb ambiguous (R3)"; }
		grep -qF "$amb_a" "$ea_f" && grep -qF "$amb_b" "$ea_f" || { show "$1"; fail "$1: ${ea_f##*/} does not name both commits"; }
		! grep -q 'not a commit' "$ea_f" || { show "$1"; fail "$1: ${ea_f##*/} calls an ambiguous prefix 'not a commit' (R3)"; }
	done
}

# expect_not_a_commit NAME HASH: the R3 message, naming the hash, on stderr
expect_not_a_commit() {
	grep 'not a commit' "$logs/$1.err" | grep -qF "$2" || { show "$1"; fail "$1: stderr does not say '$2 ... not a commit' (R3)"; }
}

# A prefix two commits share: refused at checkout, under dash and sh, and with a branch of that name.
reset_g
expect_red amb-dash dash 6001 "$amb" checkout "clone checkout" commit=null
expect_ambiguous amb-dash 6001
reset_g
expect_red amb-sh sh 6002 "$amb" checkout "clone checkout" commit=null
expect_ambiguous amb-sh 6002
reset_g; G_URL="file://$work/trap.git"
expect_red amb-branch dash 6003 "$amb" checkout "clone checkout" commit=null
expect_ambiguous amb-branch 6003

# One digit more names one commit: checked out, then the empty tree has no target file.
reset_g
expect_red amb-longer dash 6004 "${amb_a%"${amb_a#????????}"}" toolchain "clone checkout toolchain" commit="$amb_a"

# A prefix one commit shares with a blob, or with a tag, names that commit.
reset_g
expect_red commit-blob dash 6005 "$cb" toolchain "clone checkout toolchain" commit="$cb_commit"
grep -q 'no target file' "$logs/commit-blob.err" || { show commit-blob; fail "commit-blob: did not reach the toolchain step"; }
reset_g
expect_red commit-tag dash 6006 "$ct" toolchain "clone checkout toolchain" commit="$ct_commit"
grep -q 'no target file' "$logs/commit-tag.err" || { show commit-tag; fail "commit-tag: did not reach the toolchain step"; }

# A blob's and a tag's own names are not commits (the tag points at one); nor is an absent prefix.
reset_g
expect_red blob-full dash 6007 "$cb_blob" checkout "clone checkout" commit=null
expect_not_a_commit blob-full "$cb_blob"
reset_g
expect_red tag-full dash 6008 "$ct_tag" checkout "clone checkout" commit=null
expect_not_a_commit tag-full "$ct_tag"
absent=
for c in abcdef0 1234567 fedcba9 7654321 0f0f0f0; do
	test -z "$(prefix_objects "$r" "$c")" && { absent=$c; break; }
done
test -n "$absent" || fail "no absent short hash candidate"
reset_g
expect_red absent-short dash 6009 "$absent" checkout "clone checkout" commit=null
expect_not_a_commit absent-short "$absent"

# Malformed: too short (six digits of the shared prefix) or upper case; exit 2, nothing created.
i=0
for h in "${amb%?}" 037D766; do
	i=$((i + 1))
	reset_g
	run_job "malformed-$i" "malformed-$i" dash "601$i" "$h" local-stub
	test "$rc" -eq 2 || { show "malformed-$i"; fail "malformed hash '$h': exited $rc, want 2 (R4)"; }
	expect_not_a_commit "malformed-$i" "$h"
	contained "malformed-$i"
done

echo "P2 F2 gate: green (a shared prefix ambiguous under dash, sh and a same-named branch; a longer prefix, a commit beside a blob and a commit beside a tag checked out; a blob, a tag, an absent and two malformed hashes not a commit)"
