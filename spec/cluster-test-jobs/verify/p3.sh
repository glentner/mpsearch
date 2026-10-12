#!/bin/sh
# P3 gate: the three target files follow the grammar, read the same sourced or line-parsed, carry
# the settings the clusters need, and pass the job's own parser up to the module load; the job names
# no target and executes no target line; nothing this cycle adds or says in its history names an
# account, QoS, user, cluster or site path (R8, R9, R11 targets). The site literals come from
# ${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}, outside the repository; the gate fails without it.
set -u
. "$(dirname "$0")/common.sh"
need git; need dash; need clang++; need python3; need rsync
require_job

# The directory holds the job and the three targets, nothing else.
test "$(cd "$root/ci" && ls -A | tr '\n' ' ')" = "targets test-commit.sh " || fail "ci/ holds more than test-commit.sh and targets/"
test "$(cd "$TARGETS" && ls -A | tr '\n' ' ')" = "aarch64-gb10 x86-64-avx2 x86-64-avx512 " \
	|| fail "ci/targets/ holds $(cd "$TARGETS" && ls -A | tr '\n' ' '), want $REAL_TARGETS (R11)"
hit=$(grep -niE 'x86-64|aarch64|avx|gb10|spark' "$JOB" || true)
test -z "$hit" || { echo "$hit" >&2; fail "the job names a target or partition: adding a target must not touch it (R8)"; }

# A parser that executes nothing, for the equivalence check: KEY=VALUE per key, <unset> when absent.
line_parse() {
	for k in $TARGET_KEYS; do
		if grep -q "^$k=" "$1"; then echo "$k=$(target_value "$1" "$k")"; else echo "$k=<unset>"; fi
	done
}
cat >"$work/source.sh" <<'EOF'
set -eu
. "$1"
for k in $2; do
	eval "v=\${$k-<unset>}"
	printf '%s=%s\n' "$k" "$v"
done
EOF

for t in $REAL_TARGETS; do
	f="$TARGETS/$t"
	sed -n 1,2p "$f" | grep -q '^# SPDX-FileCopyrightText: ' || fail "$t lacks the SPDX copyright line"
	sed -n 1,2p "$f" | grep -qx '# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception' || fail "$t lacks the SPDX license line"
	bad=$(grep -nvE "^(#.*|[a-z_]+=([A-Za-z0-9_./:,+=@%-]*|'[^']*'))?\$" "$f" || true)
	test -z "$bad" || { echo "$bad" >&2; fail "$t: lines outside the target grammar (R8)"; }
	keys=$(grep -oE '^[a-z_]+=' "$f" | tr -d '=')
	dup=$(printf '%s\n' "$keys" | sort | uniq -d)
	test -z "$dup" || fail "$t: duplicate keys: $dup (R8)"
	for k in $keys; do
		case " $TARGET_KEYS " in *" $k "*) ;; *) fail "$t: unknown key $k (R8, R9)" ;; esac
	done
	for k in partition cpus mem time modules make_vars; do
		test -n "$(target_value "$f" "$k")" || fail "$t: required key $k missing or empty (R8)"
	done
	grep -q '^gpus=' "$f" || fail "$t: no gpus line (R8: GPUs or gres)"
	# Equivalence: a POSIX shell sourcing the file and a line parser read the same values.
	env -i PATH=/usr/bin:/bin /bin/sh "$work/source.sh" "$f" "$TARGET_KEYS" >"$work/$t.sourced" 2>"$work/$t.err" \
		|| { cat "$work/$t.err" >&2; fail "$t: a POSIX shell cannot source it (R8)"; }
	env -i PATH=/usr/bin:/bin /bin/dash "$work/source.sh" "$f" "$TARGET_KEYS" >"$work/$t.dashed" 2>&1 || fail "$t: dash cannot source it (R8)"
	line_parse "$f" >"$work/$t.parsed"
	cmp -s "$work/$t.sourced" "$work/$t.parsed" && cmp -s "$work/$t.dashed" "$work/$t.parsed" \
		|| { diff "$work/$t.sourced" "$work/$t.parsed" >&2; fail "$t: sourcing and line parsing disagree (R8)"; }
	# Well-formed values.
	target_value "$f" cpus | grep -Eqx '[1-9][0-9]*' || fail "$t: cpus is not a positive integer"
	target_value "$f" mem | grep -Eqx '[1-9][0-9]*[MG]' || fail "$t: mem is not <n>M or <n>G"
	target_value "$f" time | grep -Eqx '[0-9]{2}:[0-5][0-9]:[0-5][0-9]' || fail "$t: time is not HH:MM:SS"
	target_value "$f" modules | tr ' ' '\n' | grep -Eqx 'gcc/(1[3-9]|[2-9][0-9])\.[0-9]+(\.[0-9]+)?' \
		|| fail "$t: no GCC 13 or newer module (R11)"
	target_value "$f" make_vars | tr ' ' '\n' | grep -qx 'CXX=g++' || fail "$t: make_vars does not set CXX=g++ (R11)"
