# The phase gate's checks (a cycle record, not a standing gate). Sourced by p1.sh, p2.sh, p3.sh,
# p4.sh and prepush.sh after common.sh; gate_all runs them in order (common.sh's header).

LABEL_TRANSFORM="the transform matches its definition at every odd factor"
LABEL_CONVOLUTION="the convolution rounds to Theorem 1's integers and the carry balances them"
LABEL_TABLE="the length table holds to its measurement"
LABEL_REFUSALS="the engine refuses lengths and exponents it does not hold"
LABEL_ROUND="residues round-trip through the balanced digits"
LABEL_OPS="squarings and products equal BigInt's at every odd factor"
LABEL_MONITOR="the round-off monitor reports and refuses past the threshold"
LABEL_CHAINS="shifted chains reproduce the partial residues"
LABEL_STRUCTURED="structured inputs are refused or squared right"
ENGINE_ROUND="residues round-trip at every supported length"
ENGINE_SQUARES="squarings equal BigInt's at every supported length"
ENGINE_PRODUCTS="products equal BigInt's at every supported length"
ENGINE_PARTIAL="squaring chains reproduce the known partial residues"
ENGINE_FINAL="squaring chains reproduce the known final residues"
ENGINE_ZERO="Lucas-Lehmer chains end in zero for the Mersenne prime exponents"

ENGINE_SOURCES='src/fft.cpp src/ibdwt.cpp src/lengths.cpp src/measure.cpp'


# --- Files ---------------------------------------------------------------------------------------

# files_check: what changed (against the plan commit, which is 0bc49cb outside spec/ and issues/),
# the pinned interface and labels, AGENTS.md, the Makefile, the sources (R10, R11, § Conventions),
# tests/cli.sh as 0bc49cb's (R12)
files_check() {
	cd "$root" || die "cd $root"
	fc_from=$(git log -1 --format=%H --grep="^\[feature\] Plan $slug" -- "$spec" 2>/dev/null)
	[ -n "$fc_from" ] || fc_from=$BASE
	{ git diff --name-only "$fc_from"; git ls-files --others --exclude-standard; } | sort -u >"$work/changed"
	py "$here/files.py" changes "$PHASE" <"$work/changed" || fail "files: what changed (PLAN § 2)"
	py "$here/files.py" interface "$PHASE" || fail "files: the pinned interface (PLAN § 2)"
	py "$here/files.py" labels "$PHASE" || fail "files: the pinned check labels (PLAN § 2)"
	git show "$BASE:AGENTS.md" >"$work/base-AGENTS.md" && git show "$BASE:Makefile" >"$work/base-Makefile" \
		&& git show "$BASE:doc/weighted-transforms.md" >"$work/base-note.md" || die "git show $BASE"
	py "$here/files.py" agents "$PHASE" "$work/base-AGENTS.md" || fail "files: AGENTS.md (R13)"
	py "$here/files.py" makefile "$PHASE" "$work/base-Makefile" || fail "files: the Makefile (R10, R12)"
	py "$here/files.py" source || fail "files: the sources (R10, R11, AGENTS.md § Conventions)"
	git diff --quiet "$BASE" -- tests/cli.sh || fail "tests/cli.sh differs from $BASE's (R12)"
}


# --- Builds --------------------------------------------------------------------------------------

# builds: the tree copied once per compiler and built with make check (the default tier), both at
# once; each must pass, with the same count of checks. Then bin/measure in the clang copy.
builds() {
	for bu_x in $COMPILERS; do
		copy_tree "$work/tree-$bu_x" || die "cannot copy the tree"
		mk "$work/tree-$bu_x" "$bu_x" "check-$bu_x" 2400 check &
	done
	wait
	CHECKS=
	for bu_x in $COMPILERS; do
		grep -q '^check: [0-9]* passed, 0 failed$' "$logs/check-$bu_x.out" && grep -q '^cli: [0-9]* passed, 0 failed$' "$logs/check-$bu_x.out" \
			|| { tail_of "check-$bu_x"; fail "$bu_x: make check is red"; }
		bu_c=$(sed -n 's/^check: \([0-9]*\) passed, 0 failed$/\1/p' "$logs/check-$bu_x.out")
		[ -z "$CHECKS" ] || [ "$CHECKS" = "$bu_c" ] || fail "make check ran $CHECKS checks on one compiler and $bu_c on the other"
		CHECKS=$bu_c
	done
	mk "$work/tree-clang++" clang++ measure-build 600 bin/measure || { tail_of measure-build; fail "bin/measure does not build"; }
}


