# GOAL — The Lucas–Lehmer test on the engine, with a shift and its error checks

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** lucas-lehmer-test
- **kind:** feature
- **appetite:** big

## Problem

The Lucas–Lehmer test is the only test here that proves a Mersenne number prime, and the definition
of done in `ROADMAP.md` is stated in its terms: every known Mersenne prime exponent ends in a zero
residue. At `bcb6fdf`, `test_lucas_lehmer` (`src/math.cpp:88`) is a stub that logs its start and
returns `Result::no_verdict`, "Lucas-Lehmer is not implemented yet"; `mps -t ll` runs it alone after
the exponent check, and `test_all` runs it last through `run_in_order` (`src/math.cpp:99`). The seed
cites an older stub that returned `true`; `verdicts-and-exit-statuses` replaced it.

What there is to build on, all pre-existing:

- the portable FP64 weighted-transform engine (`spec/ibdwt-transform-engine/`, landed `194872d`):
  every length m·2^k ≤ 2^20 with odd m ≤ 15, a measured length table whose largest exponent is
  20,381,696 (`src/lengths.cpp:172`), `square`, `square_subtract` (the subtraction of 2^e at any bit,
  which is all a shifted run needs) and `multiply` (`src/ibdwt.hpp:167`–`175`). Its monitor refuses
  a squaring, with a `roundoff_error` naming p, the length, the distance and the threshold, in four
  cases: an output not finite or at least 2^51, a distance past the threshold (0.4375 by default),
  outputs that disagree with the inputs' checksums, and a root mean square past the length's limit
  (`src/ibdwt.cpp:476`). On every refusal the residue is left as it was;
- `make check-engine`, the engine's full differential suite, which prints residue digests that a
  clang and a GCC build must share; only `ibdwt-transform-engine`'s gates ever compared them;
- `reference::lucas_lehmer` on `BigInt` (`src/reference.hpp:21`), which returns residues and decides
  nothing, and the known answers in `tests/known/`: 52 Mersenne prime exponents (28 of them up to
  100,000, 22 up to 10,000, from 2), and `ll` final residues at 1237, 1277, 10303, 100069, 1000193,
  10000139 and 100000007;
- the result type and its three views (`src/result.hpp`): the verdict line, the `-j` JSON record
  (`RECORD_VERSION` 1) and the exit status. Neither view carries a transform length, a shift or
  error-check counts today, which § Invariants 1 requires of every result where they apply;
- `-f/--file PATH`, parsed into `file_path` (`src/cli.cpp:79`, `:244`) and used by nothing, and
  `src/io.*`, a format sketch that nothing reads or writes.

There is no Jacobi symbol anywhere in `src/` (`src/mp.hpp` has no GCD or Jacobi).

A plain Lucas–Lehmer loop on the engine would already give the right answer most of the time.
"Most of the time" is the problem: unprotected tests were historically wrong about 1.5% of the time
with no error reported. § Invariants 1 lets `mps` report composite from a residue only when every
error check of the run passed, and report an error, never a verdict, when the checks fail and the
run cannot recover from a good checkpoint. § Invariants 2 and Geoffrey's ruling of 2026-10-22 make
the round-off retry the driver's from this cycle on: the engine refuses and keeps its input, and the
driver replays, tells a transient fault from a reproducible excess, and retries at a larger length
from the last good checkpoint. The same ruling requires the engine's standing differential check to
have a form before any verdict depends on the engine, and this cycle's verdicts are the first. The
checks a trustworthy run needs are known (`doc/lucas-lehmer.md`):

- **A shift.** Store 2^k·s_i; each squaring doubles k modulo p, and the "−2" becomes a subtraction
  of 2^((2k+1) mod p). Two runs with different shifts push different data through the transform
  and must agree.
- **A Jacobi check.** J(s_i − 2 | M_p) = −1 for every i ≥ 1, and a shift leaves it unchanged
  (`doc/lucas-lehmer.md`, "The Jacobi invariant (the project's own derivation)"). A random error
  flips it with probability about ½.
- **Degenerate states.** x ↦ x² − 2 fixes 2 and −1 and sends 0 to −2 and then to 2, so a buffer
  zeroed near the end fakes a prime, and J(−2 | M_p) = −1 lets it pass the Jacobi check. For a
  prime, s_(p−3) ≡ ±2^((p+1)/2), a free confirmation of a zero residue.

The seed also asks for checkpoints on disk with resume, and carries deferrals from three earlier
cycles. Shaping split them (Clarifications, Q1).

## Outcome / vision

