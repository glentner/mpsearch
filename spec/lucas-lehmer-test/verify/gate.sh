# The phase gate's checks (a cycle record, not a standing gate). Sourced by p1.sh, p2.sh, p3.sh,
# p4.sh and prepush.sh after common.sh; gate_all runs them in order (common.sh's header).
# CHANGES_PHASE (default PHASE) is the phase whose file list `files_check` grades: the last phase's
# gates run every other check as of phase 3 and grade the files as of phase 4, whose deferral ledger
# edits issues/ and ROADMAP.md.

LABEL_VECTORS="the Jacobi symbol matches the vectors"
LABEL_STOP="run_in_order stops at the first verdict or error"
LABEL_KNOWN="the test reports the known answers at two given shifts"
LABEL_JACOBI="the test checks the Jacobi symbol at intervals and on the final residue"
LABEL_DEGENERATE="the test catches degenerate states"
LABEL_CONFIRM="the test confirms a zero residue before it reports prime"
LABEL_RECOVER="the test recovers from a failed check, up to its limit"
LABEL_REPLAY="the test replays a refused squaring once"
LABEL_MOVE="the test moves to a larger length when a refusal reproduces"
LABEL_DISTANCES="the test counts and logs large round-off distances"
LABEL_PROGRESS="the test logs its progress"
LABEL_FRESH="the test chooses a fresh shift each run"
LABEL_REFUSALS="the test refuses exponents it cannot run"
LABEL_VIEWS="the result of the test names its run in both views"
CHANGES_PHASE=${CHANGES_PHASE:-$PHASE}


# --- Files ---------------------------------------------------------------------------------------

files_check() {
	cd "$root" || die "cd $root"
	fc_from=$(git log -1 --format=%H --grep="^\[feature\] Plan $slug" -- "$spec" 2>/dev/null)
	[ -n "$fc_from" ] || fc_from=$BASE
	# The planner's self-test grades a prototype against the commit its build starts from
	[ -z "$SELFTEST" ] || fc_from=${GATE_FROM:-$fc_from}
	{ git diff --name-only "$fc_from"; git ls-files --others --exclude-standard; } | sort -u >"$work/changed"
	py "$here/ll_files.py" changes "$CHANGES_PHASE" <"$work/changed" || fail "files: what changed (PLAN § 2.1)"
	py "$here/ll_files.py" interface "$PHASE" || fail "files: the pinned interface (PLAN § 2.4)"
	py "$here/ll_files.py" labels "$PHASE" || fail "files: the pinned check labels (PLAN § 2.6)"
	git show "$BASE:AGENTS.md" >"$work/base-AGENTS.md" && git show "$BASE:Makefile" >"$work/base-Makefile" \
		&& git show "$BASE:tests/cli.sh" >"$work/base-cli.sh" || die "git show $BASE"
	py "$here/ll_files.py" agents "$PHASE" "$work/base-AGENTS.md" || fail "files: AGENTS.md (R17, R18)"
	py "$here/ll_files.py" makefile "$PHASE" "$work/base-Makefile" || fail "files: the Makefile (R16)"
	py "$here/ll_files.py" source "$PHASE" || fail "files: the sources (R16, AGENTS.md § Conventions, § Invariants 3)"
}


# --- Builds --------------------------------------------------------------------------------------

# builds: the tree copied once per compiler, make check in each, both at once; each green, with the
# same counts of checks, cli expectations and (from P2) known-answer runs
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
		if [ "$PHASE" -ge 2 ]; then
			grep -q '^ll: [0-9]* passed, 0 failed$' "$logs/check-$bu_x.out" || { tail_of "check-$bu_x"; fail "$bu_x: make check ran no green tests/ll.sh"; }
		fi
		bu_c=$(grep -E '^(check|cli|ll): [0-9]+ passed' "$logs/check-$bu_x.out" | tr '\n' ' ')
		[ -z "$CHECKS" ] || [ "$CHECKS" = "$bu_c" ] || fail "make check counted '$CHECKS' on one compiler and '$bu_c' on the other"
		CHECKS=$bu_c
		grep -q 'warning:' "$logs/check-$bu_x.out" && fail "$bu_x: the build printed warnings"
	done
	if [ "$PHASE" -ge 2 ]; then
		( cd "$work/tree-clang++" && py "$here/ll_runs.py" log "$logs/check-clang++.out" ) || fail "make check's tests/ll.sh does not cover the known answers (R1, R2)"
	fi
}


