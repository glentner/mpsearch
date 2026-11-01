# Research 03 — The result's run fields, the stop rule, and tests/cli.sh

Scope: R12, R14 and R15, and the parts of R16 that touch `tests/cli.sh`. Line numbers are at `cc62474`,
whose `src/` and `tests/` match `bcb6fdf`. The experiments below ran on a scratch copy of `bcb6fdf`,
built by hand with clang. Each stand-in replaced the body of `test_lucas_lehmer` and was run under
`tests/cli.sh`.

## 1. R12: the run's fields, the constructors, the two views

**Struct.** Add one aggregate to `src/result.hpp`. Don't call it `Run`: `main.cpp:45` and the printer
already use `run` for the vector of results, and `check_result.cpp:714`, `:751` and `:827` declare a
local `struct Run`. Call it `RunReport`:

```cpp
// What a run of a test did: where it ran and what its error checks saw
struct RunReport {
	std::vector<std::uint64_t> lengths {};  // each transform length, in the order it ran at them
	std::uint64_t shift = 0;
	std::uint64_t jacobi_checks = 0;
	std::uint64_t jacobi_failed = 0;
	std::uint64_t degenerate_failed = 0;
	std::uint64_t confirm_failed = 0;       // the prime confirmation
	std::uint64_t transient_faults = 0;
	std::uint64_t length_retries = 0;
	std::uint64_t past_sizing = 0;          // accepted squarings past ibdwt::SIZING_DISTANCE
	double largest_distance = 0;
};
```

`Result` gains `std::optional<RunReport> run_` and the accessor `const std::optional<RunReport>& run() const`.

**Constructors.** The constructor stays private (`result.hpp:73-75`), so "only the named constructors build a
result" still holds:

- `prime(Worktype, p, const RunReport&)` *replaces* `prime(Worktype, p)` and sets `res64_ = 0` itself, so
  no prime can carry a nonzero residue. Nothing in `src/` calls `prime` or `composite_by_residue` today;
  only the checks do. Replacing them, rather than adding overloads, means nobody can build a prime from
  the Lucas–Lehmer test without its fields.
- `composite_by_residue(Worktype, p, res64, const RunReport&)` replaces the three-argument form.
  **Don't refuse `res64 == 0`.** A nonzero residue can have its low 64 bits all zero. The driver must
  decide prime from the full residue being 0, never from the Res64.
- `error(Worktype, p, reason, const RunReport&)` is a new overload. Keep `error(Worktype, p, reason)` for
  the exponent check (`math.cpp:173`).
- Each new form throws `std::logic_error` when `run.lengths` is empty: a run that ran at no length is a
  bug.
- `probable_prime` and `no_verdict` stay as they are. R14's no verdicts carry no `RunReport`: they are
  decided before any length or shift is chosen, and R12 binds only "a verdict or an error".

**Line format.** The run section follows `"; "`. Don't use brackets: section 3 matches lines with shell
`case` globs, where `[`/`]` are special. The program tail stays last (`result.cpp:258`). Each key is its
JSON key with `-` for `_`, so one check can confirm that both views name the same fields. Lengths are
joined by `/`.

```
M31 ll: prime, res64 0000000000000000; lengths 2, shift 17, jacobi-checks J, jacobi-failed 0, degenerate-failed 0, confirm-failed 0, transient-faults 0, length-retries 0, past-sizing 0, largest-distance 0.0001 (mps 0.1.0)
M1237 ll: composite, res64 DF5E6622F83F5BE2; lengths N, shift 905, jacobi-checks J, … (mps 0.1.0)
M33 ll: error, the Jacobi check failed 4 times, past its limit; lengths 2, shift 9, jacobi-checks J, jacobi-failed 4, … (mps 0.1.0)
```

The 2 for M31 is what `ibdwt::length_for(31)` returns: length 1 holds up to 24 (`lengths.cpp:29-30`). J
and N depend on the plan's schedule and lengths. An error carries no Res64: a residue from a failed run
must not read as a result. The reason may hold commas; the driver writes it and never writes `;` in it.