# flags_check: every compile of an engine source, by each compiler, carries the IEEE flags and no
# flag that weakens them (R10); each compiler compiled every engine source (R11); and each engine
# source compiles as strict ISO C++20 with every warning an error on both (R11)
flags_check() {
	for fl_x in $COMPILERS; do
		cat "$logs/check-$fl_x.calls" "$logs/measure-build.calls" 2>/dev/null >"$work/calls-$fl_x"
		for fl_s in $ENGINE_SOURCES; do
			grep -F -- " -c $fl_s " "$work/calls-$fl_x" | awk -v x="$fl_x" '$1 == x' >"$work/flags"
			if [ "$fl_s" = src/measure.cpp ] && [ "$fl_x" != clang++ ]; then continue; fi
			test -s "$work/flags" || fail "$fl_x never compiled $fl_s (R11)"
			while IFS= read -r fl_l; do
				case " $fl_l " in *" -fno-fast-math "*) ;; *) fail "$fl_x compiled $fl_s without -fno-fast-math (R10)" ;; esac
				case " $fl_l " in *" -ffp-contract=off "*) ;; *) fail "$fl_x compiled $fl_s without -ffp-contract=off (R10)" ;; esac
				printf '%s\n' "$fl_l" | grep -Eq -- ' -(Ofast|ffast-math|funsafe-math-optimizations|fassociative-math|freciprocal-math|ffinite-math-only|fno-signed-zeros|ffp-contract=(fast|on)|ffp-model=(fast|aggressive))( |$)' \
					&& fail "$fl_x compiled $fl_s with a flag that weakens IEEE arithmetic (R10)"
			done <"$work/flags"
		done
	done
	# No fused multiply-add in any engine object, whatever the source asked for (R10): arm64 and x86
	for fl_x in $COMPILERS; do
		for fl_o in lib/fft.o lib/ibdwt.o lib/lengths.o lib/check/fft.o lib/check/ibdwt.o lib/check/lengths.o lib/measure.o; do
			[ -f "$work/tree-$fl_x/$fl_o" ] || { [ "$fl_o" = lib/measure.o ] && continue; fail "$fl_x: no $fl_o to disassemble"; continue; }
			otool -tv "$work/tree-$fl_x/$fl_o" >"$work/disasm" 2>&1 || { fail "$fl_x: otool cannot read $fl_o"; continue; }
			grep -q 'section' "$work/disasm" || fail "$fl_x: otool printed no code for $fl_o"
			fl_f=$(grep -ciE '[[:space:]](fmadd|fmsub|fnmadd|fnmsub|fmla|fmls|vfmadd[a-z0-9]*|vfmsub[a-z0-9]*|vfnmadd[a-z0-9]*|vfnmsub[a-z0-9]*)([.[:space:]]|$)' "$work/disasm")
			[ "$fl_f" = 0 ] || fail "$fl_x: $fl_o holds $fl_f fused multiply-add instructions (R10)"
		done
	done
	for fl_x in clang++ g++-16; do
		for fl_s in $ENGINE_SOURCES; do
			( cd "$root" && limit 300 "$fl_x" -std=c++20 -pedantic-errors -Wall -Wextra -Werror -fno-fast-math -ffp-contract=off \
				-I src -fsyntax-only "$fl_s" ) >"$logs/pedantic.out" 2>&1 \
				|| { tail_of pedantic; fail "$fl_s is not strict ISO C++20 for $fl_x (-pedantic-errors -Werror; R11)"; }
		done
	done
}


# --- The table -------------------------------------------------------------------------------------