`mps -t ll p` runs the Lucas–Lehmer test on the FP64 engine at the length the table names, from
s_0 = 4 with a fresh shift each run. It checks the Jacobi symbol at intervals and before any verdict,
checks for degenerate states, confirms a zero residue, and recovers from a failed check or a refused
squaring by returning to the last good state it holds in memory, moving to a larger length when a
refusal reproduces. It logs progress, and it ends with a verdict whose line and JSON record name
the length, the shift, the Res64, the error-check counts and the version, or with an error and no
verdict. The checks prove it against the known answers and prove each recovery by injecting faults.
An error in `-t all` ends the run. A change to the engine can no longer land without its full
differential suite passing under both compilers.

Persistence is the follow-up: the `.mps` checkpoint file, resume after a kill, and the runs long
enough to need them (`issues/checkpoint-and-resume.md`).

## Acceptance criteria (the contract)

Terms: **the test** is what `mps -t ll` runs (and `test_all` runs last). A **shift** k means the test
holds 2^k·s_i mod M_p; every residue a criterion compares is unshifted first. A **good state** is a
residue with its iteration, shift and length that passed every error check at that iteration; the
test holds it in memory, and it is the run's checkpoint in the sense of § Invariants 1 and 2 (Q2).
An **error check** is the Jacobi check (R4), the degenerate-state check (R7) or the prime
confirmation (R8). The **long tier** is R16's separate make target. Every `mps` run below is with
the exponent check on (no `-s`). **P_max** is the largest exponent the length table holds.

- **R1** — WHEN `mps -t ll p` runs for an odd Mersenne prime exponent p in
  `tests/known/mersenne-exponents.txt`, the test SHALL report M_p prime with a zero Res64 and exit 0,
  for every such p ≤ 10,000 in `make check` and every such p ≤ 100,000 in the long tier.
- **R2** — WHEN `mps -t ll p` runs for an exponent with an `ll` row in
  `tests/known/final-residues.txt`, the test SHALL report M_p composite with that row's Res64 and
  exit 40, for 1237, 1277 and 10303 in `make check` and for 100069 in the long tier.
- **R3** — The test SHALL choose its shift afresh on every run, from 0 ≤ k < p and from a source that
  differs between runs, and SHALL report it. WHEN the checks run the test at two different given
  shifts, 0 among them, on the same exponent, both runs SHALL report the same Res64 and verdict, for
  every exponent R1 and R2 cover in `make check`.
- **R4** — The test SHALL compute J(s_i − 2 | M_p) from the residue it holds (the shift accounted for)
  at intervals during the run and on the final residue before any verdict, and SHALL treat −1 as
  passing and +1 or 0 as a failed error check. The result SHALL report how many Jacobi checks ran.
- **R5** — The Jacobi symbol the test computes SHALL equal an independent computation (Python's
  integers) on committed vectors, each file carrying the command that reproduces it, covering
  residues modulo M_p for the exponents R1 and R2 cover in `make check`, both signs, and arguments
  that share a factor with the modulus (symbol 0).
- **R6** — IF an error check fails, THEN the test SHALL return to its last good state and redo the
  iterations after it; IF failures in one run pass a limit the plan sets, THEN the test SHALL report
  an error naming the check that failed, print no verdict, and exit 20. Checks SHALL show both: a
  corruption injected once at a given iteration is recovered from and the run ends with the known
  Res64 and one failure counted; a corruption injected on every attempt ends in the error.
- **R7** — IF the residue at an iteration the test checks is 1, −1, 2 or −2 modulo M_p, or is 0 at any
  iteration before the last, THEN the test SHALL treat it as a failed error check. The test SHALL
  check at least every good state it keeps and the final residue. A check SHALL show that a residue
  zeroed before the last iteration is caught.
- **R8** — WHEN a run's final residue s_(p−2) is 0, the test SHALL confirm that
  s_(p−3) ≡ ±2^((p+1)/2) (mod M_p) before it reports prime; IF it is not, THEN the test SHALL treat
  it as a failed error check. A check SHALL show that a residue zeroed at the last iteration is
  caught.
- **R9** — IF the engine refuses a squaring, THEN the test SHALL replay that squaring from the
  residue the engine kept, at the same length, and IF the replay is accepted, the run SHALL continue
  and count one transient fault.
- **R10** — IF the replay of R9 is refused too, THEN the test SHALL return to its last good state and
  continue at the next larger supported length that holds p. IF no larger supported length remains,
  or such retries in one run pass a limit the plan sets, THEN the test SHALL report an error naming
  p, the length and the distance, print no verdict, and exit 20. Checks SHALL show each branch by
  injecting refusals: one refusal (R9), a refusal that reproduces at one length and not the next,
  and a refusal at every length.