**Record format (`RECORD_VERSION` 2).** Spacing is `json.dumps` with its defaults (`", "`, `": "`, and
lists as `[64, 80]`). The keys come in this order: `exponent`, `worktype`, `status`, then `res64` (prime
and composite) or `reason` (error), then `lengths`, `shift`, `jacobi_checks`, `jacobi_failed`,
`degenerate_failed`, `confirm_failed`, `transient_faults`, `length_retries`, `past_sizing`,
`largest_distance`, then `program` and `record_version`:

```
{"exponent": 31, "worktype": "ll", "status": "prime", "res64": "0000000000000000", "lengths": [2], "shift": 17, "jacobi_checks": 3, "jacobi_failed": 0, "degenerate_failed": 0, "confirm_failed": 0, "transient_faults": 0, "length_retries": 0, "past_sizing": 0, "largest_distance": 0.0001, "program": {"name": "mps", "version": "0.1.0"}, "record_version": 2}
{"exponent": 1237, "worktype": "ll", "status": "composite", "res64": "DF5E6622F83F5BE2", "lengths": [N], …, "record_version": 2}
{"exponent": 33, "worktype": "ll", "status": "error", "reason": "the Jacobi check failed 4 times, past its limit", "lengths": [2], …, "record_version": 2}
```

`label_by_outcome` (`result.cpp:89-96`) gains `{OUTCOME_ERROR, "error"}`. `json_record` loses its early
return (`:278-279`) and the sentence "An error has no record" (`:276`).

**Distance format.** Round the distance *up* to four decimals, then write it as Python's `repr` writes that
value: `0.0`, `0.0001`, `0.35`, `0.4375`. In C++: `snprintf("%.4f", std::ceil(d * 1e4) / 1e4)`, then strip
trailing zeros but keep one digit after the point. Python's `repr` switches to exponent notation only below
1e-4, so every four-decimal value it prints is plain decimal, and these bytes equal
`json.dumps(math.ceil(d*1e4)/1e4)`. Rounding up means a counted distance past 0.35 almost never prints as
0.35. The engine's own `decimal()` (`ibdwt.cpp:465-469`, `%.6g`) can print `1e-05`, so don't reuse it
here. **The checks' JSON reader rejects every fraction** (`check_result.cpp:93-105`, and `"{\"a\": 1.5}"`
is on the bad list at `:476`). It has to accept `frac`: move `1.5` to the good list, add `1.`, `.5` and
`1.5.2` as bad, and keep rejecting exponents, which no record writes.

**Where an error's record goes under `-j`.** Recommendation: **stdout, as it arrives, as one more
record.** The error line still goes to stderr through `logging::critical`, in both modes.

- A record no one ever sees would not meet R12.
- Stderr is the log: stamped under `-D` and coloured under `FORCE_COLOR`, so a record there is no
  longer machine-readable.
- A fleet needs the counts most when a run fails.

In `report_result` (`result.cpp:370-373`), log the line, then `if (json) out << json_record(result)`.
Nothing held prints, because `report_run` returns at `ERR_RUN` (`:399-400`), and the exit status stays
20.

Apply the rule to every error, so the printer has one rule. The exponent check's error would then print
a record without run fields under `-j`. Bin/mps can't reach that today, since the trial-division search
is exact, so `cli.sh` doesn't change for it. **This supersedes `verdicts-and-exit-statuses` R10 ("an
error SHALL write no JSON"), and its Non-goal "JSON records for errors".** Say so in PLAN.md. It is a
change to `src/result.*`, a human gate.

**`tests/check_result.cpp` changes:**

- `:6-11`: the header's "no test decides anything yet".
- `:310`: `program()` writes `record_version` 2.
- `:330-333`, `:342-345` (`each_outcome`): the prime and the composite by residue take a `RunReport`.
  Their lines and records gain the run section.
- `:354-356`: make `an_error()` an LL error with a `RunReport`. Add a ptest error for the form without
  fields.