# lengths_check [background]: the table against its measurement (lengths.py), then the boundary
# probes, the last held and the first failing probe of the sample lengths (all 144 in full mode),
# measured again by the tree's bin/measure, one worker, and compared byte for byte (R3, R9); then
# the gate's own measurement at those lengths' maxima (needs the probe, built by probe_check)
lengths_check() {
	( cd "$root" && py "$here/lengths.py" check ) || fail "the length table against its measurement (R3)"
	( cd "$root" && py "$here/lengths.py" sample "$MODE" ) >"$work/sample" || fail "lengths.py sample"
	: >"$work/remeasured"
	while read -r lc_n lc_p; do
		( cd "$work/tree-clang++" && limit 600 bin/measure --probe "$lc_n" "$lc_p" ) >>"$work/remeasured" 2>"$logs/remeasure.err" \
			|| { cat "$logs/remeasure.err" >&2; fail "bin/measure --probe $lc_n $lc_p failed"; break; }
	done <"$work/sample"
	( cd "$root" && py "$here/lengths.py" compare "$work/remeasured" ) \
		|| fail "the measurement's boundary probes do not reproduce (R3, R9)"
	wc -l <"$work/remeasured" | tr -d ' ' >"$work/remeasured-count"
	# The gate's own measurement at each sample length's maximum: its own generator, twice the
	# squarings (probe.cpp, `measure`), so a weakened protocol cannot carry the table
	( cd "$root" && py "$here/lengths.py" maxima "$MODE" ) >"$work/maxima" || fail "lengths.py maxima"
	limit 3600 "$work/probe-clang++" measure "$work/maxima" >"$logs/own-measure.out" 2>&1 \
		|| { grep FAIL "$logs/own-measure.out" | head -n 5 >&2; fail "the gate's own measurement finds the table's maxima past the sizing (R3)"; }
}


# --- The probe -------------------------------------------------------------------------------------

# probe_check: the gate's own driver (probe.cpp) built against each compiler's modules and run; in
# full mode the two builds' residue digests must be equal (R9)
probe_check() {
	PROBE_DIGEST=
	for pr_x in $COMPILERS; do
		pr_d="$work/tree-$pr_x"
		( cd "$pr_d" && limit 600 "$pr_x" -std=c++20 -fno-fast-math -ffp-contract=off -O3 -DGATE_PHASE="$PHASE" -I src \
			"$here/probe.cpp" lib/fft.o lib/ibdwt.o lib/lengths.o lib/mp.o -o "$work/probe-$pr_x" ) >"$logs/probe-build-$pr_x.out" 2>&1 \
			|| { tail_of "probe-build-$pr_x"; fail "$pr_x: the probe does not build against the pinned interface (PLAN § 2)"; continue; }
		limit 1800 "$work/probe-$pr_x" "$PHASE" "$MODE" >"$logs/probe-$pr_x.out" 2>&1
		grep -q '^probe: ok ' "$logs/probe-$pr_x.out" || { tail_of "probe-$pr_x"; fail "$pr_x: the gate's probe of the engine (R1-R4, R6-R9, R14)"; continue; }
		pr_g=$(sed -n 's/^probe: digest //p' "$logs/probe-$pr_x.out")
		[ -z "$PROBE_DIGEST" ] || [ "$PROBE_DIGEST" = "$pr_g" ] || fail "the probe's residues differ between the clang and the GCC build (R9)"
		PROBE_DIGEST=$pr_g
	done
}


# --- The mutants -----------------------------------------------------------------------------------