- **R11** — WHEN the engine accepts a squaring whose round-off distance exceeds the length table's
  sizing limit (0.35, `spec/ibdwt-transform-engine/` R3), the test SHALL count it and log it with the
  iteration, the length and the distance (the plan may fold repeats into a count). The result SHALL
  report that count and the largest distance of the run.
- **R12** — Every result the test produces, a verdict or an error, SHALL name in its verdict line and
  in its JSON record the test, the exponent, the transform length (each length it ran at, when R10
  moved it), the shift, the Res64 for a verdict, the error-check counts (Jacobi checks run and failed,
  degenerate-state and prime-confirmation failures, transient faults, length retries, R11's count)
  and the program version. `RECORD_VERSION` SHALL be bumped in the same commit. The other tests'
  no-verdict lines SHALL be unchanged.
- **R13** — WHILE a run is in progress, the test SHALL log progress at intervals the plan sets:
  the iteration, the time per iteration, the estimated time to finish, and the largest round-off
  distance since the last progress line.
- **R14** — IF p is larger than P_max, THEN `mps -t ll p` SHALL report no verdict, with a reason that
  names P_max, and exit 42, before any squaring. IF p is 2, THEN it SHALL report no verdict, with a
  reason that the test applies to odd exponents, and exit 42.
- **R15** — WHEN a test run by `run_in_order` returns an error, the run SHALL stop there: later tests
  neither run nor print a line, and the run exits 20. The checks of `run_in_order` SHALL pin the rule
  with stand-in tests.
- **R16** — The injections R6, R7, R8 and R10 use SHALL be reachable only from the checks: no option,
  environment variable or input of `bin/mps` triggers one. With nothing to rebuild, `make check` SHALL
  take no more than 5 s longer than at `bcb6fdf` on the same machine with clang; the plan records the
  baseline it measured. A separate make target, outside `make check` and the profile's `verify`, SHALL
  run the long tier in no more than 15 minutes on the same machine with clang. Both clang and GCC
  SHALL build and pass both. `tests/cli.sh` changes only where `-t ll` and `-t all` now reach
  verdicts.
- **R17** — The profile in `AGENTS.md` § Factory SHALL gain a row naming a landing gate: before a
  change to the engine's sources (`src/ibdwt.*`, `src/fft.*`), its length table and measurement
  (`src/lengths.*`, `src/lengths.txt`, `src/measure.cpp`) or the numeric build flags (`IEEE` in the
  `Makefile`) lands, `make check-engine` SHALL have passed on it under clang and under GCC, and a
  command the row names SHALL have compared the two builds' residue digests and found them equal.
  That command SHALL exit non-zero, naming the digest that differs, when they are not. This cycle
  SHALL pass the gate before it lands.
- **R18** — In the commit that adds them, the test driver's new paths SHALL join the high-blast-radius
  list and § Map SHALL name them and the long-tier target (§ Build and run too).
  `doc/lucas-lehmer.md`, "What mps uses", SHALL describe the test as built: the shift, the Jacobi
  schedule, the degenerate and prime checks, the recovery and the length retry with their limits.
  The Jacobi symbol's algorithm SHALL be written up, cited where it is used, in the note of the
  module that holds it. The sentences that say `make check` runs the reference tests "on the
  published Mersenne prime exponents" (`doc/lucas-lehmer.md:114`, `doc/prp-and-gerbicz.md:118`,
  `doc/reference-arithmetic.md:75`) SHALL say what it runs since `known-answer-tables`. Every
  concrete number added follows `doc/README.md`'s marker rule.

## Non-goals (no-gos)