- `:415`: `RECORD_VERSION` is 2.
- `:419-425`: becomes "an error has a line and a record", compared exactly.
- `:428-437`: new `composite_by_residue` signature.
- `:440-456`: also loop over error records.
- `:93-105`, `:465-499`: the reader and its bad and good lists.
- `:625-635`, `:739-775`, `:816-851`, `:854-871`, `:884-887`: constructor signatures.
- `:704-736`: in JSON mode each run's expected stdout gains the error's record.
- `:778-813`: `error_line` gains the run section if `an_error()` carries one; stderr is otherwise
  unchanged.
- `:908-942`, `:945-974`: see section 2.

New checks:

- the line's keys and the record's keys are the same, in the same order;
- `lengths` shows two lengths after a retry (`64/80`, `[64, 80]`);
- the distance edge cases: 0, 0.35 exactly, just past 0.35, and 0.4375;
- a prime always has Res64 0;
- an empty `lengths` throws.

## 2. R15: the stop rule

At `math.cpp:111`, write `if (is_decisive(*last) or last->outcome() == OUTCOME_ERROR) break;`. Leave
`is_decisive` alone, since `status_of` and the printer use it. Replace the doc comment at `:101-105`:

> Run tests on M_p in order, stopping after the first result that ends the run. Each result goes to
> `completed` as soon as its test returns, before the next test starts. A verdict decides the exponent;
> an error ends the run too, since a run whose checks failed earns no verdict and exits 20 whatever
> follows, and a later test would only spend time. A no verdict decides nothing, so the next test runs.
> Returns the last result handed over.

At `test_all` (`:126`), "stops at the first one that decides" becomes "…that decides or fails".

**Stand-ins:** the existing `gives_*` (`check_result.cpp:878-905`). `gives_prime` needs a `RunReport`.

**Checks that assert the old rule:** only `run_in_order_stops_at_the_first_verdict`. Its doc (`:912-913`,
"goes on past … an error") and its row at `:925` (expected
`{"error","no verdict","composite"}`, which becomes `{"error"}`) both change. Add these rows:

- `{nv, error, prime}` → `{"no verdict","error"}`
- `{nv, nv, error}` → all three

Add one check through the printer: `run_in_order` feeding `report_result` over `nv, error, composite`.
Stdout holds only the no verdict (plain) or the no verdict plus the error record (JSON), the status is 20,
and `ran` stops at the error. That pins R15's "neither run nor print … exits 20".

`tests/check_math.cpp` has no `run_in_order` check (grep), so nothing there asserts the rule. But
`the_stubs_decide_nothing` (`check_math.cpp:136-171`) runs `test_lucas_lehmer` as a stub on 2, 3, 4, 31,
33, 999999937 and 999999999. Its LL row (`:152`) has to leave the table.
`test_all_runs_the_three_tests_in_order` (`check_result.cpp:945-974`) also changes. Its third result is
now a real prime with a random shift, so `expected[2]` (`:960`) can only match its shape, and the doc
"None decides" (`:949`) becomes untrue.

`cli.sh` **cannot observe R15**: LL runs last, and tf and p1 never fail. The stand-ins are its only proof.

## 3. What changes in tests/cli.sh

**What the stand-in run measured.** One stand-in returned prime for 3 and 31, a no verdict for every even
p and every p above 20,381,696 (P_max, `lengths.cpp:172`), and an error for 33. It logged only the start
line. Under the old `cli.sh` it fails **84 of the 644 expectation runs**: 83 names, since "31 exits 42"
runs twice. The record_version bump adds the 40 ptest-record expectations below, which pass again after
the one-line edit at `:457`. Grouped:

**A. The test reaches "prime" for 3 and 31.** Forced by the test being real. Exits go 42→0 and the LL
line or record changes:

- `:180-192`: `-t ll`, `-t lucas-lehmer` and `-t all` on 31; 6 runs.
- `:194-195`
- `:200-205`: 3 and 31.
- `:348-349`
- `:377-378`
- `:396-397`
- `:405-408`
- `:433-439`: 31 with `-s`/`--skip-ptest`.
- `:467-475`: 3, 31 and 0000000031, each under `-j`/`--json`.
- `:481-490`
- `:513-519`: 31.
- `:530-535`, `:542-550`: `-jd`, `-dj`, `31 -j`, `-jt ll`, `-j -j`.