# interposed DIR: DIR, a built copy of the clang tree, with the pinned definitions renamed to
# gate_real_NAME, their declarations added to the headers, and the interposer in tests/
interposed() {
	ip_d=$1
	rsync -a "$work/tree-clang++/" "$ip_d/" || return 1
	printf '#define GATE_PHASE %s\n' "$PHASE" >"$ip_d/tests/gate_phase.h"
	cp "$here/interpose.cpp" "$ip_d/tests/gate_interpose.cpp" || return 1
	python3 -I - "$ip_d" "$PHASE" <<'PY' || return 1
import sys
d, phase = sys.argv[1], int(sys.argv[2])
defs = [("src/fft.cpp", "fft::Complex fft::root(std::uint64_t k, std::uint64_t n) {", "fft::root(", "fft::gate_real_root("),
        ("src/ibdwt.cpp", "std::vector<double> ibdwt::weights(std::uint64_t p, std::uint64_t n) {", "ibdwt::weights(", "ibdwt::gate_real_weights("),
        ("src/ibdwt.cpp", "double ibdwt::roundoff(const std::vector<double>& outputs) {", "ibdwt::roundoff(", "ibdwt::gate_real_roundoff("),
        ("src/ibdwt.cpp", "ibdwt::Residue ibdwt::carry(const Engine& engine, const std::vector<double>& outputs, bool subtract, std::uint64_t bit) {", "ibdwt::carry(", "ibdwt::gate_real_carry("),
        ("src/lengths.cpp", "void ibdwt::require_held(std::uint64_t length, std::uint64_t p) {", "ibdwt::require_held(", "ibdwt::gate_real_require_held(")]
if phase >= 2:
    for name, first in [("to_digits", "ibdwt::Residue ibdwt::to_digits(const Engine& engine, const BigInt& x) {"),
                        ("from_digits", "BigInt ibdwt::from_digits(const Engine& engine, const Residue& x) {"),
                        ("square", "double ibdwt::square(const Engine& engine, Residue& x) {"),
                        ("square_subtract", "double ibdwt::square_subtract(const Engine& engine, Residue& x, std::uint64_t bit) {"),
                        ("multiply", "double ibdwt::multiply(const Engine& engine, Residue& x, const Residue& y) {"),
                        ("checksums_agree", "bool ibdwt::checksums_agree(const Transform& transform, const Residue& x, const Residue& y, const std::vector<double>& outputs) {"),
                        ("rms", "double ibdwt::rms(const std::vector<double>& outputs) {")]:
        defs.append(("src/ibdwt.cpp", first, "ibdwt::%s(" % name, "ibdwt::gate_real_%s(" % name))
for path, first, old, new in defs:
    p = d + "/" + path
    lines = open(p).read().split("\n")
    hits = [i for i, l in enumerate(lines) if l == first]
    if len(hits) != 1:
        sys.exit("interposed: %s does not hold '%s' exactly once" % (path, first))
    lines[hits[0]] = first.replace(old, new, 1)
    open(p, "w").write("\n".join(lines))
decl = {"src/fft.hpp": ["namespace fft {", "\tComplex gate_real_root(std::uint64_t k, std::uint64_t n);", "}"],
        "src/ibdwt.hpp": ["namespace ibdwt {",
                          "\tstd::vector<double> gate_real_weights(std::uint64_t p, std::uint64_t n);",
                          "\tdouble gate_real_roundoff(const std::vector<double>& outputs);",
                          "\tResidue gate_real_carry(const Engine& engine, const std::vector<double>& outputs, bool subtract, std::uint64_t bit);"]
                         + (["\tResidue gate_real_to_digits(const Engine& engine, const BigInt& x);",
                             "\tBigInt gate_real_from_digits(const Engine& engine, const Residue& x);",
                             "\tdouble gate_real_square(const Engine& engine, Residue& x);",
                             "\tdouble gate_real_square_subtract(const Engine& engine, Residue& x, std::uint64_t bit);",
                             "\tdouble gate_real_multiply(const Engine& engine, Residue& x, const Residue& y);",
                             "\tbool gate_real_checksums_agree(const Transform& transform, const Residue& x, const Residue& y, const std::vector<double>& outputs);",
                             "\tdouble gate_real_rms(const std::vector<double>& outputs);"] if phase >= 2 else [])
                         + ["}"],
        "src/lengths.hpp": ["namespace ibdwt {", "\tvoid gate_real_require_held(std::uint64_t length, std::uint64_t p);", "}"]}
for path, lines in decl.items():
    with open(d + "/" + path, "a") as f:
        f.write("\n\n// Added by the gate (interpose.cpp)\n" + "\n".join(lines) + "\n")
PY
}

# expect_failed LOG LABEL...: the run failed, and every LABEL among its failures
expect_failed() {
	ef_log=$1
	shift
	grep -q '^check: [0-9]* passed, [1-9][0-9]* failed$' "$ef_log" || return 1
	( cd "$root" && py "$here/trace.py" failed "$ef_log" "$@" )
}

# expect_all_disagree LOG LABEL...: under a sabotage, every tally line of each LABEL counts every
# case as disagreeing, "   tally <what>: K cases, K disagree", printed passing or failing (PLAN § 2.8)
expect_all_disagree() {
	ead_log=$1
	shift
	( cd "$root" && py "$here/trace.py" disagree "$ead_log" "$@" )
}