- The `.mps` checkpoint file (versioned, checksummed, written to a temporary name and renamed),
  saving on a timer, resume after a kill without changing the answer, wiring `-f`, and the format's
  sharing with PRP and P-1 (the seed's R3, R7 and R8): `issues/checkpoint-and-resume.md`, which
  `ROADMAP.md`'s PRP entry now depends on.
- `ll` runs past the long tier (the final rows at 1000193 and above, the Mersenne primes past 100,000,
  Part II's "every known Mersenne prime up to a few million"): runs of hours that need resume, so
  `issues/checkpoint-and-resume.md` too.
- The verdict path's guards G1–G3 from `verdicts-and-exit-statuses` (the exponent check's re-check on
  the production path, `utf8_length`'s table bounds, `line_text` on factors):
  `issues/verdict-path-guards.md`.
- A cumulative `make check` time baseline in the profile: `issues/check-time-baseline.md`. R16's 5 s is
  this cycle's bound, not a project ceiling.
- A factor from a Jacobi symbol of 0 (a GCD with M_p) and a Jacobi symbol that scales to the largest
  exponents: `issues/large-gcd-and-jacobi.md`. Here a symbol of 0 is a failed check (R4).
- A Gerbicz-protected Lucas–Lehmer test (ω^(2^i) in Z[√3]/(M_p)): `issues/prp-gerbicz-test.md`'s
  Notes, the seed that owns the Gerbicz check.
- An isolating check for the engine's magnitude refusal: `issues/engine-differential-standing-check.md`
  (Q8). That seed's remaining scope (other engines' suites, a scheduled run) stays there; R17 builds
  only the gate for this engine.
- A shift option on the command line: `issues/burn-in-mode.md`, whose profiles set shifts. A second,
  independent shifted run made automatically: `issues/known-primes-campaign.md`.
- Changes to the engine's arithmetic, its refusals or the length table (`src/ibdwt.*`, `src/fft.*`,
  `src/lengths.*`), exponents above P_max, lengths above 2^20, and speed:
  `issues/cpu-transform-throughput.md`, `issues/benchmark-mode.md`.
- `mps -t prp`, and stopping P-1 or trial factoring from being stubs: their own seeds.
- New or edited rows in `tests/known/`: every row R1 and R2 need is there.

## Clarifications

- **Q1:** Does the size circuit-breaker fire, and what is the pilot? — **A:** Yes. The seed is
  several deliverables: the test with its shift and error checks; the checkpoint file and resume; the
  driver's fault policy; the CLI verdict; and three carried deferrals (the `-t all` stop rule, the
  verdict-path guards, the engine's standing check), plus two decisions (the magnitude refusal, a
  Gerbicz-protected Lucas–Lehmer test). The pilot is the test in one process: the shift, the Jacobi,
  degenerate and prime checks, recovery to a good state held in memory, the round-off replay and
  length retry, progress, the verdict with its fields, the stop rule and the engine's landing gate.
  The checkpoint file, resume and the runs that need them go to `issues/checkpoint-and-resume.md`;
  G1–G3 go to `issues/verdict-path-guards.md`. That leaves 18 R-IDs, past the soft limit of about 10
  but one deliverable, a test driver and its checks, of which § Invariants 1 and 2 alone account for
  R4–R12 and the 2026-10-22 rulings for R17. The slug stays (ruled by the foreman, 2026-10-31).
- **Q2:** In the pilot, which has no checkpoint file, does a good state held in memory satisfy
  § Invariants 1 ("cannot recover from a good checkpoint") and § Invariants 2's retry clause
  ("retries from the last good checkpoint at a larger transform length")? — **A:** Yes. A good state
  held in memory is a checkpoint for both; the file only persists the same state across processes,
  and `issues/checkpoint-and-resume.md` takes that. The retry clause binds this driver from this cycle
  on by Geoffrey's ruling of 2026-10-22 (an FP engine refuses at the round-off threshold; retry binds
  the Lucas–Lehmer and PRP drivers once checkpoints and verdicts exist), and R6, R9 and R10 meet it
  with in-memory good states (ruled by the foreman, 2026-10-31).
- **Q3:** Does an error end a `-t all` run? — **A:** Yes. An error stops `run_in_order` there: later
  tests neither run nor print, and the run exits 20, pinned with stand-in tests (R15). Once the
  Lucas–Lehmer test is real, a run that has already failed should not go on to spend days (ruled by
  the foreman, 2026-10-31).