The shift is random (R3, and no option sets it), and `largest-distance` follows from it. So every new
LL expectation must blank those two values and then compare exactly. Use a built-ins helper:
`${line%%", shift "*}`, then the digits up to the next `,`, then a check that the shift is below p. Don't
use a bare glob.

**B. p = 2 (R14). Flag: a no verdict, not a verdict.** At `:200-205` and `:467-475`, 2's third line and
record take the odd-exponent reason. The exit stays 42.

**C. P_max (R14). Flag.**

- 999999929 and 999999937 at `:200-205` and `:467-475`.
- 999999999 under `-s` at `:433-439` and `:513-519`.

The third line or record takes the P_max reason, and the exit stays 42.

**D. p = 4 under `-s`. Flag: the GOAL has no rule for it.** R14 names only p = 2. Run on 4, the real test
would compute J(12 | 15) = 0 and fail every check, giving 20. **Recommend R14's odd-exponent no verdict
for every even p.** That keeps `-s 4` at exit 42, and only the third line changes (`:433-439`,
`:513-519`).

**E. 33 under `-s` ends in an error. Flag: an error, not a verdict.** 7 = M_3 divides M_33, and
s_1 = 14 ≡ 0 (mod 7). By the invariant (`doc/lucas-lehmer.md:62`), J(s_i − 2 | M_33) = 0 for every
i ≥ 3, so a correct run fails its check on every attempt and ends in R6's error, exit 20.

- `:433-443`: stdout keeps the tf and p1 lines only, and stderr gains the critical line plus any recovery
  logging, so "runs all three tests" fails.
- `:445-447`: exit 20, empty stdout.
- `:513-521`, `:536-541` (`records_33` at `:527-528`): exit 20; the tf and p1 records plus the error
  record, if the recommendation in section 1 holds.

**F. `record_version` 2 (R12 and § Conventions). Flag: outside LL.** The one line at `:457` feeds every
record. That includes the 40 ptest records at `:496-511`, which don't depend on the test.

**G. Comments** that become false: `:164-166`, `:198-199`, `:431-432`. Change `planned` (`:37`) only if
expectations are added.

**Stderr.** None of the 84 is a stderr failure except group E. That holds only if the test logs its start
line, `Running Lucas-Lehmer test`, byte for byte, and nothing else for these exponents: not on success,
not for R14's no verdicts (log the start line, then decide), and nothing at debug level.
`:275-278` asserts "-d never changes what is printed, since no message is at debug". If the start line
names the length or shift, or if R13 logs progress for p ≤ 31, about 20 more expectations break (`:189`,
`:196`, `:207`, `:261-278`, `:281-285`, `:316-317`, `:350`, `:398-402`, `:441`, `:477`, `:492`). None of
them is "where the test reaches a verdict". R13's interval should be in iterations and large enough that
`make check`'s runs print no progress line, or progress needs its own check.

## 4. A gate for R16's cli.sh clause

The proposal holds up and costs little, with three refinements.

**Measured at `bcb6fdf`:**

- The union of failures from three generic stand-ins (another reason, prime, error) is 140 names (142
  runs).
- A fourth stand-in that logs one extra line adds nothing: the error stand-in's critical line already
  flips every stderr expectation.
- A full `cli.sh` run takes about 1.3 s. Each stand-in recompiles one translation unit and relinks in
  about 2 s.
- The whole gate is a few seconds plus the build that `make check` already does.

**Refinement 1: an oracle stand-in, not only the union.** The generic union is loose. It covers every
stderr expectation, so it would hide a real test that logs progress at p = 31, and it allows exit-status
failures for 2, 4, 999999929 and 999999937 that should never occur. Instead, build one more stand-in from
the new tree, about ten lines. It returns what the test should return for each exponent:

- prime, with a fixed `RunReport`, for 3 and 31;
- R14's no verdicts for even p and for p > P_max;
- an error with a `RunReport` for 33;

and it logs only the start line. Then require **the real binary's failure multiset to equal the oracle's**,
with one exception: stderr expectations on 33 may fail in either, since recovery may log. Keep the
generic union as a looser second bound.