# --- The vectors -----------------------------------------------------------------------------------

vectors_check() {
	( cd "$work/tree-clang++" && limit 600 python3 -I tests/vectors/generate.py jacobi ) >"$work/jacobi.txt" 2>"$logs/generate.err" \
		|| { cat "$logs/generate.err" >&2; fail "tests/vectors/generate.py jacobi does not run (R5)"; }
	cmp -s "$work/jacobi.txt" "$work/tree-clang++/tests/vectors/jacobi.txt" \
		|| fail "tests/vectors/jacobi.txt is not what its own command writes (R5)"
	( cd "$work/tree-clang++" && py "$here/ll_vectors.py" check ) >"$logs/vectors.out" 2>&1 \
		|| { cat "$logs/vectors.out" >&2; fail "the Jacobi vectors against the gate's own computation (R5)"; }
}


# --- The probe -------------------------------------------------------------------------------------

probe_check() {
	for pr_x in $COMPILERS; do
		pr_d="$work/tree-$pr_x"
		pr_o=$(cd "$pr_d" && ls lib/*.o | grep -v -e '^lib/main\.o$' -e '^lib/measure\.o$' | tr '\n' ' ')
		# shellcheck disable=SC2086
		( cd "$pr_d" && limit 600 "$pr_x" -std=c++20 -fno-fast-math -ffp-contract=off -O2 -DGATE_PHASE="$PHASE" -I src \
			"$here/probe.cpp" $pr_o -o "$work/probe-$pr_x" ) >"$logs/probe-build-$pr_x.out" 2>&1 \
			|| { tail_of "probe-build-$pr_x"; fail "$pr_x: the probe does not build against the pinned interface (PLAN § 2.4)"; continue; }
		( cd "$pr_d" && limit 1200 "$work/probe-$pr_x" "$PHASE" "$MODE" ) >"$logs/probe-$pr_x.out" 2>&1
		grep -q '^probe: ok ' "$logs/probe-$pr_x.out" || { tail_of "probe-$pr_x"; fail "$pr_x: the gate's probe (R3-R15)"; }
	done
	PROBE=$(sed -n 's/^probe: ok //p' "$logs/probe-clang++.out")
}


# --- The mutants -----------------------------------------------------------------------------------

# interposed DIR: a built copy of the clang tree with the pinned definitions renamed gate_real_NAME
# and a wrapper appended to the same file, each switched by GATE_MUTANT, and test_lucas_lehmer
# renamed with a stand-in switched by GATE_LL (ll_cli.py); bin/check and bin/mps both carry them
interposed() {
	ip_d=$1
	rsync -a "$work/tree-clang++/" "$ip_d/" || return 1
	python3 -I - "$ip_d" "$PHASE" <<'PY' || return 1
import sys
d, phase = sys.argv[1], int(sys.argv[2])
def rename(path, first, old, new):
    p = d + "/" + path
    lines = open(p).read().split("\n")
    hits = [i for i, l in enumerate(lines) if l == first]
    if len(hits) != 1:
        sys.exit("interposed: %s does not hold '%s' exactly once" % (path, first))
    lines[hits[0]] = first.replace(old, new, 1)
    open(p, "w").write("\n".join(lines))
def append(path, text):
    open(d + "/" + path, "a").write("\n\n// Added by the gate\n" + text + "\n")
MUT = '#include <cstdlib>\n#include <cstring>\nstatic bool gate_is(const char* m) { const char* g = std::getenv("GATE_MUTANT"); return g and std::strcmp(g, m) == 0; }\n'
rename("src/mp.cpp", "int jacobi(const BigInt& a, const BigInt& n) {", "jacobi(", "gate_real_jacobi(")
append("src/mp.cpp", MUT + """int gate_real_jacobi(const BigInt& a, const BigInt& n);
int jacobi(const BigInt& a, const BigInt& n) {
	const int j = gate_real_jacobi(a, n);
	if (gate_is("jacobi-flip")) return -j;
	if (gate_is("jacobi-nonzero") and j == 0) return 1;
	return j;
}""")
rename("src/math.cpp", "Result run_in_order(const long unsigned p, const std::vector<TestFunction>& tests, const ResultSink& completed) {",
       "run_in_order(", "gate_real_run_in_order(")
rename("src/math.cpp", "Result test_lucas_lehmer(const long unsigned p) {", "test_lucas_lehmer(", "gate_real_test_lucas_lehmer(")
append("src/math.cpp", MUT + """Result gate_real_run_in_order(const long unsigned p, const std::vector<TestFunction>& tests, const ResultSink& completed);
Result gate_real_test_lucas_lehmer(const long unsigned p);
Result run_in_order(const long unsigned p, const std::vector<TestFunction>& tests, const ResultSink& completed) {
	if (not gate_is("old-stop-rule"))
		return gate_real_run_in_order(p, tests, completed);
	std::optional<Result> last;
	for (const TestFunction test: tests) {
		last = test(p);
		completed(*last);
		if (is_decisive(*last))
			break;
	}
	return *last;
}
Result test_lucas_lehmer(const long unsigned p) {
	const char* who = std::getenv("GATE_LL");
	if (not who)
		return gate_real_test_lucas_lehmer(p);
	logging::info("Running Lucas-Lehmer test");
	if (std::strcmp(who, "stub") == 0)
		return Result::no_verdict(WORK_LL, p, "Lucas-Lehmer is not implemented yet");
	if (std::strcmp(who, "reason") == 0)
		return Result::no_verdict(WORK_LL, p, "a stand-in for the gate");
	if (std::strcmp(who, "prime") == 0)
		return Result::prime(WORK_LL, p);
	return Result::error(WORK_LL, p, "a stand-in for the gate");
}""")
if phase >= 2:
    for first, name in [("BigInt lucas::unshifted(const BigInt& r, std::uint64_t k, std::uint64_t p) {", "unshifted"),
                        ("int lucas::jacobi_of(const BigInt& r, std::uint64_t k, std::uint64_t p) {", "jacobi_of"),
                        ("bool lucas::degenerate(const BigInt& s, std::uint64_t p, bool last) {", "degenerate"),
                        ("bool lucas::confirms_prime(const BigInt& before_last, std::uint64_t p) {", "confirms_prime")]:
        rename("src/lucas.cpp", first, "lucas::%s(" % name, "lucas::gate_real_%s(" % name)
    open(d + "/src/lucas.hpp", "a").write("""
// Added by the gate
namespace lucas {
	BigInt gate_real_unshifted(const BigInt& r, std::uint64_t k, std::uint64_t p);
	int gate_real_jacobi_of(const BigInt& r, std::uint64_t k, std::uint64_t p);
	bool gate_real_degenerate(const BigInt& s, std::uint64_t p, bool last);
	bool gate_real_confirms_prime(const BigInt& before_last, std::uint64_t p);
}
""")
    append("src/lucas.cpp", MUT + """BigInt lucas::unshifted(const BigInt& r, std::uint64_t k, std::uint64_t p) {
	return gate_real_unshifted(r, gate_is("unshift-off") ? (k + 1) % p : k, p);
}
int lucas::jacobi_of(const BigInt& r, std::uint64_t k, std::uint64_t p) {
	return gate_is("jacobi-pass") ? -1 : gate_real_jacobi_of(r, k, p);
}
bool lucas::degenerate(const BigInt& s, std::uint64_t p, bool last) {
	return gate_is("no-degenerate") ? false : gate_real_degenerate(s, p, last);
}
bool lucas::confirms_prime(const BigInt& before_last, std::uint64_t p) {
	return gate_is("no-confirmation") ? true : gate_real_confirms_prime(before_last, p);
}""")
    rename("src/lucas.cpp", "Result lucas::test(std::uint64_t p, const Options& options) {", "lucas::test(", "lucas::gate_real_test(")
    open(d + "/src/lucas.hpp", "a").write("namespace lucas { Result gate_real_test(std::uint64_t p, const Options& options); }\n")
    append("src/lucas.cpp", """Result lucas::test(std::uint64_t p, const Options& options) {
	Options o = options;
	if (gate_is("refuse-off"))
		o.refuse = {};
	if (gate_is("distance-off"))
		o.distance = {};
	if (gate_is("progress-off"))
		o.progress_seconds = 1e9;
	if (gate_is("shift-fixed") and not o.shift)
		o.shift = 0;
	const Result r = gate_real_test(p, o);
	if (gate_is("refusal-off") and r.outcome() == OUTCOME_NO_VERDICT)
		return Result::prime(WORK_LL, p);
	if (gate_is("run-dropped")) {
		if (r.outcome() == OUTCOME_PRIME)
			return Result::prime(WORK_LL, p);
		if (r.outcome() == OUTCOME_COMPOSITE and r.res64())
			return Result::composite_by_residue(WORK_LL, p, *r.res64());
		if (r.outcome() == OUTCOME_ERROR)
			return Result::error(WORK_LL, p, r.reason());
	}
	return r;
}""")
    rename("src/ibdwt.cpp", "double ibdwt::square_subtract(const Engine& engine, Residue& x, std::uint64_t bit) {",
           "ibdwt::square_subtract(", "ibdwt::gate_real_square_subtract(")
    open(d + "/src/ibdwt.hpp", "a").write("\n// Added by the gate\nnamespace ibdwt { double gate_real_square_subtract(const Engine& engine, Residue& x, std::uint64_t bit); }\n")
    append("src/ibdwt.cpp", MUT + """double ibdwt::square_subtract(const Engine& engine, Residue& x, std::uint64_t bit) {
	if (gate_is("subtract-bit"))
		return gate_real_square_subtract(engine, x, (bit + 1) % engine.exponent());
	const double distance = gate_real_square_subtract(engine, x, bit);
	if (gate_is("engine-sabotage")) {
		BigInt v = from_digits(engine, x);
		v = reduce_mersenne(add(v, BigInt(1)), engine.exponent());
		x = to_digits(engine, v);
	}
	return distance;
}""")
PY
}

# mutant_run ID LABEL...: bin/check under GATE_MUTANT=ID fails every LABEL
mutant_run() {
	mr_id=$1
	shift
	( cd "$work/mut" && GATE_MUTANT=$mr_id limit 900 bin/check ) >"$logs/mut-$mr_id.out" 2>&1
	grep -q '^check: [0-9]* passed, [1-9][0-9]* failed$' "$logs/mut-$mr_id.out" \
		&& ( cd "$root" && py "$here/ll_checklog.py" failed "$logs/mut-$mr_id.out" "$@" ) \
		|| fail "mutant $mr_id survived the checks that must catch it: $* (PLAN § 6)"
	MUTANTS=$((MUTANTS + 1))
}

mutants_check() {
	MUTANTS=0
	interposed "$work/mut" || die "cannot build the interposed copy (PLAN § 2.4's pins)"
	mk "$work/mut" clang++ mut-build 2400 bin/check bin/mps || { tail_of mut-build; fail "the interposed copy does not build (PLAN § 2.4's pins)"; return; }
	( cd "$work/mut" && limit 900 bin/check ) >"$logs/mut-none.out" 2>&1 \
		|| { tail_of mut-none; fail "the interposed copy fails the default tier with no mutant"; }
	mutant_run old-stop-rule "$LABEL_STOP"
	mutant_run jacobi-nonzero "$LABEL_VECTORS"
	if [ "$PHASE" -ge 2 ]; then
		mutant_run jacobi-flip "$LABEL_VECTORS" "$LABEL_KNOWN"
		mutant_run jacobi-pass "$LABEL_JACOBI" "$LABEL_RECOVER"
		mutant_run no-degenerate "$LABEL_DEGENERATE"
		mutant_run no-confirmation "$LABEL_CONFIRM"
		mutant_run unshift-off "$LABEL_KNOWN"
		mutant_run subtract-bit "$LABEL_KNOWN"
		mutant_run engine-sabotage "$LABEL_KNOWN"
		mutant_run refuse-off "$LABEL_REPLAY" "$LABEL_MOVE"
		mutant_run distance-off "$LABEL_DISTANCES"
		mutant_run progress-off "$LABEL_PROGRESS"
		mutant_run shift-fixed "$LABEL_FRESH"
		mutant_run refusal-off "$LABEL_REFUSALS"
		mutant_run run-dropped "$LABEL_VIEWS"
		( cd "$root" && py "$here/ll_checklog.py" tally "$logs/mut-engine-sabotage.out" "$LABEL_KNOWN" "known answers" ) \
			|| fail "under the engine's sabotage, the known-answer check calls a case it does not grade (PLAN § 2.6)"
		( cd "$work/mut" && GATE_MUTANT=engine-sabotage py "$here/ll_runs.py" sabotaged bin/mps ) >"$logs/sabotaged-runs.out" 2>&1 \
			|| { head -n 5 "$logs/sabotaged-runs.out" >&2; fail "bin/mps reports a known answer the engine did not compute (§ Invariants 1, 2)"; }
		MUTANTS=$((MUTANTS + 1))
	else
		mutant_run jacobi-flip "$LABEL_VECTORS"
	fi
}


# --- bin/mps ---------------------------------------------------------------------------------------

runs_check() {
	for rc_x in $COMPILERS; do
		( cd "$work/tree-$rc_x" && limit 1200 python3 -I -B "$here/ll_runs.py" known bin/mps ) >"$logs/runs-$rc_x.out" 2>&1 \
			|| { head -n 20 "$logs/runs-$rc_x.out" >&2; fail "$rc_x: bin/mps on the known answers and the refusals (R1-R3, R12, R14)"; }
	done
	( cd "$work/mut" && limit 1200 python3 -I -B "$here/ll_cli.py" check "$work/base-cli.sh" "$work/tree-clang++/bin/mps" "$work/mut/bin/mps" ) \
		>"$logs/cli.out" 2>&1 || { head -n 20 "$logs/cli.out" >&2; fail "tests/cli.sh against bcb6fdf's (R16)"; }
	CLI=$(tail -n 1 "$logs/cli.out")
}


# --- Time ------------------------------------------------------------------------------------------

# timing: make check with nothing to rebuild, bcb6fdf's copy against the tree's, interleaved, the
# least of five each, once more before failing (R16: at most 5 s more)
timing() {
	base_copy "$work/base" || die "cannot copy $BASE"
	mk "$work/base" clang++ base-check 2400 check || { tail_of base-check; die "make check at $BASE is red"; }
	python3 -I - "$MAKE" "$work/base" "$work/tree-clang++" "$DEFAULT_BUDGET" >"$logs/timing.out" 2>&1 <<'EOF' \
		|| { cat "$logs/timing.out" >&2; fail "make check takes more than $DEFAULT_BUDGET s longer than at bcb6fdf (R16)"; }
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
print("make check %.3f s at bcb6fdf, %.3f s now, %+.3f s (budget %+.0f s; least of %d interleaved runs each; load %.1f)"
      % (min(b), min(t), min(t) - min(b), budget, len(t), os.getloadavg()[0]))
sys.exit(0 if min(t) - min(b) <= budget else 1)
EOF
	TIMES=$(cat "$logs/timing.out")
}


# --- The long tier ---------------------------------------------------------------------------------

# long_check: make check-long on clang, alone and timed (R16: at most 900 s), then on GCC; each log
# read for every known answer of the long tier (R1, R2)
long_check() {
	lc_t0=$(perl -MTime::HiRes=time -e 'printf "%.3f", time')
	mk "$work/tree-clang++" clang++ long-clang 1800 check-long
	lc_rc=$?
	lc_t1=$(perl -MTime::HiRes=time -e 'printf "%.3f", time')
	[ "$lc_rc" = 0 ] || { tail_of long-clang; fail "clang++: make check-long is red"; }
	LONG=$(perl -e "printf '%.0f', $lc_t1 - $lc_t0")
	[ "$LONG" -le "$LONG_BUDGET" ] || fail "clang++: make check-long took $LONG s, more than $LONG_BUDGET s (R16)"
	( cd "$work/tree-clang++" && py "$here/ll_runs.py" log "$logs/long-clang.out" long ) || fail "make check-long does not cover the long tier's known answers (R1, R2)"
	LONG_GREEN=clang++
	if printf ' %s ' "$COMPILERS" | grep -q ' g++-16 '; then
		mk "$work/tree-g++-16" g++-16 long-gcc 2400 check-long || { tail_of long-gcc; fail "g++-16: make check-long is red (R16)"; }
		( cd "$work/tree-g++-16" && py "$here/ll_runs.py" log "$logs/long-gcc.out" long ) || fail "g++-16: make check-long does not cover the long tier's known answers"
		LONG_GREEN="clang++ and g++-16"
	fi
}


# --- The landing gate ------------------------------------------------------------------------------

# engine_gate_selftest: ci/engine-gate.sh's compare mode on fixture logs written here: green when
# the five digests agree, red naming the label when one differs, red on a dropped label, on no
# digest at all, on a red summary and on a malformed line; and its run mode refuses a GCC that is clang
engine_gate_selftest() {
	eg=$work/tree-clang++/ci/engine-gate.sh
	test -f "$eg" || { fail "no ci/engine-gate.sh (R17)"; return; }
	eg_f=$work/eg
	mkdir -p "$eg_f"
	python3 -I - "$eg_f" <<'PY'
import sys
d = sys.argv[1]
labels = ["squarings", "products", "partial chains", "final chains", "zero chains"]
def log(name, digests, summary="check: 10 passed, 0 failed", extra=""):
    lines = ["check: 120 passed, 0 failed", "cli: 645 passed, 0 failed", "ll: 48 passed, 0 failed"]
    lines += ["   residues %s: %s" % (l, h) for l, h in digests]
    lines += [summary, extra]
    open("%s/%s.log" % (d, name), "w").write("\n".join(lines) + "\n")
good = [(l, ("%064x" % (i + 1))) for i, l in enumerate(labels)]
log("a", good)
log("b", good)
for i, l in enumerate(labels):
    log("flip%d" % i, good[:i] + [(l, "f" * 64)] + good[i + 1:])
log("drop", good[:4])
log("none", [])
log("red", good, summary="check: 9 passed, 1 failed")
log("bad", good, extra="   residues zero chains: nothex")
log("extra", good + [("other chains", "e" * 64)])
PY
	sh "$eg" compare "$eg_f/a.log" "$eg_f/b.log" >"$logs/eg-green.out" 2>&1 || { cat "$logs/eg-green.out" >&2; fail "ci/engine-gate.sh compare is red on equal digests (R17)"; }
	for eg_i in 0 1 2 3 4; do
		case $eg_i in 0) eg_l=squarings ;; 1) eg_l=products ;; 2) eg_l="partial chains" ;; 3) eg_l="final chains" ;; 4) eg_l="zero chains" ;; esac
		for eg_o in ab ba; do
			if [ "$eg_o" = ab ]; then set -- "$eg_f/a.log" "$eg_f/flip$eg_i.log"; else set -- "$eg_f/flip$eg_i.log" "$eg_f/a.log"; fi
			sh "$eg" compare "$@" >"$logs/eg-flip.out" 2>&1
			[ $? = 1 ] && grep "$eg_l" "$logs/eg-flip.out" | grep -q "ffffffffffffffff" \
				|| fail "ci/engine-gate.sh compare does not exit 1 naming '$eg_l' and its digest when it differs (R17)"
		done
	done
	for eg_c in drop none red bad extra; do
		sh "$eg" compare "$eg_f/a.log" "$eg_f/$eg_c.log" >"$logs/eg-$eg_c.out" 2>&1 && fail "ci/engine-gate.sh compare passes a $eg_c log (R17)"
		sh "$eg" compare "$eg_f/$eg_c.log" "$eg_f/$eg_c.log" >>"$logs/eg-$eg_c.out" 2>&1 && fail "ci/engine-gate.sh compare passes two $eg_c logs (R17)"
	done
	( cd "$work/tree-clang++" && MPS_GCC=clang++ limit 60 sh ci/engine-gate.sh ) >"$logs/eg-clang.out" 2>&1
	[ $? = 2 ] || fail "ci/engine-gate.sh runs with a GCC that is clang (R17)"
	if g++ -dM -E -x c++ /dev/null 2>/dev/null | grep -q '__clang__'; then
		( cd "$work/tree-clang++" && MPS_GCC=g++ limit 60 sh ci/engine-gate.sh ) >"$logs/eg-gxx.out" 2>&1
		[ $? = 2 ] || fail "ci/engine-gate.sh takes this host's g++, which is clang, for GCC (R17)"
	fi
	EG="compare mode red on 20 defects"
}

# engine_gate_run [COMMIT]: the landing gate's run mode, alone (two engine tiers, one after the
# other), under the compiler shim, on COMMIT or else on a commit of this tree made without touching
# any ref: green, naming that commit, and clang++ and g++-16 each compiled the engine
engine_gate_run() {
	if [ -n "${1:-}" ]; then
		eg_c=$1
	else
	( cd "$root" && GIT_INDEX_FILE="$work/index" git read-tree HEAD && \
	  git ls-files -z --cached --others --exclude-standard | perl -0 -ne 'chomp; print "$_\0" if -e $_' \
	  | GIT_INDEX_FILE="$work/index" git update-index --add --remove -z --stdin && \
	  GIT_INDEX_FILE="$work/index" git write-tree ) >"$work/tree-id" 2>"$logs/eg-commit.err" || die "cannot write the tree for the landing gate"
	eg_c=$(cd "$root" && git commit-tree "$(cat "$work/tree-id")" -p HEAD -m "engine gate of the working tree") || die "git commit-tree"
	fi
	( cd "$root" && PATH="$work/shim:$PATH" MPS_GATE_LOG="$logs/eg-run.calls" MPS_GCC=g++-16 limit 3600 sh "$work/tree-clang++/ci/engine-gate.sh" "$eg_c" ) >"$logs/eg-run.out" 2>&1 \
		|| { tail_of eg-run; fail "ci/engine-gate.sh on this tree is red (R17)"; return; }
	for eg_x in clang++ g++-16; do
		awk -v x="$eg_x" '$1 == x' "$logs/eg-run.calls" | grep -q -- ' -c src/ibdwt.cpp ' \
			|| fail "ci/engine-gate.sh's run never had $eg_x compile the engine (R17)"
	done
	EG_LINE=$(tail -n 1 "$logs/eg-run.out")
	case $EG_LINE in "engine-gate: green on $eg_c"*) ;; *) fail "ci/engine-gate.sh's last line does not name the commit it tested: $EG_LINE" ;; esac
	EG="$EG; run mode green: $EG_LINE"
}


# --- The notes -------------------------------------------------------------------------------------

docs_check() {
	cd "$root" || die "cd $root"
	git show "$BASE:doc/lucas-lehmer.md" >"$work/base-note.md" || die "git show $BASE"
	py "$here/ll_docs.py" "$PHASE" || fail "the notes (R18)"
	py "$here/ll_docs.py" claims "$PHASE" >"$logs/claims.out" 2>&1 || { tail_of claims; fail "notes.py claims on the changed notes (doc/README.md's markers)"; }
}


# gate_all: everything a phase gate checks; GATE_LINE summarizes it
gate_all() {
	TIMES='timing not run (quick mode)' MUTANTS=0 LONG=- LONG_GREEN='not run' CLI='not run' EG='not run' PROBE=
	files_check
	builds
	vectors_check
	probe_check
	mutants_check
	if [ "$PHASE" -ge 2 ]; then
		runs_check
	fi
	if [ "$PHASE" -ge 3 ]; then
		engine_gate_selftest
	fi
	if [ "$MODE" = full ]; then
		timing
		if [ "$PHASE" -ge 3 ]; then
			long_check
			[ -n "${GATE_NO_ENGINE_RUN:-}" ] || engine_gate_run
		fi
	fi
	docs_check
	GATE_LINE="$MODE mode, phase $PHASE: files and pins as PLAN § 2 gives them; make check green on $COMPILERS ($CHECKS); Jacobi vectors reproduced and recomputed; probe ok ($PROBE); $MUTANTS mutants caught; cli: $CLI; $TIMES; long tier green on $LONG_GREEN in $LONG s; landing gate: $EG; notes and AGENTS.md"
}


gate_end() {
	if [ -n "$SELFTEST" ]; then
		echo "selftest summary: ${GATE_LINE:-}"
		if [ "$FAILS" -gt 0 ]; then echo "selftest: $FAILS checks failed" >&2; exit 1; fi
		echo "selftest: no check failed (not a verdict: frozen-gate check skipped)"
		exit 3
	fi
	echo "$1: green ($GATE_LINE)"
}