# mutants_check: the interposed copy passes make check's default tier with no mutant; each mutant
# turns the checks PLAN § 6 names red; and a copy whose table overshoots its measurement at the
# largest length turns the table check red and fails lengths.py
mutants_check() {
	interposed "$work/mut" || die "cannot build the interposed copy (PLAN § 2's pins)"
	mk "$work/mut" clang++ mut-build 2400 bin/check || { tail_of mut-build; fail "the interposed copy does not build (PLAN § 2's pins)"; return; }
	( cd "$work/mut" && limit 600 bin/check ) >"$logs/mut-none.out" 2>&1 \
		|| { tail_of mut-none; fail "the interposed copy fails the default tier with no mutant"; }
	MUTANTS=0
	mutant_run() { # ID LABEL...
		mr_id=$1
		shift
		( cd "$work/mut" && GATE_MUTANT=$mr_id limit 600 bin/check ) >"$logs/mut-$mr_id.out" 2>&1
		expect_failed "$logs/mut-$mr_id.out" "$@" || fail "mutant $mr_id survived the checks that must catch it: $* (PLAN § 6)"
		MUTANTS=$((MUTANTS + 1))
	}
	if [ "$PHASE" -ge 2 ]; then
		mutant_run weights "$LABEL_CONVOLUTION" "$LABEL_OPS"
		mutant_run carry-drop "$LABEL_CONVOLUTION" "$LABEL_OPS"
		mutant_run unbalanced "$LABEL_CONVOLUTION" "$LABEL_ROUND"
		mutant_run subtract-sign "$LABEL_CONVOLUTION" "$LABEL_OPS"
		mutant_run subtract-place "$LABEL_CONVOLUTION" "$LABEL_OPS"
		mutant_run silent-monitor "$LABEL_CONVOLUTION" "$LABEL_MONITOR"
		mutant_run product-square "$LABEL_OPS"
		mutant_run no-checksum "$LABEL_STRUCTURED"
		mutant_run no-envelope "$LABEL_STRUCTURED"
		# The sabotage (digit 0 of every result) must sink every graded check, the chains included;
		# the top sabotage (every residue read back wrong above its low 64 bits) every check that
		# grades a residue in full: the round trip, the squarings and the products
		if [ "$PHASE" -ge 3 ]; then
			mutant_run sabotage "$LABEL_ROUND" "$LABEL_OPS" "$LABEL_CHAINS"
			expect_all_disagree "$logs/mut-sabotage.out" "$LABEL_ROUND" "$LABEL_OPS" "$LABEL_CHAINS" \
				|| fail "under the sabotage, a graded default-tier check calls a case it does not grade (PLAN § 2.8)"
		else
			mutant_run sabotage "$LABEL_ROUND" "$LABEL_OPS"
			expect_all_disagree "$logs/mut-sabotage.out" "$LABEL_ROUND" "$LABEL_OPS" \
				|| fail "under the sabotage, a graded default-tier check calls a case it does not grade (PLAN § 2.8)"
		fi
		mutant_run sabotage-top "$LABEL_ROUND" "$LABEL_OPS"
		expect_all_disagree "$logs/mut-sabotage-top.out" "$LABEL_ROUND" "$LABEL_OPS" \
			|| fail "under the top sabotage, a check grades a residue by its low bits alone (PLAN § 2.8)"
	else
		mutant_run weights "$LABEL_CONVOLUTION"
		mutant_run carry-drop "$LABEL_CONVOLUTION"
		mutant_run unbalanced "$LABEL_CONVOLUTION"
		mutant_run subtract-sign "$LABEL_CONVOLUTION"
		mutant_run subtract-place "$LABEL_CONVOLUTION"
		mutant_run silent-monitor "$LABEL_CONVOLUTION"
	fi
	mutant_run twiddle "$LABEL_TRANSFORM"
	mutant_run no-table "$LABEL_REFUSALS"

	rsync -a "$work/tree-clang++/" "$work/over/" || die "cannot copy the tree"
	( cd "$work/over" && python3 -I - <<'PY' ) || die "cannot write the overshooting table"
import re
data = [l.split() for l in open("src/lengths.txt") if l[0].isdigit()]
first_fail = [r for r in data if r[0] == "1048576"][-1][1]
src = open("src/lengths.cpp").read()
src2 = re.sub(r"\{1048576, \d+\},", "{1048576, %s}," % first_fail, src)
assert src2 != src
open("src/lengths.cpp", "w").write(src2)
PY
	( cd "$work/over" && py "$here/lengths.py" check ) >"$logs/over-lengths.out" 2>&1 \
		&& fail "lengths.py passes a table that overshoots its measurement: the gate's own table check is hollow"
	mk "$work/over" clang++ over-build 1200 bin/check || { tail_of over-build; fail "the overshooting copy does not build"; return; }
	( cd "$work/over" && limit 600 bin/check ) >"$logs/over.out" 2>&1
	expect_failed "$logs/over.out" "$LABEL_TABLE" || fail "a table that overshoots its measurement passes \"$LABEL_TABLE\" (R3)"
	MUTANTS=$((MUTANTS + 1))
}