**Refinement 2: multisets.** "31 exits 42" and "31 runs all three tests" each appear twice (`:195`/`:204`,
`:196`/`:207`). Compare `sort` output, not `sort -u` output.

**Refinement 3: a fixed seam.** The gate builds a stand-in by renaming the line
`Result test_lucas_lehmer(const long unsigned p) {` to `…_real` in a scratch copy and appending a stand-in
definition. The plan should keep `test_lucas_lehmer` in `src/math.cpp` as a thin wrapper with that exact
line, and the gate fails unless the line occurs exactly once. The version patch also has to match exactly
once: `"record_version": 1}` at `:457`.

**Steps:**

- (a) A stub stand-in, using `bcb6fdf`'s body verbatim, under the version-patched old `cli.sh` must pass
  644 of 644.
- (b) The real binary under the same script must fail exactly the oracle's multiset.

**Holes:**

- Pass/fail is one bit. A wrong value on an expectation that already fails is masked: exit 20 where 0 was
  wanted still differs from 42. Correctness belongs to the new `cli.sh` and review.
- A stub stand-in exercises none of the printer's new paths for errors and run fields. That is intended:
  they are the test's results.

**Also check the new `cli.sh`:**

- (c) *Names it still runs.* Instrument a scratch copy of each script by replacing the line `	name=$1`
  in `expect` with `	name=$1; echo "$name" >&3` (prototyped; it lists all 644). Every old name not
  in the oracle's set must still run, with the same multiplicity.
- (d) *Where it changed.* Every hunk of `git diff -U0 bcb6fdf -- tests/cli.sh` falls inside the line
  ranges of groups A to G plus `:37`. This catches an expectation that keeps its name but is weakened
  outside those ranges, which (c) cannot see.
- (e) It passes against the real binary, with `planned` equal to its count. The script enforces that
  itself (`:616-625`).

Put it in `spec/lucas-lehmer-test/verify/` as a frozen gate. Run it once with clang and once with GCC.

## 5. Where the known-answer runs live

Recommendation: **a new script, `tests/ll.sh BINARY BOUND`**, POSIX sh with built-ins only, modelled on
`cli.sh`'s `expect`/`planned` frame. It reads `tests/known/mersenne-exponents.txt` (`mersenne <p>`) and
`tests/known/final-residues.txt` (`ll <p> <Res64>`) with `while read`, skipping `#` lines and rows above
BOUND, and runs each `mps -t ll p` with the exponent check on:

- for an odd p from the prime list: exit 0, and the line starts `Mp ll: prime, res64 0000000000000000;`;
- for each `ll` row: exit 40, and `, res64 <row>;`;
- the same through `-j` for a few of them.

`make check` runs it with 10000: 21 odd primes, plus the rows at 1237, 1277 and 10303. The long tier runs
it with 100000, which adds 6 primes and the row at 100069.

Why not `cli.sh`:

- R16 limits `cli.sh` to where verdicts change. Twenty-four new runs there would blur gate (c) and (d).
- `planned` would come to depend on what the tables hold.
- One script with a bound serves both tiers.
- The expected values stay in the tables (§ Invariants 3), never copied into a script.

Bounds at 10000 and 100000 cover R1's sets exactly, because 2 is excluded by "odd". The 100000007 row is
above P_max and makes a free R14 case for the script. Don't run the 1000193 and 10000139 rows: they are
the Non-goals' long runs.

Measure the cost against R16's 5 s. R3 requires two shifts per exponent in `bin/check` as well, so the
bin/mps runs are a third pass over the same exponents. If the budget is tight, run R1/R2 through
`ll.sh` and R3 through the driver at shift 0 plus one other.

## Risks

- Superseding `verdicts-and-exit-statuses` R10 (error records) touches `src/result.*`, a human gate.
- Group D (even p) and the stderr discipline are plan decisions the GOAL doesn't make.
- The fraction change makes the checks' JSON reader looser.
- If the test's start line changes, about 20 `cli.sh` expectations break outside R16's scope.