done

check_target() { # NAME PARTITION GPUS DESCRIPTION-WORD ARCH-WORD
	f="$TARGETS/$1"
	test "$(target_value "$f" partition)" = "$2" || fail "$1: partition is not $2"
	test "$(target_value "$f" gpus)" = "$3" || fail "$1: gpus is not '$3'"
	target_value "$f" description | grep -qF "$4" || fail "$1: description does not say $4 (R11)"
	target_value "$f" description | grep -qF "$5" || fail "$1: description does not say $5 (R11)"
}
check_target x86-64-avx512 cpu '' AVX-512 x86-64
check_target x86-64-avx2 cpu '' AVX2 x86-64
check_target aarch64-gb10 spark-batch 1 GB10 aarch64

# The job's own parser accepts each real target: with the partition matching, toolchain gets as far
# as loading the GCC module, which the fake refuses.
bad_target_lines() { # write the gate-only malformed targets into the fixture
	mark="$work/PWNED"
	i=0
	for line in "cpus=\$(touch $mark)" "mem=\`touch $mark\`" "modules=\"\$(touch $mark)\"" \
		"time=00:10:00; touch $mark" "make_vars='CXX=clang++'; touch $mark" "account='<account>'" \
		"cpus=1" " cpus=1" "cpus=\${HOME}"; do
		i=$((i + 1))
		printf '%s\n%s\n' "$STUB_TARGET" "$line" >"$fx/ci/targets/local-inject-$i"
	done
	printf '%s\n' "$STUB_TARGET" | grep -v '^modules=' >"$fx/ci/targets/local-inject-missing"
	ninject=$i
}
EXTRA_TARGETS=bad_target_lines
make_stubs
make_fixture

for t in $REAL_TARGETS; do
	reset_g
	G_PARTITION=$(target_value "$TARGETS/$t" partition)
	run_job "real-$t" "real-$t" dash 6001 "$good" "$t"
	r=$(rundir "real-$t" "$good" "$t" 6001)
	result_check "$r/result.json" failed=toolchain target="$t" commit="$good"
	m=$(target_value "$TARGETS/$t" modules | cut -d' ' -f1)
	grep -qF "fake-module: load $m" "$r/toolchain.log" \
		|| { cat "$r/toolchain.log" >&2; fail "$t: the job's parser did not reach the module load (R8)"; }
done

# Malformed target lines: refused by line number, never executed (R8).
i=0
while [ "$i" -lt "$ninject" ]; do
	i=$((i + 1))
	t="local-inject-$i"
	reset_g
	run_job "$t" "$t" dash 6100 "$good" "$t"
	test "$rc" -eq 1 || { show "$t"; fail "$t: exited $rc, want 1"; }
	r=$(rundir "$t" "$good" "$t" 6100)
	result_check "$r/result.json" failed=toolchain target="$t"
	grep -qE "line [0-9]+" "$logs/$t.err" || { show "$t"; fail "$t: the refusal does not name the line (R8)"; }
	grep -q 'fake-module: load' "$r/toolchain.log" && fail "$t: modules were loaded from a malformed target (R8)"
	test ! -e "$work/PWNED" || fail "$t: a target line was executed (R8)"
	contained "$t" "$r"
done
reset_g
run_job missing-key missing-key dash 6200 "$good" local-inject-missing
test "$rc" -eq 1 || { show missing-key; fail "a target without modules: exited $rc, want 1 (R8)"; }
grep -q modules "$logs/missing-key.err" || { show missing-key; fail "a target without modules: the message does not name the key (R8)"; }

# R9: nothing this cycle adds, changes or says in its history is site-private.
scan_cycle
grep -qE '^(account|qos|cluster)=' "$TARGETS"/* && fail "a target file sets an account, QoS or cluster (R9)"

echo "P3 gate: green (3 targets: grammar, sourced = parsed, GCC 13+, CXX=g++, parsed by the job; $ninject malformed lines refused, none executed; site-private scan of files and history clean)"