# --- The engine tier -------------------------------------------------------------------------------

# engine_tier: make check-engine with nothing to rebuild, on clang alone and timed (R12, at most
# 15 minutes), beside the full boundary re-measurement; then on GCC beside the interposed copy's
# engine tier under the sabotage, traced; then the residues of the two builds compared (R9), the
# trace against what the R-IDs name, and the sabotage's failures (R1, R4, R5, R14)
engine_tier() {
	lengths_check &
	et_lp=$!
	et_t0=$(perl -MTime::HiRes=time -e 'printf "%.3f", time')
	mk "$work/tree-clang++" clang++ engine-clang 1800 check-engine
	et_rc=$?
	et_t1=$(perl -MTime::HiRes=time -e 'printf "%.3f", time')
	wait "$et_lp" || fail "the boundary re-measurement"
	[ "$et_rc" = 0 ] || { tail_of engine-clang; fail "clang++: make check-engine is red"; }
	HEAVY=$(perl -e "printf '%.0f', $et_t1 - $et_t0")
	[ "$HEAVY" -le "$HEAVY_BUDGET" ] || fail "clang++: make check-engine took $HEAVY s, more than $HEAVY_BUDGET s (R12)"
	grep -q '^check: [0-9]* passed, 0 failed$' "$logs/engine-clang.out" || fail "clang++: make check-engine printed no green summary"

	ENGINE_GREEN=clang++
	et_jobs=
	if printf ' %s ' "$COMPILERS" | grep -q ' g++-16 '; then
		mk "$work/tree-g++-16" g++-16 engine-gcc 2400 check-engine &
		et_jobs="$et_jobs $!"
	fi
	if [ "$PHASE" -ge 2 ]; then
		( cd "$work/mut" && GATE_MUTANT=sabotage GATE_TRACE="$work/trace" limit 2400 bin/check engine >"$logs/sabotage.out" 2>&1
		  GATE_MUTANT=sabotage-top limit 2400 bin/check engine >"$logs/sabotage-top.out" 2>&1 ) &
		et_sab=$!
	fi
	for et_j in $et_jobs; do
		wait "$et_j" || { tail_of engine-gcc; fail "g++-16: make check-engine is red (R11)"; }
	done
	[ -n "$et_jobs" ] && ENGINE_GREEN="clang++ and g++-16"
	if [ "$PHASE" -ge 2 ]; then
		wait "$et_sab"
		( cd "$root" && py "$here/lengths.py" table ) >"$work/table" || die "lengths.py table"
		test -s "$work/trace" || fail "the sabotaged engine tier wrote no trace"
		( cd "$root" && py "$here/trace.py" coverage "$PHASE" "$work/trace" "$work/table" ) \
			|| fail "the engine tier does not call the engine at every case R1, R4, R5 and R14 name"
		for et_s in sabotage sabotage-top; do
			test -f "$logs/$et_s.out" || { fail "the engine tier did not run under $et_s"; continue; }
			if [ "$PHASE" -ge 3 ] && [ "$et_s" = sabotage ]; then
				set -- "$ENGINE_ROUND" "$ENGINE_SQUARES" "$ENGINE_PRODUCTS" "$ENGINE_PARTIAL" "$ENGINE_FINAL" "$ENGINE_ZERO"
			else
				set -- "$ENGINE_ROUND" "$ENGINE_SQUARES" "$ENGINE_PRODUCTS"
			fi
			expect_failed "$logs/$et_s.out" "$@" \
				|| fail "an engine-tier check passed with every result of the engine off by one ($et_s)"
			expect_all_disagree "$logs/$et_s.out" "$@" \
				|| fail "under $et_s, a graded engine-tier check calls a case it does not grade (PLAN § 2.8)"
		done
	fi
	if [ -n "$et_jobs" ]; then
		grep '^   residues ' "$logs/engine-clang.out" >"$work/res-clang"
		grep '^   residues ' "$logs/engine-gcc.out" >"$work/res-gcc"
		[ "$PHASE" -lt 2 ] || test -s "$work/res-clang" || fail "the engine tier printed no residue digests"
		cmp -s "$work/res-clang" "$work/res-gcc" || fail "the engine tier's residues differ between clang and GCC (R9)"
	fi
}