- **Q4:** What form does the engine's standing differential check take (`issues/engine-differential-
  standing-check.md`; Geoffrey's ruling of 2026-10-22 that this shaping decides it)? — **A:** A
  landing gate in the profile: `make check-engine` under clang and under GCC with the digests
  compared, required before any change to the engine's sources, the length table or the numeric
  flags lands. This cycle adds the profile row and passes the gate (R17) (ruled by the foreman,
  2026-10-31).
- **Q5:** Do the verdict-path guards G1–G3, deferred to this seed by Geoffrey's decision of
  2026-10-12, stay in this cycle? — **A:** No. They go to `issues/verdict-path-guards.md`: none touches
  the driver, and the pilot is already past the soft limit (ruled by the foreman, 2026-10-31).
- **Q6:** What are the time budget and the known-answer bounds? — **A:** The profile has no cumulative
  `make check` baseline, so the budget is this cycle's own: with nothing to rebuild, `make check` grows
  by at most 5 s over `bcb6fdf` with clang on the same machine, and runs the known primes up to 10,000
  and the rows at 1237, 1277 and 10303. The long tier runs in at most 15 minutes and covers the known
  primes up to 100,000 and the row at 100069 (R1, R2, R16). The missing profile row goes to
  `issues/check-time-baseline.md` (ruled by the foreman, 2026-10-31).
- **Q7:** What does `mps -t ll p` do for p above P_max? — **A:** No verdict, exit 42, with a reason
  naming P_max, decided before any squaring (R14) (ruled by the foreman, 2026-10-31).
- **Q8:** Does this cycle build an input that isolates the engine's magnitude refusal (seed Notes,
  from `ibdwt-transform-engine` PLAN § 5)? — **A:** No. The driver meets all four refusals through one
  `roundoff_error` path, proved by injection (R9, R10), so which monitor case refused changes nothing
  it does. The gap is the engine suite's, and its note moves to
  `issues/engine-differential-standing-check.md` (ruled by the foreman, 2026-10-31).
- **Q:** What does a Jacobi symbol of 0 mean here? — **A:** A failed error check (R4), not a factor:
  it needs gcd(s_i − 2, M_p) > 1, which a correct run at these exponents never meets, and a factor
  path would add a GCD and a re-check for a case no test can reach without injection. The planned
  text in `doc/lucas-lehmer.md` that reads 0 as "an error unless the GCD gives a factor" is rewritten
  to match (R18) (confirmed by the foreman, 2026-10-31).
- **Q:** Is a degenerate state recovered from, or final? — **A:** Recovered from, like any failed
  error check (R6, R7): § Invariants 1 reports an error only when the run cannot recover from a good
  checkpoint (confirmed by the foreman, 2026-10-31).
- **Q:** How do the checks pin a shift without a command-line option? — **A:** Through the driver's
  own interface, which takes a shift; `mps -t ll` chooses one at random (R3). No option is added
  (confirmed by the foreman, 2026-10-31).
- **Q:** Do both views name the fields § Invariants 1 lists? — **A:** Yes, the verdict line and the
  JSON record (R12), the literal reading of "every result names"; their formats are the plan's
  (confirmed by the foreman, 2026-10-31).
- **Q:** Is a Gerbicz-protected Lucas–Lehmer test worth it, against two shifted runs (seed Notes)?
  — **A:** Not in this cycle; it is new mathematics with no published analysis, and the Gerbicz check
  belongs to PRP's seed, whose Notes now carry the question (confirmed by the foreman, 2026-10-31).
- **Q:** R1 covers every known exponent up to 10,000, which includes p = 2; does the test report M_2 prime? — **A:** No. The Lucas–Lehmer recurrence applies to odd p (for p = 2 there is no iteration, and s_0 = 4 ≡ 1 mod 3), and § Invariants 1 lets `mps` report prime only after a complete Lucas–Lehmer test ends in a zero residue. R1 covers odd exponents; `-t ll 2` reports no verdict and exits 42 (R14). The engine cycle's Lucas–Lehmer zeros started at 3 for the same reason (ruled by the foreman at GOAL sign-off, 2026-10-31).

## Related materials

- Seed: `issues/lucas-lehmer-test.md`; `ROADMAP.md`, Part II, "Lucas–Lehmer with error checks,
  shifts and checkpoints".
- Earlier cycles: `spec/ibdwt-transform-engine/` (the engine, its refusals, R3's sizing limit, the
  ruling on retry), `spec/verdicts-and-exit-statuses/` (the result type, the stop rule, G1–G3),
  `spec/known-answer-tables/` (the tables), `spec/reference-big-integer/` (`reference::lucas_lehmer`).
- Rulings: an FP engine refuses at the threshold and retry binds the Lucas–Lehmer and PRP drivers
  (2026-10-22); the engine's differential suite stays out of the default `make check` and `verify`,
  with a standing check whose form this shaping decides (2026-10-22).
- The mathematics: `doc/lucas-lehmer.md` (the theorem, the shift, the Jacobi invariant, the cost of
  the check), `doc/weighted-transforms.md`, `doc/reference-arithmetic.md`, `doc/README.md` (the
  marker rule).
- Seeds written at this shaping: `issues/checkpoint-and-resume.md`, `issues/verdict-path-guards.md`,
  `issues/check-time-baseline.md`. Seeds that take this cycle's deferrals or wait on it:
  `issues/engine-differential-standing-check.md`, `issues/prp-gerbicz-test.md`,
  `issues/large-gcd-and-jacobi.md`, `issues/burn-in-mode.md`, `issues/known-primes-campaign.md`,
  `issues/benchmark-mode.md`, `issues/cpu-transform-throughput.md`.
- References (in `doc/references.md`): Lehmer 1930 and 1935; Bruce 1993; GIMPS, "The Math".