# --- Time ------------------------------------------------------------------------------------------

# timing: make check with nothing to rebuild, 0bc49cb's copy against the tree's, interleaved, the
# least of five each, once more before failing (R12: at most 5 s more)
timing() {
	base_copy "$work/base" || die "cannot copy $BASE"
	mk "$work/base" clang++ base-check 2400 check || { tail_of base-check; die "make check at $BASE is red"; }
	python3 -I - "$MAKE" "$work/base" "$work/tree-clang++" "$DEFAULT_BUDGET" >"$logs/timing.out" 2>&1 <<'EOF' \
		|| { cat "$logs/timing.out" >&2; fail "make check takes more than $DEFAULT_BUDGET s longer than at 0bc49cb (R12)"; }
import os, subprocess, sys, time
make, base, tree, budget = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
def run(d):
    t = time.perf_counter()
    subprocess.run([make, "CXX=clang++", "check"], cwd=d, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    return time.perf_counter() - t
b, t = [], []
for attempt in range(2):
    for _ in range(5):
        b.append(run(base))
        t.append(run(tree))
    if min(t) - min(b) <= budget:
        break
print("make check %.3f s at 0bc49cb, %.3f s now, %+.3f s (budget %+.0f s; least of %d interleaved runs each; load %.1f)"
      % (min(b), min(t), min(t) - min(b), budget, len(t), os.getloadavg()[0]))
sys.exit(0 if min(t) - min(b) <= budget else 1)
EOF
	TIMES=$(cat "$logs/timing.out")
}


# --- The note --------------------------------------------------------------------------------------

docs_check() {
	cd "$root" || die "cd $root"
	py "$here/docs.py" note "$PHASE" "$work/base-note.md" || fail "the note (R13)"
	py "$here/docs.py" references || fail "the bibliography (R13)"
	python3 -I -B spec/math-notes-and-references/verify/notes.py claims doc/weighted-transforms.md >"$logs/claims.out" 2>&1 \
		|| { tail_of claims; fail "notes.py claims on doc/weighted-transforms.md (doc/README.md's markers)"; }
}


# gate_all: everything a phase gate checks; GATE_LINE summarizes it
gate_all() {
	HEAVY=- TIMES='timing not run (quick mode)' MUTANTS=0 ENGINE_GREEN='not run (quick mode)'
	files_check
	builds
	flags_check
	probe_check
	mutants_check
	if [ "$MODE" = full ]; then
		engine_tier
		timing
	else
		lengths_check
		if [ "$PHASE" -ge 2 ]; then
			for qs in sabotage sabotage-top; do
				( cd "$work/mut" && GATE_MUTANT=$qs limit 600 bin/check ) >/dev/null 2>&1 && fail "the default tier passes under $qs"
			done
		fi
	fi
	docs_check
	REMEASURED=$(cat "$work/remeasured-count" 2>/dev/null || echo 0)
	GATE_LINE="$MODE mode, phase $PHASE: files and pins as PLAN § 2 gives them; IEEE flags on every engine compile and strict C++20 on both compilers; make check green on $COMPILERS ($CHECKS checks); table as measured, $REMEASURED boundary probes reproduced; probe ok${PROBE_DIGEST:+ (residues $PROBE_DIGEST)}; $MUTANTS mutants caught; engine tier green on $ENGINE_GREEN in ${HEAVY} s; $TIMES; note and bibliography"
}


# gate_end NAME: the green line, or the self-test's tally
gate_end() {
	if [ -n "$SELFTEST" ]; then
		echo "selftest summary: ${GATE_LINE:-}"
		if [ "$FAILS" -gt 0 ]; then echo "selftest: $FAILS checks failed" >&2; exit 1; fi
		echo "selftest: no check failed (not a verdict: frozen-gate check skipped)"
		exit 3
	fi
	echo "$1: green ($GATE_LINE)"
}
